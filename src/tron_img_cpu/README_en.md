[日本語版 (README.md)](README.md)

# MobileNet Image Classification - CPU Version (tron_img_cpu)

## Program Description
This program is an edge AI application running on μT-Kernel 3.0 that performs real-time image classification using the "MobileNet V1" deep learning model on input images from a MIPI-CSI2 camera (OV5640).
It scales the captured 320x240 RGB565 camera image to 800x600 using Dave2D bilinear filtering. Concurrently, it converts the camera image to a 224x224 RGB888 format and runs classification inference on the CPU using TensorFlow Lite Micro. The Top-1 category name, confidence level (%), and inference processing latency are overlaid on the LCD screen in real time.

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
   * Main rendering, camera control, and inference kick task. Handles camera capturing, LCD scaling, image conversion to 224x224 RGB888 (`image_rgb565_to_rgb888`), overlays classification results, and triggers the AI inference task by calling `tk_wup_tsk(tskid_3)`.
2. **task_2** (Priority: 10, Stack size: 1KB)
   * Dummy monitoring task. Outputs "task 2 running..." every 7 seconds.
3. **tskid_3 (task_3)** (Priority: 11, Stack size: 32KB)
   * AI inference task. Assigned a priority of 11. Receives the data conversion completion notification (`tk_wup_tsk`) from `task_1` and executes the MobileNet V1 inference on the CPU using TensorFlow Lite Micro.

## Processing Flow
1. **Initialization**: Configures SDRAM, GLCDC pin assignments, MIPI camera, and Dave2D.
2. **Rendering & Inference Kick Loop (task_1)**:
   * Awakens upon Vblank interrupt.
   * Copies camera frame (320x240 to 800x600) via `d2_blitcopy` using bilinear filtering.
   * Draws the model name `Model: MobileNet V1` on the upper-left.
   * Overlays the previous inference latency (`g_ai_inference_time_ms`) on the upper-right.
   * If a new inference result is available, overlays the classified category name and confidence level (%) on the bottom of the screen.
   * Performs cache clean operations and requests a framebuffer flip using GLCDC.
   * Converts the camera frame to 224x224 RGB888, stores it in `model_buffer_int8`, cleans the cache, and awakens the AI task (`tk_wup_tsk(tskid_3)`).
3. **Inference Loop (task_3)**:
   * Waits for a wake-up signal.
   * Runs TensorFlow Lite Micro model inference.
   * Stores top probability results and sets `g_ai_result_new`.

## Key Parameters & Definitions
* **Model**: MobileNet V1 (Input: 224x224x3, int8 quantized)
* `model_buffer_int8`: Input tensor buffer (224x224 RGB888 size: 150,528 bytes)
* `g_ai_inference_time_ms`: Inference latency (ms) measured on the CPU.

## Execution Log Example
```
microT-Kernel Version 3.00

Start User-main program (Camera & LCD D2D Test).
task 2 running...
Testing physical SDRAM at address 0x68000000...
SDRAM verification SUCCESS!

=== Camera MIPI-CSI2 & LCD Display D2D Start ===
=== Setting Bus Slave Arbitration to Fixed Priority (Group 3) ===
Clearing SDRAM framebuffers to black...
Initializing LCD (GLCDC)...
=== Starting Arm Ethos-U55 NPU Task (task 3)... ===
SUCCESS: Arm Ethos-U55 NPU driver initialized successfully!
LCD Backlight enabled.
Initializing D/AVE 2D Graphics Engine...
Initializing MIPI-CSI2 Camera (OV5640)...
[Camera Init] Product ID High: 0x56, Low: 0x40
SUCCESS: Camera initialized and capture started.
Entering Real-time Camera Display Loop...
[VIN CB] First frame captured successfully!
Loop 0: buffer = 0x22074500, vsync_cnt = 15
  Buf Data: 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000
=== NPU Task: Woken up, starting inference... ===
    [MainLoop] Calling mera_invoke()...
    [sub_0000_invoke] Calling ethosu_invoke_v3...
    [sub_0000_invoke] ethosu_invoke_v3 returned: 0
    [MainLoop] mera_invoke() returned.
    [MainLoop] Output pointer: 0x2209C3D0
    [MainLoop] Creating mock TfLiteTensor...
    [MainLoop] Constructing Classifier and ImgClassPostProcess...
    [MainLoop] Calling DoPostProcess()...
    [MainLoop] Calling PresentInferenceResult()...
=== NPU Inference results (Top-5) ===
  Top-1: Category 905 (window shade), Prob: 61%
  Top-2: Category 904 (window screen), Prob: 26%
  Top-3: Category 794 (shower curtain), Prob: 1%
  Top-4: Category 591 (handkerchief), Prob: 1%
  Top-5: Category 753 (radiator), Prob: 0%
====================================
    [MainLoop] main_loop_image_classification COMPLETE!
=== NPU Task: Inference Success! Time: 17 ms ===
```
