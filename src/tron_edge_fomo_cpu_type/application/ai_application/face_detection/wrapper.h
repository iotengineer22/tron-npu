#ifndef MODEL_WRAPPER_H
#define MODEL_WRAPPER_H

#include <stdint.h>
#include "compute_sub_0000.h"

// Define storage and output buffers for CPU-based inference (allocated in wrapper.cpp)
extern uint8_t cpu_model_storage[kBufferSize_sub_0000];
extern int8_t cpu_model_output[720];

// The input buffer is defined in ai_inference.cpp
extern int8_t model_buffer_int8[];

static inline int8_t* mera_input_ptr() {
    return model_buffer_int8;
}

static inline int8_t* mera_output_ptr() {
    return cpu_model_output;
}

static inline void mera_invoke() {
    compute_sub_0000(cpu_model_storage, model_buffer_int8, cpu_model_output);
}

#endif // MODEL_WRAPPER_H
