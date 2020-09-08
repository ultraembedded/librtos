#ifndef __CRITICAL_H__
#define __CRITICAL_H__

#include "cpu_thread.h"

//-----------------------------------------------------------------
// critical_start: Force interrupts to be disabled (recursive ok)
//-----------------------------------------------------------------
static inline int critical_start(void)
{
    return cpu_critical_start();
}
//-----------------------------------------------------------------
// critical_end: Restore interrupt enable state (recursive ok)
//-----------------------------------------------------------------
static inline void critical_end(int cr)
{
    cpu_critical_end(cr);
}

#endif

