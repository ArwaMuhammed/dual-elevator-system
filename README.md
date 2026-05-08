## Remaining Tasks

### 1. UART Telemetry
- Implement non-blocking UART status reporting every 500 ms.
- Telemetry should include:
  - Master elevator state and current floor
  - Slave elevator state and current floor
  - Active hallway/cabin requests
  - SPI communication status

### 2. SPI Communication Fault Handling
- Detect SPI timeout or communication failure.
- On communication fault:
  - Master should take control of all hallway requests.
  - Slave should switch to independent/emergency mode.
- Add communication timeout monitoring and recovery handling.

### 3. SPI Reliability Improvements
- Improve robustness of SPI frame exchange.
- Add handling for:
  - Invalid checksum frames
  - Corrupted packets
  - Missed/partial transfers
- Ensure stable full-duplex synchronization between Master and Slave.

### 4. Dispatcher Algorithm Validation
- Test and verify all required dispatching scenarios:
  - Immediate match
  - Perfect directional match
  - Passed match
  - Opposite direction rejection
  - Nearest idle elevator selection
- Validate correct elevator assignment behavior under different traffic conditions.

### 5. Bonus (Optional)
- Implement DMA-based UART telemetry transmission to reduce CPU overhead during status reporting.
