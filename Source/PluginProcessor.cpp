/*
 * simdsynth - A playground for experimenting with SIMD-based audio synthesis,
 *             with polyphonic main and sub-oscillator, filter, envelopes, and
 *             LFO per voice, up to 16 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_core/juce_core.h> // For File and JSON handling
#include <juce_dsp/juce_dsp.h>   // For oversampling and DSP utilities

#define DEFAULT_NOTE_NUM 69

// Constructor: Initializes the audio processor with stereo output and enhanced parameters
SimdSynthAudioProcessor::SimdSynthAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("SimdSynth"),
                 {std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"wavetable", parameterVersion},
                                                              "Wavetable Type", 0.0f, 2.0f, 0.0f), // Sine, saw, square
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"attack", parameterVersion},
                                                              "Attack Time", 0.01f, 5.0f, 0.1f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"decay", parameterVersion},
                                                              "Decay Time", 0.1f, 5.0f, 0.5f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"sustain", parameterVersion},
                                                              "Sustain Level", 0.0f, 1.0f, 0.8f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"release", parameterVersion},
                                                              "Release Time", 0.01f, 5.0f, 0.2f),
                  std::make_unique<juce::AudioParameterFloat>(
                      juce::ParameterID{"attackCurve", parameterVersion}, // New: Variable attack curve
                      "Attack Curve", 0.5f, 5.0f, 2.0f),
                  std::make_unique<juce::AudioParameterFloat>(
                      juce::ParameterID{"releaseCurve", parameterVersion}, // New: Variable release curve
                      "Release Curve", 0.5f, 5.0f, 3.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"filterBypass", parameterVersion},
                                                              "Filter Bypass", 0.0f, 1.0f, 0.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"cutoff", parameterVersion},
                                                              "Filter Cutoff", 20.0f, 20000.0f, 2000.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"resonance", parameterVersion},
                                                              "Filter Resonance", 0.0f, 1.0f, 0.9f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegAttack", parameterVersion},
                                                              "Filter EG Attack", 0.01f, 5.0f, 0.1f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegDecay", parameterVersion},
                                                              "Filter EG Decay", 0.1f, 5.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegSustain", parameterVersion},
                                                              "Filter EG Sustain", 0.0f, 1.0f, 0.8f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegRelease", parameterVersion},
                                                              "Filter EG Release", 0.01f, 5.0f, 0.2f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegAmount", parameterVersion},
                                                              "Filter EG Amount", -1.0f, 1.0f, 0.8f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"filterMix", parameterVersion},
                                                              "Filter Mix", 0.0f, 1.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lfoRate", parameterVersion},
                                                              "LFO Rate", 0.0f, 20.0f, 5.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lfoDepth", parameterVersion},
                                                              "LFO Depth", 0.0f, 1.0f, 0.5f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lfoPitchAmt", parameterVersion},
                                                              "LFO Pitch Amt", 0.0f, 0.5f, 0.1f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subTune", parameterVersion},
                                                              "Sub Osc Tune", -24.0f, 24.0f, -12.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subMix", parameterVersion},
                                                              "Sub Osc Mix", 0.0f, 1.0f, 0.7f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subTrack", parameterVersion},
                                                              "Sub Osc Track", 0.0f, 1.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"osc2Tune", parameterVersion},
                                                              "Osc 2 Tune", -12.0f, 12.0f, 0.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"osc2Mix", parameterVersion},
                                                              "Osc 2 Mix", 0.0f, 1.0f, 0.5f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"osc2Track", parameterVersion},
                                                              "Osc 2 Track", 0.0f, 1.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain", parameterVersion},
                                                              "Output Gain", 0.0f, 2.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"unison", parameterVersion},
                                                              "Unison Voices", 1.0f, 8.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"detune", parameterVersion},
                                                              "Unison Detune", 0.0f, 0.1f, 0.01f)}),
      currentTime(0.0), oversampling(std::make_unique<juce::dsp::Oversampling<float>>(
                            2, 2, juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR, true, true)),
      random(juce::Time::getMillisecondCounterHiRes()), smoothedGain(1.0f), smoothedCutoff(1000.0f),
      smoothedResonance(0.7f), smoothedLfoRate(5.0f), smoothedLfoDepth(0.08f), smoothedSubMix(0.5f),
      smoothedSubTune(-12.0f), smoothedSubTrack(1.0f), smoothedDetune(0.01f), smoothedOsc2Mix(0.3f),
      smoothedOsc2Tune(0.0f), smoothedOsc2Track(1.0f), smoothedAttackCurve(2.0f), smoothedReleaseCurve(3.0f),
      smoothedFilterMix(1.0f) {
    // Initialize smoothed values (defer sample rate to prepareToPlay)
    smoothedGain.setCurrentAndTargetValue(1.0f);
    smoothedCutoff.setCurrentAndTargetValue(1000.0f);
    smoothedResonance.setCurrentAndTargetValue(0.7f);
    smoothedLfoRate.setCurrentAndTargetValue(5.0f);
    smoothedLfoDepth.setCurrentAndTargetValue(0.5f);
    smoothedSubMix.setCurrentAndTargetValue(0.5f);
    smoothedSubTune.setCurrentAndTargetValue(-12.0f);
    smoothedSubTrack.setCurrentAndTargetValue(1.0f);
    smoothedDetune.setCurrentAndTargetValue(0.01f);
    smoothedOsc2Mix.setCurrentAndTargetValue(0.1f);
    smoothedOsc2Tune.setCurrentAndTargetValue(0.0f);
    smoothedOsc2Track.setCurrentAndTargetValue(1.0f);
    smoothedAttackCurve.setCurrentAndTargetValue(2.0f);
    smoothedReleaseCurve.setCurrentAndTargetValue(3.0f);
    smoothedFilterMix.setCurrentAndTargetValue(1.0f);

    // Initialize parameter pointers (add new ones)
    wavetableTypeParam = parameters.getRawParameterValue("wavetable");
    attackTimeParam = parameters.getRawParameterValue("attack");
    decayTimeParam = parameters.getRawParameterValue("decay");
    sustainLevelParam = parameters.getRawParameterValue("sustain");
    releaseTimeParam = parameters.getRawParameterValue("release");
    attackCurveParam = parameters.getRawParameterValue("attackCurve");   // New
    releaseCurveParam = parameters.getRawParameterValue("releaseCurve"); // New
    cutoffParam = parameters.getRawParameterValue("cutoff");
    resonanceParam = parameters.getRawParameterValue("resonance");
    filterMixParam = parameters.getRawParameterValue("filterMix");
    filterBypassParam = parameters.getRawParameterValue("filterBypass");
    fegAttackParam = parameters.getRawParameterValue("fegAttack");
    fegDecayParam = parameters.getRawParameterValue("fegDecay");
    fegSustainParam = parameters.getRawParameterValue("fegSustain");
    fegReleaseParam = parameters.getRawParameterValue("fegRelease");
    fegAmountParam = parameters.getRawParameterValue("fegAmount");
    lfoRateParam = parameters.getRawParameterValue("lfoRate");
    lfoDepthParam = parameters.getRawParameterValue("lfoDepth");
    lfoPitchAmtParam = parameters.getRawParameterValue("lfoPitchAmt");
    subTuneParam = parameters.getRawParameterValue("subTune");
    subMixParam = parameters.getRawParameterValue("subMix");
    subTrackParam = parameters.getRawParameterValue("subTrack");
    osc2TuneParam = parameters.getRawParameterValue("osc2Tune");
    osc2MixParam = parameters.getRawParameterValue("osc2Mix");
    osc2TrackParam = parameters.getRawParameterValue("osc2Track");
    gainParam = parameters.getRawParameterValue("gain");
    unisonParam = parameters.getRawParameterValue("unison");
    detuneParam = parameters.getRawParameterValue("detune");

    parameters.addParameterListener("wavetable", this);
    parameters.addParameterListener("attack", this);
    parameters.addParameterListener("decay", this);
    parameters.addParameterListener("sustain", this);
    parameters.addParameterListener("release", this);
    parameters.addParameterListener("attackCurve", this);
    parameters.addParameterListener("releaseCurve", this);
    parameters.addParameterListener("filterBypass", this);
    parameters.addParameterListener("cutoff", this);
    parameters.addParameterListener("resonance", this);
    parameters.addParameterListener("filterMix", this);
    parameters.addParameterListener("fegAttack", this);
    parameters.addParameterListener("fegDecay", this);
    parameters.addParameterListener("fegSustain", this);
    parameters.addParameterListener("fegRelease", this);
    parameters.addParameterListener("fegAmount", this);
    parameters.addParameterListener("lfoRate", this);
    parameters.addParameterListener("lfoDepth", this);
    parameters.addParameterListener("lfoPitchAmt", this);
    parameters.addParameterListener("subTune", this);
    parameters.addParameterListener("subMix", this);
    parameters.addParameterListener("subTrack", this);
    parameters.addParameterListener("osc2Tune", this);
    parameters.addParameterListener("osc2Mix", this);
    parameters.addParameterListener("osc2Track", this);
    parameters.addParameterListener("gain", this);
    parameters.addParameterListener("unison", this);
    parameters.addParameterListener("detune", this);

    // Store raw default values for preset loading (add new ones)
    defaultParamValues = {{"wavetable", 0.0f}, {"attack", 0.1f},       {"decay", 0.5f},        {"sustain", 0.8f},
                          {"release", 0.2f},   {"attackCurve", 2.0f},  {"releaseCurve", 3.0f}, {"cutoff", 1000.0f},
                          {"resonance", 0.7f}, {"filterBypass", 0.0f}, {"filterMix", 1.0f},    {"fegAttack", 0.1f},
                          {"fegDecay", 1.0f},  {"fegSustain", 0.5f},   {"fegRelease", 0.2f},   {"fegAmount", 0.8f},
                          {"lfoRate", 5.0f},   {"lfoDepth", 0.5f},     {"lfoPitchAmt", 0.1f},  {"subTune", -12.0f},
                          {"subMix", 0.7f},    {"subTrack", 1.0f},     {"osc2Tune", 0.0f},     {"osc2Mix", 0.5f},
                          {"osc2Track", 1.0f}, {"gain", 1.0f},         {"unison", 1.0f},       {"detune", 0.01f}};

    // Initialize random buffer
    refillRandomBuffer();

    // Initialize wavetables (unchanged, but note: sine normalization added for balance)
    sineTable.resize(WAVETABLE_SIZE);
    sawTable.resize(WAVETABLE_SIZE);
    squareTable.resize(WAVETABLE_SIZE);

    for (int i = 0; i < WAVETABLE_SIZE; ++i) {
        float phase = (float)i / (float)(WAVETABLE_SIZE - 1) * 2.0f * juce::MathConstants<float>::pi;
        sineTable[i] = std::sin(phase);
        sawTable[i] = 0.0f;
        squareTable[i] = 0.0f;
        // Band-limited saw and square (up to 10 harmonics to avoid aliasing)
        int maxHarmonics = juce::jlimit(5, 10, static_cast<int>(20000.0f / midiToFreq(DEFAULT_NOTE_NUM)));
        for (int harmonic = 1; harmonic <= maxHarmonics; ++harmonic) {
            float amp = 1.0f / harmonic;
            sawTable[i] += amp * std::sin(phase * harmonic);
            squareTable[i] += (harmonic % 2 == 1 ? amp : 0.0f) * std::sin(phase * harmonic);
        }
    }
    // Normalize all tables to [-1, 1]
    float maxSine = *std::max_element(sineTable.begin(), sineTable.end());
    float maxSaw = *std::max_element(sawTable.begin(), sawTable.end());
    float maxSquare = *std::max_element(squareTable.begin(), squareTable.end());
    for (int i = 0; i < WAVETABLE_SIZE; ++i) {
        sineTable[i] /= maxSine;
        sawTable[i] /= maxSaw;
        squareTable[i] /= maxSquare;
    }

    // Initialize voices with default parameter values (add new fields)
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i] = Voice();
        voices[i].active = false;
        voices[i].released = false;
        voices[i].noteOnTime = 0.0f;
        voices[i].noteOffTime = 0.0f;
        voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
        voices[i].attack = *attackTimeParam;
        voices[i].decay = *decayTimeParam;
        voices[i].sustain = *sustainLevelParam;
        voices[i].release = *releaseTimeParam;
        voices[i].attackCurve = *attackCurveParam;   // New
        voices[i].releaseCurve = *releaseCurveParam; // New
        voices[i].cutoff = *cutoffParam;
        voices[i].filterBypass = *filterBypassParam;
        voices[i].resonance = *resonanceParam;
        voices[i].fegAttack = *fegAttackParam;
        voices[i].fegDecay = *fegDecayParam;
        voices[i].fegSustain = *fegSustainParam;
        voices[i].fegRelease = *fegReleaseParam;
        voices[i].fegAmount = *fegAmountParam;
        voices[i].lfoRate = *lfoRateParam;
        voices[i].lfoDepth = *lfoDepthParam;
        voices[i].lfoPitchAmt = *lfoPitchAmtParam;
        voices[i].subTune = *subTuneParam;
        voices[i].subMix = *subMixParam;
        voices[i].subTrack = *subTrackParam;
        voices[i].osc2Tune = *osc2TuneParam;
        voices[i].osc2Mix = *osc2MixParam;
        voices[i].osc2Track = *osc2TrackParam;
        voices[i].osc2PhaseOffset = 0.0f;
        voices[i].detune = *detuneParam;

        voices[i].unison = juce::jlimit(1, maxUnison, static_cast<int>(*unisonParam));
        voices[i].detuneFactors.resize(maxUnison);
        voices[i].unisonPhases.resize(maxUnison);
        for (int u = 0; u < maxUnison; ++u) {
            voices[i].unisonPhases[u] = random.nextFloat() * 0.01f; // Initialize once
            float detuneCents =
                voices[i].detune * (u - (voices[i].unison - 1) / 2.0f) / (voices[i].unison - 1 + 0.0001f);
            voices[i].detuneFactors[u] = powf(2.0f, detuneCents / 12.0f);
        }

        // In constructor, only set initial values
        voices[i].smoothedAmplitude.setCurrentAndTargetValue(0.0f);
        voices[i].smoothedFilterEnv.setCurrentAndTargetValue(0.0f);
        voices[i].smoothedCutoff.setCurrentAndTargetValue(*cutoffParam);
        voices[i].smoothedFegAmount.setCurrentAndTargetValue(*fegAmountParam);
        voices[i].mainLPState = 0.0f;
        voices[i].subLPState = 0.0f;
        voices[i].osc2LPState = 0.0f;
        voices[i].dcState = 0.0f; // New: For DC blocker
    }

    // Set initial filter resonance
    filter.resonance = *resonanceParam;

    // Initialize presets
    presetManager.createDefaultPresets();
    loadPresetsFromDirectory();
}

// Destructor: Clean up oversampling
SimdSynthAudioProcessor::~SimdSynthAudioProcessor() {
    oversampling.reset();

    parameters.removeParameterListener("wavetable", this);
    parameters.removeParameterListener("attack", this);
    parameters.removeParameterListener("decay", this);
    parameters.removeParameterListener("sustain", this);
    parameters.removeParameterListener("release", this);
    parameters.removeParameterListener("attackCurve", this);
    parameters.removeParameterListener("releaseCurve", this);
    parameters.removeParameterListener("filterBypass", this);
    parameters.removeParameterListener("cutoff", this);
    parameters.removeParameterListener("resonance", this);
    parameters.removeParameterListener("filterMix", this);
    parameters.removeParameterListener("fegAttack", this);
    parameters.removeParameterListener("fegDecay", this);
    parameters.removeParameterListener("fegSustain", this);
    parameters.removeParameterListener("fegRelease", this);
    parameters.removeParameterListener("fegAmount", this);
    parameters.removeParameterListener("lfoRate", this);
    parameters.removeParameterListener("lfoDepth", this);
    parameters.removeParameterListener("lfoPitchAmt", this);
    parameters.removeParameterListener("subTune", this);
    parameters.removeParameterListener("subMix", this);
    parameters.removeParameterListener("subTrack", this);
    parameters.removeParameterListener("osc2Tune", this);
    parameters.removeParameterListener("osc2Mix", this);
    parameters.removeParameterListener("osc2Track", this);
    parameters.removeParameterListener("gain", this);
    parameters.removeParameterListener("unison", this);
    parameters.removeParameterListener("detune", this);
}

// Helper Function to Get Random Float
float SimdSynthAudioProcessor::getRandomFloatAudioThread() {
    size_t index = randomIndex.load(std::memory_order_acquire);
    size_t nextIndex = (index + 1) % randomBufferSize;

    // Check if buffer needs refilling
    if (nextIndex == 0) {
        juce::ScopedLock lock(randomBufferLock);
        if (randomIndex.load(std::memory_order_acquire) >= randomBufferSize - 1) {
            for (size_t i = 0; i < randomBufferSize; ++i) {
                randomBuffer[i] = random.nextFloat();
            }
            randomIndex.store(0, std::memory_order_release);
        }
    }

    if (randomIndex.compare_exchange_strong(index, nextIndex, std::memory_order_release)) {
        return randomBuffer[index];
    }
    return randomBuffer[index]; // Fallback
}

void SimdSynthAudioProcessor::refillRandomBuffer() {
    juce::ScopedLock lock(randomBufferLock);
    randomBuffer.resize(randomBufferSize);
    for (size_t i = 0; i < randomBufferSize; ++i) {
        randomBuffer[i] = random.nextFloat();
    }
    randomIndex.store(0, std::memory_order_release);
}

// Suggest a buffer size to reduce underflow risk
int SimdSynthAudioProcessor::getPreferredBufferSize() const { return 512; }

// parameters have changed, update the engine
void SimdSynthAudioProcessor::parameterChanged(const juce::String &parameterID, float newValue) {
    // Update smoothed values or other internal states based on parameter changes
    if (parameterID == "gain") {
        smoothedGain.setTargetValue(newValue);
    } else if (parameterID == "cutoff") {
        smoothedCutoff.setTargetValue(newValue);
    } else if (parameterID == "filterMix") {
        smoothedFilterMix.setTargetValue(newValue);
    } else if (parameterID == "resonance") {
        smoothedResonance.setTargetValue(newValue);
        filter.resonance = newValue; // Update filter resonance immediately
    } else if (parameterID == "lfoRate") {
        smoothedLfoRate.setTargetValue(newValue);
    } else if (parameterID == "lfoDepth") {
        smoothedLfoDepth.setTargetValue(newValue);
    } else if (parameterID == "subTune") {
        smoothedSubTune.setTargetValue(newValue);
    } else if (parameterID == "subMix") {
        smoothedSubMix.setTargetValue(newValue);
    } else if (parameterID == "subTrack") {
        smoothedSubTrack.setTargetValue(newValue);
    } else if (parameterID == "osc2Tune") {
        smoothedOsc2Tune.setTargetValue(newValue);
    } else if (parameterID == "osc2Mix") {
        smoothedOsc2Mix.setTargetValue(newValue);
    } else if (parameterID == "osc2Track") {
        smoothedOsc2Track.setTargetValue(newValue);
    } else if (parameterID == "detune") {
        smoothedDetune.setTargetValue(newValue);
    } else if (parameterID == "attackCurve") {
        smoothedAttackCurve.setTargetValue(newValue);
    } else if (parameterID == "releaseCurve") {
        smoothedReleaseCurve.setTargetValue(newValue);
    } else if (parameterID == "wavetable" || parameterID == "attack" || parameterID == "decay" ||
               parameterID == "sustain" || parameterID == "release" || parameterID == "filterBypass" ||
               parameterID == "filterMix" || parameterID == "fegAttack" || parameterID == "fegDecay" ||
               parameterID == "fegSustain" || parameterID == "fegRelease" || parameterID == "fegAmount" ||
               parameterID == "lfoPitchAmt" || parameterID == "unison") {
        // These parameters don't have smoothed values but still require voice updates
        // No immediate action needed here; just flag for update
    } else {
        DBG("Unhandled parameter change: " << parameterID << " = " << newValue);
    }

    // Flag that parameters have changed to trigger voice updates in processBlock
    setParametersChanged();
}

// Load presets from directory
void SimdSynthAudioProcessor::loadPresetsFromDirectory() {
    presetNames.clear();
    juce::File presetDir =
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("SimdSynth/Presets");

    if (!presetDir.exists()) {
        presetDir.createDirectory();
        presetManager.createDefaultPresets();
    }

    juce::Array<juce::File> presetFiles;
    presetDir.findChildFiles(presetFiles, juce::File::findFiles, false, "*.json");
    for (const auto &file : presetFiles) {
        presetNames.add(file.getFileNameWithoutExtension());
    }

    if (presetNames.isEmpty()) {
        DBG("No presets found in directory: " << presetDir.getFullPathName());
        presetNames.add("Default");
        presetManager.createDefaultPresets();
    }
}

// Return the number of available presets
int SimdSynthAudioProcessor::getNumPrograms() { return presetNames.size(); }

// Return the current preset index
int SimdSynthAudioProcessor::getCurrentProgram() { return currentProgram; }

// Return the name of a preset by index
const juce::String SimdSynthAudioProcessor::getProgramName(int index) {
    if (index >= 0 && index < presetNames.size()) {
        return presetNames[index];
    }
    return "Default";
}

// Rename a preset
void SimdSynthAudioProcessor::changeProgramName(int index, const juce::String &newName) {
    if (index >= 0 && index < presetNames.size()) {
        juce::File oldFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                 .getChildFile("SimdSynth/Presets")
                                 .getChildFile(presetNames[index] + ".json");
        juce::File newFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                 .getChildFile("SimdSynth/Presets")
                                 .getChildFile(newName + ".json");
        if (oldFile.existsAsFile()) {
            oldFile.moveFileTo(newFile);
            presetNames.set(index, newName);
        }
    }
}

// Convert MIDI note number to frequency
float SimdSynthAudioProcessor::midiToFreq(int midiNote) { return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f); }

// Randomize a value within a variation range
float SimdSynthAudioProcessor::randomize(float base, float var) {
    float r = random.nextFloat();
    return base * (1.0f - var + r * 2.0f * var);
}

// SIMD floor function for x86_64
#if defined(__x86_64__)
__m128 SimdSynthAudioProcessor::my_floorq_f32(__m128 x) {
    return _mm_floor_ps(x); // SSE intrinsic for floor
}
#endif

// SIMD floor function for ARM64
#if defined(__aarch64__) || defined(__arm64__)
float32x4_t SimdSynthAudioProcessor::my_floorq_f32(float32x4_t x) {
    return vrndmq_f32(x); // NEON intrinsic for floor
}
#endif

// SIMD sine approximation for x86_64
#ifdef __x86_64__
__m128 SimdSynthAudioProcessor::fast_sin_ps(__m128 x) {
    const __m128 twoPi = _mm_set1_ps(2.0f * juce::MathConstants<float>::pi);
    const __m128 invTwoPi = _mm_set1_ps(1.0f / (2.0f * juce::MathConstants<float>::pi));
    __m128 q = _mm_mul_ps(x, invTwoPi);
    q = my_floorq_f32(q);
    __m128 xWrapped = _mm_sub_ps(x, _mm_mul_ps(q, twoPi));
    __m128 sign = _mm_set1_ps(1.0f);
    __m128 absX = _mm_max_ps(xWrapped, _mm_sub_ps(_mm_setzero_ps(), xWrapped));
    __m128 gtPiOverTwo = _mm_cmpgt_ps(absX, piOverTwo);
    sign = _mm_or_ps(_mm_and_ps(gtPiOverTwo, _mm_set1_ps(-1.0f)), _mm_andnot_ps(gtPiOverTwo, _mm_set1_ps(1.0f)));
    xWrapped = _mm_sub_ps(xWrapped, _mm_and_ps(gtPiOverTwo, _mm_mul_ps(piOverTwo, _mm_set1_ps(2.0f))));
    const __m128 c3 = _mm_set1_ps(-1.0f / 6.0f);
    const __m128 c5 = _mm_set1_ps(1.0f / 120.0f);
    const __m128 c7 = _mm_set1_ps(-1.0f / 5040.0f);
    __m128 x2 = _mm_mul_ps(xWrapped, xWrapped);
    __m128 x3 = _mm_mul_ps(x2, xWrapped);
    __m128 x5 = _mm_mul_ps(x3, x2);
    __m128 x7 = _mm_mul_ps(x5, x2);
    __m128 result =
        _mm_add_ps(xWrapped, _mm_add_ps(_mm_mul_ps(c3, x3), _mm_add_ps(_mm_mul_ps(c5, x5), _mm_mul_ps(c7, x7))));
    return _mm_mul_ps(result, sign);
}
#endif

// SIMD sine approximation for ARM64
#if defined(__aarch64__) || defined(__arm64__)
float32x4_t SimdSynthAudioProcessor::fast_sin_ps(float32x4_t x) {
    const float32x4_t twoPi = vdupq_n_f32(2.0f * juce::MathConstants<float>::pi);
    const float32x4_t invTwoPi = vdupq_n_f32(1.0f / (2.0f * juce::MathConstants<float>::pi));
    float32x4_t q = vmulq_f32(x, invTwoPi);
    q = my_floorq_f32(q);
    float32x4_t xWrapped = vsubq_f32(x, vmulq_f32(q, twoPi));
    float32x4_t sign = vdupq_n_f32(1.0f);
    float32x4_t absX = vmaxq_f32(xWrapped, vsubq_f32(vdupq_n_f32(0.0f), xWrapped));
    uint32x4_t gtPiOverTwo = vcgtq_f32(absX, piOverTwo);
    sign = vbslq_f32(gtPiOverTwo, vdupq_n_f32(-1.0f), vdupq_n_f32(1.0f));
    xWrapped = vsubq_f32(xWrapped, vbslq_f32(gtPiOverTwo, vmulq_f32(piOverTwo, vdupq_n_f32(2.0f)), vdupq_n_f32(0.0f)));
    const float32x4_t c3 = vdupq_n_f32(-1.0f / 6.0f);
    const float32x4_t c5 = vdupq_n_f32(1.0f / 120.0f);
    const float32x4_t c7 = vdupq_n_f32(-1.0f / 5040.0f);
    float32x4_t x2 = vmulq_f32(xWrapped, xWrapped);
    float32x4_t x3 = vmulq_f32(x2, xWrapped);
    float32x4_t x5 = vmulq_f32(x3, x2);
    float32x4_t x7 = vmulq_f32(x5, x2);
    float32x4_t result =
        vaddq_f32(xWrapped, vaddq_f32(vmulq_f32(c3, x3), vaddq_f32(vmulq_f32(c5, x5), vmulq_f32(c7, x7))));
    return vmulq_f32(result, sign);
}
#endif

// Perform wavetable lookup using SIMD-friendly interpolation
SIMD_TYPE SimdSynthAudioProcessor::wavetable_lookup_ps(SIMD_TYPE phase, SIMD_TYPE wavetableTypes) {
    phase = SIMD_SUB(phase, SIMD_FLOOR(phase)); // Normalize to [0, 1]
    SIMD_TYPE index = SIMD_MUL(phase, SIMD_SET1(static_cast<float>(WAVETABLE_SIZE - 1)));
    SIMD_TYPE indexFloor = SIMD_FLOOR(index);
    SIMD_TYPE frac = SIMD_SUB(index, indexFloor);
    float tempIndices[4], tempTypes[4], tempOut[4];
    SIMD_STORE(tempIndices, indexFloor);
    float tempFrac[4];
    SIMD_STORE(tempFrac, frac);
    SIMD_STORE(tempTypes, wavetableTypes);
    for (int i = 0; i < 4; ++i) {
        int idx = static_cast<int>(tempIndices[i]);
        // Clamp index to prevent out-of-bounds access
        idx = juce::jlimit(0, WAVETABLE_SIZE - 2, idx);
        float f = tempFrac[i];
        const float *table;
        switch (static_cast<int>(tempTypes[i])) {
        case 0:
            table = sineTable.data();
            break;
        case 1:
            table = sawTable.data();
            break;
        case 2:
            table = squareTable.data();
            break;
        default:
            table = sineTable.data();
            tempOut[i] = 0.0f;
            continue;
        }
        tempOut[i] = table[idx] + f * (table[idx + 1] - table[idx]); // Linear interpolation
    }
    return SIMD_LOAD(tempOut);
}

void SimdSynthAudioProcessor::applyLadderFilter(Voice *voices, int voiceOffset, SIMD_TYPE input, Filter &filter,
                                                SIMD_TYPE &output) {
    if (filter.sampleRate <= 0.0f) {
        output = SIMD_SET1(0.0f);
        return;
    }

    // Initialize filter states for new voices
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        if (idx < MAX_VOICE_POLYPHONY && voices[idx].active && voices[idx].noteOnTime == currentTime) {
            for (int j = 0; j < 4; j++) {
                voices[idx].filterStates[j] = 0.0f;
            }
        }
    }

    // Vectorized cutoff computation
    alignas(32) float tempCutoffs[4], tempEnvMods[4], tempResonances[4];
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        tempCutoffs[i] =
            idx < MAX_VOICE_POLYPHONY && voices[idx].active ? voices[idx].smoothedCutoff.getNextValue() : 1000.0f;
        float egMod = idx < MAX_VOICE_POLYPHONY && voices[idx].active
                          ? voices[idx].smoothedFilterEnv.getNextValue() * voices[idx].smoothedFegAmount.getNextValue()
                          : 0.0f;
        egMod = juce::jlimit(-1.0f, 1.0f, egMod);
        tempEnvMods[i] = juce::jlimit(-2000.0f, 2000.0f, egMod * 2000.0f);

        tempCutoffs[i] = juce::jlimit(20.0f, filter.sampleRate * 0.4f, tempCutoffs[i] + tempEnvMods[i]);
        // tempResonances[i] = idx < MAX_VOICE_POLYPHONY && voices[idx].active ? voices[idx].resonance : 0.7f;
        tempResonances[i] = juce::jlimit(0.0f, 0.5f, tempResonances[i] * (1.0f - 0.4f * tempCutoffs[i] / (filter.sampleRate * 0.4f)));

        // Clear states for inactive voices
        for (int i = 0; i < 4; i++) {
            int idx = voiceOffset + i;
            if (idx < MAX_VOICE_POLYPHONY && !voices[idx].active) {
                for (int j = 0; j < 4; j++) {
                    voices[idx].filterStates[j] = 0.0f;
                }
            }
        }
    }

    SIMD_TYPE modulatedCutoffs = SIMD_LOAD(tempCutoffs);
    float tempModulated[4];
    SIMD_STORE(tempModulated, modulatedCutoffs);
    for (int i = 0; i < 4; i++) {
        tempCutoffs[i] = juce::jlimit(20.0f, filter.sampleRate * 0.45f, tempCutoffs[i] + tempEnvMods[i]);
        float wc = 2.0f * juce::MathConstants<float>::pi * tempCutoffs[i] / filter.sampleRate;
        tempCutoffs[i] = std::tan(wc / 2.0f);
        if (std::isnan(tempCutoffs[i]) || !std::isfinite(tempCutoffs[i]) || tempCutoffs[i] > 10.0f) {
            tempCutoffs[i] = 0.1f;
        }
    }
    SIMD_TYPE alpha = SIMD_SET(tempCutoffs[0], tempCutoffs[1], tempCutoffs[2], tempCutoffs[3]);
    SIMD_TYPE resonance = SIMD_LOAD(tempResonances);

    bool anyActive = false;
    for (int i = 0; i < 4; i++) {
        if (voiceOffset + i < MAX_VOICE_POLYPHONY && voices[voiceOffset + i].active) {
            anyActive = true;
        }
    }
    if (!anyActive) {
        output = SIMD_SET1(0.0f);
        return;
    }

    // explicitly check voice activity
    SIMD_TYPE states[4];
    for (int i = 0; i < 4; i++) {
        float temp[4];
        for (int j = 0; j < 4; ++j) {
            int idx = voiceOffset + j;
            temp[j] = (idx < MAX_VOICE_POLYPHONY && voices[idx].active) ? voices[idx].filterStates[i] : 0.0f;
        }
        states[i] = SIMD_LOAD(temp);
    }

    // Apply filter with clipping
    SIMD_TYPE feedback = SIMD_MUL(states[3], resonance);
    SIMD_TYPE filterInput = SIMD_SUB(input, feedback);
    states[0] = SIMD_ADD(states[0], SIMD_MUL(alpha, SIMD_SUB(filterInput, states[0])));
    states[0] = SIMD_MAX(SIMD_SET1(-1.0f), SIMD_MIN(states[0], SIMD_SET1(1.0f)));
    states[1] = SIMD_ADD(states[1], SIMD_MUL(alpha, SIMD_SUB(states[0], states[1])));
    states[1] = SIMD_MAX(SIMD_SET1(-1.0f), SIMD_MIN(states[1], SIMD_SET1(1.0f)));
    states[2] = SIMD_ADD(states[2], SIMD_MUL(alpha, SIMD_SUB(states[1], states[2])));
    states[2] = SIMD_MAX(SIMD_SET1(-1.0f), SIMD_MIN(states[2], SIMD_SET1(1.0f)));
    states[3] = SIMD_ADD(states[3], SIMD_MUL(alpha, SIMD_SUB(states[2], states[3])));
    states[3] = SIMD_MAX(SIMD_SET1(-1.0f), SIMD_MIN(states[3], SIMD_SET1(1.0f)));
    output = states[3];

    // Apply soft clipping
    float tempOut[4];
    SIMD_STORE(tempOut, output);
    for (int i = 0; i < 4; i++) {
        float over = std::abs(tempOut[i]) > 1.0f ? (tempOut[i] > 0 ? 1.0f : -1.0f) : tempOut[i];
        tempOut[i] = over - (over * over * over) / 3.0f;
        if (!std::isfinite(tempOut[i])) {
            DBG("Filter output NaN at voiceOffset " << voiceOffset << " lane " << i);
            tempOut[i] = 0.0f;
        }
    }
    output = SIMD_LOAD(tempOut);

    // Store states
    for (int i = 0; i < 4; i++) {
        float temp[4];
        SIMD_STORE(temp, states[i]);
        for (int j = 0; j < 4; ++j) {
            int idx = voiceOffset + j;
            if (idx < MAX_VOICE_POLYPHONY && voices[idx].active) {
                voices[idx].filterStates[i] = temp[j];
            }
        }
    }
}

// Find a voice to steal for new notes
int SimdSynthAudioProcessor::findVoiceToSteal() {
    int voiceToSteal = 0;
    float highestPriority = -1.0f;

    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        float priority = 0.0f;
        // FIX: Avoid stealing voices in attack/decay with high amplitude
        if (voices[i].released) {
            priority = 1000.0f + (voices[i].amplitude > 0.001f ? voices[i].releaseStartAmplitude : 0.0f);
        } else if (!voices[i].isHeld) {
            priority = 500.0f + voices[i].voiceAge;
        } else if (voices[i].amplitude < 0.5f) { // Prioritize quieter voices
            priority = 250.0f + voices[i].voiceAge;
        } else {
            priority = voices[i].voiceAge;
        }

        if (priority > highestPriority) {
            highestPriority = priority;
            voiceToSteal = i;
        }
    }

    if (voices[voiceToSteal].active && !voices[voiceToSteal].released) {
        // FIX: Apply short fade-out to reduce clicks
        voices[voiceToSteal].smoothedAmplitude.setTargetValue(0.0f);
        voices[voiceToSteal].smoothedAmplitude.reset(filter.sampleRate * oversampling->getOversamplingFactor(), 0.01f);
        voices[voiceToSteal].smoothedFilterEnv.setTargetValue(0.0f);
        voices[voiceToSteal].smoothedFilterEnv.reset(filter.sampleRate * oversampling->getOversamplingFactor(), 0.01f);
        for (int j = 0; j < 4; j++) {
            voices[voiceToSteal].filterStates[j] = 0.0f;
        }
        voices[voiceToSteal].phase = 0.0f;
        voices[voiceToSteal].subPhase = 0.0f;
        voices[voiceToSteal].osc2Phase = 0.0f;
        voices[voiceToSteal].lfoPhase = 0.0f;
        voices[voiceToSteal].mainLPState = 0.0f;
        voices[voiceToSteal].subLPState = 0.0f;
        voices[voiceToSteal].osc2LPState = 0.0f;
        voices[voiceToSteal].dcState = 0.0f;
    }

    return voiceToSteal;
}

void SimdSynthAudioProcessor::updateEnvelopes(float t) {
    for (int i = 0; i < MAX_VOICE_POLYPHONY; i++) {
        if (!voices[i].active) {
            voices[i].amplitude = 0.0f;
            voices[i].filterEnv = 0.0f;
            voices[i].smoothedAmplitude.setCurrentAndTargetValue(0.0f);
            voices[i].smoothedFilterEnv.setCurrentAndTargetValue(0.0f);
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] *= 0.999f;
            }
            continue;
        }

        float localTime = std::max(0.0f, t - voices[i].noteOnTime);
        float attack = juce::jmax(voices[i].attack, 0.02f);
        float decay = std::max(voices[i].decay, 0.02f);
        float sustain = juce::jlimit(0.0f, 1.0f, voices[i].sustain);
        float release = std::max(voices[i].release, 0.02f);
        float velScale = 1.0f / (0.3f + 0.7f * voices[i].velocity);
        attack *= velScale;
        float attackCurve = juce::jlimit(0.5f, 3.0f, voices[i].attackCurve);
        const float decayCurve = 1.5f;
        float releaseCurve = juce::jlimit(0.5f, 3.0f, voices[i].releaseCurve);

        // Amplitude envelope
        float amplitude;
        if (localTime < attack) {
            float attackPhase = localTime / attack;
            amplitude = std::pow(juce::jlimit(0.0f, 1.0f, attackPhase), attackCurve);
        } else if (localTime < attack + decay) {
            float decayPhase = (localTime - attack) / decay;
            amplitude = 1.0f - std::pow(decayPhase, decayCurve) * (1.0f - sustain);
        } else if (!voices[i].released) {
            amplitude = sustain;
        } else {
            float releaseTime = std::max(0.0f, t - voices[i].noteOffTime);
            float releasePhase = releaseTime / release;
            amplitude = voices[i].releaseStartAmplitude * (1.0f - std::pow(releasePhase, releaseCurve));
            if (amplitude <= 0.001f) {
                amplitude = 0.0f;
                voices[i].active = false;
                voices[i].smoothedAmplitude.setCurrentAndTargetValue(0.0f);
                voices[i].smoothedFilterEnv.setCurrentAndTargetValue(0.0f);
                for (int j = 0; j < 4; j++) {
                    voices[i].filterStates[j] *= 0.0;
                }
            }
        }

        voices[i].amplitude = juce::jlimit(0.0f, 1.0f, amplitude);
        // FIX: Smooth rampTime transition
        float rampTime = voices[i].released ? 0.01f : 0.005f;
        if (voices[i].released && voices[i].smoothedAmplitude.getCurrentValue() > 0.5f) {
            rampTime = 0.0075f; // Interpolate during release transition
        }
        voices[i].smoothedAmplitude.reset(filter.sampleRate * oversampling->getOversamplingFactor(), rampTime);
        voices[i].smoothedAmplitude.setTargetValue(voices[i].amplitude);

        // Filter envelope
        float filterEnv;
        if (localTime < voices[i].fegAttack) {
            float attackPhase = localTime / voices[i].fegAttack;
            filterEnv = std::pow(attackPhase, attackCurve);
        } else if (localTime < voices[i].fegAttack + voices[i].fegDecay) {
            float decayPhase = (localTime - voices[i].fegAttack) / voices[i].fegDecay;
            filterEnv = 1.0f - std::pow(decayPhase, decayCurve) * (1.0f - voices[i].fegSustain);
        } else if (!voices[i].released) {
            filterEnv = voices[i].fegSustain;
        } else {
            float releaseTime = std::max(0.0f, t - voices[i].noteOffTime);
            float releasePhase = releaseTime / voices[i].fegRelease;
            filterEnv = voices[i].fegSustain * (1.0f - std::pow(releasePhase, releaseCurve));
        }

        voices[i].filterEnv = juce::jlimit(0.0f, 1.0f, filterEnv);
        voices[i].smoothedFilterEnv.reset(filter.sampleRate * oversampling->getOversamplingFactor(), rampTime);
        voices[i].smoothedFilterEnv.setTargetValue(voices[i].filterEnv);
    }
}

// Update Voice Parameters
void SimdSynthAudioProcessor::updateVoiceParameters(float sampleRate, bool forceUpdate) {
    sampleRate = std::max(sampleRate, 44100.0f);
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        if (!voices[i].active && !forceUpdate) continue;
        voices[i].attack = *attackTimeParam;
        voices[i].decay = *decayTimeParam;
        voices[i].sustain = *sustainLevelParam;
        voices[i].release = *releaseTimeParam;
        voices[i].attackCurve = smoothedAttackCurve.getCurrentValue();
        voices[i].releaseCurve = smoothedReleaseCurve.getCurrentValue();
        voices[i].cutoff = *cutoffParam;
        voices[i].resonance = *resonanceParam;
        voices[i].filterBypass = *filterBypassParam;
        voices[i].fegAttack = *fegAttackParam;
        voices[i].fegDecay = *fegDecayParam;
        voices[i].fegSustain = *fegSustainParam;
        voices[i].fegRelease = *fegReleaseParam;
        voices[i].fegAmount = *fegAmountParam;
        voices[i].lfoRate = smoothedLfoRate.getCurrentValue();
        voices[i].lfoDepth = smoothedLfoDepth.getCurrentValue();
        voices[i].lfoPitchAmt = *lfoPitchAmtParam;
        voices[i].subTune = smoothedSubTune.getCurrentValue();
        voices[i].subMix = smoothedSubMix.getCurrentValue();
        voices[i].subTrack = smoothedSubTrack.getCurrentValue();
        voices[i].osc2Tune = smoothedOsc2Tune.getCurrentValue();
        voices[i].osc2Mix = smoothedOsc2Mix.getCurrentValue();
        voices[i].osc2Track = smoothedOsc2Track.getCurrentValue();
        voices[i].detune = smoothedDetune.getCurrentValue();
        voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
        voices[i].smoothedCutoff.setTargetValue(*cutoffParam);
        voices[i].smoothedFegAmount.setTargetValue(*fegAmountParam);
        if (voices[i].detune != *detuneParam || voices[i].unison != static_cast<int>(*unisonParam)) {
            voices[i].unison = juce::jlimit(1, maxUnison, static_cast<int>(*unisonParam));
            voices[i].detune = *detuneParam;
            for (int u = 0; u < voices[i].unison; ++u) {
                float detuneCents =
                    voices[i].detune * (u - (voices[i].unison - 1) / 2.0f) / (voices[i].unison - 1 + 0.0001f);
                voices[i].detuneFactors[u] = powf(2.0f, detuneCents / 12.0f);
                // FIX: Always reinitialize unison phases for consistency
                voices[i].unisonPhases[u] = getRandomFloatAudioThread() * 0.01f;
            }
        }
        if (voices[i].active) {
            voices[i].phaseIncrement = voices[i].frequency / sampleRate;
            const float twoPi = 2.0f * juce::MathConstants<float>::pi;
            voices[i].subPhaseIncrement =
                voices[i].frequency * powf(2.0f, voices[i].subTune / 12.0f) * voices[i].subTrack / sampleRate * twoPi;
            voices[i].osc2PhaseIncrement =
                voices[i].frequency * powf(2.0f, voices[i].osc2Tune / 12.0f) * voices[i].osc2Track / sampleRate * twoPi;
        }
    }
}

// Prepare to Play
void SimdSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    filter.sampleRate = static_cast<float>(sampleRate);
    currentTime = 0.0;
    // FIX: Use fixed 4x oversampling for consistency
    int oversamplingFactor = 4;
    if (!oversampling || oversampling->getOversamplingFactor() != oversamplingFactor ||
        oversampling->getLatencyInSamples() != samplesPerBlock * oversamplingFactor) {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
            2, oversamplingFactor, juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR, true, true);
        oversampling->initProcessing(samplesPerBlock);
    } // Initialize smoothed parameters with actual sample rate
    smoothedGain.reset(sampleRate, 0.01);
    smoothedCutoff.reset(sampleRate, 0.01);
    smoothedResonance.reset(sampleRate, 0.01);
    smoothedLfoRate.reset(sampleRate, 0.01);
    smoothedLfoDepth.reset(sampleRate, 0.01);
    smoothedSubMix.reset(sampleRate, 0.01);
    smoothedSubTune.reset(sampleRate, 0.01);
    smoothedSubTrack.reset(sampleRate, 0.01);
    smoothedDetune.reset(sampleRate, 0.01);
    smoothedOsc2Mix.reset(sampleRate, 0.01);
    smoothedOsc2Tune.reset(sampleRate, 0.01);
    smoothedOsc2Track.reset(sampleRate, 0.01);
    smoothedAttackCurve.reset(sampleRate, 0.01);
    smoothedReleaseCurve.reset(sampleRate, 0.01);
    smoothedFilterMix.reset(sampleRate, 0.01);

    // Reset voices
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i].active = false;
        voices[i].released = false;
        voices[i].amplitude = 0.0f;
        voices[i].velocity = 0.0f;
        voices[i].noteOnTime = 0.0f;
        voices[i].noteOffTime = 0.0f;
        voices[i].phase = 0.0f;
        voices[i].subPhase = 0.0f;
        voices[i].osc2Phase = 0.0f;
        voices[i].lfoPhase = 0.0f;
        voices[i].lfoPitchAmt = *lfoPitchAmtParam;
        voices[i].mainLPState = 0.0f;
        voices[i].subLPState = 0.0f;
        voices[i].osc2LPState = 0.0f;
        voices[i].dcState = 0.0f;

        voices[i].smoothedAmplitude.reset(sampleRate * oversamplingFactor, 0.01);
        voices[i].smoothedFilterEnv.reset(sampleRate * oversamplingFactor, 0.01);
        voices[i].smoothedCutoff.reset(sampleRate * oversamplingFactor, 0.01);
        voices[i].smoothedFegAmount.reset(sampleRate * oversamplingFactor, 0.01);

        for (int j = 0; j < 4; j++) {
            voices[i].filterStates[j] = 0.0f;
        }
    }

    updateVoiceParameters(static_cast<float>(sampleRate) * oversamplingFactor, true);
}

// Load a Preset
void SimdSynthAudioProcessor::setCurrentProgram(int index) {
    juce::StringArray paramIds = {"wavetable",    "attack",       "decay",     "sustain",   "release",   "attackCurve",
                                  "releaseCurve", "filterBypass", "cutoff",    "resonance", "fegAttack", "fegDecay",
                                  "fegSustain",   "fegRelease",   "fegAmount", "lfoRate",   "lfoDepth",  "lfoPitchAmt",
                                  "subTune",      "subMix",       "subTrack",  "osc2Tune",  "osc2Mix",   "osc2Track",
                                  "gain",         "unison",       "detune"};

    if (index < 0 || index >= presetNames.size()) {
        DBG("Error: Invalid preset index: " << index);
        return;
    }

    currentProgram = index;
    juce::File presetFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                .getChildFile("SimdSynth/Presets")
                                .getChildFile(presetNames[index] + ".json");

    if (!presetFile.existsAsFile()) {
        DBG("Error: Preset file not found: " << presetFile.getFullPathName());
        return;
    }

    auto jsonString = presetFile.loadFileAsString();
    auto parsedJson = juce::JSON::parse(jsonString);

    // FIX: Stricter JSON validation
    if (!parsedJson.isObject()) {
        DBG("Error: Invalid JSON format in preset: " << presetNames[index]);
        return;
    }

    juce::var synthParams = parsedJson.getProperty("SimdSynth", juce::var());
    if (!synthParams.isObject()) {
        DBG("Error: 'SimdSynth' object not found in preset: " << presetNames[index]);
        for (const auto &paramId : paramIds) {
            if (auto *param = parameters.getParameter(paramId)) {
                if (auto *floatParam = dynamic_cast<juce::AudioParameterFloat *>(param)) {
                    float value = defaultParamValues[paramId];
                    floatParam->setValueNotifyingHost(floatParam->convertTo0to1(value));
                }
            }
        }
        return;
    }

    bool anyParamUpdated = false;
    for (const auto &paramId : paramIds) {
        if (auto *param = parameters.getParameter(paramId)) {
            if (auto *floatParam = dynamic_cast<juce::AudioParameterFloat *>(param)) {
                float value = defaultParamValues[paramId];
                juce::var prop = synthParams.getProperty(paramId, juce::var());
                if (!prop.isVoid()) {
                    if (prop.isDouble() || prop.isInt() || prop.isInt64()) {
                        value = static_cast<float>(prop);
                    } else {
                        DBG("Warning: Invalid type for " << paramId << " in preset: " << presetNames[index]);
                        continue;
                    }
                } else {
                    DBG("Warning: Missing parameter " << paramId << " in preset: " << presetNames[index]);
                }
                if (paramId == "wavetable" || paramId == "unison") {
                    value = std::round(value);
                }
                value = juce::jlimit(floatParam->getNormalisableRange().start, floatParam->getNormalisableRange().end,
                                     value);
                floatParam->setValueNotifyingHost(floatParam->convertTo0to1(value));
                anyParamUpdated = true;
            }
        }
    }

    if (!anyParamUpdated) {
        DBG("Warning: No parameters updated for preset: " << presetNames[index]);
    } else {
        setParametersChanged();
    }

    filter.resonance = *resonanceParam;
    updateVoiceParameters(filter.sampleRate * oversampling->getOversamplingFactor(), true);

    smoothedGain.setCurrentAndTargetValue(*gainParam);
    smoothedCutoff.setCurrentAndTargetValue(*cutoffParam);
    smoothedResonance.setCurrentAndTargetValue(*resonanceParam);
    smoothedFilterMix.setCurrentAndTargetValue(*filterMixParam);
    smoothedLfoRate.setCurrentAndTargetValue(*lfoRateParam);
    smoothedLfoDepth.setCurrentAndTargetValue(*lfoDepthParam);
    smoothedSubMix.setCurrentAndTargetValue(*subMixParam);
    smoothedSubTune.setCurrentAndTargetValue(*subTuneParam);
    smoothedSubTrack.setCurrentAndTargetValue(*subTrackParam);
    smoothedOsc2Mix.setCurrentAndTargetValue(*osc2MixParam);
    smoothedOsc2Tune.setCurrentAndTargetValue(*osc2TuneParam);
    smoothedOsc2Track.setCurrentAndTargetValue(*osc2TrackParam);
    smoothedDetune.setCurrentAndTargetValue(*detuneParam);
    smoothedAttackCurve.setCurrentAndTargetValue(*attackCurveParam);
    smoothedReleaseCurve.setCurrentAndTargetValue(*releaseCurveParam);

    if (auto *editor = getActiveEditor()) {
        if (auto *synthEditor = dynamic_cast<SimdSynthAudioProcessorEditor *>(editor)) {
            synthEditor->updatePresetComboBox();
        }
    }
}

// Release resources
void SimdSynthAudioProcessor::releaseResources() { oversampling->reset(); }

// Process audio and MIDI with oversampling:
// Process Single Sample
void SimdSynthAudioProcessor::processSingleSample(int sampleIndex, juce::dsp::AudioBlock<float> &oversampledBlock,
                                                  double blockStartTime, float sampleRate, float voiceScaling,
                                                  int totalNumOutputChannels,
                                                  std::function<float(float, float)> wavetable_lookup_scalar) {
    float t = static_cast<float>(blockStartTime + static_cast<double>(sampleIndex) / sampleRate);
    updateEnvelopes(t);
    float outputSampleL = 0.0f, outputSampleR = 0.0f;
    const float twoPiScalar = 2.0f * juce::MathConstants<float>::pi;

    for (int batch = 0; batch < NUM_BATCHES; batch++) {
        const int voiceOffset = batch * SIMD_WIDTH;
        if (voiceOffset >= MAX_VOICE_POLYPHONY) continue;
        bool anyActive = false;
        for (int j = 0; j < SIMD_WIDTH && voiceOffset + j < MAX_VOICE_POLYPHONY; ++j) {
            if (voices[voiceOffset + j].active) {
                anyActive = true;
                break;
            }
        }
        if (!anyActive) continue;

        alignas(32) float batchCombined[SIMD_WIDTH] = {0.0f};
        alignas(32) float batchUnisonL[SIMD_WIDTH] = {0.0f};
        alignas(32) float batchUnisonR[SIMD_WIDTH] = {0.0f};
        alignas(32) float batchSub[SIMD_WIDTH] = {0.0f};
        alignas(32) float batchOsc2[SIMD_WIDTH] = {0.0f};

        for (int j = 0; j < SIMD_WIDTH && (voiceOffset + j) < MAX_VOICE_POLYPHONY; ++j) {
            int idx = voiceOffset + j;
            if (!voices[idx].active) continue;

            float amp = voices[idx].smoothedAmplitude.getNextValue() * voices[idx].velocity;
            float phase = voices[idx].phase;
            float increment = voices[idx].phaseIncrement;
            float lfoPhase = voices[idx].lfoPhase;
            float lfoRate = voices[idx].lfoRate;
            float lfoDepth = voices[idx].lfoDepth;
            float subPhase = voices[idx].subPhase;
            float subIncrement = voices[idx].subPhaseIncrement;
            float subMix = voices[idx].subMix;
            float osc2Phase = voices[idx].osc2Phase;
            float osc2Increment = voices[idx].osc2PhaseIncrement;
            float osc2Mix = voices[idx].osc2Mix;
            int wavetableType = voices[idx].wavetableType;

            lfoPhase += lfoRate * twoPiScalar / sampleRate;
            lfoPhase -= std::floor(lfoPhase / twoPiScalar) * twoPiScalar; // FIX: Faster phase wrapping
            float lfoVal = std::sin(lfoPhase) * lfoDepth;
            float phaseMod_cycles = lfoVal / twoPiScalar;
            float lfoPitchMod = lfoVal * voices[idx].lfoPitchAmt;
            float effectiveIncr = increment * (1.0f + lfoPitchMod);

            float unisonOutputL = 0.0f, unisonOutputR = 0.0f;
            int unisonVoices = voices[idx].unison;
            for (int u = 0; u < unisonVoices; ++u) {
                float detuneFactor = voices[idx].detuneFactors[u];
                float detunedPhase = (phase + phaseMod_cycles + voices[idx].unisonPhases[u]) * detuneFactor;
                float phasesNorm = detunedPhase - std::floor(detunedPhase); // FIX: Faster phase wrapping
                float mainVal = wavetable_lookup_scalar(phasesNorm, static_cast<float>(wavetableType));

                float fc = voices[idx].frequency * detuneFactor * 0.45f;
                float alphaLP = std::exp(-2.0f * juce::MathConstants<float>::pi * fc / sampleRate);
                float filteredMain = alphaLP * voices[idx].mainLPState + (1.0f - alphaLP) * mainVal;
                voices[idx].mainLPState = filteredMain;

                float uPan =
                    (unisonVoices > 1) ? (static_cast<float>(u) / (unisonVoices - 1) * 2.0f - 1.0f) * 0.5f : 0.0f;
                float panScale = juce::jlimit(0.0f, 1.0f, voices[idx].detune / 0.05f);
                uPan *= panScale;
                float leftGain = (1.0f - uPan) * 0.5f + 0.5f;
                float rightGain = (1.0f + uPan) * 0.5f + 0.5f;
                unisonOutputL += filteredMain * leftGain / static_cast<float>(unisonVoices);
                unisonOutputR += filteredMain * rightGain / static_cast<float>(unisonVoices);
            }

            float totalMix = 1.0f + subMix + osc2Mix;
            totalMix = std::max(totalMix, 1e-6f);
            float mainMix = 2.0f / totalMix;
            float subMixNorm = subMix / totalMix;
            float osc2MixNorm = osc2Mix / totalMix;
            unisonOutputL *= amp * mainMix;
            unisonOutputR *= amp * mainMix;

            float subPhasesMod = subPhase + phaseMod_cycles * twoPiScalar;
            float subSinVal = std::sin(subPhasesMod);
            float fcSub = voices[idx].frequency * powf(2.0f, voices[idx].subTune / 12.0f) * 1.0f;
            float alphaSub = std::exp(-2.0f * juce::MathConstants<float>::pi * fcSub / sampleRate);
            float filteredSub = alphaSub * voices[idx].subLPState + (1.0f - alphaSub) * subSinVal;
            voices[idx].subLPState = filteredSub;
            filteredSub *= amp * subMixNorm;

            float osc2PhasesMod = osc2Phase + phaseMod_cycles * twoPiScalar;
            float osc2PhasesCycles =
                (osc2PhasesMod / twoPiScalar) - std::floor(osc2PhasesMod / twoPiScalar); // FIX: Faster phase wrapping
            float osc2Val = wavetable_lookup_scalar(osc2PhasesCycles, static_cast<float>(wavetableType));
            float fcOsc2 = voices[idx].frequency * powf(2.0f, voices[idx].osc2Tune / 12.0f) * 1.0f;
            float alphaOsc2 = std::exp(-2.0f * juce::MathConstants<float>::pi * fcOsc2 / sampleRate);
            float filteredOsc2 = alphaOsc2 * voices[idx].osc2LPState + (1.0f - alphaOsc2) * osc2Val;
            voices[idx].osc2LPState = filteredOsc2;
            filteredOsc2 *= amp * osc2MixNorm;

            float combinedMono = ((unisonOutputL + unisonOutputR) * 0.5f + filteredSub + filteredOsc2) * 2.0f;
            batchCombined[j] = combinedMono;
            batchUnisonL[j] = unisonOutputL;
            batchUnisonR[j] = unisonOutputR;
            batchSub[j] = filteredSub;
            batchOsc2[j] = filteredOsc2;

            // Update phases
            voices[idx].phase = phase + effectiveIncr - std::floor(phase + effectiveIncr); // FIX: Faster phase wrapping
            voices[idx].subPhase =
                subPhase + subIncrement - std::floor((subPhase + subIncrement) / twoPiScalar) * twoPiScalar;
            voices[idx].osc2Phase =
                osc2Phase + osc2Increment - std::floor((osc2Phase + osc2Increment) / twoPiScalar) * twoPiScalar;
            voices[idx].lfoPhase = lfoPhase;
        }

        float filterBypass = *parameters.getRawParameterValue("filterBypass");
        if (anyActive) {
            if (filterBypass > 0.5f) {
                for (int k = 0; k < SIMD_WIDTH && voiceOffset + k < MAX_VOICE_POLYPHONY; ++k) {
                    if (voices[voiceOffset + k].active) {
                        float pan = (static_cast<int>(voiceOffset + k) % 2 * 2.0f - 1.0f) * 0.5f *
                                    (voices[voiceOffset + k].unison / 8.0f);
                        float leftGain = (1.0f - pan) * 0.5f + 0.5f;
                        float rightGain = (1.0f + pan) * 0.5f + 0.5f;
                        outputSampleL += (batchUnisonL[k] + batchSub[k] + batchOsc2[k]) * leftGain;
                        outputSampleR += (batchUnisonR[k] + batchSub[k] + batchOsc2[k]) * rightGain;
                    }
                }
            } else {
                SIMD_TYPE combinedValues = SIMD_LOAD(batchCombined);
                SIMD_TYPE filteredOutput;
                applyLadderFilter(voices, voiceOffset, combinedValues, filter, filteredOutput);
                float temp[SIMD_WIDTH];
                SIMD_STORE(temp, filteredOutput);
                for (int k = 0; k < SIMD_WIDTH && voiceOffset + k < MAX_VOICE_POLYPHONY; ++k) {
                    if (voices[voiceOffset + k].active) {
                        float filtered = temp[k];
                        // FIX: Adjust DC blocker cutoff
                        float dcCutoff = juce::jlimit(5.0f, 20.0f, 10.0f * (sampleRate / 44100.0f)); // Lower range
                        float alphaDC = std::exp(-2.0f * juce::MathConstants<float>::pi * dcCutoff / sampleRate);
                        float dcOut = filtered - alphaDC * voices[voiceOffset + k].dcState;
                        voices[voiceOffset + k].dcState = dcOut;
                        filtered = dcOut;
                        filtered = std::tanh(filtered * 0.8f);
                        float pan = (static_cast<int>(voiceOffset + k) % 2 * 2.0f - 1.0f) * 0.5f *
                                    (voices[voiceOffset + k].unison / 8.0f);
                        float leftGain = (1.0f - pan) * 0.5f + 0.5f;
                        float rightGain = (1.0f + pan) * 0.5f + 0.5f;
                        float filterMix = smoothedFilterMix.getNextValue();
                        float dryGain = 1.0f - filterMix;
                        outputSampleL += (batchUnisonL[k] + batchSub[k] + batchOsc2[k]) * dryGain * leftGain +
                                         filtered * filterMix * leftGain;
                        outputSampleR += (batchUnisonR[k] + batchSub[k] + batchOsc2[k]) * dryGain * rightGain +
                                         filtered * filterMix * rightGain;
                    }
                }
            }
        }
    }

    outputSampleL *= voiceScaling * smoothedGain.getNextValue();
    outputSampleR *= voiceScaling * smoothedGain.getNextValue();

    if (std::isnan(outputSampleL) || !std::isfinite(outputSampleL)) outputSampleL = 0.0f;
    if (std::isnan(outputSampleR) || !std::isfinite(outputSampleR)) outputSampleR = 0.0f;

    if (totalNumOutputChannels > 0) oversampledBlock.setSample(0, sampleIndex, outputSampleL);
    if (totalNumOutputChannels > 1) oversampledBlock.setSample(1, sampleIndex, outputSampleR);
}

// Process Block
void SimdSynthAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    buffer.clear();

    // Create audio block and apply oversampling
    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(block);

    // Calculate sample rates
    float sampleRate = filter.sampleRate * oversampling->getOversamplingFactor();
    float inputSampleRate = filter.sampleRate; // Original sample rate
    double blockStartTime = currentTime;

    // Update filter resonance
    filter.resonance = smoothedResonance.getNextValue();

    // Update parameters if changed
    if (parametersChanged.exchange(false, std::memory_order_acquire)) {
        updateVoiceParameters(sampleRate, true);
    }

    // Calculate voice scaling
    int activeCount = 0;
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        if (voices[i].active) ++activeCount;
    }
    float voiceScaling = (activeCount > 0) ? (1.0f / std::sqrt(static_cast<float>(activeCount))) : 1.0f;

    // Wavetable lookup lambda
    auto wavetable_lookup_scalar = [&](float ph, float wt) -> float {
        ph -= std::floor(ph);
        float index = ph * static_cast<float>(WAVETABLE_SIZE - 1);
        int idx = static_cast<int>(std::floor(index));
        float frac = index - static_cast<float>(idx);
        idx = juce::jlimit(0, WAVETABLE_SIZE - 2, idx);
        const float *table;
        switch (static_cast<int>(wt)) {
        case 0:
            table = sineTable.data();
            break;
        case 1:
            table = sawTable.data();
            break;
        case 2:
            table = squareTable.data();
            break;
        default:
            table = sineTable.data();
            break;
        }
        return table[idx] + frac * (table[idx + 1] - table[idx]);
    };

    // Process MIDI events in the input buffer's time domain
    for (const auto metadata : midiMessages) {
        // Map input sample position to oversampled domain
        int samplePosition = static_cast<int>(metadata.samplePosition * oversampling->getOversamplingFactor());
        samplePosition = juce::jmin(samplePosition, static_cast<int>(oversampledBlock.getNumSamples()) - 1);
        auto msg = metadata.getMessage();

        if (msg.isNoteOn()) {
            int note = msg.getNoteNumber();
            float velocity = 0.7f + (msg.getVelocity() / 127.0f) * 0.3f;
            DBG("MIDI Note on: " << note << " Velocity: " << velocity);

            int voiceIndex = -1;
            for (int j = 0; j < MAX_VOICE_POLYPHONY; ++j) {
                if (!voices[j].active) {
                    voiceIndex = j;
                    break;
                }
            }
            if (voiceIndex == -1) {
                voiceIndex = findVoiceToSteal();
            }

            voices[voiceIndex].active = true;
            voices[voiceIndex].released = false;
            voices[voiceIndex].isHeld = true;
            voices[voiceIndex].smoothedAmplitude.setCurrentAndTargetValue(0.0f);
            voices[voiceIndex].smoothedAmplitude.reset(sampleRate, 0.02);
            voices[voiceIndex].smoothedFilterEnv.setCurrentAndTargetValue(0.0f);
            voices[voiceIndex].smoothedFilterEnv.reset(sampleRate, 0.02);
            voices[voiceIndex].frequency = midiToFreq(note);
            voices[voiceIndex].phaseIncrement = voices[voiceIndex].frequency / sampleRate;

            float initialOffset = (voices[voiceIndex].wavetableType == 0) ? getRandomFloatAudioThread() * 0.01f : 0.0f;
            voices[voiceIndex].phase = initialOffset;
            voices[voiceIndex].subPhase = initialOffset * 2.0f * juce::MathConstants<float>::pi;
            voices[voiceIndex].osc2Phase = initialOffset * 2.0f * juce::MathConstants<float>::pi;
            voices[voiceIndex].lfoPhase = getRandomFloatAudioThread() * 2.0f * juce::MathConstants<float>::pi;
            voices[voiceIndex].noteNumber = note;
            voices[voiceIndex].velocity = velocity;
            voices[voiceIndex].voiceAge = 0.0f;
            voices[voiceIndex].noteOnTime =
                static_cast<float>(blockStartTime + static_cast<double>(samplePosition) / sampleRate);
            voices[voiceIndex].releaseStartAmplitude = 0.0f;
            const float twoPi = 2.0f * juce::MathConstants<float>::pi;
            voices[voiceIndex].subPhaseIncrement = voices[voiceIndex].frequency *
                                                   powf(2.0f, voices[voiceIndex].subTune / 12.0f) *
                                                   voices[voiceIndex].subTrack / sampleRate * twoPi;
            voices[voiceIndex].osc2PhaseIncrement = voices[voiceIndex].frequency *
                                                    powf(2.0f, voices[voiceIndex].osc2Tune / 12.0f) *
                                                    voices[voiceIndex].osc2Track / sampleRate * twoPi;
            for (int u = 0; u < voices[voiceIndex].unison; ++u) {
                jassert(u < voices[voiceIndex].unisonPhases.size());
                float baseDetune = voices[voiceIndex].detune * (u - (voices[voiceIndex].unison - 1) / 2.0f) /
                                   (voices[voiceIndex].unison - 1 + 0.0001f);
                float randVar = 1.0f + (getRandomFloatAudioThread() - 0.5f) * 0.1f;
                voices[voiceIndex].detuneFactors[u] = powf(2.0f, baseDetune * randVar / 12.0f);
                voices[voiceIndex].unisonPhases[u] = getRandomFloatAudioThread() * 0.01f;
            }
            voices[voiceIndex].mainLPState = 0.0f;
            voices[voiceIndex].subLPState = 0.0f;
            voices[voiceIndex].osc2LPState = 0.0f;
            voices[voiceIndex].dcState = 0.0f;
            DBG("Note On: MIDI note " << note << ", voiceIndex " << voiceIndex << ", frequency "
                                      << voices[voiceIndex].frequency);
        } else if (msg.isNoteOff()) {
            int note = msg.getNoteNumber();
            DBG("MIDI Note off: " << note);
            for (int j = 0; j < MAX_VOICE_POLYPHONY; ++j) {
                if (voices[j].active && voices[j].noteNumber == note) {
                    voices[j].released = true;
                    voices[j].isHeld = false;
                    voices[j].releaseStartAmplitude = voices[j].smoothedAmplitude.getCurrentValue();
                    voices[j].noteOffTime =
                        static_cast<float>(blockStartTime + static_cast<double>(samplePosition) / sampleRate);
                    DBG("Note Off: MIDI note " << note << ", voiceIndex " << j);
                }
            }
        } else if (msg.isProgramChange()) {
            int program = msg.getProgramChangeNumber();
            if (program >= 0 && program < getNumPrograms()) {
                setCurrentProgram(program);
            } else {
                DBG("Invalid program change index: " << program);
            }
        }
    }

    // Process all samples in the oversampled block
    for (int i = 0; i < oversampledBlock.getNumSamples(); ++i) {
        processSingleSample(i, oversampledBlock, blockStartTime, sampleRate, voiceScaling, totalNumOutputChannels,
                            wavetable_lookup_scalar);

        const float ageInc = 1.0f / sampleRate;
        for (int j = 0; j < MAX_VOICE_POLYPHONY; ++j) {
            if (voices[j].active) voices[j].voiceAge += ageInc;
        }
    }

    // Downsample the output
    oversampling->processSamplesDown(block);
    currentTime = blockStartTime + static_cast<double>(buffer.getNumSamples()) / inputSampleRate;
}

// Save plugin state
void SimdSynthAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute("currentProgram", currentProgram);
    copyXmlToBinary(*xml, destData);
}

// Load plugin state
void SimdSynthAudioProcessor::setStateInformation(const void *data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr) {
        if (xmlState->hasTagName(parameters.state.getType())) {
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
            setParametersChanged(); // Ensure voices are updated
            int program = xmlState->getIntAttribute("currentProgram", 0);
            if (program >= 0 && program < getNumPrograms()) {
                setCurrentProgram(program);
            }
        }
    }
}

// Create the editor
juce::AudioProcessorEditor *SimdSynthAudioProcessor::createEditor() { return new SimdSynthAudioProcessorEditor(*this); }

// Plugin creation function
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new SimdSynthAudioProcessor(); }