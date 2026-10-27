#include "../src/MercyDSP.hpp"
#include <cstdio>
#include <vector>
#include <cstdlib>
using namespace mercy;
int main(int argc, char** argv) {
    // usage: trtest in.f32 out.f32 env [sr]
    FILE* f = fopen(argv[1], "rb"); std::vector<float> x; float v;
    while (fread(&v, 4, 1, f) == 1) x.push_back(v); fclose(f);
    const size_t n = x.size() / 2; const float env = atof(argv[3]); const double sr = argc > 4 ? atof(argv[4]) : 44100;
    std::vector<float> L(n), R(n), oL(n), oR(n);
    for (size_t i = 0; i < n; ++i) { L[i] = x[2 * i]; R[i] = x[2 * i + 1]; }
    Engine e; e.setSampleRate(sr);
    for (uint32_t p : {pSmpOn, pPreOn, pSqOn, pToneOn, pClipOn, pGwOn}) e.setParam(p, 0);
    e.setParam(pAutoGain, 0); e.setParam(pTrEnv, env);
    for (size_t i = 0; i < n; i += 512) { size_t m = std::min<size_t>(512, n - i); e.process(&L[i], &R[i], &oL[i], &oR[i], (uint32_t)m); }
    fprintf(stderr, "latency %u\n", e.latencySamples());
    FILE* o = fopen(argv[2], "wb");
    for (size_t i = 0; i < n; ++i) { fwrite(&oL[i], 4, 1, o); fwrite(&oR[i], 4, 1, o); }
    fclose(o);
}
