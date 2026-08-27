#include <cstdio>
#include "log_macros.h"
#include "hal_data.h"
#include "common_util.h"
#include "DetectorPostProcessing.hpp"
#include "DetectionResult.hpp"

extern "C" {
#include "time_counter.h"
#include "wrapper.h"
#include "sub_0000_invoke.h"
int e_printf(const char *format, ...);
void update_detection_result(uint16_t index, signed short x, signed short y, signed short w, signed short h, int val_percent);
extern volatile uint32_t g_ai_inference_time_ms;
}

#define IMAGE_DATA_SIZE  (AI_INPUT_IMAGE_WIDTH * AI_INPUT_IMAGE_HEIGHT * AI_INPUT_IMAGE_BYTE_PER_PIXEL)
extern int8_t model_buffer_int8[IMAGE_DATA_SIZE];



static bool PresentInferenceResult(const std::vector<arm::app::object_detection::DetectionResult>& results)
{
    // Clear all previous results on display
    for (uint16_t i = 0; i < AI_MAX_DETECTION_NUM; i++)
    {
        update_detection_result(i, (signed short)0, (signed short)0, (signed short)0, (signed short)0, 0);
    }

    if (results.size() > 0) {
        e_printf("[NPU Result Present] Presenting %d ICs:\n", (int)results.size());
    }

    // Pass detected ICs to the display drawing loop
    for (uint16_t i = 0; i < results.size() && i < AI_MAX_DETECTION_NUM; ++i) {
        e_printf("  - IC %d: x0=%d, y0=%d, w=%d, h=%d, score=%d%%\n",
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

bool main_loop_face_detection()
{
    /* Copy the AI input image to tensor arena */
    memcpy(mera_input_ptr(), model_buffer_int8, IMAGE_DATA_SIZE);

    // Print first few input bytes for debugging to check if camera frame is zero/black
    e_printf("[NPU Input Dump] first 10 bytes: ");
    for (int i = 0; i < 10; ++i) {
        e_printf("%d ", (int)mera_input_ptr()[i]);
    }
    e_printf("\n");

    // Synchronize CPU-written input data with physical RAM for NPU access (unconditional for safety)
    SCB_CleanDCache_by_Addr((uint32_t *)mera_input_ptr(), IMAGE_DATA_SIZE);

    volatile uint32_t old_counter = TimeCounter_CurrentCountGet();

    /* Execute AI inference */
    int invoke_status = sub_0000_invoke(false);
    e_printf("[NPU Invoke] Status: %d\n", invoke_status);

    uint32_t new_counter = TimeCounter_CurrentCountGet();

    // Invalidate Dcache for the single FOMO output tensor (1728 bytes) so CPU reads from physical memory (unconditional for safety)
    SCB_InvalidateDCache_by_Addr((uint32_t *)mera_output_ptr(), 1728);

    uint32_t diff_cycles = new_counter - old_counter;
    e_printf("[NPU Time Debug] cycles: %lu\n", (unsigned long)diff_cycles);

    // Compute inference time in milliseconds (system clock is 480 MHz)
    application_processing_time.ai_inference_time_ms = (uint32_t)(((uint64_t)diff_cycles * 1000) / 480000000);
    g_ai_inference_time_ms = application_processing_time.ai_inference_time_ms;

    int8_t* output = (int8_t*)mera_output_ptr();

    // Diagnostics for NPU Output
    int8_t max_c0 = -128, min_c0 = 127;
    int8_t max_c1 = -128, min_c1 = 127;
    int8_t max_c2 = -128, min_c2 = 127;
    int32_t sum_c0 = 0, sum_c1 = 0, sum_c2 = 0;
    for (int i = 0; i < 24 * 24; ++i) {
        int8_t c0 = output[i * 3];
        int8_t c1 = output[i * 3 + 1];
        int8_t c2 = output[i * 3 + 2];
        sum_c0 += c0;
        sum_c1 += c1;
        sum_c2 += c2;
        if (c0 > max_c0) max_c0 = c0;
        if (c0 < min_c0) min_c0 = c0;
        if (c1 > max_c1) max_c1 = c1;
        if (c1 < min_c1) min_c1 = c1;
        if (c2 > max_c2) max_c2 = c2;
        if (c2 < min_c2) min_c2 = c2;
    }
    e_printf("[NPU Output Stats] C0: [%d, %d] avg %d | C1: [%d, %d] avg %d | C2: [%d, %d] avg %d\n",
             (int)min_c0, (int)max_c0, (int)(sum_c0 / 576),
             (int)min_c1, (int)max_c1, (int)(sum_c1 / 576),
             (int)min_c2, (int)max_c2, (int)(sum_c2 / 576));

    std::vector<arm::app::object_detection::DetectionResult> results;
    results.clear();

    // Instantiate and run post-processing with IC confidence threshold (e.g. 0.3)
    arm::app::DetectorPostProcess postProcess(output, results, 0.3f);

    if (!postProcess.DoPostProcess()) {
        e_printf("[PostProcess] DoPostProcess failed!\n");
        return false;
    }

    e_printf("[PostProcess] Detection results count: %d\n", (int)results.size());

    // Grid logs removed as requested to keep serial output clean

    if (!PresentInferenceResult(results)) {
        return false;
    }

    return true;
}
