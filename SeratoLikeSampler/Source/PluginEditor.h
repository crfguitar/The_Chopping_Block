
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "SamplerLookAndFeel.h"
class SliceListComponent;
class SeratoLikeSamplerAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                              public juce::FileDragAndDropTarget,
                                              public juce::Timer {
public:
    SeratoLikeSamplerAudioProcessorEditor (SeratoLikeSamplerAudioProcessor&);
    ~SeratoLikeSamplerAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override { repaint(); }
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    // File drag-and-drop
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
private:
    void drawWaveform (juce::Graphics& g, juce::Rectangle<int> r);
    SeratoLikeSamplerAudioProcessor& processor;
    SamplerLookAndFeel lookAndFeel;
    juce::Slider attack, release, cutoff, reso, gain, baseNote, maxSlices, sensitivity, minGapMs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aAttack, aRelease, aCutoff, aReso, aGain, aBaseNote, aMaxSlices, aSensitivity, aMinGapMs;
    juce::TextButton padButtons[16];
    juce::TextButton btnZoomIn { "+" };
    juce::TextButton btnZoomOut { "-" };
    // Branding
    juce::Image appLogo; // raster fallback
    std::unique_ptr<juce::Drawable> appLogoDrawable; // preferred (SVG)
    juce::TextButton btnPreview { "Preview" };
    juce::TextButton btnTap { "Tap Slice" };
    juce::ToggleButton btnLoop { "Loop" };
    juce::ToggleButton btnSnap { "Snap Loop" };
    juce::TextButton btnExportCsv { "Export CSV" };
    juce::TextButton btnExportWavs { "Export WAVs" };
    juce::ToggleButton btnNormalize { "Normalize" };
    juce::ToggleButton btnChoke { "Choke" };
    juce::ToggleButton btnGate { "Gate" };
    juce::TextButton btnSavePreset { "Save" };
    juce::TextButton btnLoadPreset { "Load" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aChoke;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aGate;
    juce::Rectangle<int> lastWaveRect;
    bool draggingLoop { false }; float dragStartNorm { 0.f }; float dragEndNorm { 1.f };
    juce::Viewport sliceViewport; std::unique_ptr<SliceListComponent> sliceList;
    // Waveform view state
    float zoom { 1.0f }; // 1 = full, >1 zoomed in
    float offset { 0.0f }; // 0..1 start position when zoomed
    int hoverBoundaryIndex { -1 }; // slice index of the boundary being hovered (== start of slice i), i>=1
    int draggingBoundaryIndex { -1 };
    int lastMouseX { -1 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeratoLikeSamplerAudioProcessorEditor)
};
