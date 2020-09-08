#include "cpu_thread.h"
#include "kernel/thread.h"
#include "kernel/os_assert.h"

#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <sys/times.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

//-----------------------------------------------------------------
// Locals:
//-----------------------------------------------------------------
static volatile uint32_t _in_interrupt = 0;
static int               _initial_switch = 0;
static sigset_t          _sig_alarm;
static ucontext_t        _initial_ctx;

#define DISABLE_TICK()    sigprocmask(SIG_BLOCK,   &_sig_alarm, NULL);
#define ENABLE_TICK()     sigprocmask(SIG_UNBLOCK, &_sig_alarm, NULL);

//-----------------------------------------------------------------
// cpu_thread_init_tcb:
//-----------------------------------------------------------------
void cpu_thread_init_tcb(struct cpu_tcb *tcb, void (*func)(void *), void *funcArg, void *stack, uint32_t stack_size)
{
    // Not required, but interesting..
    tcb->stack_size  = stack_size;

    // Critical depth = 0 so not in critical section (ints enabled)
    tcb->critical_depth = 0;

    // Create thread context
    getcontext (&tcb->ctx);
    tcb->ctx.uc_link = &_initial_ctx;
    tcb->ctx.uc_stack.ss_sp = stack;
    tcb->ctx.uc_stack.ss_size = stack_size * sizeof (uint32_t);
    makecontext (&tcb->ctx, (void (*) (void)) func, 1, funcArg);
}
//-----------------------------------------------------------------
// cpu_critical_start: Force interrupts to be disabled
//-----------------------------------------------------------------
int cpu_critical_start(void)
{
    struct thread* thread = thread_current();

    // Don't do anything to the interrupt status if already within IRQ
    if (_in_interrupt || thread == NULL)
        return 0;
    
    // Disable interrupts
    DISABLE_TICK();
    
    // Increase critical depth
    thread->tcb.critical_depth++;

    return (int)0;
}
//-----------------------------------------------------------------
// cpu_critical_end: Restore interrupt enable state
//-----------------------------------------------------------------
void cpu_critical_end(int cr)
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
        ENABLE_TICK();
    }

    return;
}
//-----------------------------------------------------------------
// cpu_context_switch:
//-----------------------------------------------------------------
void cpu_context_switch( void )
{
    struct thread* suspend_thread;
    struct thread* resume_thread;

    // Check that this not occuring recursively!
    OS_ASSERT(!_in_interrupt);
    _in_interrupt = 1;

    // Suspend current thread
    suspend_thread = thread_current();
    if (_initial_switch)
    {
        suspend_thread  = NULL;
        _initial_switch = 0;
    }

    // Load new thread context
    thread_load_context(0);

    // Resume new thread
    resume_thread = thread_current();
    _in_interrupt = 0;

    // Only suspend and resume if actually needed
    if (resume_thread != suspend_thread)
    {
        if (suspend_thread)
            swapcontext(&suspend_thread->tcb.ctx, &resume_thread->tcb.ctx);
        else
            setcontext(&resume_thread->tcb.ctx);
    }
}
//-----------------------------------------------------------------
// cpu_context_switch_irq:
//-----------------------------------------------------------------
void cpu_context_switch_irq( void )
{
    // No need to do anything in this system as all interrupts
    // cause the kernel to run...
    OS_ASSERT(_in_interrupt);
}
//-----------------------------------------------------------------
// cpu_tick:
//-----------------------------------------------------------------
static CRITICALFUNC void cpu_tick(int sig)
{
    struct thread* suspend_thread;
    struct thread* resume_thread;

    // Check that this not occuring recursively!
    OS_ASSERT(!_in_interrupt);
    _in_interrupt = 1;

    // Suspend current thread
    suspend_thread = thread_current();

    // Decrement thread sleep timers
    thread_tick();

    // Load new thread context
    thread_load_context(1);

    // Resume new thread
    resume_thread = thread_current();

    _in_interrupt = 0;

    // Only suspend and resume if actually needed
    if (resume_thread != suspend_thread)
    {
        if (suspend_thread)
            swapcontext(&suspend_thread->tcb.ctx, &resume_thread->tcb.ctx);
        else
            setcontext(&resume_thread->tcb.ctx);
    }
}
//-----------------------------------------------------------------
// cpu_thread_start:
//-----------------------------------------------------------------
void cpu_thread_start( void )
{
    struct itimerval itimer, oitimer;
    struct sigaction sigtick;

    _initial_switch = 1;    

    getcontext (&_initial_ctx);

    // Register tick handler
    memset(&sigtick, 0, sizeof(sigtick));
    sigtick.sa_handler = cpu_tick;
    sigaction(SIGVTALRM, &sigtick, NULL);

    sigemptyset(&_sig_alarm);
    sigaddset(&_sig_alarm, SIGVTALRM);

    // Configure timer
    itimer.it_interval.tv_sec  = 0;
    itimer.it_interval.tv_usec = 1 * 1000;
    itimer.it_value.tv_sec     = itimer.it_interval.tv_sec;
    itimer.it_value.tv_usec    = itimer.it_interval.tv_usec;
    setitimer(ITIMER_VIRTUAL, &itimer, NULL);

    // Switch to initial task
    cpu_context_switch();
    while (1)
        ;
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
        assert(0);
    }
    // Early assert
    else
    {
        printf("Assert failed: %s (%s:%d)\r\n", reason, file, line);
        assert(0);
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
    return (int)pCurrent->stack_size;
}
//-----------------------------------------------------------------
// cpu_idle: CPU specific idle function
//-----------------------------------------------------------------
void cpu_idle(void)
{
    // Do nothing
}

#ifdef INCLUDE_TEST_MAIN
//-----------------------------------------------------------------
// main:
//-----------------------------------------------------------------
#define APP_STACK_SIZE      (1024*8)

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