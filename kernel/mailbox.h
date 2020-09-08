#ifndef __MAILBOX_H__
#define __MAILBOX_H__

#include "thread.h"
#include "list.h"
#include "semaphore.h"

//-----------------------------------------------------------------
// Defines
//-----------------------------------------------------------------
#define MAILBOX_INIT(e, s)    {(e), (s), 0, 0, 0, SEMAPHORE_INIT(0)}
#define MAILBOX_DECL(id, size) \
        static uint32_t stack_ ## id[size]; \
        static struct mailbox mbox_ ## id = MAILBOX_INIT(stack_ ## id, size)

//-----------------------------------------------------------------
// Types
//-----------------------------------------------------------------
struct mailbox
{
    // Storage array
    uint32_t           *entries;

    // Total size of storage
    int                 size;

    // Access Pointers
    int                 head;
    int                 tail;
    
    // Number of entries currently present
    int                 count;

    struct semaphore    sema;
};

//-----------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------

// Initialise mailbox
void    mailbox_init(struct mailbox *pMbox, uint32_t *storage, int size);

// Post message to mailbox
int     mailbox_post(struct mailbox *pMbox, uint32_t val);

// Wait for mailbox message
void    mailbox_pend(struct mailbox *pMbox, uint32_t *val);

// Wait for mailbox message (with timeout)
int     mailbox_pend_timed(struct mailbox *pMbox, uint32_t *val, int timeoutMs);

#endif
