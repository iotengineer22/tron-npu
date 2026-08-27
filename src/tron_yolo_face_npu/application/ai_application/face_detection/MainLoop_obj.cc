/*
 * SPDX-FileCopyrightText: Copyright 2022 Arm Limited and/or its affiliates <open-source-office@arm.com>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <cstdio>
#include "log_macros.h"
#include "hal_data.h"
#include "common_util.h"
#include "DetectorPostProcessing.hpp"
#include "DetectionResult.hpp"

extern "C" {
#include "time_counter.h"
#include "wrapper.h"
int e_printf(const char *format, ...);
void update_detection_result(uint16_t index, signed short  x, signed short  y, signed short  w, signed short  h, int val_percent);
extern volatile uint32_t g_ai_inference_time_ms;
}

#define IMAGE_DATA_SIZE  (AI_INPUT_IMAGE_WIDTH * AI_INPUT_IMAGE_HEIGHT * AI_INPUT_IMAGE_BYTE_PER_PIXEL)
extern int8_t model_buffer_int8[IMAGE_DATA_SIZE];

int CompareOutput(const uint8_t* reference, const uint8_t* actual_output, const uint64_t size_in_elements, const float epsilon) {
    int mismatches = 0;
    for (uint32_t i = 0; i < size_in_elements; i += 1) {
        float diff = (actual_output[i] - reference[i]);
        if (diff > epsilon) {
            mismatches += 1;
        }
    }
    return mismatches;
}
const float anchor1[] = {38, 77, 47, 97, 61, 126};
const float anchor2[] = {14, 26, 19, 37, 28, 55 };

// Global static buffers to prevent function local C++ guard initialization locks (avoiding Bus Fault)
// and to avoid quantization parameter overwrite between the two tensors.
// TfLiteFloatArray and TfLiteIntArray use flexible array members data[] which do not allocate storage,
// so we define custom struct wrappers with explicit data arrays to allocate actual memory.
static struct {
    int size;
    float data[1];
} g_scale_array_buf[2];

static struct {
    int size;
    int data[1];
} g_zero_point_array_buf[2];

static TfLiteAffineQuantization g_quantization[2];

void create_int8_tensor(TfLiteTensor* tensor, int8_t* input_ptr, float scale, int zero_point, int idx) {
    // Set type to int8
    tensor->type = kTfLiteInt8;

    // Assign the existing input pointer
    tensor->data.int8 = input_ptr;

    // Set correct bytes for YOLO-Fastest output layers (648 and 2592)
    tensor->bytes = (idx == 0) ? 648 : 2592;

    // Set dims to NULL as requested
    tensor->dims = NULL;

    // Set scale using the decoupled array buffer structure (cast to TfLiteFloatArray*)
    g_scale_array_buf[idx].size = 1;
    g_scale_array_buf[idx].data[0] = scale;
    g_quantization[idx].scale = (TfLiteFloatArray*)&g_scale_array_buf[idx];

    // Set zero point using the decoupled array buffer structure (cast to TfLiteIntArray*)
    g_zero_point_array_buf[idx].size = 1;
    g_zero_point_array_buf[idx].data[0] = zero_point;
    g_quantization[idx].zero_point = (TfLiteIntArray*)&g_zero_point_array_buf[idx];

    g_quantization[idx].quantized_dimension = 0;  // Per-tensor quantization

    tensor->quantization.type = kTfLiteAffineQuantization;
    tensor->quantization.params = &g_quantization[idx];

    // Set allocation type
    tensor->allocation_type = kTfLiteArenaRw;

    // Set other fields
    tensor->is_variable = false;
}

static bool PresentInferenceResult(const std::vector<arm::app::object_detection::DetectionResult>& results)
{
    for (uint16_t i = 0; i < AI_MAX_DETECTION_NUM; i++)
    {
        update_detection_result(i, (signed short)0, (signed short)0, (signed short)0, (signed short)0, 0);
    }

    if (results.size() > 0) {
        e_printf("[NPU Result Present] Presenting %d faces:\n", (int)results.size());
    }

    for (uint16_t i = 0; i < results.size() && i < AI_MAX_DETECTION_NUM; ++i) {
        e_printf("  - Face %d: x0=%d, y0=%d, w=%d, h=%d, score=%d\n",
                (int)i, (int)results[i].m_x0, (int)results[i].m_y0,
                (int)results[i].m_w, (int)results[i].m_h, (int)(results[i].m_normalisedVal * 100));
        update_detection_result(i, 
                                (signed short)results[i].m_x0, 
                                (signed short)results[i].m_y0, 
                                (signed short)results[i].m_w, 
                                (signed short)results[i].m_h,
                                (int)(results[i].m_normalisedVal * 100));
    }

    return true;
}


/*********************************************************************************************************************
 *  main_loop_face_detection function: sets up the application context with a model object and then calls the
 *  ObjectDetectionHandler" function to perform the face detection.
 *  @param[IN]   None
 *  @retval      true	successful execution.
 *  			 false	handler call failed.
***********************************************************************************************************************/
bool main_loop_face_detection()
{
    /* Copy the AI input image to tensor arena */
    memcpy(mera_input_ptr(), model_buffer_int8, IMAGE_DATA_SIZE);

#if (BSP_CFG_DCACHE_ENABLED == 1)
    // CPUがアリーナへコピーした画像データを物理メモリに同期し、NPUが正しく読み出せるようにする
    SCB_CleanDCache_by_Addr((uint32_t *)mera_input_ptr(), IMAGE_DATA_SIZE);
#endif

    volatile uint32_t old_counter =  TimeCounter_CurrentCountGet();

    /* Execute AI inference */
    mera_invoke();

    uint32_t new_counter = TimeCounter_CurrentCountGet();

#if (BSP_CFG_DCACHE_ENABLED == 1)
    // NPUが書き換えた最新の出力テンソルデータをCPUが物理メモリから直接読み出せるようにキャッシュを無効化
    SCB_InvalidateDCache_by_Addr((uint32_t *)mera_output1_ptr(), 648);
    SCB_InvalidateDCache_by_Addr((uint32_t *)mera_output2_ptr(), 2592);
#endif

    uint32_t diff_cycles = new_counter - old_counter;
    e_printf("[NPU Time Debug] cycles: %lu\n", (unsigned long)diff_cycles);

    // 競合するライブラリ関数をバイパスするため、直接ミリ秒を計算する
    application_processing_time.ai_inference_time_ms = (uint32_t)(((uint64_t)diff_cycles * 1000) / 480000000);
    g_ai_inference_time_ms = application_processing_time.ai_inference_time_ms;

    int8_t* output0 = (int8_t*)mera_output1_ptr();
    int8_t* output1 = (int8_t*)mera_output2_ptr();

    std::vector<arm::app::object_detection::DetectionResult> results;
    arm::app::object_detection::PostProcessParams postProcessParams {
        AI_INPUT_IMAGE_HEIGHT, AI_INPUT_IMAGE_WIDTH, AI_INPUT_IMAGE_WIDTH, anchor1, anchor2
    };
    results.clear();

    TfLiteTensor outputTensor0;
    TfLiteTensor outputTensor1;

    // Need to read quatization params from tflite model (netron)
    create_int8_tensor(&outputTensor0, output0, 0.13408391177654266, 47, 0);
    create_int8_tensor(&outputTensor1, output1, 0.18535925447940826, 10, 1);


    // 第1引数にサイズ648 (6x6の解像度テンソル = outputTensor0) を渡し、
    // 第2引数にサイズ2592 (12x12の解像度テンソル = outputTensor1) を渡します。
    arm::app::DetectorPostProcess postProcess = arm::app::DetectorPostProcess(&outputTensor0, &outputTensor1,
            results, postProcessParams);

    if (!postProcess.DoPostProcess()) {
        e_printf("[PostProcess] DoPostProcess failed!\n");
        return false;
    }

    e_printf("[PostProcess] Detection results count: %d\n", (int)results.size());
    for (size_t i = 0; i < results.size(); i++) {
        e_printf("  Face %d: x0=%d, y0=%d, w=%d, h=%d, prob=%d%%\n",
                (int)i, (int)results[i].m_x0, (int)results[i].m_y0,
                (int)results[i].m_w, (int)results[i].m_h, (int)(results[i].m_normalisedVal * 100));
    }

    // デバッグ用に出力テンソルの先頭データを少しダンプ
    e_printf("[NPU Output Dump] out0[0..3]: %d, %d, %d, %d | out1[0..3]: %d, %d, %d, %d\n",
            output0[0], output0[1], output0[2], output0[3],
            output1[0], output1[1], output1[2], output1[3]);

    if (!PresentInferenceResult(results)) {
        return false;
    }

    return true;
}
