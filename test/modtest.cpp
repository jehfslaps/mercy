#define private public
#include "../src/MercyView.hpp"
#undef private
#include <cstdio>
using namespace mercy;
int main() {
    int total = 0, bad = 0;
    static const char* names[kNumStages] = {"sampler", "preamp", "squash", "tone", "clip", "gate/width", "transient", "shape", "reverb", "weight"};
    for (int st = 0; st < kNumStages; ++st) {
        const MercyView::ModPreset* mp; const int n = MercyView::modPresets(st, mp);
        const uint32_t* ps; const int np = MercyView::stageParams(st, ps);
        printf("%-10s %d presets:", names[st], n);
        for (int i = 1; i < n; ++i) {
            ++total; printf(" %s,", mp[i].name);
            for (int k = 0; k < np; ++k) {
                const ParamInfo& pi = kParamInfo[ps[k]];
                if (mp[i].v[k] < pi.min - 1e-4f || mp[i].v[k] > pi.max + 1e-4f) { ++bad; printf(" [OUT OF RANGE %s=%g]", pi.symbol, mp[i].v[k]); }
            }
        }
        printf("\n");
    }
    printf("%d module presets (+8 defaults), %d out-of-range values\n", total, bad);
}
