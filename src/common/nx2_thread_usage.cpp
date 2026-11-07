// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <fmt/format.h>

#include "common/common_types.h"
#include "common/logging.h"
#include "common/nx2_thread_usage.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Common::Nx2ThreadUsage {

namespace {

bool Enabled() {
    static const bool enabled = [] {
        const char* const value = std::getenv("EDEN_NX2_THREAD_USAGE");
        return value != nullptr && value[0] == '1';
    }();
    return enabled;
}

#ifdef _WIN32
struct Entry {
    HANDLE handle;
    std::string name;
    u64 last{};
};

std::mutex mutex;
std::vector<Entry> entries;
std::once_flag started;

u64 FileTimeTo100ns(const FILETIME& time) {
    return (u64{time.dwHighDateTime} << 32) | time.dwLowDateTime;
}

u64 ThreadTime(HANDLE handle) {
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (!GetThreadTimes(handle, &creation, &exit, &kernel, &user)) {
        return 0;
    }
    return FileTimeTo100ns(kernel) + FileTimeTo100ns(user);
}

u64 ProcessTime() {
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        return 0;
    }
    return FileTimeTo100ns(kernel) + FileTimeTo100ns(user);
}

void Monitor() {
    constexpr auto window = std::chrono::seconds{10};
    u64 last_process = ProcessTime();
    auto last_time = std::chrono::steady_clock::now();
    for (;;) {
        std::this_thread::sleep_for(window);
        const auto now = std::chrono::steady_clock::now();
        const double wall_100ns =
            std::chrono::duration<double>(now - last_time).count() * 1e7;
        last_time = now;
        const u64 process = ProcessTime();
        const double process_percent =
            static_cast<double>(process - last_process) / wall_100ns * 100.0;
        last_process = process;

        // Busy time per name: pools (worker threads sharing a name) are summed, with their count.
        std::map<std::string, std::pair<double, u32>> by_name;
        double named = 0.0;
        {
            std::scoped_lock lock{mutex};
            for (Entry& entry : entries) {
                const u64 total = ThreadTime(entry.handle);
                const double percent =
                    static_cast<double>(total - std::min(total, entry.last)) / wall_100ns * 100.0;
                entry.last = total;
                named += percent;
                // Threads of a stopped game are counted one last time, then forgotten: a game
                // started again in the same session would otherwise show every name twice.
                DWORD exit_code{};
                if (GetExitCodeThread(entry.handle, &exit_code) && exit_code != STILL_ACTIVE) {
                    if (percent > 0.0) {
                        by_name[entry.name].first += percent;
                    }
                    continue;
                }
                auto& [sum, count] = by_name[entry.name];
                sum += percent;
                ++count;
            }
            std::erase_if(entries, [](const Entry& entry) {
                DWORD exit_code{};
                if (GetExitCodeThread(entry.handle, &exit_code) && exit_code != STILL_ACTIVE) {
                    CloseHandle(entry.handle);
                    return true;
                }
                return false;
            });
        }
        std::vector<std::pair<double, std::string>> sorted;
        for (const auto& [name, usage] : by_name) {
            if (usage.first >= 1.0) {
                sorted.emplace_back(usage.first, usage.second > 1
                                                     ? fmt::format("{} x{}", name, usage.second)
                                                     : name);
            }
        }
        std::sort(sorted.begin(), sorted.end(), std::greater<>{});
        std::string line;
        for (size_t i = 0; i < sorted.size() && i < 16; ++i) {
            line += fmt::format(" | {} {:.0f}%", sorted[i].second, sorted[i].first);
        }
        LOG_INFO(Common, "Thread CPU last 10 s (100% = one logical CPU): process {:.0f}%, "
                         "unnamed threads {:.0f}%{}",
                 process_percent, std::max(0.0, process_percent - named), line);
    }
}
#endif

} // Anonymous namespace

void OnThreadNamed(const char* name) {
#ifdef _WIN32
    if (!Enabled() || name == nullptr) {
        return;
    }
    HANDLE handle{};
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &handle,
                         THREAD_QUERY_LIMITED_INFORMATION, FALSE, 0)) {
        return;
    }
    {
        std::scoped_lock lock{mutex};
        entries.push_back(Entry{handle, name, ThreadTime(handle)});
    }
    std::call_once(started, [] {
        LOG_INFO(Common, "Thread CPU usage logged every 10 s (EDEN_NX2_THREAD_USAGE)");
        std::thread{Monitor}.detach();
    });
#else
    (void)name;
#endif
}

} // namespace Common::Nx2ThreadUsage
