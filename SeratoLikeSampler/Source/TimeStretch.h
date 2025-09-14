
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>
#if defined(USE_SIGNALSMITH)
#include <signalsmith-stretch.h>
#endif

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
        const int ch = juce::jmin (src.getNumChannels(), 2);
        dst.setSize (juce::jmax (1, ch), numOut, true, true, true);
        if (numOut <= 0) return 0;

        const double pitchRatio = std::pow (2.0, (double) pitchSemis / 12.0);
        const double timeR = juce::jmax (1.0e-4, (double) timeRatio);
        const double rate = pitchRatio / timeR; // >1 = faster, <1 = slower

        const int total = src.getNumSamples();
        const int available = juce::jmax (0, total - start);
        int inputNeeded = (int) std::round ((double) numOut / juce::jmax (1.0e-4, rate));
        int inputSamples = juce::jlimit (0, available, juce::jmax (1, inputNeeded));

#if defined(USE_SIGNALSMITH)
        // High-quality path via SignalsmithStretch
        if (inputSamples > 0) {
            if (!configured || channels != ch) {
                channels = ch;
                ss.presetDefault(channels, (float) sr, false);
                configured = true;
            }
            ss.setTransposeSemitones((float) pitchSemis);
            // Build input/output channel arrays
            std::vector<const float*> in(channels);
            std::vector<float*> out(channels);
            for (int c = 0; c < channels; ++c) {
                in[c] = src.getReadPointer(c, start);
                out[c] = dst.getWritePointer(c);
            }
            ss.process(in.data(), inputSamples, out.data(), numOut);
        }
        return inputSamples;
#else
        // Lightweight linear resampling fallback
        double pos = (double) start;
        for (int i = 0; i < numOut; ++i) {
            const int i0 = juce::jlimit (0, total - 1, (int) pos);
            const int i1 = juce::jmin (total - 1, i0 + 1);
            const float frac = (float) (pos - (double) i0);
            for (int c = 0; c < ch; ++c) {
                const float s0 = src.getSample (c, i0);
                const float s1 = src.getSample (c, i1);
                const float v = s0 + (s1 - s0) * frac;
                dst.setSample (c, i, v);
            }
            pos += rate;
        }
        int consumed = (int) std::floor (pos - (double) start);
        return juce::jmax (0, consumed);
#endif
    }

private:
    double sr { 44100.0 };
    int channels { 2 };
#if defined(USE_SIGNALSMITH)
    signalsmith::stretch::SignalsmithStretch<float> ss;
    bool configured { false };
#endif
public:
    float timeRatio { 1.0f }, pitchSemis { 0.0f };
};
