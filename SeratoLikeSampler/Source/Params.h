
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
namespace params {
inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterFloat>("attack","Attack", NormalisableRange<float>(0.001f,0.5f,0,0.3f), 0.01f));
    p.push_back (std::make_unique<AudioParameterFloat>("release","Release", NormalisableRange<float>(0.005f,2.0f,0,0.3f), 0.2f));
    p.push_back (std::make_unique<AudioParameterFloat>("cutoff","Cutoff", NormalisableRange<float>(40.f,18000.f,0,0.25f), 12000.f));
    p.push_back (std::make_unique<AudioParameterFloat>("reso","Reso", NormalisableRange<float>(0.1f,2.0f,0,1.0f), 0.7f));
    p.push_back (std::make_unique<AudioParameterFloat>("gain","Gain", NormalisableRange<float>(-24.f,24.f,0.01f), 0.0f));
    p.push_back (std::make_unique<AudioParameterInt>("basenote","Base Note", 0, 127, 36));
    p.push_back (std::make_unique<AudioParameterInt>("maxslices","Max Slices", 1, 128, 64));
    p.push_back (std::make_unique<AudioParameterFloat>("sensitivity","Sensitivity", NormalisableRange<float>(0.6f, 2.0f, 0, 1.0f), 1.2f));
    p.push_back (std::make_unique<AudioParameterFloat>("mingapms","Min Gap (ms)", NormalisableRange<float>(1.f, 500.f, 1.f), 30.f));
    // Playback behaviour
    p.push_back (std::make_unique<AudioParameterBool>("choke","Choke (Mono)", false));
    p.push_back (std::make_unique<AudioParameterBool>("gate","Gate", false));
    return { p.begin(), p.end() };
  }}
