// this is another optimisation, just swapping the memory allocator out for mimalloc. Suprisingly boosts performance a lot

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

int _malloc_trim_r(struct _reent* reent, size_t pad)
{
    (void)reent;
    (void)pad;
    return 0;
}
