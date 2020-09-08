#include "test.h"

//-----------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Locals:
//-----------------------------------------------------------------
THREAD_DECL(thread0, 1024);
THREAD_DECL(thread1, 1024);

//-----------------------------------------------------------------
// thread_func
//-----------------------------------------------------------------
static void* thread_func(void *arg)
{
    return arg;
}
//-----------------------------------------------------------------
// Test Thread Function: (Max priority)
//-----------------------------------------------------------------
void testcase(void * a)
{
    void *res;

    THREAD_INIT(thread0, "thread0", thread_func, (void *)0, 0);
    THREAD_INIT(thread1, "thread1", thread_func, (void *)1, 0);

    // One thread waiting on multiple low priority threads
    // yielding so that the second will have already run and
    // exited, check checking the result
    res = thread_join(&thread_thread0);
    OS_ASSERT(res == (void*)0);

    thread_sleep(2);

    res = thread_join(&thread_thread1);
    OS_ASSERT(res == (void*)1);    

    exit(0);
}
