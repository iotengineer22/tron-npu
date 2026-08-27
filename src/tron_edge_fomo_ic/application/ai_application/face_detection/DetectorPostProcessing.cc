#include "DetectorPostProcessing.hpp"

namespace arm {
namespace app {

DetectorPostProcess::DetectorPostProcess(
        int8_t* modelOutput,
        std::vector<object_detection::DetectionResult>& results,
        float threshold)
        :   m_modelOutput{modelOutput},
            m_results{results},
            m_threshold{threshold}
{
}

bool DetectorPostProcess::DoPostProcess()
{
    m_results.clear();

    // Map float threshold [0.0, 1.0] to int8 range [-128, 127] using scale 1/256.0
    int8_t threshold_int8 = (int8_t)(m_threshold * 256.0f - 128.0f);

    // FOMO model output shape: [1, 24, 24, 3]
    const int grid_size = 24;
    const int num_classes = 3; // 0: background, 1: unused/dummy, 2: connector

    for (int y = 0; y < grid_size; ++y)
    {
        for (int x = 0; x < grid_size; ++x)
        {
            int cell_index = (y * grid_size + x) * num_classes;
            int8_t bg_score   = m_modelOutput[cell_index];
            int8_t conn_score = m_modelOutput[cell_index + 2]; // Connector class is at index 2

            // If connector score is higher than background and exceeds the threshold
            if (conn_score > bg_score && conn_score >= threshold_int8)
            {
                // Normalize INT8 score to [0.0, 1.0] probability using zero_point=-128, scale=1/256.0
                double normalisedVal = (double)(conn_score + 128) / 256.0;

                // Center coordinates of this 8x8 pixels cell in the 192x192 input image
                int32_t center_x = x * 8 + 4;
                int32_t center_y = y * 8 + 4;

                // Define a fixed-size bounding box of 16x16 pixels centered on this cell
                int32_t w = 16;
                int32_t h = 16;
                int32_t x0 = center_x - w / 2;
                int32_t y0 = center_y - h / 2;

                // Clamp coordinates to image boundaries
                if (x0 < 0) x0 = 0;
                if (y0 < 0) y0 = 0;
                if (x0 + w > 192) w = 192 - x0;
                if (y0 + h > 192) h = 192 - y0;

                m_results.push_back(object_detection::DetectionResult(normalisedVal, x0, y0, w, h));
            }
        }
    }

    return true;
}

} // namespace app
} // namespace arm
