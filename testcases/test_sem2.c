#include "test.h"

//-----------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Locals:
//-----------------------------------------------------------------
THREAD_DECL(thread0, 1024);
THREAD_DECL(thread1, 1024);

static struct semaphore _sema0;

//-----------------------------------------------------------------
// thread0_func
//-----------------------------------------------------------------
static void* thread0_func(void *arg)
{
    int *flag = (int*) arg;
    int res;

    res = semaphore_timed_pend(&_sema0, 5);
    OS_ASSERT(res == 1);
    
    *flag = 1;

    return NULL;
}
//-----------------------------------------------------------------
// thread1_func
//-----------------------------------------------------------------
static void* thread1_func(void *arg)
{
    int *flag = (int*) arg;

    // Wait for less time than thread0 is pending on the semaphore
    thread_sleep(3);

    // Kick semaphore, which should cause thread 0 to be run
    semaphore_post(&_sema0);
    
    *flag = 1;

    return NULL;
}
//-----------------------------------------------------------------
// Test Thread Function: (Max priority)
//-----------------------------------------------------------------
void testcase(void * a)
{
    volatile int flag0 = 0;
    volatile int flag1 = 0;

    semaphore_init(&_sema0, 0);

    // Low priority thead which pends on a semaphore
    THREAD_INIT(thread0, "thread0", thread0_func, (void*)&flag0, 0);
    THREAD_INIT(thread1, "thread1", thread1_func, (void*)&flag1, 1);

    // The low priority threads will not have run yet
    OS_ASSERT(flag0 == 0);
    OS_ASSERT(flag1 == 0);

    // Block on the lowest priority thread
    thread_join(&thread_thread0);

    // Both threads should have run now
    OS_ASSERT(flag0 == 1);
    OS_ASSERT(flag1 == 1);

    exit(0);
}
