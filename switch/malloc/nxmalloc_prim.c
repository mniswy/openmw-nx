// What mimalloc needs from the system it runs on (see include/mimalloc/prim.h in its sources), for
// Horizon as libnx presents it.
//
// There is no way to map or unmap memory at will. A program has one heap, set up by libnx before main,
// and the C library's sbrk hands it out from bottom to top. mimalloc takes large pieces from there and
// manages them itself; it never gives one back, as the C library's allocator never did.

#include "mimalloc.h"

#include "mimalloc/atomic.h"
#include "mimalloc/internal.h"
#include "mimalloc/prim.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <switch.h>

// ---------------------------------------------------------------------------------------------------------
// Memory

void _mi_prim_mem_init(mi_os_mem_config_t* config)
{
    config->page_size = 0x1000;
    config->alloc_granularity = 0x1000;
    config->has_overcommit = false;
    config->has_partial_free = false;
    config->has_virtual_reserve = false;
}

static Mutex sHeapMutex;

int _mi_prim_alloc(
    size_t size, size_t try_alignment, bool commit, bool allow_large, bool* is_large, bool* is_zero, void** addr)
{
    MI_UNUSED(commit);
    MI_UNUSED(allow_large);
    *is_large = false;
    // The homebrew menu ran in this heap before, so it is not known to be zero.
    *is_zero = false;
    *addr = NULL;

    if (try_alignment < 0x1000)
        try_alignment = 0x1000;

    // sbrk is not safe to call from two threads, and the alignment is made from two calls.
    mutexLock(&sHeapMutex);
    void* const current = sbrk(0);
    if (current != (void*)-1)
    {
        const size_t padding = (uintptr_t)mi_align_up_ptr(current, try_alignment) - (uintptr_t)current;
        void* const base = sbrk((ptrdiff_t)(padding + _mi_align_up(size, 0x1000)));
        if (base != (void*)-1)
            *addr = (uint8_t*)base + padding;
    }
    mutexUnlock(&sHeapMutex);

    return *addr != NULL ? 0 : ENOMEM;
}

int _mi_prim_free(void* addr, size_t size)
{
    MI_UNUSED(addr);
    MI_UNUSED(size);
    return 0;
}

// All of the heap is backed by memory from the start.

int _mi_prim_commit(void* addr, size_t size, bool* is_zero)
{
    MI_UNUSED(addr);
    MI_UNUSED(size);
    *is_zero = false;
    return 0;
}

int _mi_prim_decommit(void* addr, size_t size, bool* needs_recommit)
{
    MI_UNUSED(addr);
    MI_UNUSED(size);
    *needs_recommit = false;
    return 0;
}

int _mi_prim_reset(void* addr, size_t size)
{
    MI_UNUSED(addr);
    MI_UNUSED(size);
    return 0;
}

int _mi_prim_protect(void* addr, size_t size, bool protect)
{
    MI_UNUSED(addr);
    MI_UNUSED(size);
    MI_UNUSED(protect);
    return 0;
}

int _mi_prim_alloc_huge_os_pages(void* hint_addr, size_t size, int numa_node, bool* is_zero, void** addr)
{
    MI_UNUSED(hint_addr);
    MI_UNUSED(size);
    MI_UNUSED(numa_node);
    *is_zero = false;
    *addr = NULL;
    return ENOSYS;
}

size_t _mi_prim_numa_node(void)
{
    return 0;
}

size_t _mi_prim_numa_node_count(void)
{
    return 1;
}

// ---------------------------------------------------------------------------------------------------------
// Clock, process information, messages, settings, random numbers

mi_msecs_t _mi_prim_clock_now(void)
{
    return (mi_msecs_t)(armTicksToNs(armGetSystemTick()) / 1000000);
}

void _mi_prim_process_info(mi_process_info_t* pinfo)
{
    MI_UNUSED(pinfo);
}

// To the debugger, if there is one. Whatever else could show a message allocates memory to do it.
void _mi_prim_out_stderr(const char* msg)
{
    svcOutputDebugString(msg, strlen(msg));
}

// mimalloc reads its settings before the program has had a chance to set any.
bool _mi_prim_getenv(const char* name, char* result, size_t result_size)
{
    MI_UNUSED(name);
    MI_UNUSED(result);
    MI_UNUSED(result_size);
    return false;
}

bool _mi_prim_random_buf(void* buf, size_t buf_len)
{
    // The kernel has four values of entropy for a process, which is what mimalloc asks for.
    uint64_t entropy[4];
    for (uint64_t i = 0; i < 4; ++i)
        if (R_FAILED(svcGetInfo(&entropy[i], InfoType_RandomEntropy, INVALID_HANDLE, i)))
            return false;
    for (size_t i = 0; i < buf_len; ++i)
        ((uint8_t*)buf)[i] = ((const uint8_t*)entropy)[i % sizeof(entropy)];
    return true;
}

// ---------------------------------------------------------------------------------------------------------
// The end of a thread, when the memory it has to itself goes back to the others. A thread-specific value
// with a destructor is how to hear of it.

static pthread_key_t sThreadDoneKey = (pthread_key_t)-1;

static void threadDone(void* value)
{
    if (value != NULL)
        _mi_thread_done((mi_heap_t*)value);
}

void _mi_prim_thread_init_auto_done(void)
{
    pthread_key_create(&sThreadDoneKey, &threadDone);
}

void _mi_prim_thread_done_auto_done(void)
{
    if (sThreadDoneKey != (pthread_key_t)-1)
        pthread_key_delete(sThreadDoneKey);
}

void _mi_prim_thread_associate_default_heap(mi_heap_t* heap)
{
    if (sThreadDoneKey != (pthread_key_t)-1)
        pthread_setspecific(sThreadDoneKey, heap);
}
