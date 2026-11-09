// MERCY drum processor - interface (drawing + interaction), host-agnostic
// v1.7 "signal grain": Swiss layout, print grain, sound as data. Light + dark.
#pragma once
#include <chrono>
#include "NanoVG.hpp"
#include "MercyDSP.hpp"
#include "MercyBake.hpp"
#include "MercySession.hpp"
#include "MercyPresets.hpp"
#include "MercyMotion.hpp"
#include "font_mercy.h"
#include "logo_slapworx.h"
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace mercy {

using DGL_NAMESPACE::NanoVG;
using DGL_NAMESPACE::NanoImage;
using DGL_NAMESPACE::Color;

struct ViewHost {
    virtual ~ViewHost() {}
    virtual void hostBeginEdit(uint32_t p) = 0;
    virtual void hostSetParam(uint32_t p, float v) = 0;
    virtual void hostEndEdit(uint32_t p) = 0;
    virtual void hostSetOrder(const char* s) = 0;
    virtual void hostRepaint() = 0;
    virtual void hostResize(unsigned w, unsigned h) = 0;
    virtual void hostBrowseFile() {}
    virtual bool hostStartDrag(const std::vector<std::string>&) { return false; }
    virtual void hostSetUiState(const char*) {}
    virtual void hostKeyboardFocus(bool /*grab*/) {}
    virtual bool hostReduceMotion() { return false; }   // the OS asks for less motion
};

static constexpr float kW = 960.f, kH = 600.f;

// ------------------------------------------------------------------------------------------------
// theme: paper / ink, flat saturated blocks

static inline Color rgb(int hex, float a = 1.f) { return Color((hex >> 16) & 255, (hex >> 8) & 255, hex & 255, a); }
static inline Color mixc(const Color& a, const Color& b, float t) {
    if (t <= 0.f) return a;
    if (t >= 1.f) return b;
    Color c = a;
    c.red = a.red + (b.red - a.red) * t; c.green = a.green + (b.green - a.green) * t;
    c.blue = a.blue + (b.blue - a.blue) * t; c.alpha = a.alpha + (b.alpha - a.alpha) * t;
    return c;
}

struct Theme {
    Color paper, paper2, side, ink, ink2, grey, hair, hair2, scan, blue, violet, red, pink, mint, yellow, orange, dim,
          onMint, onMint2, hoverRow;
};

static Theme makeTheme(bool dark) {
    Theme t;
    if (!dark) {
        t.paper = rgb(0xECEAEB); t.paper2 = rgb(0xE4E1E3); t.side = rgb(0xE4E1E3);
        t.ink = rgb(0x110E12); t.ink2 = rgb(0x110E12, .80f); t.grey = rgb(0x625D66);
        t.hair = rgb(0x110E12, .30f); t.hair2 = rgb(0x110E12, .13f); t.scan = rgb(0xCFCBD0);
        t.blue = rgb(0x2630D1); t.violet = rgb(0x2A0F4A); t.dim = rgb(0xECEAEB, .84f); t.hoverRow = rgb(0x110E12, .06f);
    } else {
        t.paper = rgb(0x110E12); t.paper2 = rgb(0x18151A); t.side = rgb(0x000000);
        t.ink = rgb(0xECEAEB); t.ink2 = rgb(0xECEAEB, .82f); t.grey = rgb(0x8F8A92);
        t.hair = rgb(0xECEAEB, .26f); t.hair2 = rgb(0xECEAEB, .11f); t.scan = rgb(0x2C2830);
        t.blue = rgb(0x3B46FF); t.violet = rgb(0x6A3CB8); t.dim = rgb(0x110E12, .86f); t.hoverRow = rgb(0xECEAEB, .07f);
    }
    t.red = rgb(0xE2290F); t.pink = rgb(0xDC7F89); t.mint = rgb(0x47E4AE); t.yellow = rgb(0xE2CC1E); t.orange = rgb(0xBF3E23);
    t.onMint = rgb(0x110E12); t.onMint2 = rgb(0x110E12, .62f);
    return t;
}

// ------------------------------------------------------------------------------------------------
// fonts: three voices (grotesk, ultra-wide, mono), measured so text sits exactly as designed

enum FontId { fMed = 0, f550, fSemi, fBold, fWide, fMono, fMonoB, kNumFonts };

static const mercyfonts::Metrics& fontMetrics(int f) {
    switch (f) {
    case fMed: return mercyfonts::archivo_medium_metrics;
    case f550: return mercyfonts::archivo_550_metrics;
    case fSemi: return mercyfonts::archivo_semibold_metrics;
    case fBold: return mercyfonts::archivo_bold_metrics;
    case fWide: return mercyfonts::unbounded_extrabold_metrics;
    case fMono: return mercyfonts::jbmono_regular_metrics;
    default: return mercyfonts::jbmono_bold_metrics;
    }
}
static inline float defaultSpacing(int f) { return f == fWide ? -0.02f : (f >= fMono ? 0.01f : -0.025f); }

static uint32_t nextCodepoint(const char*& s) {
    const unsigned char c = (unsigned char)*s++;
    if (c < 0x80) return c;
    if ((c >> 5) == 6 && *s) { const uint32_t r = ((c & 31u) << 6) | ((unsigned char)*s & 63u); ++s; return r; }
    if ((c >> 4) == 14 && s[0] && s[1]) { const uint32_t r = ((c & 15u) << 12) | (((unsigned char)s[0] & 63u) << 6) | ((unsigned char)s[1] & 63u); s += 2; return r; }
    if ((c >> 3) == 30 && s[0] && s[1] && s[2]) { s += 3; return 0xFFFD; }
    return 0xFFFD;
}
static float advanceOf(const mercyfonts::Metrics& m, uint32_t cp) {
    if (cp >= 32 && cp < 127) return m.adv[cp - 32];
    switch (cp) {
    case 0x2192: return m.arrow;
    case 0x00B7: return m.middot;
    case 0x2197: return m.nearrow;
    case 0x2014: return m.emdash;
    case 0x00D7: return m.times;
    default: return 0.6f;
    }
}
// pair kerning in em (the browser applies it, fontstash doesn't, so we place glyphs ourselves)
static float kernOf(int f, uint32_t a, uint32_t b) {
    if (a < 33 || a > 126 || b < 33 || b > 126) return 0.f;
    const mercyfonts::KernPair* t; int n;
    switch (f) {
    case fMed: t = mercyfonts::archivo_medium_kern; n = mercyfonts::archivo_medium_kern_n; break;
    case f550: t = mercyfonts::archivo_550_kern; n = mercyfonts::archivo_550_kern_n; break;
    case fSemi: t = mercyfonts::archivo_semibold_kern; n = mercyfonts::archivo_semibold_kern_n; break;
    case fBold: t = mercyfonts::archivo_bold_kern; n = mercyfonts::archivo_bold_kern_n; break;
    case fWide: t = mercyfonts::unbounded_extrabold_kern; n = mercyfonts::unbounded_extrabold_kern_n; break;
    default: return 0.f;
    }
    const unsigned key = (a << 8) | b;
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) >> 1;
        if (t[mid].pair == key) return t[mid].k * 0.001f;
        if (t[mid].pair < key) lo = mid + 1; else hi = mid - 1;
    }
    return 0.f;
}
// width as a browser lays it out: every glyph advance (+ kerning) plus letter spacing after each glyph
static float measureText(int f, float size, const char* s, float spacingEm) {
    const mercyfonts::Metrics& m = fontMetrics(f);
    float w = 0;
    uint32_t prev = 0;
    while (*s) {
        const uint32_t cp = nextCodepoint(s);
        w += (advanceOf(m, cp) + kernOf(f, prev, cp)) * size + spacingEm * size;
        prev = cp;
    }
    return w;
}

// small deterministic random (layout art must not shimmer between frames)
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed * 2654435761u + 12345u) {}
    float uni() { s = s * 1664525u + 1013904223u; return (float)((s >> 8) & 0xFFFFFF) / 16777216.f; }
    float range(float a, float b) { return a + (b - a) * uni(); }
    float gauss() { return (uni() + uni() + uni() + uni() - 2.f) * 1.7320508f; }
};

struct Hit { float t0, a, tau; };
// envelope of a few drum hits sampled at n+1 points (0..1), like the mockup's waveform specimens
static void hitEnvelope(const Hit* hits, int nh, int n, uint32_t seed, float jit, float floorv, std::vector<float>& out) {
    Rng r(seed);
    out.resize((size_t)n + 1);
    for (int i = 0; i <= n; ++i) {
        const float t = (float)i / (float)n;
        float v = floorv;
        for (int k = 0; k < nh; ++k) {
            if (t >= hits[k].t0) v += hits[k].a * std::exp(-(t - hits[k].t0) / hits[k].tau);
            else if (hits[k].t0 - t < 0.012f) v += hits[k].a * (1.f - (hits[k].t0 - t) / 0.012f) * 0.6f;
        }
        out[(size_t)i] = std::min(v, 1.f) * (1.f + r.range(-jit, jit));
    }
}

static const int kStairA[][2] = {{0, 0}, {1, 0}, {1, 1}, {2, 1}, {2, 2}, {3, 2}};
static const int kStairB[][2] = {{0, 1}, {1, 1}, {1, 0}, {2, 0}, {3, 0}, {3, 1}};
static const int kCluster[][2] = {{0, 0}, {1, 0}, {2, 1}, {0, 2}, {1, 2}, {3, 2}, {2, 3}, {3, 3}, {4, 1}};
static const int kDiag2[][2] = {{0, 0}, {1, 1}};
static const int kStep2[][2] = {{0, 1}, {1, 0}};
static const int kCorner3[][2] = {{0, 0}, {1, 0}, {1, 1}};

// ------------------------------------------------------------------------------------------------
// value mapping / formatting

static inline bool isLogParam(uint32_t p) {
    return p == pSqAttack || p == pSqRelease || p == pSmpRate || p == pToneDepth || p == pGateHold || p == pHpf || p == pLpf || p == pRvDecay;
}
static inline float toNorm(uint32_t p, float v) {
    const ParamInfo& pi = kParamInfo[p];
    v = clampf(v, pi.min, pi.max);
    if (isLogParam(p)) return std::log(v / pi.min) / std::log(pi.max / pi.min);
    return (v - pi.min) / (pi.max - pi.min);
}
static inline float fromNorm(uint32_t p, float n) {
    const ParamInfo& pi = kParamInfo[p];
    n = clampf(n, 0, 1);
    float v = isLogParam(p) ? pi.min * std::pow(pi.max / pi.min, n) : pi.min + n * (pi.max - pi.min);
    if (pi.isInt) v = std::round(v);
    return v;
}
static inline bool isPercentParam(uint32_t p) {
    return p == pSquash || p == pDirt || p == pAge || p == pMix || p == pSmpCrunch || p == pPreLowEmph ||
           p == pPreColor || p == pSqBlend || p == pClipAmount || p == pWidth || p == pTrDetect ||
           p == pRvTone || p == pRvDuck || p == pRvMix || p == pPreThick;
}
static std::string fmtValue(uint32_t p, float v) {
    char b[48];
    if (isPercentParam(p)) std::snprintf(b, sizeof b, "%d%%", (int)std::lround(v * 100.f));
    else if (p == pGate) { if (v <= -79.5f) return "off"; std::snprintf(b, sizeof b, "%.1f dB", v); }
    else if (p == pInput || p == pOutput || p == pPreDrive || p == pToneLowAmt || p == pToneBiteAmt)
        std::snprintf(b, sizeof b, "%+.1f dB", v);
    else if (p == pSqThresh || p == pClipCeiling) std::snprintf(b, sizeof b, "%.1f dB", v);
    else if (p == pSmpRate) std::snprintf(b, sizeof b, "%.1f kHz", v);
    else if (p == pToneDepth) { if (v >= 19.9f) return "open"; std::snprintf(b, sizeof b, "%.1f kHz", v); }
    else if (p == pSqAttack || p == pSqRelease || p == pGateHold) {
        if (v < 10.f) std::snprintf(b, sizeof b, "%.1f ms", v); else std::snprintf(b, sizeof b, "%.0f ms", v);
    }
    else if (p == pSqRatio) std::snprintf(b, sizeof b, "%.1f:1", v);
    else if (p == pTrEnv) std::snprintf(b, sizeof b, "%d", (int)std::lround(v));
    else if (p == pHpf || p == pLpf) {
        if ((p == pHpf && v <= 20.5f) || (p == pLpf && v >= 19900.f)) return "off";
        if (v >= 1000.f) std::snprintf(b, sizeof b, "%.1f kHz", v / 1000.f); else std::snprintf(b, sizeof b, "%.0f Hz", v);
    }
    else if (p == pShAttack || p == pShSustain || p == pShFocus) { const int iv = (int)std::lround(v); if (iv == 0) return "0%"; std::snprintf(b, sizeof b, "%+d%%", iv); }
    else if (p == pSmpBits) std::snprintf(b, sizeof b, "%.1f bit", v);
    else if (p == pRvDecay) std::snprintf(b, sizeof b, "%.1f s", v);
    else if (p == pRvPre) std::snprintf(b, sizeof b, "%.0f ms", v);
    else if (p == pWtFreq || p == pWtAmount) std::snprintf(b, sizeof b, "%.1f", v);
    else std::snprintf(b, sizeof b, "%.2f", v);
    return b;
}
// parse typed text in display units
static bool parseValue(uint32_t p, const std::string& s, float& out) {
    if (s.empty()) return false;
    if (p == pGate && (s == "off" || s == "OFF")) { out = -80; return true; }
    if (p == pHpf && s == "off") { out = 20; return true; }
    if (p == pLpf && s == "off") { out = 20000; return true; }
    char* end = nullptr;
    const double d = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) return false;
    float v = (float)d;
    if (isPercentParam(p)) v *= 0.01f;
    const ParamInfo& pi = kParamInfo[p];
    out = clampf(v, pi.min, pi.max);
    return true;
}

// ------------------------------------------------------------------------------------------------
// presets

// factory presets: MercyPresets.hpp
#ifdef MERCY_TEASER
static const char* const kAppFolder = "000"; // its own user presets, so the real ones never show up
#else
static const char* const kAppFolder = "Mercy";
#endif


struct UserPreset { std::string name; Snapshot s; };

// ------------------------------------------------------------------------------------------------
// controls

enum CtlType {
    tVSlider, tDotKnob, tHSlider, tMini, tNumBox, tToggle, tOption, tOnOff, tTab, tButton, tSectionHead, tSectionPower, tEqPos, tResize, tFilter
};
enum ButtonId { bPrev, bNext, bSave, bAB, bResetOrder, bPresetName, bBake, bTheme };
enum ToggleStyle { sAutoGain, sBypassBox };

struct Ctl {
    CtlType type;
    int param = -1;
    float x = 0, y = 0, w = 0, h = 0; // hit rect
    float a = 0, b = 0, c = 0;        // geometry
    int id = 0;                       // option value / button id / stage / style
    int stage = -1;                   // advanced page: the module this control belongs to (it moves with it)
    std::string label;
    bool contains(float px, float py) const { return px >= x && px <= x + w && py >= y && py <= y + h; }
};

class MercyView {
private:
    // keeps the session alive while this view exists, even if the plugin instance goes first
    // (an AU host can dispose the plugin before the editor's NSView is released)
    std::shared_ptr<Session> sessRef;
    Session& S;

public:
    explicit MercyView(ViewHost* h, Session* s = nullptr) : sessRef(s ? nullptr : std::make_shared<Session>()), S(s ? *s : *sessRef), host(h) { init(); }
    MercyView(ViewHost* h, std::shared_ptr<Session> s) : sessRef(s ? std::move(s) : std::make_shared<Session>()), S(*sessRef), host(h) { init(); }
    void init() {
        if (!S.touched && !S.savedUi.empty()) applyUiState(S.savedUi.c_str());
        lastUiState = encodeUiState();
        if (S.bakeOpen && S.baker) bakeItems = S.baker->snapshot();
        loadUserPresets();
        buildLayout();
    }

    // ---------------------------------------------------------------- host -> view
    void paramChanged(uint32_t p, float v) {
        if (p >= kNumParams) return;
        const bool changed = std::fabs(S.values[p] - v) > 1e-6f;
        S.values[p] = v;
        if (changed && p < kFirstOutputParam && !applyingPreset && p != pBypass && S.userTouched) S.presetModified = true;
        host->hostRepaint();
    }
    void orderChanged(const char* s) {
        int tmp[kNumStages];
        if (!normalizeOrder(s, tmp)) return;
        std::memcpy(S.order, tmp, sizeof S.order);
        buildLayout();
        host->hostRepaint();
    }
    // called ~30-60 Hz
    void idle() {
        syncUiState();
        syncKeyFocus(false);
        if (S.bakeOpen) {
            ensureBaker(); S.capture.ensure(S.sampleRate.load());
            Capture::Clip clip;
            if (S.capture.poll(clip)) addClip(std::move(clip));
            if (S.page == 1 && S.captureTab) host->hostRepaint();   // the strip scrolls with the music
        }
        if (S.capture.state() != lastCapState) { lastCapState = S.capture.state(); host->hostRepaint(); }
        if (S.bakeOpen && S.baker) {
            const bool b = S.baker->busy();
            if (b || bakeWasBusy) { bakeItems = S.baker->snapshot(); host->hostRepaint(); }
            bakeWasBusy = b;
        }
        // live specimen: repaint whenever new audio has been scanned
        const uint32_t sp = S.scope.pos.load(std::memory_order_acquire);
        if (sp != lastScopePos) { lastScopePos = sp; if (S.page == 0) host->hostRepaint(); }
        if (mo.isMoving()) host->hostRepaint();   // something is still settling: keep the frames coming
    }

    // ---------------------------------------------------------------- drawing
    void draw(NanoVG& g, float width, float height) {
        ensureResources(g);
        mo.step();
        if (rmCheck++ % 120 == 0) mo.reduced = host->hostReduceMotion();
        scale = std::min(width / kW, height / kH);
        offX = (width - kW * scale) * 0.5f;
        offY = (height - kH * scale) * 0.5f;
        frameMotion();
        // light <-> dark: the new theme fades in over the old one (drawn twice only while it changes)
        const float m = themeMix;
        if (m < 0.999f) drawScene(g, width, height, false, 1.f);
        if (m > 0.001f) drawScene(g, width, height, true, m < 0.999f ? m : 1.f);
        T = makeTheme(S.dark);
    }

    // one complete picture of the interface in one theme
    void drawScene(NanoVG& g, float width, float height, bool darkPass, float a) {
        dk = darkPass;
        T = makeTheme(dk);
        alphaBase = a;
        g.save();
        g.globalAlpha(a);
        g.beginPath(); g.rect(0, 0, width, height); g.fillColor(T.side); g.fill();
        g.translate(offX, offY);
        g.scale(scale, scale);

        rc(g, 0, 0, kW, kH, T.paper);
        drawChrome(g);
        const size_t nc = ctls.size();
        for (size_t i = 0; i < nc && i < 5; ++i) drawCtlM(g, (int)i);   // tabs, theme switch
        {   // the page: slides and fades in when it changes
            g.save();
            g.intersectScissor(84.5f, 31, kW - 84.5f, kH - 31);
            g.globalAlpha(alphaBase * clampf(pageAlpha, 0, 1));
            g.translate(pageOffX, 0);
            if (S.page == 0) drawMainArt(g); else drawAdvancedArt(g);
            for (size_t i = 5; i + 1 < nc; ++i) drawCtlM(g, (int)i);
            if (S.page == 0) drawFaderGuide(g);
            if (S.page == 1 && reorderFrom >= 0 && reorderMoved) drawReorderGhost(g);
            g.restore();
        }
        if (nc > 5) drawCtlM(g, (int)nc - 1);   // resize grip
        drawBrandMark(g);

        if (panelBegin(g, 0, presetOpen, 160, 557, 0, T.dim)) { drawPresetPopup(g); g.restore(); }
        if (panelBegin(g, 1, S.page == 1 && S.bakeOpen, 781, 561, 1, T.dim)) { drawBakePanel(g); g.restore(); }
        if (modOpen >= 0) modShown = modOpen;
        if (panelBegin(g, 2, modOpen >= 0, modOX, modOY, 0, alpha(T.dim, .8f)) && modShown >= 0) {
            const int keep = modOpen; modOpen = modShown; drawModulePopup(g); modOpen = keep; g.restore();
        }
        if (panelBegin(g, 3, nameOpen, nameMode == 0 ? 259.f : kNX + kNW * .5f, nameMode == 0 ? 557.f : kNY + kNH * .5f, 0,
                       presetOpen ? alpha(T.dim, .6f) : T.dim)) { drawNamePopup(g); g.restore(); }

        // print grain over everything
        const NanoImage& gi = dk ? grainDark : grainLight;
        if (gi.isValid()) {
            g.globalAlpha(a);
            g.beginPath(); g.rect(0, 0, kW, kH);
            g.fillPaint(g.imagePattern(0, 0, 128, 128, 0, gi, 1.0f));
            g.fill();
        }
        g.restore();
    }

    // ---------------------------------------------------------------- motion
    // everything that moves is a spring (MercyMotion.hpp); this works out where things are this frame
    Motion mo;
    uint32_t rmCheck = 0, layoutGen = 0;
    bool dk = true;                 // the theme being drawn (both, while the theme changes)
    float alphaBase = 1.f, themeMix = 1.f, pageOffX = 0, pageAlpha = 1, pageW = 0;
    float eraW[3] = {1, 0, 0};
    float stOffX[kNumStages] = {}, stOffY[kNumStages] = {};
    int lastPage = -1, pressIdx = -1, modShown = -1, curStage = -1, tentOrder[kNumStages] = {};
    float modOX = 0, modOY = 0;

    void frameMotion() {
        themeMix = clampf(mo.to(mkey(7, 0), S.dark ? 1.f : 0.f, kSpringFade), 0, 1);
        const int era = (int)std::lround(S.values[pEra]);
        float sum = 0;
        for (int e = 0; e < 3; ++e) { eraW[e] = clampf(mo.to(mkey(2, (uint32_t)e), e == era ? 1.f : 0.f, kSpringFade), 0, 1); sum += eraW[e]; }
        if (sum > 1e-4f) for (int e = 0; e < 3; ++e) eraW[e] /= sum; else { eraW[0] = 1; eraW[1] = eraW[2] = 0; }
        if (S.page != lastPage) {
            if (lastPage >= 0) {
                mo.from(mkey(4, 0), S.page == 1 ? 28.f : -28.f, 0, kSpringPage);
                mo.from(mkey(4, 1), 0, 1, kSpringFade);
            }
            lastPage = S.page;
        }
        pageOffX = mo.to(mkey(4, 0), 0, kSpringPage);
        pageAlpha = mo.to(mkey(4, 1), 1, kSpringFade);
        pageW = clampf(mo.to(mkey(3, 9), S.page == 1 ? 1.f : 0.f, kSpringUI), 0, 1.04f);
        // modules: each one slides to its slot; while one is dragged the others make room for it
        std::memcpy(tentOrder, S.order, sizeof tentOrder);
        if (S.page == 1 && reorderFrom >= 0 && reorderMoved) {
            const int target = sectionAt(dragX, dragY);
            if (target >= 0 && target != reorderFrom) {
                const int moving = S.order[reorderFrom];
                int k = 0;
                for (int j = 0; j < kNumStages; ++j) if (j != reorderFrom) tentOrder[k++] = S.order[j];
                for (int j = kNumStages - 1; j > target; --j) tentOrder[j] = tentOrder[j - 1];
                tentOrder[target] = moving;
            }
        }
        if (S.page == 1) {
            int lay[kNumStages];
            for (int pos = 0; pos < kNumStages; ++pos) lay[S.order[pos]] = pos;
            for (int pos = 0; pos < kNumStages; ++pos) {
                const int st = tentOrder[pos];
                stOffX[st] = mo.to(mkey(9, (uint32_t)st, 0), secXs(pos), kSpringPos) - secXs(lay[st]);
                stOffY[st] = mo.to(mkey(9, (uint32_t)st, 1), secYs(pos), kSpringPos) - secYs(lay[st]);
            }
        }
    }
    // a panel grows out of the thing that opened it and fades in; it fades back out when closed.
    // Leaves a saved state on the stack when it returns true (the caller restores it).
    bool panelBegin(NanoVG& g, uint32_t id, bool open, float ox, float oy, int dimKind, Color dimC) {
        const float p = mo.to(mkey(5, id), open ? 1.f : 0.f, open ? kSpringUI : kSpringClose);
        if (!open && p < 0.004f) return false;
        const float a = clampf(p, 0, 1);
        g.save();
        g.globalAlpha(alphaBase * a);
        if (dimKind == 1) rc(g, 84.5f, 31, kW - 84.5f, 505, dimC); else rc(g, 84.5f, 31, kW - 84.5f, kH - 31, dimC);
        const float sc = 0.94f + 0.06f * p;
        g.translate(ox, oy); g.scale(sc, sc); g.translate(-ox, -oy);
        return true;
    }
    float hoverMix(int idx, bool hov) { return clampf(mo.to(mkey(6, (uint32_t)idx, layoutGen), hov ? 1.f : 0.f, kSpringHover), 0, 1); }
    // slider positions glide to new values (presets, resets, typed values); a drag and small steady
    // changes (automation) are followed exactly
    float dispN(const Ctl& c, int idx) {
        const float t = toNorm(c.param, S.values[c.param]);
        const uint64_t k = mkey(1, (uint32_t)c.param);
        if (idx == dragIdx || mo.reduced || (mo.isSettled(k) && std::fabs(t - mo.target(k, t)) < 0.04f)) { mo.hold(k, t); return t; }
        return clampf(mo.to(k, t, kSpringUI), 0, 1);
    }
    static bool pressable(CtlType t) { return t == tButton || t == tOption || t == tToggle || t == tOnOff || t == tSectionPower || t == tEqPos || t == tTab; }
    // a control, wherever its module is right now, with a small dip while it is pressed
    void drawCtlM(NanoVG& g, int i) {
        const Ctl& c = ctls[(size_t)i];
        bool saved = false;
        if (c.stage >= 0 && S.page == 1) {
            const float dx = stOffX[c.stage], dy = stOffY[c.stage];
            const bool dragged = reorderFrom >= 0 && reorderMoved && S.order[reorderFrom] == c.stage;
            if (dx != 0 || dy != 0 || dragged) { g.save(); saved = true; g.translate(dx, dy); if (dragged) g.globalAlpha(alphaBase * pageAlpha * .4f); }
        }
        if (pressable(c.type)) {
            const float pr = mo.to(mkey(10, (uint32_t)i, layoutGen), i == pressIdx ? 1.f : 0.f, kSpringPress);
            if (pr > 0.002f) {
                if (!saved) { g.save(); saved = true; }
                const float cx = c.x + c.w * .5f, cy = c.y + c.h * .5f, k = 1.f - 0.05f * pr;
                g.translate(cx, cy); g.scale(k, k); g.translate(-cx, -cy);
            }
        }
        drawCtl(g, c, i);
        if (saved) g.restore();
    }

    // ---------------------------------------------------------------- input (window coords)
    bool mouse(int button, bool press, float wx, float wy, unsigned mod, unsigned timeMs) {
        const bool r = mouseImpl(button, press, wx, wy, mod, timeMs);
        syncKeyFocus(press);
        return r;
    }
    bool mouseImpl(int button, bool press, float wx, float wy, unsigned mod, unsigned timeMs) {
        const float x = (wx - offX) / scale, y = (wy - offY) / scale;
        if (press && editIdx >= 0) { commitEdit(); }
        if (button == 1 && !press && pressIdx >= 0) { pressIdx = -1; host->hostRepaint(); }
        if (nameOpen) { if (press && button == 1) nameClick(x, y); return true; }
        if (presetOpen) { if (press) popupClick(button, x, y); return true; }
        if (modOpen >= 0) { if (press) modClick(x, y); return true; }
        if (S.page == 1 && S.bakeOpen && bakeMouse(button, press, x, y)) return true;
        if (button == 1) {
            if (press) return leftDown(x, y, mod, timeMs);
            return leftUp(x, y);
        }
        if (button == 2 && press) { // right
            const int i = hit(x, y);
            if (i >= 0 && isSlider(ctls[i].type)) {
                editIdx = i; editText.clear(); host->hostRepaint(); return true;
            }
        }
        return false;
    }
    bool motion(float wx, float wy, unsigned mod) {
        const float x = (wx - offX) / scale, y = (wy - offY) / scale;
        if (nameOpen) { const int z = nameZoneAt(x, y); if (z != nameHover) { nameHover = z; host->hostRepaint(); } return true; }
        if (presetOpen) { popupMotion(x, y); return true; }
        if (modOpen >= 0) { const int h = modItemAt(x, y); if (h != modHover) { modHover = h; host->hostRepaint(); } return true; }
        if (S.page == 1 && S.bakeOpen && bakeMotion(x, y)) return true;
        if (resizing) {
            const float ns = clampf(std::max(wx / kW, wy / kH), 0.75f, 2.0f);
            host->hostResize((unsigned)std::lround(kW * ns), (unsigned)std::lround(kH * ns));
            return true;
        }
        if (dragIdx >= 0) {
            const Ctl& c = ctls[dragIdx];
            const float fine = (mod & 1u) ? 0.15f : 1.f;
            float dn = 0;
            if (c.type == tVSlider) dn = -(y - lastY) / (c.b - c.a - 20.f);
            else if (c.type == tNumBox) dn = -(y - lastY) / 150.f;
            else dn = ((x - lastX) - (y - lastY) * 0.5f) / std::max(20.f, c.b - c.a);
            lastX = x; lastY = y;
            dragNorm = clampf(dragNorm + dn * fine, 0, 1);
            const float v = fromNorm(c.param, dragNorm);
            if (v != S.values[c.param]) setValue(c.param, v);
            host->hostRepaint();
            return true;
        }
        if (reorderFrom >= 0) {
            dragX = x; dragY = y;
            if (std::fabs(x - pressX) + std::fabs(y - pressY) > 5.f) reorderMoved = true;
            host->hostRepaint(); return true;
        }
        const int h = hit(x, y);
        if (h != hoverIdx) { hoverIdx = h; host->hostRepaint(); }
        return false;
    }
    bool scroll(float wx, float wy, float dy, unsigned mod) {
        const float x = (wx - offX) / scale, y = (wy - offY) / scale;
        if (modOpen >= 0) return true;
        if (S.page == 1 && S.bakeOpen && x >= kBX && x <= kBX + kBW && y >= kBY && y <= kBY + kBH) {
            if (x >= kLX - 12) {
                const int want = S.bakeScroll + (dy > 0 ? -1 : 1), ns = std::max(0, std::min(want, std::max(0, (int)bakeItems.size() - kBakeRows)));
                if (ns != want && (int)bakeItems.size() > 0) mo.kick(mkey(8, 1), dy > 0 ? -9.f : 9.f);
                S.bakeScroll = ns; host->hostRepaint();
            }
            return true;
        }
        if (nameOpen) return true;
        if (presetOpen) {
            ctxOpen = false;
            const int want = popScroll + (dy > 0 ? -1 : 1), ns = std::max(0, std::min(want, popMaxScroll()));
            if (ns != want) mo.kick(mkey(8, 0), dy > 0 ? -12.f : 12.f);   // the end of the list gives a little
            popScroll = ns; popHover = -1; jumpSel = -1; popupMotion(x, y); host->hostRepaint(); return true;
        }
        const int i = hit(x, y);
        if (i < 0 || !isSlider(ctls[i].type)) return false;
        const Ctl& c = ctls[i];
        const float step = ((mod & 1u) ? 0.005f : 0.025f) * (dy > 0 ? 1.f : -1.f);
        const float n = clampf(toNorm(c.param, S.values[c.param]) + step, 0, 1);
        host->hostBeginEdit(c.param); setValue(c.param, fromNorm(c.param, n)); host->hostEndEdit(c.param);
        host->hostRepaint();
        return true;
    }
    bool key(unsigned keyCode, bool press) {
        const bool r = keyImpl(keyCode, press);
        syncKeyFocus(false);
        return r;
    }
    bool keyImpl(unsigned keyCode, bool press) {
        if (nameOpen) return nameKey(keyCode, press);
        if (presetOpen) {
            if (press && keyCode == 27) { if (ctxOpen || pendingDelete >= 0) { ctxOpen = false; pendingDelete = -1; } else presetOpen = false; host->hostRepaint(); }
            return true;
        }
        if (modOpen >= 0) { if (press && keyCode == 27) { modOpen = -1; host->hostRepaint(); } return true; }
        if (S.bakeOpen && press && keyCode == 27 && editIdx < 0) { S.bakeOpen = false; host->hostRepaint(); return true; }
        if (editIdx < 0 || !press) return editIdx >= 0;
        if (keyCode == 13 || keyCode == 10) { commitEdit(); return true; }
        if (keyCode == 27) { editIdx = -1; host->hostRepaint(); return true; }
        if (keyCode == 8 || keyCode == 127) { if (!editText.empty()) editText.pop_back(); host->hostRepaint(); return true; }
        return true;
    }
    bool character(unsigned ch) {
        if (nameOpen) { nameChar(ch); return true; }
        if (editIdx < 0) return false;
        if (((ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '+' || ch == 'o' || ch == 'f') && editText.size() < 10) {
            editText.push_back((char)ch); host->hostRepaint();
        }
        return true;
    }

    float currentScale() const { return scale; }

    // ---- interface state, also saved with the project as plugin state "ui"
    std::string encodeUiState() const {
        std::ostringstream o;
        o << "v1;pv=2;page=" << S.page << ";bake=" << (S.bakeOpen ? 1 : 0) << ";bulk=" << (S.bulkMode ? 1 : 0)
          << ";bscroll=" << S.bakeScroll << ";preset=" << S.presetIdx << ";pmod=" << (S.presetModified ? 1 : 0)
          << ";ab=" << S.abCur << ";dark=" << (S.dark ? 1 : 0) << ";cap=" << (S.captureTab ? 1 : 0) << ";capbars=" << S.captureBars;
        for (int k = 0; k < 2; ++k) {
            if (!S.ab[k].valid) continue;
            o << (k ? ";b=" : ";a=");
            for (uint32_t p = 0; p < kFirstOutputParam; ++p) { char t[24]; std::snprintf(t, sizeof t, "%s%.6g", p ? "," : "", S.ab[k].v[p]); o << t; }
            o << "|";
            for (int i = 0; i < kNumStages; ++i) o << S.ab[k].order[i];
        }
        return o.str();
    }
    // a saved state arriving from the host (project load): only used if this session hasn't been touched yet
    void uiStateChanged(const char* s) {
        if (s == nullptr) return;
        S.savedUi = s;
        if (S.touched) return;
        applyUiState(s);
        lastUiState = encodeUiState();
        buildLayout();
        host->hostRepaint();
    }
    void applyUiState(const char* s) {
        if (s == nullptr || std::strncmp(s, "v1;", 3) != 0) return;
        int rawPreset = -1, presetListV = 1;
        const std::string str(s + 3);
        size_t pos = 0;
        while (pos < str.size()) {
            size_t end = str.find(';', pos);
            if (end == std::string::npos) end = str.size();
            const std::string kv = str.substr(pos, end - pos);
            pos = end + 1;
            const size_t eq = kv.find('=');
            if (eq == std::string::npos) continue;
            const std::string k = kv.substr(0, eq), val = kv.substr(eq + 1);
            const int iv = std::atoi(val.c_str());
            if (k == "page") S.page = iv == 1 ? 1 : 0;
            else if (k == "bake") S.bakeOpen = iv == 1;
            else if (k == "bulk") S.bulkMode = iv == 1;
            else if (k == "bscroll") S.bakeScroll = std::max(0, iv);
            else if (k == "preset") rawPreset = iv;
            else if (k == "pv") presetListV = iv;
            else if (k == "pmod") S.presetModified = iv == 1;
            else if (k == "ab") S.abCur = iv == 1 ? 1 : 0;
            else if (k == "dark") S.dark = iv != 0;
            else if (k == "cap") S.captureTab = iv == 1;
            else if (k == "capbars") S.captureBars = (iv == 0 || iv == 1 || iv == 2 || iv == 4 || iv == 8 || iv == 16) ? iv : 4;
            else if (k == "a" || k == "b") {
                Snapshot& sn = S.ab[k == "b" ? 1 : 0];
                const size_t bar = val.find('|');
                const std::string nums = val.substr(0, bar);
                for (uint32_t q = 0; q < kFirstOutputParam; ++q) sn.v[q] = kParamInfo[q].def;
                const char* c = nums.c_str();
                for (uint32_t q = 0; q < kFirstOutputParam && *c; ++q) {
                    sn.v[q] = clampf((float)std::strtod(c, nullptr), kParamInfo[q].min, kParamInfo[q].max);
                    const char* nx = std::strchr(c, ',');
                    if (nx == nullptr) break;
                    c = nx + 1;
                }
                if (bar == std::string::npos || !normalizeOrder(val.substr(bar + 1).c_str(), sn.order))
                    for (int i = 0; i < kNumStages; ++i) sn.order[i] = kDefaultOrder[i];
                sn.valid = true;
            }
        }
        if (rawPreset >= 0) {
            // sessions saved before 2.0 (no "pv"): user presets came right after the 34 factory ones
            if (presetListV < 2 && rawPreset >= kNumFactoryV1) rawPreset += kNumFactory - kNumFactoryV1;
            S.presetIdx = std::max(0, std::min(rawPreset, numPresets() - 1));
        }
        if (S.bakeOpen) { ensureBaker(); bakeItems = S.baker->snapshot(); S.capture.ensure(S.sampleRate.load()); }
    }
    void syncUiState() {
        const std::string now = encodeUiState();
        if (now == lastUiState) return;
        lastUiState = now;
        S.touched = true;       // the user changed something: live state wins from now on
        S.savedUi = now;
        host->hostSetUiState(now.c_str());
    }

    // ---- bake: files arriving from drag and drop or the file browser
    bool dropAccepting() const { return S.page == 1 && S.bakeOpen; }
    void setDropHover(bool over) { if (dropOver != over) { dropOver = over; host->hostRepaint(); } }
    void filesDropped(const std::vector<std::string>& paths) {
        if (!S.bakeOpen) return;
        ensureBaker();
        bool hadFolder = false;
        std::vector<std::string> audio = expandAudioPaths(paths, &hadFolder); // folders: every loop inside
        bakeNote = audio.empty() ? (hadFolder ? "no audio files in that folder" : "that's not an audio file (wav, aiff, flac, mp3)") : "";
        if (!audio.empty()) {
            S.captureTab = false;   // files dropped: back to baking them
            if (hadFolder && audio.size() > 1) S.bulkMode = true; // a folder of loops is a bulk job
            if (!S.bulkMode) audio.resize(1);
            S.baker->addFiles(audio, !S.bulkMode);
        }
        bakeItems = S.baker->snapshot();
        dropOver = false;
        host->hostRepaint();
    }

private:
    ViewHost* host;
    std::vector<Ctl> ctls;
    int hoverIdx = -1, dragIdx = -1, editIdx = -1;
    std::string editText;
    float dragNorm = 0, lastX = 0, lastY = 0;
    int lastClickIdx = -1; unsigned lastClickTime = 0;
    float scale = 1, offX = 0, offY = 0;
    // reorder
    int reorderFrom = -1; float dragX = 0, dragY = 0, pressX = 0, pressY = 0; bool reorderMoved = false;
    // resize
    bool resizing = false; float resizeAnchorX = 0, resizeAnchorY = 0, resizeStartScale = 1;
    // presets / AB
    bool applyingPreset = false;
    std::vector<UserPreset> userPresets;
    // resources
    void* resCtx = nullptr;

    // ---- preset browser popup
    bool presetOpen = false;
    int popScroll = 0, popHover = -1, popHoverZone = 0; // zone: 0 name, 1 delete x, 2 yes, 3 no
    int pendingDelete = -1;                              // user preset waiting for "delete? yes / no"
    bool ctxOpen = false; int ctxIdx = -1, ctxHover = -1; float ctxX = 0, ctxY = 0;
    static constexpr float kPopX = 104, kPopY = 96, kPopW = 536, kPopH = 440, kPopRowH = 22;
    static constexpr float kCtxW = 120, kCtxRowH = 24;
    static constexpr int kPopRows = 14;
    static constexpr float kPopListY = 94;   // first row of the list, below the group bar
    // rows: header (1.. = factory group, -2 = user) or up to two preset indices
    struct PopRow { int header; int a, b; };
    std::vector<PopRow> popRows() const {
        std::vector<PopRow> rows;
        int at = 0;
        for (int gi = 0; gi < kNumGroups; ++gi) {   // the factory presets, group by group
            rows.push_back({gi + 1, -1, -1});
            const int end = std::min(kNumFactory, at + kGroupSize[gi]);
            for (int i = at; i < end; i += 2) rows.push_back({0, kPresetDisplay[i], i + 1 < end ? kPresetDisplay[i + 1] : -1});
            at = end;
        }
        if (!userPresets.empty()) {
            rows.push_back({-2, -1, -1});
            const int n = (int)userPresets.size();
            for (int i = 0; i < n; i += 2) rows.push_back({0, kNumFactory + i, i + 1 < n ? kNumFactory + i + 1 : -1});
        }
        return rows;
    }
    int popMaxScroll() const { return std::max(0, (int)popRows().size() - kPopRows); }
    void openPresets() {
        presetOpen = true; popHover = -1; pendingDelete = -1; ctxOpen = false; jumpSel = -1; jumpHover = -1;
        const auto rows = popRows();
        int r = 0;
        for (int i = 0; i < (int)rows.size(); ++i) if (rows[i].a == S.presetIdx || rows[i].b == S.presetIdx) r = i;
        popScroll = std::max(0, std::min(r - kPopRows / 2, popMaxScroll()));
        mo.hold(mkey(8, 0), (float)popScroll);   // opens already scrolled to the current preset
    }
    static float popCellW() { return (kPopW - 48) / 2; }
    // preset under the mouse (or -1), plus the left edge of its cell and which part of the cell is hit
    int popupHit(float x, float y, float* cellX = nullptr, int* zone = nullptr) const {
        const float lx = kPopX + 24, ly = kPopY + kPopListY, cw = popCellW();
        if (x < lx - 6 || x > lx + cw * 2 || y < ly - kPopRowH / 2 || y > ly + kPopRows * kPopRowH - kPopRowH / 2) return -1;
        const int r = (int)((y - (ly - kPopRowH / 2)) / kPopRowH) + popScroll;
        const auto rows = popRows();
        if (r < 0 || r >= (int)rows.size() || rows[r].header != 0) return -1;
        const int c = x < lx + cw - 6 ? 0 : 1;
        const int idx = c ? rows[r].b : rows[r].a;
        const float cx = lx + c * cw, xr = cx + cw - 16;
        if (cellX) *cellX = cx;
        if (zone) {
            *zone = 0;
            if (idx >= kNumFactory) {
                if (idx == pendingDelete) {
                    if (x >= xr - 18 && x <= xr + 6) *zone = 3;
                    else if (x >= xr - 52 && x <= xr - 24) *zone = 2;
                } else if (x >= xr - 22 && x <= xr + 6) *zone = 1;
            }
        }
        return idx;
    }
    int popupItemAt(float x, float y) const { return popupHit(x, y); }
    int ctxItemAt(float x, float y) const {
        if (!ctxOpen || x < ctxX || x > ctxX + kCtxW || y < ctxY + 5 || y >= ctxY + 5 + 2 * kCtxRowH) return -1;
        return (int)((y - ctxY - 5) / kCtxRowH);
    }
    bool popFolderHover = false;
    bool onPresetFolderLink(float x, float y) const {
        const float w = tw(fMed, 12.5f, "open presets folder");
        return x >= kPopX + kPopW - 62 - w && x <= kPopX + kPopW - 40 && y >= kPopY + kPopH - 32 && y <= kPopY + kPopH - 8;
    }
    // the group bar under the title: one name per group (and "user"), click to jump there
    int jumpCount() const { return kNumGroups + (userPresets.empty() ? 0 : 1); }
    const char* jumpName(int i) const { return i < kNumGroups ? kGroupNames[i] : "user"; }
    int jumpHeader(int i) const { return i < kNumGroups ? i + 1 : -2; }
    float jumpX(int i) const {
        float x = kPopX + 24;
        for (int k = 0; k < i; ++k) x += tw(fMono, 9.5f, jumpName(k)) + 18;
        return x;
    }
    int jumpAt(float x, float y) const {
        if (y < kPopY + 60 || y > kPopY + 80) return -1;
        for (int i = 0; i < jumpCount(); ++i) {
            const float jx = jumpX(i), w = tw(fMono, 9.5f, jumpName(i));
            if (x >= jx - 6 && x <= jx + w + 6) return i;
        }
        return -1;
    }
    int rowOfHeader(int header) const {
        const auto rows = popRows();
        for (int r = 0; r < (int)rows.size(); ++r) if (rows[r].header == header) return r;
        return 0;
    }
    // the group the list is showing now: the one just jumped to, or else the last heading at or near the top row
    int jumpSel = -1, jumpHover = -1;
    int jumpShown() const {
        if (jumpSel >= 0 && jumpSel < jumpCount()) return jumpSel;
        int best = 0;
        for (int i = 0; i < jumpCount(); ++i) if (rowOfHeader(jumpHeader(i)) <= popScroll + 1) best = i;
        return best;
    }
    std::string presetDir() const {
        std::string p = presetPath(true);
        const size_t k = p.find_last_of("/\\");
        return k == std::string::npos ? p : p.substr(0, k);
    }
    void popupMotion(float x, float y) {
        const bool fh = onPresetFolderLink(x, y);
        if (fh != popFolderHover) { popFolderHover = fh; host->hostRepaint(); }
        int zone = 0;
        const int h = popupHit(x, y, nullptr, &zone);
        const int ch = ctxItemAt(x, y);
        const int jh = ctxOpen ? -1 : jumpAt(x, y);
        if (h != popHover || zone != popHoverZone || ch != ctxHover || jh != jumpHover) { popHover = h; popHoverZone = zone; ctxHover = ch; jumpHover = jh; host->hostRepaint(); }
    }
    void popupClick(int button, float x, float y) {
        if (ctxOpen) {
            const int it = ctxItemAt(x, y);
            ctxOpen = false;
            if (it == 0) openNamePopup(1, ctxIdx);
            else if (it == 1) pendingDelete = ctxIdx;
            host->hostRepaint();
            return;
        }
        const bool inside = x >= kPopX && x <= kPopX + kPopW && y >= kPopY && y <= kPopY + kPopH;
        const bool onClose = inside && x >= kPopX + kPopW - 52 && y <= kPopY + 46;
        if (button == 1 && onPresetFolderLink(x, y)) { openFolderInShell(presetDir()); host->hostRepaint(); return; }
        if (button == 1 && pendingDelete < 0) {   // group bar: the list glides to that group
            const int j = jumpAt(x, y);
            if (j >= 0) { jumpSel = j; popScroll = std::max(0, std::min(rowOfHeader(jumpHeader(j)), popMaxScroll())); host->hostRepaint(); return; }
        }
        int zone = 0;
        const int item = popupHit(x, y, nullptr, &zone);
        if (pendingDelete >= 0) {              // waiting for "delete? yes / no": anything but yes cancels
            const int pd = pendingDelete;
            pendingDelete = -1;
            if (item == pd && zone == 2) deleteUserPreset(pd);
            host->hostRepaint();
            return;
        }
        if (button == 2) {                     // right-click a user preset: rename / delete
            if (item >= kNumFactory) {
                ctxOpen = true; ctxIdx = item; ctxHover = -1;
                ctxX = std::min(x, kW - kCtxW - 6); ctxY = std::min(y, kH - 2 * kCtxRowH - 16);
            }
            host->hostRepaint();
            return;
        }
        if (button != 1) return;
        if (item >= kNumFactory && zone == 1) pendingDelete = item;
        else if (item >= 0) { loadPreset(item); presetOpen = false; }
        else if (!inside || onClose) presetOpen = false;
        host->hostRepaint();
    }
    void closeGlyph(NanoVG& g, float x0, float y0, float s, Color c) {
        ln(g, x0, y0, x0 + s, y0 + s, c, 1.6f);
        ln(g, x0, y0 + s, x0 + s, y0, c, 1.6f);
    }
    void drawPresetPopup(NanoVG& g) {
        const float PX = kPopX, PY = kPopY, PW = kPopW, PH = kPopH;   // (the dim behind it: panelBegin)
        rc(g, PX, PY, PW, PH, T.paper);
        ro(g, PX, PY, PW, PH, T.ink, 1.5f);
        splitText(g, fWide, 30, PX + 24, PY + 18, "presets.", T.ink);
        char b[48];
        std::snprintf(b, sizeof b, "%d", numPresets());
        txt(g, fMono, 9, PX + 24 + measureText(fWide, 30, "presets.", -.02f) + 10, PY + 34, b, T.ink);
        txt(g, fMono, 8.5f, PX + PW - 66, PY + 16, "fig. 02", T.ink2, 2);
        coordStr(b, sizeof b, PX, PY);
        txt(g, fMono, 8.5f, PX + PW - 66, PY + 28, b, T.ink, 2);
        closeGlyph(g, PX + PW - 40, PY + 16, 14, T.ink);
        ln(g, PX + 24, PY + 56, PX + PW - 24, PY + 56, T.hair);
        {   // group bar; the red underline slides to the group the list is showing
            const int shown = jumpShown();
            for (int i = 0; i < jumpCount(); ++i) {
                const float jx = jumpX(i);
                const float hv = clampf(mo.to(mkey(12, (uint32_t)i), i == jumpHover ? 1.f : 0.f, kSpringHover), 0, 1);
                txt(g, fMono, 9.5f, jx, PY + 65, jumpName(i), mixc(mixc(T.grey, T.ink2, hv), T.ink, i == shown ? 1.f : 0.f));
            }
            const float ux = mo.to(mkey(12, 100), jumpX(shown), kSpringUI), uw = mo.to(mkey(12, 101), tw(fMono, 9.5f, jumpName(shown)), kSpringUI);
            rc(g, ux, PY + 77, uw, 1.5f, T.red);
        }
        const auto rows = popRows();
        const float lx = PX + 24, ly = PY + kPopListY, cw = popCellW();
        const float fs = mo.to(mkey(8, 0), (float)popScroll, kSpringScroll);   // the list glides between rows
        const int base = (int)std::floor(fs);
        const float fr = fs - (float)base;
        g.save();
        g.intersectScissor(PX + 2, ly - kPopRowH / 2, PW - 22, kPopRows * kPopRowH);
        for (int k = 0; k <= kPopRows; ++k) {
            const int r = k + base;
            if (r < 0) continue;
            if (r >= (int)rows.size()) break;
            const float y = ly + ((float)k - fr) * kPopRowH;
            const PopRow& row = rows[r];
            if (row.header != 0) {
                const char* h = row.header > 0 ? kGroupNames[row.header - 1] : "user";
                txt(g, fMono, 8.5f, lx, y - 5, h, T.ink2);
                char nb[8]; std::snprintf(nb, sizeof nb, "%d", row.header > 0 ? kGroupSize[row.header - 1] : (int)userPresets.size());
                const float hx = lx + tw(fMono, 8.5f, h) + 8;
                txt(g, fMono, 7.5f, hx, y - 4.5f, nb, T.grey);
                ln(g, hx + tw(fMono, 7.5f, nb) + 10, y, lx + cw * 2 - 12, y, T.hair2);
                continue;
            }
            for (int c = 0; c < 2; ++c) {
                const int idx = c ? row.b : row.a;
                if (idx < 0) continue;
                const float x = lx + c * cw, xr = x + cw - 16;
                const bool cur = idx == S.presetIdx, hov = idx == popHover && !ctxOpen, user = idx >= kNumFactory;
                const bool pend = idx == pendingDelete;
                if (hov || pend || (ctxOpen && idx == ctxIdx)) rc(g, x - 6, y - kPopRowH / 2 + 1, cw - 10, kPopRowH - 2, T.hoverRow);
                if (cur) rc(g, x, y - 3.5f, 7, 7, T.red);
                std::snprintf(b, sizeof b, "%02d", displayPos(idx) + 1);
                txt(g, fMono, 9, x + 14, y - 5, b, T.grey);
                const float nameMax = pend ? cw - 40 - 96 : (user ? cw - 40 - 34 : cw - 50);
                const std::string nm = fitText(fMed, 13, presetName(idx), nameMax);
                txt(g, fMed, 13, x + 40, y - 6.5f, nm.c_str(), (cur || pend) ? T.red : (hov ? T.ink : T.ink2));
                if (pend) {
                    txt(g, fMono, 9, xr - 58, y - 5, "delete?", T.red, 2);
                    txt(g, fSemi, 12.5f, xr - 28, y - 6.25f, "yes", T.red, 2);
                    if (popHover == idx && popHoverZone == 2) rc(g, xr - 28 - tw(fSemi, 12.5f, "yes"), y + 7, tw(fSemi, 12.5f, "yes"), 1.2f, T.red);
                    txt(g, fMed, 12.5f, xr, y - 6.25f, "no", popHover == idx && popHoverZone == 3 ? T.ink : T.grey, 2);
                } else if (user && hov) {
                    const Color xc = popHoverZone == 1 ? T.red : T.grey;
                    closeGlyph(g, xr - 12, y - 4, 8, xc);
                }
            }
        }
        g.restore();
        if ((int)rows.size() > kPopRows) {
            const float tx = PX + PW - 14, ty = ly - 10, th = kPopRows * kPopRowH;
            ln(g, tx, ty, tx, ty + th, T.hair2);
            const float frac = (float)kPopRows / rows.size(), pos = clampf(fs / std::max(1, popMaxScroll()), 0, 1);
            rc(g, tx - 1.5f, ty + pos * th * (1 - frac), 3, th * frac, T.ink);
        }
        if (!userPresets.empty()) txt(g, fMono, 8, PX + 24, PY + PH - 24, "right-click a user preset to rename or delete it", T.grey);
        {
            const Color fc = popFolderHover ? T.red : T.ink;
            const float w = tw(fMed, 12.5f, "open presets folder");
            txt(g, fMed, 12.5f, PX + PW - 58 - w, PY + PH - 26, "open presets folder", fc);
            arrowNE(g, PX + PW - 52, PY + PH - 23, 7, fc);
        }
        cells(g, PX + PW - 30, PY + PH - 30, kCorner3, 3, 5, acc1());
        if (ctxOpen) {
            rc(g, ctxX, ctxY, kCtxW, 2 * kCtxRowH + 10, T.paper);
            ro(g, ctxX, ctxY, kCtxW, 2 * kCtxRowH + 10, T.ink, 1.2f);
            const char* items[2] = {"rename", "delete"};
            for (int i = 0; i < 2; ++i) {
                const float ry = ctxY + 5 + i * kCtxRowH;
                if (ctxHover == i) rc(g, ctxX + 4, ry + 1, kCtxW - 8, kCtxRowH - 2, T.hoverRow);
                txt(g, fMed, 13, ctxX + 14, ry + 5.5f, items[i], i == 1 ? T.red : T.ink);
            }
        }
    }

    // ---- naming a preset (save / rename)
    bool keysGrabbed = false;
    // typing (preset names, typed values) needs keyboard focus: grab it while a text box is open
    // (again on every click, in case the host took it back) and give it back when it closes
    void syncKeyFocus(bool click) {
        const bool want = nameOpen || editIdx >= 0;
        if (want && (click || !keysGrabbed)) { host->hostKeyboardFocus(true); keysGrabbed = true; }
        else if (!want && keysGrabbed) { host->hostKeyboardFocus(false); keysGrabbed = false; }
    }
    bool nameOpen = false, nameFresh = true;
    int nameMode = 0, nameTarget = -1, nameHover = 0; // mode 0 save, 1 rename; hover 1 save, 2 cancel, 3 close
    std::string nameText;
    static constexpr float kNX = 212, kNY = 220, kNW = 320, kNH = 160;
    static constexpr size_t kNameMax = 28;
    std::string nextUserName() const {
        for (int n = (int)userPresets.size() + 1;; ++n) {
            char b[32]; std::snprintf(b, sizeof b, "user %02d", n);
            bool used = false;
            for (const auto& up : userPresets) if (up.name == b) used = true;
            if (!used) return b;
        }
    }
    static std::string cleanName(const std::string& s) {
        std::string o;
        for (char ch : s) if ((unsigned char)ch >= 32 && (unsigned char)ch < 127 && !(ch == ' ' && (o.empty() || o.back() == ' '))) o += ch;
        while (!o.empty() && o.back() == ' ') o.pop_back();
        if (o.size() > kNameMax) o.resize(kNameMax);
        return o;
    }
    void openNamePopup(int mode, int target) {
        nameOpen = true; nameMode = mode; nameTarget = target; nameFresh = true; nameHover = 0;
        nameText = mode == 0 ? nextUserName() : presetName(target);
        editIdx = -1;
    }
    void commitName() {
        std::string nm = cleanName(nameText);
        nameOpen = false;
        if (nameMode == 0) saveUserPreset(nm.empty() ? nextUserName() : nm);
        else if (!nm.empty() && nameTarget >= kNumFactory) renameUserPreset(nameTarget, nm);
        host->hostRepaint();
    }
    int nameZoneAt(float x, float y) const {
        if (x >= kNX + kNW - 20 - 76 && x <= kNX + kNW - 20 && y >= kNY + 118 && y <= kNY + 144) return 1;
        const float cw = tw(fMed, 13, "cancel");
        if (x >= kNX + kNW - 110 - cw - 6 && x <= kNX + kNW - 110 + 6 && y >= kNY + 116 && y <= kNY + 146) return 2;
        if (x >= kNX + kNW - 44 && y >= kNY && y <= kNY + 40 && x <= kNX + kNW) return 3;
        return 0;
    }
    void nameClick(float x, float y) {
        const bool inside = x >= kNX && x <= kNX + kNW && y >= kNY && y <= kNY + kNH;
        const int z = nameZoneAt(x, y);
        if (z == 1) { commitName(); return; }
        if (z == 2 || z == 3 || !inside) nameOpen = false;
        else if (x >= kNX + 20 && x <= kNX + kNW - 20 && y >= kNY + 72 && y <= kNY + 102) nameFresh = false; // click in the field: keep the text, type after it
        host->hostRepaint();
    }
    bool nameKey(unsigned keyCode, bool press) {
        if (!press) return true;
        if (keyCode == 13 || keyCode == 10) { commitName(); return true; } // Enter / keypad Enter
        if (keyCode == 27) { nameOpen = false; host->hostRepaint(); return true; }
        if (keyCode == 8 || keyCode == 127) {
            if (nameFresh) nameText.clear(); else if (!nameText.empty()) nameText.pop_back();
            nameFresh = false; host->hostRepaint();
        }
        return true;
    }
    void nameChar(unsigned ch) {
        if (ch < 32 || ch >= 127) return;
        if (nameFresh) { nameText.clear(); nameFresh = false; }
        if (ch == ' ' && nameText.empty()) return;
        if (nameText.size() < kNameMax) nameText.push_back((char)ch);
        host->hostRepaint();
    }
    void drawNamePopup(NanoVG& g) {
        rc(g, kNX, kNY, kNW, kNH, T.paper);
        ro(g, kNX, kNY, kNW, kNH, T.ink, 1.5f);
        txt(g, fSemi, 15, kNX + 20, kNY + 16, nameMode == 0 ? "save preset" : "rename preset", T.ink);
        closeGlyph(g, kNX + kNW - 34, kNY + 18, 10, nameHover == 3 ? T.red : T.ink);
        ln(g, kNX + 12, kNY + 44, kNX + kNW - 12, kNY + 44, T.hair);
        txt(g, fMono, 8.5f, kNX + 20, kNY + 58, "name", T.grey);
        ro(g, kNX + 20, kNY + 72, kNW - 40, 30, T.red, 1.2f);
        const std::string shown = fitText(fMed, 14, nameText, kNW - 64);
        const float w = tw(fMed, 14, shown.c_str());
        if (nameFresh && !nameText.empty()) rc(g, kNX + 28, kNY + 78, w + 4, 18, alpha(T.red, .22f));
        txt(g, fMed, 14, kNX + 30, kNY + 80, shown.c_str(), T.ink);
        if (!nameFresh || nameText.empty()) rc(g, kNX + 31 + w, kNY + 79, 1.5f, 16, T.ink);
        txt(g, fMono, 8, kNX + 20, kNY + 124, "enter = save \xC2\xB7 esc = cancel", T.grey);
        txt(g, fMed, 13, kNX + kNW - 110, kNY + 124, "cancel", nameHover == 2 ? T.ink : T.grey, 2);
        rc(g, kNX + kNW - 20 - 76, kNY + 118, 76, 26, T.yellow);
        if (nameHover == 1) ro(g, kNX + kNW - 20 - 76, kNY + 118, 76, 26, T.onMint, 1.2f);
        txt(g, fSemi, 13, kNX + kNW - 20 - 38, kNY + 124.5f, nameMode == 0 ? "save" : "rename", T.onMint, 1);
    }

    // ---- per-module presets (hidden: click a module's name on the advanced page)
    struct ModPreset { const char* name; float v[6]; };
    static int stageParams(int st, const uint32_t*& out) {
        static const uint32_t sp[kNumStages][6] = {
            {pSmpMode, pSmpRate, pSmpBits, pSmpCrunch, pSmpAA},
            {pPreFlavor, pPreDrive, pPreLowEmph, pPreColor},
            {pSqThresh, pSqRatio, pSqAttack, pSqRelease, pSqBlend, pSqSC},
            {pToneLowFreq, pToneLowAmt, pToneBiteFreq, pToneBiteAmt, pToneDepth},
            {pClipMode, pClipAmount, pClipCeiling},
            {pGate, pGateHold, pWidth, pMonoLows},
            {pTrEnv, pTrDetect},
            {pShMode, pShChar, pShAttack, pShSustain, pShFocus},
            {pRvMode, pRvDecay, pRvPre, pRvTone, pRvDuck, pRvMix},
            {pWtCenter, pWtFreq, pWtAmount, pWtFlat},
        };
        static const int cnt[kNumStages] = {5, 4, 6, 5, 3, 4, 2, 5, 6, 4};
        out = sp[st]; return cnt[st];
    }
    // first entry of every list is "default" (filled in at runtime: era defaults / parameter defaults)
    static int modPresets(int st, const ModPreset*& out) {
        //                        mode  rate(kHz) bits crunch aa
        static const ModPreset smp[] = {{"default", {}},
            {"sp-12 style", {0, 26.04f, 12, .5f, 0}}, {"sp-12 pitched down", {0, 19.5f, 12, .75f, 0}},
            {"asr warm", {1, 30, 16, .3f, 0}}, {"asr grit", {1, 24, 12, .55f, 0}},
            {"mpc60 style", {2, 40, 12, .3f, 0}}, {"mpc 2000 clean", {2, 44.1f, 16, .1f, 1}},
            {"crushed", {0, 11, 8, .85f, 0}}, {"am radio", {2, 8, 10, .5f, 1}}};
        //                        flavor drive lowem color
        static const ModPreset pre[] = {{"default", {}},
            {"console glue", {0, 4, .3f, .4f}}, {"console push", {0, 10, .4f, .5f}},
            {"tube warmth", {1, 6, .5f, .6f}}, {"tube burn", {1, 16, .6f, .8f}},
            {"tape", {2, 8, .4f, .6f}}, {"tape hot", {2, 14, .5f, .8f}},
            {"iron", {3, 8, .6f, .5f}}, {"iron fuzz", {3, 20, .8f, .9f}}};
        //                        thresh ratio attack release blend sc
        static const ModPreset sq[] = {{"default", {}},
            {"bus glue", {-18, 2.5f, 20, 200, .5f, 0}}, {"punch", {-22, 4, 15, 80, .6f, 1}},
            {"all buttons", {-30, 20, .3f, 50, .9f, 0}}, {"parallel smash", {-36, 20, 1, 90, .45f, 0}},
            {"pump", {-28, 10, 2, 40, .8f, 0}}, {"room lift", {-34, 8, .5f, 150, .7f, 1}},
            {"gentle", {-14, 2, 10, 120, .4f, 0}}};
        //                        lowf lowamt bitef biteamt depth(kHz)
        static const ModPreset tone[] = {{"default", {}},
            {"warm", {0, 3, 0, 0, 12}}, {"big low end", {0, 7, 0, 1, 16}}, {"100 hz thump", {1, 5, 0, 1.5f, 18}},
            {"snare bite", {0, 2, 0, 5, 18}}, {"air", {0, 2, 1, 5, 20}}, {"dark", {0, 4, 0, 0, 7}},
            {"radio", {1, 0, 0, 4, 9}}};
        //                        mode amount ceiling
        static const ModPreset clp[] = {{"default", {}},
            {"clean", {0, 0, 0}}, {"soft kiss", {0, .2f, -.3f}}, {"soft loud", {0, .5f, -.3f}},
            {"hard edge", {1, .4f, -.3f}}, {"hard slam", {1, .75f, -.5f}}, {"low ceiling grit", {1, .6f, -6}}};
        //                        gate hold width monolows
        static const ModPreset gw[] = {{"default", {}},
            {"wide", {-80, 60, 1.4f, 1}}, {"open stereo", {-80, 60, 1.2f, 0}}, {"narrow", {-80, 60, .6f, 1}},
            {"mono", {-80, 60, 0, 1}}, {"tight gate", {-35, 40, 1, 1}}, {"choke", {-28, 15, .8f, 1}}};
        //                        envelope detect
        static const ModPreset tr[] = {{"default", {}},
            {"light trim", {75, .5f}}, {"tight", {50, .5f}}, {"chopped", {25, .5f}}, {"gated", {5, .55f}},
            {"catch ghosts", {55, .75f}}, {"big hits only", {40, .3f}}};
        //                        mode char attack sustain focus
        static const ModPreset sh[] = {{"default", {}},
            {"kick punch", {0, 1, 40, -20, 40}}, {"kick boom", {0, 2, 0, 40, -40}},
            {"snare crack", {1, 1, 50, 0, 20}}, {"fat snare", {1, 0, 10, 45, 30}},
            {"loop snap", {2, 1, 35, -15, 20}}, {"dry loop", {2, 1, 0, -50, 0}},
            {"thick loop", {2, 2, 0, 40, 0}}, {"soften harsh", {2, 2, -30, 0, -40}}};
        //                        mode decay(s) pre(ms) tone duck mix
        static const ModPreset rvb[] = {{"default", {}},
            {"snare plate", {0, 1.6f, 20, .65f, .4f, .25f}}, {"big plate", {0, 3.2f, 30, .55f, .5f, .3f}},
            {"dark plate", {0, 2.2f, 15, .25f, .4f, .25f}}, {"tight room", {1, .4f, 0, .5f, .2f, .2f}},
            {"drum room", {1, .8f, 5, .55f, .3f, .25f}}, {"big room", {1, 1.6f, 10, .6f, .4f, .3f}},
            {"gated snare", {2, .6f, 0, .5f, 0, .35f}}, {"industrial gate", {2, 1.1f, 0, .35f, 0, .45f}},
            {"long gate", {2, 2.0f, 5, .6f, .1f, .4f}}};
        //                        center freq amount flat
        static const ModPreset wgt[] = {{"default", {}},
            {"kick weight", {0, 5.1f, 3.4f, 0}}, {"808 chest", {0, 6.3f, 4.9f, 0}}, {"sub bloom", {0, 7.7f, 3.6f, 1}},
            {"kick body", {1, 9.5f, 6, 0}}, {"tight thump", {1, 7.4f, 5.4f, 0}}, {"snare body", {1, 0, 5.8f, 1}},
            {"big weight", {0, 5.4f, 6.2f, 0}}};
        switch (st) {
        case kStSampler: out = smp; return (int)(sizeof smp / sizeof smp[0]);
        case kStPreamp: out = pre; return (int)(sizeof pre / sizeof pre[0]);
        case kStSquash: out = sq; return (int)(sizeof sq / sizeof sq[0]);
        case kStTone: out = tone; return (int)(sizeof tone / sizeof tone[0]);
        case kStClip: out = clp; return (int)(sizeof clp / sizeof clp[0]);
        case kStGateWidth: out = gw; return (int)(sizeof gw / sizeof gw[0]);
        case kStTransient: out = tr; return (int)(sizeof tr / sizeof tr[0]);
        case kStReverb: out = rvb; return (int)(sizeof rvb / sizeof rvb[0]);
        case kStWeight: out = wgt; return (int)(sizeof wgt / sizeof wgt[0]);
        default: out = sh; return (int)(sizeof sh / sizeof sh[0]);
        }
    }
    float modPresetValue(int st, int item, int k) const {
        const uint32_t* ps; stageParams(st, ps);
        const uint32_t p = ps[k];
        if (item == 0) { // default: era starting point for era-driven modules, otherwise the parameter default
            if (p >= kFirstAdvParam && p <= kLastAdvParam) return kEraDefaults[(int)clampf(std::round(S.values[pEra]), 0, 2)][p - kFirstAdvParam];
            if (p >= kFirstNewParam && p <= kLastNewParam) return kEraNew[(int)clampf(std::round(S.values[pEra]), 0, 2)][p - kFirstNewParam];
            return kParamInfo[p].def;
        }
        const ModPreset* mp; modPresets(st, mp);
        return mp[item].v[k];
    }
    bool modPresetMatches(int st, int item) const {
        const uint32_t* ps; const int n = stageParams(st, ps);
        for (int k = 0; k < n; ++k) {
            const ParamInfo& pi = kParamInfo[ps[k]];
            if (std::fabs(S.values[ps[k]] - modPresetValue(st, item, k)) > (pi.max - pi.min) * 0.002f) return false;
        }
        return true;
    }
    int modOpen = -1, modHover = -1;
    float modX = 0, modY = 0, modH = 0;
    static constexpr float kModW = 220, kModRowH = 22;
    static constexpr float kOptFont = 11.5f; // option rows on the advanced page (narrower columns since v2.0)
    void openModule(int st, int pos) {
        const ModPreset* mp; const int n = modPresets(st, mp);
        modOpen = st; modHover = -1; modShown = st;
        modOX = secX(pos) + 30; modOY = secY(pos) + 22;
        modH = 50 + n * kModRowH + 4;
        modX = clampf(secX(pos) + 3, 84, kW - kModW - 6);
        modY = secY(pos) + 40;
        if (modY + modH > kH - 8) modY = kH - 8 - modH;
    }
    int modItemAt(float x, float y) const {
        if (modOpen < 0 || x < modX || x > modX + kModW) return -1;
        const ModPreset* mp; const int n = modPresets(modOpen, mp);
        const float ly = modY + 50;
        if (y < ly - kModRowH / 2) return -1;
        const int i = (int)((y - (ly - kModRowH / 2)) / kModRowH);
        return (i >= 0 && i < n) ? i : -1;
    }
    void modClick(float x, float y) {
        const int item = modItemAt(x, y);
        const bool inside = x >= modX && x <= modX + kModW && y >= modY && y <= modY + modH;
        if (item >= 0) {
            const uint32_t* ps; const int n = stageParams(modOpen, ps);
            for (int k = 0; k < n; ++k) setValueGesture(ps[k], modPresetValue(modOpen, item, k));
            modOpen = -1;
        } else if (!inside || (x >= modX + kModW - 36 && y <= modY + 36)) modOpen = -1;
        host->hostRepaint();
    }
    void drawModulePopup(NanoVG& g) {
        static const char* names[kNumStages] = {"sampler", "preamp", "squash", "tone", "clip", "gate/width", "transient", "shape", "verb", "weight"};
        rc(g, modX, modY, kModW, modH, T.paper);
        ro(g, modX, modY, kModW, modH, T.ink, 1.5f);
        txt(g, fSemi, 15, modX + 16, modY + 13, names[modOpen], T.ink);
        txt(g, fMono, 8.5f, modX + 16 + tw(fSemi, 15, names[modOpen]) + 8, modY + 17, "presets", T.grey);
        closeGlyph(g, modX + kModW - 24, modY + 15, 10, T.ink);
        ln(g, modX + 12, modY + 36, modX + kModW - 12, modY + 36, T.hair);
        const ModPreset* mp; const int n = modPresets(modOpen, mp);
        for (int i = 0; i < n; ++i) {
            const float y = modY + 50 + i * kModRowH;
            const bool cur = modPresetMatches(modOpen, i), hov = i == modHover;
            if (hov) rc(g, modX + 8, y - kModRowH / 2 + 1, kModW - 16, kModRowH - 2, T.hoverRow);
            if (cur) rc(g, modX + 16, y - 3.5f, 7, 7, T.red);
            txt(g, fMed, 12.5f, modX + 32, y - 6.25f, mp[i].name, cur ? T.red : (hov ? T.ink : T.ink2));
        }
    }

    // ---- bake panel
    bool dropOver = false, bakeWasBusy = false;
    std::vector<Baker::Item> bakeItems;
    std::string bakeNote;
    int bakeHover = -1, bakePress = -1;
    float bakePressX = 0, bakePressY = 0;
    static constexpr float kBX = 104, kBY = 96, kBW = 840, kBH = 432, kRowH = 62;
    static constexpr float kDX = 128, kDY = 172, kDW = 380, kDH = 172; // drop zone
    static constexpr float kLX = 540, kLW = 380;                       // file list
    static constexpr int kBakeRows = 3;
    enum { zNone = -1, zClose = 1, zSingle, zBulk, zBrowse, zBakeBtn, zOpenFolder, zClear, zDragAll, zDrop, zExport,
           zCapTab, zCapLink, zRec, zGrab, zLen0 = 40, zRow0 = 100 };
    // ---- capture tab
    int lastCapState = 0;
    float grabFlashT = -1;                 // seconds since the last grab (for the flash), < 0 = none
    std::chrono::steady_clock::time_point grabFlashAt;
    std::string capNote;                   // "nothing to grab yet" etc.
    static int lenVal(int i) { static const int v[6] = {1, 2, 4, 8, 16, 0}; return v[i]; }
    static constexpr float kCapSY = 172, kCapSH = 128, kCapLenY = 316, kCapBtnY = 342, kCapBtnH = 52;
    float lenX(int i) const {
        float x = kDX + 52;
        for (int k = 0; k < i; ++k) x += tw(fMed, 14, k == 5 ? "free" : std::to_string(lenVal(k)).c_str()) + 15.f;
        return x;
    }
    int grabBars() const { return S.captureBars > 0 ? S.captureBars : 16; }
    void addClip(Capture::Clip&& c) {
        ensureBaker();
        const std::string preset = presetName(S.presetIdx) + (S.presetModified ? " edit" : "");
        char lab[96];
        if (c.bars > 0) std::snprintf(lab, sizeof lab, "capture \xC2\xB7 %d bar%s", c.bars, c.bars == 1 ? "" : "s");
        else std::snprintf(lab, sizeof lab, "capture \xC2\xB7 free");
        const std::string stem = captureStem(preset, c.bpm, c.bars, c.seconds);
        S.baker->addCapture(std::move(c.L), std::move(c.R), c.sr, stem, lab);
        S.bakeScroll = 0; bakeWasBusy = true; capNote.clear();
        bakeItems = S.baker->snapshot();
        host->hostRepaint();
    }
    void doGrab() {
        Capture::Clip c;
        if (S.capture.grab(grabBars(), c)) { addClip(std::move(c)); grabFlashAt = std::chrono::steady_clock::now(); grabFlashT = 0; }
        else capNote = "nothing to grab yet: play something through mercy";
    }
    // single tab: "or browse  ·  or capture what you hear"
    void linkLayout(float& xb, float& wb, float& xc, float& wc) const {
        wb = tw(fMed, 13.5f, "or browse"); const float wd = tw(fMed, 13.5f, "  \xC2\xB7  "); wc = tw(fMed, 13.5f, "or capture what you hear");
        xb = kDX + kDW / 2 - (wb + wd + wc) / 2; xc = xb + wb + wd;
    }
    std::string exportNote; // after "export to desktop": where the files went
    int numDone() const { int n = 0; for (const auto& it : bakeItems) n += it.state == Baker::kDone; return n; }
    int numBakeableItems() const { int n = 0; for (const auto& it : bakeItems) n += !it.capture; return n; }
    // "export to desktop": a small solid button under the file count (white on dark, black on light)
    static constexpr float kEY = 424, kEH = 24;
    float exportLinkW() const { return tw(fMed, 12, "export to desktop") + 34.f; }
    float exportLinkX() const { return kLX; }
    void exportBakes() {
        const std::vector<std::string> files = donePaths();
        if (files.empty()) return;
        std::string dir, err; int copied = 0;
        const std::string folder = exportFolderName(kAppFolder, presetName(S.presetIdx) + (S.presetModified ? " edit" : ""));
        if (exportToDesktop(files, folder, dir, copied, err)) {
            const size_t k = dir.find_last_of("/\\");
            exportNote = std::to_string(copied) + (copied == 1 ? " loop" : " loops") + " \xE2\x86\x92 desktop / " + (k == std::string::npos ? dir : dir.substr(k + 1));
            if (!err.empty()) exportNote += " (" + err + ")";
            openFolderInShell(dir);
        } else exportNote = err;
    }

    void ensureBaker() { if (!S.baker) S.baker.reset(new Baker()); }
    void toggleBake() {
        S.bakeOpen = !S.bakeOpen; modOpen = -1;
        if (S.bakeOpen) { ensureBaker(); bakeItems = S.baker->snapshot(); bakeNote.clear(); S.capture.ensure(S.sampleRate.load()); }
    }
    BakeSettings bakeSettings() const {
        BakeSettings s;
        for (uint32_t p = 0; p < kFirstOutputParam; ++p) s.v[p] = S.values[p];
        std::memcpy(s.order, S.order, sizeof s.order);
        s.presetName = presetName(S.presetIdx) + (S.presetModified ? " edit" : "");
        return s;
    }
    float tabX(int k) const { return 252.f + k * (tw(fMed, 15, "single") + 18.f); }
    int bakeZoneAt(float x, float y) const {
        if (x < kBX || x > kBX + kBW || y < kBY || y > kBY + kBH) return zNone;
        if (x >= kBX + kBW - 52 && y <= kBY + 46) return zClose;
        if (y >= 112 && y <= 146) {
            if (x >= tabX(0) - 6 && x < tabX(0) + tw(fMed, 15, "single") + 8) return zSingle;
            if (x >= tabX(1) - 8 && x < tabX(1) + tw(fMed, 15, "bulk") + 8) return zBulk;
            if (x >= tabX(2) - 8 && x < tabX(2) + tw(fMed, 15, "capture") + 8) return zCapTab;
        }
        if (S.captureTab) {
            if (y >= kCapLenY - 6 && y <= kCapLenY + 20)
                for (int i = 0; i < 6; ++i) { const float x0 = lenX(i), w = tw(fMed, 14, i == 5 ? "free" : std::to_string(lenVal(i)).c_str());
                    if (x >= x0 - 6 && x <= x0 + w + 6) return zLen0 + i; }
            if (y >= kCapBtnY && y <= kCapBtnY + kCapBtnH) {
                if (x >= kDX && x < kDX + kDW / 2 - 4) return zRec;
                if (x > kDX + kDW / 2 + 4 && x <= kDX + kDW) return zGrab;
            }
            if (x >= kDX - 4 && x <= kDX + 150 && y >= 474 && y <= 508) return zOpenFolder;
            if (x < kLX - 20) return zNone;
        }
        if (x >= kDX && x <= kDX + kDW && y >= kDY && y <= kDY + kDH) {
            if (y >= kDY + 98 && y <= kDY + 126) {
                float xb, wb, xc, wc; linkLayout(xb, wb, xc, wc);
                if (x >= xb - 4 && x <= xb + wb + 4) return zBrowse;
                if (x >= xc - 4 && x <= xc + wc + 4) return zCapLink;
            }
            return zDrop;
        }
        if (x >= kDX && x <= kDX + kDW + 20 && y >= 360 && y <= 412) return zBakeBtn;
        if (x >= kDX - 4 && x <= kDX + 150 && y >= 474 && y <= 508) return zOpenFolder;
        if (S.bulkMode && numDone() >= 1 && x >= exportLinkX() && x <= exportLinkX() + exportLinkW() && y >= kEY && y <= kEY + kEH) return zExport;
        if (y >= 164 && y <= 192) {
            if (x >= kLX + kLW - 40 && x <= kLX + kLW + 4) return zClear;
            if (S.bulkMode && x >= 764 && x < 850) return zDragAll;
        }
        if (x >= kLX - 10 && x <= kLX + kLW + 10 && y >= 198) {
            const int r = (int)((y - 198) / kRowH);
            if (r >= 0 && r < kBakeRows && r + S.bakeScroll < (int)bakeItems.size()) return zRow0 + r + S.bakeScroll;
        }
        return zNone;
    }
    std::vector<std::string> donePaths(int only = -1) const {
        std::vector<std::string> out;
        for (int i = 0; i < (int)bakeItems.size(); ++i)
            if ((only < 0 || only == i) && bakeItems[i].state == Baker::kDone && !bakeItems[i].outPath.empty()) out.push_back(bakeItems[i].outPath);
        return out;
    }
    bool bakeMouse(int button, bool press, float x, float y) {
        const bool inside = x >= kBX && x <= kBX + kBW && y >= kBY && y <= kBY + kBH;
        if (!inside) { // clicks outside the panel never close it (e.g. clicking back into the window after a drag)
            if (!press) bakePress = -1;
            return false;
        }
        if (button != 1) return true;
        const int z = bakeZoneAt(x, y);
        if (!press) { bakePress = -1; return true; }
        ensureBaker();
        switch (z) {
        case zClose: S.bakeOpen = false; break;
        case zSingle: S.bulkMode = false; S.captureTab = false; break;
        case zBulk: S.bulkMode = true; S.captureTab = false; break;
        case zCapTab: case zCapLink: S.captureTab = true; S.capture.ensure(S.sampleRate.load()); break;
        case zRec: {
            const int st = S.capture.state();
            if (st == Capture::kIdle || st == Capture::kDone) S.capture.arm(S.captureBars); else S.capture.stop();
        } break;
        case zGrab: doGrab(); break;
        case zBrowse: case zDrop: host->hostBrowseFile(); break;
        case zBakeBtn: if (numBakeableItems() > 0) { S.baker->bakeAll(bakeSettings()); bakeWasBusy = true; exportNote.clear(); } break;
        case zOpenFolder: openFolderInShell(bakeFolder(true)); break;
        case zClear: S.baker->clear(); S.bakeScroll = 0; exportNote.clear(); break;
        case zDragAll: bakePress = -2; bakePressX = x; bakePressY = y; break;
        case zExport: exportBakes(); break;
        default:
            if (z >= zRow0) { bakePress = z - zRow0; bakePressX = x; bakePressY = y; }
            else if (z >= zLen0 && z < zLen0 + 6 && S.capture.state() == Capture::kIdle) S.captureBars = lenVal(z - zLen0);
            break;
        }
        bakeItems = S.baker->snapshot();
        host->hostRepaint();
        return true;
    }
    bool bakeMotion(float x, float y) {
        if (bakePress != -1 && std::fabs(x - bakePressX) + std::fabs(y - bakePressY) > 5.f) {
            const std::vector<std::string> paths = donePaths(bakePress == -2 ? -1 : bakePress);
            bakePress = -1;
            if (!paths.empty()) host->hostStartDrag(paths); // blocks until dropped (Windows)
            host->hostRepaint();
            return true;
        }
        const int z = bakeZoneAt(x, y);
        if (z != bakeHover) { bakeHover = z; host->hostRepaint(); }
        return x >= kBX && x <= kBX + kBW && y >= kBY && y <= kBY + kBH;
    }
    std::string fitText(int f, float size, const std::string& s, float maxW) const {
        if (tw(f, size, s.c_str()) <= maxW) return s;
        std::string t = s;
        while (t.size() > 2 && tw(f, size, (t + "~").c_str()) > maxW) {
            t.pop_back();
            while (!t.empty() && ((unsigned char)t.back() & 0xC0) == 0x80) t.pop_back();
        }
        return t + "~";
    }
    void arrowRight(NanoVG& g, float x, float y, float len, Color c, float w = 1.3f) {
        ln(g, x, y, x + len, y, c, w);
        const float xs[3] = {x + len - 4, x + len, x + len - 4}, ys[3] = {y - 3.5f, y, y + 3.5f};
        poly(g, xs, ys, 3, c, w);
    }
    void arrowNE(NanoVG& g, float x, float y, float s, Color c) { // ↗ drawn as lines (the grotesk has no glyph)
        ln(g, x, y + s, x + s, y, c, 1.2f);
        ln(g, x + s * .35f, y, x + s, y, c, 1.2f);
        ln(g, x + s, y, x + s, y + s * .65f, c, 1.2f);
    }
    // ---- capture tab: the last 16 bars MERCY played, grab / record
    void drawCapture(NanoVG& g) {
        char b[96];
        const float DX = kDX, DW = kDW, SY = kCapSY, SH = kCapSH;
        CaptureBuf* c = S.capture.history();
        const double bpm = S.capture.tempo(); const float bpb = S.capture.barBeats();
        const bool synced = S.capture.transportOn() && bpm > 1;
        const int st = S.capture.state();
        if (grabFlashT >= 0) { grabFlashT = std::chrono::duration<float>(std::chrono::steady_clock::now() - grabFlashAt).count(); if (grabFlashT > .6f) grabFlashT = -1; }
        ro(g, DX, SY, DW, SH, T.hair, 1.f);
        txt(g, fMono, 8.5f, DX + 10, SY + 9, "fig. 05 \xE2\x80\x94 always listening \xC2\xB7 last 16 bars", T.ink2);
        if (bpm > 1) std::snprintf(b, sizeof b, "%.0f bpm \xC2\xB7 %d/4%s", bpm, (int)std::lround(bpb), synced ? "" : " \xC2\xB7 stopped");
        else std::snprintf(b, sizeof b, "waiting for the host's tempo");
        txt(g, fMono, 8.5f, DX + DW - 10, SY + 9, b, T.ink2, 2);
        const float x0 = DX + 10, x1 = DX + DW - 10, top = SY + 28, bot = SY + SH - 12, ymid = (top + bot) / 2, amp = (bot - top) * .46f;
        if (c) {
            const uint64_t now = c->written.load(std::memory_order_acquire);
            const double barF = c->sr * 60.0 / (bpm > 1 ? bpm : 120.0) * (bpb > 0 ? bpb : 4.f);
            const uint64_t span = (uint64_t)std::min<double>(16 * barF, c->cap - c->sr * 0.5);
            const double fpx = span / (double)(x1 - x0);
            auto X = [&](uint64_t f) { return x1 - (float)((double)(now > f ? now - f : 0) / fpx); };
            // the grab selection / the take
            uint64_t sa = 0, sb = 0; bool sel = false, take = false;
            if (S.capture.takeSoFar(sa, sb)) take = true;
            else if (synced && S.capture.lastBars(grabBars(), sa, sb)) sel = true;
            else if (now > 0) { sb = now; const uint64_t len = (uint64_t)(grabBars() * barF); sa = now > len ? now - len : 0; sel = true; }
            if (sel || take) {
                const float a = std::max(x0, X(sa)), e = std::min(x1, X(sb));
                if (e > a) {
                    const Color col = take ? T.red : T.mint;
                    rc(g, a, top - 4, e - a, bot - top + 8, alpha(col, take ? .16f : (grabFlashT >= 0 ? .14f + .3f * (1 - grabFlashT / .6f) : .10f)));
                    ro(g, a, top - 4, e - a, bot - top + 8, col, 1.f);
                    if (sel) { std::snprintf(b, sizeof b, "last %d bar%s", grabBars(), grabBars() == 1 ? "" : "s"); if (e - a > tw(fMono, 8, b) + 8) txt(g, fMono, 8, e - 4, top - 1, b, T.mint, 2); }
                }
            }
            // waveform from the peak history
            const int cols = (int)((x1 - x0) / 1.5f);
            for (int i = 0; i < cols; ++i) {
                const float x = x0 + i * 1.5f;
                const double fa = (double)now - (x1 - x) * fpx, fb = fa + fpx * 1.5;
                if (fb <= 0 || (double)now - fa > c->cap - c->sr * 0.25) continue;
                uint64_t ba = (uint64_t)std::max(0.0, fa) / CaptureBuf::kBin, bb = (uint64_t)std::max(0.0, fb) / CaptureBuf::kBin;
                float p = 0, sum = 0; int cnt = 0;
                for (uint64_t k = ba; k <= bb && k < now / CaptureBuf::kBin; ++k) { const float v = c->peaks[(uint32_t)(k % c->peakCap)]; p = std::max(p, v); sum += v * v; ++cnt; }
                if (p <= 1e-4f || cnt == 0) continue;
                // the body (energy of the column) solid, the peaks faint around it: hits read as hits even 16 bars wide
                const float hp = std::min(1.f, std::pow(p, .8f)) * amp, hb = std::min(1.f, std::pow(std::sqrt(sum / cnt), .9f) * 1.15f) * amp;
                const bool in = (sel || take) && fa >= (double)sa && fb <= (double)sb + fpx * 1.5;
                ln(g, x, ymid - hp, x, ymid + hp, in ? alpha(T.ink, .35f) : alpha(T.grey, .22f), 1.f);
                ln(g, x, ymid - hb, x, ymid + hb, in ? T.ink : alpha(T.grey, .6f), 1.f);
            }
            // bar lines, counted back from the newest one
            const uint32_t n = c->nMarks.load(std::memory_order_acquire);
            for (uint32_t k = 0; k < std::min<uint32_t>(n, 24); ++k) {
                const uint64_t f = c->mark(n - 1 - k);
                if (f > now || now - f > span) continue;
                const float x = X(f);
                ln(g, x, top - 4, x, bot + 4, T.hair2, 1.f);
                if (k > 0 && k % 4 == 0) { std::snprintf(b, sizeof b, "-%u", k); txt(g, fMono, 7.5f, x + 2, bot - 6, b, T.grey); }
            }
        } else txt(g, fMed, 13.5f, DX + DW / 2, ymid - 8, "opening\xE2\x80\xA6", T.grey, 1);
        ln(g, x1, top - 6, x1, bot + 6, T.red, 1.5f);
        txt(g, fMono, 7.5f, x1 - 3, bot - 6, "now", T.red, 2);

        // length
        txt(g, fMono, 9.5f, DX, kCapLenY + 3, "length", T.grey);
        float lx = 0;
        for (int i = 0; i < 6; ++i) {
            const std::string v = i == 5 ? "free" : std::to_string(lenVal(i));
            const bool on = S.captureBars == lenVal(i), hov = bakeHover == zLen0 + i;
            lx = lenX(i);
            txt(g, fMed, 14, lx, kCapLenY, v.c_str(), on ? T.ink : (hov ? T.ink2 : T.grey));
            if (on) rc(g, lx, kCapLenY + 18, tw(fMed, 14, v.c_str()), 1.5f, T.red);
        }
        txt(g, fMono, 9.5f, lx + tw(fMed, 14, "free") + 12, kCapLenY + 3, "bars", T.grey);

        // record / grab
        const float BY = kCapBtnY, BH = kCapBtnH, BW = DW / 2 - 4;
        const bool live = st == Capture::kRec || st == Capture::kStopReq;
        if (live) {
            const Color onRed = rgb(0xF4F2F3);   // light on red in both themes
            rc(g, DX, BY, BW, BH, T.red);
            rc(g, DX + 22, BY + BH / 2 - 5, 10, 10, onRed);
            txt(g, fBold, 17, DX + 40, BY + 16, st == Capture::kStopReq ? "finishing" : "stop", onRed, 0, .01f);
            if (S.captureBars > 0 && synced) std::snprintf(b, sizeof b, "bar %d / %d", std::min(S.capture.barsSoFar() + 1, S.captureBars), S.captureBars);
            else { uint64_t a = 0, e = 0; S.capture.takeSoFar(a, e); const double sec = c ? (double)(e - a) / c->sr : 0; std::snprintf(b, sizeof b, "%d:%02d", (int)sec / 60, (int)sec % 60); }
            txt(g, fMono, 9, DX + BW - 12, BY + 21, b, onRed, 2);
        } else {
            const bool armed = st == Capture::kArmed, hov = bakeHover == zRec;
            ro(g, DX, BY, BW, BH, armed ? T.red : (hov ? T.ink : T.hair), 1.5f);
            g.beginPath(); g.circle(DX + 27, BY + BH / 2, 5); g.fillColor(T.red); g.fill();
            txt(g, fBold, 17, DX + 40, BY + 16, armed ? "armed" : "record", T.ink, 0, .01f);
            if (armed) txt(g, fMono, 8.5f, DX + BW - 12, BY + 22, synced ? "next bar" : "first sound", T.grey, 2);
        }
        {
            const bool hov = bakeHover == zGrab;
            const float gx = DX + DW / 2 + 4;
            if (hov) rc(g, gx, BY, BW, BH, T.hoverRow);
            ro(g, gx, BY, BW, BH, hov ? T.ink : T.hair, 1.5f);
            std::snprintf(b, sizeof b, "grab last %d bar%s", grabBars(), grabBars() == 1 ? "" : "s");
            txt(g, fBold, 17, gx + BW / 2, BY + 16, b, T.ink, 1, .01f);
        }
        if (!capNote.empty()) txt(g, fMono, 9.5f, DX, 430, capNote.c_str(), T.red);
        else if (st == Capture::kArmed || live) {
            if (synced && S.captureBars > 0) std::snprintf(b, sizeof b, "recording from the bar \xC2\xB7 stops by itself after %d bar%s", S.captureBars, S.captureBars == 1 ? "" : "s");
            else if (synced) std::snprintf(b, sizeof b, "recording from the bar \xC2\xB7 hit stop when you're done");
            else std::snprintf(b, sizeof b, "transport stopped: recording starts on the first sound");
            txt(g, fMono, 9.5f, DX, 430, b, T.ink);
            txt(g, fMono, 9.5f, DX, 446, synced ? "stop finishes the bar, so the loop stays whole" : "stop ends it; the silence after is trimmed", T.ink);
        } else {
            txt(g, fMono, 9.5f, DX, 430, "captures exactly what you hear: after every knob, module and filter", T.ink);
            txt(g, fMono, 9.5f, DX, 446, "snaps to the bar \xC2\xB7 lines up to the sample \xC2\xB7 loops wrap seamlessly", T.ink);
        }
    }

    void drawBakePanel(NanoVG& g) {
        const float PX = kBX, PY = kBY, PW = kBW, PH = kBH;
        char b[160];
        rc(g, PX, PY, PW, PH, T.paper);
        ro(g, PX, PY, PW, PH, T.ink, 1.5f);
        splitText(g, fWide, 30, PX + 24, PY + 18, "bake.", T.ink);
        const char* modes[3] = {"single", "bulk", "capture"};
        const int capSt = S.capture.state();
        const bool capLive = capSt == Capture::kRec || capSt == Capture::kStopReq;
        for (int k = 0; k < 3; ++k) {
            const bool sel = S.captureTab ? k == 2 : (k < 2 && (k == 1) == S.bulkMode);
            const bool hov = bakeHover == (k == 2 ? zCapTab : (k ? zBulk : zSingle));
            const float x = tabX(k);
            txt(g, fMed, 15, x, 122, modes[k], sel ? T.red : (hov ? T.ink : T.grey));
            if (sel) rc(g, x, 141, tw(fMed, 15, modes[k]), 1.5f, T.red);
            if (k == 2 && (capLive || capSt == Capture::kArmed)) {   // recording: a red dot on the tab
                g.beginPath(); g.circle(x + tw(fMed, 15, modes[k]) + 6, 124, 3); g.fillColor(capLive ? T.red : alpha(T.red, .5f)); g.fill();
            }
        }
        txt(g, fMono, 8.5f, PX + PW - 66, PY + 16, "fig. 03", T.ink2, 2);
        txt(g, fMono, 8.5f, PX + PW - 66, PY + 28, coordStr(b, sizeof b, PX + PW, PY), T.ink, 2);
        closeGlyph(g, PX + PW - 40, PY + 16, 14, bakeHover == zClose ? T.red : T.ink);
        ln(g, PX, 152, PX + PW, 152, T.hair);

        const float DX = kDX, DY = kDY, DW = kDW, DH = kDH;
        int done = 0, baking = 0;
        for (const auto& it : bakeItems) { if (it.state == Baker::kDone) ++done; if (it.state == Baker::kBaking) ++baking; }
        if (S.captureTab) drawCapture(g);
        else {
        // ---- drop zone: a faint score of loops behind the words
        if (dropOver) rc(g, DX, DY, DW, DH, alpha(T.red, .08f));
        {
            Rng rr(9);
            for (int row = 0; row < 5; ++row) {
                const float yy = DY + 26 + 30 * row;
                float xx = DX + 22;
                while (xx < DX + DW - 40) {
                    const float w = rr.range(26, 70), a = rr.range(4, 9);
                    const Hit h[1] = {{0.f, 1.f, .5f}};
                    staticBlob(g, xx, yy, w, a, h, 1, (uint32_t)(rr.uni() * 999), T.scan, 40, .15f, 1.f);
                    xx += w + rr.range(6, 22);
                }
                ln(g, DX + 16, yy, DX + DW - 16, yy, T.hair2, .8f);
            }
            cross(g, DX + 62, DY + 56, 6, T.grey);
            cross(g, DX + 312, DY + 116, 6, T.grey);
            txt(g, fMono, 7, DX + 70, DY + 44, "x: 190 y: 228", T.ink);
            txt(g, fMono, 7, DX + 320, DY + 104, "x: 440 y: 288", T.ink);
        }
        dashRect(g, DX, DY, DW, DH, dropOver ? T.red : T.ink, 1.5f, 6, 5);
        const char* big = dropOver ? "let go to load" : "drop audio here";
        {
            const float w = tw(fSemi, 23, big);
            rc(g, DX + DW / 2 - w / 2 - 10, DY + 56, w + 20, 36, T.paper);
            txt(g, fSemi, 23, DX + DW / 2, DY + 62, big, dropOver ? T.red : T.ink, 1);
            float xb, wb, xc, wc; linkLayout(xb, wb, xc, wc);
            const Color bc = bakeHover == zBrowse ? T.red : T.ink2, cc = bakeHover == zCapLink ? T.red : T.ink2;
            rc(g, xb - 6, DY + 100, xc + wc - xb + 12, 22, T.paper);
            txt(g, fMed, 13.5f, xb, DY + 104, "or browse", bc);
            rc(g, xb, DY + 104 + 13.5f * .92f + 4, wb, 1, bc);
            txt(g, fMed, 13.5f, xb + wb, DY + 104, "  \xC2\xB7  ", T.grey);
            txt(g, fMed, 13.5f, xc, DY + 104, "or capture what you hear", cc);
            rc(g, xc, DY + 104 + 13.5f * .92f + 4, wc, 1, cc);
            const char* fm = bakeNote.empty() ? "wav \xC2\xB7 aiff \xC2\xB7 flac \xC2\xB7 mp3 \xC2\xB7 folders" : bakeNote.c_str();
            const float wf = tw(fMono, 9, fm);
            rc(g, DX + DW / 2 - wf / 2 - 6, DY + DH - 24, wf + 12, 13, T.paper);
            txt(g, fMono, 9, DX + DW / 2, DY + DH - 22, fm, bakeNote.empty() ? T.ink : T.red, 1);
        }

        // ---- bake button
        const bool busy = S.baker && S.baker->busy();
        const int nb = numBakeableItems();
        int nbDone = 0; for (const auto& it : bakeItems) nbDone += !it.capture && it.state == Baker::kDone;
        const bool canBake = nb > 0;
        if (busy && nb > 0 && nbDone < nb) std::snprintf(b, sizeof b, "baking %d / %d", std::min(nbDone + 1, nb), nb);
        else if (S.bulkMode && nb > 1) std::snprintf(b, sizeof b, "bake all (%d)", nb);
        else std::snprintf(b, sizeof b, "bake");
        static const int stairs[][2] = {{0, 0}, {0, 1}, {1, 1}, {0, 3}, {1, 4}, {0, 6}, {1, 7}, {2, 7}};
        if (canBake) {
            rc(g, DX, 360, DW, 52, T.yellow);
            cells(g, DX + DW, 360, stairs, 8, 6.5f, T.yellow);
            if (bakeHover == zBakeBtn && !busy) ro(g, DX, 360, DW, 52, T.onMint, 1.5f);
            txt(g, fBold, 19, DX + DW / 2, 377, b, T.onMint, 1, .01f);
        } else {
            ro(g, DX, 360, DW, 52, T.hair, 1.5f);
            txt(g, fBold, 19, DX + DW / 2, 377, b, T.grey, 1, .01f);
        }
        txt(g, fMono, 9.5f, DX, 430, "uses everything you hear: era, knobs, modules, chain, filters", T.ink);
        txt(g, fMono, 9.5f, DX, 446, "loops wrap seamlessly \xC2\xB7 same length \xC2\xB7 2x quality", T.ink);
        } // single / bulk
        {
            const Color fc = bakeHover == zOpenFolder ? T.red : T.ink;
            txt(g, fMed, 13.5f, DX, 480, "open bake folder", fc);
            arrowNE(g, DX + tw(fMed, 13.5f, "open bake folder") + 6, 483, 8, fc);
            std::string f = bakeFolder(false);
            const size_t a = f.find_last_of("/\\");
            const size_t b2 = a == std::string::npos || a == 0 ? std::string::npos : f.find_last_of("/\\", a - 1);
            if (b2 != std::string::npos) f = f.substr(b2 + 1);
            std::string pretty;
            for (char ch : f) {
                if (ch == '\\' || ch == '/') pretty += " / ";
                else pretty += (char)std::tolower((unsigned char)ch);
            }
            f = pretty;
            txt(g, fMono, 9, DX, 498, fitText(fMono, 9, f, DW).c_str(), T.ink);
        }

        // ---- file list
        const float LX = kLX, LW = kLW;
        txt(g, fSemi, 15, LX, 172, "files", T.ink);
        std::snprintf(b, sizeof b, "%02d", (int)bakeItems.size());
        txt(g, fMono, 9, LX + 42, 175, b, T.ink);
        if (S.bulkMode && done >= 2) {
            const Color c = bakeHover == zDragAll ? T.ink : T.red;
            txt(g, fMed, 12.5f, 770, 173, "drag all", c);
            arrowRight(g, 774 + tw(fMed, 12.5f, "drag all"), 180.5f, 12, c);
        }
        if (!bakeItems.empty()) txt(g, fMed, 12.5f, LX + LW, 173, "clear", bakeHover == zClear ? T.ink : T.grey, 2);
        ln(g, LX, 194, LX + LW, 194, T.hair);
        if (bakeItems.empty()) {
            txt(g, fMed, 13.5f, LX + LW / 2, 262, "nothing here yet", T.grey, 1);
            txt(g, fMono, 9, LX + LW / 2, 284, "baked and captured files show up here, ready to drag", T.grey, 1);
        }
        int firstDone = -1;
        for (int k = S.bakeScroll; k < (int)bakeItems.size() && k < S.bakeScroll + kBakeRows; ++k)
            if (bakeItems[k].state == Baker::kDone) { firstDone = k; break; }
        const float fs = mo.to(mkey(8, 1), (float)S.bakeScroll, kSpringScroll);
        const int base = (int)std::floor(fs);
        const float fr = fs - (float)base;
        g.save();
        g.intersectScissor(LX - 12, 197, LW + 24, kBakeRows * kRowH + 2);
        for (int r = 0; r <= kBakeRows; ++r) {
            const int i = r + base;
            if (i < 0) continue;
            if (i >= (int)bakeItems.size()) break;
            const Baker::Item& it = bakeItems[i];
            const float ry = 204 + ((float)r - fr) * kRowH;
            const bool hov = bakeHover == zRow0 + i, isDone = it.state == Baker::kDone;
            const bool lit = isDone && (hov || (bakeHover < zRow0 && i == firstDone));
            if (r > 0 || fr > 0.01f) ln(g, LX, ry - 6, LX + LW, ry - 6, T.hair2);
            const Color tc = lit ? T.onMint : T.ink, sc = lit ? T.onMint2 : T.grey;
            if (lit) {
                rc(g, LX - 10, ry - 4, LW + 20, 56, T.mint);
                const Hit h[3] = {{.03f, .95f, .08f}, {.42f, .7f, .06f}, {.72f, .85f, .07f}};
                staticBlob(g, LX + 236, ry + 24, 80, 15, h, 3, (uint32_t)it.id * 31u + 3u, T.onMint, 80, .14f);
            }
            // status
            if (it.state == Baker::kWaiting) ro(g, LX, ry + 6, 9, 9, T.grey, 1.2f);
            else if (it.state == Baker::kBaking) rc(g, LX, ry + 6, 9, 9, T.red);
            else if (isDone) rc(g, LX, ry + 6, 9, 9, it.capture && !lit ? T.red : tc);
            else { ln(g, LX, ry + 7, LX + 9, ry + 16, T.red, 1.5f); ln(g, LX, ry + 16, LX + 9, ry + 7, T.red, 1.5f); }
            std::string nm = fitText(f550, 15, it.name, isDone ? 196.f : 250.f);
            txt(g, f550, 15, LX + 20, ry + 4, nm.c_str(), tc);
            if (it.seconds > 0) {
                std::snprintf(b, sizeof b, "%.1f s", it.seconds);
                txt(g, fMono, 9.5f, LX + 20 + tw(f550, 15, nm.c_str()) + 10, ry + 7, b, sc);
            }
            if (it.state == Baker::kWaiting) txt(g, fMono, 9.5f, LX + 20, ry + 25, "waiting", sc);
            else if (it.state == Baker::kBaking) {
                std::snprintf(b, sizeof b, "baking %d%%", (int)std::lround(it.progress * 100));
                txt(g, fMono, 9.5f, LX + 20, ry + 25, b, T.red);
                rc(g, LX + 20, ry + 42, LW - 20, 4, T.pink);
                rc(g, LX + 20, ry + 42, (LW - 20) * clampf(it.progress, 0, 1), 4, T.red);
            } else if (isDone) {
                std::string o = "> " + it.outName + (it.asFloat ? "  32-bit" : "");
                txt(g, fMono, 9.5f, LX + 20, ry + 25, fitText(fMono, 9.5f, o, 208).c_str(), sc);
                const Color dc = lit ? T.onMint : T.red;
                txt(g, fMed, 12.5f, LX + LW - 42, ry + 15, "drag", dc);
                arrowRight(g, LX + LW - 42 + tw(fMed, 12.5f, "drag") + 5, ry + 21.5f, 11, dc);
            } else txt(g, fMono, 9.5f, LX + 20, ry + 25, fitText(fMono, 9.5f, it.error, LW - 30).c_str(), T.red);
        }
        g.restore();
        if ((int)bakeItems.size() > kBakeRows) {
            const float tx = LX + LW + 14, ty = 198, th = kBakeRows * kRowH;
            ln(g, tx, ty, tx, ty + th, T.hair2);
            const int maxs = (int)bakeItems.size() - kBakeRows;
            const float frac = (float)kBakeRows / bakeItems.size(), pos = clampf(fs / std::max(1, maxs), 0, 1);
            rc(g, tx - 1.5f, ty + pos * th * (1 - frac), 3, th * frac, T.ink);
        }
        std::snprintf(b, sizeof b, "fig. 04 \xE2\x80\x94 %d file%s \xC2\xB7 %d ready \xC2\xB7 %d in progress", (int)bakeItems.size(),
                      bakeItems.size() == 1 ? "" : "s", done, baking);
        txt(g, fMono, 8.5f, LX, 404, b, T.ink);
        // ---- bulk: export the whole batch to its own folder on the desktop
        if (S.bulkMode && done >= 1) {
            const float ex = exportLinkX(), ew = exportLinkW();
            rc(g, ex, kEY, ew, kEH, bakeHover == zExport ? T.red : T.ink);
            txt(g, fMed, 12, ex + 10, kEY + 6, "export to desktop", T.paper);
            arrowNE(g, ex + 10 + tw(fMed, 12, "export to desktop") + 6, kEY + 8.5f, 7, T.paper);
            if (!exportNote.empty()) txt(g, fMono, 8.5f, kLX, kEY + kEH + 10, fitText(fMono, 8.5f, exportNote, 300).c_str(), T.ink2);
        }
        rc(g, 856, 452, 26, 26, acc1());
        cells(g, 890, 470, kCluster, 9, 5, T.red);
    }

    std::string lastUiState;

    // ---------------------------------------------------------------- layout
    static bool isSlider(CtlType t) { return t == tVSlider || t == tDotKnob || t == tHSlider || t == tMini || t == tNumBox || t == tFilter; }

    Ctl& add(CtlType t, int param, float x, float y, float w, float h) {
        Ctl c; c.type = t; c.param = param; c.x = x; c.y = y; c.w = w; c.h = h; c.stage = curStage;
        ctls.push_back(c);
        return ctls.back();
    }

    void buildLayout() {
        ctls.clear(); ++layoutGen; curStage = -1;
        if (S.page == 0) modOpen = -1; // the bake panel is remembered across pages
        hoverIdx = -1;
        const char* tabNames[4] = {"fantasy", "throne", "yeezus", "advanced"};
        const float tabXs[4] = {104, 222, 330, 452};
        for (int i = 0; i < 4; ++i) {
            Ctl& c = add(tTab, i < 3 ? (int)pEra : -1, tabXs[i] - 6, 38, tw(f550, 21, tabNames[i]) + 12, 52);
            c.id = i; c.label = tabNames[i]; c.a = tabXs[i];
        }
        {
            const float w = measureText(fMono, 8.5f, "light / dark", .01f);
            Ctl& c = add(tButton, -1, 940 - w - 6, 6, w + 10, 24); c.id = bTheme;
        }
        if (S.page == 0) buildMain(); else buildAdvanced();
        add(tResize, -1, kW - 24, kH - 24, 24, 24);
    }

    void buildMain() {
        const char* names[3] = {"squash", "dirt", "age"};
        const uint32_t ps[3] = {pSquash, pDirt, pAge};
        for (int i = 0; i < 3; ++i) {
            const float cx = 182.f + i * 120.f;
            Ctl& c = add(tVSlider, (int)ps[i], cx - 34, 104, 68, 262);
            c.a = 152; c.b = 332; c.c = cx; c.label = names[i];
        }
        { Ctl& c = add(tDotKnob, pInput, 182 - 44, 396, 88, 48); c.a = 182 - 36; c.b = 182 + 36; c.c = 182; c.label = "input"; }
        { Ctl& c = add(tHSlider, pMix, 258, 396, 90, 48); c.a = 277; c.b = 327; c.c = 302; c.label = "mix"; }
        { Ctl& c = add(tDotKnob, pOutput, 422 - 44, 396, 88, 48); c.a = 422 - 36; c.b = 422 + 36; c.c = 422; c.label = "output"; }
        { Ctl& c = add(tToggle, pAutoGain, 100, 468, 118, 32); c.id = sAutoGain; c.label = "auto gain"; }
        { Ctl& c = add(tNumBox, pTrEnv, 264, 466, 150, 36); c.label = "transient"; }
        { Ctl& c = add(tButton, -1, 100, 546, 16, 22); c.id = bPrev; }
        { Ctl& c = add(tButton, -1, 116, 544, 94, 26); c.id = bPresetName; }
        { Ctl& c = add(tButton, -1, 210, 546, 16, 22); c.id = bNext; }
        { Ctl& c = add(tButton, -1, 236, 544, 46, 26); c.id = bSave; c.label = "save"; }
        { Ctl& c = add(tButton, -1, 292, 544, 46, 26); c.id = bAB; c.label = "a/b"; }
        { Ctl& c = add(tToggle, pBypass, 350, 544, 66, 26); c.id = sBypassBox; c.label = "bypass"; }
        { Ctl& c = add(tOption, pQuality, 492, 544, 36, 26); c.id = 0; c.label = "1x"; }
        { Ctl& c = add(tOption, pQuality, 530, 544, 36, 26); c.id = 1; c.label = "2x"; }
        { Ctl& c = add(tFilter, pHpf, 660, 476 - 12, 284, 24); c.a = 704; c.b = 880; c.c = 476; c.label = "hpf"; }
        { Ctl& c = add(tFilter, pLpf, 660, 502 - 12, 284, 24); c.a = 704; c.b = 880; c.c = 502; c.label = "lpf"; }
    }

    static constexpr float kSecW = 168.f;  // 5 columns x 2 rows (v2.0: ten modules)
    float secX(int pos) const { return secXs(pos); }
    float secY(int pos) const { return secYs(pos); }

    void buildAdvanced() {
        for (int pos = 0; pos < kNumStages; ++pos) {
            const int st = S.order[pos];
            const float sx = secX(pos), sy = secY(pos);
            curStage = st;
            { Ctl& c = add(tSectionHead, -1, sx + 8, sy + 8, kSecW - 40, 30); c.id = st; c.a = (float)pos; }
            static const uint32_t powerParam[kNumStages] = {pSmpOn, pPreOn, pSqOn, pToneOn, pClipOn, pGwOn, pTrOn, pShOn, pRvOn, pWtOn};
            { Ctl& c = add(tSectionPower, (int)powerParam[st], sx + kSecW - 32, sy + 10, 26, 22); c.id = st; c.a = sx; c.c = sy; }
            float ry = sy + 50;
            const float rs = 25.f;
            int rows = 0;
            auto mini = [&](uint32_t p, const char* label) {
                Ctl& c = add(tMini, (int)p, sx + 10, ry - 12, kSecW - 14, 24);
                c.a = sx + 74; c.b = sx + 108; c.c = ry; c.label = label; c.id = (int)(sx + kSecW - 8);
                ry += rs; ++rows;
            };
            auto rowLabel = [&](const char* label) {
                Ctl& lc = add(tButton, -1, 0, 0, 0, 0); lc.id = -1; lc.label = label; lc.a = sx + 14; lc.c = ry;
            };
            auto options = [&](uint32_t p, std::initializer_list<const char*> labels, const char* label, const char* suffix) {
                float ox = sx + 14;
                if (label) { rowLabel(label); ox = sx + 74; }
                int idx = 0;
                for (const char* l : labels) {
                    const float w = tw(fMed, kOptFont, l);
                    Ctl& c = add(tOption, (int)p, ox - 4, ry - 12, w + 8, 24);
                    c.id = idx++; c.label = l; c.a = ox; c.b = kOptFont; c.c = ry;
                    ox += w + 8;
                }
                if (suffix) { Ctl& lc = add(tButton, -1, 0, 0, 0, 0); lc.id = -1; lc.label = suffix; lc.a = ox - 4; lc.c = ry; }
                ry += rs; ++rows;
            };
            auto onoff = [&](uint32_t p, const char* label) {
                rowLabel(label);
                for (int k = 0; k < 2; ++k) {
                    const float ox = sx + 80 + k * 40;
                    Ctl& c = add(tOnOff, (int)p, ox - 4, ry - 12, 38, 24);
                    c.id = k; c.label = k ? "on" : "off"; c.a = ox; c.c = ry;
                }
                ry += rs; ++rows;
            };
            switch (st) {
            case kStSampler:
                options(pSmpMode, {"sp-12", "asr", "mpc"}, nullptr, nullptr);
                mini(pSmpRate, "rate"); mini(pSmpBits, "bits"); mini(pSmpCrunch, "crunch");
                onoff(pSmpAA, "aa filter");
                break;
            case kStPreamp:
                options(pPreFlavor, {"console", "tube", "tape", "xfmr"}, nullptr, nullptr);
                mini(pPreDrive, "drive"); mini(pPreLowEmph, "low em."); mini(pPreColor, "color"); mini(pPreThick, "thick");
                break;
            case kStSquash:
                mini(pSqThresh, "threshold"); mini(pSqRatio, "ratio"); mini(pSqAttack, "attack");
                mini(pSqRelease, "release"); mini(pSqBlend, "blend"); onoff(pSqSC, "sidechain");
                break;
            case kStTone:
                options(pToneLowFreq, {"60", "100 Hz"}, "low boost", nullptr);
                mini(pToneLowAmt, "amount");
                options(pToneBiteFreq, {"3k", "10k"}, "bite", nullptr);
                mini(pToneBiteAmt, "amount");
                mini(pToneDepth, "depth");
                break;
            case kStClip: {
                options(pClipMode, {"soft", "hard"}, nullptr, nullptr);
                rowLabel("eq");
                float ox = sx + 74;
                for (int k = 0; k < 2; ++k) {
                    const char* l = k ? "post" : "pre";
                    const float w = tw(fMed, kOptFont, l);
                    Ctl& c = add(tEqPos, -1, ox - 4, ry - 12, w + 8, 24);
                    c.id = k; c.label = l; c.a = ox; c.c = ry;
                    ox += w + 10;
                }
                ry += rs; ++rows;
                mini(pClipAmount, "amount"); mini(pClipCeiling, "ceiling");
            } break;
            case kStTransient:
                mini(pTrEnv, "envelope"); mini(pTrDetect, "detect");
                break;
            case kStShape:
                options(pShMode, {"kick", "snare", "bus"}, nullptr, nullptr);
                options(pShChar, {"classic", "natural", "smooth"}, nullptr, nullptr);
                mini(pShAttack, "attack"); mini(pShSustain, "sustain"); mini(pShFocus, "focus");
                break;
            case kStGateWidth:
                mini(pGate, "gate"); mini(pGateHold, "hold"); mini(pWidth, "width");
                onoff(pMonoLows, "mono lows");
                break;
            case kStReverb:
                options(pRvMode, {"plate", "room", "gated"}, nullptr, nullptr);
                mini(pRvDecay, "decay"); mini(pRvPre, "pre-delay"); mini(pRvTone, "tone");
                mini(pRvDuck, "duck"); mini(pRvMix, "mix");
                break;
            case kStWeight:
                options(pWtCenter, {"40", "100", "200"}, "center", nullptr);
                mini(pWtFreq, "freq"); mini(pWtAmount, "amount");
                onoff(pWtFlat, "flat");
                break;
            }
            figY[pos] = sy + 50 + rows * 25 + 2;
        }
        curStage = -1;
        { Ctl& c = add(tButton, -1, 852, 546, 94, 28); c.id = bResetOrder; c.label = "reset order"; }
        { Ctl& c = add(tButton, -1, 744, 546, 80, 30); c.id = bBake; c.label = "bake"; }
    }

    int hit(float x, float y) const {
        for (int i = (int)ctls.size() - 1; i >= 0; --i)
            if (ctls[i].w > 0 && ctls[i].contains(x, y)) return i;
        return -1;
    }

    // ---------------------------------------------------------------- actions
    void setValue(uint32_t p, float v) {
        S.values[p] = v;
        S.userTouched = true;
        if (p != pBypass && !applyingPreset) S.presetModified = true;
        host->hostSetParam(p, v);
    }
    void setValueGesture(uint32_t p, float v) {
        host->hostBeginEdit(p); setValue(p, v); host->hostEndEdit(p);
    }
    void commitEdit() {
        if (editIdx < 0 || editIdx >= (int)ctls.size()) { editIdx = -1; return; }
        const Ctl& c = ctls[editIdx];
        float v;
        if (parseValue(c.param, editText, v)) setValueGesture(c.param, v);
        editIdx = -1; editText.clear();
        host->hostRepaint();
    }
    void setOrder(const int* o) {
        std::memcpy(S.order, o, sizeof S.order);
        char s[kNumStages + 1];
        for (int i = 0; i < kNumStages; ++i) s[i] = (char)('0' + S.order[i]);
        s[kNumStages] = 0;
        host->hostSetOrder(s);
        S.presetModified = true;
        buildLayout();
    }

    void selectEra(int e) {
        const int cur = (int)std::lround(S.values[pEra]);
        if (e != cur) {
            applyingPreset = true;
            for (uint32_t i = 0; i < kNumAdvParams; ++i) setValueGesture(kFirstAdvParam + i, kEraDefaults[e][i]);
            // the era sets the reverb up (plate / room / gated) but leaves its power switch alone:
            // reverb is a choice, it should never arrive just because an era was picked
            for (uint32_t i = 0; i < kNumNewParams; ++i)
                if (kFirstNewParam + i != pRvOn) setValueGesture(kFirstNewParam + i, kEraNew[e][i]);
            setValueGesture(pEra, (float)e);
            applyingPreset = false;
            S.presetModified = true;
        }
        S.page = 0;
        buildLayout();
    }

    void applySnapshot(const Snapshot& s) {
        applyingPreset = true;
        for (uint32_t p = 0; p < kFirstOutputParam; ++p) {
            if (p == pBypass) continue;
            if (S.values[p] != s.v[p]) setValueGesture(p, s.v[p]);
        }
        applyingPreset = false;
        setOrder(s.order);
    }
    Snapshot current() const {
        Snapshot s; s.valid = true;
        for (uint32_t p = 0; p < kFirstOutputParam; ++p) s.v[p] = S.values[p];
        std::memcpy(s.order, S.order, sizeof S.order);
        return s;
    }
    Snapshot factorySnapshot(int i) const {
        const FactoryPreset& f = kFactory[i];
        Snapshot s; s.valid = true;
        for (uint32_t p = 0; p < kFirstOutputParam; ++p) s.v[p] = kParamInfo[p].def;
        s.v[pEra] = (float)f.era;
        for (uint32_t k = 0; k < kNumAdvParams; ++k) s.v[kFirstAdvParam + k] = kEraDefaults[f.era][k];
        s.v[pSquash] = f.squash; s.v[pDirt] = f.dirt; s.v[pAge] = f.age; s.v[pMix] = f.mix;
        s.v[pAutoGain] = S.values[pAutoGain]; s.v[pQuality] = S.values[pQuality];
        for (int k = 0; k < f.nov; ++k) s.v[f.ov[k].p] = f.ov[k].v;
        if (f.order == nullptr || !normalizeOrder(f.order, s.order)) for (int k = 0; k < kNumStages; ++k) s.order[k] = kDefaultOrder[k];
        return s;
    }
    int numPresets() const { return kNumFactory + (int)userPresets.size(); }
    // presets keep their index (projects store it); the browser and arrows walk them in display order
    static int displayPos(int idx) {
        if (idx >= kNumFactory) return idx;
        for (int k = 0; k < kNumFactory; ++k) if (kPresetDisplay[k] == idx) return k;
        return idx;
    }
    int fromDisplay(int pos) const {
        const int n = numPresets(); pos = ((pos % n) + n) % n;
        return pos < kNumFactory ? kPresetDisplay[pos] : pos;
    }
    std::string presetName(int i) const {
#ifdef MERCY_TEASER
        if (i < kNumFactory) return teaserName(i);
#else
        if (i < kNumFactory) return kFactory[i].name;
#endif
        return userPresets[i - kNumFactory].name;
    }
    void loadPreset(int i) {
        const int n = numPresets();
        S.presetIdx = ((i % n) + n) % n;
        Snapshot s = S.presetIdx < kNumFactory ? factorySnapshot(S.presetIdx) : userPresets[S.presetIdx - kNumFactory].s;
        applySnapshot(s);
        S.presetModified = false;
    }

    std::string presetPath(bool makeDir) const {
        std::string dir;
#ifdef _WIN32
        const char* app = std::getenv("APPDATA");
        if (app == nullptr) return "";
        dir = std::string(app) + "\\" + kAppFolder;
        if (makeDir) _mkdir(dir.c_str());
        return dir + "\\user_presets.txt";
#elif defined(__APPLE__)
        const std::string home = macHomeFolder();
        dir = home + "/Library/Application Support/" + kAppFolder;
        if (makeDir) mkdir(dir.c_str(), 0755);
        return dir + "/user_presets.txt";
#else
        const char* home = std::getenv("HOME");
        if (home == nullptr) return "";
        dir = std::string(home) + "/.config/mercy";
        if (makeDir) { mkdir((std::string(home) + "/.config").c_str(), 0755); mkdir(dir.c_str(), 0755); }
        return dir + "/user_presets.txt";
#endif
    }
    void loadUserPresets() {
        userPresets.clear();
        const std::string path = presetPath(false);
        if (path.empty()) return;
        FILE* f = std::fopen(path.c_str(), "r");
        if (f == nullptr) return;
        char line[4096];
        while (std::fgets(line, sizeof line, f)) {
            char* tab1 = std::strchr(line, '\t'); if (!tab1) continue; *tab1 = 0;
            char* vals = tab1 + 1;
            char* tab2 = std::strchr(vals, '\t'); if (!tab2) continue; *tab2 = 0;
            char* ord = tab2 + 1;
            UserPreset up; up.name = line; up.s.valid = true;
            for (uint32_t p = 0; p < kFirstOutputParam; ++p) up.s.v[p] = kParamInfo[p].def;
            char* tok = vals; uint32_t p = 0;
            while (tok && *tok && p < kFirstOutputParam) {
                up.s.v[p] = clampf((float)std::strtod(tok, nullptr), kParamInfo[p].min, kParamInfo[p].max);
                ++p; tok = std::strchr(tok, ','); if (tok) ++tok;
            }
            if (!normalizeOrder(ord, up.s.order))
                for (int k = 0; k < kNumStages; ++k) up.s.order[k] = kDefaultOrder[k];
            userPresets.push_back(up);
        }
        std::fclose(f);
    }
    static void writePresetLine(FILE* f, const UserPreset& up) {
        std::fprintf(f, "%s\t", up.name.c_str());
        for (uint32_t p = 0; p < kFirstOutputParam; ++p) std::fprintf(f, "%s%.6g", p ? "," : "", up.s.v[p]);
        std::fprintf(f, "\t");
        for (int k = 0; k < kNumStages; ++k) std::fprintf(f, "%d", up.s.order[k]);
        std::fprintf(f, "\n");
    }
    // rewrite the whole file (rename/delete): write a temp file first so a failure never loses presets
    bool writeUserPresets() {
        const std::string path = presetPath(true);
        if (path.empty()) return false;
        const std::string tmp = path + ".tmp";
        FILE* f = std::fopen(tmp.c_str(), "w");
        if (f == nullptr) return false;
        for (const auto& up : userPresets) writePresetLine(f, up);
        if (std::fclose(f) != 0) { std::remove(tmp.c_str()); return false; }
        std::remove(path.c_str());
        return std::rename(tmp.c_str(), path.c_str()) == 0;
    }
    static bool sameSnapshot(const Snapshot& a, const Snapshot& b) {
        for (uint32_t p = 0; p < kFirstOutputParam; ++p)
            if (std::fabs(a.v[p] - b.v[p]) > 1e-4f * std::max(1.f, std::fabs(a.v[p]))) return false;
        for (int k = 0; k < kNumStages; ++k) if (a.order[k] != b.order[k]) return false;
        return true;
    }
    // find a preset again after re-reading the file (another MERCY window may have changed it)
    int findUserPreset(const UserPreset& want) const {
        for (int k = 0; k < (int)userPresets.size(); ++k) if (userPresets[k].name == want.name && sameSnapshot(userPresets[k].s, want.s)) return k;
        for (int k = 0; k < (int)userPresets.size(); ++k) if (userPresets[k].name == want.name) return k;
        return -1;
    }
    void saveUserPreset(const std::string& name) {
        const std::string path = presetPath(true);
        if (path.empty()) return;
        loadUserPresets();
        UserPreset up; up.name = name; up.s = current();
        FILE* f = std::fopen(path.c_str(), "a");
        if (f == nullptr) return;
        writePresetLine(f, up);
        std::fclose(f);
        userPresets.push_back(up);
        S.presetIdx = numPresets() - 1;
        S.presetModified = false;
    }
    void renameUserPreset(int idx, const std::string& name) {
        if (idx < kNumFactory || idx >= numPresets()) return;
        const UserPreset want = userPresets[idx - kNumFactory];
        const bool wasCurrent = S.presetIdx == idx;
        loadUserPresets();
        const int k = findUserPreset(want);
        if (k < 0) return;
        userPresets[k].name = name;
        writeUserPresets();
        if (wasCurrent) S.presetIdx = kNumFactory + k;
    }
    void deleteUserPreset(int idx) {
        if (idx < kNumFactory || idx >= numPresets()) return;
        const UserPreset want = userPresets[idx - kNumFactory];
        const int cur = S.presetIdx;
        const UserPreset* curUp = cur >= kNumFactory && cur < numPresets() ? &userPresets[cur - kNumFactory] : nullptr;
        const UserPreset curCopy = curUp ? *curUp : UserPreset();
        loadUserPresets();
        const int k = findUserPreset(want);
        if (k < 0) return;
        userPresets.erase(userPresets.begin() + k);
        writeUserPresets();
        if (cur == idx) { S.presetIdx = 0; S.presetModified = true; }   // the sound stays, it just isn't a saved preset any more
        else if (curUp) { const int nk = findUserPreset(curCopy); S.presetIdx = nk >= 0 ? kNumFactory + nk : 0; if (nk < 0) S.presetModified = true; }
        popScroll = std::min(popScroll, popMaxScroll());
    }
    void toggleAB() {
        S.ab[S.abCur] = current();
        const int other = 1 - S.abCur;
        if (!S.ab[other].valid) S.ab[other] = S.ab[S.abCur];
        S.abCur = other;
        applySnapshot(S.ab[S.abCur]);
    }

    bool leftDown(float x, float y, unsigned mod, unsigned t) {
        const int i = hit(x, y);
        if (i < 0) return false;
        Ctl& c = ctls[i];
        if (pressable(c.type)) pressIdx = i;
        const bool dbl = (i == lastClickIdx) && (t - lastClickTime < 350);
        lastClickIdx = i; lastClickTime = t;
        switch (c.type) {
        case tVSlider: case tDotKnob: case tHSlider: case tMini: case tNumBox: case tFilter:
            if (dbl) { setValueGesture(c.param, kParamInfo[c.param].def); lastClickIdx = -1; host->hostRepaint(); return true; }
            dragIdx = i; dragNorm = toNorm(c.param, S.values[c.param]); lastX = x; lastY = y;
            host->hostBeginEdit(c.param);
            (void)mod;
            host->hostRepaint();
            return true;
        case tToggle: case tSectionPower:
            setValueGesture(c.param, S.values[c.param] > 0.5f ? 0.f : 1.f); host->hostRepaint(); return true;
        case tOption: case tOnOff:
            setValueGesture(c.param, (float)c.id); host->hostRepaint(); return true;
        case tTab:
            if (c.id < 3) selectEra(c.id); else { S.page = (S.page == 1) ? 0 : 1; buildLayout(); }
            host->hostRepaint(); return true;
        case tEqPos: {
            int pt = -1, pc = -1;
            for (int k = 0; k < kNumStages; ++k) { if (S.order[k] == kStTone) pt = k; if (S.order[k] == kStClip) pc = k; }
            const bool eqPre = pt < pc;
            if ((c.id == 0) != eqPre) { int o[kNumStages]; std::memcpy(o, S.order, sizeof o); std::swap(o[pt], o[pc]); setOrder(o); }
            host->hostRepaint(); return true;
        }
        case tSectionHead:
            reorderFrom = (int)c.a; dragX = pressX = x; dragY = pressY = y; reorderMoved = false; host->hostRepaint(); return true;
        case tResize:
            resizing = true; resizeStartScale = scale; resizeAnchorX = offX; resizeAnchorY = offY; return true;
        case tButton:
            switch (c.id) {
            case bPrev: loadPreset(fromDisplay(displayPos(S.presetIdx) - 1)); break;
            case bNext: loadPreset(fromDisplay(displayPos(S.presetIdx) + 1)); break;
            case bSave: openNamePopup(0, -1); break;
            case bAB: toggleAB(); break;
            case bPresetName: openPresets(); break;
            case bBake: toggleBake(); break;
            case bTheme: S.dark = !S.dark; break;
            case bResetOrder: { int o[kNumStages]; for (int k = 0; k < kNumStages; ++k) o[k] = kDefaultOrder[k]; setOrder(o); } break;
            default: return false;
            }
            host->hostRepaint(); return true;
        }
        return false;
    }

    bool leftUp(float x, float y) {
        if (dragIdx >= 0) { host->hostEndEdit(ctls[dragIdx].param); dragIdx = -1; host->hostRepaint(); return true; }
        if (resizing) { resizing = false; return true; }
        if (reorderFrom >= 0 && !reorderMoved) { // a click on the name: open that module's presets
            const int pos = reorderFrom; reorderFrom = -1;
            openModule(S.order[pos], pos);
            host->hostRepaint(); return true;
        }
        if (reorderFrom >= 0) {
            const int target = sectionAt(x, y);
            if (target >= 0 && target != reorderFrom) {
                int o[kNumStages], k = 0;
                const int moving = S.order[reorderFrom];
                for (int j = 0; j < kNumStages; ++j) if (j != reorderFrom) o[k++] = S.order[j];
                // insert at target position
                for (int j = kNumStages - 1; j > target; --j) o[j] = o[j - 1];
                o[target] = moving;
                setOrder(o);
            }
            reorderFrom = -1; host->hostRepaint(); return true;
        }
        return false;
    }

    int sectionAt(float x, float y) const {
        for (int pos = 0; pos < kNumStages; ++pos)
            if (x >= secX(pos) && x < secX(pos) + kSecW && y >= secY(pos) && y < secY(pos) + 214) return pos;
        return -1;
    }

    // ---------------------------------------------------------------- drawing toolkit
    Theme T = makeTheme(true);
    int fontIds[kNumFonts] = {-1, -1, -1, -1, -1, -1, -1};
    NanoImage grainLight, grainDark, logoImg;
    uint32_t lastScopePos = 0;
    float figY[kNumStages] = {};

    static float secXs(int pos) { return 104.f + (pos % 5) * 168.f; }
    static float secYs(int pos) { return 100.f + (pos / 5) * 214.f; }

    // era colours (they cross-fade when the era changes)
    Color eraMix(const Color& c0, const Color& c1, const Color& c2) const {
        Color c = c0;
        c.red = c0.red * eraW[0] + c1.red * eraW[1] + c2.red * eraW[2];
        c.green = c0.green * eraW[0] + c1.green * eraW[1] + c2.green * eraW[2];
        c.blue = c0.blue * eraW[0] + c1.blue * eraW[1] + c2.blue * eraW[2];
        c.alpha = c0.alpha * eraW[0] + c1.alpha * eraW[1] + c2.alpha * eraW[2];
        return c;
    }
    Color acc1() const { return eraMix(T.blue, T.mint, T.red); }
    Color acc2() const { return eraMix(T.violet, T.pink, T.orange); }
    static Color alpha(Color c, float a) { c.alpha *= a; return c; }

    void txt(NanoVG& g, int f, float size, float x, float top, const char* s, Color c, int anchor = 0, float spacing = 99.f, float blur = 0.f) {
        if (spacing > 50.f) spacing = defaultSpacing(f);
        const mercyfonts::Metrics& m = fontMetrics(f);
        float x0 = x;
        if (anchor) { const float w = measureText(f, size, s, spacing); x0 = anchor == 1 ? x - w * 0.5f : x - w; }
        g.fontFaceId(fontIds[f]);
        g.fontSize(size * (m.asc + m.desc));
        g.textLetterSpacing(f >= fMono ? std::max(0.f, spacing * size) : 0.f); // DPF refuses negative spacing
        g.fontBlur(blur);
        g.textAlign(NanoVG::ALIGN_LEFT | NanoVG::ALIGN_BASELINE);
        g.fillColor(c);
        const float base = top + size * (1.f + m.asc - m.desc) * 0.5f;
        if (f >= fMono) g.text(x0, base, s, nullptr);
        else { // proportional: place every glyph at its browser position
            const char* p = s;
            uint32_t prev = 0;
            float x = x0;
            while (*p) {
                const char* q = p;
                const uint32_t cp = nextCodepoint(q);
                x += kernOf(f, prev, cp) * size;
                if (cp != ' ') g.text(x, base, p, q);
                x += advanceOf(m, cp) * size + spacing * size;
                prev = cp; p = q;
            }
        }
        if (blur > 0.f) g.fontBlur(0.f);
    }
    float tw(int f, float size, const char* s) const { const float sp = defaultSpacing(f); return measureText(f, size, s, sp) - sp * size; }
    // slapworx sticker wordmark, small, bottom right (replaces the old "by jehf slaps" credit)
    void drawBrandMark(NanoVG& g) {
        if (!logoImg.isValid()) return;
        const float h = 16.5f, w = h * (float)mercylogo::slapworx_w / (float)mercylogo::slapworx_h;
        const float x = 918.f - w, y = 587.f - h * .5f;
        g.beginPath(); g.rect(x, y, w, h);
        g.fillPaint(g.imagePattern(x, y, w, h, 0, logoImg, 1.f));
        g.fill();
    }
#ifdef MERCY_TEASER
    // "000" teaser build: names scribbled out with a black marker (a hand-drawn zigzag, the same
    // every time for a given spot). Text is drawn first, then this goes on top.
    void scribble(NanoVG& g, float x0, float y0, float x1, float y1, float width, uint32_t seed) {
        Rng r(seed);
        const Color ink = dk ? Color(6, 5, 7, 1.f) : Color(12, 11, 13, 1.f);
        const Color rim = Color(64, 62, 68, .85f);
        const Color sheen = dk ? Color(255, 255, 255, .13f) : Color(255, 255, 255, .09f);
        const float h = y1 - y0;
        std::vector<float> px, py;
        for (int pass = 0; pass < 2; ++pass) {
            px.clear(); py.clear();
            const float step = width * (pass == 0 ? .95f : 1.3f);
            bool up = pass == 0;
            for (float x = x0 - 4 - r.range(0, 3); x < x1 + 4; x += step + r.range(-step * .25f, step * .25f)) {
                px.push_back(x + r.range(-1.5f, 1.5f));
                py.push_back((up ? y0 - h * .06f : y1 + h * .06f) + r.range(-h * .1f, h * .1f));
                up = !up;
            }
            if (pass == 1) { // a loose final cross-out
                const float mid = (y0 + y1) * .5f;
                px.push_back(x1 + 5); py.push_back(mid + r.range(-2, 2));
                px.push_back(x0 - 6); py.push_back(mid + h * .15f);
                px.push_back(x1 + 2); py.push_back(mid - h * .2f);
            }
            // layers: 0 = rim (where the ink pools at the edge; shows the stroke on the dark theme),
            // 1 = the ink, 2 = a soft glossy sheen along the stroke
            for (int layer = 0; layer < 3; ++layer) {
                if (layer == 0 && !dk) continue;
                const float off = layer == 2 ? 1.f : 0.f;
                g.beginPath();
                for (size_t i = 0; i < px.size(); ++i) {
                    const float xx = px[i] - off * width * .12f, yy = py[i] - off * width * .2f;
                    if (i == 0) g.moveTo(xx, yy); else g.lineTo(xx, yy);
                }
                g.lineJoin(NanoVG::ROUND); g.lineCap(NanoVG::ROUND);
                g.strokeColor(layer == 0 ? rim : (layer == 1 ? ink : sheen));
                g.strokeWidth(layer == 0 ? width + 1.6f : (layer == 1 ? width : std::max(.8f, width * .38f)));
                g.stroke();
            }
        }
        g.lineJoin(NanoVG::MITER); g.lineCap(NanoVG::BUTT);
    }
    static std::string teaserName(int i) { char b[24]; std::snprintf(b, sizeof b, "preset %02d", displayPos(i) + 1); return b; }
#endif
    // the hero type: blurred, RGB-split
    void splitText(NanoVG& g, int f, float size, float x, float top, const char* s, Color main, float spacing = 99.f) {
        txt(g, f, size, x - 2.5f, top, s, alpha(T.red, .78f), 0, spacing, .55f);
        txt(g, f, size, x + 2.5f, top, s, alpha(T.mint, .72f), 0, spacing, .55f);
        txt(g, f, size, x, top, s, main, 0, spacing, .45f);
    }

    void ln(NanoVG& g, float x0, float y0, float x1, float y1, Color c, float w = 1.f) {
        g.beginPath(); g.moveTo(x0, y0); g.lineTo(x1, y1); g.strokeColor(c); g.strokeWidth(w); g.stroke();
    }
    void dashLn(NanoVG& g, float x0, float y0, float x1, float y1, Color c, float w, float on, float off) {
        const float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f) return;
        const float ux = dx / len, uy = dy / len;
        g.beginPath();
        for (float t = 0; t < len; t += on + off) {
            const float e = std::min(len, t + on);
            g.moveTo(x0 + ux * t, y0 + uy * t); g.lineTo(x0 + ux * e, y0 + uy * e);
        }
        g.strokeColor(c); g.strokeWidth(w); g.stroke();
    }
    void dashPoly(NanoVG& g, const float* xs, const float* ys, int n, Color c, float w, float on, float off) {
        float carry = 0; bool drawing = true;
        g.beginPath();
        for (int i = 0; i + 1 < n; ++i) {
            const float dx = xs[i + 1] - xs[i], dy = ys[i + 1] - ys[i], len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.01f) continue;
            float t = 0;
            while (t < len) {
                const float seg = (drawing ? on : off) - carry;
                const float e = std::min(len, t + seg);
                if (drawing) { g.moveTo(xs[i] + dx * t / len, ys[i] + dy * t / len); g.lineTo(xs[i] + dx * e / len, ys[i] + dy * e / len); }
                if (t + seg <= len) { drawing = !drawing; carry = 0; } else carry += e - t;
                t = e;
            }
        }
        g.strokeColor(c); g.strokeWidth(w); g.stroke();
    }
    void rc(NanoVG& g, float x, float y, float w, float h, Color c) { g.beginPath(); g.rect(x, y, w, h); g.fillColor(c); g.fill(); }
    void ro(NanoVG& g, float x, float y, float w, float h, Color c, float sw = 1.5f) {
        g.beginPath(); g.rect(x + sw * .5f, y + sw * .5f, w - sw, h - sw); g.strokeColor(c); g.strokeWidth(sw); g.stroke();
    }
    void dashRect(NanoVG& g, float x, float y, float w, float h, Color c, float sw, float on, float off) {
        const float i = sw * .5f;
        dashLn(g, x + i, y + i, x + w - i, y + i, c, sw, on, off);
        dashLn(g, x + w - i, y + i, x + w - i, y + h - i, c, sw, on, off);
        dashLn(g, x + w - i, y + h - i, x + i, y + h - i, c, sw, on, off);
        dashLn(g, x + i, y + h - i, x + i, y + i, c, sw, on, off);
    }
    void cross(NanoVG& g, float x, float y, float s, Color c, float w = 1.f) { ln(g, x - s, y, x + s, y, c, w); ln(g, x, y - s, x, y + s, c, w); }
    void cells(NanoVG& g, float x, float y, const int (*cl)[2], int n, float s, Color c) {
        g.beginPath();
        for (int i = 0; i < n; ++i) g.rect(x + cl[i][0] * s, y + cl[i][1] * s, s, s);
        g.fillColor(c); g.fill();
    }
    void dot(NanoVG& g, float x, float y, float r, Color c) { g.beginPath(); g.circle(x, y, r); g.fillColor(c); g.fill(); }
    void ring(NanoVG& g, float x, float y, float r, Color c, float w) { g.beginPath(); g.circle(x, y, r); g.strokeColor(c); g.strokeWidth(w); g.stroke(); }
    void poly(NanoVG& g, const float* xs, const float* ys, int n, Color c, float w) {
        if (n < 2) return;
        g.beginPath(); g.moveTo(xs[0], ys[0]);
        for (int i = 1; i < n; ++i) g.lineTo(xs[i], ys[i]);
        g.lineJoin(NanoVG::ROUND); g.strokeColor(c); g.strokeWidth(w); g.stroke();
    }
    // photocopied square: edges slightly ragged, like toner
    void tonerRect(NanoVG& g, float x, float y, float w, float h, Color c) {
        Rng r((uint32_t)(x * 7.f + y * 13.f + w));
        const int steps = 8;
        g.beginPath();
        for (int s = 0; s < 4; ++s)
            for (int k = 0; k < steps; ++k) {
                const float t = (float)k / steps;
                float px, py;
                switch (s) {
                case 0: px = x + w * t; py = y; break;
                case 1: px = x + w; py = y + h * t; break;
                case 2: px = x + w - w * t; py = y + h; break;
                default: px = x; py = y + h - h * t; break;
                }
                px += r.range(-.28f, .28f); py += r.range(-.28f, .28f);
                if (s == 0 && k == 0) g.moveTo(px, py); else g.lineTo(px, py);
            }
        g.closePath(); g.fillColor(c); g.fill();
    }
    // a waveform blob: symmetric envelope, toner-ragged edges
    void blob(NanoVG& g, float x0, float ymid, float w, float amp, const float* v, int n, Color c, uint32_t seed, float asym = .92f, float rough = .45f) {
        if (n < 2) return;
        Rng r(seed);
        g.beginPath();
        g.moveTo(x0, ymid);
        for (int i = 0; i < n; ++i) g.lineTo(x0 + w * i / (n - 1), ymid - amp * v[i] - .5f + r.range(-rough, rough));
        for (int i = n - 1; i >= 0; --i) g.lineTo(x0 + w * i / (n - 1), ymid + amp * v[i] * asym + .5f + r.range(-rough, rough));
        g.closePath(); g.fillColor(c); g.fill();
    }
    void staticBlob(NanoVG& g, float x0, float ymid, float w, float amp, const Hit* hits, int nh, uint32_t seed, Color c, int n = 150, float jit = .09f, float asym = .92f) {
        std::vector<float> v;
        hitEnvelope(hits, nh, n, seed, jit, .02f, v);
        blob(g, x0, ymid, w, amp, v.data(), n + 1, c, seed + 7, asym);
    }
    void editBox(NanoVG& g, float x, float y, int anchor) {
        const std::string s = editText + "_";
        const float w = std::max(56.f, tw(fMono, 12, s.c_str()) + 14.f);
        const float bx = anchor == 2 ? x - w + 4 : (anchor == 1 ? x - w / 2 : x - 4);
        rc(g, bx, y - 11, w, 22, T.paper);
        ro(g, bx, y - 11, w, 22, T.red, 1.2f);
        txt(g, fMono, 12, bx + 7, y - 6, s.c_str(), T.ink);
    }
    static const char* coordStr(char* b, size_t n, float x, float y) { std::snprintf(b, n, "x: %03d y: %03d", (int)x, (int)y); return b; }

    // ---------------------------------------------------------------- resources
    void ensureResources(NanoVG& g) {
        if (resCtx == g.getContext()) return;
        resCtx = g.getContext();
        using namespace mercyfonts;
        const unsigned char* data[kNumFonts] = {archivo_medium_data, archivo_550_data, archivo_semibold_data, archivo_bold_data,
                                                unbounded_extrabold_data, jbmono_regular_data, jbmono_bold_data};
        const unsigned int sizes[kNumFonts] = {archivo_medium_size, archivo_550_size, archivo_semibold_size, archivo_bold_size,
                                               unbounded_extrabold_size, jbmono_regular_size, jbmono_bold_size};
        const char* names[kNumFonts] = {"mg-500", "mg-550", "mg-600", "mg-700", "mw-800", "mm-400", "mm-700"};
        for (int i = 0; i < kNumFonts; ++i) fontIds[i] = g.createFontFromMemory(names[i], data[i], sizes[i], false);
        logoImg = g.createImageFromMemory(mercylogo::slapworx_png, mercylogo::slapworx_png_size, NanoVG::IMAGE_GENERATE_MIPMAPS);
        // print grain: fine fractal-ish noise turned into sparse specks (dark specks on paper, light on ink)
        const int N = 256;
        std::vector<float> v((size_t)N * N), w((size_t)N * N);
        Rng r(77);
        for (auto& x : v) x = (r.uni() + r.uni() + r.uni()) / 3.f;
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                auto at = [&](int xx, int yy) { return v[(size_t)((yy + N) % N) * N + (size_t)((xx + N) % N)]; };
                w[(size_t)y * N + x] = .5f * at(x, y) + .125f * (at(x - 1, y) + at(x + 1, y) + at(x, y - 1) + at(x, y + 1));
            }
        for (int mode = 0; mode < 2; ++mode) {
            std::vector<unsigned char> px((size_t)N * N * 4);
            const float op = mode ? .26f : .30f;
            const unsigned char c = mode ? 255 : 0;
            for (int i = 0; i < N * N; ++i) {
                const float a = clampf(1.6f * w[(size_t)i] - .62f, 0, 1) * op;
                px[(size_t)i * 4] = c; px[(size_t)i * 4 + 1] = c; px[(size_t)i * 4 + 2] = c;
                px[(size_t)i * 4 + 3] = (unsigned char)std::lround(a * 255.f);
            }
            (mode ? grainDark : grainLight) = g.createImageFromRGBA(N, N, px.data(), NanoVG::IMAGE_REPEAT_X | NanoVG::IMAGE_REPEAT_Y);
        }
    }

    // ---------------------------------------------------------------- page art
    void drawChrome(NanoVG& g) {
        rc(g, 0, 0, 84, kH, T.side);
        ln(g, 84, 0, 84, kH, T.hair);
        cells(g, 22, 30, kStairA, 6, 6, T.red);
        txt(g, fMono, 7.5f, 22, 62, "x: 041", T.grey);
        txt(g, fMono, 7.5f, 22, 72.1f, "y: 366", T.grey);
        ln(g, 42, 366, 42, 440, T.hair);
        cross(g, 42, 366, 4, T.ink);
        g.save(); g.translate(14, 350); g.rotate(-1.5707963f);
        splitText(g, fWide, 47, 0, 0, "mercy.", T.blue);
#ifdef MERCY_TEASER
        scribble(g, 2, 6, tw(fWide, 47, "mercy."), 44, 7.5f, 11);
#endif
        g.restore();
        g.save(); g.translate(34, 590); g.rotate(-1.5707963f);
        txt(g, fMono, 10.5f, 0, 0, "drum processor", T.grey, 0, .14f);
        g.restore();
        txt(g, fMono, 8.5f, 104, 14, "mercy / drum processor / v1.0", T.ink);
#ifdef MERCY_TEASER
        scribble(g, 105, 15, 103 + measureText(fMono, 8.5f, "mercy", defaultSpacing(fMono)), 26, 3.2f, 12);
#endif
        const char* co = S.page == 0 ? "x: 001 y: 006" : (S.bakeOpen ? "x: 003 y: 011" : "x: 002 y: 009");
        txt(g, fMono, 8.5f, 858, 14, co, T.ink, 2);
        ln(g, 104, 30, 940, 30, T.hair2);
    }

    void drawMainArt(NanoVG& g) {
        char b[64];
        rc(g, 640, 31, 320, 569, T.paper2);
        ln(g, 640, 31, 640, kH, T.hair);
        // fader scale
        txt(g, fMono, 7, 154, 160, "100", T.ink, 2);
        txt(g, fMono, 7, 154, 236, "50", T.ink, 2);
        txt(g, fMono, 7, 154, 320, "0", T.ink, 2);
        // spec list
        txt(g, fMono, 8.5f, 500, 114, "spec.", T.ink2);
        static const char* eras[3] = {"fantasy", "throne", "yeezus"};
        static const uint32_t powers[kNumStages] = {pSmpOn, pPreOn, pSqOn, pToneOn, pClipOn, pGwOn, pTrOn, pShOn, pRvOn, pWtOn};
        int on = 0; for (uint32_t p : powers) on += S.values[p] > 0.5f;
        char chain[24]; std::snprintf(chain, sizeof chain, "%d / %d on", on, (int)kNumStages);
        const bool busy = S.baker && S.baker->busy();
        int baked = 0; for (const auto& it : bakeItems) baked += it.state == Baker::kDone;
        char bk[24]; if (busy) std::snprintf(bk, sizeof bk, "baking"); else if (baked) std::snprintf(bk, sizeof bk, "%d baked", baked); else std::snprintf(bk, sizeof bk, "ready");
        const char* specK[5] = {"era", "chain", "look-ahead", "quality", "bake"};
        const char* specV[5] = {eras[(int)clampf(std::round(S.values[pEra]), 0, 2)], chain, "5.0 ms", S.values[pQuality] > .5f ? "2x" : "1x", bk};
        for (int i = 0; i < 5; ++i) {
            const float yy = 140 + i * 22;
            txt(g, fMono, 8.5f, 500, yy, specK[i], T.ink);
            txt(g, fMono, 8.5f, 620, yy, specV[i], T.ink2, 2);
#ifdef MERCY_TEASER
            if (i == 0) scribble(g, 620 - measureText(fMono, 8.5f, specV[0], defaultSpacing(fMono)), yy + 1, 620, yy + 12, 3.2f, 13);
#endif
            ln(g, 500, yy + 15, 620, yy + 15, T.hair2);
        }
        cells(g, 500, 260, kCorner3, 3, 6, acc1());
        // bottom bar
        ln(g, 104, 530, 620, 530, T.hair);
        std::snprintf(b, sizeof b, "preset %02d / %02d", displayPos(S.presetIdx) + 1, numPresets());
        txt(g, fMono, 7.5f, 104, 518, b, T.ink);
        txt(g, fMono, 7.5f, 620, 518, "x: 620 y: 530", T.ink, 2);
        ln(g, 284, 549, 284, 565, T.hair); ln(g, 340, 549, 340, 565, T.hair); ln(g, 428, 549, 428, 565, T.hair);

        // ---- hero: output specimen (live)
        txt(g, fMono, 8.5f, 664, 56, "fig. 01", T.ink2);
        txt(g, fMono, 8.5f, 664, 68, "output specimen / last 1.0 s", T.ink);
        rc(g, 688, 104, 204, 204, T.mint);
        rc(g, 866, 78, 64, 64, alpha(T.mint, .92f));
        const int N = Scope::N;
        float vo[N], vi[N];
        const uint32_t sp = S.scope.pos.load(std::memory_order_acquire);
        float pk = 0, pkIn = 0; int pki = N - 1;
        for (int i = 0; i < N; ++i) {
            const uint32_t idx = (sp + (uint32_t)i) % (uint32_t)N;
            const float o = S.scope.out[idx].load(std::memory_order_relaxed), in = S.scope.in[idx].load(std::memory_order_relaxed);
            vo[i] = std::isfinite(o) ? std::max(o, 0.f) : 0.f;
            vi[i] = std::isfinite(in) ? std::max(in, 0.f) : 0.f;
            if (vo[i] > pk) { pk = vo[i]; pki = i; }
            pkIn = std::max(pkIn, vi[i]);
        }
        // the specimen is drawn like a print: scaled to its own loudest moment (quiet stays quiet below -30 dB)
        const float no = 1.f / std::max(pk, .0316f), ni = 1.f / std::max(pkIn, .0316f);
        for (int i = 0; i < N; ++i) {
            vo[i] = .015f + .965f * clampf(std::pow(vo[i] * no, .8f), 0, 1);
            vi[i] = .015f + .965f * clampf(std::pow(vi[i] * ni, .8f), 0, 1);
        }
        float vi2[52];
        for (int i = 0; i < 52; ++i) { const int a = i * N / 52, e = (i + 1) * N / 52; float m = 0; for (int k = a; k < e; ++k) m = std::max(m, vi[k]); vi2[i] = m; }
        blob(g, 872, 110, 52, 20, vi2, 52, T.onMint, 4);
        txt(g, fMono, 7.5f, 930, 148, "in", T.ink, 2);
        blob(g, 700, 206, 180, 74, vo, N, T.onMint, 11);
        const bool throne = (int)std::lround(S.values[pEra]) == 1; // mint on mint would vanish: swap
        rc(g, 672, 286, 32, 32, throne ? acc2() : acc1());
        cells(g, 704, 318, kDiag2, 2, 8, throne ? acc1() : acc2());
        cells(g, 870, 292, kCluster, 9, 5, T.red);
        dashLn(g, 652, 206, 950, 206, T.ink, .7f, 1.5f, 3);
        dashLn(g, 790, 84, 790, 330, T.ink, .7f, 1.5f, 3);
        const float px = 700 + 180.f * pki / (N - 1), py = 206 - 74 * vo[pki];
        ring(g, px, py, 8, T.onMint, 1.3f);
        cross(g, px, py, 13, T.onMint, 1.1f);
        ln(g, px + 9, py - 9, 744, 92, T.ink, .8f);
        if (pk > 1e-4f) std::snprintf(b, sizeof b, "peak %.1f dB", linToDb(pk)); else std::snprintf(b, sizeof b, "peak -inf");
        txt(g, fMono, 7.5f, 746, 86, b, T.ink2);
        txt(g, fMono, 7, 796, 94, "x: 790 y: 206", T.ink);
        txt(g, fMono, 7.5f, 728, 316, "fig. 01a \xE2\x80\x94 out", T.ink2);
        // readouts
        auto meter = [&](float y, const char* name, float frac, const char* val, bool gr) {
            txt(g, fMed, 13.5f, 664, y - 6, name, T.ink);
            frac = clampf(frac, 0, 1);
            if (gr) { rc(g, 704, y - 3, 196, 7, T.pink); rc(g, 704, y - 3, 196 * frac, 7, T.red); }
            else { ln(g, 704, y, 900, y, T.hair); rc(g, 704, y - 1, 196 * frac, 3, T.ink); rc(g, 704 + 196 * frac - 3.5f, y - 3.5f, 7, 7, T.ink); }
            txt(g, fMono, 10.5f, 940, y - 5, val, T.ink2, 2);
        };
        auto dbs = [](char* o, size_t n, float v) { if (v <= -59.f) std::snprintf(o, n, "-inf"); else std::snprintf(o, n, "%.1f", v); return o; };
        char vb[16];
        meter(366, "in", (S.values[pMeterIn] + 48.f) / 54.f, dbs(vb, sizeof vb, S.values[pMeterIn]), false);
        std::snprintf(vb, sizeof vb, "%.1f", S.values[pMeterGR]);
        meter(394, "gr", S.values[pMeterGR] / 24.f, vb, true);
        meter(422, "out", (S.values[pMeterOut] + 48.f) / 54.f, dbs(vb, sizeof vb, S.values[pMeterOut]), false);
        txt(g, fMono, 7, 704, 436, "-48", T.ink);
        txt(g, fMono, 7, 900, 436, "+6", T.ink, 2);
    }

    // dotted red line joining the three fader handles
    void drawFaderGuide(NanoVG& g) {
        const uint32_t ps[3] = {pSquash, pDirt, pAge};
        float xs[3], ys[3];
        for (int i = 0; i < 3; ++i) { xs[i] = 182.f + 120.f * i; ys[i] = 324.f - mo.value(mkey(1, ps[i]), toNorm(ps[i], S.values[ps[i]])) * 164.f; }
        dashPoly(g, xs, ys, 3, T.red, 1.f, 2, 3);
    }

    void drawAdvancedArt(NanoVG& g) {
        const float X0 = 104, Y0 = 100, CW = kSecW, RH = 214, FW = kSecW - 28; // FW: figure width
        for (int k = 1; k < 5; ++k) ln(g, X0 + CW * k, Y0 + 6, X0 + CW * k, Y0 + RH * 2 - 6, T.hair);
        ln(g, X0 + 6, Y0 + RH, X0 + CW * 5 - 6, Y0 + RH, T.hair);
        for (int k = 1; k < 5; ++k) cross(g, X0 + CW * k, Y0 + RH, 5, T.ink);
        char b[64];
        for (int pos = 0; pos < kNumStages; ++pos) {
            const int st = S.order[pos];
            const float x = secXs(pos), y = secYs(pos), fx = x + 14, fy = figY[pos];
            g.save();   // the module's art moves with it (reordering)
            g.translate(stOffX[st], stOffY[st]);
            if (reorderFrom >= 0 && reorderMoved && reorderFrom == pos) g.globalAlpha(alphaBase * pageAlpha * .4f);
            const float hx = x + kSecW - 70, hy = y + 20, HW = 40; // small figure beside the name
            switch (st) {
            case kStTransient: {
                txt(g, fMono, 7.5f, fx, fy - 6, "fig. markers / gain", T.ink);
                const Hit h[4] = {{.04f, .95f, .1f}, {.3f, .6f, .07f}, {.52f, .95f, .12f}, {.8f, .55f, .07f}};
                staticBlob(g, fx, fy + 44, FW, 26, h, 4, 21, T.scan);
                const float mk[4] = {.04f, .3f, .52f, .8f};
                for (float m : mk) { const float xx = fx + m * FW; ln(g, xx, fy + 8, xx, fy + 80, T.grey); rc(g, xx - 3, fy + 8, 6, 6, T.grey); }
                const float env = S.values[pTrEnv];
                if (env >= 99.95f) {
                    ln(g, fx, fy + 16, fx + FW, fy + 16, T.red, 1.2f);
                    txt(g, fMono, 7.5f, fx + FW, fy + 84, "100 = no fade", T.ink, 2);
                } else {
                    const float rate = 0.56f * std::exp(-0.03736f * env);
                    float xs[141], ys[141];
                    for (int i = 0; i <= 140; ++i) {
                        const float t = i / 140.f;
                        float last = mk[3] - 1.f;
                        for (float m : mk) if (m <= t) last = m;
                        const float db = rate * (t - last) * 700.f;
                        xs[i] = fx + i; ys[i] = fy + 16 + std::min(62.f, db / 48.f * 62.f);
                    }
                    poly(g, xs, ys, 141, T.red, 1.2f);
                    std::snprintf(b, sizeof b, "%d \xC2\xB7 fade %.2f dB/ms", (int)std::lround(env), rate);
                    txt(g, fMono, 7.5f, fx + FW, fy + 84, b, T.ink, 2);
                }
            } break;
            case kStPreamp: {
                // a small curve box with its caption beside it, all inside the module (the old stacked layout ran past
                // the bottom edge); the era colour is a small swatch by the flavor instead of a big block
                const float bs = 28, bx = fx, by = fy - 4, tx = bx + bs + 10;
                ro(g, bx, by, bs, bs, T.hair, 1);
                ln(g, bx, by + bs / 2, bx + bs, by + bs / 2, T.hair2);
                ln(g, bx + bs / 2, by, bx + bs / 2, by + bs, T.hair2);
                const float k = .8f + S.values[pPreDrive] / 24.f * 3.2f;
                float xs[41], ys[41];
                for (int i = 0; i <= 40; ++i) { const float u = i / 40.f; xs[i] = bx + bs * u; ys[i] = by + bs / 2 - std::tanh((u - .5f) * 2 * k) / std::tanh(k) * bs * .42f; }
                poly(g, xs, ys, 41, T.ink, 1.4f);
                dot(g, bx + bs * .68f, by + bs / 2 - std::tanh(.36f * k) / std::tanh(k) * bs * .42f, 2.5f, T.red);
                txt(g, fMono, 7.5f, tx, by + 2, "fig. drive curve", T.ink);
                static const char* fl[4] = {"console", "tube", "tape", "xfmr"};
                rc(g, tx, by + 17, 6, 6, acc2());
                std::snprintf(b, sizeof b, "%s \xC2\xB7 %+.0f dB", fl[(int)clampf(std::round(S.values[pPreFlavor]), 0, 3)], S.values[pPreDrive]);
                txt(g, fMono, 7.5f, tx + 10, by + 16, b, T.ink2);
            } break;
            case kStClip: {
                txt(g, fMono, 7.5f, fx, fy, "fig. ceiling", T.ink);
                const float cy = fy + 34, A = 14;
                const float c = A * .8f * dbToLin(S.values[pClipCeiling]);
                const float drive = 1.2f + S.values[pClipAmount] * 1.5f;
                const bool hard = S.values[pClipMode] > .5f;
                float xs[122], ys[122];
                for (int k = 0; k < 122; ++k) {
                    const float raw = A * drive * std::sin(k / 9.f);
                    const float o = hard ? clampf(raw, -c, c) : c * std::tanh(raw / c);
                    xs[k] = fx + k * (FW / 121.f); ys[k] = cy - o;
                }
                dashLn(g, fx, cy - c, fx + FW, cy - c, T.red, 1, 2, 2);
                dashLn(g, fx, cy + c, fx + FW, cy + c, T.red, 1, 2, 2);
                poly(g, xs, ys, 122, T.ink, 1.4f);
                txt(g, fMono, 7.5f, fx + FW, cy - 26, fmtValue(pClipCeiling, S.values[pClipCeiling]).c_str(), T.ink, 2);
            } break;
            case kStGateWidth: {
                txt(g, fMono, 7.5f, fx, fy, "fig. stereo field", T.ink);
                const float gx = fx + 18, gy = fy + 34;
                ln(g, gx - 17, gy + 17, gx + 17, gy - 17, T.hair);
                ln(g, gx - 17, gy - 17, gx + 17, gy + 17, T.hair);
                Rng rr(5);
                const float wd = std::max(.04f, S.values[pWidth]);
                g.beginPath();
                for (int i = 0; i < 70; ++i) {
                    const float a = rr.gauss() * 11 * .8f, bb = rr.gauss() * 3 * wd;
                    g.circle(gx + (a + bb) / 1.41f, gy - (a - bb) / 1.41f, 1);
                }
                g.fillColor(T.ink); g.fill();
                rc(g, gx + 30, gy - 14, 28, 28, T.mint);
                const Hit h[2] = {{.08f, .9f, .2f}, {.6f, .5f, .1f}};
                staticBlob(g, gx + 33, gy, 22, 8, h, 2, 8, T.onMint);
                txt(g, fMono, 7.5f, gx + 64, gy - 4, S.values[pMonoLows] > .5f ? "mono < 120 Hz" : "full stereo", T.ink);
            } break;
            case kStSampler: {
                const float rate = clampf(S.values[pSmpRate], 4, 44.1f), bits = clampf(S.values[pSmpBits], 4, 16);
                const float stepW = clampf(4.5f * std::sqrt(32.f / rate), 2.f, 10.f);
                const float q = 2.2f * clampf(14.f / bits, .5f, 3.f);
                std::vector<float> xs, ys;
                for (float k = 0; k < HW; k += stepW * 0.7f) {
                    const float v = std::round(std::sin(k / 6.2f) * 6.6f / q) * q;
                    xs.push_back(hx + k); ys.push_back(hy - v);
                    xs.push_back(hx + std::min(HW, k + stepW * 0.7f)); ys.push_back(hy - v);
                }
                poly(g, xs.data(), ys.data(), (int)xs.size(), T.ink, 1.1f);
                cells(g, x + kSecW - 20, y + 192, kStairB, 6, 4, T.red);
            } break;
            case kStSquash: {
                const float kt = (clampf(S.values[pSqThresh], -40, 0) + 40.f) / 40.f * HW, ratio = std::max(1.f, S.values[pSqRatio]);
                float xs[21], ys[21];
                for (int i = 0; i <= 20; ++i) { const float k = i * HW / 20.f; const float o = k < kt ? k : kt + (k - kt) / ratio; xs[i] = hx + k; ys[i] = hy + 7 - o * .4f; }
                poly(g, xs, ys, 21, T.ink, 1.2f);
                dashLn(g, hx + kt, hy - 9, hx + kt, hy + 9, T.red, .8f, 1.5f, 1.5f);
            } break;
            case kStTone: {
                const float lowF = S.values[pToneLowFreq] > .5f ? 100.f : 60.f;
                const float kl = 60.f * std::log10(lowF / 20.f) / 3.f, kb = 60.f * std::log10(3000.f / 20.f) / 3.f;
                const float kd = 60.f * std::log10(clampf(S.values[pToneDepth], 2, 20) * 1000.f / 20.f) / 3.f;
                const bool b10 = S.values[pToneBiteFreq] > .5f;
                float xs[61], ys[61];
                for (int k = 0; k <= 60; ++k) {
                    const float lo = S.values[pToneLowAmt] / 12.f * 6.f * std::exp(-((k - kl) / 9.f) * ((k - kl) / 9.f));
                    const float bi = b10 ? S.values[pToneBiteAmt] / 12.f * 5.f / (1.f + std::exp(-(k - 50.f) / 2.f))
                                         : S.values[pToneBiteAmt] / 12.f * 5.f * std::exp(-((k - kb) / 8.f) * ((k - kb) / 8.f));
                    const float dr = std::min(8.f, std::max(0.f, k - kd) * .8f);
                    xs[k] = hx + k * (HW / 60.f); ys[k] = hy + 2 - lo - bi + dr;
                }
                poly(g, xs, ys, 61, T.ink, 1.2f);
            } break;
            case kStShape: {
                const float a = S.values[pShAttack] / 100.f, s = S.values[pShSustain] / 100.f;
                const float pk = hy - 8 - a * 4, t1 = hy - 1 - s * 4, t2 = hy + 4 - s * 3;
                const float xs[5] = {hx, hx + 3, hx + 7, hx + 28, hx + HW}, ys[5] = {hy + 7, pk, t1, t2, hy + 7};
                poly(g, xs, ys, 5, T.ink, 1.2f);
                cross(g, hx + 3, pk, 3, T.red);
            } break;
            case kStReverb: {
                // the tail's shape: plate = long smooth decay, room = early bounces + short tail, gated = flat then cut
                const int m = (int)clampf(std::round(S.values[pRvMode]), 0, 2);
                const float dec = clampf(S.values[pRvDecay], .1f, 4);
                float xs[41], ys[41];
                for (int i = 0; i <= 40; ++i) {
                    const float t = i / 40.f; float a;
                    if (m == 2) { const float len = clampf((.12f + .2f * dec) / 1.0f, .15f, .95f); a = t < .04f ? t / .04f : (t < len ? .62f : std::max(0.f, .62f - (t - len) * 30.f)); }
                    else { const float k = m == 0 ? 1.6f / dec : 3.5f / dec; a = t < .03f ? t / .03f : std::exp(-(t - .03f) * k * 2.2f); }
                    xs[i] = hx + t * HW; ys[i] = hy + 7 - a * 14;
                }
                poly(g, xs, ys, 41, T.ink, 1.2f);
                if (m == 1) for (int k = 0; k < 4; ++k) ln(g, hx + 3 + k * 3.5f, hy + 7, hx + 3 + k * 3.5f, hy + 1 - (3 - k) * 2.5f, T.red, 1);
                else cross(g, hx + 1.2f, hy - 7, 3, T.red);
            } break;
            case kStWeight: {
                // "ol": lights red while weight pushes the signal past 0 dB
                const bool ol = S.values[pWtOl] > 0.5f;
                txt(g, fMono, 8, hx + 12, hy - 4, "ol", T.grey);
                if (ol) dot(g, hx + 30, hy, 3.5f, T.red); else ro(g, hx + 26.5f, hy - 3.5f, 7, 7, T.grey, 1);
                txt(g, fMono, 7.5f, fx, fy, "fig. low end", T.ink);
                const bool flat = S.values[pWtFlat] > .5f;
                double fp, Q, kk; WeightModel::get().params(S.values[pWtCenter], S.values[pWtFreq], S.values[pWtAmount], fp, Q, kk);
                const float by = fy + 46, top = fy + 16;
                ln(g, fx, by, fx + FW, by, T.hair);
                float xs[71], ys[71], pkF = 20, pkDb = -99, pkX = fx, pkY = by;
                for (int i = 0; i <= 70; ++i) {                                                   // 20 Hz .. 400 Hz
                    const float f = 20.f * std::pow(20.f, i / 70.f);
                    const float db = std::max(-14.f, (float)WeightModel::magDb(f, fp, Q, kk, flat));
                    xs[i] = fx + i / 70.f * FW; ys[i] = by - 4 - std::min(db, 20.f) * 1.3f;
                }
                for (int i = 0; i <= 400; ++i) {                                                  // the peak, finer
                    const float f = 20.f * std::pow(20.f, i / 400.f), db = (float)WeightModel::magDb(f, fp, Q, kk, flat);
                    if (db > pkDb) { pkDb = db; pkF = f; pkX = fx + i / 400.f * FW; pkY = by - 4 - std::min(db, 20.f) * 1.3f; }
                }
                poly(g, xs, ys, 71, T.ink, 1.3f);
                dot(g, pkX, pkY, 3, T.red);
                std::snprintf(b, sizeof b, "%.0f Hz / %+.0f dB", pkF, pkDb);
                txt(g, fMono, 7.5f, fx + FW, top - 16, b, T.ink, 2);
            } break;
            default: break;
            }
            g.restore();
        }
        ln(g, 104, 540, 944, 540, T.hair);
        for (int k = 0; k < 3; ++k) ln(g, 104, 556 + k * 5, 118, 556 + k * 5, T.grey, 1.4f);
        txt(g, fMono, 10, 130, 557, "drag a section by its name to reorder the chain", T.ink);
    }

    void drawReorderGhost(NanoVG& g) {
        const int target = sectionAt(dragX, dragY);
        if (target >= 0) ro(g, secXs(target) + 4, secYs(target) + 4, kSecW - 8, 206, T.red, 1.5f);
        static const char* names[kNumStages] = {"sampler", "preamp", "squash", "tone", "clip", "gate/width", "transient", "shape", "verb", "weight"};
        txt(g, fSemi, 14, dragX + 10, dragY - 7, names[S.order[reorderFrom]], T.red);
    }

    // ---------------------------------------------------------------- controls
    void drawCtl(NanoVG& g, const Ctl& c, int idx) {
        const bool hov = idx == hoverIdx, drag = idx == dragIdx, edit = idx == editIdx;
        const float hv = hoverMix(idx, hov);   // hover highlights fade in and out
        char b[64];
        switch (c.type) {
        case tTab: {
            const int era = (int)std::lround(S.values[pEra]);
            const bool isEra = c.id < 3;
            const float x = c.a, selW = clampf(isEra ? eraW[c.id] : pageW, 0, 1);
            static const char* nums[4] = {"01", "02", "03", "\xE2\x86\x97"};
            txt(g, fMono, 7.5f, x, 41, nums[c.id], T.ink);
            txt(g, f550, 21, x, 54, c.label.c_str(), mixc(mixc(T.grey, T.ink2, hv), T.ink, selW));
#ifdef MERCY_TEASER
            if (isEra) scribble(g, x, 58, x + tw(f550, 21, c.label.c_str()), 78, 5.5f, 20 + (uint32_t)c.id);
#endif
            static const float uw[4] = {72, 62, 66, 92};
            const float w = uw[c.id];
            if (c.id == 0 && era >= 0 && era < 3 && ctls.size() > 3) {   // one underline for the era: it slides from tab to tab
                const float ux = mo.to(mkey(3, 0), ctls[(size_t)era].a, kSpringUI), uwd = mo.to(mkey(3, 1), uw[era], kSpringUI);
                const float uh = mo.to(mkey(3, 2), S.page == 0 ? 3.f : 1.5f, kSpringFade);
                rc(g, ux, 83, uwd, uh, acc1()); cells(g, ux + uwd + 3, 79, kStep2, 2, 4, T.red);
            }
            if (!isEra && pageW > 0.002f) rc(g, x, 83, w * std::min(pageW, 1.f), 3, T.red);
        } break;

        case tVSlider: {
            const float cx = c.c, top = c.a, bot = c.b;
            txt(g, f550, 19, cx, 114, c.label.c_str(), T.ink, 1);
            ln(g, cx - 11, top, cx + 11, top, T.ink, 1.6f);
            ln(g, cx - 11, bot, cx + 11, bot, T.ink, 1.6f);
            ln(g, cx, top + 8, cx, bot - 8, T.hair);
            for (int k = 0; k <= 10; ++k) {
                const float yy = bot - 8 - k * (bot - top - 16) / 10.f, l = k % 5 == 0 ? 7.f : 3.5f;
                ln(g, cx - 16 - l, yy, cx - 16, yy, T.hair);
            }
            const float hy = bot - 8 - dispN(c, idx) * (bot - top - 16);
            rc(g, cx - 2, hy, 4, bot - 8 - hy, acc1());
            tonerRect(g, cx - 10, hy - 10, 20, 20, (drag || edit) ? T.red : T.ink);
            txt(g, fMono, 7, cx + 16, hy - 15, coordStr(b, sizeof b, cx, hy), T.ink);
            ln(g, cx + 11, hy - 6, cx + 15, hy - 10, T.hair, .8f);
            if (edit) editBox(g, cx, 352, 1);
            else txt(g, fMono, 11, cx, 346, fmtValue(c.param, S.values[c.param]).c_str(), (drag ? T.ink : mixc(T.ink2, T.ink, hv)), 1);
        } break;

        case tDotKnob: {
            const float cx = c.c, y = 418;
            txt(g, fMed, 14, cx, 390, c.label.c_str(), T.ink, 1);
            ln(g, cx - 36, y, cx + 36, y, T.hair);
            ln(g, cx - 36, y - 6, cx - 36, y + 6, T.ink, 1.3f);
            ln(g, cx + 36, y - 6, cx + 36, y + 6, T.ink, 1.3f);
            dot(g, cx - 18, y, 1.6f, T.grey); dot(g, cx + 18, y, 1.6f, T.grey);
            const float n = dispN(c, idx);
            dot(g, cx - 36 + n * 72, y, 6.5f, (drag || edit) ? T.red : T.ink);
            const float v = S.values[c.param];
            if (std::fabs(v) < .05f) std::snprintf(b, sizeof b, "0.0 dB"); else std::snprintf(b, sizeof b, "%+.1f dB", v);
            if (edit) editBox(g, cx, 442, 1); else txt(g, fMono, 10, cx, 436, b, (drag ? T.ink : mixc(T.ink2, T.ink, hv)), 1);
        } break;

        case tHSlider: { // mix
            const float y = 418;
            txt(g, fMed, 14, 302, 390, c.label.c_str(), T.ink, 1);
            ln(g, 264, 409, 264, 427, T.ink, 1.6f);
            ln(g, 340, 409, 340, 427, T.ink, 1.6f);
            ln(g, 270, y, 334, y, T.hair);
            const float hx = c.a + dispN(c, idx) * (c.b - c.a);
            rc(g, 270, y - 1.5f, hx - 270, 3, T.ink);
            tonerRect(g, hx - 7, y - 7, 14, 14, (drag || edit) ? T.red : T.ink);
            if (edit) editBox(g, 302, 442, 1);
            else txt(g, fMono, 10, 302, 436, fmtValue(c.param, S.values[c.param]).c_str(), (drag ? T.ink : mixc(T.ink2, T.ink, hv)), 1);
        } break;

        case tMini: {
            const float y = c.c, a = c.a, bb = c.b;
            const char* lbl = c.label.c_str();
            if (c.param == (int)pShFocus) { static const char* fl[3] = {"tight", "body", "presence"}; lbl = fl[(int)clampf(std::round(S.values[pShMode]), 0, 2)]; }
            if (c.param == (int)pRvDecay && std::lround(S.values[pRvMode]) == 2) lbl = "length";
            txt(g, fMed, 12.5f, c.x + 4, y - 6, lbl, (drag ? T.ink : mixc(T.ink2, T.ink, hv)));
            ln(g, a, y, bb, y, T.hair);
            ln(g, a, y - 3, a, y + 3, T.hair);
            ln(g, bb, y - 3, bb, y + 3, T.hair);
            const bool bip = c.param == (int)pShAttack || c.param == (int)pShSustain || c.param == (int)pShFocus;
            const float hx = a + dispN(c, idx) * (bb - a);
            if (bip) {
                const float m = (a + bb) * .5f;
                ln(g, m, y - 4, m, y + 4, T.hair);
                rc(g, std::min(m, hx), y - 1, std::fabs(hx - m), 2, T.ink);
            } else rc(g, a, y - 1, hx - a, 2, T.ink);
            rc(g, hx - 4.5f, y - 4.5f, 9, 9, (drag || edit) ? T.red : T.ink);
            if (edit) editBox(g, (float)c.id, y, 2);
            else txt(g, fMono, 9.5f, (float)c.id, y - 4.75f, fmtValue(c.param, S.values[c.param]).c_str(), (drag ? T.ink : mixc(T.ink2, T.ink, hv)), 2);
        } break;

        case tFilter: {
            const float y = c.c;
            txt(g, fMed, 12.5f, 664, y - 6, c.label.c_str(), (drag ? T.ink : mixc(T.ink2, T.ink, hv)));
            ln(g, c.a, y, c.b, y, T.hair);
            ln(g, c.a, y - 3, c.a, y + 3, T.hair);
            ln(g, c.b, y - 3, c.b, y + 3, T.hair);
            const float hx = c.a + dispN(c, idx) * (c.b - c.a);
            rc(g, hx - 4.5f, y - 4.5f, 9, 9, (drag || edit) ? T.red : T.ink);
            const std::string v = fmtValue(c.param, S.values[c.param]);
            if (edit) editBox(g, 940, y, 2);
            else txt(g, fMono, 10, 940, y - 5, v.c_str(), v == "off" ? T.grey : T.ink2, 2);
        } break;

        case tNumBox: { // transient |-> 100
            txt(g, fMed, 14.5f, 268, 477, "transient", (drag ? T.ink : mixc(T.ink2, T.ink, hv)));
            ln(g, 334, 474, 334, 494, T.ink, 1.5f);
            ln(g, 338, 484, 352, 484, T.ink, 1.5f);
            const float ax[3] = {347, 352, 347}, ay[3] = {479, 484, 489};
            poly(g, ax, ay, 3, T.ink, 1.5f);
            const bool active = S.values[c.param] < 99.5f;
            if (active) rc(g, 360, 471, 50, 26, T.red); else ro(g, 360, 471, 50, 26, (drag ? T.red : mixc(T.ink, T.red, hv)), 1.5f);
            std::snprintf(b, sizeof b, "%d", (int)std::lround(S.values[c.param]));
            if (edit) editBox(g, 385, 512, 1);
            txt(g, fMonoB, 12, 385, 478, b, active ? rgb(0xF4F2F3) : T.ink, 1);
            txt(g, fMono, 8, 420, 479, active ? "drag up to ease off" : "drag down to trim", T.ink);
        } break;

        case tToggle: {
            const bool onv = S.values[c.param] > 0.5f;
            if (c.id == sAutoGain) {
                ln(g, 104, 474, 104, 494, T.ink, 1.5f);
                if (onv) rc(g, 112, 478, 12, 12, T.red); else ro(g, 112, 478, 12, 12, T.ink, 1.2f);
                ln(g, 132, 474, 132, 494, T.ink, 1.5f);
                txt(g, fMed, 14.5f, 142, 477, "auto gain", mixc(T.ink2, T.ink, hv));
                ln(g, 212, 474, 212, 494, T.ink, 1.5f);
            } else {
                txt(g, fMed, 13.5f, 354, 551, "bypass", mixc(T.ink2, T.ink, hv));
                if (onv) rc(g, 402, 552, 10, 10, T.red); else ro(g, 402, 552, 10, 10, T.ink, 1.2f);
            }
        } break;

        case tOption: {
            const bool sel = (int)std::lround(S.values[c.param]) == c.id;
            if (c.param == (int)pQuality) {
                if (c.id == 0) txt(g, fMed, 13.5f, 442, 551, "quality", T.ink2);
                const float sx = c.id ? 534.f : 496.f;
                if (sel) rc(g, sx, 552, 10, 10, T.red); else ro(g, sx, 552, 10, 10, T.grey, 1.2f);
                txt(g, fMed, 13.5f, sx + 14, 551, c.label.c_str(), sel ? T.ink : (mixc(T.grey, T.ink2, hv)));
                break;
            }
            const float fz = c.b > 0 ? c.b : 12.5f;
            txt(g, fMed, fz, c.a, c.c - 6 + (12.5f - fz) * 0.5f, c.label.c_str(), sel ? T.red : (mixc(T.grey, T.ink, hv)));
            if (sel) rc(g, c.a, c.c + 11, tw(fMed, fz, c.label.c_str()), 1.5f, T.red);
        } break;

        case tOnOff: {
            const bool sel = (S.values[c.param] > 0.5f) == (c.id == 1);
            if (sel) rc(g, c.a, c.c - 3.5f, 7, 7, T.red); else dot(g, c.a + 3.5f, c.c, 1.6f, T.grey);
            txt(g, fMed, 12.5f, c.a + 11, c.c - 6, c.label.c_str(), sel ? T.red : (mixc(T.grey, T.ink, hv)));
        } break;

        case tEqPos: {
            int pt = 0, pc = 0;
            for (int k = 0; k < kNumStages; ++k) { if (S.order[k] == kStTone) pt = k; if (S.order[k] == kStClip) pc = k; }
            const bool sel = (c.id == 0) == (pt < pc);
            txt(g, fMed, kOptFont, c.a, c.c - 6 + (12.5f - kOptFont) * 0.5f, c.label.c_str(), sel ? T.red : (mixc(T.grey, T.ink, hv)));
            if (sel) rc(g, c.a, c.c + 11, tw(fMed, kOptFont, c.label.c_str()), 1.5f, T.red);
        } break;

        case tSectionHead: {
            static const char* names[kNumStages] = {"sampler", "preamp", "squash", "tone", "clip", "gate/width", "transient", "shape", "verb", "weight"};
            const int pos = (int)c.a;
            const float x = secXs(pos), y = secYs(pos);
            const bool dim = reorderFrom == pos && reorderMoved;
            int shownPos = pos;   // while another module is dragged, the number follows where this one is heading
            for (int k = 0; k < kNumStages; ++k) if (tentOrder[k] == c.id) shownPos = k;
            std::snprintf(b, sizeof b, "%02d", shownPos + 1);
            txt(g, fMono, 9, x + 12, y + 16, b, T.ink);
            txt(g, fSemi, 15, x + 30, y + 13.5f, names[c.id], dim ? T.grey : (mixc(T.ink, T.red, hv)));
        } break;

        case tSectionPower: {
            const bool onv = S.values[c.param] > 0.5f;
            if (onv) rc(g, c.a + kSecW - 24, c.c + 15, 11, 11, T.red); else ro(g, c.a + kSecW - 24, c.c + 15, 11, 11, mixc(T.grey, T.ink, hv), 1.2f);
        } break;

        case tButton: {
            switch (c.id) {
            case -1: txt(g, fMed, 12.5f, c.a, c.c - 6, c.label.c_str(), T.ink2); break;
            case bPrev: {
                g.beginPath(); g.moveTo(104, 557); g.lineTo(111, 552); g.lineTo(111, 562); g.closePath();
                g.fillColor(mixc(T.ink, T.red, hv)); g.fill();
            } break;
            case bNext: {
                g.beginPath(); g.moveTo(214, 552); g.lineTo(221, 557); g.lineTo(214, 562); g.closePath();
                g.fillColor(mixc(T.ink, T.red, hv)); g.fill();
            } break;
            case bPresetName: {
                std::string nm = presetName(S.presetIdx);
                if (S.presetModified) nm += " *";
                while (nm.size() > 2 && tw(fMed, 13.5f, nm.c_str()) > 88.f) nm = nm.substr(0, nm.size() - 2) + "~";
                txt(g, fMed, 13.5f, 119, 551, nm.c_str(), (presetOpen ? T.red : mixc(T.ink, T.red, hv)));
            } break;
            case bSave: txt(g, fMed, 13.5f, 240, 551, "save", mixc(T.grey, T.ink, hv)); break;
            case bAB: {
                const float wa = measureText(fMed, 13.5f, "a", -.025f), ws = measureText(fMed, 13.5f, " / ", -.025f);
                txt(g, fMed, 13.5f, 298, 551, "a", S.abCur == 0 ? T.red : (mixc(T.grey, T.ink, hv)));
                txt(g, fMed, 13.5f, 298 + wa, 551, " / ", T.grey);
                txt(g, fMed, 13.5f, 298 + wa + ws, 551, "b", S.abCur == 1 ? T.red : (mixc(T.grey, T.ink, hv)));
            } break;
            case bBake: {
                rc(g, 744, 548, 74, 26, T.yellow);
                cells(g, 818, 548, kStep2, 2, 5, T.yellow);
                if (hov || S.bakeOpen) ro(g, 744, 548, 74, 26, T.onMint, 1.f);
                txt(g, fSemi, 14, 756, 554, "bake", T.onMint);
                {
                    const int cs = S.capture.state();
                    if (cs == Capture::kArmed || cs == Capture::kRec || cs == Capture::kStopReq) {
                        g.beginPath(); g.circle(745, 549, 4.5f); g.fillColor(T.paper); g.fill();
                        g.beginPath(); g.circle(745, 549, 3.2f); g.fillColor(cs == Capture::kArmed ? alpha(T.red, .55f) : T.red); g.fill();
                    }
                }
                const float d = S.bakeOpen ? 1.f : -1.f;
                const float xs[3] = {795, 800, 805}, ys[3] = {561.5f - 2.5f * d, 561.5f + 2.5f * d, 561.5f - 2.5f * d};
                poly(g, xs, ys, 3, T.onMint, 1.5f);
            } break;
            case bResetOrder: txt(g, fMed, 13.5f, 944, 555, "reset order", mixc(T.grey, T.ink, hv), 2); break;
            case bTheme: {
                const float wd = measureText(fMono, 8.5f, "dark", .01f), ws = measureText(fMono, 8.5f, " / ", .01f), wl = measureText(fMono, 8.5f, "light", .01f);
                const float x0 = 940 - wd - ws - wl;
                const float m = themeMix;   // the underline slides across as the theme changes
                txt(g, fMono, 8.5f, x0, 14, "light", mixc(mixc(T.grey, T.ink2, hv), T.ink, 1.f - m));
                txt(g, fMono, 8.5f, x0 + wl, 14, " / ", T.grey);
                txt(g, fMono, 8.5f, x0 + wl + ws, 14, "dark", mixc(mixc(T.grey, T.ink2, hv), T.ink, m));
                rc(g, x0 + (wl + ws) * m, 25, (wl + (wd - wl) * m) - .1f * 8.5f, 1.5f, T.red);
            } break;
            default: break;
            }
        } break;

        case tResize: {
            for (int k = 0; k < 3; ++k) { const float o = 5 + k * 5; ln(g, 956 - o, 596, 956, 596 - o, mixc(T.grey, T.ink, hv), .9f); }
        } break;
        }
    }
#ifdef MERCY_AD_RENDER
public: // only for rendering promo footage, never in the plugin
    void adSetBakeItems(const std::vector<Baker::Item>& v) { bakeItems = v; }
    void adLoadPreset(int i) { loadUserPresets(); loadPreset(i); }
    int adNumPresets() const { return numPresets(); }
#endif
};

} // namespace mercy
