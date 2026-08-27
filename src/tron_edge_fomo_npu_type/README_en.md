[日本語版 (README.md)](README.md)

# FOMO Object Detection - NPU Version (tron_edge_fomo_npu_type)

## Program Description
This program is an edge AI application running on μT-Kernel 3.0 that executes ultra-real-time lightweight object detection (FOMO) using the "Arm Ethos-U55" NPU accelerator embedded in Renesas RA8 microcontrollers.
It scales and centers the captured 320x240 RGB565 camera image to 800x600, overlaying green bounding boxes around target electronic components (FPC, Pico, Xiao, nRF54L15) on a PCB. By offloading neural network processing to the dedicated NPU hardware (reducing latency to a few milliseconds), it achieves high frame-rate object detection while maintaining minimal CPU utilization.

## Hardware & Peripherals
* **MCU / Board**: Renesas RA8 Series (e.g., EK-RA8D1)
* **AI Accelerator**: Arm Ethos-U55 NPU (Neural Processing Unit)
* **Graphics**: D/AVE 2D Engine, GLCDC (driving 1024x600 TFT LCD Panel)
* **Camera (OV5640)**: MIPI-CSI2 real-time image input (320x240 resolution)
* **Memory**: External SDRAM (allocated for Triple Buffer framebuffers and model tensors)
* **Control I/O**:
  * `MIPI_IF_EN` - MIPI switch enable pin
  * `LCD_RST` - LCD reset pin
  * `LCD_BLEN` - Backlight enable pin

## μT-Kernel 3.0 Task Configuration
1. **task_1** (Priority: 10, Stack size: 32KB)
   * Main rendering and camera control task. Copies camera frames, overlays bounding boxes, scales LCD via Dave2D, and triggers Vblank-synchronized page flips. Wakens the AI task by calling `tk_wup_tsk(tskid_ai)` when the inference task is idle.
2. **task_2** (Priority: 10, Stack size: 1KB)
   * Dummy monitoring task. Outputs "task 2 running..." every 7 seconds.
3. **tskid_ai (task_ai)** (Priority: 11, Stack size: 32KB)
   * AI inference task. Assigned a priority of 11. Executes `ai_inference_task` to communicate with the Arm Ethos-U55 NPU driver for ultra-fast hardware-accelerated neural network inference.

## Processing Flow
1. **Initialization**: Configures SDRAM, GLCDC pin assignments, MIPI camera, Dave2D engine, and opens the Arm Ethos-U55 NPU driver instance.
2. **Rendering & Vsync Loop (task_1)**:
   * Awakens upon Vblank interrupt.
   * Copies camera frame (320x240 to 800x600) via `d2_blitcopy` using bilinear filtering.
   * Maps 96x96 NPU detection coordinates to 800x600 screen coordinates (scaling factor: `6.25f`, X offset: `212.0f`) and draws green bounding boxes with class labels.
   * Overlays NPU inference latency (`g_ai_inference_time_ms`) and detected object count on the upper-left of the screen.
   * Flashes GLCDC buffer and notifies the AI task (`tk_wup_tsk`).
3. **AI Inference Loop (task_ai)**:
   * Waits for a wake-up signal (`tk_slp_tsk`).
   * Feeds the preprocessed input buffer to the Ethos-U55 NPU driver and executes inference on the hardware accelerator.
   * Stores the detected target classes and coordinates in `g_ai_detection`.

## Key Parameters & Definitions
* **Target Classes**: Background, FPC, nRF54L15, Pico, Xiao
* `AI_MAX_DETECTION_NUM`: Maximum number of objects that can be detected simultaneously.
* **Coordinate Mapping Parameters**:
  * Scaling factor: `6.25f`
  * X offset: `212.0f`

## Execution Log Example
```text
microT-Kernel Version 3.00

Start User-main program (Camera & LCD D2D Test).
task 2 running...
=== Starting AI Inference Task (NPU-based FOMO) ===
Testing physical SDRAM at address 0x68000000...
SDRAM verification SUCCESS!

=== Camera MIPI-CSI2 & LCD Display D2D Start ===
=== Setting Bus Slave Arbitration to Fixed Priority (Group 3) ===
Clearing SDRAM framebuffers to black...
Initializing LCD (GLCDC)...
SUCCESS: Arm Ethos-U55 NPU driver initialized successfully!
LCD Backlight enabled.
Initializing D/AVE 2D Graphics Engine...
Initializing MIPI-CSI2 Camera (OV5640)...
[Camera Init] Product ID High: 0x56, Low: 0x40
SUCCESS: Camera initialized and capture started.
Entering Real-time Camera Display Loop...
[VIN CB] First frame captured successfully!

[Diag] Cam buffer: min=0x0000, max=0xF77F, avg=0x001E | NPU Input: min=-128, max=110, avg=-127
[NPU Input Dump] first 10 bytes: -128 -128 -128 -128 -128 -128 -128 -128 -128 -128
[NPU Invoke] Status: 0
[NPU Time Debug] cycles: 2791736
[NPU Output Stats] C0: [99, 127] avg 121 | C1: [-128, -128] avg -128 | C2: [-128, -128] avg -128 | C3: [-128, -99] avg -121 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 5 ms

... (物体をかざすと C3:Picoクラスのスコアが急上昇) ...

[Diag] Cam buffer: min=0x0005, max=0xFFFF, avg=0x476A | NPU Input: min=-128, max=127, avg=-54
[NPU Input Dump] first 10 bytes: -112 -84 3 -112 -96 -13 -112 -84 11 -104
[NPU Invoke] Status: 0
[NPU Time Debug] cycles: 2791692
[NPU Output Stats] C0: [-97, 127] avg 117 | C1: [-128, -128] avg -128 | C2: [-128, -117] avg -127 | C3: [-128, 94] avg -119 | C4: [-128, -96] avg -127
[PostProcess] Detection results count: 4
[NPU Result Present] Presenting 4 PCBs:
  - Pico 0: x0=28, y0=68, w=16, h=16, score=72%
[update_detection_result] idx=0, x=28, y=68, w=16, h=16, prob=72%, class=3
  - Pico 1: x0=36, y0=68, w=16, h=16, score=69%
[update_detection_result] idx=1, x=36, y=68, w=16, h=16, prob=69%, class=3
  - Pico 2: x0=28, y0=76, w=16, h=16, score=86%
[update_detection_result] idx=2, x=28, y=76, w=16, h=16, prob=86%, class=3
  - Pico 3: x0=52, y0=76, w=16, h=16, score=83%
[update_detection_result] idx=3, x=52, y=76, w=16, h=16, prob=83%, class=3
AI Inference (NPU): IC detection complete in 5 ms
```
