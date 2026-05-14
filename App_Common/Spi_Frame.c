#include "Spi_Frame.h"

/*
 * Calculates the XOR checksum for data integrity.
 * It XORs all bytes except the Checksum byte itself.
 */
uint8 SpiFrame_CalculateChecksum(const SpiFrame_t *frame)
{
    uint8 sum = 0U;

    sum ^= frame->Header;
    sum ^= frame->State;
    sum ^= frame->FloorsData; /* <-- Replaced Current/Target with the packed byte */
    sum ^= frame->Direction;
    sum ^= frame->Requests;
    sum ^= frame->Flags;
    sum ^= frame->Speed;

    return sum;
}

/*
 * Validates the received frame by checking the header and checksum.
 * Returns TRUE if the data is safe to use, FALSE otherwise.
 */
boolean SpiFrame_IsValid(const SpiFrame_t *frame)
{
    /* Null pointer protection */
    if (frame == (void*)0)
    {
        return FALSE;
    }

    /* Check if the header matches our secret key */
    if (frame->Header != SPI_FRAME_HEADER)
    {
        return FALSE;
    }

    /* Verify data integrity using the XOR checksum */
    return (SpiFrame_CalculateChecksum(frame) == frame->Checksum) ? TRUE : FALSE;
}

/*
 * Compresses the 4 cabin buttons into a single 8-bit mask to save space.
 * Bit 0 = Floor 1, Bit 1 = Floor 2, etc.
 */
uint8 SpiFrame_PackCabinRequests(const ElevatorData_t *elevator)
{
    uint8 mask = 0U;
    uint8 i;

    for (i = 0U; i < ELEVATOR_NUM_FLOORS; i++)
    {
        if (elevator->CabinRequests[i] == TRUE)
        {
            mask |= (uint8)(1U << i); /* Shift 1 to the correct bit position */
        }
    }

    return mask;
}

/*
 * Builds the complete 8-byte SPI frame to be sent to the other MCU.
 */
void SpiFrame_Build(const ElevatorData_t *elevator,
                    uint8 requestsMask,
                    uint8 flags,
                    uint8 speed,
                    SpiFrame_t *frame)
{
    frame->Header       = SPI_FRAME_HEADER;
    frame->State        = (uint8)elevator->State;

    /*
     * BIT-PACKING MAGIC:
     * TargetFloor goes to the left 4 bits (High Nibble).
     * CurrentFloor goes to the right 4 bits (Low Nibble).
     * We use bitwise OR to merge them.
     */
    frame->FloorsData   = (uint8)((elevator->TargetFloor << 4) | (elevator->CurrentFloor & 0x0F));

    frame->Direction    = (uint8)elevator->Direction;
    frame->Requests     = requestsMask;
    frame->Flags        = flags;
    frame->Speed        = speed;

    /* Checksum MUST be calculated last to include all fresh data! */
    frame->Checksum     = SpiFrame_CalculateChecksum(frame);
}