// Mercy - bypass test (CLAP host).
//
//   bypasstest <path/to/Mercy.clap>
//
// 1. The "Bypass" parameter must be a plain switch: not flagged CLAP_PARAM_IS_BYPASS. With that flag
//    Bitwig ties it to its device on/off button and stops running the plugin when Mercy's own bypass
//    turns on, so the click that turns it off never arrives (customer report, Bitwig on Mac).
// 2. Bypass on -> the output is the input (after the 15 ms fade; allowing for latency and the 2x quality
//    filters, so "the same" means 60 dB down or better). Bypass off -> the effect is back.
//    Toggled several times, plus a "host stopped processing while bypassed" gap like Bitwig's.
//
// Build: gcc -O1 -I../dpf/distrho/src bypasstest.c -o bypasstest -ldl -lm
#include "clap/entry.h"
#include "clap/plugin-factory.h"
#include "clap/ext/params.h"
#include "clap/events.h"
#include "clap/process.h"
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const void* get_ext(const clap_host_t* h, const char* id) { (void)h; (void)id; return NULL; }
static void req(const clap_host_t* h) { (void)h; }
static const clap_host_t host = { CLAP_VERSION, NULL, "bypasstest", "slapworx", "", "1", get_ext, req, req, req };

// input events: at most one param change per block
static clap_event_param_value_t pending; static int havePending = 0;
static uint32_t in_size(const clap_input_events_t* l) { (void)l; return havePending ? 1 : 0; }
static const clap_event_header_t* in_get(const clap_input_events_t* l, uint32_t i) { (void)l; (void)i; return &pending.header; }
static const clap_input_events_t inEv = { NULL, in_size, in_get };
static bool out_push(const clap_output_events_t* l, const clap_event_header_t* e) { (void)l; (void)e; return true; }
static const clap_output_events_t outEv = { NULL, out_push };

#define N 512
static float inL[N], inR[N], outL[N], outR[N];
static int fails = 0;
static void check(int ok, const char* what) { printf("%s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++fails; }

int main(int argc, char** argv) {
    if (argc != 2) { fprintf(stderr, "usage: bypasstest <Mercy.clap>\n"); return 2; }
    void* lib = dlopen(argv[1], RTLD_NOW);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 2; }
    const clap_plugin_entry_t* entry = (const clap_plugin_entry_t*)dlsym(lib, "clap_entry");
    entry->init(argv[1]);
    const clap_plugin_factory_t* fac = (const clap_plugin_factory_t*)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    const clap_plugin_descriptor_t* d = fac->get_plugin_descriptor(fac, 0);
    const clap_plugin_t* p = fac->create_plugin(fac, &host, d->id);
    p->init(p);
    const clap_plugin_params_t* params = (const clap_plugin_params_t*)p->get_extension(p, CLAP_EXT_PARAMS);

    // 1. find "Bypass" and check its flags
    clap_id byp = CLAP_INVALID_ID; clap_param_info_t info; int nBypassFlag = 0;
    for (uint32_t i = 0; i < params->count(p); ++i) {
        params->get_info(p, i, &info);
        if (info.flags & CLAP_PARAM_IS_BYPASS) ++nBypassFlag;
        if (strcmp(info.name, "Bypass") == 0) byp = info.id;
    }
    check(byp != CLAP_INVALID_ID, "plugin has a \"Bypass\" parameter");
    check(nBypassFlag == 0, "no parameter is flagged as the host's bypass (CLAP_PARAM_IS_BYPASS)");

    // 2. audio: a loud, punchy test signal so the default preset clearly changes it
    p->activate(p, 44100.0, N, N);
    p->start_processing(p);
    const float* ins[2] = { inL, inR }; float* outs[2] = { outL, outR };
    clap_audio_buffer_t ib = { (float**)ins, NULL, 2, 0, 0 }, ob = { outs, NULL, 2, 0, 0 };
    clap_process_t pr; memset(&pr, 0, sizeof pr);
    pr.frames_count = N; pr.audio_inputs = &ib; pr.audio_outputs = &ob;
    pr.audio_inputs_count = 1; pr.audio_outputs_count = 1; pr.in_events = &inEv; pr.out_events = &outEv;
    long t = 0;
    // returns the RMS difference (dB, relative to the input) between output and input over one block
    double block(double bypassValue) {
        if (bypassValue >= 0) {
            memset(&pending, 0, sizeof pending);
            pending.header.size = sizeof pending; pending.header.type = CLAP_EVENT_PARAM_VALUE;
            pending.param_id = byp; pending.note_id = -1; pending.port_index = -1; pending.channel = -1; pending.key = -1;
            pending.value = bypassValue; havePending = 1;
        }
        for (int i = 0; i < N; ++i, ++t) {
            const double ph = fmod((double)t, 11025.0);           // a "drum" hit every quarter second
            const float v = (float)(0.8 * exp(-ph / 900.0) * sin(2 * M_PI * 70 * ph / 44100.0) + 0.05 * sin(t * 0.37));
            inL[i] = inR[i] = v;
        }
        pr.steady_time = t - N;
        p->process(p, &pr);
        havePending = 0;
        // keep the last 2 blocks of input so the comparison can allow for Mercy's latency
        // (2x quality oversampling delays the whole signal, dry path included, by a few samples)
        static float hist[2 * N];
        memmove(hist, hist + N, N * sizeof(float)); memcpy(hist + N, inL, N * sizeof(float));
        double best = 1e9;
        for (int lag = 0; lag < N; ++lag) {
            double e = 0, s = 0;
            for (int i = 0; i < N; ++i) { const float x = hist[N + i - lag]; e += (outL[i] - x) * (outL[i] - x); s += x * x; }
            const double db = 10 * log10((e + 1e-20) / (s + 1e-20));
            if (db < best) best = db;
        }
        return best;
    }
    double settle(double v) { double r = block(v); for (int k = 0; k < 40; ++k) r = block(-1); return r; }

    const double on0 = settle(0);
    check(on0 > -30, "effect on: output differs from the input");
    for (int round = 1; round <= 3; ++round) {
        char msg[128];
        const double b = settle(1);
        snprintf(msg, sizeof msg, "round %d, bypass on: output equals the input (diff %.1f dB)", round, b);
        check(b < -60, msg);
        if (round == 2) {   // Bitwig-like: the host stops calling process for a while, then resumes
            p->stop_processing(p); p->start_processing(p);
        }
        const double a = settle(0);
        snprintf(msg, sizeof msg, "round %d, bypass off: the effect is back (diff %.1f dB, was %.1f)", round, a, on0);
        check(a > -30, msg);
    }
    p->stop_processing(p); p->deactivate(p); p->destroy(p); entry->deinit(); dlclose(lib);
    printf(fails ? "\n%d FAILED\n" : "\nALL PASS\n", fails);
    return fails ? 1 : 0;
}
