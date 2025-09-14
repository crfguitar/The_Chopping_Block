
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
class WaveformCache {
public:
    void build (const juce::AudioBuffer<float>& buffer, int samplesPerBin = 512) {
        bins.clear();
        if (buffer.getNumSamples() == 0) return;
        const int numBins = juce::jmax (1, buffer.getNumSamples() / samplesPerBin);
        bins.resize (numBins, {0.f, 0.f});
        for (int i = 0; i < numBins; ++i) {
            const int start = i * samplesPerBin;
            const int end = juce::jmin (buffer.getNumSamples(), start + samplesPerBin);
            float mn =  1e9f, mx = -1e9f;
            for (int s = start; s < end; ++s) {
                float v = 0.f;
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    v += std::abs (buffer.getSample (ch, s));
                v /= (float) buffer.getNumChannels();
                mn = std::min (mn, v);
                mx = std::max (mx, v);
            }
            bins[i] = { mn, mx };
        }
    }
    const std::vector<std::pair<float,float>>& get() const { return bins; }
private:
    std::vector<std::pair<float,float>> bins;
};
