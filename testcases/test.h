#ifndef __TEST_H__
#define __TEST_H__

#include "kernel/critical.h"
#include "kernel/os_assert.h"
#include "kernel/list.h"
#include "kernel/thread.h"
#include "kernel/mutex.h"
#include "kernel/semaphore.h"
#include "kernel/event.h"
#include "kernel/mutex.h"
#include "kernel/mailbox.h"

#ifdef __unix__ 
#include <stdio.h>
#include <stdlib.h>
#else
#include "sim_ctrl.h"
#endif

#ifndef __unix__ 
static inline void exit(int exitcode)
{
    sim_exit(exitcode); 
}
#endif

#endif