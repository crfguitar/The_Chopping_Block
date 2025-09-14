#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// Minimal scaffold for a time/pitch engine abstraction.
// Current implementation is a passthrough so it won’t change playback yet.
// When USE_SIGNALSMITH is defined (CMake option), we can include
// signalsmith-stretch.hpp and add a backend in a future change.

class TimePitchEngine {
public:
    void prepare(double sampleRate, int maxChannels) { sr = sampleRate; channels = juce::jmax(1, maxChannels); }
    void setParams(float semitones, float stretchRatio) {
        pitchSemitones = semitones; timeStretch = stretchRatio;
    }

    // In-place passthrough helper for interleaved buffer sections
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
        juce::ignoreUnused(startSample, numSamples);
        // Passthrough for now. Backend will replace this.
    }

    // Non-interleaved IO (for future backends)
    void process(const float* const* in, float* const* out, int numChannels, int numSamples) {
        for (int ch = 0; ch < numChannels; ++ch) {
            const float* src = in[ch];
            float* dst = out[ch];
            if (src == nullptr || dst == nullptr) continue;
            std::memcpy(dst, src, sizeof(float) * (size_t) numSamples);
        }
    }

    double getSampleRate() const { return sr; }
    float  getPitchSemitones() const { return pitchSemitones; }
    float  getTimeStretch() const { return timeStretch; }

private:
    double sr { 44100.0 };
    int channels { 2 };
    float pitchSemitones { 0.0f }; // +12 = up an octave
    float timeStretch { 1.0f };    // 1.0 = native speed; 0.5 = half-time
};

#if defined(USE_SIGNALSMITH)
// Placeholder include (not used yet). In a follow-up, we’ll add a concrete
// backend which wraps signalsmith-stretch processing while keeping this
// interface stable.
// #include <signalsmith-stretch.hpp>
#endif

