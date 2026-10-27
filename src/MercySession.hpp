// MERCY - interface session. Lives in the plugin (not the window), so closing and reopening the
// window in FL brings back exactly where you were: page, bake panel, bake list (and running bakes),
// preset name, A/B slots, light/dark mode and every value.
#pragma once
#include "MercyDSP.hpp"
#include "MercyBake.hpp"
#include <memory>
#include <string>
#include <atomic>

namespace mercy {

struct Snapshot {
    bool valid = false;
    float v[kFirstOutputParam] = {};
    int order[kNumStages] = {6, 7, 0, 1, 2, 3, 4, 5};
};

// One second of input/output peak history for the live "specimen" waveform.
// Written by the audio thread, read by the interface. Lock-free; a torn read only shows one stale bin.
struct Scope {
    static constexpr int N = 180;
    std::atomic<float> in[N], out[N];
    std::atomic<uint32_t> pos {0};
    // audio thread only
    int binLen = 245, count = 0;
    float pkIn = 0, pkOut = 0;
    float pendIn[512];
    int numPend = 0;

    Scope() { for (int i = 0; i < N; ++i) { in[i].store(0.f); out[i].store(0.f); } }
    void setSampleRate(double sr) { binLen = sr > 1000 ? (int)(sr / N) : 245; count = 0; pkIn = pkOut = 0; }

    // call with the input BEFORE processing (buffers may be shared with the output)
    void scanInput(const float* l, const float* r, uint32_t n) {
        numPend = 0;
        int c = count; float p = pkIn;
        for (uint32_t i = 0; i < n; ++i) {
            const float a = std::fabs(l[i]), b = std::fabs(r[i]);
            p = std::max(p, std::max(a, b));
            if (++c >= binLen) { if (numPend < 512) pendIn[numPend++] = p; p = 0; c = 0; }
        }
        pkIn = p;
    }
    // call with the output AFTER processing
    void scanOutput(const float* l, const float* r, uint32_t n) {
        int k = 0;
        float p = pkOut;
        for (uint32_t i = 0; i < n; ++i) {
            const float a = std::fabs(l[i]), b = std::fabs(r[i]);
            p = std::max(p, std::max(a, b));
            if (++count >= binLen) {
                const uint32_t w = pos.load(std::memory_order_relaxed) % N;
                out[w].store(std::isfinite(p) ? p : 0.f, std::memory_order_relaxed);
                in[w].store(k < numPend ? pendIn[k] : 0.f, std::memory_order_relaxed);
                ++k;
                pos.store(pos.load(std::memory_order_relaxed) + 1, std::memory_order_release);
                p = 0; count = 0;
            }
        }
        pkOut = p;
    }
};

struct Session {
    Session() {
        for (uint32_t i = 0; i < kNumParams; ++i) values[i] = kParamInfo[i].def;
        for (int i = 0; i < kNumStages; ++i) order[i] = kDefaultOrder[i];
    }
    float values[kNumParams];
    int order[kNumStages];
    int page = 0;              // 0 main, 1 advanced
    bool bakeOpen = false, bulkMode = false;
    bool dark = true;          // interface theme: dark by default
    int bakeScroll = 0;
    int presetIdx = 0;
    bool presetModified = false, userTouched = false;
    Snapshot ab[2];
    int abCur = 0;
    std::unique_ptr<Baker> baker;
    std::string savedUi;       // last "ui" state handed to the plugin (from the project)
    bool touched = false;      // the user changed the layout in this session (beats the saved state)
    Scope scope;               // live waveform history (filled by the plugin's audio thread)
};

// implemented by the plugin: the session belonging to a plugin instance (nullptr if not available)
Session* sessionFromPluginInstance(void* pluginInstance);

} // namespace mercy
