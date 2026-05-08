#include "Spi_Frame.h"

uint8 SpiFrame_CalculateChecksum(const SpiFrame_t *frame)
{
    uint8 sum = 0U;

    sum ^= frame->Header;
    sum ^= frame->State;
    sum ^= frame->CurrentFloor;
    sum ^= frame->Direction;
    sum ^= frame->Requests;
    sum ^= frame->Flags;
    sum ^= frame->Speed;

    return sum;
}

boolean SpiFrame_IsValid(const SpiFrame_t *frame)
{
    if (frame == (void*)0)
    {
        return FALSE;
    }

    if (frame->Header != SPI_FRAME_HEADER)
    {
        return FALSE;
    }

    return (SpiFrame_CalculateChecksum(frame) == frame->Checksum) ? TRUE : FALSE;
}

uint8 SpiFrame_PackCabinRequests(const ElevatorData_t *elevator)
{
    uint8 mask = 0U;
    uint8 i;

    for (i = 0U; i < ELEVATOR_NUM_FLOORS; i++)
    {
        if (elevator->CabinRequests[i] == TRUE)
        {
            mask |= (uint8)(1U << i);
        }
    }

    return mask;
}

void SpiFrame_Build(const ElevatorData_t *elevator,
                    uint8 requestsMask,
                    uint8 flags,
                    uint8 speed,
                    SpiFrame_t *frame)
{
    frame->Header       = SPI_FRAME_HEADER;
    frame->State        = (uint8)elevator->State;
    frame->CurrentFloor = elevator->CurrentFloor;
    frame->Direction    = (uint8)elevator->Direction;
    frame->Requests     = requestsMask;
    frame->Flags        = flags;
    frame->Speed        = speed;
    frame->Checksum     = SpiFrame_CalculateChecksum(frame);
}