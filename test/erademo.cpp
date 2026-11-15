// Mercy - website demo renders.
//
//   erademo <in.wav> <outdir> <stem>
//
// Writes <stem>_fantasy.wav, <stem>_throne.wav and <stem>_yeezus.wav (32-bit float): the loop baked
// through a fresh Mercy with just the era button clicked, exactly what selectEra() does in the plugin
// (era voicing + that era's advanced page; verb stays off), squash/dirt/age left where a new instance
// has them, auto gain on, 2x quality. renderBake keeps the loop seamless and the exact input length,
// so all versions line up sample for sample with the dry loop and can be switched while playing.
#include "MercyDSP.hpp"
#include "MercyPresets.hpp"
#include "MercyBake.hpp"
#include <cstdio>
#include <string>
#include <vector>

using namespace mercy;

int main(int argc, char** argv) {
    if (argc != 4) { std::fprintf(stderr, "usage: erademo <in.wav> <outdir> <stem>\n"); return 1; }
    std::vector<float> L, R; double sr = 0; std::string err;
    if (!loadAudioFile(argv[1], L, R, sr, err)) { std::fprintf(stderr, "can't read %s: %s\n", argv[1], err.c_str()); return 2; }
    static const char* kNames[3] = {"fantasy", "throne", "yeezus"};
    for (int e = 0; e < 3; ++e) {
        BakeSettings b;
        for (uint32_t p = 0; p < kFirstOutputParam; ++p) b.v[p] = kParamInfo[p].def;
        b.v[pEra] = (float)e;
        for (uint32_t i = 0; i < kNumAdvParams; ++i) b.v[kFirstAdvParam + i] = kEraDefaults[e][i];
        for (uint32_t i = 0; i < kNumNewParams; ++i)
            if (kFirstNewParam + i != pRvOn) b.v[kFirstNewParam + i] = kEraNew[e][i];
        b.v[pAutoGain] = 1; b.v[pQuality] = 1; b.v[pBypass] = 0;
        if (!normalizeOrder("67012345", b.order)) return 3;
        std::vector<float> oL, oR;
        if (!renderBake(b, L, R, sr, oL, oR, nullptr, nullptr)) return 3;
        const std::string out = std::string(argv[2]) + "/" + argv[3] + "_" + kNames[e] + ".wav";
        if (!writeWavFile(out, oL, oR, sr, true, err)) { std::fprintf(stderr, "can't write %s: %s\n", out.c_str(), err.c_str()); return 4; }
        float pk = 0; for (size_t i = 0; i < oL.size(); ++i) pk = std::max(pk, std::max(std::fabs(oL[i]), std::fabs(oR[i])));
        std::printf("%-8s %zu samples (in %zu)  peak %.1f dBFS\n", kNames[e], oL.size(), L.size(), 20 * std::log10(pk + 1e-12));
    }
    return 0;
}
