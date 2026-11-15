// Mercy - website demo renders.
//
//   erademo <in.wav> <outdir> <stem> [fantasyPreset thronePreset yeezusPreset]
//
// Writes <stem>_fantasy.wav, <stem>_throne.wav and <stem>_yeezus.wav (32-bit float): the loop baked
// through Mercy with a factory preset loaded, exactly as factorySnapshot() in MercyView.hpp loads it,
// with auto gain on and 2x quality. Default presets (the "eras" group): 14 fantasy break,
// 16 gold throne, 4 yeezus raw. renderBake keeps the loop seamless and the exact input length,
// so all versions line up with the dry loop and can be switched while playing.
#include "MercyDSP.hpp"
#include "MercyPresets.hpp"
#include "MercyBake.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace mercy;

int main(int argc, char** argv) {
    if (argc != 4 && argc != 7) { std::fprintf(stderr, "usage: erademo <in.wav> <outdir> <stem> [fantasy throne yeezus preset numbers]\n"); return 1; }
    int pre[3] = {14, 16, 4};
    if (argc == 7) for (int e = 0; e < 3; ++e) pre[e] = std::atoi(argv[4 + e]);
    std::vector<float> L, R; double sr = 0; std::string err;
    if (!loadAudioFile(argv[1], L, R, sr, err)) { std::fprintf(stderr, "can't read %s: %s\n", argv[1], err.c_str()); return 2; }
    static const char* kNames[3] = {"fantasy", "throne", "yeezus"};
    for (int e = 0; e < 3; ++e) {
        if (pre[e] < 0 || pre[e] >= kNumFactory) return 1;
        const FactoryPreset& f = kFactory[pre[e]];
        BakeSettings b;
        for (uint32_t p = 0; p < kFirstOutputParam; ++p) b.v[p] = kParamInfo[p].def;
        b.v[pEra] = (float)f.era;
        for (uint32_t k = 0; k < kNumAdvParams; ++k) b.v[kFirstAdvParam + k] = kEraDefaults[f.era][k];
        b.v[pSquash] = f.squash; b.v[pDirt] = f.dirt; b.v[pAge] = f.age; b.v[pMix] = f.mix;
        b.v[pAutoGain] = 1; b.v[pQuality] = 1;
        for (int k = 0; k < f.nov; ++k) b.v[f.ov[k].p] = f.ov[k].v;
        if (f.order == nullptr || !normalizeOrder(f.order, b.order))
            for (int k = 0; k < kNumStages; ++k) b.order[k] = kDefaultOrder[k];
        std::vector<float> oL, oR;
        if (!renderBake(b, L, R, sr, oL, oR, nullptr, nullptr)) return 3;
        const std::string out = std::string(argv[2]) + "/" + argv[3] + "_" + kNames[e] + ".wav";
        if (!writeWavFile(out, oL, oR, sr, true, err)) { std::fprintf(stderr, "can't write %s: %s\n", out.c_str(), err.c_str()); return 4; }
        float pk = 0; for (size_t i = 0; i < oL.size(); ++i) pk = std::max(pk, std::max(std::fabs(oL[i]), std::fabs(oR[i])));
        std::printf("%-8s %-14s %zu samples (in %zu)  peak %.1f dBFS\n", kNames[e], f.name, oL.size(), L.size(), 20 * std::log10(pk + 1e-12));
    }
    return 0;
}
