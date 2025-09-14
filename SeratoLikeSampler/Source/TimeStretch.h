
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
class TimeStretcher {
public:
    void prepare (double, int) {}
    void setRatios (float newTimeRatio, float newPitchSemis, bool) {
        timeRatio = newTimeRatio; pitchSemis = newPitchSemis;
    }
    void process (const juce::AudioBuffer<float>& src, int start, int num, juce::AudioBuffer<float>& dst) {
        dst.setSize (src.getNumChannels(), num, true, true, true);
        for (int ch = 0; ch < dst.getNumChannels(); ++ch)
            dst.copyFrom (ch, 0, src, ch, start, num);
    }
    float timeRatio { 1.0f }, pitchSemis { 0.0f };
};
