#ifndef __OS_ASSERT_H__
#define __OS_ASSERT_H__

#include "cpu_thread.h"

// To disable RTOS assertions then define CONFIG_RTOS_RELEASE_MODE

//-----------------------------------------------------------------
// DEBUG
//-----------------------------------------------------------------
#ifndef CONFIG_RTOS_RELEASE_MODE

    #define OS_ASSERT(exp)      do { if (!(exp)) cpu_thread_assert(#exp, __FILE__, __LINE__); } while (0)
    #define OS_PANIC(reason)    cpu_thread_assert(reason, __FILE__, __LINE__)

//-----------------------------------------------------------------
// RELEASE
//-----------------------------------------------------------------
#else

    #define OS_ASSERT(exp)     ((void)0)
    #define OS_PANIC(reason)   while (1)

#endif

#endif

