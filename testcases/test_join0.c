#include "test.h"

//-----------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Locals:
//-----------------------------------------------------------------
THREAD_DECL(thread0, 1024);

//-----------------------------------------------------------------
// thread_func
//-----------------------------------------------------------------
static void* thread_func(void *arg)
{
    OS_ASSERT(arg == NULL);
    return (void *)1;
}
//-----------------------------------------------------------------
// Test Thread Function: (Max priority)
//-----------------------------------------------------------------
void testcase(void * a)
{
    void *res;

    THREAD_INIT(thread0, "thread0", thread_func, NULL, 0);

    // Higher priority thread spawns low priority thread
    // then deschedules waiting for completion.
    // Low prio thread then runs and exits returning a value.
    res = thread_join(&thread_thread0);
    OS_ASSERT(res == (void*)1);

    exit(0);
}
