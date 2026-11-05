// MERCY drum processor - core DSP (framework independent)
#pragma once
#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <vector>
#include "MercyMath.hpp"
#if defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__)
#include <xmmintrin.h>
#endif

namespace mercy {

static constexpr double kPi = 3.14159265358979323846;

// Denormals flushed to zero while MERCY processes, the same way on x86 and ARM, whatever the host has set
// (keeps the CPU load flat and makes Windows and Mac renders bit-identical).
struct DenormalGuard {
#if defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__)
    unsigned old;
    DenormalGuard() : old(_mm_getcsr()) { _mm_setcsr(old | 0x8040); } // FTZ + DAZ
    ~DenormalGuard() { _mm_setcsr(old); }
#elif defined(__aarch64__)
    uint64_t old;
    DenormalGuard() { __asm__ __volatile__("mrs %0, fpcr" : "=r"(old)); const uint64_t n = old | (1ull << 24); __asm__ __volatile__("msr fpcr, %0" : : "r"(n)); } // FZ
    ~DenormalGuard() { __asm__ __volatile__("msr fpcr, %0" : : "r"(old)); }
#endif
};

inline float dbToLin(float db) { return dm::pow(10.0f, db * 0.05f); }
inline float linToDb(float l) { return 20.0f * dm::log10(std::max(l, 1e-9f)); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// ---------------------------------------------------------------------------------------------
// Parameters

enum Param : uint32_t {
    pEra = 0, pSquash, pDirt, pAge, pInput, pMix, pOutput, pAutoGain, pBypass, pQuality,
    // sampler
    pSmpOn, pSmpMode, pSmpRate, pSmpBits, pSmpCrunch, pSmpAA,
    // preamp
    pPreOn, pPreFlavor, pPreDrive, pPreLowEmph, pPreColor,
    // squash
    pSqOn, pSqThresh, pSqRatio, pSqAttack, pSqRelease, pSqBlend, pSqSC,
    // tone
    pToneOn, pToneLowFreq, pToneLowAmt, pToneBiteFreq, pToneBiteAmt, pToneDepth,
    // clip
    pClipOn, pClipMode, pClipAmount, pClipCeiling,
    // gate / width
    pGwOn, pGate, pGateHold, pWidth, pMonoLows,
    // transient (Ableton-style transient envelope)
    pTrOn, pTrEnv, pTrDetect,
    // shape (drum transient shaper)
    pShOn, pShMode, pShChar, pShAttack, pShSustain, pShFocus,
    // output filters (soft 12 dB/oct, after auto gain)
    pHpf, pLpf,
    // reverb (v2.0): plate / room / gated, tuned for drums
    pRvOn, pRvMode, pRvDecay, pRvPre, pRvTone, pRvDuck, pRvMix,
    // weight (v2.0): resonant low-end bump + low harmonics
    pWtOn, pWtFreq, pWtAmount, pWtFocus, pWtHarm,
    // outputs (meters)
    pMeterIn, pMeterOut, pMeterGR,
    kNumParams
};

static constexpr uint32_t kFirstOutputParam = pMeterIn;

struct ParamInfo {
    const char* symbol;
    const char* name;
    const char* unit;
    float min, max, def;
    bool isInt;
    bool isBool;
};

// defaults here = FANTASY era
static const ParamInfo kParamInfo[kNumParams] = {
    {"era", "Era", "", 0, 2, 0, true, false},
    {"squash", "Squash", "%", 0, 1, 0.5f, false, false},
    {"dirt", "Dirt", "%", 0, 1, 0.5f, false, false},
    {"age", "Age", "%", 0, 1, 0.3f, false, false},
    {"input", "Input", "dB", -24, 24, 0, false, false},
    {"mix", "Mix", "%", 0, 1, 1, false, false},
    {"output", "Output", "dB", -24, 24, 0, false, false},
    {"autogain", "Auto Gain", "", 0, 1, 1, true, true},
    {"bypass", "Bypass", "", 0, 1, 0, true, true},
    {"quality", "Quality", "", 0, 1, 1, true, false},

    {"smp_on", "Sampler On", "", 0, 1, 1, true, true},
    {"smp_mode", "Sampler Mode", "", 0, 2, 1, true, false},
    {"smp_rate", "Sampler Rate", "kHz", 4, 44.1f, 32, false, false},
    {"smp_bits", "Sampler Bits", "bit", 4, 16, 14, false, false},
    {"smp_crunch", "Sampler Crunch", "%", 0, 1, 0.3f, false, false},
    {"smp_aa", "Sampler AA Filter", "", 0, 1, 0, true, true},

    {"pre_on", "Preamp On", "", 0, 1, 1, true, true},
    {"pre_flavor", "Preamp Flavor", "", 0, 3, 1, true, false},
    {"pre_drive", "Preamp Drive", "dB", 0, 24, 6, false, false},
    {"pre_lowemph", "Preamp Low Emphasis", "%", 0, 1, 0.5f, false, false},
    {"pre_color", "Preamp Color", "%", 0, 1, 0.5f, false, false},

    {"sq_on", "Squash On", "", 0, 1, 1, true, true},
    {"sq_thresh", "Squash Threshold", "dB", -40, 0, -22, false, false},
    {"sq_ratio", "Squash Ratio", ":1", 1, 20, 6, false, false},
    {"sq_attack", "Squash Attack", "ms", 0.1f, 30, 8, false, false},
    {"sq_release", "Squash Release", "ms", 20, 500, 120, false, false},
    {"sq_blend", "Squash Blend", "%", 0, 1, 0.55f, false, false},
    {"sq_sc", "Squash Sidechain HPF", "", 0, 1, 0, true, true},

    {"tone_on", "Tone On", "", 0, 1, 1, true, true},
    {"tone_lowfreq", "Tone Low Freq", "", 0, 1, 0, true, false},
    {"tone_lowamt", "Tone Low Amount", "dB", 0, 12, 4, false, false},
    {"tone_bitefreq", "Tone Bite Freq", "", 0, 1, 0, true, false},
    {"tone_biteamt", "Tone Bite Amount", "dB", 0, 12, 1.5f, false, false},
    {"tone_depth", "Tone Depth", "kHz", 2, 20, 15, false, false},

    {"clip_on", "Clip On", "", 0, 1, 1, true, true},
    {"clip_mode", "Clip Mode", "", 0, 1, 0, true, false},
    {"clip_amount", "Clip Amount", "%", 0, 1, 0.3f, false, false},
    {"clip_ceiling", "Clip Ceiling", "dB", -12, 0, -0.3f, false, false},

    {"gw_on", "Gate/Width On", "", 0, 1, 1, true, true},
    {"gate", "Gate Threshold", "dB", -80, 0, -80, false, false},
    {"gate_hold", "Gate Hold", "ms", 5, 500, 60, false, false},
    {"width", "Width", "%", 0, 2, 1, false, false},
    {"mono_lows", "Mono Lows", "", 0, 1, 1, true, true},
    {"tr_on", "Transient On", "", 0, 1, 1, true, true},
    {"tr_env", "Transient Envelope", "", 0, 100, 100, false, false},
    {"tr_detect", "Transient Detect", "%", 0, 1, 0.5f, false, false},
    {"sh_on", "Shape On", "", 0, 1, 1, true, true},
    {"sh_mode", "Shape Mode", "", 0, 2, 2, true, false},
    {"sh_char", "Shape Character", "", 0, 2, 1, true, false},
    {"sh_attack", "Shape Attack", "%", -100, 100, 0, false, false},
    {"sh_sustain", "Shape Sustain", "%", -100, 100, 0, false, false},
    {"sh_focus", "Shape Focus", "%", -100, 100, 0, false, false},
    {"hpf", "High Pass", "Hz", 20, 2000, 20, false, false},
    {"lpf", "Low Pass", "Hz", 500, 20000, 20000, false, false},
    // new modules start off, so sessions and presets from earlier versions sound exactly the same
    {"rv_on", "Reverb On", "", 0, 1, 0, true, true},
    {"rv_mode", "Reverb Mode", "", 0, 2, 0, true, false},
    {"rv_decay", "Reverb Decay", "s", 0.1f, 4, 1.2f, false, false},
    {"rv_pre", "Reverb Pre-delay", "ms", 0, 100, 10, false, false},
    {"rv_tone", "Reverb Tone", "%", 0, 1, 0.55f, false, false},
    {"rv_duck", "Reverb Duck", "%", 0, 1, 0.3f, false, false},
    {"rv_mix", "Reverb Mix", "%", 0, 1, 0.25f, false, false},
    {"wt_on", "Weight On", "", 0, 1, 0, true, true},
    {"wt_freq", "Weight Frequency", "Hz", 20, 200, 55, false, false},
    {"wt_amount", "Weight Amount", "dB", 0, 12, 4, false, false},
    {"wt_focus", "Weight Focus", "%", 0, 1, 0.5f, false, false},
    {"wt_harm", "Weight Harmonics", "%", 0, 1, 0.3f, false, false},

    {"meter_in", "Input Level", "dB", -60, 6, -60, false, false},
    {"meter_out", "Output Level", "dB", -60, 6, -60, false, false},
    {"meter_gr", "Gain Reduction", "dB", 0, 30, 0, false, false},
};

// Era starting points for the advanced page (indices pSmpOn .. pMonoLows)
static constexpr uint32_t kFirstAdvParam = pSmpOn;
static constexpr uint32_t kLastAdvParam = pMonoLows;
static constexpr uint32_t kNumAdvParams = kLastAdvParam - kFirstAdvParam + 1;

static const float kEraDefaults[3][kNumAdvParams] = {
    // FANTASY: warm, lush, big lows
    {1, 1, 32, 14, 0.3f, 0,   1, 1, 6, 0.5f, 0.5f,   1, -22, 6, 8, 120, 0.55f, 0,
     1, 0, 4, 0, 1.5f, 15,   1, 0, 0.3f, -0.3f,   1, -80, 60, 1.0f, 1},
    // THRONE: tight, loud, bite
    {1, 2, 40, 14, 0.2f, 0,   1, 0, 8, 0.35f, 0.4f,  1, -24, 12, 3, 70, 0.65f, 0,
     1, 1, 3, 0, 3, 18,      1, 1, 0.4f, -0.3f,   1, -80, 60, 0.9f, 1},
    // YEEZUS: industrial, clipped, raw
    {1, 0, 22, 10, 0.6f, 0,   1, 3, 14, 0.6f, 0.7f,  1, -30, 20, 1, 50, 0.8f, 0,
     1, 0, 5, 0, 2, 9,       1, 1, 0.6f, -1.0f,   1, -35, 40, 0.6f, 1},
};

// v2.0 modules: era starting points (indices pRvOn .. pWtHarm); applied when an era tab is clicked
static constexpr uint32_t kFirstNewParam = pRvOn;
static constexpr uint32_t kLastNewParam = pWtHarm;
static constexpr uint32_t kNumNewParams = kLastNewParam - kFirstNewParam + 1;
static const float kEraNew[3][kNumNewParams] = {
    // on mode decay pre  tone  duck  mix     on freq amt focus harm
    {1, 0, 1.8f, 15, 0.6f, 0.4f, 0.22f,   1, 50, 3, 0.4f, 0.25f},   // FANTASY: plate + a little weight
    {1, 1, 0.7f, 5, 0.5f, 0.3f, 0.2f,     0, 60, 4, 0.5f, 0.3f},    // THRONE: room
    {1, 2, 0.6f, 0, 0.45f, 0.1f, 0.3f,    1, 45, 6, 0.7f, 0.6f},    // YEEZUS: gated + heavy weight
};

// Stage ids for chain order
enum Stage { kStSampler = 0, kStPreamp, kStSquash, kStTone, kStClip, kStGateWidth, kStTransient, kStShape, kStReverb, kStWeight, kNumStages };
static const int kDefaultOrder[kNumStages] = {kStTransient, kStShape, kStSampler, kStPreamp, kStSquash, kStTone, kStWeight, kStReverb, kStClip, kStGateWidth};
static const char* const kDefaultOrderString = "6701239845";

// Accepts chain orders from every version (v1.0: 6 stages, v1.1: 7, v1.2-v1.9: 8, v2.0: 10).
// Missing stages are inserted where the default order has them. Returns false if invalid.
inline bool normalizeOrder(const char* s, int out[kNumStages]) {
    if (s == nullptr) return false;
    char buf[kNumStages + 1] = {};
    size_t n = 0;
    while (s[n] >= '0' && s[n] <= '9' && n < kNumStages) { buf[n] = s[n]; ++n; }
    if (s[n] >= '0' && s[n] <= '9') return false;
    if (n == 6) { // v1.0
        char t[kNumStages + 1] = {'6', '7'};
        std::memcpy(t + 2, buf, n); std::memcpy(buf, t, 8); n = 8;
    } else if (n == 7) { // v1.1: put shape right after transient
        char t[kNumStages + 1] = {}; size_t k = 0;
        for (size_t i = 0; i < n; ++i) { t[k++] = buf[i]; if (buf[i] == '6') t[k++] = '7'; }
        if (k != 8) return false;
        std::memcpy(buf, t, 8); n = 8;
    }
    if (n == 8) { // v1.2-v1.9: weight + reverb go right before clip
        char t[kNumStages + 1] = {}; size_t k = 0;
        for (size_t i = 0; i < n; ++i) { if (buf[i] == '4') { t[k++] = '9'; t[k++] = '8'; } t[k++] = buf[i]; }
        if (k != kNumStages) return false;
        std::memcpy(buf, t, kNumStages); n = kNumStages;
    }
    if (n != kNumStages) return false;
    bool used[kNumStages] = {};
    for (int i = 0; i < kNumStages; ++i) {
        const int v = buf[i] - '0';
        if (v < 0 || v >= kNumStages || used[v]) return false;
        used[v] = true; out[i] = v;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Building blocks

struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;
    bool active = true;

    void reset() { z1 = z2 = 0; }
    inline float process(float x) {
        if (!active) return x;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float)y;
    }
    void setNorm(double nb0, double nb1, double nb2, double na0, double na1, double na2) {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
    void lowpass(double fs, double f, double q) {
        f = std::min(f, fs * 0.49);
        const double w = 2 * kPi * f / fs, c = dm::cos(w), al = dm::sin(w) / (2 * q);
        setNorm((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
        active = true;
    }
    void highpass(double fs, double f, double q) {
        f = std::min(f, fs * 0.49);
        const double w = 2 * kPi * f / fs, c = dm::cos(w), al = dm::sin(w) / (2 * q);
        setNorm((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        active = true;
    }
    void peak(double fs, double f, double q, double gdb) {
        f = std::min(f, fs * 0.49);
        const double A = dm::pow(10.0, gdb / 40), w = 2 * kPi * f / fs, c = dm::cos(w), al = dm::sin(w) / (2 * q);
        setNorm(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        active = std::fabs(gdb) > 0.01;
    }
    void lowshelf(double fs, double f, double gdb, double S = 0.8) {
        f = std::min(f, fs * 0.49);
        const double A = dm::pow(10.0, gdb / 40), w = 2 * kPi * f / fs, c = dm::cos(w), s = dm::sin(w);
        const double al = s / 2 * std::sqrt((A + 1 / A) * (1 / S - 1) + 2), sa = 2 * std::sqrt(A) * al;
        setNorm(A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
        active = std::fabs(gdb) > 0.01;
    }
    void highshelf(double fs, double f, double gdb, double S = 0.8) {
        f = std::min(f, fs * 0.49);
        const double A = dm::pow(10.0, gdb / 40), w = 2 * kPi * f / fs, c = dm::cos(w), s = dm::sin(w);
        const double al = s / 2 * std::sqrt((A + 1 / A) * (1 / S - 1) + 2), sa = 2 * std::sqrt(A) * al;
        setNorm(A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
        active = std::fabs(gdb) > 0.01;
    }
};

// Polyphase IIR half-band (2x up / down), zero latency, ~110 dB rejection
struct HalfbandCoefs {
    static constexpr int N = 8;
    double c[N];
    HalfbandCoefs() {
        const double tbw = 0.05;
        double k = dm::tan((1 - tbw * 2) * kPi / 4);
        k *= k;
        const double kksqrt = dm::pow(1 - k * k, 0.25);
        const double e = 0.5 * (1 - kksqrt) / (1 + kksqrt), e2 = e * e, e4 = e2 * e2;
        const double q = e * (1 + e4 * (2 + e4 * (15 + 150 * e4)));
        const int order = N * 2 + 1;
        for (int idx = 0; idx < N; ++idx) {
            const int cc = idx + 1;
            int i = 0, j = 1; double acc = 0, t;
            do { t = dm::pow(q, i * (i + 1)) * dm::sin((i * 2 + 1) * cc * kPi / order) * j; acc += t; j = -j; ++i; } while (std::fabs(t) > 1e-100 && i < 100);
            const double num = acc * dm::pow(q, 0.25);
            i = 1; j = -1; acc = 0;
            do { t = dm::pow(q, i * i) * dm::cos(i * 2 * cc * kPi / order) * j; acc += t; j = -j; ++i; } while (std::fabs(t) > 1e-100 && i < 100);
            const double den = acc + 0.5;
            const double ww = num / den, wwsq = ww * ww;
            const double x = std::sqrt((1 - wwsq * k) * (1 - wwsq / k)) / (1 + wwsq);
            c[idx] = (1 - x) / (1 + x);
        }
    }
};
static const HalfbandCoefs kHB;

struct AllpassChain {
    static constexpr int M = HalfbandCoefs::N / 2;
    double xp[M] = {}, yp[M] = {};
    const double* coefs[M];
    void init(int phase) { for (int i = 0; i < M; ++i) coefs[i] = &kHB.c[i * 2 + phase]; reset(); }
    void reset() { for (int i = 0; i < M; ++i) xp[i] = yp[i] = 0; }
    inline double process(double v) {
        for (int i = 0; i < M; ++i) {
            const double o = *coefs[i] * (v - yp[i]) + xp[i];
            xp[i] = v; yp[i] = o; v = o;
        }
        return v;
    }
};

struct Upsampler2x {
    AllpassChain a0, a1;
    void init() { a0.init(0); a1.init(1); }
    void reset() { a0.reset(); a1.reset(); }
    inline void process(float x, float& y0, float& y1) { y0 = (float)a0.process(x); y1 = (float)a1.process(x); }
};

struct Downsampler2x {
    AllpassChain a0, a1;
    double prevOdd = 0;
    void init() { a0.init(0); a1.init(1); prevOdd = 0; }
    void reset() { a0.reset(); a1.reset(); prevOdd = 0; }
    inline float process(float x0, float x1) {
        const double r = 0.5 * (a0.process(x0) + a1.process(prevOdd));
        prevOdd = x1;
        return (float)r;
    }
};

struct OnePoleSmooth {
    float v = 0, coef = 0;
    void setTime(double fs, double ms) { coef = (float)dm::exp(-1.0 / (fs * ms * 0.001)); }
    inline float process(float target) { v = target + coef * (v - target); return v; }
};

// ---------------------------------------------------------------------------------------------
// Reverb for drums (v2.0): a plate-style figure-eight tank (after Dattorro) shared by three modes.
//   plate: full-size dense tank, bright        room: small tank + early reflections, short
//   gated: long dense tank behind a gate that opens on each hit and slams shut after "length"
// Everything is plain arithmetic in a fixed order (no transcendental calls per sample), so Windows
// and Mac render it bit for bit the same.

struct RvLine {
    std::vector<float> b; uint32_t mask = 0, w = 0;
    void init(size_t n) { size_t s = 16; while (s < n + 4) s <<= 1; b.assign(s, 0.f); mask = (uint32_t)s - 1; w = 0; }
    void clear() { std::fill(b.begin(), b.end(), 0.f); w = 0; }
    inline void push(float x) { b[w] = x; w = (w + 1) & mask; }
    inline float tap(uint32_t d) const { return b[(w - 1 - d) & mask]; }       // d = 0: newest
    inline float tapFrac(float d) const {
        const uint32_t i = (uint32_t)d; const float f = d - (float)i;
        const float a = b[(w - 1 - i) & mask], c = b[(w - 2 - i) & mask];
        return a + (c - a) * f;
    }
};

// base lengths at 29761 Hz (Dattorro)
static const int kRvInAP[4] = {142, 107, 379, 277};
static const int kRvTank[8] = {672, 4453, 1800, 3720, 908, 4217, 2656, 3163}; // L: ap1 dl1 ap2 dl2, R: same
static const int kRvTapL[7] = {266, 2974, 1913, 1996, 1990, 187, 1066};
static const int kRvTapR[7] = {353, 3627, 1228, 2673, 2111, 335, 121};

struct DrumReverb {

    double fs = 44100;
    RvLine pre, inAP[4], tank[8];
    int lenIn[4] = {}, lenTank[8] = {}, tapL[7] = {}, tapR[7] = {};
    float scaleCur = -1;
    float maxExc = 16;
    // per-block settings
    int mode = 0;
    float preLen = 0, bw = 0.9f, damp = 0.3f, decay = 0.5f, mix = 0.25f, duck = 0.3f;
    float inDiff1 = 0.75f, inDiff2 = 0.625f, decDiff1 = 0.7f, decDiff2 = 0.5f, exc = 8;
    float gateLen = 0, gateAtk = 0, gateRel = 0, envAtk = 0, envRel = 0, envSAtk = 0, envSRel = 0, peakRel = 0, duckAtk = 0, duckRel = 0;
    int erN = 0; int erD[6] = {}; float erG[6] = {};
    Biquad lowCut[2];
    // state
    float bwZ = 0, dampL = 0, dampR = 0, lfoL = 0, lfoR = 0.5f, lfoStep = 0;
    float env = 0, envS = 0, peak = 0, gate = 0, duckEnv = 0; int gateHold = 0, sinceHit = 1 << 28;
    float lastL = 0, lastR = 0; // tank feedback (cross-coupled)

    void setSampleRate(double sr) {
        fs = sr;
        const double k = fs / 29761.0;
        pre.init((size_t)(fs * 0.105) + 8);
        maxExc = (float)(16.0 * k);
        for (int i = 0; i < 4; ++i) inAP[i].init((size_t)(kRvInAP[i] * k) + 8);
        for (int i = 0; i < 8; ++i) tank[i].init((size_t)(kRvTank[i] * k + maxExc) + 8);
        scaleCur = -1;
        lfoStep = (float)(4.0 * 0.6 / fs);   // triangle 0 -> 1 -> 0: 0.6 Hz
        for (int ch = 0; ch < 2; ++ch) lowCut[ch].highpass(fs, 200, 0.6);
        reset();
    }
    void reset() {
        pre.clear(); for (auto& a : inAP) a.clear(); for (auto& t : tank) t.clear();
        for (auto& b : lowCut) b.reset();
        bwZ = dampL = dampR = 0; lfoL = 0; lfoR = 0.5f; env = envS = peak = gate = duckEnv = 0; gateHold = 0; sinceHit = 1 << 28;
        lastL = lastR = 0;
    }
    // tank size (fraction of the plate): recomputes the delay lengths (no reallocation)
    void setScale(float sc) {
        if (sc == scaleCur) return;
        scaleCur = sc;
        const double k = fs / 29761.0;
        for (int i = 0; i < 4; ++i) lenIn[i] = std::max(1, (int)std::lround(kRvInAP[i] * k * std::max(0.5f, sc)));
        for (int i = 0; i < 8; ++i) lenTank[i] = std::max(2, (int)std::lround(kRvTank[i] * k * sc));
        for (int i = 0; i < 7; ++i) { tapL[i] = std::max(1, (int)std::lround(kRvTapL[i] * k * sc)); tapR[i] = std::max(1, (int)std::lround(kRvTapR[i] * k * sc)); }
    }
    void configure(int m, float decaySec, float preMs, float tone, float duckAmt, float mixAmt) {
        mode = m;
        const float sc = m == 0 ? 1.0f : (m == 1 ? 0.42f : 0.75f);
        setScale(sc);
        // reverb time: plate/room = the decay control; gated = a long dense tail behind the gate
        const float rt = m == 2 ? 3.0f : decaySec;
        // four decay multiplies per trip around the tank, each about a quarter of the loop
        const double seg = (double)(lenTank[0] + lenTank[1] + lenTank[2] + lenTank[3] + lenTank[4] + lenTank[5] + lenTank[6] + lenTank[7]) / 4.0 / fs;
        decay = (float)std::min(0.9995, dm::pow(10.0, -3.0 * seg / std::max(0.05, (double)rt)));
        preLen = (float)(clampf(preMs, 0, 100) * 0.001 * fs);
        // tone: input bandwidth 3 -> 16 kHz, tank damping 2 -> 12 kHz (one-pole)
        const double fIn = 3000.0 * dm::pow(16000.0 / 3000.0, (double)clampf(tone, 0, 1));
        const double fDp = (m == 1 ? 1600.0 : 2000.0) * dm::pow(6.0, (double)clampf(tone, 0, 1));
        bw = (float)(1.0 - dm::exp(-2 * kPi * std::min(fIn, fs * 0.45) / fs));
        damp = (float)dm::exp(-2 * kPi * std::min(fDp, fs * 0.45) / fs);
        inDiff1 = m == 1 ? 0.7f : 0.75f; inDiff2 = m == 1 ? 0.55f : 0.625f;
        decDiff1 = 0.7f; decDiff2 = m == 1 ? 0.4f : 0.5f;
        exc = (float)(m == 1 ? 4.0 : 8.0) * (float)(fs / 29761.0);
        mix = clampf(mixAmt, 0, 1); duck = clampf(duckAmt, 0, 1);
        // gate: length follows the decay control (0.1 s -> 140 ms, 1.2 s -> 360 ms, 4 s -> 920 ms)
        gateLen = (float)((0.12 + 0.2 * clampf(decaySec, 0.1f, 4)) * fs);
        gateAtk = (float)dm::exp(-1.0 / (fs * 0.0015));
        gateRel = (float)dm::exp(-1.0 / (fs * 0.012));
        envAtk = (float)dm::exp(-1.0 / (fs * 0.0005));
        envRel = (float)dm::exp(-1.0 / (fs * 0.015));
        envSAtk = (float)dm::exp(-1.0 / (fs * 0.030));
        envSRel = (float)dm::exp(-1.0 / (fs * 0.150));
        peakRel = (float)dm::exp(-1.0 / (fs * 0.6));
        duckAtk = (float)dm::exp(-1.0 / (fs * 0.002));
        duckRel = (float)dm::exp(-1.0 / (fs * 0.160));
        // room: six early reflections off the pre-delay line
        erN = m == 1 ? 6 : 0;
        static const float erMs[6] = {4.3f, 7.1f, 11.3f, 13.9f, 17.2f, 21.7f}, erGain[6] = {0.62f, -0.55f, 0.48f, -0.42f, 0.34f, -0.28f};
        for (int i = 0; i < erN; ++i) { erD[i] = (int)std::lround(erMs[i] * 0.001 * fs * (0.7 + 0.3 * clampf(decaySec, 0.1f, 4) / 4.0)); erG[i] = erGain[i]; }
    }
    static inline float allpass(RvLine& d, int len, float g, float x) {
        const float y = d.tap((uint32_t)(len - 1));
        const float v = x + g * y; d.push(v);
        return y - g * v;
    }
    inline void process(float& l, float& r) {
        // input: mono sum, low cut (keep the kick out), pre-delay, bandwidth
        float x = 0.5f * (lowCut[0].process(l) + lowCut[1].process(r));
        const float ax = std::fabs(x);
        // hit detector (gated mode) + ducking follower, on the dry input
        env = ax > env ? ax + envAtk * (env - ax) : ax + envRel * (env - ax);
        envS = ax > envS ? ax + envSAtk * (envS - ax) : ax + envSRel * (envS - ax);
        peak = env > peak ? env : env + peakRel * (peak - env);
        duckEnv = ax > duckEnv ? ax + duckAtk * (duckEnv - ax) : ax + duckRel * (duckEnv - ax);
        pre.push(x);
        float in = preLen < 1.f ? x : pre.tapFrac(preLen - 1.f);
        bwZ += bw * (in - bwZ); in = bwZ;
        // input diffusion
        in = allpass(inAP[0], lenIn[0], inDiff1, in);
        in = allpass(inAP[1], lenIn[1], inDiff1, in);
        in = allpass(inAP[2], lenIn[2], inDiff2, in);
        in = allpass(inAP[3], lenIn[3], inDiff2, in);
        // triangle LFOs for the modulated tank allpasses
        lfoL += lfoStep; if (lfoL >= 2.f) lfoL -= 2.f;
        lfoR += lfoStep; if (lfoR >= 2.f) lfoR -= 2.f;
        const float triL = lfoL < 1.f ? lfoL : 2.f - lfoL, triR = lfoR < 1.f ? lfoR : 2.f - lfoR;
        // left half: modulated allpass -> delay -> damping -> decay -> allpass -> delay -> decay
        {
            float a = in + decay * lastR;
            const float dd = (float)(lenTank[0] - 1) + exc * triL;
            const float y = tank[0].tapFrac(dd); const float v = a - decDiff1 * y; tank[0].push(v); a = y + decDiff1 * v;
            tank[1].push(a);
            float b = tank[1].tap((uint32_t)(lenTank[1] - 1));
            dampL = b + damp * (dampL - b); b = dampL * decay;
            b = allpass(tank[2], lenTank[2], decDiff2, b);
            tank[3].push(b);
            lastL = tank[3].tap((uint32_t)(lenTank[3] - 1));
        }
        {
            float a = in + decay * lastL;
            const float dd = (float)(lenTank[4] - 1) + exc * triR;
            const float y = tank[4].tapFrac(dd); const float v = a - decDiff1 * y; tank[4].push(v); a = y + decDiff1 * v;
            tank[5].push(a);
            float b = tank[5].tap((uint32_t)(lenTank[5] - 1));
            dampR = b + damp * (dampR - b); b = dampR * decay;
            b = allpass(tank[6], lenTank[6], decDiff2, b);
            tank[7].push(b);
            lastR = tank[7].tap((uint32_t)(lenTank[7] - 1));
        }
        // output taps
        float wl = tank[5].tap(tapL[0]) + tank[5].tap(tapL[1]) - tank[6].tap(tapL[2]) + tank[7].tap(tapL[3])
                 - tank[1].tap(tapL[4]) - tank[2].tap(tapL[5]) - tank[3].tap(tapL[6]);
        float wr = tank[1].tap(tapR[0]) + tank[1].tap(tapR[1]) - tank[2].tap(tapR[2]) + tank[3].tap(tapR[3])
                 - tank[5].tap(tapR[4]) - tank[6].tap(tapR[5]) - tank[7].tap(tapR[6]);
        wl *= 0.32f; wr *= 0.32f;
        for (int i = 0; i < erN; ++i) {
            const float e = pre.tap((uint32_t)std::min<int>((int)(pre.mask - 1), (int)preLen + erD[i])) * erG[i];
            if (i & 1) wr += e; else wl += e;
        }
        // gated: open on a hit, hold for "length", slam shut
        if (mode == 2) {
            if (sinceHit < (1 << 28)) ++sinceHit;
            const bool hit = env > 0.002f && env > 1.8f * envS && env > 0.6f * peak && sinceHit > (int)(0.08 * fs); // a real jump keys it
            if (hit) { gateHold = (int)gateLen; sinceHit = 0; }
            const float target = gateHold > 0 ? 1.f : 0.f;
            if (gateHold > 0) --gateHold;
            gate = target + (target > gate ? gateAtk : gateRel) * (gate - target);
            wl *= gate; wr *= gate;
        }
        // ducking: the tail steps back under each hit
        const float dg = 1.f / (1.f + duck * 6.f * duckEnv);
        const float g = mix * dg * (mode == 0 ? 3.1f : (mode == 1 ? 1.85f : 2.5f)); // wet at 100% sits a few dB under the dry
        l += wl * g; r += wr * g;
    }
};

// ---------------------------------------------------------------------------------------------
// Weight (v2.0): a tuned resonant bump in the low end (with the area under it tightened as the
// focus goes up), plus harmonics of that band so the low end reads on small speakers.
struct Weight {
    Biquad bump[2], under[2], band[2], hHp[2], hLp[2];
    float harm = 0.3f; bool underOn = false;
    void reset() { for (int ch = 0; ch < 2; ++ch) { bump[ch].reset(); under[ch].reset(); band[ch].reset(); hHp[ch].reset(); hLp[ch].reset(); } }
    void configure(double fs, float freq, float amountDb, float focus, float harmonics) {
        const double f = clampf(freq, 20, 200), F = clampf(focus, 0, 1);
        const double q = 0.7 + 3.3 * F;
        underOn = F > 0.05;
        harm = clampf(harmonics, 0, 1);
        for (int ch = 0; ch < 2; ++ch) {
            bump[ch].peak(fs, f, q, clampf(amountDb, 0, 12));
            if (underOn) under[ch].highpass(fs, std::max(15.0, f * 0.6 * F), 0.6);
            band[ch].lowpass(fs, f * 1.6, 0.707);
            hHp[ch].highpass(fs, f * 1.8, 0.707);
            hLp[ch].lowpass(fs, std::min(1200.0, fs * 0.45), 0.707);
        }
    }
    inline void process(float& l, float& r) {
        float* io[2] = {&l, &r};
        for (int ch = 0; ch < 2; ++ch) {
            float x = *io[ch];
            const float lb = band[ch].process(x);
            float y = bump[ch].process(x);
            if (underOn) y = under[ch].process(y);
            if (harm > 0.001f) {
                const float u = lb * 4.f;
                const float s = dm::tanh(u + 0.35f * u * u / (1.f + u * u)) * 0.25f;   // even + odd harmonics
                const float h = hLp[ch].process(hHp[ch].process(s));
                y += h * harm * 3.2f;
            } else { hLp[ch].process(hHp[ch].process(0.f)); }
            *io[ch] = y;
        }
    }
};

// ---------------------------------------------------------------------------------------------
// Engine

class Engine {
public:
    Engine() {
        for (uint32_t i = 0; i < kNumParams; ++i) params[i].store(kParamInfo[i].def);
        for (int i = 0; i < kNumStages; ++i) order[i].store(kDefaultOrder[i]);
        for (int ch = 0; ch < 2; ++ch) {
            preUp[ch].init(); preDown[ch].init(); clipUp[ch].init(); clipDown[ch].init();
        }
        setSampleRate(44100.0);
    }

    std::atomic<float> params[kNumParams];
    std::atomic<int> order[kNumStages];

    // look-ahead latency in samples (constant for a given sample rate)
    uint32_t latencySamples() const { return laLen; }

    // meters (read by plugin wrapper)
    float meterIn = -60, meterOut = -60, meterGR = 0;

    void setParam(uint32_t i, float v) { if (i < kNumParams) params[i].store(v); }
    float getParam(uint32_t i) const { return i < kNumParams ? params[i].load() : 0.f; }

    void setOrderString(const char* s) {
        int tmp[kNumStages];
        if (!normalizeOrder(s, tmp)) return;
        for (int i = 0; i < kNumStages; ++i) order[i].store(tmp[i]);
    }

    void setSampleRate(double sr) {
        fs = sr > 1000 ? sr : 44100.0;
        reset();
        for (auto& s : smooth) s.setTime(fs, 20.0);
        bypassSmooth.setTime(fs, 15.0);
        agApplied.setTime(fs, 400.0);
        agPowCoef = (float)dm::exp(-1.0 / (fs * 1.5));
        meterFall = (float)(20.0 * 64.0 / fs); // ~20 dB/s per 64-sample hop... computed per block below
        dcCoef = (float)(1.0 - 2 * kPi * 8.0 / fs);
        // transient look-ahead: restart lands 3.5 ms before detection; 5 ms total delay
        laLen = (uint32_t)std::lround(0.005 * fs);
        trBack = (uint32_t)std::lround(0.0035 * fs);
        trRamp = std::max(1, (int)std::lround(0.00145 * fs));
        trFastRel = (float)dm::exp(-1.0 / (fs * 0.002));
        trSlowC = (float)dm::exp(-1.0 / (fs * 0.030));
        trRefr = (int)std::lround(0.030 * fs);
        trMinRe = (int)std::lround(0.008 * fs);
        trAttackWin = (int)std::lround(0.002 * fs);
        for (int ch = 0; ch < 2; ++ch) trHP.highpass(fs, std::min(9000.0, fs * 0.42), 0.7071);
        shPre = (uint32_t)std::lround(0.001 * fs); // gain leads the audio by 1 ms
        rv.setSampleRate(fs);
        shMode = -1; shChar = -1;
        firstBlock = true;
        reset();
    }

    void reset() {
        for (int ch = 0; ch < 2; ++ch) {
            smpPhase[ch] = 1.0; smpHeld[ch] = smpPrev[ch] = 0;
            for (auto* b : {&smpAA1[ch], &smpAA2[ch], &smpOut1[ch], &smpOut2[ch], &smpPunch[ch],
                            &preLowPre[ch], &preLowPost[ch], &preHiPre[ch], &preHiPost[ch],
                            &toneLow[ch], &toneDip[ch], &toneBite[ch], &toneLP[ch]})
                b->reset();
            preUp[ch].reset(); preDown[ch].reset(); clipUp[ch].reset(); clipDown[ch].reset();
            preDcX[ch] = preDcY[ch] = 0; dcX[ch] = dcY[ch] = 0;
            scHP[ch].reset();
        }
        sideLP.reset();
        std::memset(laBuf, 0, sizeof laBuf); laPos = 0;
        trHP.reset();
        trLoF = trHiF = 0; trLoS = trHiS = 1e-12f;
        trSinceLast = 1 << 28; trLastLvl = 0;
        trNumPending = 0;
        trGain = 1; trRampPos = trRamp; trRampFrom = 1;
        shDetA.reset(); shDetB.reset();
        for (int ch = 0; ch < 2; ++ch) shBand[ch].reset();
        shFastA = shSlowA = shFastS = shSlowS = 1e-9f; shBase = 0; shGainS = shGainF = 1.f;
        for (uint32_t i = 0; i < kLaMax; ++i) { shHist[i][0] = 1.f; shHist[i][1] = 1.f; }
        shCurMain = shCurFocus = 1.f;
        compEnv = 0; compGR = 0;
        gateEnv = 0; gateGain = 1; gateHoldCount = 0; gateOpen = false;
        agInPow = agOutPow = 1e-6f; agGain = 1;
        agApplied.v = 1;
        rv.reset(); wt.reset();
    }

    // process stereo (in-place allowed)
    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t frames) {
        const DenormalGuard denormals;
        uint32_t done = 0;
        float blockPeakIn = 0, blockPeakOut = 0, blockGR = 0;
        while (done < frames) {
            const uint32_t n = std::min<uint32_t>(kSub, frames - done);
            updateCoefficients();
            for (uint32_t i = 0; i < n; ++i) {
                const uint32_t k = done + i;
                const float rawL = inL[k], rawR = inR[k];
                transientDetect(rawL, rawR);
                shapeDetect(rawL, rawR);
                // look-ahead delay: everything (dry included) runs on the delayed signal
                float dryL, dryR;
                {
                    const uint32_t rd = (laPos + kLaMax - laLen) & (kLaMax - 1);
                    dryL = laBuf[rd][0]; dryR = laBuf[rd][1];
                    laBuf[laPos][0] = rawL; laBuf[laPos][1] = rawR;
                    laPos = (laPos + 1) & (kLaMax - 1);
                    if (laLen == 0) { dryL = rawL; dryR = rawR; }
                }
                transientAdvance();
                shapeAdvance();
                blockPeakIn = std::max(blockPeakIn, std::max(std::fabs(dryL), std::fabs(dryR)));

                const float inG = smooth[0].process(tgtInGain);
                float l = dryL * inG, r = dryR * inG;

                for (int s = 0; s < kNumStages; ++s) {
                    switch (curOrder[s]) {
                    case kStSampler: if (on[kStSampler]) sampler(l, r); break;
                    case kStPreamp: if (on[kStPreamp]) preamp(l, r); break;
                    case kStSquash: if (on[kStSquash]) squash(l, r); break;
                    case kStTone: if (on[kStTone]) tone(l, r); break;
                    case kStClip: if (on[kStClip]) clip(l, r); break;
                    case kStGateWidth: if (on[kStGateWidth]) gateWidth(l, r); break;
                    case kStTransient: if (on[kStTransient] && trActive) { l *= trGain; r *= trGain; } break;
                    case kStShape: if (on[kStShape] && shActive) shapeApply(l, r); break;
                    case kStReverb: if (on[kStReverb]) rv.process(l, r); break;
                    case kStWeight: if (on[kStWeight]) wt.process(l, r); break;
                    }
                }
                // DC blocker (skipped when every module is off: then nothing can add DC, and the
                // "nothing" preset nulls against bypass)
                if (anyStageOn) for (int ch = 0; ch < 2; ++ch) {
                    float& v = ch ? r : l;
                    const float y = v - dcX[ch] + dcCoef * dcY[ch];
                    dcX[ch] = v; dcY[ch] = y; v = y;
                }
                // auto gain (slow loudness match of wet vs dry)
                if (autoGain) {
                    const float pin = 0.5f * (dryL * dryL + dryR * dryR);
                    const float pout = 0.5f * (l * l + r * r);
                    agInPow = pin + agPowCoef * (agInPow - pin);
                    agOutPow = pout + agPowCoef * (agOutPow - pout);
                    if (agInPow > 1e-8f && agOutPow > 1e-12f)
                        agGain = clampf(std::sqrt(agInPow / agOutPow), 0.063f, 4.0f);
                }
                const float ag = agApplied.process(autoGain ? agGain : 1.0f);
                l *= ag; r *= ag;
                if (hpOn) { l = hpf[0].process(l); r = hpf[1].process(r); }
                if (lpOn) { l = lpf[0].process(l); r = lpf[1].process(r); }

                const float mix = smooth[1].process(tgtMix);
                l = lerpf(dryL, l, mix); r = lerpf(dryR, r, mix);
                const float og = smooth[2].process(tgtOutGain);
                l *= og; r *= og;

                const float bp = bypassSmooth.process(bypass ? 1.f : 0.f);
                l = lerpf(l, dryL, bp); r = lerpf(r, dryR, bp);

                if (!std::isfinite(l) || !std::isfinite(r)) { reset(); l = r = 0; }
                outL[k] = l; outR[k] = r;
                blockPeakOut = std::max(blockPeakOut, std::max(std::fabs(l), std::fabs(r)));
                blockGR = std::max(blockGR, -compGR);
            }
            done += n;
        }
        // meters with fall-off
        const float fallDb = 24.0f * (float)frames / (float)fs;
        const float inDb = linToDb(blockPeakIn), outDb = linToDb(blockPeakOut);
        meterIn = std::max(inDb, meterIn - fallDb);
        meterOut = std::max(outDb, meterOut - fallDb);
        meterGR = std::max(blockGR * (on[kStSquash] ? 1.f : 0.f), meterGR - fallDb);
        meterIn = clampf(meterIn, -60, 6); meterOut = clampf(meterOut, -60, 6); meterGR = clampf(meterGR, 0, 30);
    }

private:
    static constexpr uint32_t kSub = 32;
    double fs = 44100.0;

    // cached per sub-block
    int era = 0;
    bool on[kNumStages] = {true, true, true, true, true, true, true, true, false, false};
    int curOrder[kNumStages] = {6, 7, 0, 1, 2, 3, 9, 8, 4, 5};
    DrumReverb rv; Weight wt; bool rvWasOn = false, wtWasOn = false, anyStageOn = true;
    bool oversample = true, autoGain = true, bypass = false;
    float tgtInGain = 1, tgtOutGain = 1, tgtMix = 1;
    OnePoleSmooth smooth[6];
    OnePoleSmooth bypassSmooth, agApplied;
    bool firstBlock = true;

    // ---- sampler
    int smpMode = 1;
    double smpRatio = 1.0;
    float smpStep = 0, smpTrunc = 0, smpZoh = 0;
    bool smpActive = false;
    double smpPhase[2] = {1, 1};
    float smpHeld[2] = {}, smpPrev[2] = {};
    Biquad smpAA1[2], smpAA2[2], smpOut1[2], smpOut2[2], smpPunch[2];
    bool smpAAOn = false, smpAA4 = false, smpOut4 = false, smpDac = false;

    // ---- preamp
    int preFlavor = 1;
    float preGain = 1, preMakeup = 1, preColor = 0.5f;
    bool preHardClip = false;
    Biquad preLowPre[2], preLowPost[2], preHiPre[2], preHiPost[2];
    Upsampler2x preUp[2]; Downsampler2x preDown[2];
    float preDcX[2] = {}, preDcY[2] = {};

    // ---- squash
    float cThr = -22, cRatio = 6, cMakeup = 1, cBlend = 0.5f, cAtt = 0, cRel = 0, cFet = 0.15f;
    bool cRms = true, cSC = false;
    float compEnv = 0, compGR = 0, compRmsCoef = 0;
    Biquad scHP[2];

    // ---- tone
    Biquad toneLow[2], toneDip[2], toneBite[2], toneLP[2];

    // ---- clip
    float clipDrive = 1, clipCeil = 1, clipOutComp = 1;
    bool clipHard = false;
    Upsampler2x clipUp[2]; Downsampler2x clipDown[2];

    // ---- gate / width
    bool gateOn = false;
    float gateThrLin = 0, gateHoldSamples = 0, gateEnv = 0, gateGain = 1, gateEnvRel = 0, gateAtt = 0, gateRelC = 0;
    int gateHoldCount = 0; bool gateOpen = false;
    float width = 1; bool monoLows = true;
    Biquad sideLP;

    // ---- transient
    static constexpr uint32_t kLaMax = 4096; // power of two, > 5 ms at 384 kHz
    float laBuf[kLaMax][2] = {};
    uint32_t laPos = 0, laLen = 0, trBack = 0;
    int trRamp = 64, trRefr = 1323, trMinRe = 353, trAttackWin = 88;
    Biquad trHP;
    float trFastRel = 0, trSlowC = 0;
    float trLoF = 0, trHiF = 0, trLoS = 1e-12f, trHiS = 1e-12f;
    float trThrLo = 15.8f, trThrHi = 39.8f, trFloorLo = 0.0056f, trFloorHi = 0.0018f;
    int trSinceLast = 1 << 28; float trLastLvl = 0;
    int trPending[8] = {}; int trNumPending = 0;
    bool trActive = false;
    float trGain = 1, trDecay = 1, trRampFrom = 1; int trRampPos = 64;

    // detect hits on the un-delayed input (two bands: full range + air band for hats under kick tails)
    inline void transientDetect(float l, float r) {
        const float lo = std::max(std::fabs(l), std::fabs(r));
        const float hi = std::fabs(trHP.process(0.5f * (l + r)));
        trLoF = lo > trLoF ? lo : lo + trFastRel * (trLoF - lo);
        trHiF = hi > trHiF ? hi : hi + trFastRel * (trHiF - hi);
        trLoS = lo * lo + trSlowC * (trLoS - lo * lo);
        trHiS = hi * hi + trSlowC * (trHiS - hi * hi);
        if (trLoS < 1e-20f) trLoS = 1e-20f;
        if (trHiS < 1e-20f) trHiS = 1e-20f;
        const bool tLo = trLoF > trFloorLo && trLoF * trLoF > trThrLo * trLoS;
        const bool tHi = trHiF > trFloorHi && trHiF * trHiF > trThrHi * trHiS;
        if (trSinceLast < (1 << 28)) ++trSinceLast;
        if (tLo || tHi) {
            const bool fresh = trSinceLast > trRefr;
            const bool louder = trSinceLast > trMinRe && trLoF > trLastLvl * 1.9953f; // +6 dB
            if (fresh || louder) {
                trSinceLast = 0; trLastLvl = trLoF;
                if (trNumPending < 8) trPending[trNumPending++] = (int)(laLen - trBack);
            } else if (trSinceLast < trAttackWin) {
                trLastLvl = std::max(trLastLvl, trLoF);
            }
        }
    }
    // advance the envelope one (delayed) sample
    inline void transientAdvance() {
        bool restart = false;
        for (int i = 0; i < trNumPending; ) {
            if (--trPending[i] <= 0) { restart = true; trPending[i] = trPending[--trNumPending]; }
            else ++i;
        }
        if (!trActive) return;
        if (restart) { trRampPos = 0; trRampFrom = trGain; }
        if (trRampPos < trRamp) {
            ++trRampPos;
            trGain = trRampFrom + (1.f - trRampFrom) * (float)trRampPos / (float)trRamp;
        } else {
            trGain *= trDecay;
            if (trGain < 1e-6f) trGain = 1e-6f;
        }
    }

    // ---- shape (drum transient shaper)
    int shMode = -1, shChar = -1;
    bool shActive = false;
    float shAtt = 0, shSus = 0, shFoc = 0, shAttScale = 1;
    Biquad shDetA, shDetB, shBand[2];
    float shCfA = 0, shCsA = 0, shCrA = 0, shCfR = 0, shCsR = 0, shCinst = 0, shGs = 0;
    float shBase = 0, shCbase = 0;
    float shFastA = 1e-9f, shSlowA = 1e-9f, shFastS = 1e-9f, shSlowS = 1e-9f;
    float shGainS = 1, shGainF = 1;
    float shHist[kLaMax][2];
    uint32_t shPre = 44;
    float shCurMain = 1, shCurFocus = 1;
    uint32_t shWrite = 0;

    // runs on the un-delayed input: computes the gains, stored so they line up with the delayed audio
    inline void shapeDetect(float l, float r) {
        float x = 0.5f * (l + r);
        x = shDetB.process(shDetA.process(x));
        // short RMS so the followers see the hit's envelope, not individual waveform cycles
        shBase = x * x + shCbase * (shBase - x * x);
        const float v = std::sqrt(std::max(shBase, 0.f));
        // attack: instant-ish attack follower vs slow attack follower (same release)
        shFastA = v > shFastA ? v + shCfA * (shFastA - v) : v + shCrA * (shFastA - v);
        shSlowA = v > shSlowA ? v + shCsA * (shSlowA - v) : v + shCrA * (shSlowA - v);
        // sustain: same instant attack, slow release vs fast release
        shFastS = v > shFastS ? v + shCinst * (shFastS - v) : v + shCfR * (shFastS - v);
        shSlowS = v > shSlowS ? v + shCinst * (shSlowS - v) : v + shCsR * (shSlowS - v);
        float gMain = 1.f, gFocus = 1.f;
        if (shActive) {
            const float dA = clampf(linToDb(shFastA + 1e-9f) - linToDb(shSlowA + 1e-9f), 0, 18);
            const float dS = clampf(linToDb(shSlowS + 1e-9f) - linToDb(shFastS + 1e-9f), 0, 30);
            const float gate = clampf((linToDb(shSlowS + 1e-9f) + 70.f) / 20.f, 0, 1); // ignore the noise floor
            const float notAttack = 1.f - clampf(dA / 6.f, 0, 1); // sustain only works once the attack is over
            const float dbMain = (shAtt * dA * 0.9f * shAttScale + shSus * dS * 0.75f * notAttack) * gate;
            const float dbFoc = shFoc * (dA * 1.2f - dS * 0.75f * notAttack) * gate;
            gMain = dbToLin(clampf(dbMain, -36, 18));
            gFocus = dbToLin(clampf(dbFoc, -24, 15));
        }
        shGainS = gMain + shGs * (shGainS - gMain);
        shGainF = gFocus + shGs * (shGainF - gFocus);
        shHist[shWrite][0] = shGainS; shHist[shWrite][1] = shGainF;
        shWrite = (shWrite + 1) & (kLaMax - 1);
    }
    // pick the gain that lines up with the delayed audio, 1 ms early so boosts catch the very front of the hit
    inline void shapeAdvance() {
        const uint32_t back = laLen > shPre ? laLen - shPre : 0;
        const uint32_t rd = (shWrite + kLaMax - 1 - back) & (kLaMax - 1);
        shCurMain = shHist[rd][0]; shCurFocus = shHist[rd][1];
    }
    inline void shapeApply(float& l, float& r) {
        if (std::fabs(shCurFocus - 1.f) > 1e-6f) {
            l += shBand[0].process(l) * (shCurFocus - 1.f);
            r += shBand[1].process(r) * (shCurFocus - 1.f);
        } else { shBand[0].process(l); shBand[1].process(r); }
        l *= shCurMain; r *= shCurMain;
    }

    // ---- output filters
    Biquad hpf[2], lpf[2];
    bool hpOn = false, lpOn = false;

    // ---- autogain / dc / meters
    float agInPow = 1e-6f, agOutPow = 1e-6f, agGain = 1, agPowCoef = 0;
    float dcX[2] = {}, dcY[2] = {}, dcCoef = 0.999f;
    float meterFall = 0;

    float P(uint32_t i) const { return params[i].load(std::memory_order_relaxed); }

    // macro amount: 0..1 knob -> 0..2 (1.0 at 12 o'clock = advanced settings)
    static float macroT(float m) { return clampf(m, 0, 1) * 2.0f; }

    void updateCoefficients() {
        era = (int)std::lround(clampf(P(pEra), 0, 2));
        const float tSq = macroT(P(pSquash)), tDirt = macroT(P(pDirt)), tAge = macroT(P(pAge));
        oversample = P(pQuality) > 0.5f;
        autoGain = P(pAutoGain) > 0.5f;
        bypass = P(pBypass) > 0.5f;
        tgtInGain = dbToLin(P(pInput));
        tgtOutGain = dbToLin(P(pOutput));
        tgtMix = clampf(P(pMix), 0, 1);
        for (int i = 0; i < kNumStages; ++i) curOrder[i] = order[i].load(std::memory_order_relaxed);
        on[kStSampler] = P(pSmpOn) > 0.5f;
        on[kStPreamp] = P(pPreOn) > 0.5f;
        on[kStSquash] = P(pSqOn) > 0.5f;
        on[kStTone] = P(pToneOn) > 0.5f;
        on[kStClip] = P(pClipOn) > 0.5f;
        on[kStGateWidth] = P(pGwOn) > 0.5f;
        on[kStTransient] = P(pTrOn) > 0.5f;
        on[kStShape] = P(pShOn) > 0.5f;
        on[kStReverb] = P(pRvOn) > 0.5f;
        on[kStWeight] = P(pWtOn) > 0.5f;
        // a module that switches on starts from silence (no stale tail from the last time it ran)
        if (on[kStReverb] && !rvWasOn) rv.reset();
        if (on[kStWeight] && !wtWasOn) wt.reset();
        rvWasOn = on[kStReverb]; wtWasOn = on[kStWeight];
        {
            bool any = false;
            for (int k = 0; k < kNumStages; ++k) any = any || on[k];
            if (any && !anyStageOn) { dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0; }
            anyStageOn = any;
        }

        if (firstBlock) {
            smooth[0].v = tgtInGain; smooth[1].v = tgtMix; smooth[2].v = tgtOutGain;
            bypassSmooth.v = bypass ? 1.f : 0.f;
            firstBlock = false;
        }

        // ---------------- sampler (AGE)
        {
            smpMode = (int)std::lround(clampf(P(pSmpMode), 0, 2));
            const float rateK = clampf(P(pSmpRate), 4, 44.1f) * 1000.f;
            const float bits = clampf(P(pSmpBits), 4, 16);
            const float crunch = clampf(P(pSmpCrunch), 0, 1);
            const float hostRef = (float)std::min(fs, 48000.0);
            float effRate, effBits;
            if (tAge <= 1.f) {
                effRate = dm::exp(lerpf(dm::log(hostRef * 1.02f), dm::log(std::min(rateK, hostRef)), tAge));
                effBits = lerpf(24.f, bits, tAge);
            } else {
                effRate = std::min(rateK, hostRef) * dm::pow(2.f, -(tAge - 1.f) * 1.6f);
                effBits = bits - (tAge - 1.f) * 5.f;
            }
            effRate = std::max(effRate, 2000.f);
            effBits = std::max(effBits, 3.f);
            smpActive = tAge > 0.001f;
            smpRatio = std::min(1.0, (double)effRate / fs);
            smpStep = 2.0f / dm::pow(2.0f, effBits);
            const float cAmt = crunch * std::min(tAge, 1.f) + std::max(0.f, tAge - 1.f) * 0.5f;
            smpZoh = clampf(0.25f + cAmt * 0.75f, 0, 1);
            smpTrunc = clampf(cAmt, 0, 1);
            const bool aaParam = P(pSmpAA) > 0.5f;
            const float fx = effRate;
            smpDac = false;
            if (aaParam) { smpAAOn = true; smpAA4 = true; }
            else if (smpMode == 0) { smpAAOn = false; smpAA4 = false; }
            else { smpAAOn = true; smpAA4 = false; }
            const double aaF = aaParam ? fx * 0.45 : (smpMode == 1 ? fx * 0.6 : fx * 0.5);
            const double outF = smpMode == 0 ? fx * 0.42 : (smpMode == 1 ? fx * 0.45 : fx * 0.48);
            smpOut4 = smpMode == 1;
            for (int ch = 0; ch < 2; ++ch) {
                smpAA1[ch].lowpass(fs, aaF, smpAA4 ? 0.54 : 0.707);
                smpAA2[ch].lowpass(fs, aaF, 1.31);
                smpOut1[ch].lowpass(fs, outF, smpOut4 ? 0.54 : 0.707);
                smpOut2[ch].lowpass(fs, outF, 1.31);
                if (aaF >= fs * 0.45) { smpAA1[ch].active = smpAA2[ch].active = false; }
                if (outF >= fs * 0.45) { smpOut1[ch].active = smpOut2[ch].active = false; }
                smpPunch[ch].lowshelf(fs, 90, smpMode == 2 ? 1.5 * std::min(tAge, 1.f) : 0.0);
            }
            smpDac = smpMode == 2;
        }

        // ---------------- preamp (DIRT)
        {
            preFlavor = (int)std::lround(clampf(P(pPreFlavor), 0, 3));
            const float drive = clampf(P(pPreDrive), 0, 24);
            float effDrive = drive * std::min(tDirt, 1.f) + std::max(0.f, tDirt - 1.f) * (drive * 0.5f + 8.f);
            preGain = dbToLin(effDrive);
            preMakeup = dm::pow(preGain, -0.75f);
            preColor = clampf(P(pPreColor), 0, 1);
            float lowEmph = clampf(P(pPreLowEmph), 0, 1) * 9.f;
            if (preFlavor == 3) lowEmph += 4.f;
            const double lowF = preFlavor == 3 ? 180.0 : 140.0;
            const float hiEmph = preFlavor == 2 ? 2.f + preColor * 6.f : 0.f;
            for (int ch = 0; ch < 2; ++ch) {
                preLowPre[ch].lowshelf(fs, lowF, lowEmph);
                preLowPost[ch].lowshelf(fs, lowF, -lowEmph);
                preHiPre[ch].highshelf(fs, 4500, hiEmph);
                preHiPost[ch].highshelf(fs, 4500, -hiEmph);
            }
            preHardClip = (era == 2);
        }

        // ---------------- squash (SQUASH)
        {
            const float thr = P(pSqThresh), ratio = P(pSqRatio), att = P(pSqAttack), rel = P(pSqRelease), blend = P(pSqBlend);
            const float t1 = std::min(tSq, 1.f), t2 = std::max(0.f, tSq - 1.f);
            cThr = thr * t1 - t2 * 14.f;
            cRatio = 1.f + (ratio - 1.f) * t1 + t2 * (30.f - ratio) * 0.6f;
            cRatio = std::max(cRatio, 1.0f);
            cBlend = lerpf(blend * t1, 1.0f, t2 * 0.7f);
            const float relEff = rel * (1.f - 0.45f * t2);
            float attScale = 1.f; cRms = true; cFet = 0.12f;
            if (era == 1) { attScale = 0.8f; cRms = false; cFet = 0.18f; }
            if (era == 2) { attScale = 0.6f; cRms = false; cFet = 0.35f; }
            const float attEff = std::max(0.05f, att * attScale);
            cAtt = (float)dm::exp(-1.0 / (fs * attEff * 0.001));
            cRel = (float)dm::exp(-1.0 / (fs * relEff * 0.001));
            compRmsCoef = (float)dm::exp(-1.0 / (fs * 0.004));
            // static makeup for the compressed path
            cMakeup = dbToLin(clampf(-cThr * (1.f - 1.f / cRatio) * 0.55f, 0, 24));
            cSC = P(pSqSC) > 0.5f;
            for (int ch = 0; ch < 2; ++ch) scHP[ch].highpass(fs, 100, 0.707);
        }

        // ---------------- tone
        {
            const double lowF = P(pToneLowFreq) > 0.5f ? 100.0 : 60.0;
            const float lowAmt = clampf(P(pToneLowAmt), 0, 12);
            const bool bite10k = P(pToneBiteFreq) > 0.5f;
            const float biteAmt = clampf(P(pToneBiteAmt), 0, 12);
            const float depth = clampf(P(pToneDepth), 2, 20) * 1000.f;
            for (int ch = 0; ch < 2; ++ch) {
                toneLow[ch].lowshelf(fs, lowF, lowAmt, 0.7);
                toneDip[ch].peak(fs, lowF * 3.5, 0.7, -lowAmt * 0.35);
                if (bite10k) toneBite[ch].highshelf(fs, 10000, biteAmt, 0.7);
                else toneBite[ch].peak(fs, 3000, 0.8, biteAmt);
                toneLP[ch].lowpass(fs, depth, 0.65);
                if (depth >= 19900.f || depth >= fs * 0.45) toneLP[ch].active = false;
            }
        }

        // ---------------- clip (DIRT)
        {
            const float amt = clampf(P(pClipAmount), 0, 1);
            const float effAmt = amt * std::min(tDirt, 1.f) + std::max(0.f, tDirt - 1.f) * (amt * 0.5f + 0.25f);
            clipDrive = dbToLin(effAmt * 18.f);
            clipOutComp = dm::pow(clipDrive, -0.5f);
            clipCeil = dbToLin(clampf(P(pClipCeiling), -12, 0));
            clipHard = P(pClipMode) > 0.5f;
        }

        // ---------------- transient (measured from Ableton Beats mode, Loop Off)
        {
            const float e = clampf(P(pTrEnv), 0, 100);
            trActive = e < 99.95f;
            // fade rate in dB per ms: 0 -> 0.56, 25 -> 0.223, 50 -> 0.086, 75 -> 0.034
            const double rate = trActive ? 0.56 * dm::exp(-0.03736 * e) : 0.0;
            trDecay = (float)dm::pow(10.0, -rate * 1000.0 / (20.0 * fs));
            const float sens = clampf(P(pTrDetect), 0, 1);
            const float shift = (0.5f - sens) * 12.f; // dB: lower detect = higher thresholds
            trThrLo = dm::pow(10.f, (12.f + shift) / 10.f);
            trThrHi = dm::pow(10.f, (16.f + shift) / 10.f);
            trFloorLo = dbToLin(-45.f + shift * 1.5f);
            trFloorHi = dbToLin(-55.f + shift * 1.5f);
            if (!trActive) { trGain = 1.f; trRampPos = trRamp; trNumPending = 0; }
        }

        // ---------------- shape (transient shaper)
        {
            const int mode = (int)std::lround(clampf(P(pShMode), 0, 2));
            const int chr = (int)std::lround(clampf(P(pShChar), 0, 2));
            shAtt = clampf(P(pShAttack), -100, 100) * 0.01f;
            shSus = clampf(P(pShSustain), -100, 100) * 0.01f;
            shFoc = clampf(P(pShFocus), -100, 100) * 0.01f;
            shActive = std::fabs(shAtt) > 0.001f || std::fabs(shSus) > 0.001f || std::fabs(shFoc) > 0.001f;
            if (mode != shMode) {
                shMode = mode;
                // detection band tuned to the source; focus band = where the extra shaping lands
                if (mode == 0) { shDetA.lowpass(fs, 180, 0.7071); shDetB.highpass(fs, 30, 0.7071); }
                else if (mode == 1) { shDetA.highpass(fs, 150, 0.7071); shDetB.lowpass(fs, 6000, 0.7071); }
                else { shDetA.highpass(fs, 40, 0.7071); shDetB.lowpass(fs, std::min(16000.0, fs * 0.45), 0.7071); }
                const double tb = mode == 0 ? 0.004 : (mode == 1 ? 0.0015 : 0.001); // detector smoothing per source
                shCbase = (float)dm::exp(-1.0 / (fs * tb));
                shPre = (uint32_t)std::lround((0.001 + tb * 0.5) * fs);
                const double fc = mode == 0 ? 65.0 : (mode == 1 ? 240.0 : 4000.0);
                const double q = mode == 0 ? 0.9 : (mode == 1 ? 0.9 : 0.7);
                for (int ch = 0; ch < 2; ++ch) {
                    const double w = 2 * kPi * std::min(fc, fs * 0.45) / fs, al = dm::sin(w) / (2 * q), c = dm::cos(w);
                    shBand[ch].setNorm(al, 0, -al, 1 + al, -2 * c, 1 - al); shBand[ch].active = true;
                }
            }
            if (chr != shChar) {
                shChar = chr;
                // attack detector: fast vs slow attack; sustain detector: slow vs fast release
                const double fA = 0.0001, sA[3] = {0.005, 0.008, 0.012};
                const double rA[3] = {0.020, 0.030, 0.050};
                const double fR[3] = {0.030, 0.020, 0.030}, sR[3] = {0.250, 0.150, 0.400};
                const double gs[3] = {0.0005, 0.002, 0.004};
                auto co = [&](double sec) { return (float)dm::exp(-1.0 / (fs * sec)); };
                shCfA = co(fA); shCsA = co(sA[chr]); shCrA = co(rA[chr]);
                shCfR = co(fR[chr]); shCsR = co(sR[chr]); shCinst = co(0.0002);
                shGs = co(gs[chr]);
                shAttScale = chr == 0 ? 0.75f : 1.0f; // natural / smooth push transients harder than classic
            }
        }

        // ---------------- reverb / weight (v2.0)
        if (on[kStReverb])
            rv.configure((int)std::lround(clampf(P(pRvMode), 0, 2)), clampf(P(pRvDecay), 0.1f, 4), P(pRvPre), P(pRvTone), P(pRvDuck), P(pRvMix));
        if (on[kStWeight])
            wt.configure(fs, P(pWtFreq), P(pWtAmount), P(pWtFocus), P(pWtHarm));

        // ---------------- output filters: soft knee (Q 0.5), off at the extremes
        {
            const float hp = clampf(P(pHpf), 20, 2000), lp = clampf(P(pLpf), 500, 20000);
            const bool nh = hp > 20.5f, nl = lp < 19900.f && lp < fs * 0.45;
            if (nh && !hpOn) { hpf[0].reset(); hpf[1].reset(); }
            if (nl && !lpOn) { lpf[0].reset(); lpf[1].reset(); }
            hpOn = nh; lpOn = nl;
            for (int ch = 0; ch < 2; ++ch) {
                if (hpOn) hpf[ch].highpass(fs, hp, 0.5);
                if (lpOn) lpf[ch].lowpass(fs, lp, 0.5);
            }
        }

        // ---------------- gate / width
        {
            const float g = P(pGate);
            gateOn = g > -79.5f;
            gateThrLin = dbToLin(g);
            gateHoldSamples = (float)(clampf(P(pGateHold), 5, 500) * 0.001 * fs);
            gateEnvRel = (float)dm::exp(-1.0 / (fs * 0.010));
            gateAtt = (float)dm::exp(-1.0 / (fs * 0.0005));
            gateRelC = (float)dm::exp(-1.0 / (fs * 0.060));
            width = clampf(P(pWidth), 0, 2);
            monoLows = P(pMonoLows) > 0.5f;
            sideLP.lowpass(fs, 120, 0.707);
        }
    }

    // ---------------------------------------------------------------- stages
    inline float quant(float v) const {
        const float s = smpStep;
        return s * std::floor(v / s + 0.5f * (1.f - smpTrunc));
    }

    void sampler(float& l, float& r) {
        if (!smpActive) return;
        float* io[2] = {&l, &r};
        for (int ch = 0; ch < 2; ++ch) {
            float x = *io[ch];
            if (smpMode == 1) x = dm::tanh(x * 1.1f) / 1.1f; // ADC overload softness
            if (smpAAOn) { x = smpAA1[ch].process(x); if (smpAA4) x = smpAA2[ch].process(x); }
            smpPhase[ch] += smpRatio;
            if (smpPhase[ch] >= 1.0) {
                smpPhase[ch] -= 1.0;
                smpPrev[ch] = smpHeld[ch];
                smpHeld[ch] = quant(x);
            }
            const float lin = smpPrev[ch] + (smpHeld[ch] - smpPrev[ch]) * (float)smpPhase[ch];
            float y = lerpf(lin, smpHeld[ch], smpZoh);
            y = smpOut1[ch].process(y);
            if (smpOut4) y = smpOut2[ch].process(y);
            if (smpDac) { y = smpPunch[ch].process(y); y = y - 0.04f * y * y * y; }
            *io[ch] = y;
        }
    }

    inline float shapePre(float u) const {
        const float c = preColor;
        float y;
        switch (preFlavor) {
        case 0: { // console
            const float b = 0.15f * c;
            y = dm::tanh(u + b) - dm::tanh(b);
        } break;
        case 1: { // tube: asymmetric
            const float k = 1.f + 1.2f * c;
            y = u >= 0 ? dm::tanh(u) : dm::tanh(u * k) / k;
        } break;
        case 2: // tape: softer knee
            y = dm::atan(u * 1.2f) / 1.2f;
            break;
        default: { // xfmr: cubic soft clip (odd harmonics)
            const float v = clampf(u * 0.8f, -1.5f, 1.5f);
            y = (v - v * v * v * (4.f / 27.f)) * 1.25f;
            const float b = 0.1f * c; y += b * u * u / (1.f + u * u);
        } break;
        }
        if (preHardClip) {
            const float hp = 0.9f, hn = -0.75f;
            y = y > hp ? hp : (y < hn ? hn : y);
        }
        return y;
    }

    void preamp(float& l, float& r) {
        float* io[2] = {&l, &r};
        for (int ch = 0; ch < 2; ++ch) {
            float x = *io[ch];
            x = preLowPre[ch].process(x);
            x = preHiPre[ch].process(x);
            float y;
            if (oversample) {
                float a, b;
                preUp[ch].process(x, a, b);
                a = shapePre(a * preGain); b = shapePre(b * preGain);
                y = preDown[ch].process(a, b);
            } else {
                y = shapePre(x * preGain);
            }
            y = preHiPost[ch].process(y);
            y = preLowPost[ch].process(y);
            y *= preMakeup;
            // DC removal (asymmetric shapers)
            const float d = y - preDcX[ch] + 0.9995f * preDcY[ch];
            preDcX[ch] = y; preDcY[ch] = d;
            *io[ch] = d;
        }
    }

    void squash(float& l, float& r) {
        float sl = l, sr = r;
        if (cSC) { sl = scHP[0].process(sl); sr = scHP[1].process(sr); }
        float lvl = std::max(std::fabs(sl), std::fabs(sr));
        if (cRms) {
            compEnv = lvl * lvl + compRmsCoef * (compEnv - lvl * lvl);
            lvl = std::sqrt(std::max(compEnv, 0.f)) * 1.41f;
        }
        const float db = linToDb(lvl);
        const float over = db - cThr, W = 6.f;
        float gr;
        if (2.f * over < -W) gr = 0.f;
        else if (2.f * std::fabs(over) <= W) gr = (1.f / cRatio - 1.f) * (over + W * 0.5f) * (over + W * 0.5f) / (2.f * W);
        else gr = (1.f / cRatio - 1.f) * over;
        const float c = gr < compGR ? cAtt : cRel;
        compGR = gr + c * (compGR - gr);
        const float g = dbToLin(compGR) * cMakeup;
        float wl = l * g, wr = r * g;
        // FET-style grit on the squashed path
        wl = wl * (1.f - cFet) + cFet * dm::tanh(wl);
        wr = wr * (1.f - cFet) + cFet * dm::tanh(wr);
        l = lerpf(l, wl, cBlend);
        r = lerpf(r, wr, cBlend);
    }

    void tone(float& l, float& r) {
        float* io[2] = {&l, &r};
        for (int ch = 0; ch < 2; ++ch) {
            float x = *io[ch];
            x = toneLow[ch].process(x);
            x = toneDip[ch].process(x);
            x = toneBite[ch].process(x);
            x = toneLP[ch].process(x);
            *io[ch] = x;
        }
    }

    inline float clipShape(float x) const {
        const float c = clipCeil;
        if (clipHard) return x > c ? c : (x < -c ? -c : x);
        const float v = x / c, a = std::fabs(v);
        const float knee = 0.6f;
        const float o = a < knee ? a : knee + (1.f - knee) * dm::tanh((a - knee) / (1.f - knee));
        return (v < 0 ? -o : o) * c;
    }

    void clip(float& l, float& r) {
        float* io[2] = {&l, &r};
        for (int ch = 0; ch < 2; ++ch) {
            const float x = *io[ch] * clipDrive;
            float y;
            if (oversample) {
                float a, b;
                clipUp[ch].process(x, a, b);
                y = clipDown[ch].process(clipShape(a), clipShape(b));
            } else {
                y = clipShape(x);
            }
            *io[ch] = y * clipOutComp;
        }
    }

    void gateWidth(float& l, float& r) {
        if (gateOn) {
            const float lvl = std::max(std::fabs(l), std::fabs(r));
            gateEnv = lvl > gateEnv ? lvl : lvl + gateEnvRel * (gateEnv - lvl);
            if (gateEnv > gateThrLin) { gateOpen = true; gateHoldCount = (int)gateHoldSamples; }
            else if (gateEnv < gateThrLin * 0.63f) {
                if (gateHoldCount > 0) --gateHoldCount; else gateOpen = false;
            }
            const float target = gateOpen ? 1.f : 0.0003f;
            const float c = target > gateGain ? gateAtt : gateRelC;
            gateGain = target + c * (gateGain - target);
            l *= gateGain; r *= gateGain;
        }
        float m = 0.5f * (l + r), s = 0.5f * (l - r);
        if (monoLows) s = s - sideLP.process(s);
        s *= width;
        l = m + s; r = m - s;
    }
};

} // namespace mercy
