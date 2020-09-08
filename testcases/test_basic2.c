#include "test.h"

//-----------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Locals:
//-----------------------------------------------------------------
THREAD_DECL(thread0, 1024);
THREAD_DECL(thread1, 1024);

static volatile int _counter = 0;

//-----------------------------------------------------------------
// thread_func
//-----------------------------------------------------------------
static void* thread_func(void *arg)
{    
    int last;

    // Perform this operation in a critical section so that the thread
    // switch points are under our control...
    int cr = critical_start();

    while (1)
    {
        _counter++;
        last = _counter;

        // Yield to another thread on the run list at the same priority
        thread_sleep(0);

        if (_counter > 100)
            break;

        OS_ASSERT(last != _counter);        
    }

    critical_end(cr);    

    return NULL;
}
//-----------------------------------------------------------------
// Test Thread Function:
//-----------------------------------------------------------------
void testcase(void * a)
{
    THREAD_INIT(thread0, "thread0", thread_func, NULL, 0);
    THREAD_INIT(thread1, "thread1", thread_func, NULL, 0);

    // Check for round robin scheduling for threads on the same priority
    thread_sleep(6);

    exit(0);
}
