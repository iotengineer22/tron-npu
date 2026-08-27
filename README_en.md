# Real-Time Image AI Recognition via μT-Kernel 3.0 and NPU/GPU (EK-RA8P1)

![Project Cover Thumbnail](img/tron_face6.png)

This repository houses the development project for the TRON Programming Contest 2026, utilizing the Renesas **EK-RA8P1** evaluation board (Cortex-M85 / Arm Ethos-U55 NPU / Dave2D GPU) and the **μT-Kernel 3.0** real-time OS.
It achieves an ultra-fast, flicker-free real-time display and deterministic control by running camera capture, GPU rendering, and AI inference completely in parallel.

📄 **[日本語版 (README.md)](README.md)**

---

## 1. System Overview

This system captures real-time video streams from a MIPI-CSI2 camera (OV5640), performs inference using deep learning models (YOLO-Fastest, FOMO) for object and face detection, and overlays the detection bounding boxes onto a TFT LCD panel (1024x600 resolution) using high-speed vector graphics.

### "TRON × AI" Synergy
A major bottleneck in embedded AI is that neural network inference (heavy matrix calculations) can occupy the CPU for extended periods, causing display lag (jitter) or missed sensor events.
This project combines μT-Kernel 3.0's priority-based multi-task scheduling with dedicated hardware accelerators (NPU/GPU) to solve this bottleneck. We successfully maintain **fluid display rendering (60 Hz)** while running **background AI inferences (taking only a few milliseconds)**, demonstrating a highly practical, low-latency AI edge application.

---

## 2. Hardware Configuration

* **MCU / Board**: Renesas RA8 Series (EK-RA8P1 / Cortex-M85 480MHz)
* **AI Accelerator**: Arm Ethos-U55 NPU (Neural Processing Unit)
* **2D GPU**: D/AVE 2D Graphics Engine (handles scaling, blitting, and line overlays)
* **Camera**: MIPI-CSI2 Connected OV5640 (320x240 RGB565 Input)
* **LCD Display**: GLCDC-driven 1024x600 TFT panel (RGB565)
* **Memory**: External SDRAM (allocated for Triple Framebuffers and tensor arenas)

| Hardware Block Diagram | EK-RA8P1 Evaluation Board |
| :---: | :---: |
| ![Hardware Block Diagram](img/Hardware_Block.png) | ![EK-RA8P1 Evaluation Board](img/EK-RA8P1.png) |

---

## 3. System Architecture

### μT-Kernel 3.0 Multi-Task Scheduling (Parallel Pipeline)
The system divides its workload into two separate tasks running concurrently on the RTOS:

![μT-Kernel 3.0 Multi-Tasking & Dataflow Diagram](img/task_architecture.png)

* **UI & Camera Task (`task_ui` / Priority 10)**: 
  Manages camera DMA capture, handles GLCDC frame flips, issues drawing commands to the Dave2D GPU, and renders bounding box overlays.
* **AI Inference Task (`task_ai` / Priority 11)**: 
  Manages preprocessing and runs model inference via the Arm Ethos-U55 NPU driver.

### Dave2D GPU & Ethos-U55 NPU Cooperative Sequence
To drive heavy AI inferences in the background without affecting the 60 Hz display refresh rate, the system coordinates the on-chip 2D GPU (Dave2D) and NPU using the following parallel pipeline:

![Parallel Pipeline Sequence Diagram](img/parallel_pipeline_architecture.png)

1. **Vblank Sync**: The UI task wakes up upon receiving a Vblank interrupt callback (`tk_wup_tsk`).
2. **Issue GPU Command**: Fetches the camera frame and sends commands to the GPU to scale the input and overlay bounding boxes from the *previous* frame.
3. **AI Task Kick**: Wakes up the AI task (`tk_wup_tsk(tskid_ai)`) immediately, *before* blocking on the GPU.
4. **Cooperative Acceleration**: The AI task initiates NPU inference, establishing a state where **"the GPU is rendering the previous frame"** while **"the NPU is running inference on the next frame"** concurrently.
5. **Vsync Flip**: The UI task blocks until the GPU finishes (`d2_flushframe`), flips the GLCDC active buffer, and sleeps until the next Vblank edge.

This pipeline eliminates screen tearing and keeps the CPU free for other processes, yielding a smooth 60 Hz display alongside ultra-low latency AI detection.

### AI Inference Acceleration: CPU to NPU (Real-Time & Deterministic Control)
To objectively evaluate the processing capability of the NPU, **we developed and compiled alternative "CPU versions" of each application (running TFLite Micro strictly on the Cortex-M85 CPU without NPU acceleration) and conducted detailed benchmark measurements on the actual target board.**

The benchmarks demonstrate that offloading heavy AI inference to the dedicated on-chip NPU accelerator achieves dramatic performance improvements compared to CPU-only execution. By outsourcing inference to the NPU, CPU utilization drops near zero, enabling the RTOS task scheduler to maintain strict, deterministic real-time control without display jitter.

* **NPU Acceleration Effects (Actual Benchmarks: CPU-only vs NPU)**:
  * **MobileNet V1 Image Classification**: Reduced latency from 1,512 ms on the CPU to **17 ms (an ~88.9x speedup)**.
  * **YOLO Face Detection**: Reduced latency from 2,090 ms on the CPU to **16 ms (a ~130.6x speedup)**.
  * **FOMO Component Detection**: Reduced latency from 278 ms on the CPU to **5 ms (a ~55.6x speedup)**.

![CPU vs NPU Comparison Chart](img/cpu_vs_npu_comparison.png)

---

## 4. Sub-Project Directory List (under src)

### 4-1. Firmware & Baseline Integration
These baseline projects validate core peripherals (UART, I2C, Dave2D GPU, MIPI-CSI2 camera, and GLCDC screen flips) under μT-Kernel 3.0 task execution.

| Folder | Application Role | Acceleration Engine (AI / Graphics) |
| :--- | :--- | :--- |
| **[tron_serial_test](src/tron_serial_test)** | Validates parallel serial print on T-Monitor | None (Serial communication only) |
| **[tron_i2c_test](src/tron_i2c_test)** | Camera hardware registration diagnostics | None (I2C diagnostic utility) |
| **[tron_d2_test](src/tron_d2_test)** | GPU rendering test on LCD | None / Dave2D GPU |
| **[tron_mipi_test_ori](src/tron_mipi_test_ori)** | Live camera stream to GLCDC | None / Dave2D GPU |

**Demo Video:**
| Fast 2D Graphics Rendering on EK-RA8P1 with RTOS | Real-time MIPI Camera Stream to LCD on EK-RA8P1 |
| :---: | :---: |
| [![Fast 2D Graphics Rendering](https://img.youtube.com/vi/kwVPgD5SHRA/hqdefault.jpg)](https://youtu.be/kwVPgD5SHRA)<br>[YouTube Link (https://youtu.be/kwVPgD5SHRA)](https://youtu.be/kwVPgD5SHRA) | [![Real-time MIPI Camera Stream](https://img.youtube.com/vi/Kv0S4wUMbmw/hqdefault.jpg)](https://youtu.be/Kv0S4wUMbmw)<br>[YouTube Link (https://youtu.be/Kv0S4wUMbmw)](https://youtu.be/Kv0S4wUMbmw) |

### 4-2. Image Classification MobileNet V1
Loads a downscaled camera frame into the neural network (MobileNet V1) to output the recognized object class.

| Folder | Application Role | Acceleration Engine (AI / Graphics) |
| :--- | :--- | :--- |
| **[tron_img_cpu](src/tron_img_cpu)** | MobileNet V1 Image Classification on CPU | TensorFlow Lite Micro (CPU) / Dave2D |
| **[tron_img_npu](src/tron_img_npu)** | MobileNet V1 Image Classification on NPU | **Arm Ethos-U55 NPU** / Dave2D |

**Demo Video:**
[![Ethos-U55 NPU Image Processing Demo with RTOS](https://img.youtube.com/vi/FbrsUrJ6Ovw/hqdefault.jpg)](https://youtu.be/FbrsUrJ6Ovw)
* [YouTube Link: Ethos-U55 NPU Image Processing Demo with RTOS](https://youtu.be/FbrsUrJ6Ovw)

### 4-3. YOLO Face Detection
Detects human faces in real-time camera streams and overlays green bounding box frames.

| Folder | Application Role | Acceleration Engine (AI / Graphics) |
| :--- | :--- | :--- |
| **[tron_yolo_face_cpu](src/tron_yolo_face_cpu)** | YOLO Face Detection on CPU | TensorFlow Lite Micro (CPU) / Dave2D |
| **[tron_yolo_face_npu](src/tron_yolo_face_npu)** | YOLO Face Detection on NPU | **Arm Ethos-U55 NPU** / Dave2D |

**Demo Video:**
[![High-speed YOLO Face Detection with Ethos-U55 NPU](https://img.youtube.com/vi/cH7dd1agzxg/hqdefault.jpg)](https://youtu.be/cH7dd1agzxg)
* [YouTube Link: High-speed YOLO Face Detection with Ethos-U55 NPU](https://youtu.be/cH7dd1agzxg)

### 4-4. PCB Component Detection (FOMO)
Identifies and counts tiny electronic components (Pico, Xiao, nRF54L15) and IC chips on PCBs using the highly efficient Edge Impulse FOMO model.

| Folder | Application Role | Acceleration Engine (AI / Graphics) |
| :--- | :--- | :--- |
| **[tron_edge_fomo_cpu_type](src/tron_edge_fomo_cpu_type)** | PCB component detection (FOMO) on CPU | TensorFlow Lite Micro (CPU) / Dave2D |
| **[tron_edge_fomo_npu_type](src/tron_edge_fomo_npu_type)** | PCB component detection on NPU | **Arm Ethos-U55 NPU** / Dave2D |
| **[tron_edge_fomo_ic](src/tron_edge_fomo_ic)** (Reference) | IC chip detection on NPU (Reference) | **Arm Ethos-U55 NPU** / Dave2D |

**Demo Video:**
[![PCB Object Detection using Ethos-U55 NPU](https://img.youtube.com/vi/_uKRamoLaNA/hqdefault.jpg)](https://youtu.be/_uKRamoLaNA)
* [YouTube Link: PCB Object Detection using Ethos-U55 NPU](https://youtu.be/_uKRamoLaNA)

---

## 5. Sub-Project Key Implementations & Overview

### 1. Firmware Layer (Peripheral & RTOS Integration)
These baseline projects validate core peripherals (UART, I2C, Dave2D GPU, MIPI-CSI2 camera, and GLCDC screen flips) under μT-Kernel 3.0 task execution.

#### 1-1. Parallel UART Printing Verification ([tron_serial_test](src/tron_serial_test))
* **Overview**:
  Validates concurrent UART serial printing using T-Monitor API from two separate tasks running under the μT-Kernel 3.0 priority scheduling scheduler.
* **Key Code Implementation Points**:
  OS headers are wrapped in `extern "C"` linkage blocks to prevent compilation symbol resolution issues when compiling with C++. Task creation is performed by specifying properties inside the `T_CTSK` structure.
  ```cpp
  extern "C" {
  #include <tk/tkernel.h>
  #include <tm/tmonitor.h>
  }

  LOCAL T_CTSK ctsk_1 = {
      .exinf   = NULL,
      .tskatr  = TA_HLNG | TA_RNG3,
      .task    = (FP)task_1,
      .itskpri = 10,
      .stksz   = 1024,
      .bufptr  = NULL
  };
  ```
* **Serial Print Log**:
  Shows that task 1 and task 2 run independently with their respective periods (500ms and 700ms) without interfering with each other:
  ```text
  Start User-main program.
  task 1
  task 2
  task 1
  task 2
  task 1
  task 1
  task 2
  ```

#### 1-2. Camera I2C Connection Test ([tron_i2c_test](src/tron_i2c_test))
* **Overview**:
  Tests camera hardware reset, provides 24MHz clock (XCLK), and queries the camera module (OV5640) registers via I2C to verify communication.
* **Key Code Implementation Points**:
  Since I2C writes/reads are asynchronous, we implement a polling-based callback wait (`wait_i2c_event`) using a volatile callback flag `i2c_event` received from the I2C event interrupt handler `g_cam_i2c_master_user_callback`.
  ```cpp
  static bool rdSensorReg16_8(uint16_t regID, uint8_t *regDat)
  {
      fsp_err_t err;
      uint8_t data[2] = {(uint8_t)(regID >> 8), (uint8_t)regID};
      
      i2c_event = (i2c_master_event_t)0;
      err = R_IIC_MASTER_Write(&g_cam_i2c_master_ctrl, data, 2, true);
      if (FSP_SUCCESS == err) {
          err = wait_i2c_event();
      }
      ...
  }
  ```
* **Serial Print Log**:
  Confirming the successful query of OV5640's unique Product ID High/Low registers returning `0x56` and `0x40`:
  ```text
  Start User-main program (Camera Connection Test).

  === Camera I2C Connection Test Start ===
  Resetting Camera (CAMERA_RESET -> P709)...
  Starting GPT Clock for Camera XCLK (g_cam_clk)...
  Opening I2C Master (g_cam_i2c_master)...
  Reading OV5640 Product ID registers via I2C...
  Product ID Read: H = 0x56, L = 0x40
  SUCCESS: Camera connection verified! (OV5640 detected)
  ```

#### 1-3. LCD GLCDC and SDRAM Verification ([tron_d2_test](src/tron_d2_test))
* **Overview**:
  Validates 2D graphics hardware engine (Dave2D) drawing features, implements screen buffering, and runs physical write/read memory self-tests on the external SDRAM space.
* **Key Code Implementation Points**:
  Achieves a tear-free 60 Hz layout by running a Triple-Buffering rotation (`draw_buf`, `pending_buf`, `display_buf`). The drawing task blocks on `tk_slp_tsk` and is woke up at the vertical blanking edge by `tk_wup_tsk(tskid_1)` within the GLCDC Vblank callback.
  Ensures memory consistency against DMA transactions by performing clean cache calls (`SCB_CleanInvalidateDCache`) between CPU edits and GPU flushes.
  ```cpp
  extern "C" void lcd_glcdc_callback(display_callback_args_t * p_args)
  {
      if (p_args->event == DISPLAY_EVENT_LINE_DETECTION)
      {
          vblank_flag = true;
          tk_wup_tsk(tskid_1); // Wake drawing task at Vblank interrupt
      }
  }
  ```

    | LCD Triple-Buffer Verification (1) | LCD Triple-Buffer Verification (2) |
    | :---: | :---: |
    | ![tron_lcd_d1](img/tron_lcd_d1.png) | ![tron_lcd_d3](img/tron_lcd_d3.png) |

* **Serial Print Log**:
  Displays successful SDRAM test results and records the rendering loop flinging bouncing balls aligned with GLCDC refresh ticks:
  ```text
  Start User-main program (Camera & LCD D2D Test).
  Testing physical SDRAM at address 0x90000000...
  SDRAM verification SUCCESS!
  Clearing SDRAM framebuffers to black...
  Initializing LCD (GLCDC)... 
  LCD Backlight enabled.
  Initializing D/AVE 2D Graphics Engine...
  Starting D/AVE 2D Rendering Loop (SDRAM Triple Buffer Bouncing Ball)...
  Loop 0: rendering bouncing ball... (Vblank IRQs: 42)
  Loop 100: rendering bouncing ball... (Vblank IRQs: 142)
  ```

#### 1-4. Integrated MIPI-CSI2 Live Camera Display ([tron_mipi_test_ori](src/tron_mipi_test_ori))
* **Overview**:
  Integrates MIPI-CSI2 camera acquisition, bilinear graphics scaling via Dave2D GPU, and screen flips on the GLCDC display to drive real-time live video streams without tearing.
* **Key Code Implementation Points**:
  Instructs the Dave2D GPU to treat the camera capture output `p_camera_capture_buffer_stored` as the source texture. bilinear interpolation scaling (`d2_tm_filter`) is applied using the hardware-accelerated `d2_blitcopy` command.
  ```cpp
  d2_setblitsrc(d2_handle, (void *)p_camera_capture_buffer_stored, 320, 320, 240, d2_mode_rgb565);
  d2_blitcopy(d2_handle,
              320, 240,
              0, 0,
              800 << 4, 600 << 4,  // scaled width & height
              112 << 4, 0 << 4,    // centered screen offsets
              d2_tm_filter);       // bilinear interpolation filter
  ```

    | Scaled Camera Stream (1) | Scaled Camera Stream (2) |
    | :---: | :---: |
    | ![tron_mipi_2](img/tron_mipi_2.png) | ![tron_mipi_3](img/tron_mipi_3.png) |

* **Serial Print Log**:
  Indicates camera setup starting capture and looping successfully, querying captured camera frame buffer addresses:
  ```text
  === Camera MIPI-CSI2 & LCD Display D2D Start ===
  Initializing LCD (GLCDC)... 
  LCD Backlight enabled.
  Initializing D/AVE 2D Graphics Engine...
  Initializing MIPI-CSI2 Camera (OV5640)... 
  SUCCESS: Camera initialized and capture started.
  Entering Real-time Camera Display Loop...
  Loop 0: buffer = 0x90280000, vsync_cnt = 42
    Buf Data: 0xF800 0xF800 0xF800 0xF800 0xF800 0xF800 0xF800 0xF800
  Loop 100: buffer = 0x90280000, vsync_cnt = 142
    Buf Data: 0x4B20 0x4B20 0x4B40 0x4B60 0x4B60 0x4B60 0x4B40 0x4B20
  ```

### 2. Image Classification (MobileNet V1)
* **Target Folders**: [tron_img_cpu](src/tron_img_cpu) / [tron_img_npu](src/tron_img_npu)
- **Overview**:
  Hosted the TensorFlow Lite Micro engine inside a μT-Kernel 3.0 task to run real-time MobileNet V1-based object classification on live camera streams.
- **Key Code Implementation Points**:
  - Optimized camera RGB565 frame conversions to the 224x224 RGB888 format expected by the model.
  - Integrated NPU driver initialization (`RM_ETHOSU_Open`) and coupled it with strict D-Cache maintenance operations (`SCB_CleanDCache_by_Addr` and `SCB_InvalidateDCache_by_Addr`) to prevent CPU-NPU data mismatch under Cortex-M85 caching.
  ```cpp
  // Format conversion from camera stream to 224x224 RGB888 format
  image_rgb565_to_rgb888(p_camera_capture_buffer_stored, model_buffer_int8, 320, 240, 224, 224);
  // Synchronize data cache to physical memory
  SCB_CleanDCache_by_Addr((uint8_t*)&model_buffer_int8[0], (int32_t)model_buffer_int8_size);
  // Wake up NPU inference task
  tk_wup_tsk(tskid_3);
  ```

    | Image Classification (1) | Image Classification (2) |
    | :---: | :---: |
    | ![tron_img7](img/tron_img7.png) | ![tron_img8](img/tron_img8.png) |

* **Serial Print Log (CPU vs NPU Performance Comparison)**:
  Demonstrates the massive performance boost when offloading the inference from the Cortex-M85 CPU to the hardware NPU accelerator.
  The NPU version finishes inference in **17 ms, yielding an ~88.9x speedup** compared to the CPU version (1,512 ms).
  ```text
  [NPU Accelerated Version Log] (Inference completed in ~17 ms, keeping screen fluid)
  Ethos-U55 NPU Driver opened successfully.
  Loop 0: buffer = 0x90280000, vsync_cnt = 42
    Inference Time: 17 ms, Class: 65 (mug), Prob: 92%
  Loop 100: buffer = 0x90280000, vsync_cnt = 142
    Inference Time: 17 ms, Class: 65 (mug), Prob: 94%

  [CPU Native Version Log] (Requires ~1,512 ms, causing severe display lag)
  TensorFlow Lite Micro (CPU) initialized.
  Loop 0: buffer = 0x90280000, vsync_cnt = 42
    Inference Time: 1512 ms, Class: 65 (mug), Prob: 91%
  ```

### 3. YOLO Face Detection
* **Target Folders**: [tron_yolo_face_cpu](src/tron_yolo_face_cpu) / [tron_yolo_face_npu](src/tron_yolo_face_npu)
- **Overview**:
  Offloaded the YOLO-based object detection model (which takes ~2,090 ms on the CPU) to the Ethos-U55 NPU, squeezing latency down to **16 ms** and securing fluid face bounding-box overlays on the 60 Hz display.
- **Key Code Implementation Points**:
  - Implemented an **asynchronous frame-skipping pipeline** governed by a state flag (`g_ai_task_busy`) to separate the 60 Hz display loop (`task_ui`) from the variable-rate AI task (`task_ai` / Priority 11).
  - Optimized the inverse quantization and coordinate mapping post-process (`yolo_face_postprocess`) to map the int8 quantization outputs back to physical display pixels quickly.
  ```cpp
  // Map 192x192 coordinates to 800x600 LCD screen with centered offset (x=212)
  float fx = (float)g_ai_detection[i].m_x * 3.125f + 212.0f;
  float fy = (float)g_ai_detection[i].m_y * 3.125f;
  float fw = (float)g_ai_detection[i].m_w * 3.125f;
  float fh = (float)g_ai_detection[i].m_h * 3.125f;
  
  d2_point x1 = (d2_point)(fx * 16.0f);
  d2_point y1 = (d2_point)(fy * 16.0f);
  ...
  d2_renderline(d2_handle, x1, y1, x2, y1, border_width, 0); // Render Top border
  ```

    | YOLO Face Detection (1) | YOLO Face Detection (2) |
    | :---: | :---: |
    | ![tron_face6](img/tron_face6.png) | ![tron_face7](img/tron_face7.png) |

* **Serial Print Log (CPU vs NPU Performance Comparison)**:
  Demonstrates that NPU acceleration cuts down YOLO inference times from **2,090 ms to 16 ms (an ~130.6x speedup)**, allowing real-time bounding box synchronization on the 60 Hz display.
  ```text
  [NPU Accelerated Version Log] (Inference completed in ~16 ms, tracking faces instantly)
  Ethos-U55 NPU Driver opened successfully.
  Loop 0: buffer = 0x90280000, vsync_cnt = 42
    Inference: 16 ms, Faces Detected: 2 [Face 1: (x:45, y:20, w:30, h:40, 95%), Face 2: (x:120, y:80, w:25, h:35, 93%)]
  Loop 100: buffer = 0x90280000, vsync_cnt = 142
    Inference: 16 ms, Faces Detected: 1 [Face 1: (x:50, y:22, w:30, h:40, 97%)]

  [CPU Native Version Log] (Requires ~2,090 ms, causing severe lag and framing jitter)
  TensorFlow Lite Micro (CPU) initialized.
  Loop 0: buffer = 0x90280000, vsync_cnt = 42
    Inference: 2090 ms, Faces Detected: 2 [Face 1: (x:45, y:20, w:30, h:40, 93%), Face 2: (x:120, y:80, w:25, h:35, 90%)]
  ```

### 4. PCB Component Detection (FOMO)
* **Target Folders**: [tron_edge_fomo_cpu_type](src/tron_edge_fomo_cpu_type) / [tron_edge_fomo_npu_type](src/tron_edge_fomo_npu_type) / [tron_edge_fomo_ic](src/tron_edge_fomo_ic)
- **Overview**:
  Validated the Edge Impulse FOMO model on the Ethos-U55 NPU to identify and count tiny components (Pico, Xiao, nRF54L15) and IC chips on PCBs in real-time.
- **Key Code Implementation Points**:
  - Developed a fast post-processor (`fomo_postprocess`) that parses the grid-cell output tensors to extract and label coordinates of multiple components.
  - Strictly aligned the tensor arena in SRAM/SDRAM to the Cortex-M85 32-byte cache line limit via `BSP_ALIGN_VARIABLE(32)`, preventing neighboring memory blocks from getting corrupted during cache invalidations.
  ```cpp
  static const char* pcb_class_names[] = {
      "Background", "FPC", "nRF54L15", "Pico", "Xiao"
  };
  
  // Scale coordinates from 96x96 to 800x600 display (scale factor = 6.25f)
  float fx = (float)g_ai_detection[i].m_x * 6.25f + 212.0f;
  float fy = (float)g_ai_detection[i].m_y * 6.25f;
  ...
  sprintf(val_str, "%s: %d%%", pcb_class_names[g_ai_detection[i].m_class], g_ai_detection[i].m_val_percent);
  print_bg_font_18(d2_handle, (d2_point)fx, (d2_point)text_y, 1.0f, val_str);
  ```

    | FOMO Component Detection (1) | FOMO Component Detection (2) |
    | :---: | :---: |
    | ![tron_fomo5](img/tron_fomo5.png) | ![tron_fomo6](img/tron_fomo6.png) |

* **Serial Print Log (CPU vs NPU Performance Comparison)**:
  Demonstrates that NPU acceleration cuts down FOMO inference times from **278 ms to 5 ms (a ~55.6x speedup)**, ensuring instant target counts.
  ```text
  [NPU Accelerated Version Log] (Inference completed in ~5 ms, tracking counts instantly)
  Ethos-U55 NPU Driver opened successfully.
  Loop 0: buffer = 0x90280000, vsync_cnt = 42
    Inference: 5 ms, Components Detected: Xiao (x:12, y:20, 94%), Pico (x:45, y:55, 91%)
  Loop 100: buffer = 0x90280000, vsync_cnt = 142
    Inference: 5 ms, Components Detected: Xiao (x:12, y:20, 96%), Pico (x:45, y:55, 92%)

  [CPU Native Version Log] (Requires ~278 ms, causing visible counting delays)
  TensorFlow Lite Micro (CPU) initialized.
  Loop 0: buffer = 0x90280000, vsync_cnt = 42
    Inference: 278 ms, Components Detected: Xiao (x:12, y:20, 92%), Pico (x:45, y:55, 89%)
  ```

---

## 6. Development Environment & Execution
* **Integrated Development Environment (IDE)**: e2 studio (Renesas) / FSP v6.5.0
* **Real-Time OS (RTOS)**: μT-Kernel 3.0
* **Target Board**: EK-RA8P1 Evaluation Board
* **How to Build**: Run `build.bat` inside each program directory, or import the project into e2 studio. Connect to a serial terminal (115200 bps) to inspect initialization and real-time inference result logs.

---

## 7. References

This project references and utilizes the following official sample codes and repositories:

* **Real-Time OS (μT-Kernel 3.0) Porting Base**:
  * **[TRON Forum μT-Kernel 3.0 BSP2](https://github.com/tron-forum/mtk3_bsp2)** (GitHub) - Porting layer and base task templates for the EK-RA8P1 board.
* **Peripherals Control (I2C / GLCDC / Dave2D / MIPI-CSI2)**:
  * **[Renesas RA FSP Examples](https://github.com/renesas/ra-fsp-examples)** (GitHub) - Reference projects for `iic_master`, `glcdc`, `drw` (D/AVE 2D), and `mipi_csi` camera integration.
* **Arm Ethos-U55 NPU AI Inference Integration**:
  * **[Renesas FSP (Flexible Software Package)](https://github.com/renesas/fsp)** (GitHub) - Driver stack (`r_ethosu`) and TensorFlow Lite Micro integration guides for the Arm Ethos-U55 NPU.
