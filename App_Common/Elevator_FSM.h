/**
* Elevator_FSM.h
 *
 * Public interface for the Elevator Finite State Machine.
 * Both Master and Slave use this same FSM logic via App_Common.
 *
 * Created for: Dual Elevator System — Phase 1
 */

#ifndef ELEVATOR_FSM_H
#define ELEVATOR_FSM_H

#include "Elevator_Types.h"

/**
 * @brief  Initialise the elevator data structure to a known safe state.
 *         Call once during system init before starting the FSM tick.
 * @param  elevator   Pointer to the elevator instance to initialise.
 */
void ElevatorFSM_Init(ElevatorData_t *elevator);

/**
 * @brief  Run one FSM tick. Call this every 10 ms from the TIM3 callback.
 *         Reads volatile flags set by ISRs, transitions states, and
 *         drives PWM output accordingly.
 * @param  elevator   Pointer to the elevator instance.
 */
void ElevatorFSM_Tick(ElevatorData_t *elevator);

/**
 * @brief  Add a cabin floor request. Called from cabin button EXTI ISRs.
 * @param  elevator     Pointer to the elevator instance.
 * @param  floorIndex   0-based floor index (FLOOR_1 .. FLOOR_4).
 */
void ElevatorFSM_RequestFloor(ElevatorData_t *elevator, uint8 floorIndex);

#endif /* ELEVATOR_FSM_H */