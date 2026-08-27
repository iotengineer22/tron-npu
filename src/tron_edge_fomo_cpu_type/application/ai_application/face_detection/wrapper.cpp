#include <stdint.h>
#include "hal_data.h"
#include "application_config.h"
#include "wrapper.h"

// Define storage for CPU-based inference
// Aligned to 32 bytes for cache line safety and optimal performance on Cortex-M85.
#if (TENSOR_ARENA_ALLOCATION == ALLOCATE_TO_ONCHIP_RAM)
uint8_t cpu_model_storage[kBufferSize_sub_0000] BSP_ALIGN_VARIABLE(32);
#elif (TENSOR_ARENA_ALLOCATION == ALLOCATE_TO_SDRAM)
uint8_t cpu_model_storage[kBufferSize_sub_0000] BSP_PLACE_IN_SECTION(".sdram") BSP_ALIGN_VARIABLE(32);
#else
uint8_t cpu_model_storage[kBufferSize_sub_0000] BSP_ALIGN_VARIABLE(32);
#endif

int8_t cpu_model_output[720] BSP_ALIGN_VARIABLE(32);
