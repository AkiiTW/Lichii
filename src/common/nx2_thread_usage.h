// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// EDEN_NX2_THREAD_USAGE=1: every 10 s, logs how much CPU time each named emulator thread used
// (guest cores, GPU front and back, VulkanWorker, pools summed by name) and the whole process,
// so a slow scene shows which thread is saturated: the GPU threads or the guest CPU.
namespace Common::Nx2ThreadUsage {

/// Registers the calling thread under `name` (from SetCurrentThreadName). No effect when off.
void OnThreadNamed(const char* name);

} // namespace Common::Nx2ThreadUsage
