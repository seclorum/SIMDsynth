/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 8 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

#include "PluginEditor.h"

SimdSynthAudioProcessorEditor::SimdSynthAudioProcessorEditor(SimdSynthAudioProcessor &p)
    : AudioProcessorEditor(&p), processor(p) {
    // Apply custom LookAndFeel
    setLookAndFeel(&simdSynthLAF);

    // Disable Metal to avoid default.metallib error
    setPaintingIsUnclipped(true);

    // Set resizable and minimum size
    setResizable(true, true);
    getConstrainer()->setMinimumSize(600, 400);

    // Initialize preset controls
    presetComboBox = std::make_unique<juce::ComboBox>("presetComboBox");
    updatePresetComboBox();
    presetComboBox->addListener(this);
    presetComboBox->setSelectedId(1);
    addAndMakeVisible(presetComboBox.get());

    saveButton = std::make_unique<juce::TextButton>("saveButton");
    saveButton->setButtonText("Save");
    saveButton->onClick = [this] {
        auto presetName = presetNameEditor->getText();
        // processor.savePreset(presetName, processor.getParameters());
        updatePresetComboBox();
        DBG("Saved preset: " << presetName);
    };
    addAndMakeVisible(saveButton.get());

    presetNameEditor = std::make_unique<juce::TextEditor>("presetNameEditor");
    presetNameEditor->setText("Strings2");
    addAndMakeVisible(presetNameEditor.get());

    confirmButton = std::make_unique<juce::TextButton>("confirmButton");
    confirmButton->setButtonText("Confirm");
    confirmButton->onClick = [this] {
        auto presetName = presetNameEditor->getText();
        presetComboBox->setText(presetName, juce::dontSendNotification);
        DBG("Confirmed preset name: " << presetName);
    };
    addAndMakeVisible(confirmButton.get());

    loadButton = std::make_unique<juce::TextButton>("loadButton");
    loadButton->setButtonText("Load");
    loadButton->onClick = [this] {
        processor.setCurrentProgram(presetComboBox->getSelectedId() - 1);
        updatePresetComboBox();
        DBG("Loaded preset: " << presetComboBox->getText());
    };
    addAndMakeVisible(loadButton.get());

    // Initialize group components
    oscillatorGroup = std::make_unique<juce::GroupComponent>("oscillatorGroup", "Oscillator");
    addAndMakeVisible(oscillatorGroup.get());

    ampEnvelopeGroup = std::make_unique<juce::GroupComponent>("ampEnvelopeGroup", "Amp Envelope");
    addAndMakeVisible(ampEnvelopeGroup.get());

    filterGroup = std::make_unique<juce::GroupComponent>("filterGroup", "Filter");
    addAndMakeVisible(filterGroup.get());

    filterEnvelopeGroup = std::make_unique<juce::GroupComponent>("filterEnvelopeGroup", "Filter Envelope");
    addAndMakeVisible(filterEnvelopeGroup.get());

    lfoGroup = std::make_unique<juce::GroupComponent>("lfoGroup", "LFO");
    addAndMakeVisible(lfoGroup.get());

    subOscillatorGroup = std::make_unique<juce::GroupComponent>("subOscillatorGroup", "Sub Oscillator");
    addAndMakeVisible(subOscillatorGroup.get());

    outputGroup = std::make_unique<juce::GroupComponent>("outputGroup", "Output");
    addAndMakeVisible(outputGroup.get());

    // Initialize sliders for Oscillator group
    wavetableSlider = std::make_unique<juce::Slider>("wavetableSlider");
    wavetableSlider->setRange(0, 3, 1);
    wavetableSlider->setSliderStyle(juce::Slider::Rotary);
    wavetableSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    oscillatorGroup->addAndMakeVisible(wavetableSlider.get());
    wavetableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "wavetable", *wavetableSlider);

    unisonSlider = std::make_unique<juce::Slider>("unisonSlider");
    unisonSlider->setRange(1, 8, 1);
    unisonSlider->setSliderStyle(juce::Slider::Rotary);
    unisonSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    oscillatorGroup->addAndMakeVisible(unisonSlider.get());
    unisonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                              "unison", *unisonSlider);

    detuneSlider = std::make_unique<juce::Slider>("detuneSlider");
    detuneSlider->setRange(0.0, 0.1, 0.001);
    detuneSlider->setSliderStyle(juce::Slider::Rotary);
    detuneSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    oscillatorGroup->addAndMakeVisible(detuneSlider.get());
    detuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                              "detune", *detuneSlider);

    // Initialize sliders for Amp Envelope group
    attackSlider = std::make_unique<juce::Slider>("attackSlider");
    attackSlider->setRange(0.0, 5.0, 0.01);
    attackSlider->setSliderStyle(juce::Slider::Rotary);
    attackSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    ampEnvelopeGroup->addAndMakeVisible(attackSlider.get());
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                              "attack", *attackSlider);

    decaySlider = std::make_unique<juce::Slider>("decaySlider");
    decaySlider->setRange(0.0, 5.0, 0.01);
    decaySlider->setSliderStyle(juce::Slider::Rotary);
    decaySlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    ampEnvelopeGroup->addAndMakeVisible(decaySlider.get());
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                             "decay", *decaySlider);

    sustainSlider = std::make_unique<juce::Slider>("sustainSlider");
    sustainSlider->setRange(0.0, 1.0, 0.01);
    sustainSlider->setSliderStyle(juce::Slider::Rotary);
    sustainSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    ampEnvelopeGroup->addAndMakeVisible(sustainSlider.get());
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "sustain", *sustainSlider);

    releaseSlider = std::make_unique<juce::Slider>("releaseSlider");
    releaseSlider->setRange(0.0, 5.0, 0.01);
    releaseSlider->setSliderStyle(juce::Slider::Rotary);
    releaseSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    ampEnvelopeGroup->addAndMakeVisible(releaseSlider.get());
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "release", *releaseSlider);

    // Initialize sliders for Filter group
    cutoffSlider = std::make_unique<juce::Slider>("cutoffSlider");
    cutoffSlider->setRange(20.0, 20000.0, 1.0);
    cutoffSlider->setSkewFactorFromMidPoint(1000.0);
    cutoffSlider->setSliderStyle(juce::Slider::Rotary);
    cutoffSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterGroup->addAndMakeVisible(cutoffSlider.get());
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                              "cutoff", *cutoffSlider);

    resonanceSlider = std::make_unique<juce::Slider>("resonanceSlider");
    resonanceSlider->setRange(0.0, 1.0, 0.01);
    resonanceSlider->setSliderStyle(juce::Slider::Rotary);
    resonanceSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterGroup->addAndMakeVisible(resonanceSlider.get());
    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "resonance", *resonanceSlider);

    // Initialize sliders for Filter Envelope group
    fegAttackSlider = std::make_unique<juce::Slider>("fegAttackSlider");
    fegAttackSlider->setRange(0.0, 5.0, 0.01);
    fegAttackSlider->setSliderStyle(juce::Slider::Rotary);
    fegAttackSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterEnvelopeGroup->addAndMakeVisible(fegAttackSlider.get());
    fegAttackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "fegAttack", *fegAttackSlider);

    fegDecaySlider = std::make_unique<juce::Slider>("fegDecaySlider");
    fegDecaySlider->setRange(0.0, 5.0, 0.01);
    fegDecaySlider->setSliderStyle(juce::Slider::Rotary);
    fegDecaySlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterEnvelopeGroup->addAndMakeVisible(fegDecaySlider.get());
    fegDecayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "fegDecay", *fegDecaySlider);

    fegSustainSlider = std::make_unique<juce::Slider>("fegSustainSlider");
    fegSustainSlider->setRange(0.0, 1.0, 0.01);
    fegSustainSlider->setSliderStyle(juce::Slider::Rotary);
    fegSustainSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterEnvelopeGroup->addAndMakeVisible(fegSustainSlider.get());
    fegSustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "fegSustain", *fegSustainSlider);

    fegReleaseSlider = std::make_unique<juce::Slider>("fegReleaseSlider");
    fegReleaseSlider->setRange(0.0, 5.0, 0.01);
    fegReleaseSlider->setSliderStyle(juce::Slider::Rotary);
    fegReleaseSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterEnvelopeGroup->addAndMakeVisible(fegReleaseSlider.get());
    fegReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "fegRelease", *fegReleaseSlider);

    fegAmountSlider = std::make_unique<juce::Slider>("fegAmountSlider");
    fegAmountSlider->setRange(0.0, 1.0, 0.01);
    fegAmountSlider->setSliderStyle(juce::Slider::Rotary);
    fegAmountSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterEnvelopeGroup->addAndMakeVisible(fegAmountSlider.get());
    fegAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "fegAmount", *fegAmountSlider);

    // Initialize sliders for LFO group
    lfoRateSlider = std::make_unique<juce::Slider>("lfoRateSlider");
    lfoRateSlider->setRange(0.0, 20.0, 0.01);
    lfoRateSlider->setSliderStyle(juce::Slider::Rotary);
    lfoRateSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    lfoGroup->addAndMakeVisible(lfoRateSlider.get());
    lfoRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "lfoRate", *lfoRateSlider);

    lfoDepthSlider = std::make_unique<juce::Slider>("lfoDepthSlider");
    lfoDepthSlider->setRange(0.0, 1.0, 0.01);
    lfoDepthSlider->setSliderStyle(juce::Slider::Rotary);
    lfoDepthSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    lfoGroup->addAndMakeVisible(lfoDepthSlider.get());
    lfoDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "lfoDepth", *lfoDepthSlider);

    // Initialize sliders for Sub Oscillator group
    subTuneSlider = std::make_unique<juce::Slider>("subTuneSlider");
    subTuneSlider->setRange(-24.0, 24.0, 1.0);
    subTuneSlider->setSliderStyle(juce::Slider::Rotary);
    subTuneSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    subOscillatorGroup->addAndMakeVisible(subTuneSlider.get());
    subTuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "subTune", *subTuneSlider);

    subMixSlider = std::make_unique<juce::Slider>("subMixSlider");
    subMixSlider->setRange(0.0, 1.0, 0.01);
    subMixSlider->setSliderStyle(juce::Slider::Rotary);
    subMixSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    subOscillatorGroup->addAndMakeVisible(subMixSlider.get());
    subMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                              "subMix", *subMixSlider);

    subTrackSlider = std::make_unique<juce::Slider>("subTrackSlider");
    subTrackSlider->setRange(0.0, 1.0, 0.01);
    subTrackSlider->setSliderStyle(juce::Slider::Rotary);
    subTrackSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    subOscillatorGroup->addAndMakeVisible(subTrackSlider.get());
    subTrackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "subTrack", *subTrackSlider);

    // Initialize sliders for Output group
    gainSlider = std::make_unique<juce::Slider>("gainSlider");
    gainSlider->setRange(0.0, 2.0, 0.01);
    gainSlider->setSliderStyle(juce::Slider::Rotary);
    gainSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    outputGroup->addAndMakeVisible(gainSlider.get());
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                            "gain", *gainSlider);

    // Ensure all components are visible
    presetComboBox->setVisible(true);
    saveButton->setVisible(true);
    presetNameEditor->setVisible(true);
    confirmButton->setVisible(true);
    loadButton->setVisible(true);
    oscillatorGroup->setVisible(true);
    ampEnvelopeGroup->setVisible(true);
    filterGroup->setVisible(true);
    filterEnvelopeGroup->setVisible(true);
    lfoGroup->setVisible(true);
    subOscillatorGroup->setVisible(true);
    outputGroup->setVisible(true);
    wavetableSlider->setVisible(true);
    unisonSlider->setVisible(true);
    detuneSlider->setVisible(true);
    attackSlider->setVisible(true);
    decaySlider->setVisible(true);
    sustainSlider->setVisible(true);
    releaseSlider->setVisible(true);
    cutoffSlider->setVisible(true);
    resonanceSlider->setVisible(true);
    fegAttackSlider->setVisible(true);
    fegDecaySlider->setVisible(true);
    fegSustainSlider->setVisible(true);
    fegReleaseSlider->setVisible(true);
    fegAmountSlider->setVisible(true);
    lfoRateSlider->setVisible(true);
    lfoDepthSlider->setVisible(true);
    subTuneSlider->setVisible(true);
    subMixSlider->setVisible(true);
    subTrackSlider->setVisible(true);
    gainSlider->setVisible(true);

    // Set size last to avoid premature resized() calls
    setSize(800, 600);

    // Debug component initialization
    DBG("Initialized components:");
    DBG("presetComboBox visible: " << (presetComboBox->isVisible() ? "YES" : "NO")
                                   << ", bounds: " << presetComboBox->getBounds().toString());
    DBG("wavetableSlider visible: " << (wavetableSlider->isVisible() ? "YES" : "NO")
                                    << ", bounds: " << wavetableSlider->getBounds().toString());
}

SimdSynthAudioProcessorEditor::~SimdSynthAudioProcessorEditor() {
    setLookAndFeel(nullptr); // Clean up custom LookAndFeel
}

void SimdSynthAudioProcessorEditor::paint(juce::Graphics &g) {
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::red);
    g.drawRect(getLocalBounds(), 1.0f);
    // DBG("Painting editor, bounds: " << getLocalBounds().toString());
}

void SimdSynthAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();
    // Prevent layout with invalid bounds
    if (bounds.getWidth() < 600 || bounds.getHeight() < 400) {
        // DBG("Window too small: " << bounds.toString());
        return;
    }

    auto presetArea = bounds.removeFromTop(50).reduced(5); // Increased height and margin
    auto controlArea = bounds.reduced(15);

    // Layout preset controls using FlexBox
    juce::FlexBox presetBox;
    presetBox.flexDirection = juce::FlexBox::Direction::row;
    presetBox.items.add(juce::FlexItem(*presetComboBox).withFlex(3).withMargin(5));
    presetBox.items.add(juce::FlexItem(*saveButton).withFlex(1).withMargin(5));
    presetBox.items.add(juce::FlexItem(*presetNameEditor).withFlex(1).withMargin(5));
    presetBox.items.add(juce::FlexItem(*confirmButton).withFlex(1).withMargin(5));
    presetBox.items.add(juce::FlexItem(*loadButton).withFlex(1).withMargin(5));
    presetBox.performLayout(presetArea);

    // Layout groups using Grid
    juce::Grid grid;
    grid.templateColumns = {juce::Grid::Fr(1), juce::Grid::Fr(1), juce::Grid::Fr(1), juce::Grid::Fr(1),
                            juce::Grid::Fr(1)};
    grid.templateRows = {juce::Grid::Fr(2), juce::Grid::Fr(1)}; // Adjusted row heights
    grid.items.add(juce::GridItem(oscillatorGroup.get()).withMargin(15));
    grid.items.add(juce::GridItem(ampEnvelopeGroup.get()).withMargin(15));
    grid.items.add(juce::GridItem(filterGroup.get()).withMargin(15));
    grid.items.add(juce::GridItem(filterEnvelopeGroup.get()).withMargin(15));
    grid.items.add(juce::GridItem(lfoGroup.get()).withMargin(15));
    grid.items.add(juce::GridItem(subOscillatorGroup.get()).withArea(2, 1).withMargin(15));
    grid.items.add(juce::GridItem(outputGroup.get()).withArea(2, 5).withMargin(15));
    grid.performLayout(controlArea);

    // Layout sliders within each group
    layoutGroupSliders(oscillatorGroup.get(), {wavetableSlider.get(), unisonSlider.get(), detuneSlider.get()});
    layoutGroupSliders(ampEnvelopeGroup.get(),
                       {attackSlider.get(), decaySlider.get(), sustainSlider.get(), releaseSlider.get()});
    layoutGroupSliders(filterGroup.get(), {cutoffSlider.get(), resonanceSlider.get()});
    layoutGroupSliders(filterEnvelopeGroup.get(), {fegAttackSlider.get(), fegDecaySlider.get(), fegSustainSlider.get(),
                                                   fegReleaseSlider.get(), fegAmountSlider.get()});
    layoutGroupSliders(lfoGroup.get(), {lfoRateSlider.get(), lfoDepthSlider.get()});
    layoutGroupSliders(subOscillatorGroup.get(), {subTuneSlider.get(), subMixSlider.get(), subTrackSlider.get()});
    layoutGroupSliders(outputGroup.get(), {gainSlider.get()});

    // Debug bounds
    DBG("Window bounds: " << getLocalBounds().toString());
    DBG("presetComboBox bounds: " << presetComboBox->getBounds().toString());
    DBG("oscillatorGroup bounds: " << oscillatorGroup->getBounds().toString());
    DBG("wavetableSlider bounds: " << wavetableSlider->getBounds().toString());
    DBG("attackSlider bounds: " << attackSlider->getBounds().toString());
    DBG("cutoffSlider bounds: " << cutoffSlider->getBounds().toString());
}

void SimdSynthAudioProcessorEditor::layoutGroupSliders(juce::GroupComponent *group,
                                                       const std::vector<juce::Slider *> &sliders) {
    auto groupBounds = group->getLocalBounds().reduced(15);
    auto sliderHeight = groupBounds.getHeight() / (sliders.size() + 1); // Extra space for better layout
    for (auto *slider : sliders) {
        slider->setBounds(groupBounds.removeFromTop(sliderHeight).reduced(5));
    }
}

void SimdSynthAudioProcessorEditor::updatePresetComboBox() {
    presetComboBox->clear(juce::dontSendNotification);
    for (int i = 0; i < processor.getNumPrograms(); ++i) {
        presetComboBox->addItem(processor.getProgramName(i), i + 1);
    }
    int selectedId = processor.getCurrentProgram() + 1;
    presetComboBox->setSelectedId(selectedId, juce::dontSendNotification);
    presetComboBox->setTextWhenNothingSelected("Select Preset");
    DBG("ComboBox updated: selectedId=" << selectedId << ", preset=" << presetComboBox->getText());
}

void SimdSynthAudioProcessorEditor::comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged) {
    if (comboBoxThatHasChanged == presetComboBox.get()) {
        int selectedId = presetComboBox->getSelectedId();
        if (selectedId > 0) {
            processor.setCurrentProgram(selectedId - 1);
            // Removed redundant updatePresetComboBox() call to prevent loop
        }
    }
}
