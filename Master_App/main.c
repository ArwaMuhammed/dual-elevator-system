#include "Rcc.h"
#include "Gpio.h"
#include "Exti.h"
#include "Pwm.h"
#include "Timer.h"
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

static volatile ElevatorData_t masterElevator;
static volatile boolean FsmTickFlag = FALSE;

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
        if (masterElevator.CurrentFloor > 0U && (masterElevator.CurrentFloor - 1U) == floorIndex)
        {
            masterElevator.FloorReached = TRUE;
        }
    }
}

/* Cabin buttons */
static void CabinBtn_Floor1_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, FLOOR_1); }
static void CabinBtn_Floor2_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, FLOOR_2); }
static void CabinBtn_Floor3_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, FLOOR_3); }
static void CabinBtn_Floor4_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&masterElevator, FLOOR_4); }

/* Floor sensors */
static void Sensor_Floor1_CB(void) { Sensor_TriggerIfExpected(FLOOR_1); }
static void Sensor_Floor2_CB(void) { Sensor_TriggerIfExpected(FLOOR_2); }
static void Sensor_Floor3_CB(void) { Sensor_TriggerIfExpected(FLOOR_3); }
static void Sensor_Floor4_CB(void) { Sensor_TriggerIfExpected(FLOOR_4); }

/* Emergency */
static void Emergency_CB(void) { masterElevator.EmergencyActive = TRUE; }

/* TIM3 tick */
static void FsmTick_CB(void) { FsmTickFlag = TRUE; }

static void FSM_RearmTick(void)
{
    Timer_DelayMsAsync(TIMER3, 10U, FsmTick_CB);
}


static void SetIrqPriority(uint8 irqNumber, uint8 priority)
{
    NVIC_IPR_BASE[irqNumber] = (uint8)(priority << 4U);
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

    /*
     * Correct Phase 1 pin mapping:
     *
     * Cabin Floor 1  -> PA10
     * Cabin Floor 2  -> PA11
     * Cabin Floor 3  -> PA12
     * Cabin Floor 4  -> PA15
     *
     * Sensors        -> PB0, PB1, PB8, PB9
     * Emergency      -> PC13
     * PWM Motor LED  -> PA5
     * SPI1 CS (GPIO) -> PB6 (not used when standalone)
     */

    /* Cabin buttons: input pull-up, falling edge */
    Gpio_Init(GPIO_A, 10, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 11, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 12, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 15, GPIO_INPUT, GPIO_PULL_UP);

    Exti_Init(EXTI_LINE_10, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor1_CB);
    Exti_Init(EXTI_LINE_11, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor2_CB);
    Exti_Init(EXTI_LINE_12, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor3_CB);
    Exti_Init(EXTI_LINE_15, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor4_CB);

    /* Floor sensors: input pull-down, rising edge */
    Gpio_Init(GPIO_B, 0, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 1, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 8, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 9, GPIO_INPUT, GPIO_PULL_DOWN);

    Exti_Init(EXTI_LINE_0, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor1_CB);
    Exti_Init(EXTI_LINE_1, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor2_CB);
    Exti_Init(EXTI_LINE_8, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor3_CB);
    Exti_Init(EXTI_LINE_9, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor4_CB);

    /* Emergency stop: input pull-up, falling edge */
    Gpio_Init(GPIO_C, 13, GPIO_INPUT, GPIO_PULL_UP);
    Exti_Init(EXTI_LINE_13, EXTI_PORT_C, EXTI_EDGE_FALLING, Emergency_CB);

    /* PWM motor LED: PA5 = TIM2_CH1 = AF1 */
    Gpio_Init(GPIO_A, 5, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_SetAF(GPIO_A, 5, GPIO_AF1);

    Pwm_Init(TIMER2, PWM_CHANNEL_1, PWM_PSC, PWM_ARR);
    Pwm_Start(TIMER2, PWM_CHANNEL_1);
    Pwm_SetDutyPercent(TIMER2, PWM_CHANNEL_1, MOTOR_DUTY_STOP);


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

    ElevatorFSM_Init((ElevatorData_t*)&masterElevator);

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

            FSM_RearmTick();
        }


        __asm volatile ("WFI");
    }

    return 0;
}