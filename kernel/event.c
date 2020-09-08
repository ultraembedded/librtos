#include "event.h"
#include "critical.h"
#include "os_assert.h"

#ifdef INCLUDE_EVENTS

//-----------------------------------------------------------------
// event_init: Initialise event object
//-----------------------------------------------------------------
void event_init(struct event *ev)
{
    OS_ASSERT(ev != NULL);

    ev->value = 0;
    semaphore_init(&ev->sema, 0);
}
//-----------------------------------------------------------------
// event_get: Wait for an event to be set (returns bitmap)
//-----------------------------------------------------------------
uint32_t event_get(struct event *ev)
{
    uint32_t value = 0;
    int cr;

    OS_ASSERT(ev != NULL);

    cr = critical_start();

    // Wait for semaphore (it is safe to do this in a critical section)
    semaphore_pend(&ev->sema);

    // Retrieve event value & reset
    value = ev->value;
    ev->value = 0;

    critical_end(cr);

    return value;
}
//-----------------------------------------------------------------
// event_set: Post event value (or add additional bits if already set)
//-----------------------------------------------------------------
void event_set(struct event *ev, uint32_t value)
{
    int cr;

    OS_ASSERT(ev != NULL);
    OS_ASSERT(value);

    cr = critical_start();

    // Already pending event
    if (ev->value != 0)
    {
        // Add additional bits to the bitmap
        ev->value |= value;
    }
    // No pending event
    else
    {
        ev->value = value;

        // Kick semaphore for first event
        semaphore_post(&ev->sema);
    }

    critical_end(cr);
}

#endif
