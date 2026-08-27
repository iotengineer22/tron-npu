[日本語版 (README.md)](README.md)

# FOMO IC Detection - NPU Version (tron_edge_fomo_ic)

## Program Description
This program is an edge AI application running on μT-Kernel 3.0 that utilizes real-time image inputs from a MIPI-CSI2 camera (OV5640) to detect and count specific objects such as IC (Integrated Circuit) chips.
The captured 320x240 RGB565 camera image is scaled to 800x600 using bilinear filtering. It maps detected coordinates from the 192x192 inference scale onto the LCD screen, overlaying green bounding boxes and confidence scores. The inference is executed asynchronously on the Arm Ethos-U55 NPU accelerator.

## Hardware & Peripherals
* **MCU / Board**: Renesas RA8 Series (e.g., EK-RA8D1)
* **Graphics**: D/AVE 2D Engine, GLCDC (driving 1024x600 TFT LCD Panel)
* **Camera (OV5640)**: MIPI-CSI2 real-time image input (320x240 resolution)
* **Memory**: External SDRAM (allocated for Triple Buffer framebuffers)
* **Control I/O**:
  * `MIPI_IF_EN` - MIPI switch enable pin
  * `LCD_RST` - LCD reset pin
  * `LCD_BLEN` - Backlight enable pin

## μT-Kernel 3.0 Task Configuration
1. **task_1** (Priority: 10, Stack size: 32KB)
   * Main rendering and camera control task. Handles camera capturing, LCD scaling, overlays IC bounding boxes, and performs Vblank-synchronized flipping. Awakens the AI task if the inference task is idle.
2. **task_2** (Priority: 10, Stack size: 1KB)
   * Dummy monitoring task. Outputs "task 2 running..." every 7 seconds.
3. **tskid_ai (task_ai)** (Priority: 11, Stack size: 32KB)
   * AI inference task. Assigned a priority of 11. Executes `ai_inference_task` to run the TFLite Micro model on 192x192 downsampled images for IC detection.

## Processing Flow
1. **Initialization**: Configures SDRAM, GLCDC pin assignments, MIPI camera reset & start, and Dave2D setup.
2. **Rendering & Vsync Loop (task_1)**:
   * Awakens upon Vblank interrupt.
   * Copies camera frame (320x240 to 800x600) via `d2_blitcopy` using bilinear filtering.
   * Maps IC detection coordinates from 192x192 scale to 800x600 screen coordinates (scaling factor: `3.125f`, X offset: `212.0f`) and draws green bounding boxes with confidence levels.
   * Overlays AI inference latency (`g_ai_inference_time_ms`) and total detected IC count (`IC : %d`) on the upper-left of the screen.
   * Flashes GLCDC buffer and notifies the AI task (`tk_wup_tsk`).
3. **AI Inference Loop (task_ai)**:
   * Waits for a wake-up signal (`tk_slp_tsk`).
   * Feeds the preprocessed input buffer to the Arm Ethos-U55 NPU and executes inference on the hardware accelerator, updating `g_ai_detection`.

## Key Parameters & Definitions
* `AI_MAX_DETECTION_NUM`: Maximum number of objects that can be detected simultaneously.
* **Coordinate Mapping Parameters**:
  * Scaling factor: `3.125f`
  * X offset: `212.0f` (maps 192px width to 800px display width centered with camera offset)

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
Loop 0: buffer = 0x22073580, vsync_cnt = 15
  Buf Data: 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000
[Diag] Cam buffer: min=0x0000, max=0x0021, avg=0x0009 | NPU Input: min=-128, max=-120, avg=-127
[NPU Input Dump] first 10 bytes: -128 -128 -128 -128 -124 -128 -128 -128 -128 -128
[NPU Invoke] Status: 0
[NPU Time Debug] cycles: 5213572
[NPU Output Stats] C0: [127, 127] avg 127 | C1: [-128, -128] avg -128 | C2: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 10 ms
[Diag] Cam buffer: min=0x0000, max=0x18C3, avg=0x000C | NPU Input: min=-128, max=-96, avg=-127
[NPU Input Dump] first 10 bytes: -128 -128 -128 -128 -128 -120 -128 -124 -128 -128
[NPU Invoke] Status: 0
[NPU Time Debug] cycles: 5211780
[NPU Output Stats] C0: [127, 127] avg 127 | C1: [-128, -128] avg -128 | C2: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 10 ms
[Diag] Cam buffer: min=0x0000, max=0xFFFF, avg=0x391B | NPU Input: min=-128, max=127, avg=-78
[NPU Input Dump] first 10 bytes: -128 -108 -54 -120 -96 -46 -112 -104 -30 -120
[NPU Invoke] Status: 0
[NPU Time Debug] cycles: 5211708
[NPU Output Stats] C0: [-120, 127] avg 123 | C1: [-128, -128] avg -128 | C2: [-128, 120] avg -124
[PostProcess] Detection results count: 8
[NPU Result Present] Presenting 8 ICs:
  - IC 0: x0=28, y0=100, w=16, h=16, score=90%
[update_detection_result] idx=0, x=28, y=100, w=16, h=16, prob=90%
  - IC 1: x0=28, y0=108, w=16, h=16, score=93%
[update_detection_result] idx=1, x=28, y=108, w=16, h=16, prob=93%
  - IC 2: x0=28, y0=116, w=16, h=16, score=67%
[update_detection_result] idx=2, x=28, y=116, w=16, h=16, prob=67%
  - IC 3: x0=28, y0=132, w=16, h=16, score=95%
[update_detection_result] idx=3, x=28, y=132, w=16, h=16, prob=95%
  - IC 4: x0=28, y0=140, w=16, h=16, score=96%
[update_detection_result] idx=4, x=28, y=140, w=16, h=16, prob=96%
  - IC 5: x0=28, y0=148, w=16, h=16, score=95%
[update_detection_result] idx=5, x=28, y=148, w=16, h=16, prob=95%
  - IC 6: x0=28, y0=164, w=16, h=16, score=75%
[update_detection_result] idx=6, x=28, y=164, w=16, h=16, prob=75%
  - IC 7: x0=156, y0=164, w=16, h=16, score=96%
[update_detection_result] idx=7, x=156, y=164, w=16, h=16, prob=96%
AI Inference (NPU): IC detection complete in 10 ms
[Diag] Cam buffer: min=0x0043, max=0xFFFF, avg=0x4800 | NPU Input: min=-128, max=127, avg=-53
[NPU Input Dump] first 10 bytes: -112 -92 -46 -104 -80 -38 -96 -80 -22 -112
[NPU Invoke] Status: 0
[NPU Time Debug] cycles: 5211768
[NPU Output Stats] C0: [-120, 127] avg 125 | C1: [-128, -128] avg -128 | C2: [-128, 120] avg -126
[PostProcess] Detection results count: 3
[NPU Result Present] Presenting 3 ICs:
...
```
