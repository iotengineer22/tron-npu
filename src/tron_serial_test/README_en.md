[日本語版 (README.md)](README.md)

# Serial Communication Test (tron_serial_test)

## Program Description
This program runs on μT-Kernel 3.0 and serves as a fundamental serial communication test.
Utilizing the built-in T-Monitor functions (`tm_printf`, `tm_putstring`) of μT-Kernel, it outputs text messages to the serial debugging port from two independent tasks running at different intervals, verifying proper multi-tasking behavior and correct serial output wiring.

## Hardware & Peripherals
* **MCU / Board**: Renesas RA Series (e.g., EK-RA8D1, etc.)
* **Communications**: Debugging Serial Port (UART / T-Monitor Interface)

## μT-Kernel 3.0 Task Configuration
1. **task_1** (Priority: 10, Stack size: 1KB)
   * Serial output task 1. Outputs "task 1" every 500 ms.
2. **task_2** (Priority: 10, Stack size: 1KB)
   * Serial output task 2. Outputs "task 2" every 700 ms.

## Processing Flow
1. **Startup Message**: Prints "Start User-main program." via `tm_putstring` inside `usermain`.
2. **Task Creation & Execution**: Calls `tk_cre_tsk` and `tk_sta_tsk` to create and launch both task_1 and task_2 at the same priority level.
3. **Concurrent Output Processing**:
   * `task_1` wakes up every 500 ms and outputs text using `tm_printf`.
   * `task_2` wakes up every 700 ms and outputs text using `tm_printf`.
   * Both tasks are scheduled using `tk_dly_tsk` to yield the CPU and cooperate within the RTOS environment.

## Parameters & Definitions
* `ctsk_1` / `ctsk_2`: Task creation attribute structures (`T_CTSK`). The unused stack buffer pointer (`bufptr`) is explicitly initialized to `NULL` to suppress compiler warnings.

## Execution Log Example
```text
microT-Kernel Version 3.00

Start User-main program.
task 1
task 2
task 1
task 2
task 1
task 2
task 1
```
