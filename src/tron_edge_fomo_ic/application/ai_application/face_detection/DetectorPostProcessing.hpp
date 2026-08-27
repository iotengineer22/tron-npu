#ifndef DETECTOR_POST_PROCESSING_HPP
#define DETECTOR_POST_PROCESSING_HPP

#include "DetectionResult.hpp"
#include <vector>
#include <stdint.h>

namespace arm {
namespace app {

    class DetectorPostProcess {
    public:
        /**
         * @brief        Constructor.
         * @param[in]    modelOutput         Pointer to the FOMO model output buffer.
         * @param[out]   results             Vector of detected results.
         * @param[in]    threshold           Float threshold of detection (0.0 to 1.0).
         **/
        explicit DetectorPostProcess(int8_t* modelOutput,
                                     std::vector<object_detection::DetectionResult>& results,
                                     float threshold = 0.5f);

        /**
         * @brief    Performs FOMO post-processing of the result of inference.
         * @return   true if successful, false otherwise.
         **/
        bool DoPostProcess();

    private:
        int8_t* m_modelOutput;
        std::vector<object_detection::DetectionResult>& m_results;
        float m_threshold;
    };

} /* namespace app */
} /* namespace arm */

#endif /* DETECTOR_POST_PROCESSING_HPP */
