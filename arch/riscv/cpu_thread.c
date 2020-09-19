#include "cpu_thread.h"
#include "kernel/thread.h"
#include "kernel/os_assert.h"

#include "exception.h"
#include "csr.h"
#include "assert.h"
#include "timer.h"

//-----------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------

// Macro used to stop profiling functions being called
#define NO_PROFILE              __attribute__((__no_instrument_function__))

#define WEAK                    __attribute__((weak))

// Preempt rate
#define TICK_RATE_HZ            1000

//-----------------------------------------------------------------
// Locals:
//-----------------------------------------------------------------
// Detect recursive interrupts (which are not supported)
static volatile uint32_t _in_interrupt    = 0;
static fp_irq            _platform_irq_cb = 0;

//-----------------------------------------------------------------
// cpu_thread_init_tcb: Initialise thread context
//-----------------------------------------------------------------
void cpu_thread_init_tcb(struct cpu_tcb *tcb, void (*func)(void *), void *funcArg, uint32_t *stack, uint32_t stack_size)
{
    int i;

    OS_ASSERT(stack != NULL);

    tcb->stack_alloc = stack;
    tcb->stack_size  = stack_size;

    // Set default check byte 
    for (i=0;i<tcb->stack_size;i++)
        tcb->stack_alloc[i] = STACK_CHK_BYTE;

    tcb->ctx = exception_makecontext(stack, stack_size, func, funcArg);

    // Enable IRQ within this context
    context_irq_enable(tcb->ctx, 1);

    // Critical depth = 0 so not in critical section (ints enabled)
    tcb->critical_depth = 0;
}
//-----------------------------------------------------------------
// cpu_critical_start: Force interrupts to be disabled
//-----------------------------------------------------------------
int NO_PROFILE cpu_critical_start(void)
{
    struct thread* thread = thread_current();

    // Don't do anything to the interrupt status if already within IRQ
    if (_in_interrupt || thread == NULL)
        return 0;

    // Disable interrupts
    csr_clr_irq_enable();
    
    // Increase critical depth
    thread->tcb.critical_depth++;

    return (int)0;
}
//-----------------------------------------------------------------
// cpu_critical_end: Restore interrupt enable state
//-----------------------------------------------------------------
void NO_PROFILE cpu_critical_end(int cr)
{
    struct thread* thread = thread_current();

    // Don't do anything to the interrupt status if already within IRQ
    if (_in_interrupt || thread == NULL)
        return ;

    OS_ASSERT(thread->tcb.critical_depth > 0);
    OS_ASSERT(thread->tcb.critical_depth < 255);

    // Decrement critical depth
    thread->tcb.critical_depth--;

    // End of critical section?
    if (thread->tcb.critical_depth == 0)
    {
        // Manually re-enable IRQ
        csr_set_irq_enable();
    }

    return;
}
//-----------------------------------------------------------------
// cpu_context_switch: Force context switch
//-----------------------------------------------------------------
void cpu_context_switch( void )
{
    OS_ASSERT(!_in_interrupt);

    // Cause context switch
    exception_syscall(0);
}
//-----------------------------------------------------------------
// cpu_context_switch_irq: Force context switch (from IRQ)
//-----------------------------------------------------------------
void cpu_context_switch_irq( void )
{
    // No need to do anything in this system as all interrupts
    // cause the kernel to run...
    OS_ASSERT(_in_interrupt);
}
//-----------------------------------------------------------------
// cpu_syscall: Handle system call exception
//-----------------------------------------------------------------
static CRITICALFUNC struct irq_context * cpu_syscall(struct irq_context *ctx)
{
    struct thread* thread;

    // Check that this not occuring recursively!
    OS_ASSERT(!_in_interrupt);
    _in_interrupt = 1;

    // Record stack pointer in current task TCB
    thread = thread_current();
    if (thread)
    {
        // Stack range check
        OS_ASSERT(((uint32_t)ctx->reg[REG_SP] >= (uint32_t)thread->tcb.stack_alloc) &&
                  ((uint32_t)ctx->reg[REG_SP] < (uint32_t)(&thread->tcb.stack_alloc[thread->tcb.stack_size])));

        thread->tcb.ctx = ctx;
        
        // Try and detect stack overflow
        OS_ASSERT(thread->tcb.stack_alloc[0] == STACK_CHK_BYTE);
    }

    // Load new thread context
    thread_load_context(0);

    // Get stack frame of new thread
    thread = thread_current();
    ctx = (uint32_t *)thread->tcb.ctx;

    // Try and detect stack overflow
    OS_ASSERT(thread->tcb.stack_alloc[0] == STACK_CHK_BYTE);

    _in_interrupt = 0;

    return ctx;
}
//-----------------------------------------------------------------
// cpu_timer_irq: Handle (timer) interrupt exception
//-----------------------------------------------------------------
static CRITICALFUNC struct irq_context * cpu_timer_irq(struct irq_context *ctx)
{
    struct thread* thread;

    // Check that this not occuring recursively!
    OS_ASSERT(!_in_interrupt);
    _in_interrupt = 1;

    // Record stack pointer in current task TCB
    thread = thread_current();
    if (thread)
    {
        // Stack range check
        OS_ASSERT(((uint32_t)ctx->reg[REG_SP] >= (uint32_t)thread->tcb.stack_alloc) &&
                  ((uint32_t)ctx->reg[REG_SP] < (uint32_t)(&thread->tcb.stack_alloc[thread->tcb.stack_size])));

        thread->tcb.ctx = ctx;
        
        // Try and detect stack overflow
        OS_ASSERT(thread->tcb.stack_alloc[0] == STACK_CHK_BYTE);
    }

    // Handle thread scheduling
    thread_tick();

    // Reset timer (ack pending interrupt)
    timer_set_mtimecmp(timer_get_mtime() + (MCU_CLK/TICK_RATE_HZ));
    csr_clear(mip, SR_IP_MTIP);
    csr_set(mie, SR_IP_MTIP);
    
    // Load new thread context
    thread_load_context(1);

    // Get stack frame of new thread
    thread = thread_current();
    ctx = (uint32_t *)thread->tcb.ctx;

    // Try and detect stack overflow
    OS_ASSERT(thread->tcb.stack_alloc[0] == STACK_CHK_BYTE);

    _in_interrupt = 0;

    return ctx;
}
//-----------------------------------------------------------------
// cpu_irq_wrapper: Handle (external) interrupt exception
//-----------------------------------------------------------------
static CRITICALFUNC struct irq_context * cpu_irq_wrapper(struct irq_context *ctx)
{
    OS_ASSERT(_platform_irq_cb);

    // Check that this not occuring recursively!
    OS_ASSERT(!_in_interrupt);
    _in_interrupt = 1;
    ctx = _platform_irq_cb(ctx);
    _in_interrupt = 0;

    return ctx;
}
//-----------------------------------------------------------------
// cpu_thread_start:
//-----------------------------------------------------------------
void cpu_thread_start( void )
{
    // Fault handlers
    exception_set_syscall_handler(cpu_syscall);
    exception_set_timer_handler(cpu_timer_irq);

    _platform_irq_cb = exception_get_irq_handler();
    exception_set_irq_handler(cpu_irq_wrapper);

    // Make sure current IRQ enable is disabled
    csr_clr_irq_enable();

    // Enable timer IRQ source (global IRQ still disabled)
    timer_set_mtimecmp(timer_get_mtime() + (MCU_CLK/TICK_RATE_HZ));
    csr_set(mie, SR_IP_MTIP);

    // Run the scheduler to pick the highest prio thread
    thread_load_context(0);

    // Return to context of initial thread
    exception_return(thread_current()->tcb.ctx);
}
//-----------------------------------------------------------------
// cpu_thread_assert: Assert handler
//-----------------------------------------------------------------
void cpu_thread_assert(const char *reason, const char *file, int line)
{
    struct thread* thread;

    cpu_critical_start();

    thread = thread_current();
    if (thread)
    {
        printf("[%s] Assert failed: %s (%s:%d)\r\n", thread->name, reason, file, line);

        // Dump thread list
        thread_dump_list(printf);

        {
            uint32_t *registers = (uint32_t *)thread->tcb.ctx;
            int i;

            printf("Frame:\n");
            printf(" Critical Depth 0x%x\n", thread->tcb.critical_depth);
            printf(" PC 0x%x\n", thread->tcb.ctx->pc);
            printf(" SR 0x%x\n", thread->tcb.ctx->status);

            for (i=0;i<32;i++)
                printf(" R%x = 0x%x\n", i, thread->tcb.ctx->reg[i]);
        }
        
        while (1)
            ;
    }
    // Early assert
    else
    {
        printf("Assert failed: %s (%s:%d)\r\n", reason, file, line);
        while (1)
            ;
    }
}
//-----------------------------------------------------------------
// cpu_thread_stack_size:
//-----------------------------------------------------------------
int cpu_thread_stack_size(struct cpu_tcb * pCurrent)
{
    return (int)pCurrent->stack_size;
}
//-----------------------------------------------------------------
// cpu_thread_stack_free:
//-----------------------------------------------------------------
int cpu_thread_stack_free(struct cpu_tcb * pCurrent)
{
    int i;
    int free = 0;

    for (i=0;i<pCurrent->stack_size;i++)
    {
        if (pCurrent->stack_alloc[i] != STACK_CHK_BYTE)
            break;
        else
            free++;
    }

    return free;
}
//-----------------------------------------------------------------
// cpu_idle: CPU specific idle function
//-----------------------------------------------------------------
WEAK void cpu_idle(void)
{
    // Do nothing
}
#ifdef INCLUDE_TEST_MAIN
//-----------------------------------------------------------------
// main:
//-----------------------------------------------------------------
#define APP_STACK_SIZE      (1024)

//-----------------------------------------------------------------
// Locals:
//-----------------------------------------------------------------
THREAD_DECL(app, APP_STACK_SIZE);

extern void* testcase(void * a);

//-----------------------------------------------------------------
// main:
//-----------------------------------------------------------------
int main(void)
{
    // Setup the RTOS
    thread_kernel_init();

    // Create init thread
    THREAD_INIT(app, "INIT", testcase, NULL, THREAD_MAX_PRIO - 1);

    // Start kernel
    thread_kernel_run();

    // Kernel should never init
    OS_ASSERT(0);
}
#endif
