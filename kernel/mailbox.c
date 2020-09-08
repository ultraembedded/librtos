#include "mailbox.h"
#include "critical.h"
#include "os_assert.h"

#ifdef INCLUDE_MAILBOX
//-----------------------------------------------------------------
// mailbox_init: Initialise mailbox
//-----------------------------------------------------------------
void mailbox_init(struct mailbox *pMbox, uint32_t *storage, int size)
{
    int i;

    OS_ASSERT(pMbox != NULL);

    pMbox->entries = storage;

    for (i=0;i<size;i++)
        pMbox->entries[i] = NULL;

    pMbox->head  = 0;
    pMbox->tail  = 0;
    pMbox->count = 0;
    pMbox->size  = size;

    // Initialise mailbox item ready semaphore
    semaphore_init(&pMbox->sema, 0);
}
//-----------------------------------------------------------------
// mailbox_post: Post message to mailbox
//-----------------------------------------------------------------
int mailbox_post(struct mailbox *pMbox, uint32_t val)
{
    int cr;
    int res = 0;

    OS_ASSERT(pMbox != NULL);

    cr = critical_start();

    // Mailbox has free space?
    if (pMbox->count < pMbox->size)
    {
        // Add pointer to mailbox
        pMbox->entries[pMbox->tail] = val;

        // Wrap?
        if (++pMbox->tail == pMbox->size)
            pMbox->tail = 0;

        // Increment mail count
        pMbox->count++;

        // Notify waiting threads that item added
        semaphore_post(&pMbox->sema);

        res = 1;
    }
    // Mailbox full!
    else
        res = 0;

    critical_end(cr);

    return res;
}
//-----------------------------------------------------------------
// mailbox_pend: Wait for mailbox message
//-----------------------------------------------------------------
void mailbox_pend(struct mailbox *pMbox, uint32_t *val)
{
    int cr;

    OS_ASSERT(pMbox != NULL);

    cr = critical_start();

    // Pend on a new item being added
    semaphore_pend(&pMbox->sema);

    OS_ASSERT(pMbox->count > 0);

    // Retrieve the mail
    if (val)
        *val = pMbox->entries[pMbox->head];

    // Wrap
    if (++pMbox->head == pMbox->size)
        pMbox->head = 0;

    // Decrement items in queue
    pMbox->count--;

    critical_end(cr);
}
//-----------------------------------------------------------------
// mailbox_pend_timed: Wait for mailbox message (with timeout)
// Returns: 1 = mail retrieved, 0 = timeout
//-----------------------------------------------------------------
int mailbox_pend_timed(struct mailbox *pMbox, uint32_t *val, int timeoutMs)
{
    int cr;
    int result = 0;

    OS_ASSERT(pMbox != NULL);

    cr = critical_start();

    // Wait for specified timeout period
    if (semaphore_timed_pend(&pMbox->sema, timeoutMs))
    {
        OS_ASSERT(pMbox->count > 0);

        // Retrieve the mail
        if (val)
            *val = pMbox->entries[pMbox->head];

        // Wrap
        if (++pMbox->head == pMbox->size)
            pMbox->head = 0;

        // Decrement items in queue
        pMbox->count--;

        result = 1;
    }

    critical_end(cr);

    return result;
}
#endif
