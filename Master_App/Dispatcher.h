//
// Created by hp on 5/3/2026.
//

#ifndef STM32_TEMPLATE_DISPATCHER_H
#define STM32_TEMPLATE_DISPATCHER_H

#include "Elevator_Types.h"

typedef enum
{
	DISPATCH_TARGET_MASTER = 0U,
	DISPATCH_TARGET_SLAVE  = 1U,
	DISPATCH_TARGET_NONE   = 2U
} DispatchTarget_t;

typedef struct
{
	uint8 Floor;
	ElevatorDir_t Direction;
} HallCall_t;

void Dispatcher_Init(void);
void Dispatcher_AddHallCall(uint8 floor, ElevatorDir_t direction);
boolean Dispatcher_PopNextHallCall(HallCall_t *outCall);
DispatchTarget_t Dispatcher_AssignCall(const ElevatorData_t *master,
									   const ElevatorData_t *slave,
									   const HallCall_t *call);
DispatchTarget_t Dispatcher_SelectAndPop(const ElevatorData_t *master,
										 const ElevatorData_t *slave,
										 HallCall_t *outCall);

#endif /* STM32_TEMPLATE_DISPATCHER_H */
