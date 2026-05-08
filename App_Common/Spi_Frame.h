#ifndef SPI_FRAME_H
#define SPI_FRAME_H

#include "Std_Types.h"
#include "Elevator_Types.h"

#define SPI_FRAME_SIZE      8U
#define SPI_FRAME_HEADER    0xA5U

#define SPI_FLAG_EMERGENCY   0x01U
#define SPI_FLAG_DOORS_OPEN  0x02U
#define SPI_FLAG_COMM_FAULT  0x04U

typedef struct
{
    uint8 Header;
    uint8 State;
    uint8 CurrentFloor;
    uint8 Direction;
    uint8 Requests;
    uint8 Flags;
    uint8 Speed;
    uint8 Checksum;
} SpiFrame_t;

uint8 SpiFrame_CalculateChecksum(const SpiFrame_t *frame);
boolean SpiFrame_IsValid(const SpiFrame_t *frame);
uint8 SpiFrame_PackCabinRequests(const ElevatorData_t *elevator);

void SpiFrame_Build(const ElevatorData_t *elevator,
                    uint8 requestsMask,
                    uint8 flags,
                    uint8 speed,
                    SpiFrame_t *frame);

#endif