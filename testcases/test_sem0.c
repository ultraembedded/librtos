#include "test.h"

//-----------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Locals:
//-----------------------------------------------------------------
THREAD_DECL(thread0, 1024);

static struct semaphore _sema0;
static volatile int _flag0 = 0;

//-----------------------------------------------------------------
// thread_func
//-----------------------------------------------------------------
static void* thread_func(void *arg)
{
    struct semaphore *sem = (struct semaphore *)arg;

    semaphore_post(sem);

    _flag0 = 1;

    return NULL;
}
//-----------------------------------------------------------------
// Test Thread Function: (Max priority)
//-----------------------------------------------------------------
void testcase(void * a)
{
    uint32_t t0;
    uint32_t t1;
    int res;

    semaphore_init(&_sema0, 0);

    // Low priority thead which kicks a semaphore
    THREAD_INIT(thread0, "thread0", thread_func, &_sema0, 0);

    OS_ASSERT(semaphore_get_value(&_sema0) == 0);
    OS_ASSERT(semaphore_try(&_sema0) == 0);

    // Block waiting for semaphore which will enable
    // low prio thread to run
    semaphore_pend(&_sema0);

    OS_ASSERT(semaphore_get_value(&_sema0) == 0);
    OS_ASSERT(semaphore_try(&_sema0) == 0);

    // Thread should have relinshed control during semaphore post
    // to this high prio thread
    OS_ASSERT(_flag0 == 0);

    // Block waiting for thread to exit
    thread_join(&thread_thread0);

    OS_ASSERT(_flag0 == 1);

    t0 = thread_tick_count();

    // Timed wait on the semaphore (should fail)
    res = semaphore_timed_pend(&_sema0, 5);
    OS_ASSERT(res == 0);

    t1 = thread_tick_count();

    OS_ASSERT(((int32_t)(t1 - t0)) >= 5);

    t0 = thread_tick_count();

    semaphore_post(&_sema0);

    // Timed wait on the semaphore (should complete ok)
    res = semaphore_timed_pend(&_sema0, 10);
    OS_ASSERT(res == 1);

    t1 = thread_tick_count();

    OS_ASSERT(((int32_t)(t1 - t0)) <= 1);    

    exit(0);
}
