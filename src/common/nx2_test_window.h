// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

namespace Nx2TestWindow {

// A timed measurement window for play tests, shown on screen so the player knows when to be
// in place. EDEN_NX2_TEST_START / EDEN_NX2_TEST_SECONDS (seconds after the game is loaded) set
// it; without them it follows the sampler's window when EDEN_NX2_SAMPLE is set
// (EDEN_NX2_SAMPLE_START / _SECONDS). EDEN_NX2_TEST_LABEL is the instruction shown during the
// window, EDEN_NX2_TEST_PREPARE the one shown before it, EDEN_NX2_TEST_PURPOSE what the test is
// for (shown the whole time). The start and end are logged, so a log can be cut to the window.

enum class Phase { None, Waiting, Recording, Done };

struct Status {
    Phase phase = Phase::None;
    double seconds_left = 0.0; ///< Until the window starts (Waiting) or ends (Recording).
    double seconds_total = 0.0;
    std::string prepare;
    std::string label;
    std::string purpose;
};

/// Called when a game is loaded: starts the clock (and the start/end log lines) if configured.
void Start();

/// Forgets the window (the game stopped).
void Stop();

/// Where the current game is in its window.
Status Get();

} // namespace Nx2TestWindow
