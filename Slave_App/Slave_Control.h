//
// Created by hp on 5/3/2026.
//

#ifndef STM32_TEMPLATE_SLAVE_CONTROL_H
#define STM32_TEMPLATE_SLAVE_CONTROL_H

#include "Elevator_Types.h"
#include "Spi_Frame.h"

void SlaveControl_Init(ElevatorData_t *elevator);
void SlaveControl_SetIndependent(boolean enabled);
boolean SlaveControl_IsIndependent(void);

void SlaveControl_OnRxFrame(const SpiFrame_t *frame);
void SlaveControl_BuildTxFrame(SpiFrame_t *frame);

#endif /* STM32_TEMPLATE_SLAVE_CONTROL_H */
