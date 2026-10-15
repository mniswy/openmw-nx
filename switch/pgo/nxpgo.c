// Profile-guided optimisation: what a build that counts what it executes needs on this system.
//
// Built with -fprofile-generate, the compiler adds a counter to every branch and records which functions
// the indirect calls end up in. A second build made with -fprofile-use reads those numbers and lays out,
// inlines and optimises the code for what actually ran. See "Profile-guided optimisation" in ../README.md.
//
// The numbers normally leave the program through the compiler's runtime library (libgcov), which writes
// one file per source file when the program exits, each next to the object file it belongs to. That is a
// path on the PC the program was built on. So the build uses what the compiler has for systems without
// such a file system instead (-fprofile-info-section): the linker collects a pointer to each source file's
// counters in one section, and the function below writes them all to a single file, in the format that
// `gcov-tool merge-stream` turns into those per-source files on the PC.
//
// This file is only part of a build with OPENMW_SWITCH_PGO=GENERATE, and is not itself counted.

//this is done by claude, was a pretty nice optimisation. got rid of a lot of the stutters

#include "nxpgo.h"

#include <gcov.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// The section is named gcov_info, and the linker defines these two for a section whose name is a valid
// identifier. Referring to them is also what keeps the section from being discarded as unused.
extern const struct gcov_info* const __start_gcov_info[];
extern const struct gcov_info* const __stop_gcov_info[];

static void writeData(const void* data, unsigned size, void* file)
{
    fwrite(data, 1, size, (FILE*)file);
}

static void writeFileName(const char* name, void* file)
{
    __gcov_filename_to_gcfn(name, &writeData, file);
}

static void* allocate(unsigned size, void* file)
{
    (void)file;
    return malloc(size);
}

bool nxpgoWriteProfile(const char* path)
{
    FILE* const file = fopen(path, "wb");
    if (file == NULL)
        return false;
    // Tens of megabytes in pieces of a few bytes.
    setvbuf(file, NULL, _IOFBF, 1024 * 1024);

    for (const struct gcov_info* const* info = __start_gcov_info; info != __stop_gcov_info; ++info)
        __gcov_info_to_gcda(*info, &writeFileName, &writeData, &allocate, file);

    const bool failed = ferror(file) != 0;
    return fclose(file) == 0 && !failed;
}

// To find out where an indirect call went, the code that makes the call notes the address it is about to
// call in a variable of its thread, and the function that gets called passes its own address to one of
// the two functions below, which count the call if the two addresses match.
//
// libgcov has these functions. But devkitPro builds it to find a thread's variables through the register
// tpidr_el0, which nothing sets on this system: here they are found through __aarch64_read_tp, which the
// rest of the program is compiled to call (-mtp=soft). With libgcov's own, the first indirect call would
// read from an address near zero. So they are repeated here, compiled like everything else, from
// libgcc/libgcov-profiler.c of GCC 16. Defining all three symbols of libgcov's object file keeps the
// linker from taking that one. ../CMakeLists.txt checks that libgcov still calls them by these names.

struct indirect_call_tuple
{
    void* callee;
    long long* counters;
};

__thread struct indirect_call_tuple __gcov_indirect_call;

// In libgcov, and without thread variables of their own.
void __gcov_topn_values_profiler(long long* counters, long long value);
void __gcov_topn_values_profiler_atomic(long long* counters, long long value);

void __gcov_indirect_call_profiler_v4(long long value, void* function)
{
    if (function == __gcov_indirect_call.callee)
        __gcov_topn_values_profiler(__gcov_indirect_call.counters, value);
    __gcov_indirect_call.callee = NULL;
}

void __gcov_indirect_call_profiler_v4_atomic(long long value, void* function)
{
    if (function == __gcov_indirect_call.callee)
        __gcov_topn_values_profiler_atomic(__gcov_indirect_call.counters, value);
    __gcov_indirect_call.callee = NULL;
}
