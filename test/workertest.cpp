#include "../src/MercyBake.hpp"
#include <cstdio>
#include <chrono>
using namespace mercy;
static BakeSettings defaults(const char* preset) {
    BakeSettings s; for (uint32_t p = 0; p < kFirstOutputParam; ++p) s.v[p] = kParamInfo[p].def;
    for (int i = 0; i < kNumStages; ++i) s.order[i] = kDefaultOrder[i]; s.presetName = preset; return s;
}
static void waitIdle(Baker& b) { for (int i = 0; i < 2000 && b.busy(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
static void dump(Baker& b, const char* title) {
    printf("-- %s\n", title);
    for (auto& it : b.snapshot()) printf("   %-24s state %d  %.2fs  -> %s %s%s\n", it.name.c_str(), (int)it.state, it.seconds, it.outName.c_str(), it.asFloat ? "(32f)" : "(24)", it.error.empty() ? "" : (" ERR " + it.error).c_str());
}
int main() {
    {
        Baker b;
        b.addFiles({"/tmp/fmt_24.wav"}, true);
        b.addFiles({"/tmp/fmt.flac"}, true);           // single mode: replaces
        b.bakeAll(defaults("Default Break"));
        waitIdle(b); dump(b, "single mode (should hold only fmt.flac)");
        b.addFiles({"/tmp/fmt_16.wav", "/tmp/fmt.aiff", "/tmp/fmt.mp3", "/tmp/fmt_16.wav", "/home/claude/mercy/README.md"}, false); // bulk: dedupe + skip non-audio
        BakeSettings loud = defaults("gold throne"); loud.v[pOutput] = 12; // force overs -> float file
        b.bakeAll(loud);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        b.bakeAll(defaults("chopped room"));            // re-bake mid-way with new settings
        waitIdle(b); dump(b, "bulk after mid-way re-bake");
        b.addFiles({"/mnt/user-data/uploads/mercy_dry.wav"}, false);
        b.bakeAll(defaults("snap"));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        printf("-- destroying while busy=%d ...\n", (int)b.busy());
    }
    printf("-- destroyed cleanly\n");
    { Baker b; BakeSettings s = defaults("x"); s.v[pOutput] = 12; b.addFiles({"/tmp/fmt_24.wav"}, true); b.bakeAll(s); waitIdle(b); dump(b, "overs -> 32-bit float"); }
}
