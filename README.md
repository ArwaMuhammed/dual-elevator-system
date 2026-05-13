## Final Project: Collaborative Dual-Elevator System over SPI IPC
Course: Embedded Systems Level 1 (Cortex-M4)
Project Type: Distributed Reactive System & Hardware IPC Challenge

## Project Objective
Design and implement a coordinated, two-elevator control system serving 4 floors using two STM32 (Cortex-M4) MCUs. The MCUs communicate over a robust full-duplex SPI IPC link, and the Master runs a task-allocation algorithm to optimize building traffic.

## Hardware Architecture & IO Mapping (Summary)
- Master MCU (Board A): Dispatcher + Elevator A
- Slave MCU (Board B): Elevator B
- Inputs (EXTI only): cabin buttons (4), emergency stop (highest priority), hallway calls (Master only), floor sensors (4)
- Outputs: PWM LED motor simulation; UART telemetry (non-blocking, 500 ms updates)

## IPC Link (SPI Protocol Requirements)
- Full-duplex SPI, 4-wire (SCK, MOSI, MISO, CS)
- Slave preloads TX before Master transfer (non-blocking)
- Fixed-length 8-byte frame with header and checksum
- Periodic exchange (target ~50 ms) for real-time decisions

## Mandated Task Allocation Algorithm (Master)
- Comm Fault: on SPI timeout, Master takes all calls; Slave enters independent/emergency mode
- Immediate: idle elevator already at the floor
- Perfect Match: moving toward the floor in same direction
- Passed Match: same direction but already passed the floor (lower priority)
- Opposite Direction: do not assign until current path ends
- Idle: no directional match; assign nearest idle

## Software Engineering Requirements
- Concurrency: use `volatile` for ISR-shared flags
- Critical Sections: protect SPI RX/TX buffers via `Enter_Critical()`/`Exit_Critical()`
- FSM Design: both elevators must be explicit FSMs
- Non-Blocking Logic: polling allowed only for UART debug output; all other timing must use timers + interrupts (no busy wait)

## Point 6 (Lab Preparation) - Packet Definition (Required)
8-byte SPI frame diagram used by both MCUs:

Byte Index | 0    | 1     | 2     | 3    | 4         | 5     | 6      | 7
---------- | ---- | ----- | ----- | ---- | --------- | ----- | ------ | ----
Field      | HDR  | STATE | FLOOR | DIR  | REQ_BITS  | FLAGS | SPEED  | CHK
Details    | 0xA5 | FSM   | 0..3  | U/D  | 4-bit req | misc  | 0..100 | XOR

- CHK: XOR of bytes 0..6
- REQ_BITS: cabin requests bitmask (bit0 = Floor0 ... bit3 = Floor3)

## Point 7 (Evaluation)
Evaluation depends on discussion and demo; this README reflects the expected behavior and required protocol definition.

## Current Focus
- Finish UART telemetry accuracy and stability
- Finalize SPI fault handling behavior
- Validate routing and synchronization under stress
