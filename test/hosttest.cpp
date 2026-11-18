// slapworx host torture test (CLAP). Loads the real plugin and does the things DAWs do that have
// bitten us, or that commonly break plugins in some host. Every bug we fix gets a check here.
//
//   hosttest <path/to/Plugin.clap>          (run under xvfb-run for the window checks)
//
// Checks:
//   params      names, ranges and defaults are sane; nothing is flagged CLAP_PARAM_IS_BYPASS
//               (Bitwig ties that flag to its device on/off button: the 2026-11-17 bypass bug)
//   bypass      the plugin's bypass on/off/on/off works, also with processing stopped in between
//   ui-flush    a click in the plugin window while the host is NOT processing still reaches the host
//               (request_flush + params.flush), the root cause of the Bitwig bypass bug
//   namespace   parameter events meant for another namespace are ignored
//   blocks      the sound doesn't depend on the host's buffer size (1, 7, 64, 512, 4096)
//   rates       44.1 / 48 / 88.2 / 96 / 192 kHz: finite, not silent, sensible level
//   extremes    long silence, +12 dBFS noise and DC: no NaN/inf, no runaway levels
//   state       random settings -> save -> new instance -> load: same values, same sound
//   fixture     a project saved by an earlier version still loads with the same settings
//   instances   several copies at once sound exactly like one copy (no shared state)
//   lifecycle   activate/deactivate at different rates, reset mid-play, many times
//   window      open/close the editor 10 times; at 2x scale the window is exactly 2x
//
// Build: g++ -std=c++17 -O1 -I../dpf/distrho/src hosttest.cpp -o hosttest -ldl -lX11 -lXtst
#include "clap/entry.h"
#include "clap/plugin-factory.h"
#include "clap/ext/params.h"
#include "clap/ext/state.h"
#include "clap/ext/gui.h"
#include "clap/ext/timer-support.h"
#include "clap/ext/latency.h"
#include "clap/events.h"
#include "clap/process.h"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>

static int g_fail = 0, g_pass = 0;
static void check(bool ok, const char* group, const std::string& what) {
    std::printf("%s  [%s] %s\n", ok ? "PASS" : "FAIL", group, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

// ---------------------------------------------------------------------------------------- host
struct Host {
    clap_host_t h{};
    clap_host_params_t params{};
    clap_host_timer_support_t timers{};
    clap_host_latency_t latency{};
    clap_host_gui_t gui{};
    bool flushRequested = false, restartRequested = false, inActivate = false;
    int latencyChangedOutsideActivate = 0;
    clap_id timerId = 0; bool haveTimer = false;
    Host() {
        h.clap_version = CLAP_VERSION; h.host_data = this; h.name = "slapworx hosttest"; h.vendor = "slapworx";
        h.url = ""; h.version = "1";
        h.get_extension = [](const clap_host_t* hh, const char* id) -> const void* {
            Host* s = (Host*)hh->host_data;
            if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &s->params;
            if (!std::strcmp(id, CLAP_EXT_TIMER_SUPPORT)) return &s->timers;
            if (!std::strcmp(id, CLAP_EXT_LATENCY)) return &s->latency;
            if (!std::strcmp(id, CLAP_EXT_GUI)) return &s->gui;
            return nullptr;
        };
        h.request_restart = [](const clap_host_t* hh) { ((Host*)hh->host_data)->restartRequested = true; };
        h.request_process = [](const clap_host_t*) {};
        h.request_callback = [](const clap_host_t*) {};
        params.rescan = [](const clap_host_t*, clap_param_rescan_flags) {};
        params.clear = [](const clap_host_t*, clap_id, clap_param_clear_flags) {};
        params.request_flush = [](const clap_host_t* hh) { ((Host*)hh->host_data)->flushRequested = true; };
        timers.register_timer = [](const clap_host_t* hh, uint32_t, clap_id* id) {
            Host* s = (Host*)hh->host_data; *id = ++s->timerId; s->haveTimer = true; return true; };
        timers.unregister_timer = [](const clap_host_t* hh, clap_id) { ((Host*)hh->host_data)->haveTimer = false; return true; };
        gui.resize_hints_changed = [](const clap_host_t*) {};
        gui.request_resize = [](const clap_host_t*, uint32_t, uint32_t) { return true; };
        gui.request_show = [](const clap_host_t*) { return true; };
        gui.request_hide = [](const clap_host_t*) { return true; };
        gui.closed = [](const clap_host_t*, bool) {};
        latency.changed = [](const clap_host_t* hh) {
            Host* s = (Host*)hh->host_data; if (!s->inActivate) ++s->latencyChangedOutsideActivate; };
    }
};

// events
struct InEvents {
    std::vector<clap_event_param_value_t> ev;
    clap_input_events_t list{};
    InEvents() {
        list.ctx = this;
        list.size = [](const clap_input_events_t* l) { return (uint32_t)((InEvents*)l->ctx)->ev.size(); };
        list.get = [](const clap_input_events_t* l, uint32_t i) -> const clap_event_header_t* { return &((InEvents*)l->ctx)->ev[i].header; };
    }
    void add(clap_id id, double v, uint16_t space = CLAP_CORE_EVENT_SPACE_ID, uint32_t time = 0) {
        clap_event_param_value_t e{};
        e.header.size = sizeof e; e.header.time = time; e.header.space_id = space; e.header.type = CLAP_EVENT_PARAM_VALUE;
        e.param_id = id; e.note_id = -1; e.port_index = -1; e.channel = -1; e.key = -1; e.value = v;
        ev.push_back(e);
    }
};
struct OutEvents {
    std::vector<clap_event_param_value_t> values;
    clap_output_events_t list{};
    OutEvents() {
        list.ctx = this;
        list.try_push = [](const clap_output_events_t* l, const clap_event_header_t* e) {
            if (e->type == CLAP_EVENT_PARAM_VALUE) ((OutEvents*)l->ctx)->values.push_back(*(const clap_event_param_value_t*)e);
            return true; };
    }
};
struct Stream {   // for state save/load
    std::vector<uint8_t> data; size_t pos = 0;
    clap_ostream_t os{}; clap_istream_t is{};
    Stream() {
        os.ctx = this; is.ctx = this;
        os.write = [](const clap_ostream_t* s, const void* b, uint64_t n) -> int64_t {
            auto* st = (Stream*)s->ctx; st->data.insert(st->data.end(), (const uint8_t*)b, (const uint8_t*)b + n); return (int64_t)n; };
        is.read = [](const clap_istream_t* s, void* b, uint64_t n) -> int64_t {
            auto* st = (Stream*)s->ctx; const size_t k = std::min<size_t>(n, st->data.size() - st->pos);
            std::memcpy(b, st->data.data() + st->pos, k); st->pos += k; return (int64_t)k; };
    }
};

// ---------------------------------------------------------------------------------------- plugin
static const clap_plugin_entry_t* g_entry = nullptr;
static const clap_plugin_factory_t* g_factory = nullptr;
static std::string g_id;

struct Inst {
    Host host;
    const clap_plugin_t* p = nullptr;
    const clap_plugin_params_t* params = nullptr;
    const clap_plugin_state_t* state = nullptr;
    const clap_plugin_gui_t* gui = nullptr;
    const clap_plugin_timer_support_t* timer = nullptr;
    double sr = 44100; uint32_t maxFrames = 4096; bool active = false, processing = false;
    int64_t steady = 0;
    Inst() {
        p = g_factory->create_plugin(g_factory, &host.h, g_id.c_str());
        p->init(p);
        params = (const clap_plugin_params_t*)p->get_extension(p, CLAP_EXT_PARAMS);
        state = (const clap_plugin_state_t*)p->get_extension(p, CLAP_EXT_STATE);
        gui = (const clap_plugin_gui_t*)p->get_extension(p, CLAP_EXT_GUI);
        timer = (const clap_plugin_timer_support_t*)p->get_extension(p, CLAP_EXT_TIMER_SUPPORT);
    }
    ~Inst() { stop(); deactivate(); p->destroy(p); }
    void activate(double rate = 44100, uint32_t mx = 4096) {
        if (active) deactivate();
        sr = rate; maxFrames = mx;
        host.inActivate = true; p->activate(p, rate, 1, mx); host.inActivate = false;
        active = true;
    }
    void deactivate() { if (active) { stop(); p->deactivate(p); active = false; } }
    void start() { if (active && !processing) { p->start_processing(p); processing = true; } }
    void stop() { if (processing) { p->stop_processing(p); processing = false; } }
    // process `n` frames of stereo audio in blocks of `block`
    void run(const float* inL, const float* inR, float* outL, float* outR, size_t n, uint32_t block, InEvents* first = nullptr) {
        start();
        std::vector<float> bl(block), br(block), ol(block), orr(block);
        InEvents empty; OutEvents out;
        for (size_t done = 0; done < n; done += block) {
            const uint32_t k = (uint32_t)std::min<size_t>(block, n - done);
            std::memcpy(bl.data(), inL + done, k * 4); std::memcpy(br.data(), inR + done, k * 4);
            const float* ins[2] = { bl.data(), br.data() }; float* outs[2] = { ol.data(), orr.data() };
            clap_audio_buffer_t ib{ (float**)ins, nullptr, 2, 0, 0 }, ob{ outs, nullptr, 2, 0, 0 };
            clap_process_t pr{}; pr.steady_time = steady; pr.frames_count = k;
            pr.audio_inputs = &ib; pr.audio_outputs = &ob; pr.audio_inputs_count = 1; pr.audio_outputs_count = 1;
            pr.in_events = (done == 0 && first) ? &first->list : &empty.list; pr.out_events = &out.list;
            p->process(p, &pr);
            std::memcpy(outL + done, ol.data(), k * 4); std::memcpy(outR + done, orr.data(), k * 4);
            steady += k;
        }
    }
    std::vector<clap_param_info_t> infos() const {
        std::vector<clap_param_info_t> v;
        for (uint32_t i = 0; i < params->count(p); ++i) { clap_param_info_t inf; params->get_info(p, i, &inf); v.push_back(inf); }
        return v;
    }
    double get(clap_id id) const { double v = 0; params->get_value(p, id, &v); return v; }
};

// ---------------------------------------------------------------------------------------- audio
static std::vector<float> drums(size_t n, double sr, unsigned seed = 1) {
    std::vector<float> v(n); std::mt19937 rng(seed); std::uniform_real_distribution<float> u(-1, 1);
    const size_t hit = (size_t)(sr * 0.25);
    for (size_t i = 0; i < n; ++i) {
        const double ph = (double)(i % hit), t = ph / sr;
        const bool snare = ((i / hit) % 2) == 1;
        v[i] = snare ? (float)(0.6 * std::exp(-t * 25) * u(rng) + 0.3 * std::exp(-t * 30) * std::sin(2 * M_PI * 190 * t))
                     : (float)(0.9 * std::exp(-t * 12) * std::sin(2 * M_PI * 55 * t * (1 + 2 * std::exp(-t * 40)))) ;
        v[i] += 0.02f * u(rng);
    }
    return v;
}
static double rmsDb(const std::vector<float>& a, size_t from = 0) {
    double s = 0; for (size_t i = from; i < a.size(); ++i) s += (double)a[i] * a[i];
    return 10 * std::log10(s / std::max<size_t>(1, a.size() - from) + 1e-30);
}
static double diffDb(const std::vector<float>& a, const std::vector<float>& b, size_t from = 0) {
    double e = 0, s = 0; for (size_t i = from; i < a.size(); ++i) { e += (double)(a[i] - b[i]) * (a[i] - b[i]); s += (double)b[i] * b[i]; }
    return 10 * std::log10((e + 1e-30) / (s + 1e-30));
}
static bool allFinite(const std::vector<float>& a, float lim = 1e6f) { for (float x : a) if (!std::isfinite(x) || std::fabs(x) > lim) return false; return true; }
static float peak(const std::vector<float>& a) { float m = 0; for (float x : a) m = std::max(m, std::fabs(x)); return m; }
static std::string fmt(const char* f, double a, double b = 0) { char s[160]; std::snprintf(s, sizeof s, f, a, b); return s; }

static clap_id findParam(Inst& in, const char* name) {
    for (auto& i : in.infos()) if (!std::strcmp(i.name, name)) return i.id;
    return CLAP_INVALID_ID;
}

// ---------------------------------------------------------------------------------------- tests
static void testParams() {
    Inst in; int bypassFlag = 0, bad = 0, n = 0;
    for (auto& i : in.infos()) {
        ++n;
        if (i.flags & CLAP_PARAM_IS_BYPASS) ++bypassFlag;
        if (!i.name[0] || !(i.min_value <= i.default_value && i.default_value <= i.max_value) || !(i.min_value < i.max_value)) {
            ++bad; std::printf("      bad parameter: id %u '%s' [%g .. %g] default %g\n", i.id, i.name, i.min_value, i.max_value, i.default_value);
        }
    }
    check(n > 0, "params", fmt("plugin has %.0f parameters", n));
    check(bypassFlag == 0, "params", "no parameter is flagged as the host's bypass (CLAP_PARAM_IS_BYPASS)");
    check(bad == 0, "params", "every parameter has a name, min < max and the default inside the range");
}

static void testBypass() {
    Inst in; clap_id byp = findParam(in, "Bypass");
    check(byp != CLAP_INVALID_ID, "bypass", "plugin has a \"Bypass\" parameter");
    if (byp == CLAP_INVALID_ID) return;
    in.activate();
    const size_t N = 44100; auto x = drums(N, 44100);
    std::vector<float> l(N), r(N);
    auto pass = [&](double v) { InEvents e; e.add(byp, v); in.run(x.data(), x.data(), l.data(), r.data(), N, 512, &e); return diffDb(l, x, N / 2); };
    // latency: compare against the input delayed by the reported latency
    const clap_plugin_latency_t* lat = (const clap_plugin_latency_t*)in.p->get_extension(in.p, CLAP_EXT_LATENCY);
    const uint32_t L = lat ? lat->get(in.p) : 0;
    auto passAligned = [&](double v) {
        InEvents e; e.add(byp, v); in.run(x.data(), x.data(), l.data(), r.data(), N, 512, &e);
        std::vector<float> xd(N, 0.f); for (size_t i = L; i < N; ++i) xd[i] = x[i - L];
        return diffDb(l, xd, N / 2);
    };
    const double on0 = pass(0);
    check(on0 > -30, "bypass", fmt("effect on changes the sound (%.1f dB from the input)", on0));
    for (int round = 1; round <= 3; ++round) {
        const double b = passAligned(1);
        check(b < -60, "bypass", fmt("round %.0f, bypass on: output is the input (%.1f dB)", round, b));
        if (round == 2) { in.stop(); in.start(); }   // the host stops and restarts processing in between
        const double a = pass(0);
        check(a > -30, "bypass", fmt("round %.0f, bypass off: the effect is back (%.1f dB)", round, a));
    }
}

// click the bypass button in the plugin window while the host is NOT processing (Bitwig with the
// device switched off): the plugin must ask for a flush, and the flush must deliver the change
static void testUiFlush(Display* dpy) {
    Inst in; clap_id byp = findParam(in, "Bypass");
    if (!in.gui || byp == CLAP_INVALID_ID || !dpy) { check(false, "ui-flush", "plugin window or bypass missing"); return; }
    in.activate();                       // active, but process() is never called
    Window par = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1000, 640, 0, 0, 0x202020);
    XMapWindow(dpy, par); XSync(dpy, False);
    in.gui->create(in.p, CLAP_WINDOW_API_X11, false);
    clap_window_t w{}; w.api = CLAP_WINDOW_API_X11; w.x11 = par;
    in.gui->set_parent(in.p, &w); in.gui->show(in.p);
    auto pump = [&](int frames) { for (int i = 0; i < frames; ++i) {
        if (in.host.haveTimer && in.timer) in.timer->on_timer(in.p, in.host.timerId);
        while (XPending(dpy)) { XEvent ev; XNextEvent(dpy, &ev); } usleep(16000); } };
    pump(40);
    in.host.flushRequested = false;
    // Mercy's bypass box on the main page (design coordinates; window at 1x)
    XTestFakeMotionEvent(dpy, -1, 383, 557, 0); XTestFakeButtonEvent(dpy, 1, True, 0); XTestFakeButtonEvent(dpy, 1, False, 30);
    XSync(dpy, False); pump(20);
    check(in.host.flushRequested, "ui-flush", "a click in the window asks the host for a flush while it isn't processing");
    OutEvents out; InEvents none;
    if (in.host.flushRequested) in.params->flush(in.p, &none.list, &out.list);
    bool sawBypass = false; for (auto& e : out.values) if (e.param_id == byp && e.value > 0.5) sawBypass = true;
    check(sawBypass, "ui-flush", "the flush tells the host that bypass is now on");
    check(in.get(byp) > 0.5, "ui-flush", "the plugin itself has bypass on");
    // click again: off
    in.host.flushRequested = false;
    XTestFakeMotionEvent(dpy, -1, 383, 557, 0); XTestFakeButtonEvent(dpy, 1, True, 0); XTestFakeButtonEvent(dpy, 1, False, 30);
    XSync(dpy, False); pump(20);
    OutEvents out2; if (in.host.flushRequested) in.params->flush(in.p, &none.list, &out2.list);
    check(in.host.flushRequested && in.get(byp) < 0.5, "ui-flush", "a second click turns bypass off again, still without process()");
    in.gui->hide(in.p); in.gui->destroy(in.p); XDestroyWindow(dpy, par); XSync(dpy, False);
}

static void testNamespace() {
    Inst in; in.activate();
    std::vector<std::pair<clap_id, double>> before;
    InEvents e;
    for (auto& i : in.infos()) {
        if (i.flags & CLAP_PARAM_IS_READONLY) continue;
        before.push_back({ i.id, in.get(i.id) });
        e.add(i.id, i.max_value, 0xb33f);
    }
    const size_t N = 4096; auto x = drums(N, 44100); std::vector<float> l(N), r(N);
    in.run(x.data(), x.data(), l.data(), r.data(), N, 512, &e);
    int changed = 0; for (auto& b : before) if (in.get(b.first) != b.second) ++changed;
    check(changed == 0, "namespace", fmt("events for another namespace change nothing (%.0f changed)", changed));
}

static void testBlocks() {
    const size_t N = 44100 * 2; auto x = drums(N, 44100);
    std::vector<float> ref(N), r(N);
    { Inst in; in.activate(44100, 4096); in.run(x.data(), x.data(), ref.data(), r.data(), N, 512); }
    for (uint32_t b : { 1u, 7u, 64u, 4096u }) {
        std::vector<float> l(N); Inst in; in.activate(44100, 4096);
        in.run(x.data(), x.data(), l.data(), r.data(), N, b);
        const double d = diffDb(l, ref, 4410);
        check(allFinite(l) && d < -60, "blocks", fmt("buffer size %4.0f sounds the same as 512 (%.1f dB)", b, d));
    }
}

static void testRates() {
    double ref = 0;
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 }) {
        const size_t N = (size_t)sr * 2; auto x = drums(N, sr); std::vector<float> l(N), r(N);
        Inst in; in.activate(sr, 1024); in.run(x.data(), x.data(), l.data(), r.data(), N, 1024);
        const double lv = rmsDb(l, N / 4);
        if (sr == 44100.0) ref = lv;
        check(allFinite(l) && lv > -60 && std::fabs(lv - ref) < 6, "rates",
              fmt("%.1f kHz: finite, not silent, level within 6 dB of 44.1 kHz (%+.1f dB)", sr / 1000, lv - ref));
    }
}

static void testExtremes() {
    Inst in; in.activate(48000, 512);
    const size_t N = 48000 * 10; std::vector<float> z(N, 0.f), l(N), r(N);
    in.run(z.data(), z.data(), l.data(), r.data(), N, 512);
    check(allFinite(l) && peak(l) < 1e-3f, "extremes", fmt("10 s of silence stays silent and finite (peak %.2g)", peak(l)));
    std::mt19937 rng(7); std::uniform_real_distribution<float> u(-4, 4);   // +12 dBFS noise
    std::vector<float> hot(48000 * 3); for (auto& s : hot) s = u(rng);
    std::vector<float> lh(hot.size()), rh(hot.size());
    in.run(hot.data(), hot.data(), lh.data(), rh.data(), hot.size(), 512);
    check(allFinite(lh, 64.f), "extremes", fmt("+12 dBFS noise: finite, peak %.1f", peak(lh)));
    std::vector<float> dc(48000 * 2, 0.5f), ld(dc.size()), rd(dc.size());
    in.run(dc.data(), dc.data(), ld.data(), rd.data(), dc.size(), 512);
    check(allFinite(ld, 16.f), "extremes", fmt("DC input: finite, peak %.2f", peak(ld)));
    std::vector<float> l2(48000); auto x = drums(48000, 48000);
    in.run(x.data(), x.data(), l2.data(), rd.data(), l2.size(), 512);
    check(allFinite(l2) && rmsDb(l2, 24000) > -60, "extremes", "normal audio afterwards still works");
}

static std::vector<std::pair<clap_id, double>> randomize(Inst& in, InEvents& e, unsigned seed) {
    std::mt19937 rng(seed); std::vector<std::pair<clap_id, double>> set;
    for (auto& i : in.infos()) {
        if (i.flags & CLAP_PARAM_IS_READONLY) continue;
        if (!std::strcmp(i.name, "Bypass")) continue;     // keep the effect audible
        std::uniform_real_distribution<double> u(i.min_value, i.max_value);
        double v = u(rng); if (i.flags & CLAP_PARAM_IS_STEPPED) v = std::round(v);
        e.add(i.id, v); set.push_back({ i.id, v });
    }
    return set;
}

static bool sameValues(Inst& a, Inst& b, int* nBad) {
    *nBad = 0;
    for (auto& i : a.infos()) {
        if (i.flags & CLAP_PARAM_IS_READONLY) continue;
        const double va = a.get(i.id), vb = b.get(i.id);
        if (std::fabs(va - vb) > 1e-6 * (i.max_value - i.min_value) + 1e-9) {
            if (++*nBad <= 5) std::printf("      '%s': %g vs %g\n", i.name, va, vb);
        }
    }
    return *nBad == 0;
}

static void testState() {
    for (unsigned seed : { 11u, 22u, 33u }) {
        Inst a; a.activate(); InEvents e; randomize(a, e, seed);
        const size_t N = 44100; auto x = drums(N, 44100); std::vector<float> la(N), r(N), lb(N);
        a.run(x.data(), x.data(), la.data(), r.data(), 512, 512, &e);   // apply the settings
        Stream s; const bool saved = a.state->save(a.p, &s.os);
        Inst b; s.pos = 0; const bool loaded = b.state->load(b.p, &s.is);
        int nBad = 0; const bool same = sameValues(a, b, &nBad);
        check(saved && loaded && same, "state", fmt("random settings #%.0f: save -> load in a new instance gives the same values (%.0f differ)", seed / 11, nBad));
        // same sound from a fresh start
        Inst a2; Stream s2; s2.data = s.data; a2.state->load(a2.p, &s2.is);
        a2.activate(); b.activate();
        a2.run(x.data(), x.data(), la.data(), r.data(), N, 512); b.run(x.data(), x.data(), lb.data(), r.data(), N, 512);
        const double d = diffDb(lb, la);
        check(d < -120 || (rmsDb(la) < -100 && rmsDb(lb) < -100), "state", fmt("random settings #%.0f: both loaded copies sound identical (%.1f dB)", seed / 11, d));
    }
}

static void testFixture(const std::string& dir) {
    // fixtures/<name>.state = a saved state, fixtures/<name>.values = "name=value" lines it must restore
    for (const char* name : { "mercy-1.0" }) {
        std::ifstream fs(dir + "/" + name + ".state", std::ios::binary);
        std::ifstream fv(dir + "/" + name + ".values");
        if (!fs || !fv) { check(false, "fixture", std::string(name) + ": fixture files missing"); continue; }
        Stream s; s.data.assign(std::istreambuf_iterator<char>(fs), {});
        Inst in; const bool ok = in.state->load(in.p, &s.is);
        std::map<std::string, clap_id> ids; std::map<clap_id, clap_param_info_t> info;
        for (auto& i : in.infos()) { ids[i.name] = i.id; info[i.id] = i; }
        int nBad = 0, n = 0; std::string line;
        while (std::getline(fv, line)) {
            const size_t eq = line.rfind('='); if (eq == std::string::npos) continue;
            const std::string pn = line.substr(0, eq); const double want = std::atof(line.c_str() + eq + 1);
            ++n;
            if (!ids.count(pn)) { if (++nBad <= 5) std::printf("      '%s' no longer exists\n", pn.c_str()); continue; }
            const auto& i = info[ids[pn]]; const double got = in.get(ids[pn]);
            if (std::fabs(got - want) > 1e-5 * (i.max_value - i.min_value) + 1e-9) { if (++nBad <= 5) std::printf("      '%s': %g, want %g\n", pn.c_str(), got, want); }
        }
        check(ok && n > 0 && nBad == 0, "fixture", fmt("a project saved by Mercy 1.0 loads with all %.0f settings intact (%.0f wrong)", n, nBad));
    }
}

static void makeFixture(const std::string& dir) {
    Inst a; a.activate(); InEvents e; randomize(a, e, 1234);
    std::vector<float> x(512, 0.f), l(512), r(512);
    a.run(x.data(), x.data(), l.data(), r.data(), 512, 512, &e);
    Stream s; a.state->save(a.p, &s.os);
    std::ofstream(dir + "/mercy-1.0.state", std::ios::binary).write((const char*)s.data.data(), (std::streamsize)s.data.size());
    std::ofstream fv(dir + "/mercy-1.0.values");
    for (auto& i : a.infos()) if (!(i.flags & CLAP_PARAM_IS_READONLY)) { char b[64]; std::snprintf(b, sizeof b, "%.9g", a.get(i.id)); fv << i.name << "=" << b << "\n"; }
    std::printf("wrote %s/mercy-1.0.state (%zu bytes) and .values\n", dir.c_str(), s.data.size());
}

static void testInstances() {
    const size_t N = 44100; auto x = drums(N, 44100);
    std::vector<float> solo(N), r(N);
    { Inst in; in.activate(); in.run(x.data(), x.data(), solo.data(), r.data(), N, 256); }
    Inst a, b, c; a.activate(); b.activate(); c.activate();
    std::vector<float> la(N), lb(N), lc(N), rr(256);
    for (size_t i = 0; i < N; i += 256) {   // interleave block by block, like a host running three tracks
        const uint32_t k = (uint32_t)std::min<size_t>(256, N - i);
        a.run(x.data() + i, x.data() + i, la.data() + i, rr.data(), k, k);
        b.run(x.data() + i, x.data() + i, lb.data() + i, rr.data(), k, k);
        c.run(x.data() + i, x.data() + i, lc.data() + i, rr.data(), k, k);
    }
    const double d = std::max(diffDb(la, solo), std::max(diffDb(lb, solo), diffDb(lc, solo)));
    check(d < -120, "instances", fmt("three copies at once each sound exactly like one copy (%.1f dB)", d));
}

static void testLifecycle() {
    Inst in; bool ok = true;
    const double rates[] = { 44100, 48000, 96000, 44100, 88200, 48000 };
    for (int k = 0; k < 12; ++k) {
        const double sr = rates[k % 6]; in.activate(sr, 2048);
        const size_t N = (size_t)(sr / 4); auto x = drums(N, sr); std::vector<float> l(N), r(N);
        in.run(x.data(), x.data(), l.data(), r.data(), N, 2048);
        if (k % 3 == 1) in.p->reset(in.p);
        in.run(x.data(), x.data(), l.data(), r.data(), N, 2048);
        ok = ok && allFinite(l) && rmsDb(l, N / 2) > -60;
    }
    check(ok, "lifecycle", "12 activate/deactivate cycles at changing sample rates, with resets: always finite and audible");
    check(in.host.latencyChangedOutsideActivate == 0, "lifecycle",
          fmt("latency changes are reported only inside activate (%.0f outside)", in.host.latencyChangedOutsideActivate));
}

static void testWindow(Display* dpy) {
    if (!dpy) { check(false, "window", "no X display (run under xvfb-run)"); return; }
    Inst in; bool ok = true;
    for (int k = 0; k < 10 && ok; ++k) {
        Window par = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 2000, 1300, 0, 0, 0);
        XMapWindow(dpy, par); XSync(dpy, False);   // the plugin talks to X on its own connection
        ok = in.gui->create(in.p, CLAP_WINDOW_API_X11, false);
        if (k == 9) in.gui->set_scale(in.p, 2.0);
        clap_window_t w{}; w.api = CLAP_WINDOW_API_X11; w.x11 = par;
        ok = ok && in.gui->set_parent(in.p, &w) && in.gui->show(in.p);
        for (int i = 0; i < 6; ++i) { if (in.host.haveTimer && in.timer) in.timer->on_timer(in.p, in.host.timerId);
            while (XPending(dpy)) { XEvent ev; XNextEvent(dpy, &ev); } usleep(10000); }
        if (k == 9) {
            uint32_t ww = 0, hh = 0; in.gui->get_size(in.p, &ww, &hh);
            check(ww == 1920 && hh == 1200, "window", fmt("at 2x scale the window is exactly 2x: %.0f wide", ww) + " x " + std::to_string(hh));
        }
        in.gui->hide(in.p); in.gui->destroy(in.p); XDestroyWindow(dpy, par); XSync(dpy, False);
    }
    check(ok, "window", "open and close the plugin window 10 times");
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: hosttest <Plugin.clap> [--make-fixture <dir>] [--fixtures <dir>]\n"); return 2; }
    void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { std::fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
    g_entry = (const clap_plugin_entry_t*)dlsym(lib, "clap_entry");
    g_entry->init(argv[1]);
    g_factory = (const clap_plugin_factory_t*)g_entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    g_id = g_factory->get_plugin_descriptor(g_factory, 0)->id;
    std::string fixtures = "fixtures";
    for (int i = 2; i + 1 < argc; ++i) {
        if (!std::strcmp(argv[i], "--make-fixture")) { makeFixture(argv[i + 1]); return 0; }
        if (!std::strcmp(argv[i], "--fixtures")) fixtures = argv[i + 1];
    }
    Display* dpy = XOpenDisplay(nullptr);
    testParams(); testBypass(); testUiFlush(dpy); testNamespace(); testBlocks(); testRates(); testExtremes();
    testState(); testFixture(fixtures); testInstances(); testLifecycle(); testWindow(dpy);
    if (dpy) XCloseDisplay(dpy);
    g_entry->deinit();
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
