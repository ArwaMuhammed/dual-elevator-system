/**
 * Elevator_FSM.c
 *
 * Elevator Finite State Machine using Software Timers.
 * No extra hardware timers needed!
 */

#include "Elevator_FSM.h"
#include "Pwm.h"
#include "Std_Types.h"
#include "Timer.h"

/* ── Motor PWM config ───────────────────────────────────────── */
#define MOTOR_TIMER_ID   TIMER2
#define MOTOR_CHANNEL    PWM_CHANNEL_1

/* ── Software Timers Config (Based on 10ms FSM Tick) ────────── */
#define FLOOR_TRAVEL_TICKS  20U  /* 200 * 10ms = 2 seconds to travel 1 floor */
#define DOOR_OPEN_TICKS     30U  /* 300 * 10ms = 3 seconds door open time */

/* ── Private Variables for Software Counting ────────────────── */
static uint16 FSM_TravelTicksRemaining = 0U;
static uint16 FSM_DoorTicksRemaining = 0U;

/* ── Forward declarations of private helpers ────────────────── */
static uint8   FSM_FindNextFloor(ElevatorData_t *elevator);
static boolean FSM_HasAnyRequest(ElevatorData_t *elevator);
static void    FSM_SetMotorSpeed(uint8 dutyPercent);


/* ═══════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════ */

void ElevatorFSM_Init(ElevatorData_t *elevator)
{
    uint8 i;
    elevator->State             = ELEVATOR_STATE_IDLE;
    elevator->Direction         = ELEVATOR_DIR_NONE;
    elevator->CurrentFloor      = FLOOR_1;
    elevator->TargetFloor       = FLOOR_1;
    elevator->DoorTimerExpired  = FALSE;
    elevator->EmergencyActive   = FALSE;
    elevator->FloorReached      = FALSE;

    for (i = 0U; i < ELEVATOR_NUM_FLOORS; i++) {
        elevator->CabinRequests[i] = FALSE;
    }

    FSM_SetMotorSpeed(MOTOR_DUTY_STOP);
}

void ElevatorFSM_Tick(ElevatorData_t *elevator)
{
    /* ── EMERGENCY overrides every state ── */
    if (elevator->EmergencyActive == TRUE)
    {
        elevator->State     = ELEVATOR_STATE_EMERGENCY;
        elevator->Direction = ELEVATOR_DIR_NONE;
        FSM_SetMotorSpeed(MOTOR_DUTY_STOP);
        return;
    }

    switch (elevator->State)
    {
        /* ════════════════════════════════════════════
         * IDLE — wait for any cabin request
         * ════════════════════════════════════════════ */
        case ELEVATOR_STATE_IDLE:
        {
            if (FSM_HasAnyRequest(elevator) == TRUE)
            {
                uint8 next = FSM_FindNextFloor(elevator);
                elevator->TargetFloor = next;

                if (next > elevator->CurrentFloor)
                {
                    elevator->State     = ELEVATOR_STATE_MOVING_UP;
                    elevator->Direction = ELEVATOR_DIR_UP;
                    FSM_SetMotorSpeed(MOTOR_DUTY_FULL);
                    FSM_TravelTicksRemaining = FLOOR_TRAVEL_TICKS; /* Start travel countdown! */
                }
                else if (next < elevator->CurrentFloor)
                {
                    elevator->State     = ELEVATOR_STATE_MOVING_DOWN;
                    elevator->Direction = ELEVATOR_DIR_DOWN;
                    FSM_SetMotorSpeed(MOTOR_DUTY_FULL);
                    FSM_TravelTicksRemaining = FLOOR_TRAVEL_TICKS; /* Start travel countdown! */
                }
                else
                {
                    elevator->CabinRequests[next] = FALSE;
                    elevator->State               = ELEVATOR_STATE_DOORS_OPEN;
                    elevator->Direction           = ELEVATOR_DIR_NONE;
                    FSM_SetMotorSpeed(MOTOR_DUTY_STOP);
                    FSM_DoorTicksRemaining        = DOOR_OPEN_TICKS; /* Start door countdown */
                }
            }
            break;
        }

        /* ════════════════════════════════════════════
         * MOVING_UP — countdown ticks to reach floor
         * ════════════════════════════════════════════ */
        case ELEVATOR_STATE_MOVING_UP:
        {
            /* 1. Decrement the virtual sensor timer every 10ms */
            if (FSM_TravelTicksRemaining > 0U) {
                FSM_TravelTicksRemaining--;
                if (FSM_TravelTicksRemaining == 0U) {
                    elevator->FloorReached = TRUE; /* 2 seconds have passed! */
                }
            }

            /* 2. Check if we arrived */
            if (elevator->FloorReached == TRUE)
            {
                __asm volatile ("CPSID I");
                elevator->FloorReached = FALSE;
                __asm volatile ("CPSIE I");

                if (elevator->CurrentFloor == (elevator->TargetFloor - 1U)) {
                    FSM_SetMotorSpeed(MOTOR_DUTY_SLOW);
                }

                elevator->CurrentFloor++;

                if (elevator->CurrentFloor == elevator->TargetFloor)
                {
                    /* Reached target! Open doors. */
                    elevator->CabinRequests[elevator->CurrentFloor] = FALSE;
                    elevator->State     = ELEVATOR_STATE_DOORS_OPEN;
                    elevator->Direction = ELEVATOR_DIR_NONE;
                    FSM_SetMotorSpeed(MOTOR_DUTY_STOP);
                    FSM_DoorTicksRemaining = DOOR_OPEN_TICKS; /* Start door timer */
                }
                else
                {
                    /* Passing a floor, need to keep going. Reset the timer! */
                    FSM_TravelTicksRemaining = FLOOR_TRAVEL_TICKS;
                }
            }
            break;
        }

        /* ════════════════════════════════════════════
         * MOVING_DOWN — countdown ticks to reach floor
         * ════════════════════════════════════════════ */
        case ELEVATOR_STATE_MOVING_DOWN:
        {
            /* 1. Decrement the virtual sensor timer every 10ms */
            if (FSM_TravelTicksRemaining > 0U) {
                FSM_TravelTicksRemaining--;
                if (FSM_TravelTicksRemaining == 0U) {
                    elevator->FloorReached = TRUE;
                }
            }

            /* 2. Check if we arrived */
            if (elevator->FloorReached == TRUE)
            {
                __asm volatile ("CPSID I");
                elevator->FloorReached = FALSE;
                __asm volatile ("CPSIE I");

                if (elevator->CurrentFloor == (elevator->TargetFloor + 1U)) {
                    FSM_SetMotorSpeed(MOTOR_DUTY_SLOW);
                }

                elevator->CurrentFloor--;

                if (elevator->CurrentFloor == elevator->TargetFloor)
                {
                    /* Reached target! Open doors. */
                    elevator->CabinRequests[elevator->CurrentFloor] = FALSE;
                    elevator->State     = ELEVATOR_STATE_DOORS_OPEN;
                    elevator->Direction = ELEVATOR_DIR_NONE;
                    FSM_SetMotorSpeed(MOTOR_DUTY_STOP);
                    FSM_DoorTicksRemaining = DOOR_OPEN_TICKS;
                }
                else
                {
                    /* Passing a floor, need to keep going. Reset the timer! */
                    FSM_TravelTicksRemaining = FLOOR_TRAVEL_TICKS;
                }
            }
            break;
        }

        /* ════════════════════════════════════════════
         * DOORS_OPEN — countdown ticks to close doors
         * ════════════════════════════════════════════ */
        case ELEVATOR_STATE_DOORS_OPEN:
        {
            if (FSM_DoorTicksRemaining > 0U) {
                FSM_DoorTicksRemaining--;
                if (FSM_DoorTicksRemaining == 0U) {
                    elevator->DoorTimerExpired = TRUE;
                }
            }

            if (elevator->DoorTimerExpired == TRUE)
            {
                __asm volatile ("CPSID I");
                elevator->DoorTimerExpired = FALSE;
                __asm volatile ("CPSIE I");

                if (FSM_HasAnyRequest(elevator) == TRUE)
                {
                    uint8 next = FSM_FindNextFloor(elevator);
                    elevator->TargetFloor = next;

                    if (next > elevator->CurrentFloor)
                    {
                        elevator->State     = ELEVATOR_STATE_MOVING_UP;
                        elevator->Direction = ELEVATOR_DIR_UP;
                        FSM_SetMotorSpeed(MOTOR_DUTY_FULL);
                        FSM_TravelTicksRemaining = FLOOR_TRAVEL_TICKS;
                    }
                    else if (next < elevator->CurrentFloor)
                    {
                        elevator->State     = ELEVATOR_STATE_MOVING_DOWN;
                        elevator->Direction = ELEVATOR_DIR_DOWN;
                        FSM_SetMotorSpeed(MOTOR_DUTY_FULL);
                        FSM_TravelTicksRemaining = FLOOR_TRAVEL_TICKS;
                    }
                    else
                    {
                        elevator->CabinRequests[next] = FALSE;
                        elevator->State               = ELEVATOR_STATE_DOORS_OPEN;
                        FSM_DoorTicksRemaining        = DOOR_OPEN_TICKS;
                    }
                }
                else
                {
                    elevator->State     = ELEVATOR_STATE_IDLE;
                    elevator->Direction = ELEVATOR_DIR_NONE;
                }
            }
            break;
        }

        case ELEVATOR_STATE_EMERGENCY:
        {
            FSM_SetMotorSpeed(MOTOR_DUTY_STOP);
            break;
        }

        default:
            break;
    }
}

void ElevatorFSM_RequestFloor(ElevatorData_t *elevator, uint8 floorIndex)
{
    if (floorIndex < ELEVATOR_NUM_FLOORS) {
        elevator->CabinRequests[floorIndex] = TRUE;
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Private helpers
 * ═══════════════════════════════════════════════════════════════ */

static boolean FSM_HasAnyRequest(ElevatorData_t *elevator)
{
    uint8 i;
    for (i = 0U; i < ELEVATOR_NUM_FLOORS; i++) {
        if (elevator->CabinRequests[i] == TRUE) return TRUE;
    }
    return FALSE;
}

static uint8 FSM_FindNextFloor(ElevatorData_t *elevator)
{
    uint8 i;
    uint8 best      = elevator->CurrentFloor;
    uint8 bestDist  = 0xFFU;
    uint8 dist;

    if (elevator->Direction == ELEVATOR_DIR_UP) {
        for (i = elevator->CurrentFloor + 1U; i < ELEVATOR_NUM_FLOORS; i++) {
            if (elevator->CabinRequests[i] == TRUE) return i;
        }
        for (i = 0U; i < elevator->CurrentFloor; i++) {
            if (elevator->CabinRequests[i] == TRUE) return i;
        }
    }
    else if (elevator->Direction == ELEVATOR_DIR_DOWN) {
        for (i = elevator->CurrentFloor; i > 0U; i--) {
            if (elevator->CabinRequests[i - 1U] == TRUE) return (i - 1U);
        }
        for (i = elevator->CurrentFloor + 1U; i < ELEVATOR_NUM_FLOORS; i++) {
            if (elevator->CabinRequests[i] == TRUE) return i;
        }
    }
    else {
        /* IDLE — pick the closest floor by distance */
        for (i = 0U; i < ELEVATOR_NUM_FLOORS; i++) {
            if (elevator->CabinRequests[i] == TRUE) {
                dist = (i > elevator->CurrentFloor) ? (i - elevator->CurrentFloor) : (elevator->CurrentFloor - i);
                if (dist < bestDist) {
                    bestDist = dist;
                    best     = i;
                }
            }
        }
    }
    return best;
}

static void FSM_SetMotorSpeed(uint8 dutyPercent)
{
    Pwm_SetDutyPercent(MOTOR_TIMER_ID, MOTOR_CHANNEL, dutyPercent);
}