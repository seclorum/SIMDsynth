/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 8 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

#include "PluginEditor.h"

SimdSynthAudioProcessorEditor::SimdSynthAudioProcessorEditor(SimdSynthAudioProcessor& p)
        : AudioProcessorEditor(&p), processor(p) {
    // Initialize preset combo box
    addAndMakeVisible(presetComboBox);
    presetComboBox.addListener(this);
    updatePresetComboBox(); // Populate combo box
    presetComboBox.setSelectedItemIndex(processor.getCurrentProgram(), juce::dontSendNotification);
    presetComboBox.setTextWhenNothingSelected("Select Preset");

    // Initialize sliders and labels with custom setup function
    auto setupSlider = [](juce::Slider& slider, juce::Label& label, const juce::String& name) {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
    };

    // Setup sliders for existing parameters
    setupSlider(wavetableSlider, wavetableLabel, "Wavetable");
    setupSlider(attackSlider, attackLabel, "Attack (s)");
    setupSlider(decaySlider, decayLabel, "Decay (s)");
    setupSlider(sustainSlider, sustainLabel, "Sustain"); // New: Sustain control
    setupSlider(releaseSlider, releaseLabel, "Release (s)"); // New: Release control
    setupSlider(cutoffSlider, cutoffLabel, "Cutoff (Hz)");
    setupSlider(resonanceSlider, resonanceLabel, "Resonance");
    setupSlider(fegAttackSlider, fegAttackLabel, "FEG Attack (s)");
    setupSlider(fegDecaySlider, fegDecayLabel, "FEG Decay (s)");
    setupSlider(fegSustainSlider, fegSustainLabel, "FEG Sustain");
    setupSlider(fegReleaseSlider, fegReleaseLabel, "FEG Release (s)");
    setupSlider(fegAmountSlider, fegAmountLabel, "FEG Amount"); // New: Filter envelope amount
    setupSlider(lfoRateSlider, lfoRateLabel, "LFO Rate (Hz)");
    setupSlider(lfoDepthSlider, lfoDepthLabel, "LFO Depth");
    setupSlider(subTuneSlider, subTuneLabel, "Sub Tune (st)");
    setupSlider(subMixSlider, subMixLabel, "Sub Mix");
    setupSlider(subTrackSlider, subTrackLabel, "Sub Track");
    setupSlider(gainSlider, gainLabel, "Gain"); // New: Output gain control
    setupSlider(unisonSlider, unisonLabel, "Unison Voices"); // New: Unison voices
    setupSlider(detuneSlider, detuneLabel, "Unison Detune"); // New: Unison detune

    // Customize wavetable slider for discrete waveform selection
    wavetableSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    wavetableSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    wavetableSlider.setRange(0.0, 2.0, 1.0); // Discrete steps for sine (0), saw (1), square (2)
    wavetableSlider.setTextValueSuffix(""); // Remove numerical suffix
    wavetableSlider.textFromValueFunction = [](double value) {
        int val = static_cast<int>(value);
        switch (val) {
            case 0: return "Sine";
            case 1: return "Saw";
            case 2: return "Square";
            default: return "Unknown";
        }
    };
    wavetableSlider.valueFromTextFunction = [](const juce::String& text) {
        if (text == "Sine") return 0.0;
        if (text == "Saw") return 1.0;
        if (text == "Square") return 2.0;
        return 0.0;
    };

    // Customize unison slider for integer steps
    unisonSlider.setRange(1.0, 8.0, 1.0); // Integer steps for 1 to 8 voices
    unisonSlider.setTextValueSuffix(" voices");

    // Add sliders and labels to the editor
    addAndMakeVisible(wavetableSlider); addAndMakeVisible(wavetableLabel);
    addAndMakeVisible(attackSlider); addAndMakeVisible(attackLabel);
    addAndMakeVisible(decaySlider); addAndMakeVisible(decayLabel);
    addAndMakeVisible(sustainSlider); addAndMakeVisible(sustainLabel); // New
    addAndMakeVisible(releaseSlider); addAndMakeVisible(releaseLabel); // New
    addAndMakeVisible(cutoffSlider); addAndMakeVisible(cutoffLabel);
    addAndMakeVisible(resonanceSlider); addAndMakeVisible(resonanceLabel);
    addAndMakeVisible(fegAttackSlider); addAndMakeVisible(fegAttackLabel);
    addAndMakeVisible(fegDecaySlider); addAndMakeVisible(fegDecayLabel);
    addAndMakeVisible(fegSustainSlider); addAndMakeVisible(fegSustainLabel);
    addAndMakeVisible(fegReleaseSlider); addAndMakeVisible(fegReleaseLabel);
    addAndMakeVisible(fegAmountSlider); addAndMakeVisible(fegAmountLabel); // New
    addAndMakeVisible(lfoRateSlider); addAndMakeVisible(lfoRateLabel);
    addAndMakeVisible(lfoDepthSlider); addAndMakeVisible(lfoDepthLabel);
    addAndMakeVisible(subTuneSlider); addAndMakeVisible(subTuneLabel);
    addAndMakeVisible(subMixSlider); addAndMakeVisible(subMixLabel);
    addAndMakeVisible(subTrackSlider); addAndMakeVisible(subTrackLabel);
    addAndMakeVisible(gainSlider); addAndMakeVisible(gainLabel); // New
    addAndMakeVisible(unisonSlider); addAndMakeVisible(unisonLabel); // New
    addAndMakeVisible(detuneSlider); addAndMakeVisible(detuneLabel); // New

    // Attach sliders to parameters
    wavetableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "wavetable", wavetableSlider);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "attack", attackSlider);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "decay", decaySlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "sustain", sustainSlider); // New
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "release", releaseSlider); // New
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
    fegAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "fegAmount", fegAmountSlider); // New
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
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "gain", gainSlider); // New
    unisonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "unison", unisonSlider); // New
    detuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.getParameters(), "detune", detuneSlider); // New

    // Set editor size (increased height to accommodate new controls)
    setSize(600, 1400);
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
    bounds.removeFromTop(spacing);

    // Place sliders and labels in a vertical layout
    wavetableLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    wavetableSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    attackLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    attackSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    decayLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth));
    decaySlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth));
    bounds.removeFromTop(spacing);

    sustainLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth)); // New
    sustainSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth)); // New
    bounds.removeFromTop(spacing);

    releaseLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth)); // New
    releaseSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth)); // New
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

    fegAmountLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth)); // New
    fegAmountSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth)); // New
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
    bounds.removeFromTop(spacing);

    gainLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth)); // New
    gainSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth)); // New
    bounds.removeFromTop(spacing);

    unisonLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth)); // New
    unisonSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth)); // New
    bounds.removeFromTop(spacing);

    detuneLabel.setBounds(bounds.removeFromTop(sliderHeight).removeFromLeft(labelWidth)); // New
    detuneSlider.setBounds(bounds.removeFromTop(sliderHeight).removeFromRight(sliderWidth)); // New
}

void SimdSynthAudioProcessorEditor::updatePresetComboBox() {
    presetComboBox.clear(juce::dontSendNotification);
    for (int i = 0; i < processor.getNumPrograms(); ++i) {
        presetComboBox.addItem(processor.getProgramName(i), i + 1);
    }
    int selectedId = processor.getCurrentProgram() + 1;
    presetComboBox.setSelectedId(selectedId, juce::dontSendNotification);
    presetComboBox.setTextWhenNothingSelected("Select Preset");
    DBG("ComboBox updated: selectedId=" << selectedId << ", preset=" << presetComboBox.getText());
}

void SimdSynthAudioProcessorEditor::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) {
    if (comboBoxThatHasChanged == &presetComboBox) {
        int selectedId = presetComboBox.getSelectedId();
        if (selectedId > 0) {
            processor.setCurrentProgram(selectedId - 1);
            // Removed redundant updatePresetComboBox() call to prevent loop
        }
    }
}
