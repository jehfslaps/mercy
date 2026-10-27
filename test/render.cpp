// MERCY - cross-platform sound check.
//
//   render <in.wav> <outdir>        render every test scenario to <outdir>/<name>.f32 (+ list.txt)
//   render --compare <dirA> <dirB>  compare two render folders; exit 1 if any scenario differs
//
// It is built with exactly the same compiler flags as the plugin (make render in src/), so the
// Windows build and the Mac build each render the same loop through the same settings and the
// results are compared sample by sample. The goal is bit-identical; the report shows the largest
// difference in dB so anything that isn't identical can be judged.
#include "MercyDSP.hpp"
#include "MercyPresets.hpp"
#include "MercyBake.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <functional>
#include <cctype>

using namespace mercy;

struct Scenario {
    std::string name;
    double sr = 44100;
    uint32_t block = 512;
    std::vector<float> v;  // kFirstOutputParam values
    std::string order = "67012345";
    // optional automation: called before every block with the block's start time in seconds
    std::function<void(Engine&, double)> automate;
    bool bake = false;
};

static std::vector<float> defaults() {
    std::vector<float> v(kFirstOutputParam);
    for (uint32_t p = 0; p < kFirstOutputParam; ++p) v[p] = kParamInfo[p].def;
    return v;
}
static std::vector<float> factory(int i, float quality) {
    const FactoryPreset& f = kFactory[i];
    std::vector<float> v = defaults();
    v[pEra] = (float)f.era;
    for (uint32_t k = 0; k < kNumAdvParams; ++k) v[kFirstAdvParam + k] = kEraDefaults[f.era][k];
    v[pSquash] = f.squash; v[pDirt] = f.dirt; v[pAge] = f.age; v[pMix] = f.mix;
    v[pQuality] = quality;
    for (int k = 0; k < f.nov; ++k) v[f.ov[k].p] = f.ov[k].v;
    return v;
}
// the user's own presets (the three era demos from the ad), as saved in user_presets.txt
static std::vector<float> fromLine(const char* vals) {
    std::vector<float> v = defaults();
    const char* tok = vals; uint32_t p = 0;
    while (tok && *tok && p < kFirstOutputParam) {
        v[p] = clampf((float)std::strtod(tok, nullptr), kParamInfo[p].min, kParamInfo[p].max);
        ++p; tok = std::strchr(tok, ','); if (tok) ++tok;
    }
    return v;
}
static const char* kFantasyDemo = "0,0.5,0.5,0.3,0,1,0,1,0,1,1,1,32,14,0.3,0,1,1,6,0.5,0.5,1,-22,6,8,120,0.55,0,1,0,4,0,1.5,15,1,0,0.3,-0.3,1,-80,60,1,1,1,100,0.5,1,2,1,0,0,0,20,20000";
static const char* kThroneDemo  = "1,0.74375,0.775001,0.79375,0,1,0,1,0,1,1,2,40,14,0.2,0,1,0,8,0.35,0.4,1,-24,12,3,70,0.65,0,1,1,3,0,3,18,1,1,0.4,-0.3,1,-80,60,0.9,1,1,100,0.5,1,2,1,0,0,0,20,20000";
static const char* kYeezusDemo  = "2,0.65,0.725002,0.599999,0,1,0,1,0,1,1,0,22,10,0.6,0,1,3,14,0.6,0.7,1,-30,20,1,50,0.8,0,1,0,5,0,2,9,1,1,0.6,-1,1,-35,40,0.6,1,1,36.6667,0.5,1,2,1,0,0,0,20,20000";

static std::string slug(const std::string& s) {
    std::string o;
    for (char c : s) o += (std::isalnum((unsigned char)c) ? c : '_');
    return o;
}

static std::vector<Scenario> scenarios() {
    std::vector<Scenario> out;
    for (int i = 0; i < kNumFactory; ++i) {
        Scenario s; s.name = "f" + std::to_string(i) + "_" + slug(kFactory[i].name) + "_2x"; s.v = factory(i, 1); out.push_back(s);
        s.name = "f" + std::to_string(i) + "_" + slug(kFactory[i].name) + "_1x"; s.v = factory(i, 0); out.push_back(s);
    }
    { Scenario s; s.name = "demo_fantasy"; s.v = fromLine(kFantasyDemo); out.push_back(s); }
    { Scenario s; s.name = "demo_throne"; s.v = fromLine(kThroneDemo); out.push_back(s); }
    { Scenario s; s.name = "demo_yeezus"; s.v = fromLine(kYeezusDemo); out.push_back(s); }
    // the ad's transient move: yeezus demo at 100, dragged down to 20 over one second, while playing
    { Scenario s; s.name = "auto_transient_sweep"; s.v = fromLine(kYeezusDemo); s.v[pTrEnv] = 100;
      s.automate = [](Engine& e, double t) { e.setParam(pTrEnv, (float)(t < 1 ? 100 : t > 2 ? 20 : 100 - 80 * (t - 1))); };
      out.push_back(s); }
    // a fader ride (squash) and a mix ride
    { Scenario s; s.name = "auto_squash_mix"; s.v = factory(4, 1);
      s.automate = [](Engine& e, double t) { e.setParam(pSquash, (float)(0.5 + 0.5 * std::sin(t * 2.1))); e.setParam(pMix, (float)(0.6 + 0.4 * std::cos(t * 1.3))); };
      out.push_back(s); }
    // other sample rates and host block sizes
    for (double sr : {48000.0, 88200.0, 96000.0}) {
        Scenario s; s.name = "sr" + std::to_string((int)sr) + "_yeezus"; s.sr = sr; s.v = fromLine(kYeezusDemo); out.push_back(s);
    }
    for (uint32_t b : {32u, 333u, 1024u, 4096u}) {
        Scenario s; s.name = "block" + std::to_string(b) + "_throne"; s.block = b; s.v = fromLine(kThroneDemo); out.push_back(s);
    }
    // chain order, eq post, input/output/hpf/lpf, bypass
    { Scenario s; s.name = "order_reversed"; s.v = factory(5, 1); s.order = "54321076"; out.push_back(s); }
    { Scenario s; s.name = "io_filters"; s.v = factory(1, 1); s.v[pInput] = 9; s.v[pOutput] = -4; s.v[pHpf] = 120; s.v[pLpf] = 6000; s.v[pAutoGain] = 0; out.push_back(s); }
    { Scenario s; s.name = "bypass"; s.v = factory(2, 1); s.v[pBypass] = 1; out.push_back(s); }
    // bake (offline render, seamless loop) with the yeezus demo
    { Scenario s; s.name = "bake_yeezus"; s.v = fromLine(kYeezusDemo); s.bake = true; out.push_back(s); }
    { Scenario s; s.name = "bake_sp_grit"; s.v = factory(6, 1); s.bake = true; out.push_back(s); }
    return out;
}

static bool writeF32(const std::string& path, const std::vector<float>& L, const std::vector<float>& R) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::vector<float> inter(L.size() * 2);
    for (size_t i = 0; i < L.size(); ++i) { inter[2 * i] = L[i]; inter[2 * i + 1] = R[i]; }
    const bool ok = std::fwrite(inter.data(), 4, inter.size(), f) == inter.size();
    std::fclose(f);
    return ok;
}
static bool readF32(const std::string& path, std::vector<float>& x) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END); const long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
    x.resize((size_t)n / 4);
    const bool ok = std::fread(x.data(), 4, x.size(), f) == x.size();
    std::fclose(f);
    return ok;
}

static int render(const char* inPath, const std::string& dir) {
    std::vector<float> L, R; double fileSr = 0; std::string err;
    if (!loadAudioFile(inPath, L, R, fileSr, err)) { std::fprintf(stderr, "can't read %s: %s\n", inPath, err.c_str()); return 2; }
    // the loop twice in a row, so stages that build up over time (auto gain, compressor) are exercised
    std::vector<float> inL(L), inR(R);
    inL.insert(inL.end(), L.begin(), L.end()); inR.insert(inR.end(), R.begin(), R.end());
    FILE* list = std::fopen((dir + "/list.txt").c_str(), "w");
    if (!list) { std::fprintf(stderr, "can't write to %s\n", dir.c_str()); return 2; }
    int n = 0;
    for (const Scenario& s : scenarios()) {
        std::vector<float> oL, oR;
        if (s.bake) {
            BakeSettings b;
            for (uint32_t p = 0; p < kFirstOutputParam; ++p) b.v[p] = s.v[p];
            if (!normalizeOrder(s.order.c_str(), b.order)) return 3;
            if (!renderBake(b, L, R, fileSr, oL, oR, nullptr, nullptr)) return 3;
        } else {
            Engine e;
            for (uint32_t p = 0; p < kFirstOutputParam; ++p) e.setParam(p, s.v[p]);
            e.setOrderString(s.order.c_str());
            e.setSampleRate(s.sr);
            const size_t len = inL.size();
            oL.resize(len); oR.resize(len);
            for (size_t i = 0; i < len; i += s.block) {
                if (s.automate) s.automate(e, (double)i / s.sr);
                const uint32_t m = (uint32_t)std::min<size_t>(s.block, len - i);
                e.process(&inL[i], &inR[i], &oL[i], &oR[i], m);
            }
        }
        writeF32(dir + "/" + s.name + ".f32", oL, oR);
        std::fprintf(list, "%s\n", s.name.c_str());
        ++n;
    }
    std::fclose(list);
    std::printf("rendered %d scenarios to %s\n", n, dir.c_str());
    return 0;
}

static int compare(const std::string& a, const std::string& b) {
    FILE* list = std::fopen((a + "/list.txt").c_str(), "r");
    if (!list) { std::fprintf(stderr, "no list.txt in %s\n", a.c_str()); return 2; }
    char line[512];
    int total = 0, identical = 0, failed = 0;
    double worstDb = -400;
    while (std::fgets(line, sizeof line, list)) {
        line[std::strcspn(line, "\r\n")] = 0;
        if (!line[0]) continue;
        ++total;
        std::vector<float> x, y;
        if (!readF32(a + "/" + line + ".f32", x) || !readF32(b + "/" + line + ".f32", y) || x.size() != y.size()) {
            std::printf("  MISSING/LENGTH  %s\n", line); ++failed; continue;
        }
        double maxd = 0, peak = 0; size_t diffs = 0;
        for (size_t i = 0; i < x.size(); ++i) {
            const double d = std::fabs((double)x[i] - (double)y[i]);
            if (std::memcmp(&x[i], &y[i], 4) != 0) ++diffs;
            if (d > maxd) maxd = d;
            if (std::fabs(x[i]) > peak) peak = std::fabs(x[i]);
        }
        if (diffs == 0) { ++identical; std::printf("  identical       %s\n", line); continue; }
        const double db = 20 * std::log10(maxd + 1e-30);
        if (db > worstDb) worstDb = db;
        // -120 dBFS is far below anything audible and below 24-bit dither
        const bool ok = db < -120;
        if (!ok) ++failed;
        std::printf("  %s  %-28s max diff %7.1f dBFS (%zu of %zu samples differ)\n", ok ? "tiny diff " : "DIFFERENT ", line, db, diffs, x.size());
    }
    std::fclose(list);
    std::printf("\n%d scenarios: %d bit-identical, %d with tiny differences, %d failed", total, identical, total - identical - failed, failed);
    if (total > identical) std::printf(" (largest difference %.1f dBFS)", worstDb);
    std::printf("\n");
    return failed ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc == 4 && !std::strcmp(argv[1], "--compare")) return compare(argv[2], argv[3]);
    if (argc == 3) return render(argv[1], argv[2]);
    std::fprintf(stderr, "usage: render <in.wav> <outdir> | render --compare <dirA> <dirB>\n");
    return 2;
}
