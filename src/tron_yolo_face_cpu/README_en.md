[日本語版 (README.md)](README.md)

# YOLO Face Detection - CPU Version (tron_yolo_face_cpu)

## Program Description
This program is an edge AI application running on μT-Kernel 3.0 that performs real-time face detection on images from a MIPI-CSI2 camera (OV5640) using a YOLO-based neural network model.
It captures a 320x240 RGB565 camera image and displays it scaled to 800x600. It maps detected face coordinates from the 192x192 inference grid onto the LCD screen (scaling factor: `3.125f`, X-offset: `212.0f`) and overlays green bounding boxes and confidence scores (%). The inference is executed asynchronously on the CPU using TensorFlow Lite Micro.

## Hardware & Peripherals
* **MCU / Board**: Renesas RA8 Series (e.g., EK-RA8D1)
* **Graphics**: D/AVE 2D Engine, GLCDC (driving 1024x600 TFT LCD Panel)
* **Camera (OV5640)**: MIPI-CSI2 real-time image input (320x240 resolution)
* **Memory**: External SDRAM (allocated for Triple Buffer framebuffers and model tensors)
* **Control I/O**:
  * `MIPI_IF_EN` - MIPI switch enable pin
  * `LCD_RST` - LCD reset pin
  * `LCD_BLEN` - Backlight enable pin

## μT-Kernel 3.0 Task Configuration
1. **task_1** (Priority: 10, Stack size: 32KB)
   * Main rendering and camera control task. Handles camera capture, LCD scaling, overlays face bounding boxes, and performs Vblank-synchronized page flipping. Awakens the AI task by calling `tk_wup_tsk(tskid_ai)` when the inference task is idle.
2. **task_2** (Priority: 10, Stack size: 1KB)
   * Dummy monitoring task. Outputs "task 2 running..." every 7 seconds.
3. **tskid_ai (task_ai)** (Priority: 11, Stack size: 32KB)
   * AI inference task. Assigned a priority of 11. Runs `ai_inference_task` to execute the face detection model on the CPU using TensorFlow Lite Micro.

## Processing Flow
1. **Initialization**: Configures SDRAM, GLCDC pin assignments, MIPI camera reset & start, and Dave2D driver setup.
2. **Rendering & Vsync Loop (task_1)**:
   * Awakens upon Vblank interrupt.
   * Copies camera frame (320x240 to 800x600) via `d2_blitcopy` using bilinear filtering.
   * Maps 192x192 face detection coordinates to 800x600 screen coordinates (scaling factor: `3.125f`, X offset: `212.0f`) and draws green bounding boxes with confidence levels.
   * Overlays AI inference latency (`g_ai_inference_time_ms`) and detected face count (`Faces : %d`) on the upper-left of the screen.
   * Flashes GLCDC buffer and notifies the AI task (`tk_wup_tsk`).
3. **AI Inference Loop (task_ai)**:
   * Waits for a wake-up signal (`tk_slp_tsk`).
   * Runs TensorFlow Lite Micro model inference on the CPU and updates `g_ai_detection`.

## Key Parameters & Definitions
* `AI_MAX_DETECTION_NUM`: Maximum number of faces that can be detected simultaneously.
* **Coordinate Mapping Parameters**:
  * Scaling factor: `3.125f`
  * X offset: `212.0f`
* `g_ai_inference_time_ms`: Inference latency (ms) measured on the CPU.

## Execution Log Example
```
microT-Kernel Version 3.00

Start User-main program (Camera & LCD D2D Test).
task 2 running...
=== Starting AI Inference Task (CPU-based YOLO-Fastest) ===
Testing physical SDRAM at address 0x68000000...
SDRAM verification SUCCESS!

=== Camera MIPI-CSI2 & LCD Display D2D Start ===
=== Setting Bus Slave Arbitration to Fixed Priority (Group 3) ===
Clearing SDRAM framebuffers to black...
Initializing LCD (GLCDC)...
SUCCESS: TensorFlow Lite Micro initialized successfully!
Input tensor size: 36864 bytes, shape: 192 x 192 x 1
LCD Backlight enabled.
Initializing D/AVE 2D Graphics Engine...
Initializing MIPI-CSI2 Camera (OV5640)...
[Camera Init] Product ID High: 0x56, Low: 0x40
SUCCESS: Camera initialized and capture started.
Entering Real-time Camera Display Loop...
[VIN CB] First frame captured successfully!
Loop 0: buffer = 0x22073580, vsync_cnt = 15
  Buf Data: 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000
AI Inference: Found 0 face(s) in 2081 ms (Pre: 9 ms, Inv: 2072 ms, Post: 0 ms)
task 2 running...
AI Inference: Found 0 face(s) in 2091 ms (Pre: 9 ms, Inv: 2082 ms, Post: 0 ms)
AI Inference: Found 0 face(s) in 2090 ms (Pre: 9 ms, Inv: 2080 ms, Post: 0 ms)
AI Inference: Found 0 face(s) in 2090 ms (Pre: 9 ms, Inv: 2081 ms, Post: 0 ms)
AI Inference: Found 0 face(s) in 2090 ms (Pre: 9 ms, Inv: 2072 ms, Post: 9 ms)
Loop 100: buffer = 0x22028580, vsync_cnt = 274
  Buf Data: 0x0020 0x0020 0x0020 0x0020 0x0020 0x0040 0x0020 0x0040
task 2 running...
AI Inference: Found 1 face(s) in 2102 ms (Pre: 9 ms, Inv: 2092 ms, Post: 0 ms)
  Face 0: Box[18, 57, 61, 64], Conf: 97%
AI Inference: Found 1 face(s) in 2092 ms (Pre: 9 ms, Inv: 2083 ms, Post: 0 ms)
  Face 0: Box[0, 70, 56, 66], Conf: 97%
```
