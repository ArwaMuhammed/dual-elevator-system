## 📁 Project Structure (Application Layer)

The project is organized into three main application modules to clearly separate responsibilities between shared logic, the Master controller, and the Slave controller.

### 🔹 `App_Common`

Contains all shared logic used by both MCUs.

* Elevator finite state machine (FSM)
* Shared data structures (states, directions, requests)
* SPI communication frame definition and utilities

This layer ensures both elevators follow consistent behavior.

---

### 🔹 `Master_App`

Implements the logic for the Master MCU (Dispatcher + Elevator A).

Responsibilities:

* Reads hallway and cabin requests
* Runs the task allocation (dispatcher) algorithm
* Controls Elevator A locally
* Sends commands to the Slave MCU via SPI
* Outputs system telemetry over UART

This module acts as the **decision-making brain** of the system.

---

### 🔹 `Slave_App`

Implements the logic for the Slave MCU (Elevator B).

Responsibilities:

* Controls Elevator B using the shared FSM
* Receives commands from the Master via SPI
* Sends current state and status back to the Master

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

