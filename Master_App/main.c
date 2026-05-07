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
#define IRQ_EXTI2       8U
#define IRQ_EXTI3       9U
#define IRQ_EXTI9_5     23U
#define IRQ_TIM3        29U
#define IRQ_TIM4        30U

#define PWM_PSC         0U
#define PWM_ARR         1599U

static volatile ElevatorData_t elevator;
static volatile boolean FsmTickFlag = FALSE;

/* Cabin buttons */
static void CabinBtn_Floor1_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&elevator, FLOOR_1); }
static void CabinBtn_Floor2_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&elevator, FLOOR_2); }
static void CabinBtn_Floor3_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&elevator, FLOOR_3); }
static void CabinBtn_Floor4_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&elevator, FLOOR_4); }

/* Floor sensors */
static void Sensor_Floor1_CB(void) { elevator.FloorReached = TRUE; }
static void Sensor_Floor2_CB(void) { elevator.FloorReached = TRUE; }
static void Sensor_Floor3_CB(void) { elevator.FloorReached = TRUE; }
static void Sensor_Floor4_CB(void) { elevator.FloorReached = TRUE; }

/* Emergency */
static void Emergency_CB(void) { elevator.EmergencyActive = TRUE; }

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
     * Cabin Floor 1  -> PA4
     * Cabin Floor 2  -> PA6
     * Cabin Floor 3  -> PA7
     * Cabin Floor 4  -> PA8
     *
     * Sensors        -> PB0, PB1, PB2, PB3
     * Emergency      -> PC9
     * PWM Motor LED  -> PA5
     */

    /* Cabin buttons: input pull-up, falling edge */
    Gpio_Init(GPIO_A, 4, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 6, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 7, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 8, GPIO_INPUT, GPIO_PULL_UP);

    Exti_Init(EXTI_LINE_4, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor1_CB);
    Exti_Init(EXTI_LINE_6, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor2_CB);
    Exti_Init(EXTI_LINE_7, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor3_CB);
    Exti_Init(EXTI_LINE_8, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor4_CB);

    /* Floor sensors: input pull-down, rising edge */
    Gpio_Init(GPIO_B, 0, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 1, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 2, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 3, GPIO_INPUT, GPIO_PULL_DOWN);

    Exti_Init(EXTI_LINE_0, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor1_CB);
    Exti_Init(EXTI_LINE_1, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor2_CB);
    Exti_Init(EXTI_LINE_2, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor3_CB);
    Exti_Init(EXTI_LINE_3, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor4_CB);

    /* Emergency stop: input pull-up, falling edge */
    Gpio_Init(GPIO_C, 9, GPIO_INPUT, GPIO_PULL_UP);
    Exti_Init(EXTI_LINE_9, EXTI_PORT_C, EXTI_EDGE_FALLING, Emergency_CB);

    /* PWM motor LED: PA5 = TIM2_CH1 = AF1 */
    Gpio_Init(GPIO_A, 5, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_SetAF(GPIO_A, 5, GPIO_AF1);

    Pwm_Init(TIMER2, PWM_CHANNEL_1, PWM_PSC, PWM_ARR);
    Pwm_Start(TIMER2, PWM_CHANNEL_1);
    Pwm_SetDutyPercent(TIMER2, PWM_CHANNEL_1, MOTOR_DUTY_STOP);

    /* NVIC priorities */
    SetIrqPriority(IRQ_EXTI0,   1U);
    SetIrqPriority(IRQ_EXTI1,   1U);
    SetIrqPriority(IRQ_EXTI2,   1U);
    SetIrqPriority(IRQ_EXTI3,   1U);
    SetIrqPriority(IRQ_EXTI9_5, 0U);
    SetIrqPriority(IRQ_TIM3,    3U);
    SetIrqPriority(IRQ_TIM4,    3U);

    /* Enable EXTI lines */
    Exti_Enable(EXTI_LINE_0);
    Exti_Enable(EXTI_LINE_1);
    Exti_Enable(EXTI_LINE_2);
    Exti_Enable(EXTI_LINE_3);
    Exti_Enable(EXTI_LINE_4);
    Exti_Enable(EXTI_LINE_6);
    Exti_Enable(EXTI_LINE_7);
    Exti_Enable(EXTI_LINE_8);
    Exti_Enable(EXTI_LINE_9);

    ElevatorFSM_Init((ElevatorData_t*)&elevator);

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

            ElevatorFSM_Tick((ElevatorData_t*)&elevator);

            FSM_RearmTick();
        }

        __asm volatile ("WFI");
    }

    return 0;
}