/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 16 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Custom LookAndFeel for improved slider appearance
class CustomLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height, float sliderPos,
                              float rotaryStartAngle, float rotaryEndAngle, juce::Slider &slider) override {
            auto radius = juce::jmin(width, height) * 0.4f;
            auto centreX = x + width * 0.5f;
            auto centreY = y + height * 0.5f;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            // Draw knob background
            g.setColour(juce::Colours::darkgrey);
            g.fillEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2);

            // Draw knob outline
            g.setColour(juce::Colours::white);
            g.drawEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2, 1.0f);

            // Draw pointer
            juce::Path p;
            auto pointerLength = radius * 0.8f;
            auto pointerThickness = 2.0f;
            p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
            p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
            g.setColour(juce::Colours::white);
            g.fillPath(p);
        }
};

class SimdSynthAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::ComboBox::Listener {
    public:
        explicit SimdSynthAudioProcessorEditor(SimdSynthAudioProcessor &p);
        ~SimdSynthAudioProcessorEditor() override;

        void comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged) override;

        void paint(juce::Graphics &g) override;
        void resized() override;
        void updatePresetComboBox();

    private:
        CustomLookAndFeel simdSynthLAF;

        void layoutGroupSliders(juce::GroupComponent *group, const std::vector<juce::Slider *> &sliders);

        SimdSynthAudioProcessor &processor;

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
