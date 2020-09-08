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

static struct semaphore _sema0;

//-----------------------------------------------------------------
// thread_func
//-----------------------------------------------------------------
static void* thread_func(void *arg)
{
    int *flag = (int*) arg;

    semaphore_pend(&_sema0);
    
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
    volatile int flag2 = 0;

    semaphore_init(&_sema0, 2);

    // Low priority thead which pends on a semaphore
    THREAD_INIT(thread0, "thread0", thread_func, (void*)&flag0, 1);
    THREAD_INIT(thread1, "thread1", thread_func, (void*)&flag1, 1);
    THREAD_INIT(thread2, "thread2", thread_func, (void*)&flag2, 0);

    OS_ASSERT(semaphore_get_value(&_sema0) == 2);

    // The low priority threads will not have run yet
    OS_ASSERT(flag0 == 0);
    OS_ASSERT(flag1 == 0);
    OS_ASSERT(flag2 == 0);

    thread_sleep(4);

    // The two higher priority threads should grab the semaphores
    // but the low priority thread will lose out...
    OS_ASSERT(flag0 == 1);
    OS_ASSERT(flag1 == 1);
    OS_ASSERT(flag2 == 0);

    // Kick the semaphore one more time to give it to the last thread
    semaphore_post(&_sema0);

    thread_sleep(1);

    OS_ASSERT(flag0 == 1);
    OS_ASSERT(flag1 == 1);
    OS_ASSERT(flag2 == 1);

    exit(0);
}
