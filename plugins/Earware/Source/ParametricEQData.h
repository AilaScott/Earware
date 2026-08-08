#pragma once

#include <cstdint>

enum EarwareFilterType : uint8_t {
    EarwareFilterPK   = 0,
    EarwareFilterLSC  = 1,
    EarwareFilterHSC  = 2,
};

struct EarwareFilterStage {
    EarwareFilterType type;
    float freq;
    float gain;
    float q;
};

struct EarwareEqPreset {
    float preampGain;
    EarwareFilterStage filters[10];
};

extern const int earwareNumModels;
extern const int earwareOverEarStart;
extern const int earwareInEarStart;
extern const int earwareEarbudStart;

const EarwareEqPreset* earwareGetPreset(int index);
const char* earwareGetModelName(int index);
int earwareFindModel(const char* name);
void earwareComputeCurve(int index, float* freqsOut, float* eqOut, int numPoints);
