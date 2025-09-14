#include "SamplerLookAndFeel.h"
using namespace juce;

SamplerLookAndFeel::SamplerLookAndFeel() {
    setColour (TextButton::textColourOffId, Colours::white);
    setColour (TextButton::textColourOnId, Colours::white);
}

void SamplerLookAndFeel::drawButtonBackground (Graphics& g, Button& button, const Colour&, bool isOver, bool isDown) {
    auto bounds = button.getLocalBounds().toFloat();
    const float radius = jmin (12.0f, jmin (bounds.getWidth(), bounds.getHeight()) * 0.2f);

    const bool isPad = button.getProperties().contains ("padIndex");

    Colour base = button.findColour (TextButton::buttonColourId);
    if (! isPad)
        base = base.isTransparent() ? Colours::grey.darker (1.2f) : base;

    if (isDown) base = base.darker (0.25f);
    else if (isOver) base = base.brighter (0.10f);

    // Soft drop shadow for pads
    if (isPad) {
        DropShadow ds (Colours::black.withAlpha (0.50f), 14, { 0, 6 });
        ds.drawForRectangle (g, bounds.getSmallestIntegerContainer());
    }

    // Gradient fill for depth
    Colour top = base.brighter (0.25f);
    Colour bottom = base.darker (0.15f);
    ColourGradient grad (top, bounds.getTopLeft(), bottom, bounds.getBottomRight(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds.reduced (1.5f), radius);

    // Subtle inner highlight
    if (isPad) {
        ColourGradient inner (Colours::white.withAlpha (0.15f),
                              bounds.getTopLeft(),
                              Colours::transparentWhite,
                              bounds.getBottomLeft(), false);
        g.setGradientFill (inner);
        g.fillRoundedRectangle (bounds.reduced (3.0f), radius - 2.0f);
    }

    // Border
    g.setColour (base.darker (0.45f));
    g.drawRoundedRectangle (bounds.reduced (0.8f), radius, 1.8f);
}

void SamplerLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool) {
    auto bounds = b.getLocalBounds();
    const float fontSize = jmin (24.0f, bounds.getHeight() * 0.55f);
    g.setFont (Font (fontSize, Font::bold));
    g.setColour (b.findColour (TextButton::textColourOffId));
    g.drawFittedText (b.getButtonText(), bounds.reduced (2), Justification::centred, 1);
}

