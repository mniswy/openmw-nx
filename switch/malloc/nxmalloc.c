// The C library's memory allocation functions, done by mimalloc.
//
// newlib's allocator has one lock for everything, taken for every allocation and every release. OpenMW
// allocates from five or six threads at once, and measured on the console the main thread spent between
// a third and half of its time in the allocator, much of that waiting for the lock. mimalloc gives each
// thread memory of its own to allocate from.
//
// A program replaces the allocator by defining its functions: the linker then has no reason to take
// them from the C library. That goes for the names the C library uses internally as well (_malloc_r and
// so on, which take the calling thread's reentrancy structure), or memory from one allocator would end
// up being released to the other. If the C library's allocator does get linked in for some function
// missing here, the link fails with a duplicate definition, which is the time to add the function.

#include <malloc.h>
#include <reent.h>
#include <stdlib.h>

#include <mimalloc.h>

void* malloc(size_t size)
{
    return mi_malloc(size);
}

void free(void* pointer)
{
    mi_free(pointer);
}

void* calloc(size_t count, size_t size)
{
    return mi_calloc(count, size);
}

void* realloc(void* pointer, size_t size)
{
    return mi_realloc(pointer, size);
}

void* memalign(size_t alignment, size_t size)
{
    return mi_memalign(alignment, size);
}

void* aligned_alloc(size_t alignment, size_t size)
{
    return mi_aligned_alloc(alignment, size);
}

int posix_memalign(void** pointer, size_t alignment, size_t size)
{
    return mi_posix_memalign(pointer, alignment, size);
}

void* valloc(size_t size)
{
    return mi_valloc(size);
}

void* pvalloc(size_t size)
{
    return mi_pvalloc(size);
}

size_t malloc_usable_size(void* pointer)
{
    return mi_usable_size(pointer);
}

void* _malloc_r(struct _reent* reent, size_t size)
{
    (void)reent;
    return mi_malloc(size);
}

void _free_r(struct _reent* reent, void* pointer)
{
    (void)reent;
    mi_free(pointer);
}

void* _calloc_r(struct _reent* reent, size_t count, size_t size)
{
    (void)reent;
    return mi_calloc(count, size);
}

void* _realloc_r(struct _reent* reent, void* pointer, size_t size)
{
    (void)reent;
    return mi_realloc(pointer, size);
}

void* _memalign_r(struct _reent* reent, size_t alignment, size_t size)
{
    (void)reent;
    return mi_memalign(alignment, size);
}

void* _valloc_r(struct _reent* reent, size_t size)
{
    (void)reent;
    return mi_valloc(size);
}

void* _pvalloc_r(struct _reent* reent, size_t size)
{
    (void)reent;
    return mi_pvalloc(size);
}

size_t _malloc_usable_size_r(struct _reent* reent, void* pointer)
{
    (void)reent;
    return mi_usable_size(pointer);
}

// In the C library this shares a source file with _free_r.
int _malloc_trim_r(struct _reent* reent, size_t pad)
{
    (void)reent;
    (void)pad;
    return 0;
}
