#include "PluginProcessor.h"
#include "PluginEditor.h"
SeratoLikeSamplerAudioProcessor::SeratoLikeSamplerAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    , apvts (*this, nullptr, "PARAMS", params::createLayout()) {}
bool SeratoLikeSamplerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
void SeratoLikeSamplerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {
    engine.prepare (sampleRate, samplesPerBlock);
}
void SeratoLikeSamplerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
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
void SeratoLikeSamplerAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {
    auto state = apvts.copyState(); std::unique_ptr<juce::XmlElement> xml (state.createXml()); copyXmlToBinary (*xml, destData);
}
void SeratoLikeSamplerAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}
bool SeratoLikeSamplerAudioProcessor::isInterestedInFileDrag (const juce::StringArray& files) {
    for (auto& f : files)
        if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".aiff") || f.endsWithIgnoreCase(".mp3") || f.endsWithIgnoreCase(".flac"))
            return true;
    return false;
}
void SeratoLikeSamplerAudioProcessor::filesDropped (const juce::StringArray& files, int, int) {
    for (auto& path : files) { juce::File f (path); if (f.existsAsFile()) { if (engine.loadFile (f)) break; } }
}
juce::AudioProcessorEditor* SeratoLikeSamplerAudioProcessor::createEditor() { return new SeratoLikeSamplerAudioProcessorEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SeratoLikeSamplerAudioProcessor(); }
