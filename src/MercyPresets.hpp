// MERCY - factory presets (pure data; shared by the interface and the cross-platform sound test)
#pragma once
#include "MercyDSP.hpp"

namespace mercy {

struct PresetOverride { uint32_t p; float v; };
struct FactoryPreset {
    const char* name; int era; float squash, dirt, age, mix;
    PresetOverride ov[16]; int nov;
};
// overrides are applied on top of the era's advanced defaults
static const FactoryPreset kFactory[] = {
    {"default break", 0, .5f, .5f, .3f, 1, {}, 0},
    {"dark fantasy", 0, .62f, .55f, .42f, 1, {{pToneLowAmt, 6}, {pToneDepth, 12}, {pPreDrive, 8}}, 3},
    {"throne room", 1, .5f, .5f, .3f, 1, {}, 0},
    {"paris", 1, .7f, .6f, .35f, 1, {{pSqRatio, 20}, {pClipAmount, .5f}, {pToneBiteAmt, 4}}, 3},
    {"yeezus raw", 2, .5f, .5f, .3f, 1, {}, 0},
    {"industrial", 2, .8f, .85f, .6f, 1, {{pGate, -30}}, 1},
    {"sp grit", 0, .4f, .45f, .75f, 1, {{pSmpMode, 0}, {pSmpRate, 26.04f}, {pSmpBits, 12}, {pSmpCrunch, .6f}}, 4},
    {"lofi 12-bit", 0, .5f, .55f, .9f, 1, {{pSmpMode, 0}, {pSmpBits, 12}, {pSmpRate, 26.04f}, {pToneDepth, 10}}, 4},
    {"glue", 0, .3f, .25f, .1f, .8f, {{pSqBlend, .4f}}, 1},
    {"tight break", 0, .5f, .5f, .3f, 1, {{pTrEnv, 50}}, 1},
    {"chopped room", 1, .55f, .5f, .3f, 1, {{pTrEnv, 30}, {pTrDetect, .6f}}, 2},
    {"snap", 0, .5f, .5f, .3f, 1, {{pShMode, 2}, {pShChar, 1}, {pShAttack, 45}, {pShSustain, -20}}, 4},
    {"fat room", 0, .45f, .45f, .3f, 1, {{pShMode, 2}, {pShChar, 2}, {pShSustain, 55}}, 3},
    {"soul sample", 0, .35f, .4f, .45f, 1, {{pToneLowAmt, 2}, {pSqAttack, 15}, {pSqRelease, 200}, {pMonoLows, 0}}, 4},
    // ---- 20 more, built around the era techniques
    {"fantasy break", 0, .62f, .55f, .4f, 1, {{pTrEnv, 75}, {pShMode, 2}, {pShAttack, 20}, {pToneLowAmt, 5}, {pToneDepth, 13}, {pClipAmount, .35f}, {pHpf, 30}}, 7},
    {"lush room", 0, .55f, .4f, .25f, .85f, {{pSqBlend, .45f}, {pSqRelease, 180}, {pToneLowAmt, 3}, {pToneDepth, 16}, {pWidth, 1.2f}}, 5},
    {"gold throne", 1, .65f, .6f, .25f, 1, {{pSqRatio, 12}, {pToneBiteAmt, 4}, {pClipMode, 1}, {pClipAmount, .5f}, {pShMode, 2}, {pShAttack, 25}}, 6},
    {"hotel mix", 1, .5f, .45f, .2f, 1, {{pPreFlavor, 0}, {pPreDrive, 6}, {pSqAttack, 5}, {pToneLowFreq, 1}, {pToneLowAmt, 3}, {pWidth, .9f}}, 6},
    {"stadium snare", 1, .7f, .5f, .2f, 1, {{pShMode, 1}, {pShAttack, 45}, {pShFocus, 30}, {pToneBiteFreq, 1}, {pToneBiteAmt, 3}, {pSqBlend, .7f}}, 6},
    {"black skin", 2, .75f, .8f, .45f, 1, {{pPreFlavor, 3}, {pPreDrive, 16}, {pClipMode, 1}, {pClipAmount, .7f}, {pGate, -32}, {pWidth, .5f}}, 6},
    {"industrial kick", 2, .7f, .9f, .35f, 1, {{pShMode, 0}, {pShAttack, 50}, {pShFocus, 50}, {pTrEnv, 45}, {pToneDepth, 8}, {pClipAmount, .75f}}, 6},
    {"blown 808 room", 2, .6f, .85f, .3f, 1, {{pPreFlavor, 1}, {pPreLowEmph, .8f}, {pPreDrive, 18}, {pToneLowAmt, 6}, {pTrEnv, 60}}, 5},
    {"sp-12 crunch", 0, .5f, .5f, .7f, 1, {{pSmpMode, 0}, {pSmpRate, 26.04f}, {pSmpBits, 12}, {pSmpCrunch, .7f}, {pSmpAA, 0}, {pToneDepth, 11}}, 6},
    {"asr soul chop", 0, .45f, .4f, .55f, 1, {{pSmpMode, 1}, {pSmpRate, 32}, {pSmpBits, 14}, {pSmpCrunch, .4f}, {pToneLowAmt, 3}, {pShSustain, 15}}, 6},
    {"mpc thump", 1, .55f, .5f, .45f, 1, {{pSmpMode, 2}, {pSmpRate, 40}, {pSmpBits, 16}, {pShMode, 0}, {pShAttack, 30}, {pToneLowFreq, 1}, {pToneLowAmt, 4}}, 7},
    {"tape glue", 0, .4f, .55f, .15f, .9f, {{pPreFlavor, 2}, {pPreDrive, 10}, {pPreColor, .7f}, {pSqRatio, 4}, {pSqAttack, 15}, {pSqBlend, .5f}}, 6},
    {"all buttons", 1, .9f, .5f, .2f, 1, {{pSqRatio, 20}, {pSqAttack, .3f}, {pSqRelease, 50}, {pSqBlend, .9f}, {pClipAmount, .4f}}, 5},
    {"parallel smash", 0, .85f, .45f, .2f, .6f, {{pSqRatio, 20}, {pSqRelease, 90}, {pSqBlend, 1}}, 3},
    {"dry and tight", 1, .5f, .45f, .2f, 1, {{pTrEnv, 40}, {pShMode, 2}, {pShSustain, -40}, {pGate, -45}, {pGateHold, 80}, {pWidth, .8f}}, 6},
    {"chopped hats", 0, .45f, .45f, .3f, 1, {{pTrEnv, 25}, {pTrDetect, .7f}, {pShMode, 2}, {pShAttack, 20}, {pToneBiteFreq, 1}, {pToneBiteAmt, 3}}, 6},
    {"dusty vinyl", 0, .4f, .35f, .65f, 1, {{pSmpMode, 1}, {pSmpRate, 22}, {pSmpBits, 12}, {pToneDepth, 7}, {pHpf, 60}}, 5},
    {"telephone break", 2, .6f, .7f, .5f, 1, {{pHpf, 400}, {pLpf, 3500}, {pPreDrive, 14}, {pClipAmount, .6f}}, 4},
    {"big room toms", 0, .6f, .5f, .25f, 1, {{pShMode, 2}, {pShChar, 2}, {pShSustain, 45}, {pToneLowAmt, 5}, {pSqRelease, 250}, {pWidth, 1.3f}}, 6},
    {"melodic sample", 0, .3f, .35f, .4f, .8f, {{pSqAttack, 20}, {pSqRelease, 220}, {pToneLowAmt, 1.5f}, {pMonoLows, 0}, {pPreDrive, 4}}, 5},
    // ---- new in 2.0 (shown first in the browser): "nothing" + presets built on verb, weight and thick
#define RV(m, d, pr, t, du, mx) {pRvOn, 1}, {pRvMode, m}, {pRvDecay, d}, {pRvPre, pr}, {pRvTone, t}, {pRvDuck, du}, {pRvMix, mx}
#define WT(c, f, a, fl) {pWtOn, 1}, {pWtCenter, c}, {pWtFreq, f}, {pWtAmount, a}, {pWtFlat, fl}
    // every module off: nulls bit for bit against bypass; build a sound from scratch
    {"nothing", 0, .5f, .5f, .3f, 1, {{pSmpOn, 0}, {pPreOn, 0}, {pSqOn, 0}, {pToneOn, 0}, {pClipOn, 0}, {pGwOn, 0}, {pTrOn, 0}, {pShOn, 0}}, 8},
    {"plate snare", 0, .55f, .5f, .3f, 1, {RV(0, 1.6f, 20, .65f, .4f, .28f)}, 7},
    {"dark plate", 0, .6f, .55f, .4f, 1, {RV(0, 2.6f, 15, .3f, .45f, .3f), WT(0, 4, 3, 0), {pToneDepth, 12}}, 13},
    {"big fantasy", 0, .62f, .55f, .35f, 1, {RV(0, 3.2f, 30, .55f, .5f, .32f), WT(0, 4, 5, 0), {pPreThick, .4f}}, 13},
    {"wide plate break", 0, .5f, .5f, .3f, 1, {RV(0, 2.0f, 20, .6f, .35f, .26f), {pWidth, 1.25f}}, 8},
    {"thick break", 0, .55f, .7f, .35f, 1, {{pPreThick, .6f}, {pPreDrive, 12}, WT(0, 5, 3, 0)}, 7},
    {"sub bloom", 0, .45f, .45f, .3f, 1, {WT(0, 2, 5, 1), {pPreThick, .5f}}, 6},
    {"throne kit room", 1, .55f, .5f, .3f, 1, {RV(1, .8f, 5, .55f, .3f, .25f), WT(1, 2, 3, 0)}, 12},
    {"soul room", 1, .4f, .45f, .5f, 1, {RV(1, 1.2f, 8, .45f, .35f, .3f), {pSmpMode, 1}, {pPreThick, .3f}}, 9},
    {"tight room", 1, .5f, .45f, .25f, 1, {RV(1, .4f, 0, .5f, .2f, .2f), {pTrEnv, 60}}, 8},
    {"gold plate", 1, .65f, .6f, .25f, 1, {RV(0, 1.2f, 10, .7f, .45f, .2f), WT(1, 3, 3, 0), {pClipAmount, .5f}, {pPreThick, .3f}}, 14},
    {"kick body", 1, .55f, .5f, .3f, 1, {WT(1, 2, 4, 0), {pShMode, 0}, {pShAttack, 25}}, 7},
    {"gated snare", 2, .6f, .6f, .3f, 1, {RV(2, .6f, 0, .5f, 0, .35f)}, 7},
    {"industrial gate", 2, .8f, .85f, .5f, 1, {RV(2, 1.1f, 0, .35f, 0, .45f), WT(0, 3, 6, 0), {pGate, -30}}, 13},
    {"gated toms", 2, .6f, .55f, .25f, 1, {RV(2, 1.6f, 5, .6f, .1f, .4f), {pShMode, 2}, {pShChar, 2}, {pShSustain, 40}}, 10},
    {"808 weight", 2, .55f, .6f, .3f, 1, {WT(0, 3, 7, 0)}, 5},
    {"crushed but heavy", 2, .7f, .9f, .4f, 1, {{pPreThick, .8f}, {pPreDrive, 18}, WT(0, 4, 4, 0)}, 7},
#undef RV
#undef WT
};
static constexpr int kNumFactoryV1 = 34;   // the 1.x presets keep their places; 2.0's come after (and show first)
static constexpr int kNumFactory = sizeof(kFactory) / sizeof(kFactory[0]);

} // namespace mercy
