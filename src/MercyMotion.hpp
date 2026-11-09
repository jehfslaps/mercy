// MERCY - fluid motion: damped springs for everything that moves in the interface.
// Every animated value is a spring with a target. Changing the target redirects the motion from where it is,
// with the speed it has, so nothing ever jumps and everything can be interrupted. The springs are integrated
// at a fixed 240 Hz step from the wall clock, so the motion is the same at any frame rate. The view only
// repaints while something is still moving. Interface thread only (never the audio thread).
#pragma once
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace mercy {

struct SpringCfg { float response, damping; };   // response ~ seconds to settle; damping 1 = no overshoot
static constexpr SpringCfg kSpringUI     {0.38f, 0.78f};   // panels, knobs, value changes
static constexpr SpringCfg kSpringPress  {0.10f, 1.00f};   // button press
static constexpr SpringCfg kSpringFade   {0.475f, 1.00f};  // fades and quiet transitions
static constexpr SpringCfg kSpringScroll {0.42f, 1.00f};   // a list settling after a scroll
static constexpr SpringCfg kSpringHover  {0.16f, 1.00f};   // hover highlights (quick, still eased)
static constexpr SpringCfg kSpringPos    {0.38f, 0.86f};   // modules sliding to new slots
static constexpr SpringCfg kSpringPage   {0.40f, 0.92f};   // a page sliding in
static constexpr SpringCfg kSpringClose  {0.22f, 1.00f};   // panels going away (quicker than they arrive)

static inline uint64_t mkey(uint32_t kind, uint32_t id, uint32_t sub = 0) {
    return ((uint64_t)kind << 48) ^ ((uint64_t)sub << 32) ^ (uint64_t)id;
}

class Motion {
public:
    struct Spring { float x = 0, v = 0, target = 0; SpringCfg cfg {0.38f, 0.78f}; bool settled = true; uint32_t seen = 0; };

    bool reduced = false;   // the OS asks for less motion: everything snaps
    double fixedDt = 0;     // tests and recordings only: advance by exactly this much per frame

    // the current value of `key`, heading for `target`. The first time a key is seen it starts at its target.
    float to(uint64_t key, float target, SpringCfg cfg = kSpringUI) {
        auto it = s.find(key);
        if (it == s.end()) { Spring sp; sp.x = sp.target = target; sp.cfg = cfg; sp.seen = frame; s.emplace(key, sp); return target; }
        Spring& sp = it->second; sp.seen = frame; sp.cfg = cfg;
        if (reduced) { sp.x = sp.target = target; sp.v = 0; sp.settled = true; return target; }
        if (sp.target != target) { sp.target = target; sp.settled = false; moving = true; }
        return sp.x;
    }
    // start a value somewhere else (e.g. "the panel grows out of the button"): it then springs to its target
    void from(uint64_t key, float value, float target, SpringCfg cfg = kSpringUI) {
        Spring& sp = s[key]; sp.cfg = cfg; sp.target = target; sp.seen = frame;
        if (reduced) { sp.x = target; sp.v = 0; sp.settled = true; return; }
        sp.x = value; sp.v = 0; sp.settled = false; moving = true;
    }
    // follow a value exactly (direct manipulation); the spring carries on from here when let go
    void hold(uint64_t key, float value, float velocity = 0) {
        Spring& sp = s[key]; sp.x = sp.target = value; sp.v = velocity; sp.seen = frame; sp.settled = velocity == 0;
        if (!sp.settled) moving = true;
    }
    // a push (soft resistance at the end of a list): moves away and settles back
    void kick(uint64_t key, float velocity) {
        if (reduced) return;
        auto it = s.find(key); if (it == s.end()) return;
        it->second.v += velocity; it->second.settled = false; moving = true;
    }
    float value(uint64_t key, float fallback = 0) const { auto it = s.find(key); return it == s.end() ? fallback : it->second.x; }
    float target(uint64_t key, float fallback = 0) const { auto it = s.find(key); return it == s.end() ? fallback : it->second.target; }
    bool isSettled(uint64_t key) const { auto it = s.find(key); return it == s.end() || it->second.settled; }
    bool isMoving() const { return moving; }

    // advance every spring to now. Call once at the start of each frame.
    void step() {
        const auto now = std::chrono::steady_clock::now();
        double dt = fixedDt > 0 ? fixedDt : (started ? std::chrono::duration<double>(now - last).count() : 0.0);
        last = now; started = true; ++frame;
        if (dt > 0.1) dt = 0.1;                       // after a pause (window hidden): don't fast-forward wildly
        if (!moving) { acc = 0; return; }
        acc += dt;
        const double h = 1.0 / 240.0;
        int n = 0;
        while (acc >= h && n < 48) { acc -= h; ++n; }
        bool any = false;
        for (auto& kv : s) {
            Spring& sp = kv.second;
            if (sp.settled) continue;
            const float w = 6.2831853f / std::max(0.02f, sp.cfg.response), z = sp.cfg.damping;
            for (int i = 0; i < n; ++i) {             // semi-implicit Euler: stable at this step for these springs
                const float a = -w * w * (sp.x - sp.target) - 2.f * z * w * sp.v;
                sp.v += a * (float)h; sp.x += sp.v * (float)h;
            }
            const float span = std::max(1e-3f, std::fabs(sp.target) * 1e-4f);
            if (std::fabs(sp.x - sp.target) < span && std::fabs(sp.v) < span * 8.f) { sp.x = sp.target; sp.v = 0; sp.settled = true; }
            else any = true;
        }
        moving = any;
        if (s.size() > 4096) prune();
    }

private:
    std::unordered_map<uint64_t, Spring> s;
    std::chrono::steady_clock::time_point last;
    bool started = false, moving = false;
    double acc = 0;
    uint32_t frame = 0;
    void prune() { for (auto it = s.begin(); it != s.end();) { if (it->second.settled && frame - it->second.seen > 600) it = s.erase(it); else ++it; } }
};

} // namespace mercy
