#ifndef APP_COMMON_CRITICAL_H
#define APP_COMMON_CRITICAL_H

/* Simple global interrupt gate for short critical sections. */
static inline void Enter_Critical(void) { __asm volatile ("CPSID I"); }
static inline void Exit_Critical(void)  { __asm volatile ("CPSIE I"); }

#endif /* APP_COMMON_CRITICAL_H */

