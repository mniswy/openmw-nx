#ifndef OPENMW_SWITCH_NXPGO_H
#define OPENMW_SWITCH_NXPGO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// For a build made with OPENMW_SWITCH_PGO=GENERATE, which counts what it executes: writes the counts
/// gathered so far to the given file, for `switch/build.sh --pgo=use` to build by. See nxpgo.c.
///
/// Returns false if the file could not be written.
bool nxpgoWriteProfile(const char* path);

#ifdef __cplusplus
}
#endif

#endif
