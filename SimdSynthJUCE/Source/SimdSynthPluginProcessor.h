#pragma once

#include <JuceHeader.h>
#include "SimdSynthCore.h" // Include the provided simdsynth code

//==============================================================================
class SimdSynthPluginProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    SimdSynthPluginProcessor();
    ~SimdSynthPluginProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

juce::AudioProcessorValueTreeState parameters;

private:
    //==============================================================================
    // Synthesizer and voice management
    juce::Synthesiser synth;
    Filter filter;
    std::vector<Chord> chords;
    bool demoMode = false;
    float demoTime = 0.0f;
    size_t demoIndex = 0;

    // Audio parameters for UI control
    float attackParam = 0.0f;
    float decayParam = 0.0f;
    float resonanceParam = 0.0f;
    float cutoffParam = 0.0f;
    float demoModeParam = 0.0f;

    // Helper to initialize chords (from main())
    void initializeChords();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimdSynthPluginProcessor)
};