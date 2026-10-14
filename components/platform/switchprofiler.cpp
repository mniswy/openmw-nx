#include "switchprofiler.hpp"

#if defined(__SWITCH__)

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <switch.h>

// The first instruction of the program. Addresses are recorded relative to it, which makes them the
// addresses that the symbols have in openmw.elf.
extern "C" void _start();

namespace Platform::Switch::Profiler
{
    namespace
    {
        constexpr std::size_t sMaxThreads = 16;
        constexpr std::size_t sNameSize = 16;
        constexpr std::size_t sMaxDepth = 32;
        constexpr std::size_t sCapacity = 16 * 1024 * 1024; // in 32-bit words, 64 MiB
        constexpr std::int64_t sIntervalNs = 2'000'000;
        constexpr std::uint32_t sUnknownAddress = 0xFFFFFFFF;

        struct Target
        {
            Handle mHandle;
            char mName[sNameSize];
        };

        // Only ever appended to: the sampling thread reads the first sTargetCount entries without a lock.
        std::array<Target, sMaxThreads> sTargets;
        std::atomic<std::size_t> sTargetCount{ 0 };
        std::mutex sTargetsMutex;

        // A sample is: thread index, milliseconds since the start, number of addresses, the addresses.
        std::unique_ptr<std::uint32_t[]> sSamples;
        std::size_t sUsed = 0;

        std::string sPath;
        std::thread sThread;
        std::atomic<bool> sStop{ false };
        std::uint64_t sStartTick = 0;
        std::atomic<s32> sOwnPriority{ 0 };

        std::uint32_t toRelative(std::uint64_t address)
        {
            const std::uint64_t relative = address - reinterpret_cast<std::uintptr_t>(&_start);
            return relative < sUnknownAddress ? static_cast<std::uint32_t>(relative) : sUnknownAddress;
        }

        // Nothing here may allocate or take a lock: the thread in question is stopped at an arbitrary
        // point, quite possibly while holding the lock that would be needed.
        void record(std::size_t thread, const ThreadContext& context)
        {
            if (sUsed + 3 + sMaxDepth > sCapacity)
                return;

            std::uint32_t* const sample = &sSamples[sUsed];
            std::size_t depth = 0;
            sample[3 + depth++] = toRelative(context.pc.x);
            // The caller if the function has not set up a frame of its own. Sorted out when reading.
            sample[3 + depth++] = toRelative(context.lr);

            // Frame records are {caller's frame pointer, return address} pairs linked through the stack.
            MemoryInfo stack;
            u32 pageInfo;
            if (R_SUCCEEDED(svcQueryMemory(&stack, &pageInfo, context.sp)) && (stack.perm & Perm_R) != 0)
            {
                const std::uint64_t end = stack.addr + stack.size;
                std::uint64_t frame = context.fp;
                std::uint64_t lowest = context.sp;
                while (depth < sMaxDepth && frame >= lowest && frame % 8 == 0 && frame + 16 <= end)
                {
                    std::uint64_t pair[2];
                    std::memcpy(pair, reinterpret_cast<const void*>(frame), sizeof(pair));
                    sample[3 + depth++] = toRelative(pair[1]);
                    lowest = frame + 16;
                    frame = pair[0];
                }
            }

            sample[0] = static_cast<std::uint32_t>(thread);
            sample[1] = static_cast<std::uint32_t>(armTicksToNs(armGetSystemTick() - sStartTick) / 1'000'000);
            sample[2] = static_cast<std::uint32_t>(depth);
            sUsed += 3 + depth;
        }

        void run()
        {
            // Above every thread that is sampled, or the samples would come when those threads let them.
            svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2A);
            s32 priority = 0;
            svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
            sOwnPriority = priority;

            while (!sStop)
            {
                const std::size_t count = sTargetCount.load(std::memory_order_acquire);
                for (std::size_t i = 0; i < count; ++i)
                {
                    const Handle handle = sTargets[i].mHandle;
                    // Fails for a thread that has ended.
                    if (R_FAILED(svcSetThreadActivity(handle, ThreadActivity_Paused)))
                        continue;
                    ThreadContext context;
                    if (R_SUCCEEDED(svcGetThreadContext3(&context, handle)))
                        record(i, context);
                    svcSetThreadActivity(handle, ThreadActivity_Runnable);
                }
                svcSleepThread(sIntervalNs);
            }
        }

        void finish()
        {
            sStop = true;
            sThread.join();

            std::ofstream file(sPath, std::ios_base::binary);
            const char magic[8] = { 'O', 'M', 'W', 'P', 'R', 'O', 'F', '1' };
            file.write(magic, sizeof(magic));
            // Worth knowing when reading the result: at a low priority the samples are not evenly spaced.
            const std::uint32_t priority = static_cast<std::uint32_t>(sOwnPriority.load());
            file.write(reinterpret_cast<const char*>(&priority), sizeof(priority));
            const std::uint32_t count = static_cast<std::uint32_t>(sTargetCount.load());
            file.write(reinterpret_cast<const char*>(&count), sizeof(count));
            for (std::uint32_t i = 0; i < count; ++i)
                file.write(sTargets[i].mName, sNameSize);
            const std::uint64_t used = sUsed;
            file.write(reinterpret_cast<const char*>(&used), sizeof(used));
            file.write(reinterpret_cast<const char*>(sSamples.get()),
                static_cast<std::streamsize>(used * sizeof(std::uint32_t)));
        }
    }

    void start()
    {
        const char* const path = std::getenv("OPENMW_SWITCH_PROFILE");
        if (path == nullptr || *path == '\0' || sSamples != nullptr)
            return;

        sPath = path;
        sSamples = std::make_unique<std::uint32_t[]>(sCapacity);
        sStartTick = armGetSystemTick();
        sThread = std::thread(&run);
        std::atexit(&finish);
    }

    void addCurrentThread(std::string_view name)
    {
        if (sSamples == nullptr)
            return;

        const Handle handle = threadGetCurHandle();
        const std::lock_guard<std::mutex> lock(sTargetsMutex);
        const std::size_t count = sTargetCount.load();
        if (count == sMaxThreads)
            return;
        for (std::size_t i = 0; i < count; ++i)
            if (sTargets[i].mHandle == handle)
                return;

        Target& target = sTargets[count];
        target.mHandle = handle;
        name.copy(target.mName, sNameSize - 1);
        sTargetCount.store(count + 1, std::memory_order_release);
    }
}

#endif
