
#include "PluginEditor.h"
#include <cmath>
#include "AudioEngine.h"
#include "SliceListComponent.h"
#include "JuceHeader.h"
NoobToolsAudioProcessorEditor::~NoobToolsAudioProcessorEditor() { setLookAndFeel (nullptr); }
NoobToolsAudioProcessorEditor::NoobToolsAudioProcessorEditor (NoobToolsAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p) {
    using namespace juce;
    setSize (960, 540);
    // Install custom LookAndFeel for a more polished UI
    setLookAndFeel (&lookAndFeel);
    // Load embedded logo (BinaryData) first, then fall back to external files
    {
        int bestScore = -1; juce::Image bestImg;
        std::unique_ptr<juce::Drawable> bestDrawable;
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
            const char* name = BinaryData::namedResourceList[i];
            juce::String nm (name);
            int dataSize = 0; const char* data = BinaryData::getNamedResource (name, dataSize);
            if (data == nullptr || dataSize <= 0) continue;
            // Try SVG first
            if (nm.endsWithIgnoreCase("_svg")) {
                auto svgText = juce::String::fromUTF8 (data, dataSize);
                std::unique_ptr<juce::XmlElement> svg (juce::XmlDocument::parse (svgText));
                if (svg != nullptr) {
                    auto drawable = juce::Drawable::createFromSVG (*svg);
                    if (drawable != nullptr) {
                        int score = nm.containsIgnoreCase("logo") ? 3 : 2;
                        if (score > bestScore) { bestScore = score; bestDrawable = std::move (drawable); bestImg = {}; }
                    }
                }
            }
            // Raster fallback
            if (nm.endsWithIgnoreCase("_png") || nm.endsWithIgnoreCase("_jpg") || nm.endsWithIgnoreCase("_jpeg")) {
                juce::Image img = juce::ImageFileFormat::loadFrom (data, (size_t) dataSize);
                if (img.isValid()) { int score = nm.containsIgnoreCase("logo") ? 2 : 1; if (score > bestScore) { bestScore = score; bestImg = img; bestDrawable.reset(); } }
            }
        }
        if (bestDrawable) appLogoDrawable = std::move (bestDrawable);
        else if (bestImg.isValid()) appLogo = bestImg;
        auto tryLoad = [this](const juce::File& f){ if (f.existsAsFile()) { if (auto img = juce::ImageFileFormat::loadFrom (f); img.isValid()) appLogo = img; } };
        juce::File exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        if (! appLogo.isValid()) tryLoad (exeDir.getChildFile ("logo.png"));
        if (! appLogo.isValid()) tryLoad (exeDir.getChildFile ("Resources").getChildFile ("logo.png"));
        if (! appLogo.isValid()) {
            juce::File appFile = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
            juce::File maybeBundle = appFile.getFileExtension().equalsIgnoreCase (".vst3") ? appFile : appFile.getParentDirectory();
            if (maybeBundle.getFileName().endsWithIgnoreCase (".vst3"))
                tryLoad (maybeBundle.getChildFile ("Contents").getChildFile ("Resources").getChildFile ("logo.png"));
        }
    }
    auto& apvts = processor.getAPVTS();
    auto setup = [] (Slider& s, Slider::SliderStyle style) { s.setSliderStyle (style); s.setTextBoxStyle (Slider::TextBoxBelow, false, 60, 18); };
    setup (attack,  Slider::RotaryHorizontalVerticalDrag);
    setup (release, Slider::RotaryHorizontalVerticalDrag);
    setup (cutoff,  Slider::RotaryHorizontalVerticalDrag);
    setup (reso,    Slider::RotaryHorizontalVerticalDrag);
    setup (gain,    Slider::RotaryHorizontalVerticalDrag);
    setup (baseNote,Slider::RotaryHorizontalVerticalDrag);
    setup (maxSlices,Slider::RotaryHorizontalVerticalDrag);
    setup (sensitivity,Slider::RotaryHorizontalVerticalDrag);
    setup (minGapMs, Slider::RotaryHorizontalVerticalDrag);
    addAndMakeVisible (attack);  aAttack  = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "attack", attack);
    addAndMakeVisible (release); aRelease = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", release);
    addAndMakeVisible (cutoff);  aCutoff  = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "cutoff", cutoff);
    addAndMakeVisible (reso);    aReso    = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "reso", reso);
    addAndMakeVisible (gain);    aGain    = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "gain", gain);
    addAndMakeVisible (baseNote); aBaseNote = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "basenote", baseNote);
    addAndMakeVisible (maxSlices); aMaxSlices = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "maxslices", maxSlices);
    addAndMakeVisible (sensitivity); aSensitivity = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "sensitivity", sensitivity);
    addAndMakeVisible (minGapMs); aMinGapMs = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "mingapms", minGapMs);
    // Preview + Tap
    setWantsKeyboardFocus (true);
    addAndMakeVisible (btnPreview);
    btnPreview.setClickingTogglesState (true);
    btnPreview.onClick = [this]{ processor.getEngine().togglePreview(); };
    addAndMakeVisible (btnTap);
    btnTap.onClick = [this]{ processor.getEngine().tapSliceAtCurrent(); };
    addAndMakeVisible (btnLoop);
    btnLoop.onClick = [this]{ processor.getEngine().setLoopPreview (btnLoop.getToggleState()); };
    addAndMakeVisible (btnSnap);
    addAndMakeVisible (btnExportCsv);
    // Choke (mono) + Gate toggles bound to parameters
    addAndMakeVisible (btnChoke);
    addAndMakeVisible (btnGate);
    aChoke = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "choke", btnChoke);
    aGate  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "gate", btnGate);
    btnExportCsv.onClick = [this]{
        auto& engine = processor.getEngine();
        juce::FileChooser fc ("Export slices CSV", juce::File::getSpecialLocation (juce::File::userDesktopDirectory), "*.csv");
        if (fc.browseForFileToSave (true)) {
            auto file = fc.getResult();
            juce::FileOutputStream os (file);
            if (os.openedOk()) {
                const auto& slices = engine.getSlices();
                const double sr = engine.getPool().getSampleRate();
                os << "index,start_samples,end_samples,duration_samples,start_sec,end_sec,duration_sec,midi_note,note_name\n";
                for (size_t i = 0; i < slices.size(); ++i) {
                    const auto& s = slices[i];
                    const int durSamp = juce::jmax (0, s.endSample - s.startSample);
                    const double st = s.startSample / sr;
                    const double en = s.endSample / sr;
                    const double du = durSamp / sr;
                    auto midi = s.midiNote;
                    static const char* names[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                    int octave = (midi / 12) - 1; const char* nm = names[midi % 12];
                    os << (int) i << "," << s.startSample << "," << s.endSample << "," << durSamp << ","
                       << juce::String (st, 6) << "," << juce::String (en, 6) << "," << juce::String (du, 6) << "," << midi << "," << juce::String(nm) + juce::String(octave) << "\n";
                }
                os.flush();
            }
        }
    };
    addAndMakeVisible (btnExportWavs);
    addAndMakeVisible (btnNormalize);
    btnExportWavs.onClick = [this]{
        auto& engine = processor.getEngine();
        const auto& src = engine.getPool().getBuffer();
        if (src.getNumSamples() <= 0) return;
        juce::FileChooser fc ("Choose export folder", juce::File::getSpecialLocation (juce::File::userDesktopDirectory), "");
        if (! fc.browseForDirectory()) return;
        auto dir = fc.getResult();
        const auto& slices = engine.getSlices();
        juce::WavAudioFormat fmt;
        for (size_t i = 0; i < slices.size(); ++i) {
            const auto& s = slices[i];
            int n = juce::jmax (0, s.endSample - s.startSample);
            if (n <= 0) continue;
            juce::AudioBuffer<float> tmp (src.getNumChannels(), n);
            for (int ch = 0; ch < tmp.getNumChannels(); ++ch)
                tmp.copyFrom (ch, 0, src, juce::jmin (ch, src.getNumChannels()-1), s.startSample, n);
            if (btnNormalize.getToggleState()) {
                float peak = 0.0f;
                for (int ch = 0; ch < tmp.getNumChannels(); ++ch)
                    peak = juce::jmax (peak, tmp.getMagnitude (ch, 0, n));
                if (peak > 0.00001f) {
                    float g = 0.999f / peak;
                    for (int ch = 0; ch < tmp.getNumChannels(); ++ch)
                        tmp.applyGain (ch, 0, n, g);
                }
            }
            static const char* names[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            int octave = (s.midiNote / 12) - 1; const char* nm = names[s.midiNote % 12];
            auto base = juce::String::formatted ("%03d_%s%d_%d_%d.wav", (int) i, nm, octave, s.startSample, s.endSample);
            auto outFile = dir.getChildFile (base);
            std::unique_ptr<juce::FileOutputStream> os (outFile.createOutputStream());
            if (os && os->openedOk()) {
                if (auto* rawWriter = fmt.createWriterFor (os.release(), (double) engine.getPool().getSampleRate(), (unsigned int) tmp.getNumChannels(), 24, {}, 0)) {
                    std::unique_ptr<juce::AudioFormatWriter> writer (rawWriter);
                    writer->writeFromAudioSampleBuffer (tmp, 0, n);
                }
            }
        }
    };
    const juce::String keyMap = "1234567890qwerty"; // keyboard mapping for pads
    // Palette approximating the reference image (orange -> green -> purple/blue)
    const juce::Colour padColours[16] = {
        juce::Colour::fromRGB (246, 142,  51), // 1
        juce::Colour::fromRGB (245, 178,  46), // 2
        juce::Colour::fromRGB (208, 220,  72), // 3
        juce::Colour::fromRGB (120, 231, 102), // 4
        juce::Colour::fromRGB (240, 101,  73), // 5
        juce::Colour::fromRGB (244, 170,  67), // 6
        juce::Colour::fromRGB (119, 214,  93), // 7
        juce::Colour::fromRGB ( 75, 205, 175), // 8
        juce::Colour::fromRGB (224,  70, 135), // 9
        juce::Colour::fromRGB (147,  91, 238), // 10
        juce::Colour::fromRGB ( 74, 152, 229), // 11
        juce::Colour::fromRGB ( 69, 199, 213), // 12
        juce::Colour::fromRGB (184,  45, 170), // 13
        juce::Colour::fromRGB (116,  96, 222), // 14
        juce::Colour::fromRGB ( 47, 110, 210), // 15
        juce::Colour::fromRGB ( 53, 175, 219)  // 16
    };

    for (int i = 0; i < 16; ++i) {
        addAndMakeVisible (padButtons[i]);
        padButtons[i].setButtonText (keyMap.substring (i, i + 1));
        padButtons[i].getProperties().set ("padIndex", i);
        padButtons[i].setColour (juce::TextButton::buttonColourId, padColours[i]);
        padButtons[i].setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        padButtons[i].setTooltip ("Pad " + juce::String (i + 1) + " — key '" + keyMap.substring (i, i + 1) + "'");
        padButtons[i].onClick = [this, i] {
            int midiNote = 36 + i;
            if (editMode) {
                processor.getEngine().createUserSliceAtCurrent (midiNote, btnQuantize.getToggleState());
                repaint();
            } else {
                juce::MidiMessage m = juce::MidiMessage::noteOn (1, midiNote, (juce::uint8) 100); juce::MidiBuffer buf; buf.addEvent (m, 0);
                auto* proc = dynamic_cast<NoobToolsAudioProcessor*>(getAudioProcessor());
                if (proc != nullptr) { juce::AudioBuffer<float> scratch; scratch.setSize (2, 128, false, false, true); proc->processBlock (scratch, buf); }
            }
        };
    }
    // Zoom buttons
    addAndMakeVisible (btnZoomIn);
    addAndMakeVisible (btnZoomOut);
    btnZoomIn.onClick = [this]{ zoom = juce::jlimit (1.0f, 64.0f, zoom * 1.25f); repaint(); };
    btnZoomOut.onClick = [this]{ zoom = juce::jlimit (1.0f, 64.0f, zoom / 1.25f); if (zoom <= 1.01f) { zoom = 1.0f; offset = 0.0f; } repaint(); };
    // Subtle dark style for utility buttons so pads stand out
    auto dark = juce::Colour::fromRGB (45, 60, 66);
    for (juce::Button* b : { (juce::Button*)&btnPreview, (juce::Button*)&btnTap, (juce::Button*)&btnExportCsv, (juce::Button*)&btnExportWavs, (juce::Button*)&btnEdit, (juce::Button*)&btnQuantize })
        b->setColour (juce::TextButton::buttonColourId, dark);
    // Slice list
    sliceList = std::make_unique<SliceListComponent> (processor);
    sliceViewport.setViewedComponent (sliceList.get(), false);
    addAndMakeVisible (sliceViewport);

    // Edit/Quantize controls
    addAndMakeVisible (btnEdit);
    addAndMakeVisible (btnQuantize);
    btnQuantize.setButtonText ("Quantize");
    btnEdit.onClick = [this]{ editMode = btnEdit.getToggleState(); sliceViewport.setVisible (editMode); repaint(); };
    btnQuantize.setToggleState (true, juce::dontSendNotification);

    startTimerHz (30);
}
void NoobToolsAudioProcessorEditor::paint (juce::Graphics& g) {
    g.fillAll (juce::Colours::black.withBrightness (0.11f));
    auto r = getLocalBounds();
    auto top = r.removeFromTop (40);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("NoobTools — The Chopping Block", top, juce::Justification::centred);
    // Waveform panel with subtle shadow and border
    auto wfPanel = r.removeFromTop (220).reduced (10, 2);
    juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 16, { 0, 6 }).drawForRectangle (g, wfPanel);
    auto wfBg = wfPanel.reduced (2);
    auto rf = wfBg.toFloat();
    juce::Colour bgTop = juce::Colour::fromRGB (40, 40, 42);
    juce::Colour bgBottom = juce::Colour::fromRGB (32, 32, 34);
    g.setGradientFill (juce::ColourGradient (bgTop, rf.getTopLeft(), bgBottom, rf.getBottomLeft(), false));
    g.fillRoundedRectangle (rf, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (rf, 6.0f, 1.2f);
    // Inner content area
    auto wf = wfBg.reduced (6, 6);
    lastWaveRect = wf;
    drawWaveform (g, wf);
    // Place zoom buttons in the top-right of waveform panel
    const int zSize = 24;
    btnZoomIn.setBounds (wfPanel.getRight() - (zSize * 2 + 8), wfPanel.getY() + 6, zSize, zSize);
    btnZoomOut.setBounds (wfPanel.getRight() - (zSize + 4), wfPanel.getY() + 6, zSize, zSize);
    auto pads = r.removeFromTop (220).reduced (10);
    const int cellW = pads.getWidth() / 4; const int cellH = pads.getHeight() / 4;
    for (int rIdx = 0; rIdx < 4; ++rIdx)
        for (int c = 0; c < 4; ++c) {
            int i = rIdx * 4 + c;
            padButtons[i].setBounds (pads.getX() + c*cellW + 4, pads.getY() + rIdx*cellH + 4, cellW - 8, cellH - 8);
        }
    auto knobs = r.reduced (10); auto w = knobs.getWidth() / 9;
    attack .setBounds (knobs.removeFromLeft (w).reduced (6));
    release.setBounds (knobs.removeFromLeft (w).reduced (6));
    cutoff .setBounds (knobs.removeFromLeft (w).reduced (6));
    reso   .setBounds (knobs.removeFromLeft (w).reduced (6));
    gain   .setBounds (knobs.removeFromLeft (w).reduced (6));
    baseNote.setBounds (knobs.removeFromLeft (w).reduced (6));
    maxSlices.setBounds (knobs.removeFromLeft (w).reduced (6));
    sensitivity.setBounds (knobs.removeFromLeft (w).reduced (6));
    minGapMs.setBounds (knobs.removeFromLeft (w).reduced (6));
    // Buttons on the right of waveform area
    int btnTop = 8 + 40; int right = getWidth() - 10;
    btnExportWavs.setBounds (right - 100, btnTop, 100, 26); right -= 105;
    btnNormalize.setBounds (right - 100, btnTop, 100, 26); right -= 105;
    btnExportCsv.setBounds (right - 90, btnTop, 90, 26); right -= 95;
    btnLoop.setBounds (right - 70, btnTop, 70, 26); right -= 75;
    btnChoke.setBounds (right - 80, btnTop, 80, 26); right -= 85;
    btnGate .setBounds (right - 70, btnTop, 70, 26); right -= 75;
    btnSnap.setBounds (right - 100, btnTop, 100, 26); right -= 105;
    btnQuantize.setBounds (right - 90, btnTop, 90, 26); right -= 95;
    btnEdit.setBounds (right - 90, btnTop, 90, 26); right -= 95;
    btnTap.setBounds (right - 90, btnTop, 90, 26); right -= 95;
    btnPreview.setBounds (right - 90, btnTop, 90, 26);
    // Slice list viewport overlays the pad area
    sliceViewport.setBounds (pads);
    sliceViewport.setVisible (false); // hidden until Edit mode
}
void NoobToolsAudioProcessorEditor::resized() {}
void NoobToolsAudioProcessorEditor::drawWaveform (juce::Graphics& g, juce::Rectangle<int> r) {
    auto& engine = processor.getEngine(); const auto& wf = engine.getWaveform().get();
    // Background
    g.setColour (juce::Colour::fromRGB (58, 60, 62));
    g.fillRoundedRectangle (r.toFloat(), 4.0f);
    // Grid and zero-line + time ruler
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    const int zeroY = r.getCentreY();
    g.drawHorizontalLine (zeroY, (float) r.getX(), (float) r.getRight());
    for (int i = 1; i < 10; ++i) {
        int gx = r.getX() + (r.getWidth() * i) / 10;
        g.drawVerticalLine (gx, (float) r.getY(), (float) r.getBottom());
    }
    if (engine.isLoading()) { g.setColour (juce::Colours::white.withAlpha (0.7f)); g.drawFittedText ("Loading...", r, juce::Justification::centred, 1); return; }
    if (wf.empty()) {
        // Show large logo in the drop area; disappears once audio is loaded
        if (appLogoDrawable || appLogo.isValid()) {
            const int marginX = juce::jmax (8, r.getWidth() / 40);
            const int marginY = juce::jmax (8, r.getHeight() / 20);
            auto area = r.reduced (marginX, marginY).toFloat();
            if (appLogoDrawable) {
                // Vector logo scales perfectly; fill up to ~96% of area
                auto target = area.reduced (area.getWidth() * 0.02f, area.getHeight() * 0.02f);
                appLogoDrawable->drawWithin (g, target, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
            } else {
                auto logo = appLogo;
                const float aspect = (float) logo.getWidth() / (float) logo.getHeight();
                const float maxW = area.getWidth() * 0.96f;
                const float maxH = area.getHeight() * 0.96f;
                juce::Rectangle<float> fitted;
                if (maxW / maxH > aspect)
                    fitted = juce::Rectangle<float> (maxH * aspect, maxH).withCentre (area.getCentre());
                else
                    fitted = juce::Rectangle<float> (maxW, maxW / aspect).withCentre (area.getCentre());
                g.drawImage (logo, fitted);
            }
            // Helper text under the logo
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.setFont (juce::Font (18.0f, juce::Font::bold));
            auto textArea = juce::Rectangle<int> (r.getX(), r.getBottom() - 24, r.getWidth(), 24);
            g.drawFittedText ("Drop audio here...", textArea, juce::Justification::centredTop, 1);
        } else {
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.setFont (juce::Font (20.0f, juce::Font::bold));
            g.drawFittedText ("Drop audio here...", r, juce::Justification::centred, 1);
        }
        return;
    }
    const int N = (int) wf.size();
    int startBin = 0, endBin = N;
    if (zoom > 1.0f) {
        int visible = juce::jmax (1, (int) std::round ((float) N / zoom));
        startBin = juce::jlimit (0, juce::jmax (0, N - 1), (int) std::round (offset * (N - visible)));
        endBin = juce::jlimit (startBin + 1, N, startBin + visible);
    }
    int binsShown = juce::jmax (1, endBin - startBin);
    // Time ruler ticks (seconds) across current view
    {
        const auto& pool = engine.getPool();
        double sr = juce::jmax (1.0, pool.getSampleRate());
        int totalSamples = pool.getBuffer().getNumSamples();
        if (totalSamples > 0) {
            double totalSec = (double) totalSamples / sr;
            // Visible window start/end in seconds
            double visStartNorm = (double) startBin / (double) N;
            double visWidthNorm = (double) binsShown / (double) N;
            double aSec = visStartNorm * totalSec;
            double bSec = (visStartNorm + visWidthNorm) * totalSec;
            // Choose tick spacing: 0.1,0.2,0.5,1,2,5,10...
            double targetPx = 80.0; // desired spacing in pixels
            double secPerPx = (bSec - aSec) / (double) r.getWidth();
            double raw = targetPx * secPerPx;
            double pow10 = std::pow (10.0, std::floor (std::log10 (raw)));
            double candidates[3] = { 1.0, 2.0, 5.0 };
            double tick = pow10 * candidates[0];
            for (double c : candidates) {
                double t = pow10 * c;
                if (t >= raw) { tick = t; break; }
            }
            int first = (int) std::ceil (aSec / tick);
            g.setColour (juce::Colours::white.withAlpha (0.20f));
            g.setFont (juce::Font (12.0f));
            for (int i = first;; ++i) {
                double tSec = i * tick;
                if (tSec > bSec + 1e-6) break;
                double norm = (tSec - aSec) / (bSec - aSec);
                int gx = r.getX() + (int) std::round (norm * r.getWidth());
                // Tick and label at the top
                g.drawVerticalLine (gx, (float) r.getY(), (float) r.getY() + 6.0f);
                juce::String label = juce::String (tSec, tSec < 10.0 ? 2 : 1) + "s";
                g.drawFittedText (label, juce::Rectangle<int> (gx + 3, r.getY() + 2, 50, 14), juce::Justification::left, 1);
            }
        }
    }
    // draw waveform min/max bars in visible range
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    for (int bi = startBin; bi < endBin; ++bi) {
        float t = (float) (bi - startBin) / (float) binsShown;
        int x = r.getX() + (int) std::round (t * (float) r.getWidth());
        float mn = wf[(size_t) bi].first; float mx = wf[(size_t) bi].second;
        int y1 = r.getCentreY() - (int) (mx * (r.getHeight() * 0.45f));
        int y2 = r.getCentreY() + (int) (mn * (r.getHeight() * 0.45f));
        g.drawVerticalLine (x, (float) y1, (float) y2);
    }
    const auto& slices = engine.getSlices(); g.setColour (juce::Colours::orange.withAlpha (0.8f));
    const auto& poolBuf = engine.getPool().getBuffer(); int totalSamples = poolBuf.getNumSamples();
    if (totalSamples > 0) {
        for (const auto& s : slices) {
            float global = s.startSample / (float) totalSamples;
            // map to visible 0..1
            float visStart = (float) startBin / (float) N;
            float visWidth = (float) binsShown / (float) N;
            float local = visWidth > 0.0f ? (global - visStart) / visWidth : global;
            if (local >= 0.0f && local <= 1.0f) {
                int x = r.getX() + (int) std::round (local * (float) r.getWidth());
                g.drawLine ((float) x, (float) r.getY(), (float) x, (float) r.getBottom(), 1.2f);
            }
        }
    }
    // Draw small draggable handles on boundaries (skip first boundary at 0)
    const int handleW = 12, handleH = 16;
    const int handleTop = r.getY() + 8;
    for (int i = 1; i < (int) slices.size(); ++i) {
        float global = (totalSamples > 0) ? (slices[(size_t) i].startSample / (float) totalSamples) : 0.0f;
        float local = ((float) binsShown / (float) N) > 0.0f ? (global - ((float) startBin / (float) N)) / ((float) binsShown / (float) N) : global;
        if (local < 0.0f || local > 1.0f) continue;
        int cx = r.getX() + (int) std::round (local * (float) r.getWidth());
        juce::Rectangle<int> hRect (cx - handleW/2, handleTop, handleW, handleH);
        auto colour = (i == hoverBoundaryIndex || i == draggingBoundaryIndex) ? juce::Colours::orange.brighter (0.2f) : juce::Colours::darkgrey;
        g.setColour (colour);
        g.fillRoundedRectangle (hRect.toFloat(), 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (hRect.toFloat(), 2.0f, 1.0f);
        if (i == hoverBoundaryIndex) {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawFittedText (juce::String (i), hRect.expanded (8, 2).withY (hRect.getY() - 16), juce::Justification::centredTop, 1);
        }
    }
    // Draw loop region
    auto loopNorm = engine.getLoopRegionNorm();
    float visStart = (float) startBin / (float) N;
    float visWidth = (float) binsShown / (float) N;
    float aVis = visWidth > 0.0f ? (loopNorm.first - visStart) / visWidth : loopNorm.first;
    float bVis = visWidth > 0.0f ? (loopNorm.second - visStart) / visWidth : loopNorm.second;
    int lx1 = r.getX() + (int) std::round (juce::jlimit (0.0f, 1.0f, aVis) * r.getWidth());
    int lx2 = r.getX() + (int) std::round (juce::jlimit (0.0f, 1.0f, bVis) * r.getWidth());
    if (lx2 > lx1) {
        g.setColour (juce::Colours::yellow.withAlpha (0.12f));
        g.fillRect (juce::Rectangle<int> (lx1, r.getY(), lx2 - lx1, r.getHeight()));
        g.setColour (juce::Colours::yellow.withAlpha (0.7f));
        g.drawLine ((float) lx1, (float) r.getY(), (float) lx1, (float) r.getBottom(), 1.2f);
        g.drawLine ((float) lx2, (float) r.getY(), (float) lx2, (float) r.getBottom(), 1.2f);
    }
    // Draw preview playhead with glow
    g.setColour (juce::Colours::aqua.withAlpha (0.9f));
    float n = engine.getPreviewPositionNorm();
    float nVis = visWidth > 0.0f ? (n - visStart) / visWidth : n;
    int x = r.getX() + (int) std::round (juce::jlimit (0.0f, 1.0f, nVis) * (float) r.getWidth());
    g.drawLine ((float) x, (float) r.getY(), (float) x, (float) r.getBottom(), 1.2f);
    g.setColour (juce::Colours::aqua.withAlpha (0.25f));
    g.drawLine ((float) (x - 1), (float) r.getY(), (float) (x - 1), (float) r.getBottom(), 1.0f);
    g.drawLine ((float) (x + 1), (float) r.getY(), (float) (x + 1), (float) r.getBottom(), 1.0f);

    // Highlight hovered boundary (thicker orange line)
    if (hoverBoundaryIndex >= 1 && totalSamples > 0 && hoverBoundaryIndex < (int) slices.size()) {
        float global = slices[(size_t) hoverBoundaryIndex].startSample / (float) totalSamples;
        float local = visWidth > 0.0f ? (global - visStart) / visWidth : global;
        if (local >= 0.0f && local <= 1.0f) {
            int hx = r.getX() + (int) std::round (local * (float) r.getWidth());
            g.setColour (juce::Colours::orange);
            g.drawLine ((float) hx, (float) r.getY(), (float) hx, (float) r.getBottom(), 2.5f);
        }
    }
}

bool NoobToolsAudioProcessorEditor::keyPressed (const juce::KeyPress& key) {
    if (key == juce::KeyPress::spaceKey) { processor.getEngine().togglePreview(); return true; }
    if (key.getModifiers().isCommandDown() && (key.getTextCharacter() == 'z' || key.getTextCharacter() == 'Z') && ! key.getModifiers().isShiftDown()) { if (processor.getEngine().undo()) { repaint(); } return true; }
    if ((key.getModifiers().isCommandDown() && (key.getTextCharacter() == 'y' || key.getTextCharacter() == 'Y')) ||
        (key.getModifiers().isCommandDown() && (key.getTextCharacter() == 'z' || key.getTextCharacter() == 'Z') && key.getModifiers().isShiftDown()))
    { if (processor.getEngine().redo()) { repaint(); } return true; }
    if (key.getTextCharacter() == '+' || key.getTextCharacter() == '=') { zoom = juce::jlimit (1.0f, 64.0f, zoom * 1.25f); repaint(); return true; }
    if (key.getTextCharacter() == '-') { zoom = juce::jlimit (1.0f, 64.0f, zoom / 1.25f); if (zoom <= 1.01f) { zoom = 1.0f; offset = 0.0f; } repaint(); return true; }
    if (key.getKeyCode() == juce::KeyPress::leftKey) { float step = 0.05f / zoom; offset = juce::jlimit (0.0f, 1.0f, offset - step); repaint(); return true; }
    if (key.getKeyCode() == juce::KeyPress::rightKey) { float step = 0.05f / zoom; offset = juce::jlimit (0.0f, 1.0f, offset + step); repaint(); return true; }
    if (key.getTextCharacter() == 't' || key.getTextCharacter() == 'T') { processor.getEngine().tapSliceAtCurrent(); return true; }
    // Trigger slices from keyboard (maps to indices 0..15)
    {
        const juce::String map = "1234567890qwerty"; // 16 keys
        juce::juce_wchar ch = key.getTextCharacter();
        int idx = map.indexOfChar (ch);
        if (idx >= 0) {
            auto* proc = dynamic_cast<NoobToolsAudioProcessor*>(getAudioProcessor());
            if (proc != nullptr) {
                int base = (int) proc->getAPVTS().getRawParameterValue("basenote")->load();
                juce::MidiMessage m = juce::MidiMessage::noteOn (1, base + idx, (juce::uint8) 100);
                juce::MidiBuffer buf; buf.addEvent (m, 0);
                juce::AudioBuffer<float> scratch; scratch.setSize (2, 128, false, false, true);
                proc->processBlock (scratch, buf);
            }
            return true;
        }
    }
    // Pad keyboard mapping for edit/play
    const juce::String keyMap = "1234567890qwerty";
    juce::juce_wchar ch = key.getTextCharacter();
    int index = keyMap.indexOfChar (ch);
    if (index >= 0 && index < 16) {
        int midiNote = 36 + index;
        if (editMode) {
            processor.getEngine().createUserSliceAtCurrent (midiNote, btnQuantize.getToggleState());
        } else {
            juce::MidiMessage m = juce::MidiMessage::noteOn (1, midiNote, (juce::uint8) 100); juce::MidiBuffer buf; buf.addEvent (m, 0);
            auto* proc = dynamic_cast<NoobToolsAudioProcessor*>(getAudioProcessor());
            if (proc != nullptr) { juce::AudioBuffer<float> scratch; scratch.setSize (2, 128, false, false, true); proc->processBlock (scratch, buf); }
        }
        return true;
    }
    return false;
}

void NoobToolsAudioProcessorEditor::mouseDown (const juce::MouseEvent& e) {
    if (lastWaveRect.contains (e.getPosition())) {
        lastMouseX = e.x;
        float n = (e.x - lastWaveRect.getX()) / (float) lastWaveRect.getWidth();
        n = juce::jlimit (0.0f, 1.0f, n);
        processor.getEngine().setPreviewPositionNorm (n);
        if (hoverBoundaryIndex >= 1) {
            if (e.mods.isCtrlDown()) {
                if (processor.getEngine().deleteSlice (hoverBoundaryIndex)) { repaint(); return; }
            }
            draggingBoundaryIndex = hoverBoundaryIndex;
        } else {
            draggingLoop = true; dragStartNorm = dragEndNorm = n;
        }
        repaint();
    }
}

void NoobToolsAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e) {
    if (draggingLoop && lastWaveRect.contains (e.getPosition())) {
        float n = (e.x - lastWaveRect.getX()) / (float) lastWaveRect.getWidth();
        n = juce::jlimit (0.0f, 1.0f, n);
        dragEndNorm = n; repaint();
    }
    if (draggingBoundaryIndex >= 1 && lastWaveRect.contains (e.getPosition())) {
        auto& engine = processor.getEngine();
        const auto& wf = engine.getWaveform().get();
        const int N = (int) wf.size();
        const int totalSamples = engine.getPool().getBuffer().getNumSamples();
        float visStart = 0.0f, visWidth = 1.0f;
        if (zoom > 1.0f && N > 0) {
            int visible = juce::jmax (1, (int) std::round ((float) N / zoom));
            int startBin = juce::jlimit (0, juce::jmax (0, N - 1), (int) std::round (offset * (N - visible)));
            visStart = (float) startBin / (float) N;
            visWidth = (float) visible / (float) N;
        }
        float local = juce::jlimit (0.0f, 1.0f, (e.x - lastWaveRect.getX()) / (float) lastWaveRect.getWidth());
        float global = juce::jlimit (0.0f, 1.0f, visStart + local * visWidth);
        int sample = (int) std::round (global * (float) juce::jmax (0, totalSamples));
        if (processor.getEngine().moveBoundary (draggingBoundaryIndex, sample))
            repaint();
    }
}

void NoobToolsAudioProcessorEditor::mouseUp (const juce::MouseEvent& e) {
    if (draggingBoundaryIndex >= 1) { draggingBoundaryIndex = -1; repaint(); return; }
    if (draggingLoop) {
        draggingLoop = false;
        // If the drag distance is small, treat as click only (don’t change loop region)
        if (std::abs (dragEndNorm - dragStartNorm) > 0.005f) {
            float a = juce::jmin (dragStartNorm, dragEndNorm);
            float b = juce::jmax (dragStartNorm, dragEndNorm);
            if (btnSnap.getToggleState()) {
                // Snap to nearest slice boundaries
                const auto& slices = processor.getEngine().getSlices();
                const int total = processor.getEngine().getTotalLengthSamples();
                auto snapToNearest = [&slices](int samp){
                    if (slices.empty()) return 0;
                    // binary search on startSample
                    int lo = 0, hi = (int) slices.size();
                    while (lo < hi) {
                        int mid = (lo + hi) / 2;
                        if (slices[(size_t) mid].startSample < samp) lo = mid + 1; else hi = mid;
                    }
                    int i1 = juce::jlimit (0, (int) slices.size()-1, lo);
                    int i0 = juce::jlimit (0, (int) slices.size()-1, lo-1);
                    int d0 = std::abs (slices[(size_t) i0].startSample - samp);
                    int d1 = std::abs (slices[(size_t) i1].startSample - samp);
                    return d0 <= d1 ? slices[(size_t) i0].startSample : slices[(size_t) i1].startSample;
                };
                int aS = juce::jlimit (0, total, (int) std::round (a * total));
                int bS = juce::jlimit (0, total, (int) std::round (b * total));
                int sa = snapToNearest (aS);
                int sb = snapToNearest (bS);
                if (sb <= sa) {
                    // pick next start if available
                    for (const auto& s : slices) { if (s.startSample > sa) { sb = s.startSample; break; } }
                    if (sb <= sa) sb = total; // fallback
                }
                a = sa / (float) total; b = sb / (float) total;
            }
            processor.getEngine().setLoopRegionNorm (a, b);
            btnLoop.setToggleState (true, juce::dontSendNotification);
            processor.getEngine().setLoopPreview (true);
        }
        repaint();
    }
}

void NoobToolsAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e) {
    if (lastWaveRect.contains (e.getPosition())) {
        processor.getEngine().setLoopRegionNorm (0.f, 1.f);
        btnLoop.setToggleState (false, juce::dontSendNotification);
        processor.getEngine().setLoopPreview (false);
        repaint();
    }
}

void NoobToolsAudioProcessorEditor::mouseMove (const juce::MouseEvent& e) {
    lastMouseX = e.x;
    hoverBoundaryIndex = -1;
    if (! lastWaveRect.contains (e.getPosition())) { setMouseCursor (juce::MouseCursor::NormalCursor); return; }
    auto& engine = processor.getEngine();
    const auto& wf = engine.getWaveform().get();
    const int N = (int) wf.size();
    const int totalSamples = engine.getPool().getBuffer().getNumSamples();
    float visStart = 0.0f, visWidth = 1.0f;
    if (zoom > 1.0f && N > 0) {
        int visible = juce::jmax (1, (int) std::round ((float) N / zoom));
        int startBin = juce::jlimit (0, juce::jmax (0, N - 1), (int) std::round (offset * (N - visible)));
        visStart = (float) startBin / (float) N;
        visWidth = (float) visible / (float) N;
    }
    const auto& slices = engine.getSlices();
    int bestIdx = -1; int bestDist = 9999;
    const int thresholdPx = 8;
    const int handleW = 8, handleH = 12; const int handleTop = lastWaveRect.getY() + 2;
    for (int i = 1; i < (int) slices.size(); ++i) {
        float global = (totalSamples > 0) ? (slices[(size_t) i].startSample / (float) totalSamples) : 0.0f;
        float local = visWidth > 0.0f ? (global - visStart) / visWidth : global;
        if (local < 0.0f || local > 1.0f) continue;
        int cx = lastWaveRect.getX() + (int) std::round (local * (float) lastWaveRect.getWidth());
        juce::Rectangle<int> hRect (cx - handleW/2, handleTop, handleW, handleH);
        if (hRect.contains (e.getPosition())) { bestIdx = i; break; }
        int d = std::abs (e.x - cx);
        if (d < bestDist && d <= thresholdPx) { bestDist = d; bestIdx = i; }
    }
    hoverBoundaryIndex = bestIdx;
    setMouseCursor (hoverBoundaryIndex >= 1 ? juce::MouseCursor::LeftRightResizeCursor
                                            : juce::MouseCursor::NormalCursor);
    if (bestIdx != -1) repaint (lastWaveRect);
}

bool NoobToolsAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files) {
    for (auto& f : files)
        if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".aiff") || f.endsWithIgnoreCase(".mp3") || f.endsWithIgnoreCase(".flac"))
            return true;
    return false;
}

void NoobToolsAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int) {
    for (auto& path : files) {
        juce::File f (path);
        if (f.existsAsFile()) {
            if (processor.getEngine().loadFileAsync (f))
                break;
        }
    }
}
