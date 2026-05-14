# Detailed Report - Laboratory Preparation (Point 6)

## Register Map (Used Peripherals)

Peripheral | Base Address | Notes
--------- | ------------ | -----
RCC       | 0x40023800   | AHB1/APB1/APB2 clock control
GPIOA     | 0x40020000   | Cabin buttons, PWM, SPI NSS (Slave)
GPIOB     | 0x40020400   | Floor sensors, SPI1 SCK/MISO/MOSI
GPIOC     | 0x40020800   | Emergency, hallway calls (Master)
GPIOD     | 0x40020C00   | USART2 TX/RX
EXTI      | 0x40013C00   | EXTI line controller
SYSCFG    | 0x40013800   | EXTI port mapping
NVIC      | 0xE000E100   | Interrupt controller
TIM2      | 0x40000000   | PWM (motor LED)
TIM3      | 0x40000400   | FSM tick (10 ms)
TIM4      | 0x40000800   | Reserved/available
SPI1      | 0x40013000   | Full-duplex IPC
USART2    | 0x40004400   | Telemetry UART
DMA1      | 0x40026000   | UART TX DMA (Stream6 Ch4)

## PWM Math (LED Motor Simulation)
Target PWM frequency: 10 kHz

Given APB1 timer clock = 16 MHz:

f_PWM = 16,000,000 / ((PSC + 1) * (ARR + 1))

Using PSC = 0 and ARR = 1599:

f_PWM = 16,000,000 / (1 * 1600) = 10,000 Hz

## Packet Definition (8-byte SPI Frame)
**Optimization Note:** To strict adhere to the 8-byte frame requirement while transmitting all necessary data, **Bit-Packing** is utilized. The Target Floor and Current Floor are compressed into a single byte (`FloorsData`).

Byte Index | 0    | 1     | 2          | 3    | 4         | 5     | 6      | 7
---------- | ---- | ----- | ---------- | ---- | --------- | ----- | ------ | ----
Field      | HDR  | STATE | FLOORS_DATA| DIR  | REQ_BITS  | FLAGS | SPEED  | CHK
Details    | 0xA5 | FSM   | Tgt/Cur    | U/D  | 4-bit req | misc  | 0..100 | XOR

- **HDR (Byte 0):** Fixed synchronization header (`0xA5`).
- **STATE (Byte 1):** Elevator FSM state ID (e.g., IDLE, MOVING_UP).
- **FLOORS_DATA (Byte 2):** Packed byte. The **High Nibble** (bits 4-7) contains the `TargetFloor`. The **Low Nibble** (bits 0-3) contains the `CurrentFloor`.
- **DIR (Byte 3):** Current motor direction (UP/DOWN/IDLE).
- **REQ_BITS (Byte 4):** Cabin request bitmask (bit 0 = Floor 1 ... bit 3 = Floor 4).
- **FLAGS (Byte 5):** System status flags (Bit 0: Emergency, Bit 1: Doors Open, Bit 2: Comm Fault).
- **SPEED (Byte 6):** PWM duty-cycle mapped speed (0 to 100%).
- **CHK (Byte 7):** XOR checksum of Bytes 0 to 6 for data integrity validation.

## SPI1 Electrical/Timing Configuration

### SPI Mode (CPOL/CPHA)
Configured as **SPI Mode 0**:
- **CPOL = 0** → clock idle LOW
- **CPHA = 0** → sample on the first transition (rising edge)

### Bit Order
Data is **MSB-first** (LSBFIRST = 0, default).

### SPI Clock (BR prescaler)
In `Spi1_Init()` the BR field is set to `0b011` (BR = 3).

SPI prescaler table (STM32F4):
- BR=000 → /2
- BR=001 → /4
- BR=010 → /8
- **BR=011 → /16**
- BR=100 → /32
- BR=101 → /64
- BR=110 → /128
- BR=111 → /256

Assuming `PCLK2 = 16 MHz` (default clocking used by this project):

`f_SCK = PCLK2 / 16 = 1 MHz`

## Port & Pin Mapping (Per Peripheral)

### Master MCU (Board A)
- RCC: enables GPIOA/GPIOB/GPIOC/GPIOD, SYSCFG, TIM2/TIM3/TIM4, SPI1, USART2, DMA1
- GPIOA:
  - PA10/PA11/PA12/PA15: cabin buttons (EXTI lines 10/11/12/15)
  - PA5: PWM output (TIM2_CH1)
- GPIOB:
  - PB0/PB1/PB8/PB9: floor sensors (EXTI lines 0/1/8/9)
  - PB3: SPI1_SCK (AF5)
  - PB4: SPI1_MISO (AF5)
  - PB5: SPI1_MOSI (AF5)
  - PB6: SPI CS (software NSS, GPIO output)
- GPIOC:
  - PC13: emergency stop (EXTI line 13)
  - PC2/PC3/PC4/PC5/PC6/PC7: hallway calls (EXTI lines 2..7)
- GPIOA:
  - PA2: USART2_TX (AF7)
  - PA3: USART2_RX (AF7)
- SPI1: master mode, software NSS on PB6
- USART2: **9600 baud (8N1)**, telemetry TX via DMA1 Stream6 Channel4 (Proteus Safe Configuration)

### Slave MCU (Board B)
- RCC: enables GPIOA/GPIOB/GPIOC, SYSCFG, TIM2/TIM3/TIM4, SPI1
- GPIOA:
  - PA10/PA11/PA12/PA15: cabin buttons (EXTI lines 10/11/12/15)
  - PA5: PWM output (TIM2_CH1)
  - PA4: SPI1_NSS (hardware NSS, EXTI line 4 for CS rising)
- GPIOB:
  - PB0/PB1/PB8/PB9: floor sensors (EXTI lines 0/1/8/9)
  - PB3: SPI1_SCK (AF5)
  - PB4: SPI1_MISO (AF5)
  - PB5: SPI1_MOSI (AF5)
- GPIOC:
  - PC13: emergency stop (EXTI line 13)
- SPI1: slave mode, hardware NSS on PA4