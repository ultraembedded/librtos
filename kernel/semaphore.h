#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include "thread.h"
#include "list.h"

//-----------------------------------------------------------------
// Defines
//-----------------------------------------------------------------
#define SEMAPHORE_INIT(c)   {(c), LIST_INIT}
#define SEMAPHORE_DECL(id, c) \
        static struct semaphore sema_ ## id = SEMAPHORE_INIT(c)

//-----------------------------------------------------------------
// Types
//-----------------------------------------------------------------
struct semaphore
{
    uint32_t            count;
    struct link_list    pend_list;
};

//-----------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------

// Initialise semaphore with an initial value
void    semaphore_init(struct semaphore *pSem, uint32_t initial_count);

// Increment semaphore count
void    semaphore_post(struct semaphore *pSem);

// Increment semaphore (interrupt context safe)
void    semaphore_post_irq(struct semaphore *pSem);

// Decrement semaphore or block if already 0
void    semaphore_pend(struct semaphore *pSem);

// Attempt to decrement semaphore or return 0
int     semaphore_try(struct semaphore *pSem);

// Decrement semaphore (with timeout)
int     semaphore_timed_pend(struct semaphore *pSem, int timeoutMs);

// Get semaphore value
uint32_t semaphore_get_value(struct semaphore *pSem);

#endif

