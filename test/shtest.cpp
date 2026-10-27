#include ENGINE_HDR
#include <cstdio>
#include <vector>
#include <cstdlib>
#include <cstring>
using namespace mercy;
// usage: shtest in.f32 out.f32 [param=value ...]   (other stages default unless 'solo')
int main(int argc, char** argv) {
    FILE* f = fopen(argv[1], "rb"); std::vector<float> x; float v;
    while (fread(&v, 4, 1, f) == 1) x.push_back(v); fclose(f);
    const size_t n = x.size() / 2;
    std::vector<float> L(n), R(n), oL(n), oR(n);
    for (size_t i = 0; i < n; ++i) { L[i] = x[2 * i]; R[i] = x[2 * i + 1]; }
    Engine e; e.setSampleRate(44100);
    for (int a = 3; a < argc; ++a) {
        if (!strcmp(argv[a], "solo")) { for (uint32_t p : {pSmpOn, pPreOn, pSqOn, pToneOn, pClipOn, pGwOn}) e.setParam(p, 0); e.setParam(pAutoGain, 0); continue; }
        char key[64]; float val; if (sscanf(argv[a], "%63[^=]=%f", key, &val) != 2) continue;
        for (uint32_t p = 0; p < kNumParams; ++p) if (!strcmp(kParamInfo[p].symbol, key)) e.setParam(p, val);
    }
    for (size_t i = 0; i < n; i += 512) { size_t m = std::min<size_t>(512, n - i); e.process(&L[i], &R[i], &oL[i], &oR[i], (uint32_t)m); }
    FILE* o = fopen(argv[2], "wb");
    for (size_t i = 0; i < n; ++i) { fwrite(&oL[i], 4, 1, o); fwrite(&oR[i], 4, 1, o); }
    fclose(o);
}
