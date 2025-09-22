#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SimdSynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    SimdSynthAudioProcessorEditor(SimdSynthAudioProcessor& p);
    ~SimdSynthAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void updatePresetComboBox();

private:
    void layoutGroupSliders(juce::GroupComponent* group, const std::vector<juce::Slider*>& sliders);

    SimdSynthAudioProcessor& processor;

    // Preset controls
    std::unique_ptr<juce::ComboBox> presetComboBox;
    std::unique_ptr<juce::TextButton> saveButton;
    std::unique_ptr<juce::TextEditor> presetNameEditor;
    std::unique_ptr<juce::TextButton> confirmButton;
    std::unique_ptr<juce::TextButton> loadButton;

    // Group components
    std::unique_ptr<juce::GroupComponent> oscillatorGroup;
    std::unique_ptr<juce::GroupComponent> ampEnvelopeGroup;
    std::unique_ptr<juce::GroupComponent> filterGroup;
    std::unique_ptr<juce::GroupComponent> filterEnvelopeGroup;
    std::unique_ptr<juce::GroupComponent> lfoGroup;
    std::unique_ptr<juce::GroupComponent> subOscillatorGroup;
    std::unique_ptr<juce::GroupComponent> outputGroup;

    // Sliders
    std::unique_ptr<juce::Slider> wavetableSlider;
    std::unique_ptr<juce::Slider> unisonSlider;
    std::unique_ptr<juce::Slider> detuneSlider;
    std::unique_ptr<juce::Slider> attackSlider;
    std::unique_ptr<juce::Slider> decaySlider;
    std::unique_ptr<juce::Slider> sustainSlider;
    std::unique_ptr<juce::Slider> releaseSlider;
    std::unique_ptr<juce::Slider> cutoffSlider;
    std::unique_ptr<juce::Slider> resonanceSlider;
    std::unique_ptr<juce::Slider> fegAttackSlider;
    std::unique_ptr<juce::Slider> fegDecaySlider;
    std::unique_ptr<juce::Slider> fegSustainSlider;
    std::unique_ptr<juce::Slider> fegReleaseSlider;
    std::unique_ptr<juce::Slider> fegAmountSlider;
    std::unique_ptr<juce::Slider> lfoRateSlider;
    std::unique_ptr<juce::Slider> lfoDepthSlider;
    std::unique_ptr<juce::Slider> subTuneSlider;
    std::unique_ptr<juce::Slider> subMixSlider;
    std::unique_ptr<juce::Slider> subTrackSlider;
    std::unique_ptr<juce::Slider> gainSlider;

    // Slider attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wavetableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> unisonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> detuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fegAttackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fegDecayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fegSustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fegReleaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fegAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subTuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subTrackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimdSynthAudioProcessorEditor)
};