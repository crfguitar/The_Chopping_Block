#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>
#include <memory>
#include "PluginProcessor.h"

class SliceListComponent : public juce::Component, public juce::Timer {
public:
    explicit SliceListComponent (NoobToolsAudioProcessor& p) : processor (p) { startTimerHz (10); rebuild(); }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::transparentBlack); }
    void resized() override {
        int y = 0; const int rowH = 28; const int pad = 4;
        for (size_t i = 0; i < rows.size(); ++i) {
            auto& r = rows[i];
            int x = 4;
            r.idx->setBounds (x, y + 4, 36, rowH - 8); x += 38;
            r.note->setBounds (x, y + 4, 52, rowH - 8); x += 54;
            r.time->setBounds (x, y + 4, 150, rowH - 8); x += 152;
            // Pitch (semitones) and Time (ratio)
            if (r.pitch) { r.pitch->setBounds (x, y + 4, 80, rowH - 8); x += 84; }
            if (r.ratio) { r.ratio->setBounds (x, y + 4, 90, rowH - 8); x += 94; }
            if (r.reverse) { r.reverse->setBounds (x, y + 4, 72, rowH - 8); x += 74; }
            // Remaining width for gain bar
            r.gain->setBounds (x, y + 2, juce::jmax (40, getWidth() - x - pad), rowH - 4);
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
            float semi = engine.getSlicePitchSemitones ((int) i);
            if (rows[i].pitch && std::abs (rows[i].pitch->getValue() - semi) > 0.01f)
                rows[i].pitch->setValue (semi, juce::dontSendNotification);
            float ratio = engine.getSliceTimeRatio ((int) i);
            if (rows[i].ratio && std::abs (rows[i].ratio->getValue() - ratio) > 0.001f)
                rows[i].ratio->setValue (ratio, juce::dontSendNotification);
            bool rev = engine.getSliceReverse ((int) i);
            if (rows[i].reverse && rows[i].reverse->getToggleState() != rev)
                rows[i].reverse->setToggleState (rev, juce::dontSendNotification);
        }
    }
private:
    struct Row {
        std::unique_ptr<juce::Label> idx, note, time;
        std::unique_ptr<juce::Slider> gain, pitch, ratio;
        std::unique_ptr<juce::ToggleButton> reverse;
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
            rows[i].pitch = std::make_unique<juce::Slider>();
            rows[i].ratio = std::make_unique<juce::Slider>();
            rows[i].reverse = std::make_unique<juce::ToggleButton>("Rev");
            addAndMakeVisible (*rows[i].idx);
            addAndMakeVisible (*rows[i].note);
            addAndMakeVisible (*rows[i].time);
            addAndMakeVisible (*rows[i].pitch);
            addAndMakeVisible (*rows[i].ratio);
            addAndMakeVisible (*rows[i].reverse);
            addAndMakeVisible (*rows[i].gain);
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
            // Pitch semitones (-24..+24)
            auto* sp = rows[i].pitch.get();
            sp->setRange (-24.0, 24.0, 0.01); sp->setSliderStyle (juce::Slider::LinearBar); sp->setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
            sp->setValue (engine.getSlicePitchSemitones ((int) i));
            sp->onValueChange = [idx = (int) i, sp, &engine](){ engine.setSlicePitchSemitones (idx, (float) sp->getValue()); };
            // Time ratio (0.25..4.0)
            auto* sr = rows[i].ratio.get();
            sr->setRange (0.25, 4.0, 0.001); sr->setSliderStyle (juce::Slider::LinearBar); sr->setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
            sr->setValue (engine.getSliceTimeRatio ((int) i));
            sr->onValueChange = [idx = (int) i, sr, &engine](){ engine.setSliceTimeRatio (idx, (float) sr->getValue()); };
            // Reverse toggle
            auto* rb = rows[i].reverse.get();
            rb->setToggleState (engine.getSliceReverse ((int) i), juce::dontSendNotification);
            rb->onClick = [idx = (int) i, rb, &engine](){ engine.setSliceReverse (idx, rb->getToggleState()); };
        }
        lastCount = slices.size();
        setSize (getWidth(), (int) (rows.size() * 28 + 2));
        resized();
    }
    NoobToolsAudioProcessor& processor;
    std::vector<Row> rows; size_t lastCount { 0 };
};
