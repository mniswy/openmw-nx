// Fills the gaps between POSIX and what devkitA64's newlib/libnx provide, so that third-party
// dependencies (OpenSceneGraph, MyGUI, Bullet, SQLite, ...) build without Switch-specific patches.

#include <dlfcn.h>

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <EGL/egl.h>

#include <nxglthread.h>
#include <switch.h>

// ---------------------------------------------------------------------------------------------------------
// Dynamic loading
//
// There are no shared objects. The only symbols anyone looks up at runtime are OpenGL entry points
// (OpenSceneGraph resolves every GL extension function with dlsym on the program itself), so lookups in
// the program are answered by EGL.

static int sProgramHandle;
static const char* sError;

void* dlopen(const char* file, int mode)
{
    (void)mode;
    if (file == NULL)
        return &sProgramHandle;
    sError = "dynamic loading is not supported on this platform";
    return NULL;
}

void* dlsym(void* handle, const char* name)
{
    if (handle == RTLD_DEFAULT || handle == &sProgramHandle)
    {
        void* const result = nxglthreadReplaceFunction(name, (void*)eglGetProcAddress(name));
        if (result == NULL)
            sError = "symbol not found";
        return result;
    }
    sError = "invalid handle";
    return NULL;
}

int dlclose(void* handle)
{
    (void)handle;
    return 0;
}

char* dlerror(void)
{
    const char* const result = sError;
    sError = NULL;
    return (char*)result;
}

// ---------------------------------------------------------------------------------------------------------
// sysconf: declared by newlib but not implemented.

long sysconf(int name)
{
    switch (name)
    {
        case _SC_NPROCESSORS_CONF:
        case _SC_NPROCESSORS_ONLN:
        {
            u64 mask = 0;
            if (R_SUCCEEDED(svcGetInfo(&mask, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0)) && mask != 0)
                return __builtin_popcountll(mask);
            return 1;
        }
        case _SC_PAGESIZE:
            return 0x1000;
        default:
            errno = EINVAL;
            return -1;
    }
}

// ---------------------------------------------------------------------------------------------------------
// File ownership: there are no users on this system. SQLite's POSIX backend refers to both functions.

uid_t geteuid(void)
{
    return 0;
}

int fchown(int fd, uid_t owner, gid_t group)
{
    (void)fd;
    (void)owner;
    (void)group;
    return 0;
}

// ---------------------------------------------------------------------------------------------------------
// Thread stack size
//
// libnx gives threads created without an explicit size a 128 KiB stack, which is far below what code written
// for desktop systems assumes (1 MiB on Windows, 8 MiB on Linux). std::thread offers no way to ask for more,
// so every pthread_create call is routed through here by linking with -Wl,--wrap=pthread_create.

#define NXCOMPAT_MIN_THREAD_STACK_SIZE ((size_t)2 * 1024 * 1024)

int __real_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg);

int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg)
{
    pthread_attr_t adjusted;
    void* stackAddress = NULL;
    size_t stackSize = 0;

    if (attr != NULL)
    {
        adjusted = *attr;
        // Leave callers that supply their own stack memory alone.
        if (pthread_attr_getstack(&adjusted, &stackAddress, &stackSize) != 0 || stackAddress != NULL)
            return __real_pthread_create(thread, attr, start, arg);
    }
    else if (pthread_attr_init(&adjusted) != 0)
        return __real_pthread_create(thread, attr, start, arg);

    if (stackSize < NXCOMPAT_MIN_THREAD_STACK_SIZE)
        pthread_attr_setstacksize(&adjusted, NXCOMPAT_MIN_THREAD_STACK_SIZE);

    const int result = __real_pthread_create(thread, &adjusted, start, arg);

    if (attr == NULL)
        pthread_attr_destroy(&adjusted);

    return result;
}

// ---------------------------------------------------------------------------------------------------------
// SDL thread priorities
//
// devkitPro's SDL raises a thread only for SDL_THREAD_PRIORITY_HIGH. Every other request, including the
// SDL_THREAD_PRIORITY_TIME_CRITICAL that its audio thread makes, puts the thread at the lowest priority,
// where the audio mixer waits for its turn behind background work and the sound breaks up. Linked with
// -Wl,--wrap=SDL_SYS_SetThreadPriority.

#define NXCOMPAT_SDL_THREAD_PRIORITY_HIGH 2
#define NXCOMPAT_SDL_THREAD_PRIORITY_TIME_CRITICAL 3

int __real_SDL_SYS_SetThreadPriority(int priority);

int __wrap_SDL_SYS_SetThreadPriority(int priority)
{
    if (priority == NXCOMPAT_SDL_THREAD_PRIORITY_TIME_CRITICAL)
        priority = NXCOMPAT_SDL_THREAD_PRIORITY_HIGH;
    return __real_SDL_SYS_SetThreadPriority(priority);
}

// ---------------------------------------------------------------------------------------------------------
// rename
//
// POSIX has rename replace a file that is already at the destination, which is how programs save a file
// without ever leaving a half-written one: write a new file, rename it over the old one. The SD card's
// file system refuses instead. Linked with -Wl,--wrap=rename.

int __real_rename(const char* from, const char* to);

int __wrap_rename(const char* from, const char* to)
{
    int result = __real_rename(from, to);
    if (result != 0 && errno == EEXIST)
    {
        struct stat source;
        struct stat destination;
        if (stat(from, &source) == 0 && stat(to, &destination) == 0 && S_ISREG(source.st_mode)
            && S_ISREG(destination.st_mode) && unlink(to) == 0)
            result = __real_rename(from, to);
        else
            errno = EEXIST;
    }
    return result;
}
