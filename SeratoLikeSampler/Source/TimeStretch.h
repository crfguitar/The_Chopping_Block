
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// Lightweight, always-compilable stretcher with a linear resampling fallback.
// If USE_SIGNALSMITH is enabled and the header is available, we can switch
// to a higher-quality backend in a follow-up without changing this interface.
class TimeStretcher {
public:
    void prepare (double sampleRate, int /*blockSize*/) { sr = sampleRate; }
    void setRatios (float newTimeRatio, float newPitchSemis, bool /*formantPreserve*/) {
        timeRatio = newTimeRatio; pitchSemis = newPitchSemis;
    }

    // Process returns how many input samples were consumed starting at 'start'.
    int process (const juce::AudioBuffer<float>& src, int start, int numOut, juce::AudioBuffer<float>& dst) {
        const int channels = juce::jmin (src.getNumChannels(), 2);
        dst.setSize (juce::jmax (1, channels), numOut, true, true, true);
        if (numOut <= 0) return 0;

        // Effective playback speed: pitch affects rate; timeRatio slows/speeds independently in this fallback.
        const double pitchRatio = std::pow (2.0, (double) pitchSemis / 12.0);
        const double timeR = juce::jmax (1.0e-4, (double) timeRatio);
        const double rate = pitchRatio / timeR; // >1 = faster, <1 = slower

        const int total = src.getNumSamples();
        double pos = (double) start;
        int maxConsumed = 0;

        for (int i = 0; i < numOut; ++i) {
            const int i0 = juce::jlimit (0, total - 1, (int) pos);
            const int i1 = juce::jmin (total - 1, i0 + 1);
            const float frac = (float) (pos - (double) i0);
            for (int ch = 0; ch < channels; ++ch) {
                const float s0 = src.getSample (ch, i0);
                const float s1 = src.getSample (ch, i1);
                const float v = s0 + (s1 - s0) * frac;
                dst.setSample (ch, i, v);
            }
            pos += rate;
        }

        maxConsumed = (int) std::floor (pos - (double) start);
        return juce::jmax (0, maxConsumed);
    }

private:
    double sr { 44100.0 };
public:
    float timeRatio { 1.0f }, pitchSemis { 0.0f };
};
