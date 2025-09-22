/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 8 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SimdSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::ComboBox::Listener {
public:
    explicit SimdSynthAudioProcessorEditor(SimdSynthAudioProcessor&);
    ~SimdSynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void updatePresetComboBox();

private:
    SimdSynthAudioProcessor& processor;

    // Preset combo box
    juce::ComboBox presetComboBox;

    // Sliders and labels
    juce::Slider wavetableSlider, attackSlider, decaySlider, sustainSlider, releaseSlider,
                 cutoffSlider, resonanceSlider, fegAttackSlider, fegDecaySlider,
                 fegSustainSlider, fegReleaseSlider, fegAmountSlider,
                 lfoRateSlider, lfoDepthSlider, subTuneSlider, subMixSlider,
                 subTrackSlider, gainSlider, unisonSlider, detuneSlider;

    juce::Label wavetableLabel, attackLabel, decayLabel, sustainLabel, releaseLabel,
                cutoffLabel, resonanceLabel, fegAttackLabel, fegDecayLabel,
                fegSustainLabel, fegReleaseLabel, fegAmountLabel,
                lfoRateLabel, lfoDepthLabel, subTuneLabel, subMixLabel,
                subTrackLabel, gainLabel, unisonLabel, detuneLabel;

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
            wavetableAttachment, attackAttachment, decayAttachment, sustainAttachment,
            releaseAttachment, cutoffAttachment, resonanceAttachment,
            fegAttackAttachment, fegDecayAttachment, fegSustainAttachment,
            fegReleaseAttachment, fegAmountAttachment, lfoRateAttachment,
            lfoDepthAttachment, subTuneAttachment, subMixAttachment,
            subTrackAttachment, gainAttachment, unisonAttachment, detuneAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimdSynthAudioProcessorEditor)
};
