#include "PluginProcessor.h"
#include "PluginEditor.h"
NoobToolsAudioProcessor::NoobToolsAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    , apvts (*this, nullptr, "PARAMS", params::createLayout()) {}
bool NoobToolsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
void NoobToolsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {
    engine.prepare (sampleRate, samplesPerBlock);
}
void NoobToolsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    auto attack  = apvts.getRawParameterValue("attack")->load();
    auto release = apvts.getRawParameterValue("release")->load();
    auto cutoff  = apvts.getRawParameterValue("cutoff")->load();
    auto reso    = apvts.getRawParameterValue("reso")->load();
    auto gain    = apvts.getRawParameterValue("gain")->load();
    auto baseNote   = (int) apvts.getRawParameterValue("basenote")->load();
    auto maxSlices  = (int) apvts.getRawParameterValue("maxslices")->load();
    auto sensitivity=       apvts.getRawParameterValue("sensitivity")->load();
    auto minGapMs   =       apvts.getRawParameterValue("mingapms")->load();
    engine.setParams (attack, release, cutoff, reso, gain);
    engine.setSliceControls (baseNote, maxSlices, sensitivity);
    engine.setMinGapMs (minGapMs);
    // Playback behaviour
    {
        auto choke = (bool) apvts.getRawParameterValue("choke")->load();
        engine.setChoke (choke);
        auto gate  = (bool) apvts.getRawParameterValue("gate")->load();
        engine.setGate (gate);
    }
    engine.render (buffer, midi);
}
void NoobToolsAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {
    auto state = apvts.copyState(); std::unique_ptr<juce::XmlElement> xml (state.createXml()); copyXmlToBinary (*xml, destData);
}
void NoobToolsAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}
bool NoobToolsAudioProcessor::isInterestedInFileDrag (const juce::StringArray& files) {
    for (auto& f : files)
        if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".aiff") || f.endsWithIgnoreCase(".mp3") || f.endsWithIgnoreCase(".flac"))
            return true;
    return false;
}
void NoobToolsAudioProcessor::filesDropped (const juce::StringArray& files, int, int) {
    for (auto& path : files) { juce::File f (path); if (f.existsAsFile()) { if (engine.loadFile (f)) break; } }
}
juce::AudioProcessorEditor* NoobToolsAudioProcessor::createEditor() { return new NoobToolsAudioProcessorEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NoobToolsAudioProcessor(); }
