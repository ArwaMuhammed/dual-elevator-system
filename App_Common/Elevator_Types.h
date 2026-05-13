/**
 * Elevator_Types.h
 *
 * Shared data types used by both Master and Slave MCUs.
 * All flags shared with ISRs must be declared volatile at the point of use.
 *
 * Created for: Dual Elevator System — Phase 1
 */

#ifndef ELEVATOR_TYPES_H
#define ELEVATOR_TYPES_H

#include "Std_Types.h"

/* ─── Number of floors ─────────────────────────────────────── */
#define ELEVATOR_NUM_FLOORS     4U

/* ─── Floor indices ─────────────────────────────────────────── */
#define FLOOR_1     0U
#define FLOOR_2     1U
#define FLOOR_3     2U
#define FLOOR_4     3U

/* ─── FSM States ────────────────────────────────────────────── */
typedef enum
{
    ELEVATOR_STATE_IDLE         = 0U,
    ELEVATOR_STATE_MOVING_UP    = 1U,
    ELEVATOR_STATE_MOVING_DOWN  = 2U,
    ELEVATOR_STATE_DOORS_OPEN   = 3U,
    ELEVATOR_STATE_EMERGENCY    = 4U
} ElevatorState_t;

/* ─── Direction ─────────────────────────────────────────────── */
typedef enum
{
    ELEVATOR_DIR_NONE = 0U,
    ELEVATOR_DIR_UP   = 1U,
    ELEVATOR_DIR_DOWN = 2U
} ElevatorDir_t;

/* ─── PWM duty cycles representing motor speed ──────────────── */
#define MOTOR_DUTY_STOP     0U
#define MOTOR_DUTY_SLOW     20U
#define MOTOR_DUTY_FULL     100U

/* ─── Door open duration in milliseconds ───────────────────── */
#define DOOR_OPEN_DURATION_MS   250U

/* ─── Main elevator data structure ─────────────────────────── */
typedef struct
{
    ElevatorState_t   State;                            /* Current FSM state            */
    ElevatorDir_t     Direction;                        /* Current travel direction      */
    uint8             CurrentFloor;                     /* 0-based floor index (0–3)     */
    uint8             TargetFloor;                      /* Next floor to serve           */
    boolean           CabinRequests[ELEVATOR_NUM_FLOORS]; /* Pending cabin button presses */
    boolean           DoorTimerExpired;                 /* Set by TIM4 callback (ISR)    */
    boolean           EmergencyActive;                  /* Set by emergency EXTI (ISR)   */
    boolean           FloorReached;                     /* Set by floor sensor EXTI (ISR)*/
} ElevatorData_t;

#endif /* ELEVATOR_TYPES_H */