#ifndef __THREAD_H__
#define __THREAD_H__

//-----------------------------------------------------------------
// Includes
//-----------------------------------------------------------------
#include "cpu_thread.h"
#include "list.h"

//-----------------------------------------------------------------
// Basic Types
//-----------------------------------------------------------------
#ifndef CONFIG_RTOS_HAS_NO_STDINT
    #include <stdint.h>
#else
    typedef unsigned char       uint8_t;
    typedef signed char         int8_t;
    typedef unsigned short      uint16_t;
    typedef signed short        int16_t;
    typedef unsigned long       uint32_t;
    typedef signed   long       int32_t;
    typedef unsigned long long  uint64_t;
    typedef signed   long long  int64_t;
#endif

//-----------------------------------------------------------------
// Standard Defines
// (should already be available if standard header files are available)
//-----------------------------------------------------------------
#ifndef NULL
    #define NULL    0
#endif

#ifndef FALSE
    #define FALSE   0
#endif

#ifndef TRUE
    #define TRUE    1
#endif

//-----------------------------------------------------------------
// Defines
//-----------------------------------------------------------------

// Max name length for a thread
#define THREAD_NAME_LEN     16

// Idle task priority (do not change)
#define THREAD_IDLE_PRIO    -1

// Min thread priority number
#define THREAD_MIN_PRIO     0

// Max thread priority number
#define THREAD_MAX_PRIO     10

// Min interrupt priority
#define THREAD_INT_PRIO     (THREAD_MAX_PRIO + 1)

// Thread structure checkword
#define THREAD_CHECK_WORD   0xFEADDE01

// Thread sleep arg used to yield
#define THREAD_YIELD        0

//-----------------------------------------------------------------
// Enums
//-----------------------------------------------------------------

// Thread state enumeration
typedef enum eThreadState
{
    THREAD_RUNABLE,
    THREAD_SLEEPING,
    THREAD_BLOCKED,
    THREAD_DEAD
} tThreadState;

//-----------------------------------------------------------------
// Types
//-----------------------------------------------------------------
struct thread
{
    // CPU specific thread state
    struct cpu_tcb  tcb;

    // Unique thread ID
    int             thread_id;

    // Thread name (used in debug output)
    char            name[THREAD_NAME_LEN];

    // Thread priority
    int             priority;

    // state (Run-able, blocked or sleeping)
    tThreadState    state;

#ifdef CONFIG_RTOS_ABSOLUTE_TIME
    // Sleep wake time (absolute)
    uint64_t        wakeup_time;
#else
    // Sleep time remaining (ticks) (delta)
    uint32_t        wait_delta;
#endif

    // Thread run count
    uint32_t        run_count;

#ifdef CONFIG_RTOS_MEASURE_THREAD_TIME
    // Measure time each thread is active for?
    uint32_t        run_time;
    uint32_t        run_start;
#endif

    // Thread function
    void           *(*thread_func)(void *);
    void            *thread_arg;

    // Run/Sleep/Blocked list node
    struct link_node node;

    // next thread in complete name list
    struct thread   *next_all;

    // Blocking node items
    struct link_node blocking_node;
    void*           unblocking_arg;

    // List of threads pending on thread exit
    struct link_list join_list;
    void            *exit_value;

    // Thread check word
    uint32_t        checkword;
};

//-----------------------------------------------------------------
// Macros
//-----------------------------------------------------------------
#define THREAD_DECL(id, stack_size) \
        static struct thread thread_ ## id; \
        static stk_t stack_ ## id[stack_size]

#define THREAD_INIT(id, name, func, arg, prio) \
        thread_init(& thread_ ## id, name, prio, func, (void*)(arg), stack_ ## id, sizeof(stack_ ## id) / sizeof(stk_t))

//-----------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------

// Initialise the RTOS kernel
int             thread_kernel_init(void);

// Start the RTOS kernel
void            thread_kernel_run(void);

// Init thread (immediately run-able)
int             thread_init(struct thread *pThread, const char *name, int pri, void *(*f)(void *), void *arg, void *stack, uint32_t stack_size);

// Init thread with specified start state
int             thread_init_ex(struct thread *pThread, const char *name, int pri, void *(*f)(void *), void *arg, void *stack, uint32_t stack_size, tThreadState initial_state);

// Kill thread and remove from all thread lists.
// Once complete, thread data/stack will not be accessed again by RTOS.
// You cannot kill a thread from itself, use thread_suicide instead.
int             thread_kill(struct thread *pThread);

// Allows a thread to self terminate
void            thread_suicide(struct thread *pThread, void *exit_arg);

// Wait for thread to exit
void*           thread_join(struct thread *pThread);

// Sleep thread for x time units
void            thread_sleep(uint32_t time_units);

// Extended thread sleep API
void            thread_sleep_thread(struct thread *pSleepThread, uint32_t time_units);
void            thread_sleep_cancel(struct thread *pThread);

// Get current thread
struct thread*  thread_current(void);

// Kernel tick handler
void            thread_tick(void);

// Get the tick count for the RTOS
uint32_t        thread_tick_count(void);

// Block specified thread
void            thread_block(struct thread *pThread);

// Unblock specified thread
void            thread_unblock(struct thread *pThread);

// Unblock thread from running (called from ISR context)
void            thread_unblock_irq(struct thread *pThread);

// Dump thread list via specified printf
void            thread_dump_list(int (*os_printf)(const char* ctrl1, ... ));

// Calculate CPU load percentage
int             thread_get_cpu_load(void);

// Find highest priority run-able thread to run
void            thread_load_context(int preempt);

// Get list of all system threads
struct thread * thread_get_first_thread(void);

#endif

