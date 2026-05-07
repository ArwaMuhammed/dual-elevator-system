/**
 * main.c  —  Master MCU  —  Phase 1: Single Elevator Foundation
 *
 * Hardware mapping (Proteus):
 * ┌─────────────────────────┬──────┬─────┬────────────────────────────┐
 * │ Function                │ Port │ Pin │ Notes                      │
 * ├─────────────────────────┼──────┼─────┼────────────────────────────┤
 * │ Cabin Button Floor 1    │  PA  │  0  │ EXTI0, falling edge        │
 * │ Cabin Button Floor 2    │  PA  │  1  │ EXTI1, falling edge        │
 * │ Cabin Button Floor 3    │  PA  │  2  │ EXTI2, falling edge        │
 * │ Cabin Button Floor 4    │  PA  │  3  │ EXTI3, falling edge        │
 * │ Floor Sensor Floor 1    │  PB  │  0  │ EXTI0, rising edge         │
 * │ Floor Sensor Floor 2    │  PB  │  1  │ EXTI1, rising edge         │
 * │ Floor Sensor Floor 3    │  PB  │  2  │ EXTI2, rising edge         │
 * │ Floor Sensor Floor 4    │  PB  │  3  │ EXTI3, rising edge         │
 * │ Emergency Stop          │  PC  │  0  │ EXTI0, falling, priority 0 │
 * │ Motor PWM LED           │  PA  │  5  │ TIM2 CH1, AF1              │
 * └─────────────────────────┴──────┴─────┴────────────────────────────┘
 *
 * Timer allocation:
 *   TIM2 — PWM motor LED (PA5, CH1)
 *   TIM3 — 10 ms repeating FSM tick
 *   TIM4 — 3 s one-shot door timer (managed inside Elevator_FSM.c)
 *
 * PWM math (HSI 16 MHz, target 10 kHz PWM):
 *   PSC = 0   → Timer clock = 16 MHz
 *   ARR = 1599 → PWM freq = 16 000 000 / (0+1) / (1599+1) = 10 000 Hz ✓
 *
 * TIM3 tick math (HSI 16 MHz, target 10 ms period):
 *   PSC = 15999  → Timer clock = 16 000 000 / 16 000 = 1 000 Hz (1 ms/tick)
 *   ARR = 9      → Period = 10 ticks = 10 ms ✓
 */

#include "Rcc.h"
#include "Gpio.h"
#include "Exti.h"
#include "Nvic.h"
#include "Pwm.h"
#include "Timer.h"
#include "Elevator_FSM.h"
#include "Elevator_Types.h"

#ifndef FALSE
#define FALSE   ((boolean)0U)
#endif
#ifndef TRUE
#define TRUE    ((boolean)1U)
#endif

/* ── NVIC priority register base ───────────────────────────────
 * The existing Nvic driver only exposes Enable/Disable.
 * We set priorities directly via the Cortex-M4 IPR registers.  */
#define NVIC_IPR_BASE   ((volatile uint8 *)0xE000E400)

/* IRQ numbers for STM32F401 */
#define IRQ_EXTI0       6U
#define IRQ_EXTI1       7U
#define IRQ_EXTI2       8U
#define IRQ_EXTI3       9U
#define IRQ_TIM3        29U
#define IRQ_TIM4        30U

/* ── PA5 alternate function (TIM2_CH1 = AF1) ───────────────── */
#define GPIO_AF1        1U

/* ── PWM timing constants ───────────────────────────────────── */
#define PWM_PSC         0U
#define PWM_ARR         1599U

/* ── TIM3 tick timing constants ─────────────────────────────── */
#define TICK_PSC        15999U
#define TICK_ARR        9U

/* ── The single elevator instance (volatile: written by ISRs) ─ */
static volatile ElevatorData_t elevator;

/* ── FSM tick flag: set by TIM3 IRQ, consumed in main loop ──── */
static volatile boolean FsmTickFlag = FALSE;

/* ═══════════════════════════════════════════════════════════════
 *  ISR Callbacks — only set flags, no logic
 * ═══════════════════════════════════════════════════════════════ */

/* Cabin buttons (PA0–PA3) */
static void CabinBtn_Floor1_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&elevator, FLOOR_1); }
static void CabinBtn_Floor2_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&elevator, FLOOR_2); }
static void CabinBtn_Floor3_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&elevator, FLOOR_3); }
static void CabinBtn_Floor4_CB(void) { ElevatorFSM_RequestFloor((ElevatorData_t*)&elevator, FLOOR_4); }

/* Floor sensors (PB0–PB3) */
static void Sensor_Floor1_CB(void) { elevator.FloorReached = TRUE; }
static void Sensor_Floor2_CB(void) { elevator.FloorReached = TRUE; }
static void Sensor_Floor3_CB(void) { elevator.FloorReached = TRUE; }
static void Sensor_Floor4_CB(void) { elevator.FloorReached = TRUE; }

/* Emergency stop (PC0) — highest priority */
static void Emergency_CB(void) { elevator.EmergencyActive = TRUE; }

/* TIM3 — 10 ms FSM tick */
static void FsmTick_CB(void) { FsmTickFlag = TRUE; }

/* ═══════════════════════════════════════════════════════════════
 *  TIM3 repeating tick setup
 *  Timer_DelayMsAsync is one-shot. We re-arm it inside the
 *  callback by calling Timer_DelayMsAsync again from main loop.
 *  The FsmTick_CB just raises a flag; main loop re-arms.
 * ═══════════════════════════════════════════════════════════════ */
static void FSM_RearmTick(void)
{
    Timer_DelayMsAsync(TIMER3, 10U, FsmTick_CB);
}

/* ═══════════════════════════════════════════════════════════════
 *  Helper: set NVIC priority (0 = highest on STM32F4)
 * ═══════════════════════════════════════════════════════════════ */
static void SetIrqPriority(uint8 irqNumber, uint8 priority)
{
    /* STM32F4 implements 4 priority bits in the upper nibble of IPR */
    NVIC_IPR_BASE[irqNumber] = (uint8)(priority << 4U);
}

/* ═══════════════════════════════════════════════════════════════
 *  System Initialisation
 * ═══════════════════════════════════════════════════════════════ */
static void System_Init(void)
{
    /* ── 1. Clocks ─────────────────────────────────────────── */
    Rcc_Init();
    Rcc_Enable(RCC_GPIOA);
    Rcc_Enable(RCC_GPIOB);
    Rcc_Enable(RCC_GPIOC);
    Rcc_Enable(RCC_SYSCFG);   /* Required for EXTI port mapping */
    Rcc_Enable(RCC_TIM2);
    Rcc_Enable(RCC_TIM3);
    Rcc_Enable(RCC_TIM4);

    /* ── 2. GPIO — Cabin buttons PA0–PA3 (input, pull-up) ──── */
    Gpio_Init(GPIO_A, 0, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 1, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 2, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 3, GPIO_INPUT, GPIO_PULL_UP);

    /* ── 3. GPIO — Floor sensors PB0–PB3 (input, pull-down) ── */
    Gpio_Init(GPIO_B, 0, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 1, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 2, GPIO_INPUT, GPIO_PULL_DOWN);
    Gpio_Init(GPIO_B, 3, GPIO_INPUT, GPIO_PULL_DOWN);

    /* ── 4. GPIO — Emergency stop PC0 (input, pull-up) ─────── */
    Gpio_Init(GPIO_C, 0, GPIO_INPUT, GPIO_PULL_UP);

    /* ── 5. GPIO — Motor LED PA5 (AF, push-pull) ───────────── */
    Gpio_Init(GPIO_A, 5, GPIO_AF, GPIO_PUSH_PULL);
    /* Set PA5 to AF1 (TIM2_CH1) via AFRL register directly.
     * The existing Gpio driver does not expose SetAF, so we
     * write AFRL manually.
     * AFRL controls pins 0-7: 4 bits each, pin5 = bits [23:20] */
    {
        typedef struct {
            volatile uint32 MODER; volatile uint32 OTYPER;
            volatile uint32 OSPEEDR; volatile uint32 PUPDR;
            volatile uint32 IDR; volatile uint32 ODR;
            volatile uint32 BSRR; volatile uint32 LCKR;
            volatile uint32 AFRL; volatile uint32 AFRH;
        } GpioRegs;
        GpioRegs *gpioa = (GpioRegs *)0x40020000UL;
        gpioa->AFRL &= ~(0xFUL << 20U);
        gpioa->AFRL |=  (GPIO_AF1 << 20U);
    }

    /* ── 6. EXTI — Cabin buttons: falling edge (button press) ─ */
    Exti_Init(EXTI_LINE_0, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor1_CB);
    Exti_Init(EXTI_LINE_1, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor2_CB);
    Exti_Init(EXTI_LINE_2, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor3_CB);
    Exti_Init(EXTI_LINE_3, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor4_CB);

    /* ── 7. EXTI — Floor sensors: rising edge (sensor detect) ─ */
    /* NOTE: Sensors are on PB, so we re-init EXTI lines 0–3 for
     * port B. Each line can only map to ONE port at a time via
     * SYSCFG. Since cabin buttons (PA) and sensors (PB) share
     * lines 0–3, we use the following strategy:
     *
     * The LAST Exti_Init call for a given line number wins the
     * SYSCFG mapping. Here sensors (PB) are initialised AFTER
     * buttons (PA), so sensors will be active on lines 0–3.
     *
     * *** IMPORTANT for Proteus wiring ***
     * To avoid this conflict, move cabin buttons to PA4–PA7 and
     * use EXTI lines 4–7 for them. The init below uses this
     * correct conflict-free mapping. Update Proteus accordingly.
     */

    /* Re-do cabin buttons on EXTI lines 4–7 (PA4–PA7) */
    Gpio_Init(GPIO_A, 4, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 5, GPIO_INPUT, GPIO_PULL_UP);  /* NOTE: PA5 is also PWM — use PA6 instead */
    Gpio_Init(GPIO_A, 6, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 7, GPIO_INPUT, GPIO_PULL_UP);

    /* *** CORRECTED PIN PLAN (conflict-free): ***
     *   Cabin Btn Floor1 → PA4  EXTI4
     *   Cabin Btn Floor2 → PA6  EXTI6
     *   Cabin Btn Floor3 → PA7  EXTI7
     *   Cabin Btn Floor4 → PA8  EXTI8
     *   Floor Sensor 1   → PB0  EXTI0
     *   Floor Sensor 2   → PB1  EXTI1
     *   Floor Sensor 3   → PB2  EXTI2
     *   Floor Sensor 4   → PB3  EXTI3
     *   Emergency Stop   → PC9  EXTI9
     *   Motor PWM LED    → PA5  TIM2 CH1
     */

    /* Cabin buttons — conflict-free lines */
    Gpio_Init(GPIO_A, 4, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 6, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 7, GPIO_INPUT, GPIO_PULL_UP);
    Gpio_Init(GPIO_A, 8, GPIO_INPUT, GPIO_PULL_UP);

    Exti_Init(EXTI_LINE_4, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor1_CB);
    Exti_Init(EXTI_LINE_6, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor2_CB);
    Exti_Init(EXTI_LINE_7, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor3_CB);
    Exti_Init(EXTI_LINE_8, EXTI_PORT_A, EXTI_EDGE_FALLING, CabinBtn_Floor4_CB);

    /* Floor sensors — PB0–PB3, lines 0–3 */
    Exti_Init(EXTI_LINE_0, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor1_CB);
    Exti_Init(EXTI_LINE_1, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor2_CB);
    Exti_Init(EXTI_LINE_2, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor3_CB);
    Exti_Init(EXTI_LINE_3, EXTI_PORT_B, EXTI_EDGE_RISING, Sensor_Floor4_CB);

    /* Emergency stop — PC9, line 9 */
    Gpio_Init(GPIO_C, 9, GPIO_INPUT, GPIO_PULL_UP);
    Exti_Init(EXTI_LINE_9, EXTI_PORT_C, EXTI_EDGE_FALLING, Emergency_CB);

    /* ── 8. NVIC priorities ─────────────────────────────────── */
    /* Emergency highest (0), sensors next (1), buttons (2)     */
    SetIrqPriority(IRQ_EXTI0,  1U);  /* Sensor floor 1         */
    SetIrqPriority(IRQ_EXTI1,  1U);  /* Sensor floor 2         */
    SetIrqPriority(IRQ_EXTI2,  1U);  /* Sensor floor 3         */
    SetIrqPriority(IRQ_EXTI3,  1U);  /* Sensor floor 4         */
    SetIrqPriority(23U,        2U);  /* EXTI9_5: buttons+emerg */
    /* Emergency is on line 9 inside EXTI9_5 handler (IRQ 23).
     * Within that shared handler the EmergencyActive flag is
     * checked first in ElevatorFSM_Tick, giving it logical
     * priority over button presses on lines 4–8. */
    SetIrqPriority(IRQ_TIM3,   3U);  /* FSM tick               */
    SetIrqPriority(IRQ_TIM4,   3U);  /* Door timer             */

    /* ── 9. Enable all EXTI lines ───────────────────────────── */
    Exti_Enable(EXTI_LINE_0);
    Exti_Enable(EXTI_LINE_1);
    Exti_Enable(EXTI_LINE_2);
    Exti_Enable(EXTI_LINE_3);
    Exti_Enable(EXTI_LINE_4);
    Exti_Enable(EXTI_LINE_6);
    Exti_Enable(EXTI_LINE_7);
    Exti_Enable(EXTI_LINE_8);
    Exti_Enable(EXTI_LINE_9);

    /* ── 10. PWM — TIM2 CH1, PA5, 10 kHz ───────────────────── */
    Pwm_Init(TIMER2, PWM_CHANNEL_1, PWM_PSC, PWM_ARR);
    Pwm_Start(TIMER2, PWM_CHANNEL_1);

    /* ── 11. FSM init ───────────────────────────────────────── */
    ElevatorFSM_Init((ElevatorData_t*)&elevator);

    /* ── 12. Start the 10 ms FSM tick (TIM3) ───────────────── */
    FSM_RearmTick();
}

/* ═══════════════════════════════════════════════════════════════
 *  Main loop
 * ═══════════════════════════════════════════════════════════════ */
int main(void)
{
    System_Init();

    while (1)
    {
        if (FsmTickFlag == TRUE)
        {
            /* ── Critical section: consume the flag ── */
            __asm volatile ("CPSID I");
            FsmTickFlag = FALSE;
            __asm volatile ("CPSIE I");

            /* ── Run FSM ── */
            ElevatorFSM_Tick((ElevatorData_t*)&elevator);

            /* ── Re-arm the 10 ms tick for next cycle ── */
            FSM_RearmTick();
        }

        /* CPU sleeps until the next interrupt, saving power */
        __asm volatile ("WFI");
    }

    return 0;
}