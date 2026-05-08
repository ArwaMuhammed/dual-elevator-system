## 📁 Project Structure (Application Layer)

The project is organized into three main application modules to clearly separate responsibilities between shared logic, the Master controller, and the Slave controller.

### 🔹 `App_Common`

Contains all shared logic used by both MCUs.

* Elevator finite state machine (FSM)
* Shared data structures (states, directions, requests)

This layer ensures both elevators follow consistent behavior.

---

### 🔹 `Master_App`

Implements the logic for the Master MCU (Elevator A).

Responsibilities:

* Reads cabin requests
* Controls Elevator A locally

This module acts as the **decision-making brain** of the system.

---

### 🔹 `Slave_App`

Implements the logic for the Slave MCU (Elevator B).

Responsibilities:

* Controls Elevator B using the shared FSM

This module acts as an **execution unit** with no independent decision-making.

---

## 🚀 Development Phases

The project will be developed incrementally to ensure stability and proper integration.

### Phase 1 — Single Elevator Foundation

* Implement and test Elevator FSM (states, movement, doors)
* Verify button inputs (EXTI) and motor simulation (PWM)

---

### Phase 2 — Master Dispatcher Logic

* Implement request handling (hallway + cabin)
* Develop and test the elevator assignment algorithm
* Use simulated data for the second elevator

---

### Phase 3 — SPI Communication

* Implement basic SPI Master/Slave communication
* Design and test the fixed-length communication frame
* Ensure reliable full-duplex data exchange

---

### Phase 4 — Dual Elevator Integration

* Connect both MCUs via SPI
* Integrate Slave control with real commands
* Synchronize states between Master and Slave

---

### Phase 5 — System Timing & Stability

* Add timer-based scheduling (no blocking delays)
* Ensure periodic SPI exchange and telemetry updates
* Handle high-frequency inputs safely

---

### Phase 6 — Finalization & Testing

* Implement emergency handling and fault scenarios
* Validate system behavior under all conditions
* Optimize performance and clean architecture

---

## Standalone Mode (No SPI Link)

The current firmware runs both boards independently with no SPI connection.
Leave PB3/PB4/PB5/PB6 unconnected between boards.

## Hardware Pin Map (EXTI-safe)

### Master MCU (Board A)

Cabin buttons (pull-up, falling edge, button to GND):
* Floor 1: PA10 (EXTI10)
* Floor 2: PA11 (EXTI11)
* Floor 3: PA12 (EXTI12)
* Floor 4: PA15 (EXTI15)

Floor sensors (pull-down, rising edge, button to 3.3V):
* Sensor 1: PB0 (EXTI0)
* Sensor 2: PB1 (EXTI1)
* Sensor 3: PB8 (EXTI8)
* Sensor 4: PB9 (EXTI9)

Emergency stop (pull-up, falling edge, button to GND):
* PC13 (EXTI13)

PWM motor LED:
* PA5 (TIM2_CH1) -> LED + 220 ohm -> GND


### Slave MCU (Board B)

Cabin buttons (pull-up, falling edge, button to GND):
* Floor 1: PA10 (EXTI10)
* Floor 2: PA11 (EXTI11)
* Floor 3: PA12 (EXTI12)
* Floor 4: PA15 (EXTI15)

Floor sensors (pull-down, rising edge, button to 3.3V):
* Sensor 1: PB0 (EXTI0)
* Sensor 2: PB1 (EXTI1)
* Sensor 3: PB8 (EXTI8)
* Sensor 4: PB9 (EXTI9)

Emergency stop (pull-up, falling edge, button to GND):
* PC13 (EXTI13)

PWM motor LED:
* PA5 (TIM2_CH1) -> LED + 220 ohm -> GND

## Quick Self-Test (Host)

No host-side test is required in standalone mode.
