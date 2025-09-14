
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "WaveformCache.h"
class SamplePool {
public:
    bool loadFromFile (const juce::File& file) {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (! reader) return false;
        buffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
        sampleRate = reader->sampleRate; fileName = file.getFileNameWithoutExtension();
        waveform.build (buffer, 1024); return true;
    }
    void clear() { buffer.setSize (0, 0); fileName.clear(); sampleRate = 44100.0; waveform = WaveformCache{}; }
    const juce::AudioBuffer<float>& getBuffer() const { return buffer; }
    double getSampleRate() const { return sampleRate; }
    const juce::String& getName() const { return fileName; }
    const WaveformCache& getWaveform() const { return waveform; }
private:
    juce::AudioBuffer<float> buffer; double sampleRate { 44100.0 }; juce::String fileName; WaveformCache waveform;
};
