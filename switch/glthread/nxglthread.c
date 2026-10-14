// Mesa's "glthread" for devkitPro's build of Mesa.
//
// Mesa can take the OpenGL calls a program makes, write them into a queue and have a worker thread run
// them through the driver. Half the time OpenMW spends drawing on this console is spent inside Mesa, on one
// thread, so this moves that half to another core. On other systems it is turned on with the environment
// variable mesa_glthread.
//
// The code for it is all in devkitPro's libEGL.a, but nothing starts it: what does that on other systems
// is the DRI front end, which the Switch port replaces with a small EGL driver (src/egl/drivers/switch),
// and devkitPro's patch takes the hooks for it out of the state tracker. Three things are missing:
//
//  - starting the worker for a context,
//  - waiting for the worker before anything else touches the context: when it is made current or
//    released, when a frame is presented, when a surface or the context is destroyed,
//  - stopping the worker before the context is destroyed.
//
// They are added here around the driver's own functions. The linker option --wrap=_eglInitDriver has
// libEGL call the function below in place of the driver's, which in turn is reachable as
// __real__eglInitDriver.
//
// A fourth thing is wrong rather than missing, and --wrap=thrd_create is for that: see the end of the
// file.
//
// This relies on the layout of a few of Mesa's internal structures, so it is only built for the version of
// Mesa those were checked against (see ../CMakeLists.txt). The headers come from devkitPro's source tree
// for that version.
//
// Careful with _EGLDisplay: it holds a mutex, and the C library's mutex has shrunk since devkitPro built
// Mesa. Compiled here, every member behind the mutex is in a different place than libEGL.a has it. Nothing
// in this file looks into a display.

#include "nxglthread.h"

#ifdef NXGLTHREAD_SUPPORTED

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <threads.h>

#include <switch.h>

#include "eglcontext.h"
#include "eglcurrent.h"
#include "egldriver.h"
#include "mapi/glapi/glapi.h"
#include "state_tracker/st_api.h"

// src/mesa/main/glthread.h, which cannot be included without the rest of Mesa's headers.
struct gl_context;
void _mesa_glthread_init(struct gl_context* ctx);
void _mesa_glthread_destroy(struct gl_context* ctx);
void _mesa_glthread_finish(struct gl_context* ctx);

// The driver's context, from src/egl/drivers/switch/egl_switch.c.
struct switch_egl_context
{
    _EGLContext base;
    struct st_context_iface* stctx;
};

// As found in devkitPro's switch-mesa 20.1.0-5 by disassembling it.
_Static_assert(sizeof(_EGLContext) == 104, "_EGLContext differs from the one in libEGL.a");
_Static_assert(offsetof(struct st_context_iface, state_manager) == 16, "st_context_iface differs");
_Static_assert(offsetof(struct st_manager, set_background_context) == 24, "st_manager differs");

#define NXGLTHREAD_MAX_CONTEXTS 8

struct ThreadedContext
{
    _EGLContext* context;
    struct gl_context* gl;
    // eglDestroyContext was called while the context was current, which only destroys it once it is
    // released.
    bool doomed;
};

static struct ThreadedContext sContexts[NXGLTHREAD_MAX_CONTEXTS];
static Mutex sContextsMutex;

static bool sEnabled;
static void (*sThreadStartCallback)(void);

static EGLBoolean (*sMakeCurrent)(_EGLDriver*, _EGLDisplay*, _EGLSurface*, _EGLSurface*, _EGLContext*);
static EGLBoolean (*sSwapBuffers)(_EGLDriver*, _EGLDisplay*, _EGLSurface*);
static EGLBoolean (*sDestroySurface)(_EGLDriver*, _EGLDisplay*, _EGLSurface*);
static EGLBoolean (*sDestroyContext)(_EGLDriver*, _EGLDisplay*, _EGLContext*);

bool nxglthreadSetEnabled(bool enabled)
{
    sEnabled = enabled;
    return true;
}

void nxglthreadSetThreadStartCallback(void (*callback)(void))
{
    sThreadStartCallback = callback;
}

// Call with sContextsMutex locked.
static struct ThreadedContext* findContext(_EGLContext* context)
{
    if (context != NULL)
        for (int i = 0; i < NXGLTHREAD_MAX_CONTEXTS; ++i)
            if (sContexts[i].context == context)
                return &sContexts[i];
    return NULL;
}

// Mesa calls this on the worker thread when it starts. The DRI front end uses it to make the context
// current for the window system on that thread as well, which nothing in the Switch driver depends on.
static void setBackgroundContext(struct st_context_iface* st, struct util_queue_monitoring* queue)
{
    (void)st;
    (void)queue;
    if (sThreadStartCallback != NULL)
        sThreadStartCallback();
}

// Waits until the worker has run everything queued for the context that is current on this thread.
// After that the worker sleeps, and the context can be used from here.
static void finishCurrent(void)
{
    struct gl_context* const gl = _glapi_get_context();
    if (gl != NULL)
        _mesa_glthread_finish(gl);
}

static void startThread(_EGLContext* context)
{
    struct gl_context* const gl = _glapi_get_context();
    if (gl == NULL)
        return;

    mutexLock(&sContextsMutex);
    struct ThreadedContext* entry = findContext(context);
    if (entry == NULL)
        for (int i = 0; i < NXGLTHREAD_MAX_CONTEXTS && entry == NULL; ++i)
            if (sContexts[i].context == NULL)
                entry = &sContexts[i];
    const bool start = entry != NULL && entry->context == NULL;
    if (start)
    {
        entry->context = context;
        entry->gl = gl;
        entry->doomed = false;
    }
    mutexUnlock(&sContextsMutex);

    if (start)
    {
        ((struct switch_egl_context*)context)->stctx->state_manager->set_background_context = &setBackgroundContext;
        _mesa_glthread_init(gl);
    }
}

static EGLBoolean makeCurrent(
    _EGLDriver* driver, _EGLDisplay* display, _EGLSurface* draw, _EGLSurface* read, _EGLContext* context)
{
    // The context this thread gives up, if any.
    _EGLContext* const previous = _eglGetCurrentContext();
    struct gl_context* doomed = NULL;
    mutexLock(&sContextsMutex);
    struct ThreadedContext* const entry = findContext(previous);
    if (entry != NULL && entry->doomed && previous != context)
    {
        doomed = entry->gl;
        entry->context = NULL;
    }
    mutexUnlock(&sContextsMutex);

    if (doomed != NULL)
        _mesa_glthread_destroy(doomed);
    else
        finishCurrent();

    EGLBoolean result = sMakeCurrent(driver, display, draw, read, context);

    if (result && context != NULL && sEnabled)
    {
        mutexLock(&sContextsMutex);
        const bool started = findContext(context) != NULL;
        mutexUnlock(&sContextsMutex);
        if (!started)
        {
            startThread(context);
            // A thread's calls go where the context said when the thread made it current, and until a
            // moment ago this context said to run them directly.
            result = sMakeCurrent(driver, display, draw, read, context);
        }
    }
    return result;
}

static EGLBoolean swapBuffers(_EGLDriver* driver, _EGLDisplay* display, _EGLSurface* surface)
{
    finishCurrent();
    return sSwapBuffers(driver, display, surface);
}

static EGLBoolean destroySurface(_EGLDriver* driver, _EGLDisplay* display, _EGLSurface* surface)
{
    finishCurrent();
    return sDestroySurface(driver, display, surface);
}

static EGLBoolean destroyContext(_EGLDriver* driver, _EGLDisplay* display, _EGLContext* context)
{
    struct gl_context* gl = NULL;
    mutexLock(&sContextsMutex);
    struct ThreadedContext* const entry = findContext(context);
    if (entry != NULL)
    {
        if (context->Binding == NULL)
        {
            gl = entry->gl;
            entry->context = NULL;
        }
        else
            entry->doomed = true;
    }
    mutexUnlock(&sContextsMutex);

    // The state tracker would do this when it destroys the context, were that not patched out. The
    // worker's queue is a part of the context.
    if (gl != NULL)
        _mesa_glthread_destroy(gl);

    return sDestroyContext(driver, display, context);
}

void __real__eglInitDriver(_EGLDriver* driver);

void __wrap__eglInitDriver(_EGLDriver* driver)
{
    __real__eglInitDriver(driver);

    sMakeCurrent = driver->API.MakeCurrent;
    sSwapBuffers = driver->API.SwapBuffers;
    sDestroySurface = driver->API.DestroySurface;
    sDestroyContext = driver->API.DestroyContext;

    driver->API.MakeCurrent = &makeCurrent;
    driver->API.SwapBuffers = &swapBuffers;
    driver->API.DestroySurface = &destroySurface;
    driver->API.DestroyContext = &destroyContext;
}

// Mesa queues a glBufferSubData only if its data fits into one of the worker's command buffers, which
// hold 8 KB. Anything larger it runs on the calling thread, after waiting for the worker to get through
// everything queued before it. OpenSceneGraph uploads the vertices of every animated mesh this way in
// every frame, and each of those waits has the two threads take turns instead of working side by side.
// Passed on in pieces, the data goes into the queue like every other call.
//
// The size of a piece: Mesa puts 32 bytes in front of the data (struct marshal_cmd_BufferSubData), and
// with those a piece takes 4096 bytes, so that two of them fill a command buffer exactly. The queue holds
// no more than seven such buffers, and these uploads are most of what goes through it: 4096 bytes of data
// would leave room for one piece per buffer and the other half of it empty.

#define NXGLTHREAD_UPLOAD_PIECE (4096 - 32)

static void (*sBufferSubData)(unsigned int target, intptr_t offset, ptrdiff_t size, const void* data);

static void bufferSubData(unsigned int target, intptr_t offset, ptrdiff_t size, const void* data)
{
    while (data != NULL && size > NXGLTHREAD_UPLOAD_PIECE)
    {
        sBufferSubData(target, offset, NXGLTHREAD_UPLOAD_PIECE, data);
        offset += NXGLTHREAD_UPLOAD_PIECE;
        size -= NXGLTHREAD_UPLOAD_PIECE;
        data = (const uint8_t*)data + NXGLTHREAD_UPLOAD_PIECE;
    }
    sBufferSubData(target, offset, size, data);
}

void* nxglthreadReplaceFunction(const char* name, void* function)
{
    if (sEnabled && function != NULL
        && (strcmp(name, "glBufferSubData") == 0 || strcmp(name, "glBufferSubDataARB") == 0))
    {
        sBufferSubData = (void (*)(unsigned int, intptr_t, ptrdiff_t, const void*))function;
        return (void*)&bufferSubData;
    }
    return function;
}

// Mesa starts its threads with thrd_create and takes any result but 0 for a failure (u_thread_create in
// src/util/u_thread.h). That holds for the C11 threads Mesa brings along. devkitPro has it use the
// C library's instead, where success is thrd_success, which is 4. So the worker's thread starts, Mesa
// believes it did not and gives up on the worker, and both of them free the memory with the thread's
// arguments. Mesa is the only caller of thrd_create in the program.

int __real_thrd_create(thrd_t* thread, thrd_start_t function, void* argument);

int __wrap_thrd_create(thrd_t* thread, thrd_start_t function, void* argument)
{
    const int result = __real_thrd_create(thread, function, argument);
    return result == thrd_success ? 0 : result;
}

#else

bool nxglthreadSetEnabled(bool enabled)
{
    (void)enabled;
    return false;
}

void nxglthreadSetThreadStartCallback(void (*callback)(void))
{
    (void)callback;
}

void* nxglthreadReplaceFunction(const char* name, void* function)
{
    (void)name;
    return function;
}

#endif
