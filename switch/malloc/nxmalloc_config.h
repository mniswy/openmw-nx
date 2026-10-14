// Included ahead of everything else in each of mimalloc's source files (see ../CMakeLists.txt).

#ifndef OPENMW_SWITCH_NXMALLOC_CONFIG_H
#define OPENMW_SWITCH_NXMALLOC_CONFIG_H

#include <stdint.h>

// mimalloc tells threads apart by their thread pointer, and on this architecture would read it from the
// register tpidr_el0. Horizon keeps the thread pointer in tpidrro_el0 and leaves tpidr_el0 alone, so
// that every thread would look like the same one.
static inline uintptr_t nxmallocThreadId(void)
{
    uintptr_t value;
    __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(value));
    return value;
}

#define MI_PRIM_THREAD_ID nxmallocThreadId

#endif
