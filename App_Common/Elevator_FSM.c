/**
 * Elevator_FSM.c
 *
 * Elevator Finite State Machine implementation.
 *
 * States:
 *   IDLE        — No requests. Motor stopped.
 *   MOVING_UP   — Travelling upward.  Motor at full duty.
 *   MOVING_DOWN — Travelling downward. Motor at full duty.
 *   DOORS_OPEN  — Stopped at a floor. Motor stopped. Door timer running.
 *   EMERGENCY   — Emergency stop pressed. All motion halted.
 *
 * Timer / PWM usage (defined in main.c init):
 *   TIM2 CH1 (PA5) — PWM motor LED
 *   TIM3            — 10 ms repeating FSM tick  (calls ElevatorFSM_Tick)
 *   TIM4            — 3 s one-shot door timer   (sets elevator.DoorTimerExpired)
 *
 * Volatile rule: all flags in ElevatorData_t that are written by ISRs
 * (CabinRequests, DoorTimerExpired, EmergencyActive, FloorReached) are
 * declared boolean (uint8). The ElevatorData_t pointer passed around is
 * to the single volatile-qualified instance in main.c.
 *
 * Created for: Dual Elevator System — Phase 1
 */

#include "Elevator_FSM.h"
#include "Pwm.h"
#include "Timer.h"

/* ── Motor PWM config (matches main.c init) ─────────────────── */
#define MOTOR_TIMER_ID   TIMER2
#define MOTOR_CHANNEL    PWM_CHANNEL_1

/* ── Door timer config ──────────────────────────────────────── */
#define DOOR_TIMER_ID    TIMER4

/* ── Forward declarations of private helpers ────────────────── */
static uint8   FSM_FindNextFloor(ElevatorData_t *elevator);
static boolean FSM_HasAnyRequest(ElevatorData_t *elevator);
static void    FSM_SetMotorSpeed(uint8 dutyPercent);
static void    FSM_StartDoorTimer(ElevatorData_t *elevator);
static void    FSM_DoorTimerCallback(void);

/* ── Single pointer used by the door-timer callback ─────────── */
/* The callback has no parameters (TimerCallback signature), so  */
/* we keep a module-level pointer to the active elevator.        */
static volatile ElevatorData_t *FSM_ActiveElevator = (void*)0;

/* ═══════════════════════════════════════════════════════════════
 *  Public API
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

    for (i = 0U; i < ELEVATOR_NUM_FLOORS; i++)
    {
        elevator->CabinRequests[i] = FALSE;
    }

    /* Motor off at startup */
    FSM_SetMotorSpeed(MOTOR_DUTY_STOP);
}

/* ── ElevatorFSM_Tick ───────────────────────────────────────────
 * Called every 10 ms from TIM3 IRQ.
 * Reads ISR-set flags, runs state machine, drives PWM.
 * ─────────────────────────────────────────────────────────────── */
void ElevatorFSM_Tick(ElevatorData_t *elevator)
{
    /* ── EMERGENCY overrides every state ── */
    if (elevator->EmergencyActive == TRUE)
    {
        elevator->State     = ELEVATOR_STATE_EMERGENCY;
        elevator->Direction = ELEVATOR_DIR_NONE;
        FSM_SetMotorSpeed(MOTOR_DUTY_STOP);
        Timer_Stop(DOOR_TIMER_ID);
        return;
    }

    switch (elevator->State)
    {
        /* ════════════════════════════════════════════
         *  IDLE — wait for any cabin request
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
                }
                else if (next < elevator->CurrentFloor)
                {
                    elevator->State     = ELEVATOR_STATE_MOVING_DOWN;
                    elevator->Direction = ELEVATOR_DIR_DOWN;
                    FSM_SetMotorSpeed(MOTOR_DUTY_FULL);
                }
                else
                {
                    /* Already at the requested floor — open doors */
                    elevator->CabinRequests[next] = FALSE;
                    elevator->State               = ELEVATOR_STATE_DOORS_OPEN;
                    elevator->Direction           = ELEVATOR_DIR_NONE;
                    FSM_SetMotorSpeed(MOTOR_DUTY_STOP);
                    FSM_StartDoorTimer(elevator);
                }
            }
            break;
        }

        /* ════════════════════════════════════════════
         *  MOVING_UP — wait for floor sensor
         * ════════════════════════════════════════════ */
        case ELEVATOR_STATE_MOVING_UP:
        {
            if (elevator->FloorReached == TRUE)
            {
                /* ── Consume the flag inside a critical section ── */
                __asm volatile ("CPSID I");  /* disable interrupts */
                elevator->FloorReached = FALSE;
                __asm volatile ("CPSIE I");  /* re-enable          */

                /* Simulate slow-down one floor before target */
                if (elevator->CurrentFloor == (elevator->TargetFloor - 1U))
                {
                    FSM_SetMotorSpeed(MOTOR_DUTY_SLOW);
                }

                elevator->CurrentFloor++;

                if (elevator->CurrentFloor == elevator->TargetFloor)
                {
                    elevator->CabinRequests[elevator->CurrentFloor] = FALSE;
                    elevator->State     = ELEVATOR_STATE_DOORS_OPEN;
                    elevator->Direction = ELEVATOR_DIR_NONE;
                    FSM_SetMotorSpeed(MOTOR_DUTY_STOP);
                    FSM_StartDoorTimer(elevator);
                }
            }
            break;
        }

        /* ════════════════════════════════════════════
         *  MOVING_DOWN — wait for floor sensor
         * ════════════════════════════════════════════ */
        case ELEVATOR_STATE_MOVING_DOWN:
        {
            if (elevator->FloorReached == TRUE)
            {
                __asm volatile ("CPSID I");
                elevator->FloorReached = FALSE;
                __asm volatile ("CPSIE I");

                if (elevator->CurrentFloor == (elevator->TargetFloor + 1U))
                {
                    FSM_SetMotorSpeed(MOTOR_DUTY_SLOW);
                }

                elevator->CurrentFloor--;

                if (elevator->CurrentFloor == elevator->TargetFloor)
                {
                    elevator->CabinRequests[elevator->CurrentFloor] = FALSE;
                    elevator->State     = ELEVATOR_STATE_DOORS_OPEN;
                    elevator->Direction = ELEVATOR_DIR_NONE;
                    FSM_SetMotorSpeed(MOTOR_DUTY_STOP);
                    FSM_StartDoorTimer(elevator);
                }
            }
            break;
        }

        /* ════════════════════════════════════════════
         *  DOORS_OPEN — wait for door timer to expire
         * ════════════════════════════════════════════ */
        case ELEVATOR_STATE_DOORS_OPEN:
        {
            if (elevator->DoorTimerExpired == TRUE)
            {
                __asm volatile ("CPSID I");
                elevator->DoorTimerExpired = FALSE;
                __asm volatile ("CPSIE I");

                /* Check if more requests exist after closing doors */
                if (FSM_HasAnyRequest(elevator) == TRUE)
                {
                    uint8 next = FSM_FindNextFloor(elevator);
                    elevator->TargetFloor = next;

                    if (next > elevator->CurrentFloor)
                    {
                        elevator->State     = ELEVATOR_STATE_MOVING_UP;
                        elevator->Direction = ELEVATOR_DIR_UP;
                        FSM_SetMotorSpeed(MOTOR_DUTY_FULL);
                    }
                    else if (next < elevator->CurrentFloor)
                    {
                        elevator->State     = ELEVATOR_STATE_MOVING_DOWN;
                        elevator->Direction = ELEVATOR_DIR_DOWN;
                        FSM_SetMotorSpeed(MOTOR_DUTY_FULL);
                    }
                    else
                    {
                        /* Same floor requested again */
                        elevator->CabinRequests[next] = FALSE;
                        elevator->State               = ELEVATOR_STATE_DOORS_OPEN;
                        FSM_StartDoorTimer(elevator);
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

        /* ════════════════════════════════════════════
         *  EMERGENCY — stays here until reset
         * ════════════════════════════════════════════ */
        case ELEVATOR_STATE_EMERGENCY:
        {
            /* Motor is already stopped. Stay here.
             * A hardware reset is required to recover. */
            FSM_SetMotorSpeed(MOTOR_DUTY_STOP);
            break;
        }

        default:
            break;
    }
}

void ElevatorFSM_RequestFloor(ElevatorData_t *elevator, uint8 floorIndex)
{
    if (floorIndex < ELEVATOR_NUM_FLOORS)
    {
        elevator->CabinRequests[floorIndex] = TRUE;
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  Private helpers
 * ═══════════════════════════════════════════════════════════════ */

/**
 * Returns TRUE if at least one cabin request is pending.
 */
static boolean FSM_HasAnyRequest(ElevatorData_t *elevator)
{
    uint8 i;
    for (i = 0U; i < ELEVATOR_NUM_FLOORS; i++)
    {
        if (elevator->CabinRequests[i] == TRUE)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * Finds the nearest requested floor relative to CurrentFloor.
 * Priority: continue in current direction first, then reverse.
 * Falls back to simple nearest if direction is NONE (IDLE).
 */
static uint8 FSM_FindNextFloor(ElevatorData_t *elevator)
{
    uint8 i;
    uint8 best      = elevator->CurrentFloor;
    uint8 bestDist  = 0xFFU;
    uint8 dist;

    /* Prefer floors in the current direction of travel */
    if (elevator->Direction == ELEVATOR_DIR_UP)
    {
        for (i = elevator->CurrentFloor + 1U; i < ELEVATOR_NUM_FLOORS; i++)
        {
            if (elevator->CabinRequests[i] == TRUE)
            {
                return i; /* Nearest above */
            }
        }
        /* Nothing above — reverse and find nearest below */
        for (i = 0U; i < elevator->CurrentFloor; i++)
        {
            if (elevator->CabinRequests[i] == TRUE)
            {
                return i;
            }
        }
    }
    else if (elevator->Direction == ELEVATOR_DIR_DOWN)
    {
        /* Search downward first */
        for (i = elevator->CurrentFloor; i > 0U; i--)
        {
            if (elevator->CabinRequests[i - 1U] == TRUE)
            {
                return (i - 1U);
            }
        }
        /* Nothing below — reverse */
        for (i = elevator->CurrentFloor + 1U; i < ELEVATOR_NUM_FLOORS; i++)
        {
            if (elevator->CabinRequests[i] == TRUE)
            {
                return i;
            }
        }
    }
    else
    {
        /* IDLE — pick nearest floor by absolute distance */
        for (i = 0U; i < ELEVATOR_NUM_FLOORS; i++)
        {
            if (elevator->CabinRequests[i] == TRUE)
            {
                dist = (i > elevator->CurrentFloor)
                       ? (i - elevator->CurrentFloor)
                       : (elevator->CurrentFloor - i);

                if (dist < bestDist)
                {
                    bestDist = dist;
                    best     = i;
                }
            }
        }
    }

    return best;
}

/**
 * Drives the motor PWM LED to the requested duty cycle.
 */
static void FSM_SetMotorSpeed(uint8 dutyPercent)
{
    Pwm_SetDutyPercent(MOTOR_TIMER_ID, MOTOR_CHANNEL, dutyPercent);
}

/**
 * Starts the one-shot door-open timer (TIM4, 3 seconds).
 */
static void FSM_StartDoorTimer(ElevatorData_t *elevator)
{
    FSM_ActiveElevator = elevator;
    Timer_DelayMsAsync(DOOR_TIMER_ID, DOOR_OPEN_DURATION_MS, FSM_DoorTimerCallback);
}

/**
 * TIM4 callback — fired from ISR context after 3 seconds.
 * Only sets a flag; all logic runs in ElevatorFSM_Tick.
 */
static void FSM_DoorTimerCallback(void)
{
    if (FSM_ActiveElevator != (void*)0)
    {
        FSM_ActiveElevator->DoorTimerExpired = TRUE;
    }
}