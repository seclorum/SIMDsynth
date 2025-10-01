/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 16 voices.
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
    getConstrainer()->setMinimumSize(800, 980);

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

    oscillator2Group = std::make_unique<juce::GroupComponent>("oscillator2Group", "2nd Oscillator");
    addAndMakeVisible(oscillator2Group.get());

    subOscillatorGroup = std::make_unique<juce::GroupComponent>("subOscillatorGroup", "Sub Oscillator");
    addAndMakeVisible(subOscillatorGroup.get());

    filterGroup = std::make_unique<juce::GroupComponent>("filterGroup", "Filter");
    addAndMakeVisible(filterGroup.get());

    lfoGroup = std::make_unique<juce::GroupComponent>("lfoGroup", "LFO");
    addAndMakeVisible(lfoGroup.get());

    ampEnvelopeGroup = std::make_unique<juce::GroupComponent>("ampEnvelopeGroup", "Amp Envelope");
    addAndMakeVisible(ampEnvelopeGroup.get());

    filterEnvelopeGroup = std::make_unique<juce::GroupComponent>("filterEnvelopeGroup", "Filter Envelope");
    addAndMakeVisible(filterEnvelopeGroup.get());

    outputGroup = std::make_unique<juce::GroupComponent>("outputGroup", "Output");
    addAndMakeVisible(outputGroup.get());

    // Initialize sliders for Oscillator group (wavetableSlider and unisonSlider unchanged)
    wavetableSlider = std::make_unique<juce::Slider>("wavetableSlider");
    wavetableSlider->setRange(0, 3, 1);
    wavetableSlider->setSliderStyle(juce::Slider::Rotary);
    wavetableSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    oscillatorGroup->addAndMakeVisible(wavetableSlider.get());
    wavetableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "wavetable", *wavetableSlider);
    wavetableLabel = std::make_unique<juce::Label>("wavetableLabel", "Wavetable Type");
    oscillatorGroup->addAndMakeVisible(wavetableLabel.get());
    wavetableLabel->setJustificationType(juce::Justification::centred);

    unisonSlider = std::make_unique<juce::Slider>("unisonSlider");
    unisonSlider->setRange(1, 8, 1);
    unisonSlider->setSliderStyle(juce::Slider::Rotary);
    unisonSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    oscillatorGroup->addAndMakeVisible(unisonSlider.get());
    unisonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                              "unison", *unisonSlider);
    unisonLabel = std::make_unique<juce::Label>("unisonLabel", "Unison Voices");
    oscillatorGroup->addAndMakeVisible(unisonLabel.get());
    unisonLabel->setJustificationType(juce::Justification::centred);

    detuneSlider = std::make_unique<juce::Slider>("detuneSlider");
    detuneSlider->setRange(0.0, 0.1, 0.001);
    detuneSlider->setSliderStyle(juce::Slider::Rotary);
    detuneSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    oscillatorGroup->addAndMakeVisible(detuneSlider.get());
    detuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                              "detune", *detuneSlider);
    detuneLabel = std::make_unique<juce::Label>("detuneLabel", "Unison Detune");
    oscillatorGroup->addAndMakeVisible(detuneLabel.get()); // Changed to addAndMakeVisible
    detuneLabel->setJustificationType(juce::Justification::centred);

    // Initialize sliders for Amp Envelope group
    attackSlider = std::make_unique<juce::Slider>("attackSlider");
    attackSlider->setRange(0.0, 5.0, 0.01);
    attackSlider->setSliderStyle(juce::Slider::Rotary);
    attackSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    ampEnvelopeGroup->addAndMakeVisible(attackSlider.get());
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                              "attack", *attackSlider);
    attackLabel = std::make_unique<juce::Label>("attackLabel", "Attack Time");
    ampEnvelopeGroup->addAndMakeVisible(attackLabel.get()); // Changed to addAndMakeVisible
    attackLabel->setJustificationType(juce::Justification::centred);

    decaySlider = std::make_unique<juce::Slider>("decaySlider");
    decaySlider->setRange(0.0, 5.0, 0.01);
    decaySlider->setSliderStyle(juce::Slider::Rotary);
    decaySlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    ampEnvelopeGroup->addAndMakeVisible(decaySlider.get());
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                             "decay", *decaySlider);
    decayLabel = std::make_unique<juce::Label>("decayLabel", "Decay Time");
    ampEnvelopeGroup->addAndMakeVisible(decayLabel.get()); // Changed to addAndMakeVisible
    decayLabel->setJustificationType(juce::Justification::centred);

    sustainSlider = std::make_unique<juce::Slider>("sustainSlider");
    sustainSlider->setRange(0.0, 1.0, 0.01);
    sustainSlider->setSliderStyle(juce::Slider::Rotary);
    sustainSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    ampEnvelopeGroup->addAndMakeVisible(sustainSlider.get());
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "sustain", *sustainSlider);
    sustainLabel = std::make_unique<juce::Label>("sustainLabel", "Sustain Level");
    ampEnvelopeGroup->addAndMakeVisible(sustainLabel.get()); // Changed to addAndMakeVisible
    sustainLabel->setJustificationType(juce::Justification::centred);

    releaseSlider = std::make_unique<juce::Slider>("releaseSlider");
    releaseSlider->setRange(0.0, 5.0, 0.01);
    releaseSlider->setSliderStyle(juce::Slider::Rotary);
    releaseSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    ampEnvelopeGroup->addAndMakeVisible(releaseSlider.get());
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "release", *releaseSlider);
    releaseLabel = std::make_unique<juce::Label>("releaseLabel", "Release Time");
    ampEnvelopeGroup->addAndMakeVisible(releaseLabel.get()); // Changed to addAndMakeVisible
    releaseLabel->setJustificationType(juce::Justification::centred);

    // Add attackCurve slider
    attackCurveSlider = std::make_unique<juce::Slider>("attackCurveSlider");
    attackCurveSlider->setRange(0.5, 5.0, 0.01);
    attackCurveSlider->setSliderStyle(juce::Slider::Rotary);
    attackCurveSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    ampEnvelopeGroup->addAndMakeVisible(attackCurveSlider.get());
    attackCurveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "attackCurve", *attackCurveSlider);
    attackCurveLabel = std::make_unique<juce::Label>("attackCurveLabel", "Attack Curve");
    ampEnvelopeGroup->addAndMakeVisible(attackCurveLabel.get()); // Changed to addAndMakeVisible
    attackCurveLabel->setJustificationType(juce::Justification::centred);

    // Add releaseCurve slider
    releaseCurveSlider = std::make_unique<juce::Slider>("releaseCurveSlider");
    releaseCurveSlider->setRange(0.5, 5.0, 0.01);
    releaseCurveSlider->setSliderStyle(juce::Slider::Rotary);
    releaseCurveSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    ampEnvelopeGroup->addAndMakeVisible(releaseCurveSlider.get());
    releaseCurveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "releaseCurve", *releaseCurveSlider);
    releaseCurveLabel = std::make_unique<juce::Label>("releaseCurveLabel", "Release Curve");
    ampEnvelopeGroup->addAndMakeVisible(releaseCurveLabel.get()); // Changed to addAndMakeVisible
    releaseCurveLabel->setJustificationType(juce::Justification::centred);

    // Initialize sliders for Filter group
    cutoffSlider = std::make_unique<juce::Slider>("cutoffSlider");
    cutoffSlider->setRange(20.0, 20000.0, 1.0);
    cutoffSlider->setSkewFactorFromMidPoint(1000.0);
    cutoffSlider->setSliderStyle(juce::Slider::Rotary);
    cutoffSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterGroup->addAndMakeVisible(cutoffSlider.get());
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                              "cutoff", *cutoffSlider);
    cutoffLabel = std::make_unique<juce::Label>("cutoffLabel", "Filter Cutoff");
    filterGroup->addAndMakeVisible(cutoffLabel.get()); // Changed to addAndMakeVisible
    cutoffLabel->setJustificationType(juce::Justification::centred);

    resonanceSlider = std::make_unique<juce::Slider>("resonanceSlider");
    resonanceSlider->setRange(0.0, 1.0, 0.01);
    resonanceSlider->setSliderStyle(juce::Slider::Rotary);
    resonanceSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterGroup->addAndMakeVisible(resonanceSlider.get());
    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "resonance", *resonanceSlider);
    resonanceLabel = std::make_unique<juce::Label>("resonanceLabel", "Filter Resonance");
    filterGroup->addAndMakeVisible(resonanceLabel.get()); // Changed to addAndMakeVisible
    resonanceLabel->setJustificationType(juce::Justification::centred);

    // Initialize sliders for Filter Envelope group
    fegAttackSlider = std::make_unique<juce::Slider>("fegAttackSlider");
    fegAttackSlider->setRange(0.0, 5.0, 0.01);
    fegAttackSlider->setSliderStyle(juce::Slider::Rotary);
    fegAttackSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterEnvelopeGroup->addAndMakeVisible(fegAttackSlider.get());
    fegAttackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "fegAttack", *fegAttackSlider);
    fegAttackLabel = std::make_unique<juce::Label>("fegAttackLabel", "FEG Attack");
    filterEnvelopeGroup->addAndMakeVisible(fegAttackLabel.get()); // Changed to addAndMakeVisible
    fegAttackLabel->setJustificationType(juce::Justification::centred);

    fegDecaySlider = std::make_unique<juce::Slider>("fegDecaySlider");
    fegDecaySlider->setRange(0.0, 5.0, 0.01);
    fegDecaySlider->setSliderStyle(juce::Slider::Rotary);
    fegDecaySlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterEnvelopeGroup->addAndMakeVisible(fegDecaySlider.get());
    fegDecayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "fegDecay", *fegDecaySlider);
    fegDecayLabel = std::make_unique<juce::Label>("fegDecayLabel", "FEG Decay");
    filterEnvelopeGroup->addAndMakeVisible(fegDecayLabel.get()); // Changed to addAndMakeVisible
    fegDecayLabel->setJustificationType(juce::Justification::centred);

    fegSustainSlider = std::make_unique<juce::Slider>("fegSustainSlider");
    fegSustainSlider->setRange(0.0, 1.0, 0.01);
    fegSustainSlider->setSliderStyle(juce::Slider::Rotary);
    fegSustainSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterEnvelopeGroup->addAndMakeVisible(fegSustainSlider.get());
    fegSustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "fegSustain", *fegSustainSlider);
    fegSustainLabel = std::make_unique<juce::Label>("fegSustainLabel", "FEG Sustain");
    filterEnvelopeGroup->addAndMakeVisible(fegSustainLabel.get()); // Changed to addAndMakeVisible
    fegSustainLabel->setJustificationType(juce::Justification::centred);

    fegReleaseSlider = std::make_unique<juce::Slider>("fegReleaseSlider");
    fegReleaseSlider->setRange(0.0, 5.0, 0.01);
    fegReleaseSlider->setSliderStyle(juce::Slider::Rotary);
    fegReleaseSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterEnvelopeGroup->addAndMakeVisible(fegReleaseSlider.get());
    fegReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "fegRelease", *fegReleaseSlider);
    fegReleaseLabel = std::make_unique<juce::Label>("fegReleaseLabel", "FEG Release");
    filterEnvelopeGroup->addAndMakeVisible(fegReleaseLabel.get()); // Changed to addAndMakeVisible
    fegReleaseLabel->setJustificationType(juce::Justification::centred);

    fegAmountSlider = std::make_unique<juce::Slider>("fegAmountSlider");
    fegAmountSlider->setRange(0.0, 1.0, 0.01);
    fegAmountSlider->setSliderStyle(juce::Slider::Rotary);
    fegAmountSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    filterEnvelopeGroup->addAndMakeVisible(fegAmountSlider.get());
    fegAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "fegAmount", *fegAmountSlider);
    fegAmountLabel = std::make_unique<juce::Label>("fegAmountLabel", "FEG Amount");
    filterEnvelopeGroup->addAndMakeVisible(fegAmountLabel.get()); // Changed to addAndMakeVisible
    fegAmountLabel->setJustificationType(juce::Justification::centred);

    // Initialize sliders for LFO group
    lfoRateSlider = std::make_unique<juce::Slider>("lfoRateSlider");
    lfoRateSlider->setRange(0.0, 20.0, 0.01);
    lfoRateSlider->setSliderStyle(juce::Slider::Rotary);
    lfoRateSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    lfoGroup->addAndMakeVisible(lfoRateSlider.get());
    lfoRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "lfoRate", *lfoRateSlider);
    lfoRateLabel = std::make_unique<juce::Label>("lfoRateLabel", "LFO Rate");
    lfoGroup->addAndMakeVisible(lfoRateLabel.get()); // Changed to addAndMakeVisible
    lfoRateLabel->setJustificationType(juce::Justification::centred);

    lfoDepthSlider = std::make_unique<juce::Slider>("lfoDepthSlider");
    lfoDepthSlider->setRange(0.0, 1.0, 0.01);
    lfoDepthSlider->setSliderStyle(juce::Slider::Rotary);
    lfoDepthSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    lfoGroup->addAndMakeVisible(lfoDepthSlider.get());
    lfoDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "lfoDepth", *lfoDepthSlider);
    lfoDepthLabel = std::make_unique<juce::Label>("lfoDepthLabel", "LFO Depth");
    lfoGroup->addAndMakeVisible(lfoDepthLabel.get()); // Changed to addAndMakeVisible
    lfoDepthLabel->setJustificationType(juce::Justification::centred);

    lfoPitchAmtSlider = std::make_unique<juce::Slider>("lfoPitchAmtSlider");
    lfoPitchAmtSlider->setRange(0.0, 0.2, 0.001);
    lfoPitchAmtSlider->setSliderStyle(juce::Slider::Rotary);
    lfoPitchAmtSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    lfoGroup->addAndMakeVisible(lfoPitchAmtSlider.get());
    lfoPitchAmtAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "lfoPitchAmt", *lfoPitchAmtSlider);
    lfoPitchAmtLabel = std::make_unique<juce::Label>("lfoPitchAmtLabel", "LFO Pitch Amt");
    lfoGroup->addAndMakeVisible(lfoPitchAmtLabel.get()); // Changed to addAndMakeVisible
    lfoPitchAmtLabel->setJustificationType(juce::Justification::centred);

    // Initialize sliders for 2nd Oscillator group
    osc2TuneSlider = std::make_unique<juce::Slider>("osc2TuneSlider");
    osc2TuneSlider->setRange(-1.0, 12.0, 0.01);
    osc2TuneSlider->setSliderStyle(juce::Slider::Rotary);
    osc2TuneSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    oscillator2Group->addAndMakeVisible(osc2TuneSlider.get());
    osc2TuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "osc2Tune", *osc2TuneSlider);
    osc2TuneLabel = std::make_unique<juce::Label>("osc2TuneLabel", "Osc 2 Tune");
    oscillator2Group->addAndMakeVisible(osc2TuneLabel.get()); // Changed to addAndMakeVisible
    osc2TuneLabel->setJustificationType(juce::Justification::centred);

    osc2MixSlider = std::make_unique<juce::Slider>("osc2MixSlider");
    osc2MixSlider->setRange(0.0, 1.0, 0.01);
    osc2MixSlider->setSliderStyle(juce::Slider::Rotary);
    osc2MixSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    oscillator2Group->addAndMakeVisible(osc2MixSlider.get());
    osc2MixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "osc2Mix", *osc2MixSlider);
    osc2MixLabel = std::make_unique<juce::Label>("osc2MixLabel", "Osc 2 Mix");
    oscillator2Group->addAndMakeVisible(osc2MixLabel.get()); // Changed to addAndMakeVisible
    osc2MixLabel->setJustificationType(juce::Justification::centred);

    osc2TrackSlider = std::make_unique<juce::Slider>("osc2TrackSlider");
    osc2TrackSlider->setRange(0.0, 1.0, 0.01);
    osc2TrackSlider->setSliderStyle(juce::Slider::Rotary);
    osc2TrackSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    oscillator2Group->addAndMakeVisible(osc2TrackSlider.get());
    osc2TrackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "osc2Track", *osc2TrackSlider);
    osc2TrackLabel = std::make_unique<juce::Label>("osc2TrackLabel", "Osc 2 Track");
    oscillator2Group->addAndMakeVisible(osc2TrackLabel.get()); // Changed to addAndMakeVisible
    osc2TrackLabel->setJustificationType(juce::Justification::centred);

    // Initialize sliders for Sub Oscillator group
    subTuneSlider = std::make_unique<juce::Slider>("subTuneSlider");
    subTuneSlider->setRange(-24.0, 24.0, 1.0);
    subTuneSlider->setSliderStyle(juce::Slider::Rotary);
    subTuneSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    subOscillatorGroup->addAndMakeVisible(subTuneSlider.get());
    subTuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "subTune", *subTuneSlider);
    subTuneLabel = std::make_unique<juce::Label>("subTuneLabel", "Sub Osc Tune");
    subOscillatorGroup->addAndMakeVisible(subTuneLabel.get()); // Changed to addAndMakeVisible
    subTuneLabel->setJustificationType(juce::Justification::centred);

    subMixSlider = std::make_unique<juce::Slider>("subMixSlider");
    subMixSlider->setRange(0.0, 1.0, 0.01);
    subMixSlider->setSliderStyle(juce::Slider::Rotary);
    subMixSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    subOscillatorGroup->addAndMakeVisible(subMixSlider.get());
    subMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                              "subMix", *subMixSlider);
    subMixLabel = std::make_unique<juce::Label>("subMixLabel", "Sub Osc Mix");
    subOscillatorGroup->addAndMakeVisible(subMixLabel.get()); // Changed to addAndMakeVisible
    subMixLabel->setJustificationType(juce::Justification::centred);

    subTrackSlider = std::make_unique<juce::Slider>("subTrackSlider");
    subTrackSlider->setRange(0.0, 1.0, 0.01);
    subTrackSlider->setSliderStyle(juce::Slider::Rotary);
    subTrackSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    subOscillatorGroup->addAndMakeVisible(subTrackSlider.get());
    subTrackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), "subTrack", *subTrackSlider);
    subTrackLabel = std::make_unique<juce::Label>("subTrackLabel", "Sub Osc Track");
    subOscillatorGroup->addAndMakeVisible(subTrackLabel.get()); // Changed to addAndMakeVisible
    subTrackLabel->setJustificationType(juce::Justification::centred);

    // Initialize sliders for Output group
    gainSlider = std::make_unique<juce::Slider>("gainSlider");
    gainSlider->setRange(0.0, 2.0, 0.01);
    gainSlider->setSliderStyle(juce::Slider::Rotary);
    gainSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    outputGroup->addAndMakeVisible(gainSlider.get());
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getParameters(),
                                                                                            "gain", *gainSlider);
    gainLabel = std::make_unique<juce::Label>("gainLabel", "Output Gain");
    outputGroup->addAndMakeVisible(gainLabel.get()); // Changed to addAndMakeVisible
    gainLabel->setJustificationType(juce::Justification::centred);

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
    oscillator2Group->setVisible(true);
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
    osc2TuneSlider->setVisible(true);
    osc2MixSlider->setVisible(true);
    osc2TrackSlider->setVisible(true);
    subTuneSlider->setVisible(true);
    subMixSlider->setVisible(true);
    subTrackSlider->setVisible(true);
    gainSlider->setVisible(true);
    attackCurveSlider->setVisible(true);
    releaseCurveSlider->setVisible(true);
    lfoPitchAmtSlider->setVisible(true);

    // repaint();

    // Set size last to avoid premature resized() calls
    setSize(800, 980);

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
    if (bounds.getWidth() < 600 || bounds.getHeight() < 400) {
        DBG("Window too small (" << bounds.toString() << ") - layout may clip");
    }

    auto presetArea = bounds.removeFromTop(50).reduced(5);
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
    grid.templateRows = {juce::Grid::Fr(2), juce::Grid::Fr(3)};
    grid.items.add(juce::GridItem(oscillatorGroup.get()).withMargin(15));
    grid.items.add(juce::GridItem(oscillator2Group.get()).withMargin(15));
    grid.items.add(juce::GridItem(subOscillatorGroup.get()).withMargin(15));
    grid.items.add(juce::GridItem(filterGroup.get()).withMargin(15));
    grid.items.add(juce::GridItem(lfoGroup.get()).withMargin(15));
    grid.items.add(juce::GridItem(ampEnvelopeGroup.get()).withArea(2, 1).withMargin(15));
    grid.items.add(juce::GridItem(filterEnvelopeGroup.get()).withArea(2, 2).withMargin(15));
    grid.items.add(juce::GridItem(outputGroup.get()).withArea(2, 5).withMargin(15).withHeight(100));
    grid.performLayout(controlArea);

    // Layout sliders and labels within each group
    layoutGroupSliders(oscillatorGroup.get(), {{wavetableSlider.get(), wavetableLabel.get()},
                                               {unisonSlider.get(), unisonLabel.get()},
                                               {detuneSlider.get(), detuneLabel.get()}});
    layoutGroupSliders(ampEnvelopeGroup.get(), {{attackSlider.get(), attackLabel.get()},
                                                {decaySlider.get(), decayLabel.get()},
                                                {sustainSlider.get(), sustainLabel.get()},
                                                {releaseSlider.get(), releaseLabel.get()},
                                                {attackCurveSlider.get(), attackCurveLabel.get()},
                                                {releaseCurveSlider.get(), releaseCurveLabel.get()}});
    layoutGroupSliders(filterGroup.get(),
                       {{cutoffSlider.get(), cutoffLabel.get()}, {resonanceSlider.get(), resonanceLabel.get()}});
    layoutGroupSliders(filterEnvelopeGroup.get(), {{fegAttackSlider.get(), fegAttackLabel.get()},
                                                   {fegDecaySlider.get(), fegDecayLabel.get()},
                                                   {fegSustainSlider.get(), fegSustainLabel.get()},
                                                   {fegReleaseSlider.get(), fegReleaseLabel.get()},
                                                   {fegAmountSlider.get(), fegAmountLabel.get()}});
    layoutGroupSliders(lfoGroup.get(), {{lfoRateSlider.get(), lfoRateLabel.get()},
                                        {lfoDepthSlider.get(), lfoDepthLabel.get()},
                                        {lfoPitchAmtSlider.get(), lfoPitchAmtLabel.get()}});
    layoutGroupSliders(oscillator2Group.get(), {{osc2TuneSlider.get(), osc2TuneLabel.get()},
                                                {osc2MixSlider.get(), osc2MixLabel.get()},
                                                {osc2TrackSlider.get(), osc2TrackLabel.get()}});
    layoutGroupSliders(subOscillatorGroup.get(), {{subTuneSlider.get(), subTuneLabel.get()},
                                                  {subMixSlider.get(), subMixLabel.get()},
                                                  {subTrackSlider.get(), subTrackLabel.get()}});
    layoutGroupSliders(outputGroup.get(), {{gainSlider.get(), gainLabel.get()}});

    // Debug bounds
    DBG("Window bounds: " << getLocalBounds().toString());
    DBG("presetComboBox bounds: " << presetComboBox->getBounds().toString());
    DBG("oscillatorGroup bounds: " << oscillatorGroup->getBounds().toString());
    DBG("oscillator2Group bounds: " << oscillator2Group->getBounds().toString());
    DBG("subOscillatorGroup bounds: " << subOscillatorGroup->getBounds().toString());
    DBG("outputGroup bounds: " << outputGroup->getBounds().toString());
    DBG("osc2TuneSlider bounds: " << osc2TuneSlider->getBounds().toString());
    DBG("subTuneSlider bounds: " << subTuneSlider->getBounds().toString());
    DBG("gainSlider bounds: " << gainSlider->getBounds().toString());
}

void SimdSynthAudioProcessorEditor::layoutGroupSliders(
    juce::GroupComponent *group, const std::vector<std::pair<juce::Slider *, juce::Label *>> &slidersAndLabels) {
    auto groupBounds = group->getLocalBounds().reduced(15);
    auto sliderHeight = juce::jmax(60.0f, static_cast<float>(groupBounds.getHeight()) /
                                              slidersAndLabels.size()); // Increased min height for label
    for (auto &[slider, label] : slidersAndLabels) {
        auto sliderArea = groupBounds.removeFromTop(sliderHeight).reduced(5);
        slider->setBounds(sliderArea);
        if (label) {
            // Place label below slider's text box (20px height from setTextBoxStyle)
            auto labelBounds = sliderArea.withHeight(20).translated(0, sliderArea.getHeight());
            label->setBounds(labelBounds);
        }
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