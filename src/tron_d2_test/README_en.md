[日本語版 (README.md)](README.md)

# Dave2D Graphics Rendering Test (tron_d2_test)

## Program Description
This program runs on μT-Kernel 3.0 and tests the "D/AVE 2D (Dave2D)" 2D graphics accelerator and GLCDC (Global LCD Controller) on Renesas RA microcontrollers.
By allocating a triple buffer framebuffer in external SDRAM, it displays a flicker-free (tearing-free) "bouncing ball" animation synchronized with the Vblank (vertical synchronization) interrupt. In addition, it performs a connection verification test for the camera (OV5640) via I2C and a self-test of the physical SDRAM.

## Hardware & Peripherals
* **MCU / Board**: Renesas RA8 Series (e.g., EK-RA8D1)
* **Graphics**: D/AVE 2D Engine, GLCDC (driving 1024x600 TFT LCD Panel)
* **Camera (OV5640)**: Connection & ID read validation via I2C (slave address `0x3C`)
* **Memory**: External SDRAM (allocated for Triple Buffer framebuffers: 3 buffers in total)
* **I/O Pins**:
  * `CAMERA_RESET` (P709) - Camera reset pin
  * `DISP_RESET` (P511, etc.) - LCD reset pin
  * `DISP_BLEN` (P514) - LCD backlight control pin

## μT-Kernel 3.0 Task Configuration
1. **task_1** (Priority: 10, Stack size: 32KB)
   * Main processing task. Initializes peripherals (SDRAM, GLCDC, Dave2D), verifies camera connection, runs SDRAM self-test, and executes the D/AVE 2D rendering and Vblank synchronization loop.
2. **task_2** (Priority: 10, Stack size: 1KB)
   * Dummy task for monitoring system health. Outputs "task 2 running..." every 7 seconds.

## Processing Flow
1. **Cold Start Delay**: Waits 500 ms immediately after task startup to allow power supply stabilization.
2. **External SDRAM Init & Self-Test**: Writes test patterns to SDRAM and reads them back to verify memory integrity.
3. **AXI Bus Arbitration Settings**: Configures AXI bus slave arbitration to "Fixed Priority (Group 3)" to prioritize the GLCDC bus bandwidth.
4. **Camera Connection Check**: Toggles `CAMERA_RESET`, starts GPT to output a 24 MHz camera clock (XCLK), and reads OV5640 Product ID registers (`0x300a`, `0x300b`) via I2C to verify communication.
5. **LCD Reset & GLCDC Start**: Performs a robust hardware reset sequence (500ms High -> 200ms Low -> 500ms High) for the LCD, then enables GLCDC and the backlight.
6. **D/AVE 2D Engine Init**: Initializes Dave2D hardware and sets drawing parameters (e.g., copy blend mode, disabled anti-aliasing to save bandwidth).
7. **Triple-Buffered Rendering Loop**:
   * Draws the bouncing ball, static circles, and a box using `d2_startframe` and `d2_endframe`.
   * Manages cache coherency using `SCB_CleanInvalidateDCache` and blocks CPU execution using `d2_flushframe` until GPU completes rendering.
   * Requests a framebuffer flip using `R_GLCDC_BufferChange`.
   * Calls `tk_slp_tsk` to put the task to sleep. It is awakened immediately by `tk_wup_tsk` inside the Vblank interrupt callback (zero CPU overhead synchronization).
   * Rotates the buffer indices.

## Parameters & Definitions
* `DISPLAY_HSIZE_INPUT0` / `DISPLAY_VSIZE_INPUT0`: LCD display size.
* `CAMERA_RESET` (P709), `DISP_RESET` (P511), `DISP_BLEN` (P514): Control GPIO pins.
* `fb_background` / `fb_background_2`: Framebuffers allocated in SDRAM.

## Execution Log Example
```
microT-Kernel Version 3.00

Start User-main program (Camera & LCD D2D Test).
task 2 running...

=== Camera I2C & LCD D2D Connection Test Start ===
Resetting Camera (CAMERA_RESET -> P709)...
Starting GPT Clock for Camera XCLK (g_cam_clk)...
Opening I2C Master (g_cam_i2c_master)...
Reading OV5640 Product ID registers via I2C...
Product ID Read: H = 0x56, L = 0x40
SUCCESS: Camera connection verified! (OV5640 detected)
Testing physical SDRAM at address 0x68000000...
SDRAM verification SUCCESS!
=== Setting Bus Slave Arbitration to Fixed Priority (Group 3) ===
Clearing SDRAM framebuffers to black...
Initializing LCD (GLCDC)...
LCD Backlight enabled.
Initializing D/AVE 2D Graphics Engine...
Starting D/AVE 2D Rendering Loop (SDRAM Triple Buffer Bouncing Ball)...
Loop 0: rendering bouncing ball... (Vblank IRQs: 1)
GLCDC BufferChange Error: 1006
Loop 100: rendering bouncing ball... (Vblank IRQs: 101)
task 2 running...
Loop 200: rendering bouncing ball... (Vblank IRQs: 201)
task 2 running...
Loop 300: rendering bouncing ball... (Vblank IRQs: 301)
Loop 400: rendering bouncing ball... (Vblank IRQs: 401)
task 2 running...
Loop 500: rendering bouncing ball... (Vblank IRQs: 501)
Loop 600: rendering bouncing ball... (Vblank IRQs: 601)
task 2 running...
Loop 700: rendering bouncing ball... (Vblank IRQs: 701)
task 2 running...
```
