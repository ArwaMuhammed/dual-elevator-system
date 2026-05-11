## Remaining Tasks(in order)

**1- Fix UART telemetry update/display issues** to correctly reflect Slave elevator states, hallway assignments, and real-time floor/status changes.

**2- Re-integrate and stabilize communication fault handling** to properly detect SPI failures and switch the system into the required fault behavior:

  - Master handles all hallway requests.
  
  - Slave enters independent/emergency mode.

**3- Perform final validation** and stress testing for SPI synchronization, hallway routing, and dual-elevator coordination under continuous button events.
