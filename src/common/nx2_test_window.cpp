// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/nx2_test_window.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>

#include "common/logging.h"

namespace Nx2TestWindow {
namespace {

using Clock = std::chrono::steady_clock;

struct Config {
    double start = -1.0;
    double seconds = 0.0;
    std::string prepare;
    std::string label;
    std::string purpose;
};

std::mutex mutex;
Config config;
Clock::time_point origin;
std::atomic<unsigned> generation{0};
bool running = false;

double EnvSeconds(const char* name, double fallback) {
    const char* text = std::getenv(name);
    return text != nullptr && *text != '\0' ? std::strtod(text, nullptr) : fallback;
}

std::string EnvText(const char* name, const char* fallback) {
    const char* text = std::getenv(name);
    return text != nullptr && *text != '\0' ? std::string{text} : std::string{fallback};
}

Config ReadConfig() {
    Config result;
    const char* sample = std::getenv("EDEN_NX2_SAMPLE");
    const bool sampling = sample != nullptr && *sample != '\0';
    result.start = EnvSeconds("EDEN_NX2_TEST_START",
                              sampling ? EnvSeconds("EDEN_NX2_SAMPLE_START", 60.0) : -1.0);
    result.seconds = EnvSeconds("EDEN_NX2_TEST_SECONDS",
                                sampling ? EnvSeconds("EDEN_NX2_SAMPLE_SECONDS", 30.0) : 0.0);
    result.prepare = EnvText("EDEN_NX2_TEST_PREPARE", "Va au marché, devant la fontaine");
    result.label = EnvText("EDEN_NX2_TEST_LABEL", "Tourne autour de la fontaine");
    result.purpose = EnvText("EDEN_NX2_TEST_PURPOSE", "");
    return result;
}

} // Anonymous namespace

void Start() {
    const Config read = ReadConfig();
    if (read.start < 0.0 || read.seconds <= 0.0) {
        return;
    }
    unsigned my_generation;
    {
        std::scoped_lock lock{mutex};
        config = read;
        origin = Clock::now();
        running = true;
        my_generation = ++generation;
    }
    LOG_INFO(Common, "Nx2TestWindow: recording from {:.0f} s to {:.0f} s after load ({})",
             read.start, read.start + read.seconds, read.label);
    if (!read.purpose.empty()) {
        LOG_INFO(Common, "Nx2TestWindow: purpose: {}", read.purpose);
    }
    std::thread{[read, my_generation] {
        std::this_thread::sleep_for(std::chrono::duration<double>(read.start));
        if (generation.load() != my_generation) {
            return;
        }
        LOG_INFO(Common, "Nx2TestWindow: recording starts");
        std::this_thread::sleep_for(std::chrono::duration<double>(read.seconds));
        if (generation.load() != my_generation) {
            return;
        }
        LOG_INFO(Common, "Nx2TestWindow: recording ends");
    }}.detach();
}

void Stop() {
    std::scoped_lock lock{mutex};
    running = false;
    ++generation;
}

Status Get() {
    std::scoped_lock lock{mutex};
    Status status;
    if (!running) {
        return status;
    }
    const double elapsed = std::chrono::duration<double>(Clock::now() - origin).count();
    status.prepare = config.prepare;
    status.label = config.label;
    status.purpose = config.purpose;
    status.seconds_total = config.seconds;
    if (elapsed < config.start) {
        status.phase = Phase::Waiting;
        status.seconds_left = config.start - elapsed;
    } else if (elapsed < config.start + config.seconds) {
        status.phase = Phase::Recording;
        status.seconds_left = config.start + config.seconds - elapsed;
    } else {
        status.phase = Phase::Done;
    }
    return status;
}

} // namespace Nx2TestWindow
