#include "Rcc.h"
#include "Gpio.h"
#include "Exti.h"
#include "Pwm.h"
#include "Timer.h"
#include "Spi.h"
#include "Spi_Frame.h"
#include "Slave_Control.h"
#include "Elevator_FSM.h"
#include "Elevator_Types.h"

#define NVIC_IPR_BASE   ((volatile uint8 *)0xE000E400)

#define IRQ_EXTI0       6U
#define IRQ_EXTI1       7U
#define IRQ_EXTI9_5     23U
#define IRQ_EXTI15_10   40U
#define IRQ_TIM3        29U
#define IRQ_TIM4        30U

#define PWM_PSC         0U
#define PWM_ARR         1599U

#define SPI_CS_PORT GPIO_B
#define SPI_CS_PIN  6U

static volatile ElevatorData_t slaveElevator;
static volatile boolean FsmTickFlag = FALSE;

static SpiFrame_t SlaveRxFrame;
static SpiFrame_t SlaveTxFrame;
static uint8 SlaveRxIndex = 0U;
static uint8 SlaveTxIndex = 0U;

/* ───────────────── Sensor filter ───────────────── */

static void Sensor_TriggerIfExpected(uint8 floorIndex)
{
    if (slaveElevator.Direction == ELEVATOR_DIR_UP)
    {
        if ((slaveElevator.CurrentFloor + 1U) == floorIndex)
        {
            slaveElevator.FloorReached = TRUE;
        }
    }
    else if (slaveElevator.Direction == ELEVATOR_DIR_DOWN)
    {
        if ((slaveElevator.CurrentFloor > 0U) &&
            ((slaveElevator.CurrentFloor - 1U) == floorIndex))
        {
            slaveElevator.FloorReached = TRUE;
        }
    }
}

/* ───────────────── Cabin buttons ───────────────── */

static void CabinBtn_Floor1_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&slaveElevator, FLOOR_1); }
static void CabinBtn_Floor2_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&slaveElevator, FLOOR_2); }
static void CabinBtn_Floor3_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&slaveElevator, FLOOR_3); }
static void CabinBtn_Floor4_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&slaveElevator, FLOOR_4); }

/* ───────────────── Floor sensors ───────────────── */

static void Sensor_Floor1_CB(void) { Sensor_TriggerIfExpected(FLOOR_1); }
static void Sensor_Floor2_CB(void) { Sensor_TriggerIfExpected(FLOOR_2); }
static void Sensor_Floor3_CB(void) { Sensor_TriggerIfExpected(FLOOR_3); }
static void Sensor_Floor4_CB(void) { Sensor_TriggerIfExpected(FLOOR_4); }

/* ───────────────── Emergency ───────────────── */

static void Emergency_CB(void) { slaveElevator.EmergencyActive = TRUE; }

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

/* ───────────────── SPI slave service ───────────────── */

static void Slave_PrepareTxFrame(void)
{
    SlaveControl_BuildTxFrame(&SlaveTxFrame);
}

static void Slave_ResetSpiState(void)
{
    SlaveRxIndex = 0U;
    SlaveTxIndex = 0U;

    Slave_PrepareTxFrame();

    (void)Spi1_SlavePreloadByte(((uint8*)&SlaveTxFrame)[SlaveTxIndex]);
    SlaveTxIndex++;
}

static void Slave_ServiceSpi(void)
{
    uint8 rxByte;

    /* Only receive while Master CS is LOW */
    if (Gpio_ReadPin(SPI_CS_PORT, SPI_CS_PIN) == HIGH)
    {
        SlaveRxIndex = 0U;
        SlaveTxIndex = 0U;
        Slave_PrepareTxFrame();
        (void)Spi1_SlavePreloadByte(((uint8*)&SlaveTxFrame)[SlaveTxIndex]);
        SlaveTxIndex++;
        return;
    }

    if (Spi1_SlaveReadByte(&rxByte) == SPI_OK)
    {
        ((uint8*)&SlaveRxFrame)[SlaveRxIndex] = rxByte;
        SlaveRxIndex++;

        if (SlaveTxIndex < SPI_FRAME_SIZE)
        {
            (void)Spi1_SlavePreloadByte(((uint8*)&SlaveTxFrame)[SlaveTxIndex]);
            SlaveTxIndex++;
        }

        if (SlaveRxIndex >= SPI_FRAME_SIZE)
        {
            SlaveControl_OnRxFrame(&SlaveRxFrame);
            SlaveRxIndex = 0U;
            SlaveTxIndex = 0U;
        }
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
     * Slave pin mapping:
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
     * PWM LED:
     * PA5
     *
     * SPI1:
     * PB3 SCK, PB4 MISO, PB5 MOSI, PB6 CS input
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

    /* PWM motor LED */
    Gpio_Init(GPIO_A, 5, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_SetAF(GPIO_A, 5, GPIO_AF1);

    Pwm_Init(TIMER2, PWM_CHANNEL_1, PWM_PSC, PWM_ARR);
    Pwm_Start(TIMER2, PWM_CHANNEL_1);
    Pwm_SetDutyPercent(TIMER2, PWM_CHANNEL_1, MOTOR_DUTY_STOP);

    /* SPI CS input */
    Gpio_Init(GPIO_B, 6, GPIO_INPUT, GPIO_PULL_UP);

    Spi1_Init(SPI_SLAVE, SPI_IDLE_LOW, SPI_SAMPLE_FIRST_TRANSITION);

    /* NVIC priorities */
    SetIrqPriority(IRQ_EXTI0,      2U);
    SetIrqPriority(IRQ_EXTI1,      2U);
    SetIrqPriority(IRQ_EXTI9_5,    1U);
    SetIrqPriority(IRQ_EXTI15_10,  0U);
    SetIrqPriority(IRQ_TIM3,       3U);
    SetIrqPriority(IRQ_TIM4,       3U);

    /* Enable EXTI lines */
    Exti_Enable(EXTI_LINE_0);
    Exti_Enable(EXTI_LINE_1);
    Exti_Enable(EXTI_LINE_8);
    Exti_Enable(EXTI_LINE_9);
    Exti_Enable(EXTI_LINE_10);
    Exti_Enable(EXTI_LINE_11);
    Exti_Enable(EXTI_LINE_12);
    Exti_Enable(EXTI_LINE_13);
    Exti_Enable(EXTI_LINE_15);

    ElevatorFSM_Init((ElevatorData_t*)&slaveElevator);
    SlaveControl_Init((ElevatorData_t*)&slaveElevator);

    Slave_ResetSpiState();

    FSM_RearmTick();
}

int main(void)
{
    System_Init();

    while (1)
    {
        Slave_ServiceSpi();

        if (FsmTickFlag == TRUE)
        {
            __asm volatile ("CPSID I");
            FsmTickFlag = FALSE;
            __asm volatile ("CPSIE I");

            ElevatorFSM_Tick((ElevatorData_t*)&slaveElevator);

            FSM_RearmTick();
        }
    }

    return 0;
}