// MERCY - capture: bake what you're hearing, live.
// The plugin keeps a rolling history of its own output (allocated the first time the bake panel opens), plus
// where every bar line fell. "grab" copies the last N whole bars out of it; "record" starts on the next bar and
// stops on a bar. Bar lines are stored output-aligned (shifted by MERCY's latency), so beat 1 is sample 0.
// Live audio already carries the previous bar's tail, so a capture of a looping groove loops seamlessly as it is:
// no fades, no tail folding (folding would double the tail).
// Threads: the audio thread is the only writer of the ring, the bar lines and the record positions; the
// interface reads them (acquire/release on the counters) and sends commands through atomics.
#pragma once
#include <atomic>
#include <vector>
#include <memory>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace mercy {

struct CaptureTime {          // what the host told us at the start of this block
    bool playing = false, valid = false;
    double posInBar = 0;      // beats since the last bar line
    double bpm = 0;
    float beatsPerBar = 4;
};

struct CaptureBuf {
    explicit CaptureBuf(double sampleRate) : sr(sampleRate) {
        const double secs = sampleRate <= 96000.5 ? 66.0 : 33.0;        // 16 bars down to ~60 bpm
        cap = (uint32_t)(secs * sampleRate);
        L.assign(cap, 0.f); R.assign(cap, 0.f);
        peakCap = cap / kBin + 1; peaks.assign(peakCap, 0.f);
        for (auto& m : marks) m.store(0);
    }
    static constexpr uint32_t kBin = 512, kMarks = 256;
    double sr; uint32_t cap, peakCap;
    std::vector<float> L, R, peaks;
    std::atomic<uint64_t> written {0};            // frames written so far (audio thread)
    std::atomic<uint64_t> marks[kMarks];          // bar lines, output-aligned absolute frames
    std::atomic<uint32_t> nMarks {0};
    // audio-thread scratch
    float binPeak = 0; uint32_t binFill = 0;
    uint64_t lastMarkTimeline = 0; bool haveLastMark = false;

    uint64_t mark(uint32_t i) const { return marks[i % kMarks].load(std::memory_order_relaxed); }
    // copy [a, b) (absolute frames) out of the ring. Only valid while b <= written and written - a <= cap.
    bool copy(uint64_t a, uint64_t b, std::vector<float>& oL, std::vector<float>& oR) const {
        const uint64_t w = written.load(std::memory_order_acquire);
        if (b <= a || b > w || w - a > cap) return false;
        const size_t n = (size_t)(b - a); oL.resize(n); oR.resize(n);
        for (size_t i = 0; i < n; ++i) { const uint32_t k = (uint32_t)((a + i) % cap); oL[i] = L[k]; oR[i] = R[k]; }
        return written.load(std::memory_order_acquire) - a <= cap;          // not overwritten while copying
    }
};

class Capture {
public:
    enum RecState { kIdle = 0, kArmed, kRec, kStopReq, kDone };
    struct Clip {
        std::vector<float> L, R; double sr = 0;
        int bars = 0;            // 0 = not bar-synced (free)
        double bpm = 0, seconds = 0;
    };

    // ---- interface thread
    void ensure(double sr) {                  // allocate the history (first time the bake panel opens)
        if (sr < 1000) return;
        CaptureBuf* b = buf.load(std::memory_order_acquire);
        if (b && std::fabs(b->sr - sr) < 0.5) return;
        owned.emplace_back(new CaptureBuf(sr));      // older buffers stay alive until the session ends
        buf.store(owned.back().get(), std::memory_order_release);
        recState.store(kIdle);
    }
    bool listening() const { return buf.load(std::memory_order_acquire) != nullptr; }
    CaptureBuf* history() const { return buf.load(std::memory_order_acquire); }
    double tempo() const { return bpm.load(); }
    float barBeats() const { return beatsPerBar.load(); }
    bool transportOn() const { return playing.load(); }

    // the last `bars` whole bars that have fully played (output-aligned). false if not enough history.
    bool lastBars(int bars, uint64_t& a, uint64_t& b) const {
        CaptureBuf* c = history(); if (!c || bars < 1) return false;
        const uint64_t w = c->written.load(std::memory_order_acquire);
        const uint32_t n = c->nMarks.load(std::memory_order_acquire);
        for (int k = (int)n - 1; k >= bars && k >= (int)n - (int)CaptureBuf::kMarks + bars; --k) {
            const uint64_t e = c->mark((uint32_t)k), s = c->mark((uint32_t)(k - bars));
            if (e > w || s >= e) continue;
            if (w - s > c->cap) return false;
            a = s; b = e; return true;
        }
        return false;
    }
    // "grab": the last N bars you heard. Falls back to N bars of time at the last tempo (120 if none) when
    // the host isn't giving bar lines (transport stopped).
    bool grab(int bars, Clip& out) const {
        CaptureBuf* c = history(); if (!c) return false;
        uint64_t a = 0, b = 0;
        out.sr = c->sr; out.bpm = tempo();
        if (transportOn() && lastBars(bars, a, b)) { out.bars = bars; }
        else {
            const double bp = tempo() > 1 ? tempo() : 120.0, bpb = barBeats() > 0 ? barBeats() : 4.f;
            const uint64_t len = (uint64_t)std::llround(bars * bpb * 60.0 / bp * c->sr);
            b = c->written.load(std::memory_order_acquire);
            const uint64_t have = std::min<uint64_t>(b, c->cap - (uint64_t)(c->sr * 0.25));
            a = b - std::min(len, have); out.bars = 0;
        }
        if (!c->copy(a, b, out.L, out.R)) return false;
        out.seconds = (double)out.L.size() / c->sr;
        return !out.L.empty() && peakOf(out) > 1e-5f;     // nothing audible: nothing to save
    }
    void arm(int bars) { recBars.store(bars); recFree.store(false); recState.store(kArmed); }
    void stop() {
        int s = recState.load();
        if (s == kArmed) recState.compare_exchange_strong(s, kIdle);
        else if (s == kRec) recState.compare_exchange_strong(s, kStopReq);
    }
    int state() const { return recState.load(); }
    int recordBars() const { return recBars.load(); }
    // where the take is so far (for drawing): [start, now]
    bool takeSoFar(uint64_t& a, uint64_t& b) const {
        CaptureBuf* c = history(); const int s = recState.load();
        if (!c || (s != kRec && s != kStopReq && s != kDone)) return false;
        a = recStart.load(); b = s == kDone ? recEnd.load() : c->written.load(std::memory_order_acquire);
        return b > a;
    }
    int barsSoFar() const {   // whole bars recorded so far (bar-synced takes)
        CaptureBuf* c = history(); if (!c) return 0;
        return (int)(c->nMarks.load(std::memory_order_acquire) - recStartMark.load());
    }
    // call from idle(): a finished take, once its last sample has been written
    bool poll(Clip& out) {
        if (recState.load() != kDone) return false;
        CaptureBuf* c = history(); if (!c) { recState.store(kIdle); return false; }
        const uint64_t a = recStart.load(), b = recEnd.load();
        if (c->written.load(std::memory_order_acquire) < b) return false;      // the last bar's audio is still coming
        recState.store(kIdle);
        out = Clip(); out.sr = c->sr; out.bpm = tempo();
        if (!c->copy(a, b, out.L, out.R)) return false;
        if (recFree.load()) {   // free take: trim the silence after the last sound (keep 60 ms of air)
            size_t last = 0; for (size_t i = 0; i < out.L.size(); ++i) if (std::fabs(out.L[i]) > 1e-3f || std::fabs(out.R[i]) > 1e-3f) last = i;
            const size_t keep = std::min(out.L.size(), last + (size_t)(0.06 * out.sr));
            out.L.resize(keep); out.R.resize(keep); out.bars = 0;
        } else out.bars = (int)std::lround((double)(b - a) / (c->sr * 60.0 / std::max(1.0, out.bpm) * std::max(1.f, barBeats())));
        out.seconds = (double)out.L.size() / out.sr;
        return out.L.size() > 16 && peakOf(out) > 1e-5f;
    }
    static float peakOf(const Clip& c) {
        float p = 0; for (size_t i = 0; i < c.L.size(); ++i) p = std::max(p, std::max(std::fabs(c.L[i]), std::fabs(c.R[i])));
        return p;
    }

    // ---- audio thread: the plugin's output, after processing
    void process(const float* l, const float* r, uint32_t n, const CaptureTime& t, uint32_t latency) {
        CaptureBuf* c = buf.load(std::memory_order_acquire);
        if (!c || n == 0) return;
        const uint64_t w = c->written.load(std::memory_order_relaxed);
        playing.store(t.playing && t.valid);
        if (t.valid && t.bpm > 1) { bpm.store(t.bpm); beatsPerBar.store(t.beatsPerBar > 0 ? t.beatsPerBar : 4.f); }
        // 1. the audio into the ring + the peak history
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t k = (uint32_t)((w + i) % c->cap);
            c->L[k] = l[i]; c->R[k] = r[i];
            c->binPeak = std::max(c->binPeak, std::max(std::fabs(l[i]), std::fabs(r[i])));
            if (++c->binFill >= CaptureBuf::kBin) {
                c->peaks[(uint32_t)(((w + i) / CaptureBuf::kBin) % c->peakCap)] = std::isfinite(c->binPeak) ? c->binPeak : 0.f;
                c->binPeak = 0; c->binFill = 0;
            }
        }
        // 2. a bar line inside this block?
        int64_t barAt = -1;
        if (t.playing && t.valid && t.bpm > 1) {
            const double bpb = t.beatsPerBar > 0 ? t.beatsPerBar : 4.0, spb = 60.0 / t.bpm * c->sr;
            double pos = std::fmod(t.posInBar, bpb); if (pos < 0) pos += bpb;
            double toNext = (pos < 1e-7 || bpb - pos < 1e-7) ? 0.0 : (bpb - pos) * spb;
            if (toNext < n) barAt = (int64_t)std::llround(toNext);
            if (barAt >= (int64_t)n) barAt = -1;
            if (barAt >= 0) {
                const uint64_t tl = w + (uint64_t)barAt;                       // timeline (input) frame of the bar line
                if (c->haveLastMark && tl - c->lastMarkTimeline < (uint64_t)(spb * bpb * 0.5)) barAt = -1;   // the same bar line twice
                else {
                    c->lastMarkTimeline = tl; c->haveLastMark = true;
                    const uint32_t m = c->nMarks.load(std::memory_order_relaxed);
                    c->marks[m % CaptureBuf::kMarks].store(tl + latency, std::memory_order_relaxed);   // where it is in the output
                    c->nMarks.store(m + 1, std::memory_order_release);
                }
            }
        }
        // 3. recording
        int s = recState.load(std::memory_order_acquire);
        const bool synced = t.playing && t.valid && t.bpm > 1;
        if (s == kArmed) {
            if (synced) {
                if (barAt >= 0) {
                    recStart.store(w + (uint64_t)barAt + latency); recStartMark.store(c->nMarks.load(std::memory_order_relaxed) - 1);
                    recFree.store(false); recState.store(kRec); s = kRec;
                }
            } else {  // transport stopped: start on the first sound (10 ms early so the attack is whole)
                for (uint32_t i = 0; i < n; ++i)
                    if (std::fabs(l[i]) > 1e-3f || std::fabs(r[i]) > 1e-3f) {
                        const uint64_t back = (uint64_t)(0.01 * c->sr), at = w + i;
                        recStart.store(at > back ? at - back : 0); recFree.store(true); recState.store(kRec); s = kRec; break;
                    }
            }
        }
        if (s == kRec || s == kStopReq) {
            const uint64_t now = w + n, start = recStart.load();
            bool end = false; uint64_t endAt = now;
            if (!recFree.load()) {
                const uint32_t m = c->nMarks.load(std::memory_order_relaxed), m0 = recStartMark.load();
                const int bars = recBars.load();
                if (barAt >= 0 && m - 1 > m0 && ((bars > 0 && (int)(m - 1 - m0) >= bars) || s == kStopReq)) { end = true; endAt = c->mark(m - 1); }
                else if (!synced) end = true;                                  // transport stopped mid-take: keep what we have
            } else if (s == kStopReq) end = true;
            if (now > start && now - start >= c->cap - (uint64_t)(c->sr * 2)) end = true;     // the history is full
            if (end) {
                recEnd.store(std::max(endAt, start + 1)); recState.store(kDone); }
        }
        c->written.store(w + n, std::memory_order_release);
    }

private:
    std::atomic<CaptureBuf*> buf {nullptr};
    std::vector<std::unique_ptr<CaptureBuf>> owned;
    std::atomic<int> recState {kIdle}, recBars {4};
    std::atomic<bool> recFree {false}, playing {false};
    std::atomic<uint64_t> recStart {0}, recEnd {0};
    std::atomic<uint32_t> recStartMark {0};
    std::atomic<double> bpm {0};
    std::atomic<float> beatsPerBar {4};
};

} // namespace mercy
