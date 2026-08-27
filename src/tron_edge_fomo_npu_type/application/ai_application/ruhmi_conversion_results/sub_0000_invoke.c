#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "common_data.h"

#include "sub_0000_tensors.h"
#include "sub_0000_command_stream.h"
#include "sub_0000_model_data.h"

#include "sub_0000_invoke.h"

#include "application_config.h"

// Include Ethos-U driver headers (Assumed to be available)
#include "ethosu_driver.h"
extern struct ethosu_driver g_ethosu0;

// Define arenas with allocation and 32-byte alignment for cache line safety
#if (TENSOR_ARENA_ALLOCATION == ALLOCATE_TO_ONCHIP_RAM)
uint8_t sub_0000_arena[147456] BSP_ALIGN_VARIABLE(32);
#elif (TENSOR_ARENA_ALLOCATION == ALLOCATE_TO_SDRAM)
uint8_t sub_0000_arena[147456] BSP_PLACE_IN_SECTION(".sdram") BSP_ALIGN_VARIABLE(32);
#else
#error "Add your preferred buffer definition"
#endif

// Fast scratch arena not used for Ethos-U55
uint8_t* sub_0000_fast_scratch = sub_0000_arena;

int sub_0000_invoke(bool clean_outputs) {
  // Initialize base addresses and sizes
  uint64_t base_addrs[5] = {0};
  size_t base_addrs_size[5] = {0};
  int num_base_addrs = 5;

  // Variables for command stream
  uint8_t* cms_data = NULL;
  int cms_size = 0;

  // Prepare base_addrs and base_addrs_size arrays
  // Buffer sub_0000_model with size 39456 and address: 4294967295
  base_addrs[0] = (uint64_t)(uintptr_t)sub_0000_model_data;
  base_addrs_size[0] = sub_0000_model_data_size;
  // Buffer sub_0000_arena with size 147456 and address: 0
  base_addrs[1] = (uint64_t)(uintptr_t) (sub_0000_arena+0);
  base_addrs_size[1] = 147456;

  // Buffer sub_0000_fast_scratch with size 147456 and address: 0
  base_addrs[2] = (uint64_t)(uintptr_t) (sub_0000_arena+0);
  base_addrs_size[2] = 147456;

  // Buffer input_tensor_0 with size 27648 and address: 36864
  base_addrs[3] = (uint64_t)(uintptr_t) (sub_0000_arena+36864);
  base_addrs_size[3] = 27648;

  // Buffer output_tensor_0 with size 720 and address: 0
  if (clean_outputs) {
    memset(sub_0000_arena + 0, 0, 720);
  }
  base_addrs[4] = (uint64_t)(uintptr_t) (sub_0000_arena+0);
  base_addrs_size[4] = 720;

  // Command stream data
  cms_data = (uint8_t*)sub_0000_command_stream;
  cms_size = (int) sub_0000_command_stream_size;

  // Invoke the Ethos-U driver
  if (num_base_addrs > 8) {
    num_base_addrs = 8;
  }
  int result = ethosu_invoke_v3(&g_ethosu0, cms_data, cms_size, base_addrs, base_addrs_size, num_base_addrs, NULL);

  if (result == -1) {
    // Ethos-U invocation failed
    return -1;
  }

  return 0;
}
