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

static volatile ElevatorData_t masterElevator;
static ElevatorData_t slaveElevatorShadow;

static volatile boolean FsmTickFlag = FALSE;
static uint8 SpiTickCounter = 0U;
static uint8 PendingSlaveRequestsMask = 0U;

/* ───────────────── Sensor filter ───────────────── */

static void Sensor_TriggerIfExpected(uint8 floorIndex)
{
    if (masterElevator.Direction == ELEVATOR_DIR_UP)
    {
        if ((masterElevator.CurrentFloor + 1U) == floorIndex)
        {
            masterElevator.FloorReached = TRUE;
        }
    }
    else if (masterElevator.Direction == ELEVATOR_DIR_DOWN)
    {
        if ((masterElevator.CurrentFloor > 0U) &&
            ((masterElevator.CurrentFloor - 1U) == floorIndex))
        {
            masterElevator.FloorReached = TRUE;
        }
    }
}

/* ───────────────── Cabin buttons ───────────────── */

static void CabinBtn_Floor1_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, FLOOR_1); }
static void CabinBtn_Floor2_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, FLOOR_2); }
static void CabinBtn_Floor3_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, FLOOR_3); }
static void CabinBtn_Floor4_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, FLOOR_4); }

/* ───────────────── Floor sensors ───────────────── */

static void Sensor_Floor1_CB(void) { Sensor_TriggerIfExpected(FLOOR_1); }
static void Sensor_Floor2_CB(void) { Sensor_TriggerIfExpected(FLOOR_2); }
static void Sensor_Floor3_CB(void) { Sensor_TriggerIfExpected(FLOOR_3); }
static void Sensor_Floor4_CB(void) { Sensor_TriggerIfExpected(FLOOR_4); }

/* ───────────────── Emergency ───────────────── */

static void Emergency_CB(void) { masterElevator.EmergencyActive = TRUE; }

/* ───────────────── Hallway buttons ───────────────── */

static void Hall_U1_CB(void) { Dispatcher_AddHallCall(FLOOR_1, ELEVATOR_DIR_UP); }
static void Hall_D2_CB(void) { Dispatcher_AddHallCall(FLOOR_2, ELEVATOR_DIR_DOWN); }
static void Hall_U2_CB(void) { Dispatcher_AddHallCall(FLOOR_2, ELEVATOR_DIR_UP); }
static void Hall_D3_CB(void) { Dispatcher_AddHallCall(FLOOR_3, ELEVATOR_DIR_DOWN); }
static void Hall_U3_CB(void) { Dispatcher_AddHallCall(FLOOR_3, ELEVATOR_DIR_UP); }
static void Hall_D4_CB(void) { Dispatcher_AddHallCall(FLOOR_4, ELEVATOR_DIR_DOWN); }

/* ───────────────── Timer tick ───────────────── */

static void FsmTick_CB(void)
{
    FsmTickFlag = TRUE;
}

static void FSM_RearmTick(void)
{
    Timer_DelayMsAsync(TIMER3, 10U, FsmTick_CB);
}

static void SetIrqPriority(uint8 irqNumber, uint8 priority)
{
    NVIC_IPR_BASE[irqNumber] = (uint8)(priority << 4U);
}

/* ───────────────── SPI helpers ───────────────── */

static uint8 Master_GetSpeedFromState(const ElevatorData_t *elevator)
{
    if ((elevator->State == ELEVATOR_STATE_MOVING_UP) ||
        (elevator->State == ELEVATOR_STATE_MOVING_DOWN))
    {
        return MOTOR_DUTY_FULL;
    }

    return MOTOR_DUTY_STOP;
}

static void Master_UpdateSlaveShadowFromFrame(const SpiFrame_t *frame)
{
    if (SpiFrame_IsValid(frame) == TRUE)
    {
        slaveElevatorShadow.State        = (ElevatorState_t)frame->State;
        slaveElevatorShadow.CurrentFloor = frame->CurrentFloor;
        slaveElevatorShadow.Direction    = (ElevatorDir_t)frame->Direction;

        slaveElevatorShadow.EmergencyActive =
            ((frame->Flags & SPI_FLAG_EMERGENCY) != 0U) ? TRUE : FALSE;
    }
}

static void Master_BuildTxFrame(SpiFrame_t *frame)
{
    uint8 flags = 0U;
    uint8 speed;

    if (masterElevator.EmergencyActive == TRUE)
    {
        flags |= SPI_FLAG_EMERGENCY;
    }

    if (masterElevator.State == ELEVATOR_STATE_DOORS_OPEN)
    {
        flags |= SPI_FLAG_DOORS_OPEN;
    }

    speed = Master_GetSpeedFromState((ElevatorData_t*)&masterElevator);

    SpiFrame_Build((ElevatorData_t*)&masterElevator,
                   PendingSlaveRequestsMask,
                   flags,
                   speed,
                   frame);
}

static void Master_SpiExchange(void)
{
    SpiFrame_t txFrame;
    SpiFrame_t rxFrame;

    Master_BuildTxFrame(&txFrame);

    Gpio_WritePin(SPI_CS_PORT, SPI_CS_PIN, LOW);

    (void)Spi1_TransmitReceiveBuffer((uint8*)&txFrame,
                                     (uint8*)&rxFrame,
                                     SPI_FRAME_SIZE);

    Gpio_WritePin(SPI_CS_PORT, SPI_CS_PIN, HIGH);

    if (SpiFrame_IsValid(&rxFrame) == TRUE)
    {
        Master_UpdateSlaveShadowFromFrame(&rxFrame);

        /* Slave received the command frame, so clear sent requests */
        PendingSlaveRequestsMask = 0U;
    }
}

static void Master_HandleHallRequests(void)
{
    HallCall_t call;
    DispatchTarget_t target;

    target = Dispatcher_SelectAndPop((ElevatorData_t*)&masterElevator,
                                     &slaveElevatorShadow,
                                     &call);

    if (target == DISPATCH_TARGET_MASTER)
    {
        ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, call.Floor);
    }
    else if (target == DISPATCH_TARGET_SLAVE)
    {
        PendingSlaveRequestsMask |= (uint8)(1U << call.Floor);
    }
}

/* ───────────────── Init ───────────────── */

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

    /*
     * Master pin mapping:
     *
     * Cabin buttons:
     * F1 PA10, F2 PA11, F3 PA12, F4 PA15
     *
     * Floor sensors:
     * S1 PB0, S2 PB1, S3 PB8, S4 PB9
     *
     * Emergency:
     * PC13
     *
     * Hallway buttons:
     * U1 PC2, D2 PC3, U2 PC4, D3 PC5, U3 PC6, D4 PC7
     *
     * PWM LED:
     * PA5
     *
     * SPI1:
     * PB3 SCK, PB4 MISO, PB5 MOSI, PB6 CS
     */

    /* Cabin buttons */
    Gpio_Init(GPIO_A, 10, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 11, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 12, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 15, GPIO_INPUT, GPIO_PULL_UP);

    Exti_Init(EXTI_LINE_10, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor1_CB);
    Exti_Init(EXTI_LINE_11, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor2_CB);
    Exti_Init(EXTI_LINE_12, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor3_CB);
    Exti_Init(EXTI_LINE_15, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor4_CB);

    /* Floor sensors */
    Gpio_Init(GPIO_B, 0, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 1, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 8, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 9, GPIO_INPUT, GPIO_PULL_DOWN);

    Exti_Init(EXTI_LINE_0, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor1_CB);
    Exti_Init(EXTI_LINE_1, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor2_CB);
    Exti_Init(EXTI_LINE_8, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor3_CB);
    Exti_Init(EXTI_LINE_9, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor4_CB);

    /* Emergency */
    Gpio_Init(GPIO_C, 13, GPIO_INPUT, GPIO_PULL_UP);
    Exti_Init(EXTI_LINE_13, EXTI_PORT_C, EXTI_EDGE_FALLING, Emergency_CB);

    /* Hallway buttons */
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

    /* PWM motor LED */
    Gpio_Init(GPIO_A, 5, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_SetAF(GPIO_A, 5, GPIO_AF1);

    Pwm_Init(TIMER2, PWM_CHANNEL_1, PWM_PSC, PWM_ARR);
    Pwm_Start(TIMER2, PWM_CHANNEL_1);
    Pwm_SetDutyPercent(TIMER2, PWM_CHANNEL_1, MOTOR_DUTY_STOP);

    /* SPI CS pin */
    Gpio_Init(SPI_CS_PORT, SPI_CS_PIN, GPIO_OUTPUT, GPIO_PUSH_PULL);
    Gpio_WritePin(SPI_CS_PORT, SPI_CS_PIN, HIGH);

    Spi1_Init(SPI_MASTER, SPI_IDLE_LOW, SPI_SAMPLE_FIRST_TRANSITION);

    /* NVIC priorities */
    SetIrqPriority(IRQ_EXTI0,      2U);
    SetIrqPriority(IRQ_EXTI1,      2U);
    SetIrqPriority(IRQ_EXTI2,      2U);
    SetIrqPriority(IRQ_EXTI3,      2U);
    SetIrqPriority(IRQ_EXTI4,      2U);
    SetIrqPriority(IRQ_EXTI9_5,    1U);
    SetIrqPriority(IRQ_EXTI15_10,  0U);
    SetIrqPriority(IRQ_TIM3,       3U);
    SetIrqPriority(IRQ_TIM4,       3U);

    /* Enable EXTI lines */
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

    FSM_RearmTick();
}

int main(void)
{
    System_Init();

    while (1)
    {
        if (FsmTickFlag == TRUE)
        {
            __asm volatile ("CPSID I");
            FsmTickFlag = FALSE;
            __asm volatile ("CPSIE I");

            ElevatorFSM_Tick((ElevatorData_t*)&masterElevator);

            Master_HandleHallRequests();

            SpiTickCounter++;
            if (SpiTickCounter >= 5U)
            {
                SpiTickCounter = 0U;
                Master_SpiExchange();
            }

            FSM_RearmTick();
        }

        __asm volatile ("WFI");
    }

    return 0;
}