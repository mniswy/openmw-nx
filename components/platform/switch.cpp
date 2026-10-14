#include "switch.hpp"

#if defined(__SWITCH__)

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

#include <nxglthread.h>
#include <nxpgo.h>
#include <switch.h>

#include <components/debug/debuglog.hpp>

#include "switchprofiler.hpp"

// Only a build that counts what it executes for profile-guided optimisation has this function (see
// switch/pgo/nxpgo.c). Whether it is there is looked up when the program runs rather than decided when it
// is compiled, so that this file is the same code in the build that takes the counts and in the build
// that is optimised by them.
extern "C" bool nxpgoWriteProfile(const char* path) __attribute__((weak));

namespace Platform::Switch
{
    namespace
    {
        std::filesystem::path findApplicationDirectory()
        {
            // The working directory is where the .nro is, spelled with a device name: "sdmc:/switch/openmw".
            std::error_code ec;
            std::string directory = std::filesystem::current_path(ec).string();
            if (!ec)
            {
                if (const std::size_t colon = directory.find(':'); colon != std::string::npos)
                    directory.erase(0, colon + 1);
                if (!directory.empty() && directory.front() == '/')
                {
                    std::filesystem::path result(directory);
                    if (std::filesystem::exists(result / "openmw.cfg", ec))
                        return result;
                }
            }
            // Started from somewhere else, for example sent over the network with nxlink.
            return "/switch/openmw";
        }

        // The homebrew menu starts programs without a way to give them environment variables, which is how
        // OpenMW, OpenSceneGraph and others take their debugging and profiling options. An optional file
        // next to the executable stands in for that: one NAME=value per line, '#' starts a comment.
        void loadEnvironmentFile(const std::filesystem::path& path)
        {
            std::ifstream file(path);
            std::string line;
            while (std::getline(file, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                const std::size_t equals = line.find('=');
                if (line.empty() || line.front() == '#' || equals == 0 || equals == std::string::npos)
                    continue;
                setenv(line.substr(0, equals).c_str(), line.substr(equals + 1).c_str(), 1);
            }
        }

        constexpr s32 sLowestThreadPriority = 0x3B;

        // What the homebrew loader gives the main thread, replaced by the actual value in init.
        s32 sMainThreadPriority = 0x2C;

        bool sOpenGLThread = false;

        std::filesystem::path getExecutionCountsPath()
        {
            return getApplicationDirectory() / "pgo-profile.gcfn";
        }
    }

    const std::filesystem::path& getApplicationDirectory()
    {
        static const std::filesystem::path directory = findApplicationDirectory();
        return directory;
    }

    void init()
    {
        // Sends stdout and stderr to the PC when started through nxlink, does nothing otherwise.
        if (R_SUCCEEDED(socketInitializeDefault()))
        {
            nxlinkStdio();
            std::atexit(socketExit);
        }

        // The homebrew menu opened from the album runs as an applet with a few hundred MiB of memory.
        const AppletType type = appletGetAppletType();
        if (type != AppletType_Application && type != AppletType_SystemApplication)
            throw std::runtime_error(
                "OpenMW needs the full memory of the console.\n\nOpen the homebrew menu by holding R while "
                "starting any game, instead of opening it from the album.");

        svcGetThreadPriority(&sMainThreadPriority, CUR_THREAD_HANDLE);

        // Before the defaults below, so that the file can replace them.
        loadEnvironmentFile(getApplicationDirectory() / "environment.txt");

        Profiler::start();
        Profiler::addCurrentThread("main");

        if (nxpgoWriteProfile != nullptr)
            std::atexit([] { nxpgoWriteProfile(getExecutionCountsPath().c_str()); });

        // ICU loads its data file (icudt*.dat) from this directory.
        setenv("ICU_DATA", getApplicationDirectory().c_str(), 0);

        // OpenSceneGraph only draws on a separate thread when it detects more than one processor, and it
        // does not know how to count them here. There are three cores, so ask for what it does on desktops.
        setenv("OSG_THREADING", "DrawThreadPerContext", 0);

        // Asking OpenGL whether an error has occurred means waiting until Mesa's thread (below) has run
        // every call made so far. OpenSceneGraph asks twice per frame unless told not to.
        setenv("OSG_GL_ERROR_CHECKING", "OFF", 0);

        // Half the time of the thread that draws goes into Mesa. With this, Mesa does its part on a
        // thread of its own, as it does on other systems when this variable is set.
        setenv("mesa_glthread", "true", 0);
        if (std::string_view(std::getenv("mesa_glthread")) == "true")
        {
            nxglthreadSetThreadStartCallback([] { setCurrentThreadPriority(ThreadPriority::Main, "gl"); });
            sOpenGLThread = nxglthreadSetEnabled(true);
        }
    }

    std::pair<int, int> getDisplaySize()
    {
        // As of the last time the system's messages were read, which SDL does whenever it looks for events.
        if (appletGetOperationMode() == AppletOperationMode_Handheld)
            return { 1280, sScreenHeight };
        return { 1920, 1080 };
    }

    void showFatalError(const std::string& title, const std::string& message)
    {
        ErrorApplicationConfig config;
        if (R_SUCCEEDED(errorApplicationCreate(&config, title.c_str(), message.c_str())))
            errorApplicationShow(&config);
    }

    void setCurrentThreadPriority(ThreadPriority priority, std::string_view name)
    {
        s32 value = sLowestThreadPriority;
        if (priority == ThreadPriority::Main)
            value = sMainThreadPriority;
        else if (priority == ThreadPriority::Worker)
            value = std::min(sMainThreadPriority + 1, sLowestThreadPriority);

        if (const Result result = svcSetThreadPriority(CUR_THREAD_HANDLE, value); R_FAILED(result))
            Log(Debug::Warning) << "Failed to set thread priority " << value << ": 0x" << std::hex << result;

        if (!name.empty())
            Profiler::addCurrentThread(name);
    }

    void logSystemInfo()
    {
        Log(Debug::Info) << "OpenGL calls run on "
                         << (sOpenGLThread ? "a thread of Mesa's" : "the thread that makes them");

        if (nxpgoWriteProfile != nullptr)
            Log(Debug::Info) << "This build counts what it executes for profile-guided optimisation, which "
                                "makes it slow. The counts are written to "
                             << getExecutionCountsPath() << " when the game is quit from its menu.";

        if (R_FAILED(clkrstInitialize()))
            return;

        const auto megahertz = [](PcvModuleId module) {
            ClkrstSession session;
            u32 rate = 0;
            if (R_SUCCEEDED(clkrstOpenSession(&session, module, 3)))
            {
                clkrstGetClockRate(&session, &rate);
                clkrstCloseSession(&session);
            }
            return rate / 1000000.0;
        };
        Log(Debug::Info) << "Clock rates: CPU " << megahertz(PcvModuleId_CpuBus) << " MHz, GPU "
                         << megahertz(PcvModuleId_GPU) << " MHz, memory " << megahertz(PcvModuleId_EMC) << " MHz";
        clkrstExit();
    }
}

#endif
