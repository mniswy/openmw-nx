#ifndef OPENMW_COMPONENTS_PLATFORM_SWITCHPROFILER_HPP
#define OPENMW_COMPONENTS_PLATFORM_SWITCHPROFILER_HPP

#if defined(__SWITCH__)

#include <string_view>

/// A sampling profiler for a system that offers none to homebrew. A thread stops each of the threads it
/// was told about a few hundred times a second, notes where they are and how they got there, and the
/// whole record is written to a file when the program ends. switch/profile.py turns it into function
/// names, given the openmw.elf of the same build.
namespace Platform::Switch::Profiler
{
    /// Starts sampling if the environment variable OPENMW_SWITCH_PROFILE names the file to write.
    void start();

    /// Has the current thread sampled from now on. Does nothing for a thread that already is.
    void addCurrentThread(std::string_view name);
}

#endif

#endif
