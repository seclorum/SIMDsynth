//
// Created by Jay Vaughan on 22.09.25.
//

// PluginEditor.cpp
#include "PluginEditor.h"

SimdSynthAudioProcessorEditor::SimdSynthAudioProcessorEditor(SimdSynthAudioProcessor& p)
        : AudioProcessorEditor(&p), processor(p) {
    // Initialize preset combo box
    addAndMakeVisible(presetComboBox);
    presetComboBox.addListener(this);
    for (int i = 0; i < processor.getNumPrograms(); ++i) {
        presetComboBox.addItem(processor.getProgramName(i), i + 1);
    }
    presetComboBox.setSelectedItemIndex(processor.getCurrentProgram(), juce::dontSendNotification);
    presetComboBox.setTextWhenNothingSelected("Select Preset");

    // Initialize sliders and labels
    auto setupSlider = [](juce::Slider& slider, juce::Label& label, const juce::String& name) {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
    };

    setupSlider(wavetableSlider, wavetableLabel, "Wavetable");
    setupSlider(attackSlider, attackLabel, "Attack");
    setupSlider(decaySlider, decayLabel, "Decay");
    setupSlider(cutoffSlider, cutoffLabel, "Cutoff");
    setupSlider(resonanceSlider, resonanceLabel, "Resonance");
    setupSlider(fegAttackSlider, fegAttackLabel, "FEG Attack");
    setupSlider(fegDecaySlider, fegDecayLabel, "FEG Decay");
    setupSlider(fegSustainSlider, fegSustainLabel, "FEG Sustain");
    setupSlider(fegReleaseSlider, fegReleaseLabel, "FEG Release");
    setupSlider(lfoRateSlider, lfoRateLabel, "LFO Rate");
    setupSlider(lfoDepthSlider, lfoDepthLabel, "LFO Depth");
    setupSlider(subTuneSlider, subTuneLabel, "Sub Tune");
    setupSlider(subMixSlider, subMixLabel, "Sub Mix");
    setupSlider(subTrackSlider, subTrackLabel, "Sub Track");

    // Add sliders and labels to the editor
    addAndMakeVisible(wavetableSlider); addAndMakeVisible(wavetableLabel);
    addAndMakeVisible(attackSlider); addAndMakeVisible(attackLabel);
    addAndMakeVisible(decaySlider); addAndMakeVisible(decayLabel);
    addAndMakeVisible(cutoffSlider); addAndMakeVisible(cutoffLabel);
    addAndMakeVisible(resonanceSlider); addAndMakeVisible(resonanceLabel);
    addAndMakeVisible(fegAttackSlider); addAndMakeVisible(fegAttackLabel);
    addAndMakeVisible(fegDecaySlider); addAndMakeVisible(fegDecayLabel);
    addAndMakeVisible(fegSustainSlider); addAndMakeVisible(fegSustainLabel);
    addAndMakeVisible(fegReleaseSlider); addAndMakeVisible(fegReleaseLabel);
    addAndMakeVisible(lfoRateSlider); addAndMakeVisible(lfoRateLabel);
    addAndMakeVisible(lfoDepthSlider); addAndMakeVisible(lfoDepthLabel);
    addAndMakeVisible(subTuneSlider); addAndMakeVisible(subTuneLabel);
    addAndMakeVisible(subMixSlider); addAndMakeVisible(subMixLabel);
    addAndMakeVisible(subTrackSlider); addAndMakeVisible(subTrackLabel);

    // Attach sliders to parameters
    wavetableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "wavetable", wavetableSlider);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "attack", attackSlider);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "decay", decaySlider);
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "cutoff", cutoffSlider);
    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "resonance", resonanceSlider);
    fegAttackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "fegAttack", fegAttackSlider);
    fegDecayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "fegDecay", fegDecaySlider);
    fegSustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "fegSustain", fegSustainSlider);
    fegReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "fegRelease", fegReleaseSlider);
    lfoRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "lfoRate", lfoRateSlider);
    lfoDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "lfoDepth", lfoDepthSlider);
    subTuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "subTune", subTuneSlider);
    subMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "subMix", subMixSlider);
    subTrackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "subTrack", subTrackSlider);

    // Set editor size
    setSize(600, 1240);
}

SimdSynthAudioProcessorEditor::~SimdSynthAudioProcessorEditor() {}

void SimdSynthAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("SimdSynth", getLocalBounds().reduced(10).removeFromTop(30),
                     juce::Justification::centred, 1);
}

void SimdSynthAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds().reduced(10);
    const int comboBoxHeight = 30;
    const int sliderHeight = 40;
    const int labelWidth = 100;
    const int sliderWidth = getWidth() - 20 - labelWidth;
    const int spacing = 5;

    // Place preset combo box at the top
    presetComboBox.setBounds(bounds.removeFromTop(comboBoxHeight));

    // Place sliders and labels in a vertical layout
    bounds.removeFromTop(spacing);
    wavetableLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    wavetableSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    attackLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    attackSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    decayLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    decaySlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    cutoffLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    cutoffSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    resonanceLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    resonanceSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    fegAttackLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    fegAttackSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    fegDecayLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    fegDecaySlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    fegSustainLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    fegSustainSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    fegReleaseLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    fegReleaseSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    lfoRateLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    lfoRateSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    lfoDepthLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    lfoDepthSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    subTuneLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    subTuneSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    subMixLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    subMixSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    subTrackLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    subTrackSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
}

void SimdSynthAudioProcessorEditor::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) {
    if (comboBoxThatHasChanged == &presetComboBox) {
        processor.setCurrentProgram(presetComboBox.getSelectedItemIndex());
    }
}