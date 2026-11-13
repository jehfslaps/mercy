// Host echo test: a DAW hands parameter values back with VST3-style rounding (0..1 float). The view must keep
// its exact values (no false "edited", bakes use the preset as saved) yet still take real changes.
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <algorithm>
#define private public
#include "../src/MercyView.hpp"
#undef private
#include <cstdio>
#include <vector>
#include <utility>
using namespace mercy;
struct Host : ViewHost {
    std::vector<std::pair<uint32_t, float>> sent;
    void hostBeginEdit(uint32_t) override {} void hostEndEdit(uint32_t) override {}
    void hostSetParam(uint32_t p, float v) override { sent.push_back({p, v}); }
    void hostSetOrder(const char*) override {} void hostRepaint() override {} void hostResize(unsigned, unsigned) override {}
};
static float vst3Round(uint32_t p, float v) {   // plain -> float32 normalized -> plain, as a VST3 host round trip
    const ParamInfo& pi = kParamInfo[p];
    const float n = (float)((double)(v - pi.min) / (double)(pi.max - pi.min));
    float back = pi.min + n * (pi.max - pi.min);
    return std::nextafter(back, back + 1.f);   // and one more ulp off, to be sure it differs
}
int main() {
    Host h; MercyView view(&h);
    int fails = 0;
    for (int i = 0; i < kNumFactory; ++i) {
        h.sent.clear();
        view.S.userTouched = true;
        view.loadPreset(i);
        const Snapshot want = view.factorySnapshot(i);
        for (auto& e : h.sent) view.paramChanged(e.first, vst3Round(e.first, e.second));   // the late echo
        bool exact = true;
        for (uint32_t p = 0; p < kFirstOutputParam; ++p)
            if (p != pAutoGain && p != pQuality && view.S.values[p] != want.v[p]) exact = false;
        if (view.S.presetModified || !exact) { ++fails; printf("FAIL %s: modified=%d exact=%d\n", kFactory[i].name, view.S.presetModified, exact); }
    }
    // a real change from the host (automation) must still come through and mark the preset as edited
    view.loadPreset(0);
    const float before = view.S.values[pSquash];
    view.paramChanged(pSquash, before + 0.2f);
    if (std::fabs(view.S.values[pSquash] - (before + 0.2f)) > 1e-6f || !view.S.presetModified) { ++fails; printf("FAIL automation not taken\n"); }
    // and after a real change, a small one right next to it is taken too (no stale echo filter)
    view.paramChanged(pSquash, before + 0.20001f);
    if (view.S.values[pSquash] != before + 0.20001f) { ++fails; printf("FAIL small follow-up change ignored\n"); }
    printf("%d presets checked, %d failures\n", kNumFactory, fails);
    return fails ? 1 : 0;
}
