#include "test.h"

//-----------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Locals:
//-----------------------------------------------------------------
THREAD_DECL(thread0, 8192);
THREAD_DECL(thread1, 8192);

static volatile int _flag = 0;

//-----------------------------------------------------------------
// thread_func
//-----------------------------------------------------------------
static void* thread_func(void *arg)
{
    if (arg == (void*)0)
        thread_sleep(5);
    else
    {
        _flag = 1;
        thread_sleep(10);
        _flag = 2;
    }

    return NULL;
}
//-----------------------------------------------------------------
// Test Thread Function:
//-----------------------------------------------------------------
void testcase(void * a)
{
    THREAD_INIT(thread0, "thread0", thread_func, 0, 0);
    THREAD_INIT(thread1, "thread1", thread_func, 1, 0);

    thread_sleep(1);
    OS_ASSERT(_flag == 1);
    thread_kill(&thread_thread0);

    thread_sleep(7);
    OS_ASSERT(_flag == 1);

    thread_sleep(4);
    OS_ASSERT(_flag == 2);

    exit(0);
}
