[日本語版 (README.md)](README.md)

# I2C Camera Connection Verification Test (tron_i2c_test)

## Program Description
This program runs on μT-Kernel 3.0 and verifies the I2C communication interface with the camera module (OV5640) connected to the Renesas RA microcontroller.
It toggles the camera's hardware reset pin, uses GPT (General Purpose Timer) clock outputs to provide an active clock to the camera, and reads the Product ID registers from the OV5640 via the I2C master driver to verify that the camera is properly connected and recognized.

## Hardware & Peripherals
* **MCU / Board**: Renesas RA8 Series (e.g., EK-RA8D1)
* **Camera (OV5640)**: I2C (IIC channel 1) communication (slave address `0x3C`)
* **GPT Timer**: Generates camera XCLK (24 MHz clock) outputting on P501 pin
* **Control I/O**:
  * `CAMERA_RESET` (P709) - Camera hardware reset control pin

## μT-Kernel 3.0 Task Configuration
1. **task_1** (Priority: 10, Stack size: 2KB)
   * Main I2C test task. Runs the camera reset sequence, enables the camera clock, opens the I2C master driver, and executes the read test for OV5640 Product ID registers (`0x300a`, `0x300b`). It then loops indefinitely with a 10-second interval regardless of the validation outcome.
2. **task_2** (Priority: 10, Stack size: 1KB)
   * Dummy monitoring task. Periodically outputs system status messages to the console every 7 seconds.

## Processing Flow
1. **Camera Reset**: Pulls `CAMERA_RESET` Low for 100 ms, then drives it High and waits 10 ms (hardware initialization).
2. **XCLK Generation**: Opens the GPT driver (g_cam_clk) and starts outputting a 24 MHz clock signal on the P501 pin.
3. **I2C Init**: Opens the I2C master driver (g_cam_i2c_master) and sets the slave address to 7-bit address `0x3C` (OV5640 default).
4. **Product ID Validation**:
   * Reads the high-byte from register `0x300A` and the low-byte from register `0x300B`.
   * Checks if the retrieved values match the expected Product ID for the OV5640 (`0x5640`, `0x5641`, or `0x564C`).
   * Prints the verification outcome (SUCCESS/ERROR) to the console.
5. **Periodic Sleep Loop**: Subsequently, task_1 delays itself (`tk_dly_tsk`) for 10 seconds before looping.

## Key Parameters & Definitions
* `CAMERA_RESET` (P709): Camera reset GPIO pin.
* Slave Address: `0x3C` (OV5640)
* Verified Registers:
  * Product ID High: `0x300a` (Expected: `0x56`)
  * Product ID Low: `0x300b` (Expected: `0x40`, `0x41`, or `0x4C`)

## Execution Log Example
```text
=== Camera I2C Connection Test Start ===
Resetting Camera (CAMERA_RESET -> P709)...
task 2 running...
Starting GPT Clock for Camera XCLK (g_cam_clk)...
Opening I2C Master (g_cam_i2c_master)...
Reading OV5640 Product ID registers via I2C...
Product ID Read: H = 0x56, L = 0x40
SUCCESS: Camera connection verified! (OV5640 detected)
task 1 heart beat...
task 1 heart beat...
task 2 running...
```
