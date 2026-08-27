[日本語版 (README.md)](README.md)

# MIPI Camera Display Test - Original Version (tron_mipi_test_ori)

## Program Description
This program runs on μT-Kernel 3.0 and represents the original/initial version of the camera display test program interfacing with a MIPI-CSI2 connected CMOS image sensor camera (OV5640) to display images in real time.
It captures a 320x240 RGB565 image from the camera, scales it to an 800x600 resolution (using bilinear filtering) via the D/AVE 2D engine, and renders it in the center of the LCD screen. By executing Vblank-synchronized page flips with a triple buffering architecture, it provides smooth, flicker-free real-time rendering.

## Hardware & Peripherals
* **MCU / Board**: Renesas RA8 Series (e.g., EK-RA8D1)
* **Graphics**: D/AVE 2D Engine, GLCDC (driving 1024x600 TFT LCD Panel)
* **Camera (OV5640)**: MIPI-CSI2 real-time image input (320x240 resolution)
* **Memory**: External SDRAM (allocated for Triple Buffer framebuffers)
* **Control I/O**:
  * `MIPI_IF_EN` - MIPI switch enable pin (connects the board multiplexer to the camera interface)
  * `LCD_RST` - LCD reset pin
  * `LCD_BLEN` - Backlight enable pin

## μT-Kernel 3.0 Task Configuration
1. **task_1** (Priority: 10, Stack size: 32KB)
   * Main camera display loop. Initializes SDRAM, GLCDC, the MIPI camera, and Dave2D, then handles camera frame acquisition, scaled GPU blitting to the active buffer, and Vblank-synchronized flipping using `tk_slp_tsk`.
2. **task_2** (Priority: 10, Stack size: 1KB)
   * Dummy monitoring task. Outputs system alive messages to the console every 7 seconds.

## Processing Flow
1. **Initialization**: Configures external SDRAM, GLCDC pin registers, toggles MIPI camera reset lines, launches camera capture DMA, and opens the Dave2D driver.
2. **Real-time Camera Display Loop**:
   * Puts task_1 to sleep until a Vblank interrupt is triggered (0% CPU polling overhead).
   * Verifies that the camera DMA pointer `p_camera_capture_buffer_stored` contains valid capture data.
   * Begins a new GPU command sequence using `d2_startframe`.
   * Scales and copies the 320x240 camera image to the 800x600 display framebuffer via `d2_blitcopy` with bilinear filtering enabled (`d2_tm_filter`), applying an X-offset of 112 to center the image.
   * Flushes the command list and blocks on the CPU until the GPU finishes rendering.
   * Switches the active GLCDC layer buffer using `R_GLCDC_BufferChange`.
   * Rotates the triple buffer indices.

## Parameters & Definitions
* `DISPLAY_HSIZE_INPUT0` / `DISPLAY_VSIZE_INPUT0`: LCD display resolution.
* `fb_background` / `fb_background_2`: SDRAM framebuffer arrays.

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
LCD Backlight enabled.
Initializing D/AVE 2D Graphics Engine...
Initializing MIPI-CSI2 Camera (OV5640)...
[Camera Init] Product ID High: 0x56, Low: 0x40
SUCCESS: Camera initialized and capture started.
Entering Real-time Camera Display Loop...
[VIN CB] First frame captured successfully!
Loop 0: buffer = 0x22053580, vsync_cnt = 15
  Buf Data: 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000
task 2 running...
Loop 100: buffer = 0x22008580, vsync_cnt = 274
  Buf Data: 0x18C3 0x18C4 0x08A6 0x08C5 0x08A5 0x10C5 0x10C5 0x08C4
task 2 running...
task 2 running...
Loop 200: buffer = 0x2202DD80, vsync_cnt = 533
  Buf Data: 0x10A4 0x18E4 0x10C6 0x08A6 0x18C5 0x10A5 0x18C5 0x20A5
```
