// #include "Slave_Control.h"
// #include "Elevator_FSM.h"
//
// static volatile ElevatorData_t *SlaveElevator = (void*)0;
// static volatile boolean SlaveIndependent = FALSE;
//
// static uint8 Slave_GetSpeedFromState(const ElevatorData_t *elevator)
// {
// 	if (elevator->State == ELEVATOR_STATE_MOVING_UP || elevator->State == ELEVATOR_STATE_MOVING_DOWN)
// 	{
// 		return MOTOR_DUTY_FULL;
// 	}
// 	if (elevator->State == ELEVATOR_STATE_DOORS_OPEN)
// 	{
// 		return MOTOR_DUTY_STOP;
// 	}
// 	return MOTOR_DUTY_STOP;
// }
//
// void SlaveControl_Init(ElevatorData_t *elevator)
// {
// 	SlaveElevator = elevator;
// 	SlaveIndependent = FALSE;
// }
//
// void SlaveControl_SetIndependent(boolean enabled)
// {
// 	SlaveIndependent = enabled;
// }
//
// boolean SlaveControl_IsIndependent(void)
// {
// 	return SlaveIndependent;
// }
//
// void SlaveControl_OnRxFrame(const SpiFrame_t *frame)
// {
// 	uint8 requestsMask = 0U;
// 	uint8 flags = 0U;
// 	uint8 i;
//
// 	if (SlaveElevator == (void*)0)
// 	{
// 		return;
// 	}
//
// 	if (SlaveIndependent == TRUE)
// 	{
// 		return;
// 	}
//
// 	if (SpiFrame_IsValid(frame) == FALSE)
// 	{
// 		return;
// 	}
//
// 	requestsMask = frame->Requests;
// 	flags = frame->Flags;
//
// 	if ((flags & SPI_FLAG_EMERGENCY) != 0U)
// 	{
// 		SlaveElevator->EmergencyActive = TRUE;
// 	}
//
// 	for (i = 0U; i < ELEVATOR_NUM_FLOORS; i++)
// 	{
// 		if ((requestsMask & (uint8)(1U << i)) != 0U)
// 		{
// 			ElevatorFSM_RequestFloor((ElevatorData_t*)SlaveElevator, i);
// 		}
// 	}
// }
//
// void SlaveControl_BuildTxFrame(SpiFrame_t *frame)
// {
// 	uint8 flags = 0U;
// 	uint8 requestsMask;
// 	uint8 speed;
//
// 	if (SlaveElevator == (void*)0)
// 	{
// 		return;
// 	}
//
// 	if (SlaveElevator->EmergencyActive == TRUE)
// 	{
// 		flags |= SPI_FLAG_EMERGENCY;
// 	}
//
// 	if (SlaveElevator->State == ELEVATOR_STATE_DOORS_OPEN)
// 	{
// 		flags |= SPI_FLAG_DOORS_OPEN;
// 	}
//
// 	if (SlaveIndependent == TRUE)
// 	{
// 		flags |= SPI_FLAG_COMM_FAULT;
// 	}
//
// 	requestsMask = SpiFrame_PackCabinRequests((ElevatorData_t*)SlaveElevator);
// 	speed = Slave_GetSpeedFromState((ElevatorData_t*)SlaveElevator);
//
// 	SpiFrame_Build((ElevatorData_t*)SlaveElevator, requestsMask, flags, speed, frame);
// }

#include "Slave_Control.h"
#include "Elevator_FSM.h"

static volatile ElevatorData_t *SlaveElevator = (void*)0;
static volatile boolean SlaveIndependent = FALSE;

static uint8 Slave_GetSpeedFromState(const ElevatorData_t *elevator)
{
    if (elevator->State == ELEVATOR_STATE_MOVING_UP || elevator->State == ELEVATOR_STATE_MOVING_DOWN)
    {
       return MOTOR_DUTY_FULL;
    }
    return MOTOR_DUTY_STOP;
}

void SlaveControl_Init(ElevatorData_t *elevator)
{
    SlaveElevator = elevator;
    SlaveIndependent = FALSE;
}

void SlaveControl_SetIndependent(boolean enabled)
{
    SlaveIndependent = enabled;
}

boolean SlaveControl_IsIndependent(void)
{
    return SlaveIndependent;
}

void SlaveControl_OnRxFrame(const SpiFrame_t *frame)
{
    uint8 requestsMask = 0U;
    uint8 flags = 0U;
    uint8 i;

    if (SlaveElevator == (void*)0 || SlaveIndependent == TRUE)
    {
       return;
    }

    /* ROBUSTNESS: Corrupted frames naturally fail the checksum and are ignored */
    if (SpiFrame_IsValid(frame) == FALSE)
    {
       return;
    }

    requestsMask = frame->Requests;
    flags = frame->Flags;

    if ((flags & SPI_FLAG_EMERGENCY) != 0U)
    {
       SlaveElevator->EmergencyActive = TRUE;
    }

    for (i = 0U; i < ELEVATOR_NUM_FLOORS; i++)
    {
       if ((requestsMask & (uint8)(1U << i)) != 0U)
       {
          ElevatorFSM_RequestFloor((ElevatorData_t*)SlaveElevator, i);
       }
    }
}

void SlaveControl_BuildTxFrame(SpiFrame_t *frame)
{
    uint8 flags = 0U;
    uint8 requestsMask;
    uint8 speed;

    if (SlaveElevator == (void*)0)
    {
       return;
    }

    if (SlaveElevator->EmergencyActive == TRUE)
    {
       flags |= SPI_FLAG_EMERGENCY;
    }

    if (SlaveElevator->State == ELEVATOR_STATE_DOORS_OPEN)
    {
       flags |= SPI_FLAG_DOORS_OPEN;
    }

    if (SlaveIndependent == TRUE)
    {
       flags |= SPI_FLAG_COMM_FAULT;
    }

    requestsMask = SpiFrame_PackCabinRequests((ElevatorData_t*)SlaveElevator);
    speed = Slave_GetSpeedFromState((ElevatorData_t*)SlaveElevator);

    SpiFrame_Build((ElevatorData_t*)SlaveElevator, requestsMask, flags, speed, frame);
}