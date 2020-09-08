#ifndef __EVENT_H__
#define __EVENT_H__

#include "thread.h"
#include "semaphore.h"

//-----------------------------------------------------------------
// Defines
//-----------------------------------------------------------------
#define EVENT_INIT()    {0, SEMAPHORE_INIT(0)}
#define EVENT_DECL(id) \
        static struct event event_ ## id = EVENT_INIT()

//-----------------------------------------------------------------
// Types
//-----------------------------------------------------------------
struct event
{
    uint32_t            value;
    struct semaphore    sema;
};

//-----------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------

// Initialise event object
void        event_init(struct event *ev);

// Wait for an event to be set (returns bitmap)
void        event_set(struct event *ev, uint32_t value);

// Post event value (or add additional bits if already set)
uint32_t    event_get(struct event *ev);

#endif
