// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include "common/common_types.h"
#include "common/logging.h"
#include "common/nx2_placement.h"

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <sched.h>
#endif

namespace Common::Nx2Placement {

namespace {

enum class Mode { Off, Gpu, Split };

struct Plan {
    Mode mode{Mode::Off};
    u64 gpu{};
    u64 rest{};
    /// With EDEN_NX2_PLACEMENT_CORES (default on): the GPU front and back each get a physical
    /// core of the GPU domain to themselves, VulkanWorker the domain's other cores.
    u64 front{};
    u64 back{};
    u64 worker{};
};

#ifdef _WIN32
std::vector<u64> QueryDomains() {
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationCache, nullptr, &length);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || length == 0) {
        return {};
    }
    std::vector<u8> buffer(length);
    auto* const first = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data());
    if (!GetLogicalProcessorInformationEx(RelationCache, first, &length)) {
        return {};
    }
    std::vector<u64> domains;
    for (DWORD offset = 0; offset < length;) {
        const auto* const info =
            reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
        if (info->Size == 0) {
            break;
        }
        if (info->Relationship == RelationCache && info->Cache.Level == 3 &&
            (info->Cache.Type == CacheUnified || info->Cache.Type == CacheData) &&
            info->Cache.GroupMask.Group == 0 && info->Cache.GroupMask.Mask != 0) {
            domains.push_back(static_cast<u64>(info->Cache.GroupMask.Mask));
        }
        offset += info->Size;
    }
    return domains;
}

std::vector<u64> QueryCores() {
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || length == 0) {
        return {};
    }
    std::vector<u8> buffer(length);
    auto* const first = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data());
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, first, &length)) {
        return {};
    }
    std::vector<u64> cores;
    for (DWORD offset = 0; offset < length;) {
        const auto* const info =
            reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
        if (info->Size == 0) {
            break;
        }
        if (info->Relationship == RelationProcessorCore && info->Processor.GroupCount >= 1 &&
            info->Processor.GroupMask[0].Group == 0 && info->Processor.GroupMask[0].Mask != 0) {
            cores.push_back(static_cast<u64>(info->Processor.GroupMask[0].Mask));
        }
        offset += info->Size;
    }
    return cores;
}

u64 ProcessMask() {
    DWORD_PTR process{};
    DWORD_PTR system{};
    if (!GetProcessAffinityMask(GetCurrentProcess(), &process, &system)) {
        return 0;
    }
    return static_cast<u64>(process);
}

bool SetMask(u64 mask) {
    return SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(mask)) != 0;
}
#elif defined(__linux__)
u64 ParseCpuList(const std::string& list) {
    u64 mask = 0;
    std::stringstream stream{list};
    std::string range;
    while (std::getline(stream, range, ',')) {
        const size_t dash = range.find('-');
        const int low = std::atoi(range.substr(0, dash).c_str());
        const int high = dash == std::string::npos ? low : std::atoi(range.substr(dash + 1).c_str());
        for (int cpu = low; cpu <= high && cpu < 64; ++cpu) {
            mask |= u64{1} << cpu;
        }
    }
    return mask;
}

std::vector<u64> QueryDomains() {
    std::set<u64> unique;
    for (int cpu = 0; cpu < 64; ++cpu) {
        const std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cache/";
        for (int index = 0; index < 8; ++index) {
            std::ifstream level_file{base + "index" + std::to_string(index) + "/level"};
            int level = 0;
            if (!(level_file >> level)) {
                break;
            }
            if (level != 3) {
                continue;
            }
            std::ifstream list_file{base + "index" + std::to_string(index) + "/shared_cpu_list"};
            std::string list;
            if (std::getline(list_file, list)) {
                if (const u64 mask = ParseCpuList(list); mask != 0) {
                    unique.insert(mask);
                }
            }
        }
    }
    return {unique.begin(), unique.end()};
}

std::vector<u64> QueryCores() {
    std::set<u64> unique;
    for (int cpu = 0; cpu < 64; ++cpu) {
        std::ifstream file{"/sys/devices/system/cpu/cpu" + std::to_string(cpu) +
                          "/topology/thread_siblings_list"};
        std::string list;
        if (!std::getline(file, list)) {
            continue;
        }
        if (const u64 mask = ParseCpuList(list); mask != 0) {
            unique.insert(mask);
        }
    }
    return {unique.begin(), unique.end()};
}

u64 ProcessMask() {
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) != 0) {
        return 0;
    }
    u64 mask = 0;
    for (int cpu = 0; cpu < 64; ++cpu) {
        if (CPU_ISSET(cpu, &set)) {
            mask |= u64{1} << cpu;
        }
    }
    return mask;
}

bool SetMask(u64 mask) {
    cpu_set_t set;
    CPU_ZERO(&set);
    for (int cpu = 0; cpu < 64; ++cpu) {
        if ((mask >> cpu) & 1) {
            CPU_SET(cpu, &set);
        }
    }
    return sched_setaffinity(0, sizeof(set), &set) == 0;
}
#else
std::vector<u64> QueryDomains() {
    return {};
}
std::vector<u64> QueryCores() {
    return {};
}
u64 ProcessMask() {
    return 0;
}
bool SetMask(u64) {
    return false;
}
#endif

Mode ReadMode() {
    const char* const value = std::getenv("EDEN_NX2_PLACEMENT");
    const std::string_view mode = value != nullptr ? value : "";
    if (mode == "gpu" || mode == "1") {
        return Mode::Gpu;
    }
    if (mode == "split" || mode == "2") {
        return Mode::Split;
    }
    return Mode::Off;
}

Plan MakePlan() {
    Plan plan{.mode = ReadMode()};
    if (plan.mode == Mode::Off) {
        return plan;
    }
    const u64 process = ProcessMask();
    if (process == 0) {
        LOG_WARNING(Common, "Nx2Placement: process affinity unavailable, placement off");
        plan.mode = Mode::Off;
        return plan;
    }
    u64 gpu = 0;
    if (const char* manual = std::getenv("EDEN_NX2_PLACEMENT_GPU"); manual != nullptr) {
        gpu = std::strtoull(manual, nullptr, 16);
    } else {
        const std::vector<u64> domains = QueryDomains();
        std::string listed;
        for (const u64 domain : domains) {
            listed += fmt::format(" {:#x}", domain);
        }
        LOG_INFO(Common, "Nx2Placement: L3 domains:{}", listed.empty() ? " none" : listed);
        if (domains.size() < 2) {
            LOG_INFO(Common, "Nx2Placement: a single L3 domain, nothing to place");
            plan.mode = Mode::Off;
            return plan;
        }
        size_t index = domains.size() - 1;
        if (const char* chosen = std::getenv("EDEN_NX2_PLACEMENT_DOMAIN"); chosen != nullptr) {
            index = std::min<size_t>(std::strtoul(chosen, nullptr, 10), domains.size() - 1);
        }
        gpu = domains[index];
    }
    plan.gpu = gpu & process;
    plan.rest = process & ~plan.gpu;
    // Three busy GPU threads need at least two cores; the rest needs somewhere to run.
    if (std::popcount(plan.gpu) < 2 || plan.rest == 0) {
        LOG_WARNING(Common, "Nx2Placement: unusable masks gpu={:#x} rest={:#x}, placement off",
                    plan.gpu, plan.rest);
        plan.mode = Mode::Off;
        return plan;
    }
    plan.front = plan.back = plan.worker = plan.gpu;
    const char* const per_core = std::getenv("EDEN_NX2_PLACEMENT_CORES");
    if (per_core == nullptr || per_core[0] != '0') {
        std::vector<u64> cores;
        for (const u64 core : QueryCores()) {
            if ((core & plan.gpu) == core) {
                cores.push_back(core);
            }
        }
        if (cores.size() >= 2) {
            plan.front = cores[0];
            plan.back = cores[1];
            u64 others = 0;
            for (size_t i = 2; i < cores.size(); ++i) {
                others |= cores[i];
            }
            // Two cores only: VulkanWorker shares them (its load is the smallest of the three).
            plan.worker = others != 0 ? others : plan.gpu;
        }
    }
    LOG_INFO(Common, "Nx2Placement: mode={} gpu={:#x} rest={:#x} front={:#x} back={:#x} worker={:#x}",
             plan.mode == Mode::Gpu ? "gpu" : "split", plan.gpu, plan.rest, plan.front, plan.back,
             plan.worker);
    return plan;
}

const Plan& GetPlan() {
    static const Plan plan = MakePlan();
    return plan;
}

constexpr const char* RoleName(Role role) {
    switch (role) {
    case Role::GpuFront:
        return "GPU";
    case Role::GpuBack:
        return "GPUBack";
    case Role::VulkanWorker:
        return "VulkanWorker";
    case Role::GuestCore:
        return "guest core";
    case Role::Background:
        return "background worker";
    }
    return "?";
}

} // Anonymous namespace

void Apply(Role role) {
    const Plan& plan = GetPlan();
    if (plan.mode == Mode::Off) {
        return;
    }
    u64 mask = 0;
    switch (role) {
    case Role::GpuFront:
        mask = plan.front;
        break;
    case Role::GpuBack:
        mask = plan.back;
        break;
    case Role::VulkanWorker:
        mask = plan.worker;
        break;
    case Role::GuestCore:
    case Role::Background:
        mask = plan.mode == Mode::Split ? plan.rest : 0;
        break;
    }
    if (mask == 0) {
        return;
    }
    if (!SetMask(mask)) {
        LOG_WARNING(Common, "Nx2Placement: could not place a {} thread on {:#x}", RoleName(role),
                    mask);
    } else if (role != Role::Background) {
        LOG_INFO(Common, "Nx2Placement: {} thread on {:#x}", RoleName(role), mask);
    }
}

} // namespace Common::Nx2Placement
