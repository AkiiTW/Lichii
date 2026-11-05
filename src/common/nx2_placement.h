// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// EXPERIMENT: thread placement by L3 cache domain (Zen 2 CCX, Zen 3/4 CCD). The GPU front and
// back threads hand every method to each other through a shared ring; when Windows puts them on
// two CCXs, each handoff and every ring line crosses the Infinity Fabric.
//
// EDEN_NX2_PLACEMENT=gpu    GPU, GPUBack and VulkanWorker are kept on one L3 domain.
// EDEN_NX2_PLACEMENT=split  as gpu, and guest CPU cores, pipeline builders and texture workers
//                           are kept off that domain (they no longer share its SMT siblings).
// EDEN_NX2_PLACEMENT=off    (default) no change.
// EDEN_NX2_PLACEMENT_DOMAIN=<n> picks the L3 domain for the GPU threads (default: the last one,
// away from CPU 0 and its interrupts). EDEN_NX2_PLACEMENT_GPU=<hex mask> sets the GPU threads'
// logical processors by hand (processor group 0), the rest gets the complement.
namespace Common::Nx2Placement {

enum class Role {
    GpuFront,
    GpuBack,
    VulkanWorker,
    GuestCore,
    Background,
};

/// Applies the placement for `role` to the calling thread. No effect unless enabled.
void Apply(Role role);

} // namespace Common::Nx2Placement
