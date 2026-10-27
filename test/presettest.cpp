#include "../src/MercyView.hpp"
#include <cstdio>
#include <vector>
#include <cmath>
using namespace mercy;
int main() {
    FILE* f = fopen("/tmp/dry.f32", "rb"); std::vector<float> x; float v;
    while (fread(&v, 4, 1, f) == 1) x.push_back(v); fclose(f);
    const size_t n = x.size() / 2; std::vector<float> L(n), R(n), oL(n), oR(n);
    for (size_t i = 0; i < n; ++i) { L[i] = x[2 * i]; R[i] = x[2 * i + 1]; }
    double inPow = 0; for (size_t i = 0; i < n; ++i) inPow += L[i] * L[i] + R[i] * R[i];
    for (int p = 0; p < kNumFactory; ++p) {
        const FactoryPreset& fp = kFactory[p];
        Engine e; e.setSampleRate(44100);
        e.setParam(pEra, fp.era);
        for (uint32_t k = 0; k < kNumAdvParams; ++k) e.setParam(kFirstAdvParam + k, kEraDefaults[fp.era][k]);
        e.setParam(pSquash, fp.squash); e.setParam(pDirt, fp.dirt); e.setParam(pAge, fp.age); e.setParam(pMix, fp.mix);
        for (int k = 0; k < fp.nov; ++k) e.setParam(fp.ov[k].p, fp.ov[k].v);
        for (size_t i = 0; i < n; i += 512) { size_t m = std::min<size_t>(512, n - i); e.process(&L[i], &R[i], &oL[i], &oR[i], (uint32_t)m); }
        double pw = 0; float pk = 0; bool bad = false;
        for (size_t i = 44100; i < n; ++i) { pw += oL[i] * oL[i] + oR[i] * oR[i]; pk = std::max(pk, std::max(std::fabs(oL[i]), std::fabs(oR[i]))); if (!std::isfinite(oL[i])) bad = true; }
        const double rmsDb = 10 * std::log10(pw / (2.0 * (n - 44100))), inDb = 10 * std::log10(inPow / (2.0 * n));
        printf("%02d %-16s loudness vs input %+5.1f dB  peak %+5.1f dBFS%s\n", p + 1, fp.name, rmsDb - inDb, 20 * std::log10(pk), bad ? "  NaN!" : "");
    }
}
