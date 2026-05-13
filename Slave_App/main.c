#include "stm32f401xe.h"
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
#define IRQ_EXTI4       10U
#define IRQ_EXTI15_10   40U
#define IRQ_TIM3        29U
#define IRQ_TIM4        30U

#define PWM_PSC         0U
#define PWM_ARR         1599U

static volatile ElevatorData_t slaveElevator;
static volatile boolean FsmTickFlag = FALSE;
static volatile uint8 SpiTimeoutCounter = 0U;
static SpiFrame_t SlaveRxFrame;
static SpiFrame_t SlaveTxFrame;
static volatile uint8 SlaveRxIndex = 0U;

static void Slave_PrepareTxFrame(void) { SlaveControl_BuildTxFrame(&SlaveTxFrame); }

/* ───────────────── NON-BLOCKING SPI SLAVE INTERRUPTS ───────────────── */

void SPI1_IRQHandler(void)
{
    if (SPI1->SR & (1U << 0)) /* Check RXNE */
    {
        ((uint8*)&SlaveRxFrame)[SlaveRxIndex] = SPI1->DR;
        SlaveRxIndex++;

        if (SlaveRxIndex < SPI_FRAME_SIZE) {
            SPI1->DR = ((uint8*)&SlaveTxFrame)[SlaveRxIndex];
        } else {
            SlaveControl_OnRxFrame(&SlaveRxFrame);
            SlaveRxIndex = 0U;
        }
    }
}

static void Slave_CS_Rising_CB(void)
{
    SpiTimeoutCounter = 0U;
    SlaveRxIndex = 0U;
    Slave_PrepareTxFrame();

    volatile uint8 dummy = SPI1->DR;
    (void)dummy;

    SPI1->DR = ((uint8*)&SlaveTxFrame)[0];
}

/* ─────────────────────────────────────────────────────────────────────── */

static void CabinBtn_Floor1_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&slaveElevator, FLOOR_1); }
static void CabinBtn_Floor2_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&slaveElevator, FLOOR_2); }
static void CabinBtn_Floor3_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&slaveElevator, FLOOR_3); }
static void CabinBtn_Floor4_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&slaveElevator, FLOOR_4); }

static void Emergency_CB(void) { slaveElevator.EmergencyActive = TRUE; }

static void FsmTick_CB(void) { FsmTickFlag = TRUE; }
static void FSM_RearmTick(void) { Timer_DelayMsAsync(TIMER3, 10U, FsmTick_CB); }
static void SetIrqPriority(uint8 irqNumber, uint8 priority) { NVIC_IPR_BASE[irqNumber] = (uint8)(priority << 4U); }

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

    Gpio_Init(GPIO_A, 10, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 11, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 12, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 15, GPIO_INPUT, GPIO_PULL_UP);
    Exti_Init(EXTI_LINE_10, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor1_CB);
    Exti_Init(EXTI_LINE_11, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor2_CB);
    Exti_Init(EXTI_LINE_12, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor3_CB);
    Exti_Init(EXTI_LINE_15, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor4_CB);

    Gpio_Init(GPIO_C, 13, GPIO_INPUT, GPIO_PULL_UP);
    Exti_Init(EXTI_LINE_13, EXTI_PORT_C, EXTI_EDGE_FALLING, Emergency_CB);

    Gpio_Init(GPIO_A, 5, GPIO_AF, GPIO_PUSH_PULL);
    Gpio_SetAF(GPIO_A, 5, GPIO_AF1);
    Pwm_Init(TIMER2, PWM_CHANNEL_1, PWM_PSC, PWM_ARR);
    Pwm_Start(TIMER2, PWM_CHANNEL_1);
    Pwm_SetDutyPercent(TIMER2, PWM_CHANNEL_1, MOTOR_DUTY_STOP);

    Exti_Init(EXTI_LINE_4, EXTI_PORT_A, EXTI_EDGE_RISING, Slave_CS_Rising_CB);
    SetIrqPriority(IRQ_EXTI4, 1U);
    Exti_Enable(EXTI_LINE_4);

    Spi1_Init(SPI_SLAVE, SPI_IDLE_LOW, SPI_SAMPLE_FIRST_TRANSITION);

    SetIrqPriority(IRQ_EXTI15_10,  0U);
    SetIrqPriority(IRQ_TIM3,       3U);
    SetIrqPriority(IRQ_TIM4,       3U);

    Exti_Enable(EXTI_LINE_10);
    Exti_Enable(EXTI_LINE_11);
    Exti_Enable(EXTI_LINE_12);
    Exti_Enable(EXTI_LINE_13);
    Exti_Enable(EXTI_LINE_15);

    ElevatorFSM_Init((ElevatorData_t*)&slaveElevator);
    SlaveControl_Init((ElevatorData_t*)&slaveElevator);

    /* Safety Delay for Proteus */
    for(volatile uint32 i=0; i<100000; i++);

    Slave_CS_Rising_CB();
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

            SpiTimeoutCounter++;
            if (SpiTimeoutCounter >= 20U) {
                SlaveControl_SetIndependent(TRUE);
            } else {
                SlaveControl_SetIndependent(FALSE);
            }

            ElevatorFSM_Tick((ElevatorData_t*)&slaveElevator);
            FSM_RearmTick();
        }
    }
    return 0;
}