#ifndef __CPU_THREAD_H__
#define __CPU_THREAD_H__

#include "exception.h"

//-----------------------------------------------------------------
// Defines
//-----------------------------------------------------------------
#define STACK_CHK_BYTE      0xCAFEFEAD

// Optional: Define gcc section linkage to relocate critical functions to faster memory
#ifndef CRITICALFUNC
    #define CRITICALFUNC
#endif

//-----------------------------------------------------------------
// Structures
//-----------------------------------------------------------------

// Task Control Block
struct cpu_tcb
{
    volatile struct irq_context *ctx;

    // Stack end pointer
    uint32_t *stack_alloc;

    // Stack size
    uint32_t  stack_size;

    // Critical section / Interrupt status
    uint32_t  critical_depth;
};

typedef uint32_t stk_t;

//-----------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------

// Initialise thread context
void    cpu_thread_init_tcb(struct cpu_tcb *tcb, void (*func)(void *), void *funcArg, uint32_t *stack, uint32_t stack_size );

// Start first thread switch
void    cpu_thread_start(void);

// Force context switch
void    cpu_context_switch(void);

// Force context switch (from IRQ)
void    cpu_context_switch_irq(void);

// Critical section entry & exit
int     cpu_critical_start(void);
void    cpu_critical_end(int cr);

// CPU specific idle function
void    cpu_idle(void);

// Specified thread TCB's free stack entries count
int     cpu_thread_stack_free(struct cpu_tcb * pCurrent);

// Specified thread TCB's total stack size
int     cpu_thread_stack_size(struct cpu_tcb * pCurrent);

// CPU clocks/time measurement functions (optional, used if CONFIG_RTOS_MEASURE_THREAD_TIME defined)
uint64_t cpu_timenow(void);
int64_t  cpu_timediff(uint64_t a, uint64_t b);

// System specific assert handling function
void    cpu_thread_assert(const char *reason, const char *file, int line);

#endif

