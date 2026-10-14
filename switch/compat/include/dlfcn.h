// Minimal <dlfcn.h> for Nintendo Switch, where there is no dynamic loader.
// It exists so that third-party code written for POSIX compiles unmodified; see nxcompat.c.

#ifndef OPENMW_SWITCH_COMPAT_DLFCN_H
#define OPENMW_SWITCH_COMPAT_DLFCN_H

#ifdef __cplusplus
extern "C"
{
#endif

#define RTLD_LAZY 0x0001
#define RTLD_NOW 0x0002
#define RTLD_GLOBAL 0x0100
#define RTLD_LOCAL 0x0000
#define RTLD_DEFAULT ((void*)0)

    void* dlopen(const char* file, int mode);
    void* dlsym(void* handle, const char* name);
    int dlclose(void* handle);
    char* dlerror(void);

#ifdef __cplusplus
}
#endif

#endif
