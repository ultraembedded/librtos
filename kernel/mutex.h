#ifndef __MUTEX_H__
#define __MUTEX_H__

#include "list.h"

//-----------------------------------------------------------------
// Defines
//-----------------------------------------------------------------
#define MUTEX_INIT()                {0, 0, 0, LIST_INIT}
#define MUTEX_INIT_RECURSIVE()      {0, 1, 0, LIST_INIT}
#define MUTEX_DECL(id) \
        static struct mutex mtx_ ## id = MUTEX_INIT()

//-----------------------------------------------------------------
// Types
//-----------------------------------------------------------------
struct mutex
{
    void *              owner;
    int                 recursive;
    int                 depth;
    struct link_list    pend_list;
};

//-----------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------

// Initialise mutex
void    mutex_init(struct mutex *mtx, int recursive);

// Acquire mutex (optionally recursive)
void    mutex_lock(struct mutex *mtx);

// Acquire mutex, return 1 if acquired, 0 if not
int     mutex_trylock(struct mutex *mtx);

// Release mutex (optionally recursive)
void    mutex_unlock(struct mutex *mtx);

#endif

