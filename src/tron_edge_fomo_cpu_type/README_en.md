[日本語版 (README.md)](README.md)

# FOMO Object Detection - CPU Version (tron_edge_fomo_cpu_type)

## Program Description
This program is an edge AI application running on μT-Kernel 3.0 that processes input images from a MIPI-CSI2 camera (OV5640) on the CPU, utilizing a lightweight "FOMO (Faster Objects, More Objects)" model to detect and display specific electronic components (e.g., devices on a PCB).
The captured 320x240 RGB565 camera image is scaled to 800x600 using Dave2D bilinear filtering and centered on the LCD screen. It overlays bounding boxes, class names, and confidence scores for detected objects in real time. The inference process runs asynchronously in the background using TensorFlow Lite Micro (CPU execution), coordinating with the rendering task.

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
   * Main rendering and camera control task. Handles camera initialization, capture, LCD scaling, overlays bounding boxes, and performs Vblank-synchronized page flipping. Wakens the AI task by calling `tk_wup_tsk(tskid_ai)` when it is not busy.
2. **task_2** (Priority: 10, Stack size: 1KB)
   * Dummy monitoring task. Outputs "task 2 running..." every 7 seconds.
3. **tskid_ai (task_ai)** (Priority: 11, Stack size: 32KB)
   * AI inference task. Assigned a lower priority (11) than the rendering task (10) to safeguard display responsiveness. Runs `ai_inference_task` to feed images into the TFLite Micro model and perform inference on the CPU.

## Processing Flow
1. **Initialization**: Configures SDRAM, GLCDC pin assignments, MIPI camera reset & start, and Dave2D graphics engine setup.
2. **Rendering & Vsync Loop (task_1)**:
   * Awakens upon Vblank interrupt.
   * Scales and copies camera frame (320x240 to 800x600) via `d2_blitcopy` using bilinear filtering.
   * Overlays bounding boxes and statistics using current AI results (coordinates, class ID, confidence).
   * Flushes GPU command lists and waits for GPU execution completion.
   * Flips the active framebuffer using GLCDC API.
   * Triggers the AI task (`tk_wup_tsk(tskid_ai)`) if it is idle (`!g_ai_task_busy`).
3. **AI Inference Loop (task_ai)**:
   * Waits for a wake-up signal (`tk_slp_tsk`).
   * Fetches the camera buffer, performs preprocessing.
   * Runs TensorFlow Lite Micro model inference on the CPU.
   * Stores the detected target classes and coordinates in a global array `g_ai_detection`.

## Key Parameters & Definitions
* **Target Classes (pcb_class_names)**:
  1. FPC
  2. nRF54L15
  3. Pico
  4. Xiao
* `AI_MAX_DETECTION_NUM`: Maximum number of objects that can be detected simultaneously.
* `g_ai_inference_time_ms`: Inference latency (ms) measured on the CPU.

## Execution Log Example
```text
microT-Kernel Version 3.00

Start User-main program (Camera & LCD D2D Test).
task 2 running...
=== Starting AI Inference Task (CPU-based FOMO) ===
Testing physical SDRAM at address 0x68000000...
SDRAM verification SUCCESS!

=== Camera MIPI-CSI2 & LCD Display D2D Start ===
=== Setting Bus Slave Arbitration to Fixed Priority (Group 3) ===
Clearing SDRAM framebuffers to black...
Initializing LCD (GLCDC)...
LCD Backlight enabled.
Initializing D/AVE 2D Graphics Engine...
Initializing MIPI-CSI2 Camera (OV5640)...
[Camera Init] Product ID High: 0x56, Low: 0x40
SUCCESS: Camera initialized and capture started.
Entering Real-time Camera Display Loop...
[VIN CB] First frame captured successfully!
Loop 0: buffer = 0x22073580, vsync_cnt = 15
  Buf Data: 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000
[Diag] Cam buffer: min=0x0000, max=0xDE9C, avg=0x0022 | NPU Input: min=-128, max=85, avg=-127
[CPU Input Dump] first 10 bytes: -128 -128 -128 -128 -128 -128 -128 -128 -128 -128
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133601684
[CPU Output Stats] C0: [101, 127] avg 122 | C1: [-128, -128] avg -128 | C2: [-128, -128] avg -128 | C3: [-128, -101] avg -122 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0000, max=0xFFFF, avg=0x2BCC | NPU Input: min=-128, max=127, avg=-83
[CPU Input Dump] first 10 bytes: -128 -112 -22 -128 -112 -5 -128 -100 -22 -128
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133654476
[CPU Output Stats] C0: [120, 127] avg 126 | C1: [-128, -128] avg -128 | C2: [-128, -127] avg -127 | C3: [-128, -120] avg -127 | C4: [-128, -127] avg -127
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0003, max=0xFFFF, avg=0x3D2F | NPU Input: min=-128, max=127, avg=-61
[CPU Input Dump] first 10 bytes: -120 -100 3 -128 -104 3 -120 -96 28 -120
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133663272
[CPU Output Stats] C0: [126, 127] avg 126 | C1: [-128, -128] avg -128 | C2: [-128, -126] avg -127 | C3: [-128, -126] avg -127 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0022, max=0xFFFF, avg=0x4555 | NPU Input: min=-128, max=127, avg=-56
[CPU Input Dump] first 10 bytes: -128 -92 3 -120 -100 -22 -112 -92 3 -120
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133601628
[CPU Output Stats] C0: [125, 127] avg 126 | C1: [-128, -128] avg -128 | C2: [-128, -126] avg -127 | C3: [-128, -125] avg -127 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0023, max=0xFFFF, avg=0x491A | NPU Input: min=-128, max=127, avg=-50
[CPU Input Dump] first 10 bytes: -120 -96 -5 -120 -96 -5 -120 -92 20 -112
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133654464
[CPU Output Stats] C0: [126, 127] avg 126 | C1: [-128, -128] avg -128 | C2: [-128, -126] avg -127 | C3: [-128, -127] avg -127 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0022, max=0xFFFF, avg=0x4CA1 | NPU Input: min=-128, max=127, avg=-46
[CPU Input Dump] first 10 bytes: -120 -88 -5 -128 -100 -13 -112 -84 3 -112
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133628044
[CPU Output Stats] C0: [125, 127] avg 126 | C1: [-128, -128] avg -128 | C2: [-128, -125] avg -127 | C3: [-128, -128] avg -128 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0023, max=0xFFFF, avg=0x4640 | NPU Input: min=-128, max=127, avg=-53
[CPU Input Dump] first 10 bytes: -128 -100 3 -128 -96 3 -120 -92 11 -112
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133522468
[CPU Output Stats] C0: [127, 127] avg 127 | C1: [-128, -128] avg -128 | C2: [-128, -127] avg -127 | C3: [-128, -127] avg -127 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0022, max=0xFFFF, avg=0x4157 | NPU Input: min=-128, max=127, avg=-60
[CPU Input Dump] first 10 bytes: -120 -100 -13 -120 -96 -13 -112 -92 11 -120
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133619288
[CPU Output Stats] C0: [-89, 127] avg 121 | C1: [-128, -128] avg -128 | C2: [-128, -126] avg -127 | C3: [-128, 88] avg -122 | C4: [-128, -126] avg -127
[PostProcess] Detection results count: 2
[NPU Result Present] Presenting 2 PCBs:
  - Pico 0: x0=4, y0=68, w=16, h=16, score=56%
[update_detection_result] idx=0, x=4, y=68, w=16, h=16, prob=56%, class=3
  - Pico 1: x0=20, y0=76, w=16, h=16, score=84%
[update_detection_result] idx=1, x=20, y=76, w=16, h=16, prob=84%, class=3
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0001, max=0xFFFF, avg=0x3C68 | NPU Input: min=-128, max=127, avg=-64
[CPU Input Dump] first 10 bytes: -128 -100 -5 -120 -104 -5 -120 -100 -5 -128
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133865668
[CPU Output Stats] C0: [-110, 127] avg 101 | C1: [-128, -128] avg -128 | C2: [-128, -114] avg -127 | C3: [-128, 110] avg -103 | C4: [-128, -118] avg -127
[PostProcess] Detection results count: 9
[NPU Result Present] Presenting 9 PCBs:
  - Pico 0: x0=20, y0=44, w=16, h=16, score=52%
[update_detection_result] idx=0, x=20, y=44, w=16, h=16, prob=52%, class=3
  - Pico 1: x0=28, y0=52, w=16, h=16, score=74%
[update_detection_result] idx=1, x=28, y=52, w=16, h=16, prob=74%, class=3
  - Pico 2: x0=28, y0=60, w=16, h=16, score=92%
[update_detection_result] idx=2, x=28, y=60, w=16, h=16, prob=92%, class=3
  - Pico 3: x0=36, y0=60, w=16, h=16, score=56%
[update_detection_result] idx=3, x=36, y=60, w=16, h=16, prob=56%, class=3
  - Pico 4: x0=12, y0=68, w=16, h=16, score=79%
[update_detection_result] idx=4, x=12, y=68, w=16, h=16, prob=79%, class=3
  - Pico 5: x0=20, y0=68, w=16, h=16, score=86%
[update_detection_result] idx=5, x=20, y=68, w=16, h=16, prob=86%, class=3
  - Pico 6: x0=28, y0=68, w=16, h=16, score=77%
[update_detection_result] idx=6, x=28, y=68, w=16, h=16, prob=77%, class=3
  - Pico 7: x0=12, y0=76, w=16, h=16, score=91%
[update_detection_result] idx=7, x=12, y=76, w=16, h=16, prob=91%, class=3
  - Pico 8: x0=20, y0=76, w=16, h=16, score=72%
[update_detection_result] idx=8, x=20, y=76, w=16, h=16, prob=72%, class=3
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0000, max=0xFFFF, avg=0x36A0 | NPU Input: min=-128, max=127, avg=-72
[CPU Input Dump] first 10 bytes: -128 -104 -22 -128 -108 -5 -128 -104 3 -128
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 134877664
[CPU Output Stats] C0: [-115, 127] avg 99 | C1: [-128, -128] avg -128 | C2: [-128, -117] avg -127 | C3: [-128, 115] avg -100 | C4: [-128, -88] avg -127
```
