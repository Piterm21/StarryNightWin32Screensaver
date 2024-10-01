#pragma once

#define WIN32_LEAN_AND_MEAN 1
#include "Windows.h"
#undef WIN32_LEAN_AND_MEAN
#include <stdint.h>

class FrameTimer
{
public:
    FrameTimer(float InTargetSecondsPerFrame);
    void WaitUntilFrametime();

    float CurrentFrameTime = 0.0f;

private:
    LARGE_INTEGER LastCounter;
    int64_t PerfCountFrequency;

    float TargetSecondsPerFrame;
};