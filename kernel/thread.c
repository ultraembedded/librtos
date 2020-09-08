#include "thread.h"
#include "critical.h"
#include "os_assert.h"

//-----------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------
#ifdef PLATFORM_IDLE_TASK_STACK
    #define IDLE_TASK_STACK        PLATFORM_IDLE_TASK_STACK
#else
    #define IDLE_TASK_STACK        256
#endif

//-----------------------------------------------------------------
// Locals:
//-----------------------------------------------------------------
static struct link_list     _thread_runnable;
static struct link_list     _thread_blocked;
static struct link_list     _thread_sleeping;
static struct link_list     _thread_dead;
static struct thread*       _current_thread = NULL;
static struct thread        _idle_task;
static struct thread*       _thread_list_all = NULL;
static volatile uint32_t    _tick_count;
static volatile uint32_t    _thread_picks;
static uint32_t             _idle_task_stack[IDLE_TASK_STACK];
static int                  _thread_id;
static int                  _initd = 0;
static int                  _running;

//-----------------------------------------------------------------
// Prototypes:
//-----------------------------------------------------------------
static void *               thread_idle_task(void* arg);
static struct thread*       thread_pick(void);
static void                 thread_func(void *pThd);

static void                 thread_switch(void);
static void                 thread_insert_priority(struct link_list *pList, struct thread *pInsertNode);
static void                 thread_unblock_int(struct thread *pThread);

//-----------------------------------------------------------------
// thread_kernel_init: Initialise the RTOS kernel
//-----------------------------------------------------------------
int thread_kernel_init(void)
{
    OS_ASSERT(!_initd);

    // Disable interrupts
    critical_start();

    // Initialise thread lists
    list_init(&_thread_runnable);
    list_init(&_thread_sleeping);
    list_init(&_thread_blocked);
    list_init(&_thread_dead);

    _thread_list_all = NULL;
    _thread_id = 0;
    _tick_count = 0;
    _thread_picks = 0;
    _running = 0;

    // Create an idle task
    thread_init(&_idle_task, "IDLE_TASK", THREAD_IDLE_PRIO, thread_idle_task, (void*)NULL, (void*)_idle_task_stack, IDLE_TASK_STACK);

    _initd = 1;
    return 1;
}
//-----------------------------------------------------------------
// thread_kernel_run: Start the RTOS kernel
//-----------------------------------------------------------------
void thread_kernel_run(void)
{
    OS_ASSERT(_initd);
    OS_ASSERT(!_running);

    // Start with idle task so we then pick the best thread to
    // run rather than the first in the list
    _current_thread = &_idle_task;
    _running = TRUE;

    // Switch context to the highest priority thread
    cpu_thread_start();
}
//-----------------------------------------------------------------
// thread_init_ex: Init thread with specified start state
//-----------------------------------------------------------------
int thread_init_ex(struct thread *pThread, const char *name, int pri, void *(*f)(void *), void *arg, void *stack, uint32_t stack_size, tThreadState initial_state)
{
    int cr;
    int l = 0;

    OS_ASSERT(pThread != NULL);

    // Thread name
    if (!name)
        name = "NO_NAME";

    while (l < THREAD_NAME_LEN && name[l])
    {
        pThread->name[l] = name[l];
        l++;
    }

    pThread->name[THREAD_NAME_LEN-1] = 0;

    // Setup priority
    pThread->priority = pri;

    // Thread function
    pThread->thread_func = f;
    pThread->thread_arg = arg;

    pThread->state = initial_state;
    pThread->run_count = 0;
    pThread->exit_value = NULL;

#ifdef CONFIG_RTOS_ABSOLUTE_TIME
    pThread->wakeup_time = 0;
#else
    pThread->wait_delta = 0;
#endif

#ifdef CONFIG_RTOS_MEASURE_THREAD_TIME
    pThread->run_time = 0;
    pThread->run_start = 0;
#endif

    // Join list init
    list_init(&pThread->join_list);

    // Task control block
    cpu_thread_init_tcb(&pThread->tcb, thread_func, pThread, stack, stack_size);

    // Begin critical section
    cr = critical_start();

    pThread->thread_id = ++_thread_id;

    // Runable: Insert this thread at the end of run list
    if (initial_state == THREAD_RUNABLE)
        thread_insert_priority(&_thread_runnable, pThread);
    else if (initial_state == THREAD_BLOCKED)
        list_insert_last(&_thread_blocked, &pThread->node);
    else
    {
        OS_ASSERT(initial_state != THREAD_SLEEPING);
    }

    // Add to simple all threads list
    pThread->next_all = _thread_list_all;
    _thread_list_all = pThread;

    // Set the checkword
    pThread->checkword = THREAD_CHECK_WORD;

    critical_end(cr);

    return 1;
}
//-----------------------------------------------------------------
// thread_init: Init thread (runnable now)
//-----------------------------------------------------------------
int thread_init(struct thread *pThread, const char *name, int pri, void *(*f)(void *), void *arg, void *stack, uint32_t stack_size)
{
    return thread_init_ex(pThread, name, pri, f, arg, stack, stack_size, THREAD_RUNABLE);
}
//-----------------------------------------------------------------
// thread_kill: Kill thread and remove from all thread lists.
// Once complete, thread data/stack will not be accessed again by
// RTOS.
// You cannot kill a thread from itself, use thread_suicide instead.
// Returns: 1 = ok, 0 = failed
//-----------------------------------------------------------------
int thread_kill(struct thread *pThread)
{
    int ok = 0;
    struct thread *pCurr = NULL;
    struct thread *pLast = NULL;
    int cr = critical_start();

    // Thread cannot kill itself using thread_kill
    if (pThread != _current_thread)
    {
        // Thread currently runable: remove from run list
        if (pThread->state == THREAD_RUNABLE)
            list_remove(&_thread_runnable, &pThread->node);
        // Blocked: remove from blocked list
        else if (pThread->state == THREAD_BLOCKED)
            list_remove(&_thread_blocked, &pThread->node);
        // Sleeping: remove from sleep list
        else if (pThread->state == THREAD_SLEEPING)
        {
#ifndef CONFIG_RTOS_ABSOLUTE_TIME
            // Is there another thread after this in the delta timer list?
            struct link_node *node = list_next(&_thread_sleeping, &pThread->node);
            if (node)
            {
                struct thread *pNextThread = list_entry(node, struct thread, node);

                // Add current item's remaining time to next
                pNextThread->wait_delta += pThread->wait_delta;
            }
#endif
            // Remove from sleeping list
            list_remove(&_thread_sleeping, &pThread->node);
        }
        // Dead: Remove from dead list
        else if (pThread->state == THREAD_DEAD)
            list_remove(&_thread_dead, &pThread->node);
        else
            OS_PANIC("Unknown thread state!");

        // Remove from simple 'all threads' list
        pCurr = _thread_list_all;
        while (pCurr != NULL)
        {
            if (pCurr == pThread)
            {
                if (pLast != NULL)
                    pLast->next_all = pCurr->next_all;
                else
                    _thread_list_all = pCurr->next_all;
                break;
            }
            else
                pCurr = pCurr->next_all;

            pLast = pCurr;
        }

        ok = 1;
    }

    critical_end(cr);

    return ok;
}
//-----------------------------------------------------------------
// thread_suicide: Allows a thread to self terminate.
//-----------------------------------------------------------------
void thread_suicide(struct thread *pThread, void *exit_arg)
{
    int cr = critical_start();

    OS_ASSERT(pThread == _current_thread);
    OS_ASSERT(pThread->state == THREAD_RUNABLE);

    // Remove from the run list
    list_remove(&_thread_runnable, &pThread->node);
    
    // Mark thread as dead and add to dead thread list
    pThread->state = THREAD_DEAD;
    list_insert_last(&_thread_dead, &pThread->node);

    // Record optional exit arg for later use
    pThread->exit_value = exit_arg;

    // If there are threads pending on this thread exiting
    while (!list_is_empty(&pThread->join_list))
    {
        // Unblock the first pending thread
        struct link_node *node = list_first(&pThread->join_list);

        // Get the thread item
        struct thread* thread = list_entry(node, struct thread, blocking_node);

        // Remove node from linked list
        list_remove(&pThread->join_list, node);

        // Unblock the waiting thread
        thread_unblock_int(thread);
    }

    // Switch context to the new highest priority thread
    cpu_context_switch();

    critical_end(cr);
}
//-----------------------------------------------------------------
// thread_join: Wait for thread to exit
//-----------------------------------------------------------------
void* thread_join(struct thread *pThread)
{
    void *res;
    int cr;

    OS_ASSERT(pThread);

    // Can't wait for ourselves to exit!
    OS_ASSERT(pThread != _current_thread);

    cr = critical_start();

    // If thread alive
    if (pThread->state != THREAD_DEAD)
    {
        // Setup list node
        struct link_node * listnode = &_current_thread->blocking_node;

        // Add node to end of pending join list
        list_insert_last(&pThread->join_list, listnode);

        // Block the thread from running
        thread_block(_current_thread);
    }
    
    res = pThread->exit_value;

    critical_end(cr);

    return res;
}
//-----------------------------------------------------------------
// thread_sleep_thread: Put a specific thread on to the sleep queue
//-----------------------------------------------------------------
void thread_sleep_thread(struct thread *pSleepThread, uint32_t time_units)
{
    int cr = critical_start();
#ifndef CONFIG_RTOS_ABSOLUTE_TIME
    uint32_t total = 0;
    uint32_t prevtotal = 0;
#endif
    struct link_node *node;

    OS_ASSERT(pSleepThread);

    // Is the thread currently runable?
    if (pSleepThread->state == THREAD_RUNABLE)
    {
        // Remove from the run list
        list_remove(&_thread_runnable, &pSleepThread->node);
    }
    // or is it blocked
    else if (pSleepThread->state == THREAD_BLOCKED)
    {
        // Remove from the blocked list
        list_remove(&_thread_blocked, &pSleepThread->node);
    }
    else
        OS_PANIC("Thread already sleeping!");

    // Mark thread as sleeping
    pSleepThread->state = THREAD_SLEEPING;

#ifdef CONFIG_RTOS_ABSOLUTE_TIME
    pSleepThread->wakeup_time = cpu_timenow();
    pSleepThread->wakeup_time+= time_units;
#else
    // NOTE: Add 1 to the sleep time to get at least the time slept for.
    time_units = time_units + 1;
#endif

    // Get the first sleeping thread
    node = list_first(&_thread_sleeping);

    // Current sleep list is empty?
    if (node == NULL)
    {
#ifndef CONFIG_RTOS_ABSOLUTE_TIME
        // delta is total timeout
        pSleepThread->wait_delta = time_units;
#endif
        // Add to the start of the sleep list
        list_insert_first(&_thread_sleeping, &pSleepThread->node);
    }
    // Timer list has items
    else
    {
        // Iterate through current list and add at correct location
        for ( ; node ; node = list_next(&_thread_sleeping, node))
        {
            // Get the thread item
            struct thread * pThread = list_entry(node, struct thread, node);

#ifndef CONFIG_RTOS_ABSOLUTE_TIME
            // Increment cumulative total
            total += pThread->wait_delta;
#endif

            // New timeout less than total (or end of list reached)
#ifdef CONFIG_RTOS_ABSOLUTE_TIME
            if (pSleepThread->wakeup_time <= pThread->wakeup_time)
#else
            if (time_units <= total)
#endif
            {
#ifndef CONFIG_RTOS_ABSOLUTE_TIME
                // delta time from previous to this node
                pSleepThread->wait_delta = time_units - prevtotal;
#endif

                // Insert into list before this sleeping thread
                list_insert_before(&_thread_sleeping, &pThread->node, &pSleepThread->node);

#ifndef CONFIG_RTOS_ABSOLUTE_TIME
                // Adjust next nodes delta time
                pThread->wait_delta -= pSleepThread->wait_delta;
#endif
                break;
            }

#ifndef CONFIG_RTOS_ABSOLUTE_TIME
            prevtotal = total;
#endif

            // End of list reached, still not added
            if (list_next(&_thread_sleeping, node) == NULL)
            {
#ifndef CONFIG_RTOS_ABSOLUTE_TIME
                // delta time from previous to this node
                pSleepThread->wait_delta = time_units - prevtotal;
#endif

                // Insert into list after last node (end of list)
                list_insert_last(&_thread_sleeping, &pSleepThread->node);
                break;
            }
        }
    }

    critical_end(cr);
}
//-----------------------------------------------------------------
// thread_sleep_cancel: Stop a thread from sleeping
//-----------------------------------------------------------------
void thread_sleep_cancel(struct thread *pThread)
{
    int cr = critical_start();

    OS_ASSERT(pThread);

    // If the item has not already expired (and is in the sleeping list)
    if (pThread->state == THREAD_SLEEPING)
    {
#ifndef CONFIG_RTOS_ABSOLUTE_TIME
        // Is there another thread after this in the delta list?
        struct link_node *node = list_next(&_thread_sleeping, &pThread->node);
        if (node)
        {
            struct thread *pNextThread = list_entry(node, struct thread, node);

            // Add current item's remaining time to next
            pNextThread->wait_delta += pThread->wait_delta;
        }

        // Clear the sleep timer
        pThread->wait_delta = 0;
#endif

        // Remove from the sleeping list
        list_remove(&_thread_sleeping, &pThread->node);

        // Until this thread is put back in the run list or
        // is re-added to the sleep list then mark as blocked.
        pThread->state = THREAD_BLOCKED;
        list_insert_last(&_thread_blocked, &pThread->node);
    }
    // Else thread timeout has expired and is now runable (or blocked)

    critical_end(cr);
}
//-----------------------------------------------------------------
// thread_sleep: Sleep thread for x time units
//-----------------------------------------------------------------
void thread_sleep(uint32_t time_units)
{
    int cr = critical_start();

    // Put the current thread to sleep
    if (time_units > 0)
        thread_sleep_thread(_current_thread, time_units);

    // Switch context to the next highest priority thread
    thread_switch();

    critical_end(cr);
}
//-----------------------------------------------------------------
// thread_switch: Switch context to the highest priority thread
//-----------------------------------------------------------------
static void thread_switch(void)
{
    // Get the current run count
    uint32_t oldRuncount = _current_thread->run_count;

    // Cause context switch
    cpu_context_switch();

    // In-order to get back to this point, we must have been
    // picked by thread_pick() and the run-count incremented.
    OS_ASSERT(oldRuncount != (_current_thread->run_count));

    // This thread must be in the run list otherwise something has gone wrong!
    OS_ASSERT(_current_thread->state == THREAD_RUNABLE);
}
//-----------------------------------------------------------------
// thread_load_context: Find highest priority run-able thread to run
//-----------------------------------------------------------------
CRITICALFUNC void thread_load_context(int preempt)
{
    struct thread * pThread;

    // If non pre-emptive scheduler, don't change threads for pre-emption.
    // (Don't change context until the current thread is non-runnable)
#ifdef CONFIG_RTOS_COOPERATIVE_SCHEDULING
    if (preempt && _current_thread->state == THREAD_RUNABLE)
        return;
#endif

#ifdef CONFIG_RTOS_MEASURE_THREAD_TIME
    // How long was this thread scheduled for?
    if (_current_thread->run_start != 0)
        _current_thread->run_time += cpu_timediff(cpu_timenow(), _current_thread->run_start);
#endif

    // Now pick the highest thread that can be run and restore it's context.
    pThread = thread_pick();

#ifdef CONFIG_RTOS_MEASURE_THREAD_TIME
    // Take a snapshot of the system clock when thread selected
    pThread->run_start = cpu_timenow();

    // Time=0 has a special meaning!
    if (pThread->run_start == 0)
        pThread->run_start = 1;
#endif

    // Load new thread's context
    _current_thread = pThread;
}
//-----------------------------------------------------------------
// thread_current: Get the current thread that is active!
//-----------------------------------------------------------------
struct thread* thread_current()
{
    return _current_thread;
}
//-----------------------------------------------------------------
// thread_pick: Pick the highest priority runable thread
// NOTE: Must be called within critical protection region
//-----------------------------------------------------------------
static CRITICALFUNC struct thread* thread_pick(void)
{
    struct thread *pThread;
    struct link_node *node;

    // If we have a current running task and if the current thread
    // is still run-able, put it in the correct position in the list.
    if (_current_thread && _current_thread->state == THREAD_RUNABLE)
    {
        // Remove it from the run list
        list_remove(&_thread_runnable, &_current_thread->node);
        // and re-insert at appropriate position in run list
        // based on thread priority.
        // This will be after all the other threads at the same
        // priority level, hence allowing round robin execution
        // of other threads with the same priority level.
        thread_insert_priority(&_thread_runnable, _current_thread);
    }

    // Get the first runable thread
    node = list_first(&_thread_runnable);
    OS_ASSERT(node != NULL);

    pThread = list_entry(node, struct thread, node);

    // We should have found a task to run as long as there is at least one
    // task on the run list (there should be as this is why we have the idle
    // task!).
    OS_ASSERT(pThread != NULL);
    OS_ASSERT(pThread->checkword == THREAD_CHECK_WORD);
    OS_ASSERT(pThread->state == THREAD_RUNABLE);

    pThread->run_count++;

    // Total thread context switches / timer ticks have occurred
    _thread_picks++;

    return pThread;
}
//-----------------------------------------------------------------
// thread_tick: Kernel tick handler
// NOTE: Must be called within critical protection region (or INT)
//-----------------------------------------------------------------
CRITICALFUNC void thread_tick(void)
{
    struct thread *pThread = NULL;
    struct link_node *node;
#ifdef CONFIG_RTOS_ABSOLUTE_TIME
    uint64_t current_time = cpu_timenow();
#endif

    // Get the first sleeping thread
    node = list_first(&_thread_sleeping);
    pThread = list_entry(node, struct thread, node);

#ifndef CONFIG_RTOS_ABSOLUTE_TIME
    // Decrement a tick from the first item in the list
    if (pThread && pThread->wait_delta)
        pThread->wait_delta--;
#endif

    // Iterate through list of sleeping threads
    while (pThread != NULL)
    {
        OS_ASSERT(pThread->checkword == THREAD_CHECK_WORD);
        OS_ASSERT(pThread->state == THREAD_SLEEPING);

        // Has this item timed out?
#ifdef CONFIG_RTOS_ABSOLUTE_TIME
        if (current_time >= pThread->wakeup_time)
#else
        if (pThread->wait_delta == 0)
#endif
        {
            // Remove from the sleep list
            list_remove(&_thread_sleeping, &pThread->node);

            // Add to the run list and mark runable
            pThread->state = THREAD_RUNABLE;
            thread_insert_priority(&_thread_runnable, pThread);

            // Get next node (new first node)
            node = list_first(&_thread_sleeping);
            pThread = list_entry(node, struct thread, node);
        }
        // Non-zero timeout remaining, end of timed out items
        else
            break;
    }

    // Thats all, thread_load_context() will do the pick
    // of the highest priority runable task...

    _tick_count++;
}
//-----------------------------------------------------------------
// thread_tick_count: Get the tick count for the RTOS
//-----------------------------------------------------------------
uint32_t thread_tick_count(void)
{
    return _tick_count;
}
//-----------------------------------------------------------------
// thread_func: Wrapper for thread entry point
//-----------------------------------------------------------------
static void thread_func(void *pThd)
{
    struct thread *pThread = (struct thread *)pThd;
    void *ret_arg;

    OS_ASSERT(pThread);
    OS_ASSERT(pThread->checkword == THREAD_CHECK_WORD);

    // Execute thread function
    ret_arg = pThread->thread_func(pThread->thread_arg);

    // Now thread has exited, call thread_suicide!
    thread_suicide(pThread, ret_arg);
    
    // We should never reach here!
    OS_PANIC("Should not be here");
}
//-----------------------------------------------------------------
// thread_block: Block specified thread from executing
// WARNING: Call within critical section!
//-----------------------------------------------------------------
void thread_block(struct thread *pThread)
{
    OS_ASSERT(pThread->checkword == THREAD_CHECK_WORD);
    OS_ASSERT(pThread->state == THREAD_RUNABLE);

    // Mark thread as blocked
    pThread->state = THREAD_BLOCKED;

    // Remove from the run list
    list_remove(&_thread_runnable, &pThread->node);

    // Add to the blocked list
    list_insert_last(&_thread_blocked, &pThread->node);

    // Switch context to the new highest priority thread
    thread_switch();
}
//-----------------------------------------------------------------
// thread_unblock_int: Unblock specified thread. Internal function.
// Manipulates the ready/blocked/sleeping lists only.
// WARNING: Call within critical section!
//-----------------------------------------------------------------
static void thread_unblock_int(struct thread *pThread)
{
    // WARNING: There is no gaurantee that unblock is not called
    // on a thread that is already runable.
    // This is due to a timed semaphore timing out, being made
    // runable again but that thread not being scheduled prior
    // to the post operation which will unblock it...

    // Is thread sleeping (i.e doing a timed pend using thread_sleep)?
    if (pThread->state == THREAD_SLEEPING)
    {
#ifndef CONFIG_RTOS_ABSOLUTE_TIME
        // Is there another thread after this in the delta sleeping list?
        struct link_node *node = list_next(&_thread_sleeping, &pThread->node);
        if (node)
        {
            struct thread *pNextThread = list_entry(node, struct thread, node);

            // Add current item's remaining time to next
            pNextThread->wait_delta += pThread->wait_delta;
        }

        // Clear the sleep timer
        pThread->wait_delta = 0;
#endif
        // Remove from the sleeping list
        list_remove(&_thread_sleeping, &pThread->node);
    }
    // Is the thread in the blocked list
    else if (pThread->state == THREAD_BLOCKED)
    {
        // Remove from the blocked list
        list_remove(&_thread_blocked, &pThread->node);
    }
    // Already in the run list, exit!
    else if (pThread->state == THREAD_RUNABLE)
        return ;

    // Mark thread as run-able
    pThread->state = THREAD_RUNABLE;

    // Add to the run list
    thread_insert_priority(&_thread_runnable, pThread);
}
//-----------------------------------------------------------------
// thread_unblock: Unblock specified thread / enable execution
// WARNING: Call within critical section!
//-----------------------------------------------------------------
void thread_unblock(struct thread *pThread)
{
    OS_ASSERT(pThread->checkword == THREAD_CHECK_WORD);

    // Make sure thread is now in the run list
    thread_unblock_int(pThread);

    // If un-blocked thread is higher priority than this thread
    // then switch context to the new highest priority thread
    if (pThread->priority > _current_thread->priority)
        thread_switch();
}
//-----------------------------------------------------------------
// thread_unblock_irq: Unblock thread from IRQ
//-----------------------------------------------------------------
void thread_unblock_irq(struct thread *pThread)
{
    // Critical section should not be required, but for sanity
    int cr = critical_start();

    OS_ASSERT(pThread->checkword == THREAD_CHECK_WORD);

    // Make sure thread is now in the run list
    thread_unblock_int(pThread);

    // Schedule a context switch to occur after IRQ completes
    cpu_context_switch_irq();

    critical_end(cr);
}
//-----------------------------------------------------------------
// thread_insert_priority: Insert thread into list in priority order
//-----------------------------------------------------------------
static CRITICALFUNC void thread_insert_priority(struct link_list *pList, struct thread *pInsertNode)
{
    struct link_node *node;

    OS_ASSERT(pList != NULL);
    OS_ASSERT(pInsertNode != NULL);

    // No items in the queue, insert at the head
    if (list_is_empty(pList))
        list_insert_first(pList, &pInsertNode->node);
    else
    {
        // Iterate through list and add in order of priority
        // NOTE: Duplicates will be added to end of list of duplicate
        // threads priorities.
        list_for_each(pList, node)
        {
            // Get the thread item
            struct thread* thread = list_entry(node, struct thread, node);

            // Is threads priority lower than this thread?
            if (pInsertNode->priority > thread->priority)
            {
                // Insert before this node
                list_insert_before(pList, node, &pInsertNode->node);
                break;
            }

            // End of the list reached and node not inserted yet
            if (list_next(pList, node) == NULL)
            {
                // Insert after current last node
                list_insert_after(pList, node, &pInsertNode->node);
                break;
            }
        }
    }
}
//-----------------------------------------------------------------
// thread_idle_task: Idle task function
//-----------------------------------------------------------------
static void *thread_idle_task(void* arg)
{
    while (1)
        cpu_idle();

    return NULL;
}
//-----------------------------------------------------------------
// thread_print_thread: Print thread details to OS_PRINTF
//-----------------------------------------------------------------
static void thread_print_thread(int idx, struct thread *pThread, uint32_t sleepTime, int (*os_printf)(const char* ctrl1, ... ))
{
    char stateChar;

    os_printf("%d:\t", idx+1);
    os_printf("|%10.10s|\t", pThread->name);
    os_printf("%d\t", pThread->priority);

    if (pThread == _current_thread)
        stateChar = '*';
    else
    {
        switch (pThread->state)
        {
            case THREAD_RUNABLE:
                stateChar = 'R';
            break;
            case THREAD_SLEEPING:
                stateChar = 'S';
            break;
            case THREAD_BLOCKED:
                stateChar = 'B';
            break;
            case THREAD_DEAD:
                stateChar = 'X';
            break;
            default:
                stateChar = 'U';
            break;
        }
    }

    os_printf("%c\t", stateChar);
    os_printf("%ld\t", sleepTime);
    os_printf("%ld\t", pThread->run_count);
    os_printf("%ld\r\n", cpu_thread_stack_free(&pThread->tcb));
}
//-----------------------------------------------------------------
// thread_dump_list: Dump thread list via specified printf
//-----------------------------------------------------------------
void thread_dump_list(int (*os_printf)(const char* ctrl1, ... ))
{
    struct thread      *pThread;
    struct link_node  *node;
#ifdef CONFIG_RTOS_ABSOLUTE_TIME
    uint64_t current_time = cpu_timenow();
#else
    uint32_t sleepTimeTotal = 0;
#endif
    int idx = 0;

    int cr = critical_start();

    os_printf("Thread Dump:\r\n");
    os_printf("Num     Name        Pri    State    Sleep    Runs    Free Stack\r\n");

    // Print all runable threads
    pThread = _thread_list_all;
    while (pThread != NULL)
    {
        if (pThread->state == THREAD_RUNABLE)
            thread_print_thread(idx++, pThread, 0, os_printf);
        pThread = pThread->next_all;
    }

    // Print sleeping threads
    node = list_first(&_thread_sleeping);
    while (node != NULL)
    {
        pThread = list_entry(node, struct thread, node);

#ifdef CONFIG_RTOS_ABSOLUTE_TIME
        thread_print_thread(idx++, pThread, pThread->wakeup_time - current_time, os_printf);
#else
        sleepTimeTotal += pThread->wait_delta;
        thread_print_thread(idx++, pThread, sleepTimeTotal, os_printf);
#endif

        node = list_next(&_thread_sleeping, node);
    }

    // Print blocked threads
    pThread = _thread_list_all;
    while (pThread != NULL)
    {
        if (pThread->state == THREAD_BLOCKED)
            thread_print_thread(idx++, pThread, 0, os_printf);
        pThread = pThread->next_all;
    }

    // Print dead threads
    pThread = _thread_list_all;
    while (pThread != NULL)
    {
        if (pThread->state == THREAD_DEAD)
            thread_print_thread(idx++, pThread, 0, os_printf);
        pThread = pThread->next_all;
    }

    critical_end(cr);
}
//-----------------------------------------------------------------
// thread_get_cpu_load: Calculate CPU load percentage.
// Higher = heavier system task load.
// Requires CONFIG_RTOS_MEASURE_THREAD_TIME to be defined along with 
// appropriate system clock/time measurement functions.
//-----------------------------------------------------------------
#ifdef CONFIG_RTOS_MEASURE_THREAD_TIME
int thread_get_cpu_load(void)
{
    struct thread      *pThread;
    uint32_t idle_time = 0;
    uint32_t total_time = 0;

    int cr = critical_start();

    // Walk the thread list and calculate sum of total time spent in all threads 
    pThread = _thread_list_all;
    while (pThread != NULL)
    {
        // Idle task
        if (pThread == &_idle_task)
        {
            OS_ASSERT( idle_time == 0 );
            idle_time = pThread->run_time;
            total_time += pThread->run_time;
        }
        // Other tasks
        else
            total_time += pThread->run_time;

        // Clear time once read
        pThread->run_time = 0;

        // Next thread in the list
        pThread = pThread->next_all;
    }

    critical_end(cr);

    // Scale if large numbers
    if (idle_time > (1 << 24))
    {
        idle_time >>= 8;
        total_time >>= 8;
    }

    if (total_time)
        return 100 - ((idle_time * 100) / total_time);
    else
        return 0;
}
#endif
//-----------------------------------------------------------------
// thread_get_first_thread:
//-----------------------------------------------------------------
struct thread *thread_get_first_thread(void)
{
    return _thread_list_all;
}
