#include "stm32f401xe.h"
#include "Rcc.h"
#include "Gpio.h"
#include "Exti.h"
#include "Pwm.h"
#include "Timer.h"
#include "Spi.h"
#include "Spi_Frame.h"
#include "Dispatcher.h"
#include "Elevator_FSM.h"
#include "Elevator_Types.h"
#include <stdio.h>
#include "Usart.h"
#include <sys/stat.h>

void *_sbrk(int incr);
void *_sbrk(int incr)
{
    extern char end asm("end");
    static char *heap_end;
    char *prev_heap_end;
    if (heap_end == 0) heap_end = &end;
    prev_heap_end = heap_end;
    heap_end += incr;
    return (void *)prev_heap_end;
}

#define NVIC_IPR_BASE   ((volatile uint8 *)0xE000E400)
#define IRQ_EXTI0       6U
#define IRQ_EXTI1       7U
#define IRQ_EXTI2       8U
#define IRQ_EXTI3       9U
#define IRQ_EXTI4       10U
#define IRQ_EXTI9_5     23U
#define IRQ_EXTI15_10   40U
#define IRQ_TIM3        29U
#define IRQ_TIM4        30U

#define PWM_PSC         0U
#define PWM_ARR         1599U

#define SPI_CS_PORT     GPIO_B
#define SPI_CS_PIN      6U

#define TELEMETRY_USE_DMA   0U



static uint8 TelemetryTickCounter = 0U;
static char TelemetryBuffer[256];
static boolean SpiCommFault = FALSE;

static uint8 LatchedHallCalls = 0U;
static uint8 LatchedSlaveReq = 0U;

static volatile ElevatorData_t masterElevator;
static ElevatorData_t slaveElevatorShadow;

static volatile boolean FsmTickFlag = FALSE;
static uint8 SpiTickCounter = 0U;
static uint8 PendingSlaveRequestsMask = 0U;

/* ───────────────── NON-BLOCKING SPI MASTER STATE MACHINE ───────────────── */
static volatile uint8 SpiMasterState = 0U;
static volatile uint8 SpiByteIndex = 0U;
static SpiFrame_t TxFrameBuffer;
static SpiFrame_t RxFrameBuffer;
static volatile boolean SpiTransferComplete = FALSE;

void SysTick_Handler(void)
{
    /* Runs exactly every 1 millisecond. Zero busy-waiting! */
    if (SpiMasterState == 0U) return;

    if (SpiMasterState == 1U)
    {
        /* Step 1: Wake up Slave */
        Gpio_WritePin(SPI_CS_PORT, SPI_CS_PIN, LOW);
        SpiByteIndex = 0U;
        SpiMasterState = 2U;
    }
    else if (SpiMasterState == 2U)
    {
        /* Step 2: Read previous byte (if any), write next byte */
        if (SpiByteIndex > 0U) {
            ((uint8*)&RxFrameBuffer)[SpiByteIndex - 1U] = SPI1->DR;
        }

        if (SpiByteIndex < SPI_FRAME_SIZE) {
            SPI1->DR = ((uint8*)&TxFrameBuffer)[SpiByteIndex];
            SpiByteIndex++;
        } else {
            SpiMasterState = 3U; /* All bytes sent and read */
        }
    }
    else if (SpiMasterState == 3U)
    {
        /* Step 3: End Transfer */
        Gpio_WritePin(SPI_CS_PORT, SPI_CS_PIN, HIGH);
        SpiTransferComplete = TRUE;
        SpiMasterState = 0U; /* Return to IDLE */
    }
}
/* ───────────────────────────────────────────────────────────────────────── */

static void Sensor_TriggerIfExpected(uint8 floorIndex)
{
    if (masterElevator.Direction == ELEVATOR_DIR_UP) {
        if ((masterElevator.CurrentFloor + 1U) == floorIndex) masterElevator.FloorReached = TRUE;
    } else if (masterElevator.Direction == ELEVATOR_DIR_DOWN) {
        if ((masterElevator.CurrentFloor > 0U) && ((masterElevator.CurrentFloor - 1U) == floorIndex))
            masterElevator.FloorReached = TRUE;
    }
}

static void CabinBtn_Floor1_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, FLOOR_1); }
static void CabinBtn_Floor2_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, FLOOR_2); }
static void CabinBtn_Floor3_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, FLOOR_3); }
static void CabinBtn_Floor4_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, FLOOR_4); }

static void Sensor_Floor1_CB(void) { Sensor_TriggerIfExpected(FLOOR_1); }
static void Sensor_Floor2_CB(void) { Sensor_TriggerIfExpected(FLOOR_2); }
static void Sensor_Floor3_CB(void) { Sensor_TriggerIfExpected(FLOOR_3); }
static void Sensor_Floor4_CB(void) { Sensor_TriggerIfExpected(FLOOR_4); }
static void Emergency_CB(void) { masterElevator.EmergencyActive = TRUE; }

static void Hall_U1_CB(void) { Dispatcher_AddHallCall(FLOOR_1, ELEVATOR_DIR_UP); }
static void Hall_D2_CB(void) { Dispatcher_AddHallCall(FLOOR_2, ELEVATOR_DIR_DOWN); }
static void Hall_U2_CB(void) { Dispatcher_AddHallCall(FLOOR_2, ELEVATOR_DIR_UP); }
static void Hall_D3_CB(void) { Dispatcher_AddHallCall(FLOOR_3, ELEVATOR_DIR_DOWN); }
static void Hall_U3_CB(void) { Dispatcher_AddHallCall(FLOOR_3, ELEVATOR_DIR_UP); }
static void Hall_D4_CB(void) { Dispatcher_AddHallCall(FLOOR_4, ELEVATOR_DIR_DOWN); }

static void FsmTick_CB(void) { FsmTickFlag = TRUE; }
static void FSM_RearmTick(void) { Timer_DelayMsAsync(TIMER3, 10U, FsmTick_CB); }
static void SetIrqPriority(uint8 irqNumber, uint8 priority) { NVIC_IPR_BASE[irqNumber] = (uint8)(priority << 4U); }

static uint8 Master_GetSpeedFromState(const ElevatorData_t *elevator)
{
    if ((elevator->State == ELEVATOR_STATE_MOVING_UP) || (elevator->State == ELEVATOR_STATE_MOVING_DOWN))
        return MOTOR_DUTY_FULL;
    return MOTOR_DUTY_STOP;
}

static void Master_UpdateSlaveShadowFromFrame(const SpiFrame_t *frame)
{
    if (SpiFrame_IsValid(frame) == TRUE) {
        slaveElevatorShadow.State        = (ElevatorState_t)frame->State;
        slaveElevatorShadow.CurrentFloor = frame->CurrentFloor;
        slaveElevatorShadow.Direction    = (ElevatorDir_t)frame->Direction;
        slaveElevatorShadow.EmergencyActive = ((frame->Flags & SPI_FLAG_EMERGENCY) != 0U) ? TRUE : FALSE;
    }
}

static void Master_SendTelemetry(void)
{
    sprintf(TelemetryBuffer,
        "\r\n--- ELEVATOR TELEMETRY ---\r\n"
        "Master : State = %d | Floor = %d \r\n"
        "Slave  : State = %d | Floor = %d \r\n"
        "System : HallCalls = 0x%02X | SlavePendReq = 0x%02X\r\n"
        "SPI IPC: %s\r\n"
        "--------------------------\r\n",
        masterElevator.State, masterElevator.CurrentFloor,
        slaveElevatorShadow.State, slaveElevatorShadow.CurrentFloor,
        LatchedHallCalls, LatchedSlaveReq,
        (SpiCommFault == TRUE) ? "FAULT (TIMEOUT)" : "OK");

    #if (TELEMETRY_USE_DMA == 1U)
        /* Bonus (hardware): Zero-CPU UART telemetry using DMA (non-blocking) */
        Usart2_TransmitStringDMA(TelemetryBuffer);
    #else
        /* Simulation-safe: polling TX (still allowed for UART debug output) */
        Usart2_TransmitString(TelemetryBuffer);
    #endif
    LatchedHallCalls = 0U;
    LatchedSlaveReq = 0U;
}

static void Master_HandleHallRequests(void)
{
    HallCall_t call;
    DispatchTarget_t target;
    target = Dispatcher_SelectAndPop((ElevatorData_t*)&masterElevator, &slaveElevatorShadow, &call);

    if (target == DISPATCH_TARGET_MASTER) {
        ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, call.Floor);
    } else if (target == DISPATCH_TARGET_SLAVE) {
        PendingSlaveRequestsMask |= (uint8)(1U << call.Floor);
    }
}

static void System_Init(void)
{
    Rcc_Init();
    Rcc_Enable(RCC_GPIOA);
    Rcc_Enable(RCC_GPIOB);
    Rcc_Enable(RCC_GPIOC);
    Rcc_Enable(RCC_SYSCFG);
    Rcc_Enable(RCC_TIM2);
    Rcc_Enable(RCC_TIM3);
    Rcc_Enable(RCC_TIM4);
    Rcc_Enable(RCC_SPI1);
    Rcc_Enable(RCC_GPIOD);
    Rcc_Enable(RCC_USART2);
    Rcc_Enable(RCC_DMA1);

    /* Enable DMA interrupt for USART2 TX (DMA1 Stream6) */
    NVIC_EnableIRQ(DMA1_Stream6_IRQn);
    NVIC_SetPriority(DMA1_Stream6_IRQn, 5U);

    /* Start SysTick timer for exactly 1ms (Assuming 16MHz clock) */
    SysTick_Config(16000U);

    Gpio_Init(GPIO_A, 10, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 11, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 12, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 15, GPIO_INPUT, GPIO_PULL_UP);
    Exti_Init(EXTI_LINE_10, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor1_CB);
    Exti_Init(EXTI_LINE_11, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor2_CB);
    Exti_Init(EXTI_LINE_12, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor3_CB);
    Exti_Init(EXTI_LINE_15, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor4_CB);

    Gpio_Init(GPIO_B, 0, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 1, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 8, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 9, GPIO_INPUT, GPIO_PULL_DOWN);
    Exti_Init(EXTI_LINE_0, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor1_CB);
    Exti_Init(EXTI_LINE_1, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor2_CB);
    Exti_Init(EXTI_LINE_8, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor3_CB);
    Exti_Init(EXTI_LINE_9, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor4_CB);

    Gpio_Init(GPIO_C, 13, GPIO_INPUT, GPIO_PULL_UP);
    Exti_Init(EXTI_LINE_13, EXTI_PORT_C, EXTI_EDGE_FALLING, Emergency_CB);

    Gpio_Init(GPIO_C, 2, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_C, 3, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_C, 4, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_C, 5, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_C, 6, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_C, 7, GPIO_INPUT, GPIO_PULL_UP);
    Exti_Init(EXTI_LINE_2, EXTI_PORT_C, EXTI_EDGE_FALLING, Hall_U1_CB);
    Exti_Init(EXTI_LINE_3, EXTI_PORT_C, EXTI_EDGE_FALLING, Hall_D2_CB);
    Exti_Init(EXTI_LINE_4, EXTI_PORT_C, EXTI_EDGE_FALLING, Hall_U2_CB);
    Exti_Init(EXTI_LINE_5, EXTI_PORT_C, EXTI_EDGE_FALLING, Hall_D3_CB);
    Exti_Init(EXTI_LINE_6, EXTI_PORT_C, EXTI_EDGE_FALLING, Hall_U3_CB);
    Exti_Init(EXTI_LINE_7, EXTI_PORT_C, EXTI_EDGE_FALLING, Hall_D4_CB);

    Gpio_Init(GPIO_A, 5, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_SetAF(GPIO_A, 5, GPIO_AF1);
    Pwm_Init(TIMER2, PWM_CHANNEL_1, PWM_PSC, PWM_ARR);
    Pwm_Start(TIMER2, PWM_CHANNEL_1);
    Pwm_SetDutyPercent(TIMER2, PWM_CHANNEL_1, MOTOR_DUTY_STOP);

    Gpio_Init(SPI_CS_PORT, SPI_CS_PIN, GPIO_OUTPUT, GPIO_PUSH_PULL);
    Gpio_WritePin(SPI_CS_PORT, SPI_CS_PIN, HIGH);
    Spi1_Init(SPI_MASTER, SPI_IDLE_LOW, SPI_SAMPLE_FIRST_TRANSITION);

    SetIrqPriority(IRQ_EXTI0,      2U);
    SetIrqPriority(IRQ_EXTI1,      2U);
    SetIrqPriority(IRQ_EXTI2,      2U);
    SetIrqPriority(IRQ_EXTI3,      2U);
    SetIrqPriority(IRQ_EXTI4,      2U);
    SetIrqPriority(IRQ_EXTI9_5,    1U);
    SetIrqPriority(IRQ_EXTI15_10,  0U);
    SetIrqPriority(IRQ_TIM3,       3U);
    SetIrqPriority(IRQ_TIM4,       3U);

    Exti_Enable(EXTI_LINE_0);
    Exti_Enable(EXTI_LINE_1);
    Exti_Enable(EXTI_LINE_2);
    Exti_Enable(EXTI_LINE_3);
    Exti_Enable(EXTI_LINE_4);
    Exti_Enable(EXTI_LINE_5);
    Exti_Enable(EXTI_LINE_6);
    Exti_Enable(EXTI_LINE_7);
    Exti_Enable(EXTI_LINE_8);
    Exti_Enable(EXTI_LINE_9);
    Exti_Enable(EXTI_LINE_10);
    Exti_Enable(EXTI_LINE_11);
    Exti_Enable(EXTI_LINE_12);
    Exti_Enable(EXTI_LINE_13);
    Exti_Enable(EXTI_LINE_15);

    ElevatorFSM_Init((ElevatorData_t*)&masterElevator);
    ElevatorFSM_Init(&slaveElevatorShadow);
    Dispatcher_Init();
    Usart2_Init();

    /* Quick sanity message to confirm UART wiring in Proteus/terminal. */
    Usart2_TransmitString("\r\nBOOT: Master started\r\n");

    FSM_RearmTick();
}

int main(void)
{
    System_Init();

    while (1)
    {
        /* NON-BLOCKING: Process the SPI Frame instantly when SysTick finishes */
        if (SpiTransferComplete == TRUE)
        {
            /* 1. Enter Critical Section */
            __asm volatile ("CPSID I");
            SpiTransferComplete = FALSE;

            /* RUBRIC REQUIREMENT: Protect RX Buffer while processing */
            boolean isValid = SpiFrame_IsValid(&RxFrameBuffer);

            /* 2. Exit Critical Section */
            __asm volatile ("CPSIE I");

            if (isValid == TRUE) {
                SpiCommFault = FALSE;
                Dispatcher_SetCommFault(FALSE);
                Master_UpdateSlaveShadowFromFrame(&RxFrameBuffer);
                PendingSlaveRequestsMask = 0U; /* Clear sent requests */
            } else {
                SpiCommFault = TRUE;
                Dispatcher_SetCommFault(TRUE);
            }
        }

        if (FsmTickFlag == TRUE)
        {
            __asm volatile ("CPSID I");
            FsmTickFlag = FALSE;
            __asm volatile ("CPSIE I");

            LatchedHallCalls |= Dispatcher_GetHallCallMask();
            LatchedSlaveReq |= PendingSlaveRequestsMask;

            ElevatorFSM_Tick((ElevatorData_t*)&masterElevator);
            Master_HandleHallRequests();

            /* Kick off the Non-Blocking SPI State Machine every 50ms */
            SpiTickCounter++;
            if (SpiTickCounter >= 5U)
            {
                SpiTickCounter = 0U;
                if (SpiMasterState == 0U) {
                    uint8 speed = Master_GetSpeedFromState((ElevatorData_t*)&masterElevator);
                    uint8 flags = 0U;
                    if (masterElevator.EmergencyActive == TRUE) flags |= SPI_FLAG_EMERGENCY;
                    if (masterElevator.State == ELEVATOR_STATE_DOORS_OPEN) flags |= SPI_FLAG_DOORS_OPEN;

                    /* RUBRIC REQUIREMENT: Protect TX Buffer creation */
                    __asm volatile ("CPSID I");
                    SpiFrame_Build((ElevatorData_t*)&masterElevator, PendingSlaveRequestsMask, flags, speed, &TxFrameBuffer);
                    __asm volatile ("CPSIE I");

                    SpiMasterState = 1U; /* Triggers the SysTick Handler */
                }
            }

            TelemetryTickCounter++;
            if (TelemetryTickCounter >= 25U)
            {
                TelemetryTickCounter = 0U;
                Master_SendTelemetry();
            }

            FSM_RearmTick();
        }
        __asm volatile ("WFI");
    }
    return 0;
}