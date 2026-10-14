#ifndef OPENMW_COMPONENTS_PLATFORM_SWITCH_HPP
#define OPENMW_COMPONENTS_PLATFORM_SWITCH_HPP

#if defined(__SWITCH__)

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace Platform::Switch
{
    /// Called once from Platform::init.
    void init();

    /// Height of the console's own screen. The interface is laid out for it.
    inline constexpr int sScreenHeight = 720;

    /// Width and height to draw at: those of the console's own screen, or 1920x1080 for a television while
    /// the console is in its dock. What the settings say about the resolution is not used.
    std::pair<int, int> getDisplaySize();

    /// Directory the application was installed to on the SD card, e.g. /switch/openmw.
    ///
    /// The path is spelled without a device name ("sdmc:"). std::filesystem treats "sdmc:/switch" as a
    /// relative path, whereas "/switch" is absolute and still resolves to the SD card.
    const std::filesystem::path& getApplicationDirectory();

    /// Shows the system error dialogue. There is no terminal and no message box to report a fatal error with.
    void showFatalError(const std::string& title, const std::string& message);

    /// Every thread but the main one starts with the lowest priority the system has. Threads of that
    /// priority take turns with each other, and all of them give way to the main thread.
    enum class ThreadPriority
    {
        /// The priority of the main thread, for the thread that draws: both are needed for every frame.
        Main,
        /// Right below the main thread, for threads that the main thread hands a part of its frame to.
        Worker,
        /// What a new thread starts with, for work that nothing waits for.
        Background,
    };

    /// \param name What to call the thread in a profile (see switchprofiler.hpp), which leaves out
    /// threads without a name.
    void setCurrentThreadPriority(ThreadPriority priority, std::string_view name = {});

    /// Writes to the log what the frame rate depends on the most and the application does not decide:
    /// the clock rates of the CPU, the GPU and the memory, which overclocking tools change behind its
    /// back, and whether Mesa runs OpenGL calls on a thread of its own.
    void logSystemInfo();
}

#endif

#endif
