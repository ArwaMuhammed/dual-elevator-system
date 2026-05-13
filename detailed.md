# Detailed Report - Laboratory Preparation (Point 6)

## Register Map (Used Peripherals)

Peripheral | Base Address | Notes
--------- | ------------ | -----
RCC | 0x40023800 | AHB1/APB1/APB2 clock control
GPIOA | 0x40020000 | Cabin buttons, PWM, SPI NSS (Slave)
GPIOB | 0x40020400 | Floor sensors, SPI1 SCK/MISO/MOSI
GPIOC | 0x40020800 | Emergency, hallway calls (Master)
GPIOD | 0x40020C00 | USART2 TX/RX
EXTI | 0x40013C00 | EXTI line controller
SYSCFG | 0x40013800 | EXTI port mapping
NVIC | 0xE000E100 | Interrupt controller
TIM2 | 0x40000000 | PWM (motor LED)
TIM3 | 0x40000400 | FSM tick (10 ms)
TIM4 | 0x40000800 | Reserved/available
SPI1 | 0x40013000 | Full-duplex IPC
USART2 | 0x40004400 | Telemetry UART
DMA1 | 0x40026000 | UART TX DMA (Stream6 Ch4)

## PWM Math (LED Motor Simulation)
Target PWM frequency: 10 kHz

Given APB1 timer clock = 16 MHz:

f_PWM = 16,000,000 / ((PSC + 1) * (ARR + 1))

Using PSC = 0 and ARR = 1599:

f_PWM = 16,000,000 / (1 * 1600) = 10,000 Hz

## Packet Definition (8-byte SPI Frame)

Byte Index | 0    | 1     | 2     | 3    | 4         | 5     | 6      | 7
---------- | ---- | ----- | ----- | ---- | --------- | ----- | ------ | ----
Field      | HDR  | STATE | FLOOR | DIR  | REQ_BITS  | FLAGS | SPEED  | CHK
Details    | 0xA5 | FSM   | 0..3  | U/D  | 4-bit req | misc  | 0..100 | XOR

- HDR: fixed header (0xA5)
- STATE: elevator FSM state ID
- FLOOR: current floor index (0..3)
- DIR: current direction (UP/DOWN/IDLE)
- REQ_BITS: cabin request bitmask (bit0=Floor0 ... bit3=Floor3)
- FLAGS: status flags (e.g., emergency, doors open)
- SPEED: duty-cycle based speed (0..100)
- CHK: XOR checksum of bytes 0..6

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
- GPIOD:
  - PD5: USART2_TX (AF7)
  - PD6: USART2_RX (AF7)
- SPI1: master mode, software NSS on PB6
- USART2: 9600 baud, telemetry TX via DMA1 Stream6 Channel4

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
