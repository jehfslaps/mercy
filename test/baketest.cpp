#include "../src/MercyBake.hpp"
#include <cstdio>
#include <chrono>
#include <cmath>
using namespace mercy;
static double snrDb(const std::vector<float>& a, const std::vector<float>& b, size_t from, size_t to) {
    double s = 0, e = 0; for (size_t i = from; i < to; ++i) { s += (double)b[i] * b[i]; e += (double)(a[i] - b[i]) * (a[i] - b[i]); }
    return 10 * std::log10(s / std::max(e, 1e-30));
}
int main(int argc, char** argv) {
    std::vector<float> L, R; double sr; std::string err;
    if (!loadAudioFile(argv[1], L, R, sr, err)) { printf("load failed: %s\n", err.c_str()); return 1; }
    const size_t n = L.size();
    printf("loaded %s: %zu frames @ %.0f Hz (%.2f s)\n", fileName(argv[1]).c_str(), n, sr, n / sr);
    BakeSettings s;
    for (uint32_t p = 0; p < kFirstOutputParam; ++p) s.v[p] = kParamInfo[p].def;
    s.v[pTrEnv] = 50; s.v[pShAttack] = 30; s.v[pSquash] = 0.65f; // something with memory (compressor, transient, auto gain)
    for (int i = 0; i < kNumStages; ++i) s.order[i] = kDefaultOrder[i];
    s.presetName = "test";
    std::vector<float> bL, bR;
    auto t0 = std::chrono::high_resolution_clock::now();
    renderBake(s, L, R, sr, bL, bR, nullptr, nullptr);
    auto t1 = std::chrono::high_resolution_clock::now();
    printf("bake time %.2f s for %.2f s of audio\n", std::chrono::duration<double>(t1 - t0).count(), n / sr);
    // reference: live engine playing the loop 4 times in a row; take the 3rd repetition, latency-compensated
    Engine e; e.setSampleRate(sr);
    for (uint32_t p = 0; p < kFirstOutputParam; ++p) e.setParam(p, s.v[p]);
    e.setParam(pQuality, 1);
    const size_t lat = e.latencySamples(), reps = 6;
    std::vector<float> rl(n * reps), rr(n * reps), ol(n * reps), orr(n * reps);
    for (size_t k = 0; k < reps; ++k) for (size_t i = 0; i < n; ++i) { rl[k * n + i] = L[i]; rr[k * n + i] = R[i]; }
    for (size_t i = 0; i < n * reps; i += 512) { size_t m = std::min<size_t>(512, n * reps - i); e.process(&rl[i], &rr[i], &ol[i], &orr[i], (uint32_t)m); }
    std::vector<float> refL(n), refR(n);
    for (size_t i = 0; i < n; ++i) { refL[i] = ol[4 * n + i + lat]; refR[i] = orr[4 * n + i + lat]; }
    printf("bake vs live loop (5th pass): whole file %.1f dB SNR, first 100 ms %.1f dB, last 100 ms %.1f dB\n",
           snrDb(bL, refL, 0, n), snrDb(bL, refL, 0, (size_t)(0.1 * sr)), snrDb(bL, refL, n - (size_t)(0.1 * sr), n));
    // seamlessness: jump across the loop point (end -> start) vs the same jump inside the live stream
    double jb = std::fabs(bL[0] - bL[n - 1]), jr = std::fabs(ol[4 * n + lat] - ol[4 * n + lat - 1]);
    printf("loop point step: bake %.4f vs live stream %.4f\n", jb, jr);
    std::string out = std::string("/tmp/bake_out.wav");
    printf("write 24-bit: %s\n", writeWavFile(out, bL, bR, sr, false, err) ? "ok" : err.c_str());
    std::vector<float> cL, cR; double csr;
    loadAudioFile(out, cL, cR, csr, err);
    printf("re-read: %zu frames, max diff vs float %.2e (24-bit quantization)\n", cL.size(), [&]{ double m=0; for(size_t i=0;i<n;i++) m=std::max(m,(double)std::fabs(cL[i]-std::max(-1.f,std::min(1.f,bL[i])))); return m; }());
}
