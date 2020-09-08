#include "mutex.h"
#include "thread.h"
#include "critical.h"
#include "os_assert.h"

#ifdef INCLUDE_MUTEX
//-----------------------------------------------------------------
// mutex_init: Initialise mutex
//-----------------------------------------------------------------
void mutex_init(struct mutex *mtx, int recursive)
{
    OS_ASSERT(mtx != NULL);

    // No default owner
    mtx->owner = NULL;

    // No recursive depth count yet
    mtx->depth = 0;

    // Is this mutex recursive
    mtx->recursive = recursive;

    // Pending thread list
    list_init(&mtx->pend_list);
}
//-----------------------------------------------------------------
// mutex_lock: Acquire mutex (optionally recursive)
//-----------------------------------------------------------------
void mutex_lock(struct mutex *mtx)
{
    struct thread* this_thread;
    int cr;

    OS_ASSERT(mtx != NULL);

    cr = critical_start();

    // Get current (this) thread
    this_thread = thread_current();

    // Is the mutex not already locked
    if (mtx->owner == NULL)
    {
        // Acquire mutex for this thread
        mtx->owner = this_thread;

        OS_ASSERT(mtx->depth == 0);
    }
    // Is the mutex already locked by this thread
    else if (mtx->recursive && mtx->owner == this_thread)
    {
        // Increase recursive depth
        mtx->depth++;
    }    
    // The mutex is already 'owned', add thread to pending list
    else
    {
        struct link_node *listnode;

        OS_ASSERT(mtx->owner != this_thread);

        // Get list node
        listnode = &this_thread->blocking_node;

        // Add node to end of pending list
        list_insert_last(&mtx->pend_list, listnode);

        // Block the thread from running
        thread_block(this_thread);
    }

    critical_end(cr);
}
//-----------------------------------------------------------------
// mutex_trylock: Acquire mutex, return 1 if acquired, 0 if not
//-----------------------------------------------------------------
int mutex_trylock(struct mutex *mtx)
{
    struct thread* this_thread;
    int cr;
    int result = 0;

    OS_ASSERT(mtx != NULL);

    cr = critical_start();

    // Get current (this) thread
    this_thread = thread_current();

    // Is the mutex not already locked
    if (mtx->owner == NULL)
    {
        // Acquire mutex for this thread
        mtx->owner = this_thread;

        OS_ASSERT(mtx->depth == 0);
        result = 1;
    }
    // Is the mutex already locked by this thread
    else if (mtx->recursive && mtx->owner == this_thread)
    {
        // Increase recursive depth
        mtx->depth++;
        result = 1;
    }    
    // The mutex is already 'owned' by another thread, fail
    else
        result = 0;

    critical_end(cr);

    return result;
}
//-----------------------------------------------------------------
// mutex_unlock: Release mutex (optionally recursive)
//-----------------------------------------------------------------
void mutex_unlock(struct mutex *mtx)
{
    struct thread* this_thread;
    int cr;

    OS_ASSERT(mtx != NULL);

    cr = critical_start();

    // Get current (this) thread
    this_thread = thread_current();

    // We cannot release a mutex that we dont own!
    OS_ASSERT(this_thread == mtx->owner);

    // Reduce recursive depth count
    if (mtx->depth > 0)
        mtx->depth--;
    // If there are threads pending on this mutex
    else if (!list_is_empty(&mtx->pend_list))
    {
        // Unblock the first pending thread
        struct link_node *node = list_first(&mtx->pend_list);

        // Get the thread item
        struct thread* thread = list_entry(node, struct thread, blocking_node);

        // Remove node from linked list
        list_remove(&mtx->pend_list, node);

        // Transfer mutex ownership
        mtx->owner = thread;

        // Unblock the first waiting thread
        thread_unblock(thread);
    }
    // Else no-one wants it, no owner
    else
        mtx->owner = NULL;

    critical_end(cr);
}
#endif
