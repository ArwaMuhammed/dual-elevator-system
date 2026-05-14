#ifndef SPI_FRAME_H
#define SPI_FRAME_H

#include "Std_Types.h"
#include "Elevator_Types.h"

/* FIXED: Restored to 8 bytes to meet Project Requirement 3 (Fixed-length frame) */
#define SPI_FRAME_SIZE      8U
#define SPI_FRAME_HEADER    0xA5U

/* Bit flags for the system status */
#define SPI_FLAG_EMERGENCY   0x01U
#define SPI_FLAG_DOORS_OPEN  0x02U
#define SPI_FLAG_COMM_FAULT  0x04U

/*
 * Packed structure to ensure no memory padding by the compiler.
 * Total size is exactly 8 Bytes for the SPI IPC link.
 */
typedef struct __attribute__((packed))
{
    uint8 Header;       /* Byte 0: Sync header (0xA5) */
    uint8 State;        /* Byte 1: Current state of the elevator FSM */
    uint8 FloorsData;   /* Byte 2: High Nibble = Target Floor, Low Nibble = Current Floor */
    uint8 Direction;    /* Byte 3: Motor direction (UP/DOWN/STOP) */
    uint8 Requests;     /* Byte 4: Cabin button requests packed as bits */
    uint8 Flags;        /* Byte 5: System flags (Emergency, Faults, etc.) */
    uint8 Speed;        /* Byte 6: PWM speed duty cycle */
    uint8 Checksum;     /* Byte 7: XOR checksum of Bytes 0 to 6 */
} SpiFrame_t;

/* Function Prototypes */
uint8 SpiFrame_CalculateChecksum(const SpiFrame_t *frame);
boolean SpiFrame_IsValid(const SpiFrame_t *frame);
uint8 SpiFrame_PackCabinRequests(const ElevatorData_t *elevator);

void SpiFrame_Build(const ElevatorData_t *elevator,
                    uint8 requestsMask,
                    uint8 flags,
                    uint8 speed,
                    SpiFrame_t *frame);

#endif