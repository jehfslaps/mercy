#include "DistrhoPlugin.hpp"
#include "MercyDSP.hpp"
#include "MercySession.hpp"

START_NAMESPACE_DISTRHO

using namespace mercy;

class MercyPlugin : public Plugin
{
public:
    MercyPlugin() : Plugin(kNumParams, 0, 2)
    {
        std::strcpy(fOrder, kDefaultOrderString);
        fEngine.setSampleRate(getSampleRate());
        fSession->sampleRate.store(getSampleRate());
        setLatency(fEngine.latencySamples());
    }

protected:
#ifdef MERCY_TEASER
    const char* getLabel() const override { return "000"; }
    const char* getDescription() const override { return "Drum processor."; }
    const char* getHomePage() const override { return "https://slapworx.com"; }
#else
    const char* getLabel() const override { return "Mercy"; }
    const char* getDescription() const override { return "Drum break compression, saturation and lo-fi grit."; }
    const char* getHomePage() const override { return "https://mercy.audio"; }
#endif
    const char* getMaker() const override { return "slapworx"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(2, 0, 0); }

    void initParameter(uint32_t index, Parameter& p) override
    {
        const ParamInfo& pi = kParamInfo[index];
        if (index == pBypass) {
            p.initDesignation(kParameterDesignationBypass);
            return;
        }
        p.name = pi.name;
        p.symbol = pi.symbol;
        p.unit = pi.unit;
        p.ranges.min = pi.min;
        p.ranges.max = pi.max;
        p.ranges.def = pi.def;
        p.hints = kParameterIsAutomatable;
        if (pi.isInt) p.hints |= kParameterIsInteger;
        if (pi.isBool) p.hints |= kParameterIsBoolean;
        if (index >= kFirstOutputParam) p.hints = kParameterIsOutput;
        if (index == pSqAttack || index == pSqRelease || index == pSmpRate || index == pToneDepth || index == pGateHold || index == pHpf || index == pLpf)
            p.hints |= kParameterIsLogarithmic;

        auto setEnum = [&](std::initializer_list<const char*> labels) {
            const uint32_t n = (uint32_t)labels.size();
            ParameterEnumerationValue* v = new ParameterEnumerationValue[n];
            uint32_t i = 0;
            for (const char* l : labels) { v[i].label = l; v[i].value = (float)i; ++i; }
            p.enumValues.count = n;
            p.enumValues.restrictedMode = true;
            p.enumValues.values = v;
        };
        switch (index) {
#ifdef MERCY_TEASER
        case pEra: setEnum({"01", "02", "03"}); break; // what a host shows in automation
#else
        case pEra: setEnum({"Fantasy", "Throne", "Yeezus"}); break;
#endif
        case pQuality: setEnum({"1x", "2x"}); break;
        case pSmpMode: setEnum({"SP-12", "ASR", "MPC"}); break;
        case pPreFlavor: setEnum({"Console", "Tube", "Tape", "Xfmr"}); break;
        case pToneLowFreq: setEnum({"60 Hz", "100 Hz"}); break;
        case pToneBiteFreq: setEnum({"3 kHz", "10 kHz"}); break;
        case pClipMode: setEnum({"Soft", "Hard"}); break;
        case pShMode: setEnum({"Kick", "Snare", "Bus"}); break;
        case pShChar: setEnum({"Classic", "Natural", "Smooth"}); break;
        default: break;
        }
    }

    void initState(uint32_t index, State& state) override
    {
        if (index == 0) {
            state.key = "order";
            state.defaultValue = kDefaultOrderString;
            state.label = "Chain Order";
            state.hints = kStateIsHostReadable;
        }
        else if (index == 1) {
            // interface state (page, bake panel, preset, A/B), saved with the project
            state.key = "ui";
            state.defaultValue = "";
            state.label = "Interface State";
            state.hints = 0;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        switch (index) {
        case pMeterIn: return fEngine.meterIn;
        case pMeterOut: return fEngine.meterOut;
        case pMeterGR: return fEngine.meterGR;
        case pWtOl: return fEngine.wtOverload;
        default: return fEngine.getParam(index);
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index < kFirstOutputParam) fEngine.setParam(index, value);
    }

    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, "ui") == 0) {
            fSession->savedUi = value != nullptr ? value : "";
            return;
        }
        if (std::strcmp(key, "order") == 0 && value != nullptr) {
            fEngine.setOrderString(value); // also accepts v1.0 six-stage orders
            for (int i = 0; i < kNumStages; ++i) fOrder[i] = (char)('0' + fEngine.order[i].load());
            fOrder[kNumStages] = 0;
        }
    }

    void activate() override
    {
        fEngine.setSampleRate(getSampleRate());
        fSession->scope.setSampleRate(getSampleRate());
        fSession->sampleRate.store(getSampleRate());
        setLatency(fEngine.latencySamples());
    }
    void sampleRateChanged(double sr) override
    {
        fEngine.setSampleRate(sr);
        fSession->scope.setSampleRate(sr);
        fSession->sampleRate.store(sr);
        setLatency(fEngine.latencySamples());
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        fSession->scope.scanInput(inputs[0], inputs[1], frames);
        fEngine.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
        fSession->scope.scanOutput(outputs[0], outputs[1], frames);
        if (fSession->capture.listening()) {   // costs nothing until the bake panel has been opened
            const TimePosition& tp = getTimePosition();
            CaptureTime ct;
            ct.playing = tp.playing; ct.valid = tp.bbt.valid;
            if (tp.bbt.valid) {
                ct.bpm = tp.bbt.beatsPerMinute; ct.beatsPerBar = tp.bbt.beatsPerBar;
                ct.posInBar = (tp.bbt.beat - 1) + (tp.bbt.ticksPerBeat > 0 ? tp.bbt.tick / tp.bbt.ticksPerBeat : 0.0);
            }
            fSession->capture.process(outputs[0], outputs[1], frames, ct, fEngine.latencySamples());
        }
    }

public:
    // interface session: survives the plugin window being closed (see MercySession.hpp)
    std::shared_ptr<mercy::Session> fSession = std::make_shared<mercy::Session>();

private:
    Engine fEngine;
    char fOrder[kNumStages + 2];

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MercyPlugin)
};

Plugin* createPlugin() { return new MercyPlugin(); }

END_NAMESPACE_DISTRHO

std::shared_ptr<mercy::Session> mercy::sessionFromPluginInstance(void* p)
{
    if (p == nullptr) return nullptr;
    return static_cast<DISTRHO::MercyPlugin*>(static_cast<DISTRHO::Plugin*>(p))->fSession;
}
