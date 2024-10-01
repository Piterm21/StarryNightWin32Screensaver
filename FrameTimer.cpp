#include "FrameTimer.h"

#include <timeapi.h>

FrameTimer::FrameTimer(float InTargetSecondsPerFrame)
{
    //Request for sleep to be accurate to 1 millisecond, it may fail, but given the context we don't want to spin lock even if sleep isn't accurate
    timeBeginPeriod(1);
    QueryPerformanceCounter(&LastCounter);

    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    TargetSecondsPerFrame = InTargetSecondsPerFrame;
}

void FrameTimer::WaitUntilFrametime()
{
    LARGE_INTEGER CurrentCounter;
    QueryPerformanceCounter(&CurrentCounter);
    CurrentFrameTime = ((float)(CurrentCounter.QuadPart - LastCounter.QuadPart) / (float)PerfCountFrequency);

    if (CurrentFrameTime < TargetSecondsPerFrame)
    {
        //Sleep if we have more than 1 millisecond until frame time
        DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - CurrentFrameTime));
        if (SleepMS > 0)
        {
            Sleep(SleepMS);
        }

        while (CurrentFrameTime < TargetSecondsPerFrame)
        {
            LARGE_INTEGER CurrentCounter;
            QueryPerformanceCounter(&CurrentCounter);

            CurrentFrameTime = ((float)(CurrentCounter.QuadPart - LastCounter.QuadPart) / (float)PerfCountFrequency);
        }
    }

    LastCounter = CurrentCounter;
}