// capture: grab / record land on the bar grid, line up to the sample, and a looped capture of a looping
// groove plays back exactly like the live stream (no seam). Runs the real Engine, host blocks of odd sizes.
//   g++ -std=gnu++17 -O2 -I../src capturetest.cpp ../src/MercyBake.cpp -lpthread -o capturetest && ./capturetest ../test/dry_loop.wav
#include "../src/MercyBake.hpp"
#include "../src/MercyCapture.hpp"
#include <cstdio>
#include <cmath>
#include <random>
using namespace mercy;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct Host {   // a host playing a loop at a tempo, calling run() with varying block sizes
    double sr, bpm; int bpb = 4; uint64_t pos = 0; bool playing = true; std::mt19937 rng {7};
    Engine e; Capture cap; std::vector<float> loopL, loopR; uint64_t loopStart = 0;
    std::vector<float> outL, outR;     // everything the plugin output (for reference)
    Host(double s, double t, const std::vector<float>& l, const std::vector<float>& r) : sr(s), bpm(t), loopL(l), loopR(r) {
        e.setSampleRate(sr);
        for (uint32_t p = 0; p < kFirstOutputParam; ++p) e.setParam(p, kParamInfo[p].def);
        e.setParam(pSquash, .6f); e.setParam(pRvOn, 1); e.setParam(pRvMix, .3f); e.setParam(pRvDecay, 1.2f);   // something with memory + a tail
        cap.ensure(sr);
    }
    void run(uint64_t frames) {
        std::uniform_int_distribution<int> bs(17, 1100);
        while (frames > 0) {
            const uint32_t n = (uint32_t)std::min<uint64_t>(frames, (uint64_t)bs(rng));
            std::vector<float> il(n), ir(n), ol(n), orr(n);
            for (uint32_t i = 0; i < n; ++i) { const size_t k = (size_t)((pos - loopStart + i) % loopL.size()); il[i] = playing ? loopL[k] : 0; ir[i] = playing ? loopR[k] : 0; }
            CaptureTime t; t.playing = playing; t.valid = playing; t.bpm = bpm; t.beatsPerBar = (float)bpb;
            const double beats = (double)(pos - loopStart) / sr * bpm / 60.0; t.posInBar = std::fmod(beats, bpb);
            e.process(il.data(), ir.data(), ol.data(), orr.data(), n);
            cap.process(ol.data(), orr.data(), n, t, e.latencySamples());
            outL.insert(outL.end(), ol.begin(), ol.end()); outR.insert(outR.end(), orr.begin(), orr.end());
            pos += n; frames -= n;
        }
    }
    uint64_t bar(double k) const { return loopStart + (uint64_t)std::llround(k * bpb * 60.0 / bpm * sr); }
};
static double diffDb(const std::vector<float>& a, const std::vector<float>& b, size_t ao, size_t bo, size_t n) {
    double s = 0, e = 0; for (size_t i = 0; i < n; ++i) { s += (double)b[bo + i] * b[bo + i]; const double d = a[ao + i] - b[bo + i]; e += d * d; }
    return 10 * std::log10(std::max(e, 1e-30) / std::max(s, 1e-30));
}
int main(int argc, char** argv) {
    std::vector<float> L, R; double sr = 48000; std::string err;
    if (argc < 2 || !loadAudioFile(argv[1], L, R, sr, err)) { printf("need a loop: %s\n", err.c_str()); return 1; }
    // treat the loop as 1 bar... at whatever tempo makes it exactly 2 bars long
    const double bpm = 2 * 4 * 60.0 / (L.size() / sr);
    printf("loop %.3f s -> %.3f bpm, 2 bars, sr %.0f\n", L.size() / sr, bpm, sr);
    Host h(sr, bpm, L, R);
    const uint32_t lat = h.e.latencySamples();
    h.run(h.bar(9.37));                                   // play 9.37 bars

    // ---- grab the last 4 bars
    Capture::Clip c;
    bool ok = h.cap.grab(4, c);
    CHECK(ok && c.bars == 4, "grab returned 4 bars (%d)", c.bars);
    const uint64_t a = h.bar(5) + lat, b = h.bar(9) + lat;  // bars 5..9 of the timeline, as they came out of the plugin
    CHECK(c.L.size() == b - a, "grab length %zu == 4 bars %llu", c.L.size(), (unsigned long long)(b - a));
    CHECK(diffDb(c.L, h.outL, 0, (size_t)a, c.L.size()) < -200, "grab is sample-exact against the output stream (latency %u)", lat);
    // looping it: the end flows into the start exactly like the stream did (the groove repeats every 2 bars)
    const size_t seam = 2048;
    std::vector<float> wrap(c.L.end() - seam, c.L.end()); wrap.insert(wrap.end(), c.L.begin(), c.L.begin() + seam);
    // what the stream itself did across that same bar line (the groove repeats every 2 bars, so bar 9 = bar 5)
    const double seamDb = diffDb(wrap, h.outL, 0, (size_t)(b - seam), 2 * seam);
    printf("      seam vs the stream across the same bar line: %.1f dB\n", seamDb);
    // the stream's own loop-to-loop difference at that point (age adds random crackle, so it never repeats exactly)
    const double selfDb = diffDb(h.outL, h.outL, (size_t)b, (size_t)a, seam);
    printf("      the stream vs itself 4 bars earlier: %.1f dB\n", selfDb);
    CHECK(seamDb <= selfDb + 3.5, "looping the capture is as smooth as the music repeating itself (%.1f vs %.1f dB)", seamDb, selfDb);

    // ---- record: arm mid-bar, starts on the next bar, stops after 2 bars
    h.cap.arm(2);
    h.run(h.bar(9.9) - h.pos);
    CHECK(h.cap.state() == Capture::kArmed, "armed mid-bar: waits for the bar line");
    h.run(h.bar(10.3) - h.pos);
    CHECK(h.cap.state() == Capture::kRec, "recording from the bar line");
    h.run(h.bar(12.05) - h.pos);
    CHECK(h.cap.state() == Capture::kDone, "stopped by itself after 2 bars");
    Capture::Clip r; h.run(lat + 64);
    ok = h.cap.poll(r);
    const uint64_t ra = h.bar(10) + lat, rb = h.bar(12) + lat;
    CHECK(ok && r.L.size() == rb - ra, "record length %zu == 2 bars %llu", r.L.size(), (unsigned long long)(rb - ra));
    CHECK(ok && diffDb(r.L, h.outL, 0, (size_t)ra, r.L.size()) < -200, "record starts exactly on bar 10 (output-aligned)");

    // ---- record "free" length + stop: finishes the bar
    h.cap.arm(0); h.run(h.bar(13.2) - h.pos); h.run(h.bar(14.3) - h.pos);
    h.cap.stop(); CHECK(h.cap.state() == Capture::kStopReq, "stop requested");
    h.run(h.bar(15.02) - h.pos); h.run(lat + 64);
    ok = h.cap.poll(r);
    CHECK(ok && r.L.size() == h.bar(15) - h.bar(13) && r.bars == 2, "stop finished the bar: bars 13-15 (%zu frames, %d bars)", r.L.size(), r.bars);

    // ---- transport stopped: starts on the first sound, trailing silence trimmed
    h.playing = false; h.run((uint64_t)(sr * 3));        // silence (and the reverb dies)
    h.cap.arm(4); h.run((uint64_t)(sr * 0.5));
    CHECK(h.cap.state() == Capture::kArmed, "stopped transport: still armed in silence");
    h.playing = true; h.loopStart = h.pos;                // the user hits play... but the host reports no tempo
    {   // play 1.5 s with no bar info
        const double keep = h.bpm; h.bpm = 0; h.run((uint64_t)(sr * 1.5)); h.bpm = keep;
    }
    printf("      (free take state %d)\n", h.cap.state());
    h.cap.stop(); h.run(2048); ok = h.cap.poll(r);
    CHECK(ok && r.bars == 0 && r.seconds > 1.0 && r.seconds < 2.2, "free take %.2f s", r.seconds);
    printf(fails ? "\n%d FAILED\n" : "\nall capture checks passed\n", fails);
    return fails ? 1 : 0;
}
