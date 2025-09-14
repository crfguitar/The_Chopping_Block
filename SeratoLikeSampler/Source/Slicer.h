
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
struct SlicePoint { int sampleIndex = 0; };
class SpectralFluxSlicer {
public:
    void prepare (double sampleRate, int fftOrder = 12, int hop = 512) {
        sr = sampleRate; order = fftOrder; fftSize = 1 << order; hopSize = hop;
        window.setSize (1, fftSize);
        for (int i = 0; i < fftSize; ++i)
            window.setSample (0, i, 0.5f * (1.f - std::cos(2.f * juce::MathConstants<float>::pi * (float)i / (float)(fftSize-1))));
        fft = std::make_unique<juce::dsp::FFT>(order);
        mag.resize (fftSize/2); prevMag.assign (fftSize/2, 0.0f);
    }
    void setThresholdScale (float s) { thresholdScale = s; }
    void setLocalWindow (int w)      { localWindow = juce::jlimit (4, 128, w); }
    void setHopSize (int hop)        { hopSize = juce::jmax (64, hop); }
    std::vector<SlicePoint> slice (const juce::AudioBuffer<float>& buffer, int channel = 0, int targetSlices = 16) {
        std::vector<float> novelty; computeNovelty (buffer, channel, novelty);
        const int n = (int) novelty.size();
        std::vector<int> peaks; const int w = localWindow;
        for (int i = 1; i < n-1; ++i) {
            float localMean = 0.f; int count = 0;
            for (int k = -w; k <= w; ++k) { int idx = juce::jlimit (0, n-1, i+k); localMean += novelty[(size_t)idx]; ++count; }
            localMean /= (float) count; float th = localMean * thresholdScale;
            if (novelty[(size_t)i] > th && novelty[(size_t)i] > novelty[(size_t)i-1] && novelty[(size_t)i] > novelty[(size_t)i+1])
                peaks.push_back (i);
        }
        std::vector<SlicePoint> out; out.push_back({0});
        for (int idx : peaks) { int sampleIdx = idx * hopSize; if (sampleIdx > 200) out.push_back({sampleIdx}); }
        if ((int) out.size() > targetSlices) {
            std::vector<SlicePoint> reduced; float stride = (float) out.size() / (float) targetSlices;
            for (int i = 0; i < targetSlices; ++i)
                reduced.push_back (out[(size_t) juce::jlimit(0, (int)out.size()-1, (int) std::round(i * stride))]);
            out = std::move(reduced);
        }
        return out;
    }
private:
    void computeNovelty (const juce::AudioBuffer<float>& buffer, int channel, std::vector<float>& novelty) {
        novelty.clear(); if (buffer.getNumSamples() < fftSize) return;
        tempBlock.setSize (1, fftSize);
        juce::HeapBlock<float> fftData; fftData.allocate ((size_t)(2 * fftSize), true);
        for (int pos = 0; pos + fftSize < buffer.getNumSamples(); pos += hopSize) {
            for (int i = 0; i < fftSize; ++i) {
                float w = window.getSample (0, i);
                float x = buffer.getSample (juce::jmin (channel, buffer.getNumChannels()-1), pos + i);
                fftData[i] = x * w;
            }
            for (int i = fftSize; i < 2*fftSize; ++i) fftData[i] = 0.0f;
            fft->performRealOnlyForwardTransform (fftData.getData());
            for (int k = 0; k < fftSize/2; ++k) {
                float re = fftData[k]; float im = fftData[fftSize - k - 1];
                mag[(size_t)k] = std::sqrt (re*re + im*im);
            }
            float flux = 0.0f;
            for (int k = 0; k < fftSize/2; ++k) {
                float d = mag[(size_t)k] - prevMag[(size_t)k];
                if (d > 0) flux += d;
                prevMag[(size_t)k] = mag[(size_t)k];
            }
            novelty.push_back (flux);
        }
    }
    double sr = 44100.0; int order = 12, fftSize = 4096, hopSize = 512; float thresholdScale { 1.2f }; int localWindow { 16 };
    juce::AudioBuffer<float> window, tempBlock;
    std::unique_ptr<juce::dsp::FFT> fft; std::vector<float> mag, prevMag;
};
