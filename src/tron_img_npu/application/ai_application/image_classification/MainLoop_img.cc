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
#include "Classifier.hpp"
#include "ImgClassProcessing.hpp"
#include "log_macros.h"
#include "common_util.h"
#include "Labels.h"

extern "C" {
#include "time_counter.h"
#include <wrapper.h>
#ifdef _B
#undef _B
#endif
#include <tm/tmonitor.h>
void update_classification_result(unsigned index, unsigned short category, float probability);
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


#include <string.h>

void create_int8_tensor(TfLiteTensor* tensor, int8_t* input_ptr, float scale, int zero_point) {
    // Clear all fields to 0 to prevent garbage stack values from causing faults
    memset(tensor, 0, sizeof(TfLiteTensor));

    // Set type to int8
    tensor->type = kTfLiteInt8;

    // Assign the existing input pointer
    tensor->data.int8 = input_ptr;

    // Set bytes
    tensor->bytes = 1000;

    // Set dims to NULL as requested
    tensor->dims = NULL;

    // Use structures with explicit data storage to avoid out-of-bounds writes
    // (TfLiteFloatArray and TfLiteIntArray use flexible array members data[] which do not allocate storage)
    static struct {
        int size;
        float data[1];
    } scale_array_buf;

    static struct {
        int size;
        int data[1];
    } zero_point_array_buf;

    static TfLiteAffineQuantization quantization;

    // Set scale
    scale_array_buf.size = 1;
    scale_array_buf.data[0] = scale;
    quantization.scale = (TfLiteFloatArray*)&scale_array_buf;

    // Set zero point
    zero_point_array_buf.size = 1;
    zero_point_array_buf.data[0] = zero_point;
    quantization.zero_point = (TfLiteIntArray*)&zero_point_array_buf;

    quantization.quantized_dimension = 0;  // Per-tensor quantization

    tensor->quantization.type = kTfLiteAffineQuantization;
    tensor->quantization.params = &quantization;

    // Set allocation type
    tensor->allocation_type = kTfLiteArenaRw;

    // Set other fields
    tensor->is_variable = false;
}

static bool PresentInferenceResult(const std::vector<arm::app::ClassificationResult> &results)
{
    const char** labels = getLabelPtr();
    tm_printf((UB *)"=== NPU Inference results (Top-%u) ===\n", (unsigned int)results.size());
    for (uint32_t i = 0; i < results.size(); ++i) {
        update_classification_result((unsigned)i, (unsigned short)results[i].m_labelIdx, (float)results[i].m_normalisedVal);
        uint16_t cat = results[i].m_labelIdx;
        float prob = results[i].m_normalisedVal;
        
        // Print clean label name without trailing commas
        char clean_label[32] = {0};
        int j = 0;
        for (; labels[cat][j] != '\0' && labels[cat][j] != ',' && j < 24; j++) {
            clean_label[j] = labels[cat][j];
        }
        clean_label[j] = '\0';
        
        tm_printf((UB *)"  Top-%u: Category %d (%s), Prob: %d%%\n", 
                  (unsigned int)(i + 1), (int)cat, clean_label, (int)(prob * 100.0f));
    }
    tm_printf((UB *)"====================================\n");

    return true;
}

void copy_data_to_mera(int8_t* ptr, uint8_t* src, uint32_t size) {
    for(uint32_t i = 0; i < size; i++){
        ptr[i] = src[i] - 128;
    }
}

bool main_loop_image_classification()
{
    copy_data_to_mera((int8_t*)mera_input_ptr(), (uint8_t*)model_buffer_int8, (uint32_t)mera_input_size());
    //memcpy(mera_input_ptr(), model_buffer_int8, mera_input_size());
    /* Run inference over this image. */
    volatile uint32_t old_counter =  TimeCounter_CurrentCountGet();
    tm_printf((UB *)"    [MainLoop] Calling mera_invoke()...\n");
    mera_invoke();
    tm_printf((UB *)"    [MainLoop] mera_invoke() returned.\n");
    volatile uint32_t new_counter = TimeCounter_CurrentCountGet();
    volatile uint32_t diff = new_counter - old_counter;
    application_processing_time.ai_inference_time_ms = TimeCounter_CountValueConvertToMs(old_counter, new_counter);
//
    int8_t* output = (int8_t*)mera_output_ptr();
    tm_printf((UB *)"    [MainLoop] Output pointer: 0x%08X\n", (uint32_t)(uintptr_t)output);

    TfLiteTensor outputTensor;
    tm_printf((UB *)"    [MainLoop] Creating mock TfLiteTensor...\n");
    create_int8_tensor(&outputTensor, output, 0.00390625, -128);

    tm_printf((UB *)"    [MainLoop] Constructing Classifier and ImgClassPostProcess...\n");
    arm::app::Classifier classifier;
    std::vector<arm::app::ClassificationResult> results;
    arm::app::ImgClassPostProcess postProcess = arm::app::ImgClassPostProcess(&outputTensor,
            classifier,
            results);

    tm_printf((UB *)"    [MainLoop] Calling DoPostProcess()...\n");
    if (!postProcess.DoPostProcess()) {
        tm_printf((UB *)"    [MainLoop] Post-processing failed.\n");
        return false;
    }

    tm_printf((UB *)"    [MainLoop] Calling PresentInferenceResult()...\n");
    /* Add results to context for access outside handler. */
    if (!PresentInferenceResult(results)) {
        tm_printf((UB *)"    [MainLoop] PresentInferenceResult failed.\n");
        return false;
    }
    tm_printf((UB *)"    [MainLoop] main_loop_image_classification COMPLETE!\n");
//    std::vector<arm::app::object_detection::DetectionResult> results;
//    arm::app::object_detection::PostProcessParams postProcessParams {
//        AI_INPUT_IMAGE_HEIGHT, AI_INPUT_IMAGE_WIDTH, AI_INPUT_IMAGE_WIDTH, anchor1, anchor2
//    };
//    results.clear();
//
//    TfLiteTensor outputTensor0;
//    TfLiteTensor outputTensor1;
//
//    // Need to read quatization params from tflite model (netron)
//    create_int8_tensor(&outputTensor0, output0, 0.13408391177654266, 47);
//    create_int8_tensor(&outputTensor1, output1, 0.18535925447940826, 10);
//
//
//    arm::app::DetectorPostProcess postProcess = arm::app::DetectorPostProcess(&outputTensor0, &outputTensor1,
//            results, postProcessParams);
//
//    if (!postProcess.DoPostProcess()) {
//        error("Post-processing failed.");
//        return false;
//    }
//
//    if (!PresentInferenceResult(results)) {
//        return false;
//    }

//    volatile int misses = CompareOutput((const uint8_t*)model_Identity, output1, model_Identity_COUNT, 1.0f);
//    misses = CompareOutput((const uint8_t*)model_Identity_1, output2, model_Identity_1_COUNT, 1.0f);

    return true;
}
