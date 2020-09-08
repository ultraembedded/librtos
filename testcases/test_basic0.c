#include "test.h"

//-----------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Locals:
//-----------------------------------------------------------------
THREAD_DECL(thread0, 1024);
THREAD_DECL(thread1, 1024);
THREAD_DECL(thread2, 1024);
THREAD_DECL(thread3, 1024);

static volatile int flag0 = 0;
static volatile int flag1 = 0;
static volatile int flag2 = 0;
static volatile int flag3 = 0;

//-----------------------------------------------------------------
// thread_func
//-----------------------------------------------------------------
static void* thread_func(void *arg)
{
    int *flag = (int*) arg;
    printf("thread!\n");
    
    *flag = 1;

    // Busy wait, thread will be preempted on timeslice interval
    // and round robin interleaved with other threads at same prio
    while (1);

    return NULL;
}
//-----------------------------------------------------------------
// Test Thread Function:
//-----------------------------------------------------------------
void testcase(void * a)
{
    THREAD_INIT(thread0, "thread0", thread_func, &flag0, 0);
    THREAD_INIT(thread1, "thread1", thread_func, &flag1, 0);
    THREAD_INIT(thread2, "thread2", thread_func, &flag2, 0);
    THREAD_INIT(thread3, "thread3", thread_func, &flag3, 0);

    // Deschedule for long enough for other threads to complete
    thread_sleep(4);

    OS_ASSERT(flag0 == 1);
    OS_ASSERT(flag1 == 1);
    OS_ASSERT(flag2 == 1);
    OS_ASSERT(flag3 == 1);

    exit(0);
}
