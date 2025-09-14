#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>
#include <memory>
#include "PluginProcessor.h"

class SliceListComponent : public juce::Component, public juce::Timer {
public:
    explicit SliceListComponent (SeratoLikeSamplerAudioProcessor& p) : processor (p) { startTimerHz (10); rebuild(); }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::transparentBlack); }
    void resized() override {
        int y = 0; const int rowH = 28; const int pad = 4;
        for (size_t i = 0; i < rows.size(); ++i) {
            auto& r = rows[i];
            r.idx->setBounds (4, y + 4, 40, rowH - 8);
            r.note->setBounds (48, y + 4, 60, rowH - 8);
            r.time->setBounds (112, y + 4, 180, rowH - 8);
            r.gain->setBounds (296, y + 2, getWidth() - 300 - pad, rowH - 4);
            y += rowH;
        }
    }
    void timerCallback() override {
        auto& engine = processor.getEngine();
        auto count = engine.getSlices().size();
        if (count != lastCount) rebuild();
        // keep values in sync in case engine updated externally
        for (size_t i = 0; i < rows.size(); ++i) {
            float db = engine.getSliceGainDb ((int) i);
            if (std::abs (rows[i].gain->getValue() - db) > 0.01) rows[i].gain->setValue (db, juce::dontSendNotification);
        }
    }
private:
    struct Row {
        std::unique_ptr<juce::Label> idx, note, time;
        std::unique_ptr<juce::Slider> gain;
    };
    void rebuild() {
        rows.clear();
        removeAllChildren();
        auto& engine = processor.getEngine();
        const auto& slices = engine.getSlices();
        rows.resize (slices.size());
        for (size_t i = 0; i < slices.size(); ++i) {
            const auto& s = slices[i];
            rows[i].idx = std::make_unique<juce::Label>();
            rows[i].note = std::make_unique<juce::Label>();
            rows[i].time = std::make_unique<juce::Label>();
            rows[i].gain = std::make_unique<juce::Slider>();
            addAndMakeVisible (*rows[i].idx); addAndMakeVisible (*rows[i].note); addAndMakeVisible (*rows[i].time); addAndMakeVisible (*rows[i].gain);
            rows[i].idx->setText (juce::String ((int) i), juce::dontSendNotification);
            static const char* names[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            int octave = (s.midiNote / 12) - 1; const char* nm = names[s.midiNote % 12];
            rows[i].note->setText (juce::String (nm) + juce::String (octave), juce::dontSendNotification);
            double sr = engine.getPool().getSampleRate();
            double st = s.startSample / sr; double en = s.endSample / sr; double du = juce::jmax (0, s.endSample - s.startSample) / sr;
            rows[i].time->setText (juce::String (st, 3) + "s  ->  " + juce::String (en, 3) + "s  (" + juce::String (du, 3) + "s)", juce::dontSendNotification);
            auto* sl = rows[i].gain.get();
            sl->setRange (-24.0, 24.0, 0.01); sl->setSliderStyle (juce::Slider::LinearBar); sl->setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
            sl->setValue (engine.getSliceGainDb ((int) i));
            sl->onValueChange = [this, idx = (int) i, sl, &engine](){ engine.setSliceGainDb (idx, (float) sl->getValue()); };
        }
        lastCount = slices.size();
        setSize (getWidth(), (int) (rows.size() * 28 + 2));
        resized();
    }
    SeratoLikeSamplerAudioProcessor& processor;
    std::vector<Row> rows; size_t lastCount { 0 };
};
