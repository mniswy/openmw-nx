#ifndef OPENMW_SWITCH_NXGLTHREAD_H
#define OPENMW_SWITCH_NXGLTHREAD_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Whether OpenGL contexts made current for the first time from now on have their calls run by a worker
/// thread of Mesa's ("glthread") instead of by the thread that makes them. Off until asked for.
///
/// Returns false if this build cannot do it, see nxglthread.c.
bool nxglthreadSetEnabled(bool enabled);

/// A function for each of those worker threads to call when it starts, for example to give itself a
/// priority. Like any other thread it starts with the lowest one.
void nxglthreadSetThreadStartCallback(void (*callback)(void));

/// For whoever hands out OpenGL functions by name: given a name and the function Mesa has for it, returns
/// the function to hand out. That is the same one for all but the few that would have the calling thread
/// wait for the worker when there is a way around it.
void* nxglthreadReplaceFunction(const char* name, void* function);

#ifdef __cplusplus
}
#endif

#endif
