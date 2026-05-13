#include "Dispatcher.h"

/* Hall call bitmap: U1, D2, U2, D3, U3, D4 */
static volatile uint8 HallCallMask = 0U;
static volatile boolean SystemCommFault = FALSE; /* Tracks SPI health */

static void EnterCritical(void) { __asm volatile ("CPSID I"); }
static void ExitCritical(void)  { __asm volatile ("CPSIE I"); }

static uint8 Dispatcher_CallToBit(uint8 floor, ElevatorDir_t direction)
{
    if (floor == FLOOR_1 && direction == ELEVATOR_DIR_UP)   { return 0U; }
    if (floor == FLOOR_2 && direction == ELEVATOR_DIR_DOWN) { return 1U; }
    if (floor == FLOOR_2 && direction == ELEVATOR_DIR_UP)   { return 2U; }
    if (floor == FLOOR_3 && direction == ELEVATOR_DIR_DOWN) { return 3U; }
    if (floor == FLOOR_3 && direction == ELEVATOR_DIR_UP)   { return 4U; }
    if (floor == FLOOR_4 && direction == ELEVATOR_DIR_DOWN) { return 5U; }
    return 0xFFU;
}

static boolean Dispatcher_BitToCall(uint8 bitIndex, HallCall_t *outCall)
{
    switch (bitIndex)
    {
       case 0U: outCall->Floor = FLOOR_1; outCall->Direction = ELEVATOR_DIR_UP;   return TRUE;
       case 1U: outCall->Floor = FLOOR_2; outCall->Direction = ELEVATOR_DIR_DOWN; return TRUE;
       case 2U: outCall->Floor = FLOOR_2; outCall->Direction = ELEVATOR_DIR_UP;   return TRUE;
       case 3U: outCall->Floor = FLOOR_3; outCall->Direction = ELEVATOR_DIR_DOWN; return TRUE;
       case 4U: outCall->Floor = FLOOR_3; outCall->Direction = ELEVATOR_DIR_UP;   return TRUE;
       case 5U: outCall->Floor = FLOOR_4; outCall->Direction = ELEVATOR_DIR_DOWN; return TRUE;
       default: return FALSE;
    }
}

void Dispatcher_Init(void)
{
    HallCallMask = 0U;
    SystemCommFault = FALSE;
}

/* Call this from Master's main.c when SPI fails or recovers */
void Dispatcher_SetCommFault(boolean status)
{
    SystemCommFault = status;
}

void Dispatcher_AddHallCall(uint8 floor, ElevatorDir_t direction)
{
    uint8 bit = Dispatcher_CallToBit(floor, direction);
    if (bit != 0xFFU)
    {
       EnterCritical();
       HallCallMask |= (uint8)(1U << bit);
       ExitCritical();
    }
}

boolean Dispatcher_PopNextHallCall(HallCall_t *outCall)
{
    uint8 bit;
    uint8 maskSnapshot;

    EnterCritical();
    maskSnapshot = HallCallMask;
    ExitCritical();

    for (bit = 0U; bit < 6U; bit++)
    {
       if ((maskSnapshot & (uint8)(1U << bit)) != 0U)
       {
          if (Dispatcher_BitToCall(bit, outCall) == TRUE)
          {
             EnterCritical();
             HallCallMask &= (uint8)~(1U << bit);
             ExitCritical();
             return TRUE;
          }
       }
    }
    return FALSE;
}

typedef enum
{
    DISPATCH_CLASS_IMMEDIATE = 0,
    DISPATCH_CLASS_PERFECT   = 1,
    DISPATCH_CLASS_PASSED    = 2,
    DISPATCH_CLASS_IDLE      = 3,
    DISPATCH_CLASS_OPPOSITE  = 4,
    DISPATCH_CLASS_UNAVAIL   = 5
} DispatchClass_t;

static DispatchClass_t Dispatcher_Classify(const ElevatorData_t *elevator, uint8 callFloor, ElevatorDir_t callDir, uint8 *distance)
{
    uint8 dist = (callFloor > elevator->CurrentFloor) ? (callFloor - elevator->CurrentFloor) : (elevator->CurrentFloor - callFloor);

    if ((elevator->State == ELEVATOR_STATE_EMERGENCY) || (elevator->EmergencyActive == TRUE))
    {
       *distance = dist;
       return DISPATCH_CLASS_UNAVAIL;
    }

    if (elevator->State == ELEVATOR_STATE_IDLE)
    {
       *distance = dist;
       return (elevator->CurrentFloor == callFloor) ? DISPATCH_CLASS_IMMEDIATE : DISPATCH_CLASS_IDLE;
    }

    if (elevator->Direction == callDir)
    {
       *distance = dist;
       if (callDir == ELEVATOR_DIR_UP) {
          return (elevator->CurrentFloor <= callFloor) ? DISPATCH_CLASS_PERFECT : DISPATCH_CLASS_PASSED;
       }
       if (callDir == ELEVATOR_DIR_DOWN) {
          return (elevator->CurrentFloor >= callFloor) ? DISPATCH_CLASS_PERFECT : DISPATCH_CLASS_PASSED;
       }
    }

    *distance = dist;
    return DISPATCH_CLASS_OPPOSITE;
}

DispatchTarget_t Dispatcher_AssignCall(const ElevatorData_t *master, const ElevatorData_t *slave, const HallCall_t *call)
{
    uint8 masterDist = 0U;
    uint8 slaveDist = 0U;

    /* RUBRIC REQUIREMENT: Comm Fault -> Master takes all calls */
    if (SystemCommFault == TRUE)
    {
        return DISPATCH_TARGET_MASTER;
    }

    DispatchClass_t masterClass = Dispatcher_Classify(master, call->Floor, call->Direction, &masterDist);
    DispatchClass_t slaveClass  = Dispatcher_Classify(slave,  call->Floor, call->Direction, &slaveDist);

    if ((masterClass == DISPATCH_CLASS_OPPOSITE || masterClass == DISPATCH_CLASS_UNAVAIL) &&
        (slaveClass == DISPATCH_CLASS_OPPOSITE || slaveClass == DISPATCH_CLASS_UNAVAIL))
    {
       return DISPATCH_TARGET_NONE;
    }

    if (masterClass < slaveClass)  return DISPATCH_TARGET_MASTER;
    if (slaveClass < masterClass)  return DISPATCH_TARGET_SLAVE;
    if (masterDist <= slaveDist)   return DISPATCH_TARGET_MASTER; /* Tie-breaker goes to master */

    return DISPATCH_TARGET_SLAVE;
}

DispatchTarget_t Dispatcher_SelectAndPop(const ElevatorData_t *master, const ElevatorData_t *slave, HallCall_t *outCall)
{
    HallCall_t call;
    DispatchTarget_t target;

    if (Dispatcher_PopNextHallCall(&call) == FALSE) return DISPATCH_TARGET_NONE;

    target = Dispatcher_AssignCall(master, slave, &call);
    if (target == DISPATCH_TARGET_NONE)
    {
       Dispatcher_AddHallCall(call.Floor, call.Direction); /* Requeue */
       return DISPATCH_TARGET_NONE;
    }

    if (outCall != (void*)0) *outCall = call;
    return target;
}

uint8 Dispatcher_GetHallCallMask(void)
{
    uint8 maskSnapshot;
    EnterCritical();
    maskSnapshot = HallCallMask;
    ExitCritical();
    return maskSnapshot;
}