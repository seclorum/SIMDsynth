//
// Created by Jay Vaughan on 22.09.25.
//

#ifndef SIMDSYNTH_PLUGINEDITOR_H
#define SIMDSYNTH_PLUGINEDITOR_H
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class SimdSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::ComboBox::Listener {
public:
    SimdSynthAudioProcessorEditor(SimdSynthAudioProcessor&);
    ~SimdSynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void updatePresetComboBox();
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    juce::ComboBox presetComboBox;

private:
    SimdSynthAudioProcessor& processor;


    // Sliders for each parameter
    juce::Slider wavetableSlider;
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider cutoffSlider;
    juce::Slider resonanceSlider;
    juce::Slider fegAttackSlider;
    juce::Slider fegDecaySlider;
    juce::Slider fegSustainSlider;
    juce::Slider fegReleaseSlider;
    juce::Slider lfoRateSlider;
    juce::Slider lfoDepthSlider;
    juce::Slider subTuneSlider;
    juce::Slider subMixSlider;
    juce::Slider subTrackSlider;

    // Labels for sliders
    juce::Label wavetableLabel;
    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label cutoffLabel;
    juce::Label resonanceLabel;
    juce::Label fegAttackLabel;
    juce::Label fegDecayLabel;
    juce::Label fegSustainLabel;
    juce::Label fegReleaseLabel;
    juce::Label lfoRateLabel;
    juce::Label lfoDepthLabel;
    juce::Label subTuneLabel;
    juce::Label subMixLabel;
    juce::Label subTrackLabel;

    // Attachments to link sliders to parameters
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wavetableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fegAttackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fegDecayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fegSustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fegReleaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subTuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subTrackAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimdSynthAudioProcessorEditor)
};
#endif //SIMDSYNTH_PLUGINEDITOR_H
