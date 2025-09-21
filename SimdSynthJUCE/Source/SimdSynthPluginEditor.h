//
// Created by Jay Vaughan on 14.09.25.
//

#ifndef SIMDSYNTH_PLUGINEDITOR_H
#define SIMDSYNTH_PLUGINEDITOR_H
#pragma once

#include <JuceHeader.h>
#include "SimdSynthPluginProcessor.h"

//==============================================================================
class SimdSynthEditor : public juce::AudioProcessorEditor
{
public:
    SimdSynthEditor(SimdSynthProcessor&);
    ~SimdSynthEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // Reference to the processor
    SimdSynthProcessor& processor;

    // Sliders for parameters
    juce::Slider attackSlider;
    juce::Slider decaySlider;
    juce::Slider resonanceSlider;
    juce::Slider cutoffSlider;
    juce::ToggleButton demoModeToggle;

    // Labels for sliders
    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label resonanceLabel;
    juce::Label cutoffLabel;
    juce::Label demoModeLabel;

    // Parameter attachments for thread-safe updates
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> demoModeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimdSynthEditor)
};

//==============================================================================
SimdSynthEditor::SimdSynthEditor(SimdSynthProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // Set editor size
    setSize(400, 300);

    // Configure attack slider
    attackSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(attackSlider);
    attackLabel.setText("Attack (s)", juce::dontSendNotification);
    addAndMakeVisible(attackLabel);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "attack", attackSlider);

    // Configure decay slider
    decaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    decaySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(decaySlider);
    decayLabel.setText("Decay (s)", juce::dontSendNotification);
    addAndMakeVisible(decayLabel);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "decay", decaySlider);

    // Configure resonance slider
    resonanceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    resonanceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(resonanceSlider);
    resonanceLabel.setText("Resonance", juce::dontSendNotification);
    addAndMakeVisible(resonanceLabel);
    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "resonance", resonanceSlider);

    // Configure cutoff slider
    cutoffSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    cutoffSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(cutoffSlider);
    cutoffLabel.setText("Cutoff (Hz)", juce::dontSendNotification);
    addAndMakeVisible(cutoffLabel);
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "cutoff", cutoffSlider);

    // Configure demo mode toggle
    demoModeToggle.setButtonText("Demo Mode");
    addAndMakeVisible(demoModeToggle);
    demoModeLabel.setText("Demo Mode", juce::dontSendNotification);
    addAndMakeVisible(demoModeLabel);
    demoModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters, "demoMode", demoModeToggle);
}

SimdSynthEditor::~SimdSynthEditor()
{
}

void SimdSynthEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawText("SIMD Synthesizer", getLocalBounds(), juce::Justification::centredTop, true);
}

void SimdSynthEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    int sliderHeight = 40;
    int labelHeight = 20;

    // Position attack slider and label
    attackLabel.setBounds(area.removeFromTop(labelHeight));
    attackSlider.setBounds(area.removeFromTop(sliderHeight));

    // Position decay slider and label
    decayLabel.setBounds(area.removeFromTop(labelHeight));
    decaySlider.setBounds(area.removeFromTop(sliderHeight));

    // Position resonance slider and label
    resonanceLabel.setBounds(area.removeFromTop(labelHeight));
    resonanceSlider.setBounds(area.removeFromTop(sliderHeight));

    // Position cutoff slider and label
    cutoffLabel.setBounds(area.removeFromTop(labelHeight));
    cutoffSlider.setBounds(area.removeFromTop(sliderHeight));

    // Position demo mode toggle and label
    demoModeLabel.setBounds(area.removeFromTop(labelHeight));
    demoModeToggle.setBounds(area.removeFromTop(sliderHeight));
}
#endif //SIMDSYNTH_PLUGINEDITOR_H