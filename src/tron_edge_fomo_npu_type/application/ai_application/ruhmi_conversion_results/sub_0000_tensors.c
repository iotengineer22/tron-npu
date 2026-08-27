#include "sub_0000_tensors.h"

const TensorInfo sub_0000_tensors[] = {
  { "_split_1_command_stream", 1, 5160, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 2, 39456, "MODEL", 0xffffffff },
  { "_split_1_scratch", 3, 147456, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 4, 147456, "FAST_SCRATCH", 0x0 },
  { "serving_default_x_0", 5, 27648, "INPUT_TENSOR", 0x9000 },
  { "StatefulPartitionedCall_0_70066", 0, 720, "OUTPUT_TENSOR", 0x0 },
};

const size_t sub_0000_tensors_count = sizeof(sub_0000_tensors) / sizeof(sub_0000_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0000_address_serving_default_x_0 = 0x9000;
const uint32_t sub_0000_address_StatefulPartitionedCall_0_70066 = 0x0;

