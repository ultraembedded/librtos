#include "test.h"

//-----------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Locals:
//-----------------------------------------------------------------
THREAD_DECL(thread0, 1024);
THREAD_DECL(thread1, 1024);

static struct mutex _mtx0;
static volatile int _counter = 0;

//-----------------------------------------------------------------
// thread0_func
//-----------------------------------------------------------------
static void* thread0_func(void *arg)
{
    int last;

    mutex_lock(&_mtx0);

    OS_ASSERT(_counter == 0);
    
    while (_counter < 10)
    {
        last = _counter;
        thread_sleep(1);

        OS_ASSERT(last == _counter);

        _counter++;
    }

    mutex_unlock(&_mtx0);

    return NULL;
}
//-----------------------------------------------------------------
// thread1_func
//-----------------------------------------------------------------
static void* thread1_func(void *arg)
{
    int last;

    mutex_lock(&_mtx0);

    OS_ASSERT(_counter == 10);
    
    _counter++;

    mutex_unlock(&_mtx0);

    return NULL;
}
//-----------------------------------------------------------------
// Test Thread Function: (Max priority)
//-----------------------------------------------------------------
void testcase(void * a)
{
    // Non-recursive mutex
    mutex_init(&_mtx0, 0);

    // Low priority thead which pends on a semaphore
    THREAD_INIT(thread0, "thread0", thread0_func, NULL, 1);
    THREAD_INIT(thread1, "thread1", thread1_func, NULL, 0);

    // Block on the lowest priority thread
    thread_join(&thread_thread1);

    OS_ASSERT(_counter == 11);

    exit(0);
}
