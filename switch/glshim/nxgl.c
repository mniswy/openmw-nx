// Desktop OpenGL entry points for Nintendo Switch.
//
// The Mesa build shipped by devkitPro exports no gl* symbols for desktop OpenGL: they are only reachable
// through eglGetProcAddress. Everything that expects to link against a regular libGL (OpenSceneGraph and
// OpenMW itself) gets its entry points from this library instead. Each stub resolves its target on first
// use, so no explicit initialisation is required.

#define GL_GLEXT_PROTOTYPES 1

#include <EGL/egl.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include <stdio.h>
#include <stdlib.h>

static void* nxglResolve(const char* name)
{
    void* const result = (void*)eglGetProcAddress(name);
    if (result == NULL)
    {
        fprintf(stderr, "nxgl: OpenGL entry point %s is not provided by the driver\n", name);
        fflush(stderr);
        abort();
    }
    return result;
}

// Racing threads store the same value, so the unsynchronised lazy initialisation is benign.
#define NXGL_VOID(name, params, args)                                                                                  \
    void name params                                                                                                   \
    {                                                                                                                  \
        static __typeof__(name)* fn;                                                                                   \
        if (__builtin_expect(fn == NULL, 0))                                                                           \
            fn = (__typeof__(name)*)nxglResolve(#name);                                                                \
        fn args;                                                                                                       \
    }

#define NXGL_RET(ret, name, params, args)                                                                              \
    ret name params                                                                                                    \
    {                                                                                                                  \
        static __typeof__(name)* fn;                                                                                   \
        if (__builtin_expect(fn == NULL, 0))                                                                           \
            fn = (__typeof__(name)*)nxglResolve(#name);                                                                \
        return fn args;                                                                                                \
    }

#include "nxgl_stubs.inc"
