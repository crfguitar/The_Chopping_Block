// Minimal custom LookAndFeel for colourful performance pads and dark buttons
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class SamplerLookAndFeel : public juce::LookAndFeel_V4 {
public:
    SamplerLookAndFeel();
    ~SamplerLookAndFeel() override = default;

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool isMouseOverButton,
                               bool isButtonDown) override;

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool isMouseOverButton,
                         bool isButtonDown) override;
};

