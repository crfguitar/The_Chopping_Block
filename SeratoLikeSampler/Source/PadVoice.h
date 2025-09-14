
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "TimeStretch.h"

struct PadSlice {
    int startSample = 0;
    int endSample = 0;
    int midiNote = 36;
    float gainLin { 1.0f };
    float pitchSemitones { 0.0f }; // per-pad pitch shift in semitones
    float timeRatio { 1.0f };      // per-pad time stretch ratio (1.0 = normal)
    bool reverse { false };        // play slice backwards
};

class PadVoice {
public:
    void prepare (double sampleRate, int blockSize) {
        sr = sampleRate;
        adsr.setSampleRate (sampleRate);
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };
        lp.reset(); lp.prepare (spec);
        lp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        lp.setCutoffFrequency (12000.0f);
        lp.setResonance (0.7f);
        stretcher.prepare (sampleRate, blockSize);
        temp.setSize (2, 1);
    }
    void setParams (float attack, float release, float cutoff, float reso, float gainDb) {
        juce::ADSR::Parameters p; p.attack = attack; p.decay = 0.0f; p.sustain = 1.0f; p.release = release;
        adsr.setParameters (p);
        lp.setCutoffFrequency (cutoff);
        lp.setResonance (reso);
        gainLin = juce::Decibels::decibelsToGain (gainDb);
    }
    void startNote (const juce::AudioBuffer<float>& src, const PadSlice& slice) {
        source = &src; current = slice; pos = current.reverse ? current.endSample : current.startSample; adsr.noteOn(); active = true; sliceGainLin = current.gainLin;
        // Configure stretcher for this note
        stretcher.setRatios (current.timeRatio, current.pitchSemitones, false);
    }
    void stopNote() { adsr.noteOff(); }
    void kill() { active = false; }
    bool isActive() const { return active; }
    bool isPlayingMidi (int midiNote) const { return active && current.midiNote == midiNote; }
    void render (juce::AudioBuffer<float>& out, int startSample, int numSamples) {
        if (! active || source == nullptr) return;
        temp.setSize (out.getNumChannels(), numSamples, false, true, true); temp.clear();
        const int sliceLength = juce::jmax (0, current.endSample - current.startSample);
        const int remaining = current.reverse ? juce::jmax (0, pos - current.startSample) : juce::jmax (0, current.endSample - pos);
        const int toCopy = juce::jlimit (0, numSamples, remaining);
        if (toCopy > 0) {
            if (current.reverse) {
                // Copy reversed samples into temp
                temp.clear();
                for (int ch = 0; ch < temp.getNumChannels(); ++ch) {
                    const int srcCh = juce::jmin (ch, source->getNumChannels()-1);
                    for (int i = 0; i < toCopy; ++i) {
                        int srcIndex = juce::jlimit (current.startSample, current.endSample - 1, pos - 1 - i);
                        temp.setSample (ch, i, source->getSample (srcCh, srcIndex));
                    }
                }
                pos -= toCopy;
            } else {
                // Forward playback: allow stretcher to process
                // For now, stretcher is a passthrough. This call sets up future integration.
                stretcher.process (*source, pos, toCopy, temp);
                pos += toCopy;
            }
        }
        adsr.applyEnvelopeToBuffer (temp, 0, numSamples);
        juce::dsp::AudioBlock<float> blk (temp);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        lp.process (ctx);
        for (int ch = 0; ch < out.getNumChannels(); ++ch)
            out.addFrom (ch, startSample, temp, ch, 0, numSamples, gainLin * sliceGainLin);
        const bool reachedEnd = current.reverse ? (pos <= current.startSample) : (pos >= current.endSample);
        if (reachedEnd || ! adsr.isActive()) active = false;
    }
private:
    const juce::AudioBuffer<float>* source { nullptr };
    PadSlice current; int pos { 0 }; double sr { 44100.0 }; bool active { false };
    juce::ADSR adsr;
    juce::dsp::StateVariableTPTFilter<float> lp;
    float gainLin { 1.0f }; float sliceGainLin { 1.0f };
    TimeStretcher stretcher; juce::AudioBuffer<float> temp;
};
