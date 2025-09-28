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
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"attackCurve", parameterVersion},  // New: Variable attack curve
                                                              "Attack Curve", 0.5f, 5.0f, 2.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"releaseCurve", parameterVersion},  // New: Variable release curve
                                                              "Release Curve", 0.5f, 5.0f, 3.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"cutoff", parameterVersion},
                                                              "Filter Cutoff", 20.0f, 20000.0f, 1000.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"resonance", parameterVersion},
                                                              "Filter Resonance", 0.0f, 1.0f, 0.7f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegAttack", parameterVersion},
                                                              "Filter EG Attack", 0.01f, 5.0f, 0.1f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegDecay", parameterVersion},
                                                              "Filter EG Decay", 0.1f, 5.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegSustain", parameterVersion},
                                                              "Filter EG Sustain", 0.0f, 1.0f, 0.5f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegRelease", parameterVersion},
                                                              "Filter EG Release", 0.01f, 5.0f, 0.2f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegAmount", parameterVersion},
                                                              "Filter EG Amount", -1.0f, 1.0f, 0.5f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lfoRate", parameterVersion},
                                                              "LFO Rate", 0.0f, 20.0f, 5.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lfoDepth", parameterVersion},
                                                              "LFO Depth", 0.0f, 1.0f, 0.2f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lfoPitchAmt", parameterVersion},
                                                                 "LFO Pitch Amt", 0.0f, 0.2f, 0.05f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subTune", parameterVersion},
                                                              "Sub Osc Tune", -24.0f, 24.0f, -12.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subMix", parameterVersion},
                                                              "Sub Osc Mix", 0.0f, 1.0f, 0.5f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subTrack", parameterVersion},
                                                              "Sub Osc Track", 0.0f, 1.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"osc2Tune", parameterVersion},
                                                              "Osc 2 Tune", -12.0f, 12.0f, 0.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"osc2Mix", parameterVersion},
                                                              "Osc 2 Mix", 0.0f, 1.0f, 0.1f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"osc2Track", parameterVersion},
                                                              "Osc 2 Track", 0.0f, 1.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain", parameterVersion},
                                                              "Output Gain", 0.0f, 2.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"unison", parameterVersion},
                                                              "Unison Voices", 1.0f, 8.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"detune", parameterVersion},
                                                              "Unison Detune", 0.0f, 0.1f, 0.01f)}),
      currentTime(0.0),
      oversampling(std::make_unique<juce::dsp::Oversampling<float>>(
          2, 2, juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR, true, true)),
      random(juce::Time::getMillisecondCounterHiRes()),
      smoothedGain(1.0f),
      smoothedCutoff(1000.0f),
      smoothedResonance(0.7f),
      smoothedLfoRate(5.0f),
      smoothedLfoDepth(0.08f),
      smoothedSubMix(0.5f),
      smoothedSubTune(-12.0f),
      smoothedSubTrack(1.0f),
      smoothedDetune(0.01f),
      smoothedOsc2Mix(0.3f),
      smoothedOsc2Tune(0.0f),
      smoothedOsc2Track(1.0f),
      smoothedAttackCurve(2.0f),  // New: Smoothed attack curve
      smoothedReleaseCurve(3.0f)  // New: Smoothed release curve
{
    // Initialize smoothed values with 50ms ramp time to prevent zipper noise
    smoothedGain.reset(44100.0, 0.05);
    smoothedCutoff.reset(44100.0, 0.05);
    smoothedResonance.reset(44100.0, 0.05);
    smoothedLfoRate.reset(44100.0, 0.05);
    smoothedLfoDepth.reset(44100.0, 0.05);
    smoothedSubMix.reset(44100.0, 0.05);
    smoothedSubTune.reset(44100.0, 0.05);
    smoothedSubTrack.reset(44100.0, 0.05);
    smoothedDetune.reset(44100.0, 0.05);
    smoothedOsc2Mix.reset(44100.0, 0.05);
    smoothedOsc2Tune.reset(44100.0, 0.05);
    smoothedOsc2Track.reset(44100.0, 0.05);
    smoothedAttackCurve.reset(44100.0, 0.05);  // New
    smoothedReleaseCurve.reset(44100.0, 0.05); // New

    // Initialize parameter pointers (add new ones)
    wavetableTypeParam = parameters.getRawParameterValue("wavetable");
    attackTimeParam = parameters.getRawParameterValue("attack");
    decayTimeParam = parameters.getRawParameterValue("decay");
    sustainLevelParam = parameters.getRawParameterValue("sustain");
    releaseTimeParam = parameters.getRawParameterValue("release");
    attackCurveParam = parameters.getRawParameterValue("attackCurve");  // New
    releaseCurveParam = parameters.getRawParameterValue("releaseCurve"); // New
    cutoffParam = parameters.getRawParameterValue("cutoff");
    resonanceParam = parameters.getRawParameterValue("resonance");
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

    // Store raw default values for preset loading (add new ones)
    defaultParamValues = {
        {"wavetable", 0.0f},
        {"attack", 0.1f},
        {"decay", 0.5f},
        {"sustain", 0.8f},
        {"release", 0.2f},
        {"attackCurve", 2.0f},  // New
        {"releaseCurve", 3.0f}, // New
        {"cutoff", 1000.0f},
        {"resonance", 0.7f},
        {"fegAttack", 0.1f},
        {"fegDecay", 1.0f},
        {"fegSustain", 0.5f},
        {"fegRelease", 0.2f},
        {"fegAmount", 0.5f},
        {"lfoRate", 5.0f},
        {"lfoDepth", 0.08f},
        {"lfoPitchAmt", 0.05f},
        {"subTune", -12.0f},
        {"subMix", 0.5f},
        {"subTrack", 1.0f},
        {"osc2Tune", 0.0f},
        {"osc2Mix", 0.1f},
        {"osc2Track", 1.0f},
        {"gain", 1.0f},
        {"unison", 1.0f},
        {"detune", 0.01f}
    };

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
        for (int harmonic = 1; harmonic <= 10; ++harmonic) {
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
        voices[i].attackCurve = *attackCurveParam;  // New
        voices[i].releaseCurve = *releaseCurveParam; // New
        voices[i].cutoff = *cutoffParam;
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
            float detuneCents = voices[i].detune * (u - (voices[i].unison - 1) / 2.0f) /
                                (voices[i].unison - 1 + 0.0001f);
            voices[i].detuneFactors[u] = powf(2.0f, detuneCents / 12.0f);
        }

        voices[i].releaseStartAmplitude = 0.0f;
        voices[i].smoothedAmplitude.reset(44100.0, 0.05);
        voices[i].smoothedAmplitude.setCurrentAndTargetValue(0.0f);
        voices[i].smoothedFilterEnv.reset(44100.0, 0.05);
        voices[i].smoothedFilterEnv.setCurrentAndTargetValue(0.0f);

        voices[i].smoothedCutoff.reset(44100.0, 0.05);
        voices[i].smoothedCutoff.setCurrentAndTargetValue(*cutoffParam);
        voices[i].smoothedFegAmount.reset(44100.0, 0.05);
        voices[i].smoothedFegAmount.setCurrentAndTargetValue(*fegAmountParam);
        voices[i].mainLPState = 0.0f;
        voices[i].subLPState = 0.0f;
        voices[i].osc2LPState = 0.0f;
        voices[i].dcState = 0.0f;     // New: For DC blocker

    }

    // Set initial filter resonance
    filter.resonance = *resonanceParam;

    // Initialize presets
    presetManager.createDefaultPresets();
    loadPresetsFromDirectory();
}

// Helper Function to Get Random Float
float SimdSynthAudioProcessor::getRandomFloatAudioThread() {
    // Lock-free read from buffer
    size_t index = randomIndex.load(std::memory_order_acquire);
    size_t nextIndex = (index + 1) % randomBufferSize;

    // Update index atomically
    if (randomIndex.compare_exchange_strong(index, nextIndex, std::memory_order_release)) {
        return randomBuffer[index];
    }

    // Fallback if buffer is contended (rare)
    return randomBuffer[index];
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
int SimdSynthAudioProcessor::getPreferredBufferSize() const {
    return 512;
}

// Destructor: Clean up oversampling
SimdSynthAudioProcessor::~SimdSynthAudioProcessor() {
    oversampling.reset();
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
    for (const auto& file : presetFiles) {
        presetNames.add(file.getFileNameWithoutExtension());
    }

    if (presetNames.isEmpty()) {
        DBG("No presets found in directory: " << presetDir.getFullPathName());
        presetNames.add("Default");
        presetManager.createDefaultPresets();
    }
}

// Return the number of available presets
int SimdSynthAudioProcessor::getNumPrograms() {
    return presetNames.size();
}

// Return the current preset index
int SimdSynthAudioProcessor::getCurrentProgram() {
    return currentProgram;
}

// Load a preset by index and update all voices
void SimdSynthAudioProcessor::setCurrentProgram(int index) {

    juce::StringArray paramIds = {
        "wavetable",  "attack",    "decay",     "sustain",  "release",
        "attackCurve", "releaseCurve",  // New
        "cutoff",     "resonance", "fegAttack", "fegDecay", "fegSustain",
        "fegRelease", "fegAmount", "lfoRate",   "lfoDepth", "subTune",
        "subMix",     "subTrack",  "osc2Tune",  "osc2Mix",  "osc2Track",
        "gain",       "unison",    "detune"
    };

    if (index < 0 || index >= presetNames.size()) {
        DBG("Error: Invalid preset index: " << index << ", presetNames size: " << presetNames.size());
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

    DBG("Loading preset: " << presetNames[index] << ", File: " << presetFile.getFullPathName());
    DBG("JSON: " << jsonString);

    if (!parsedJson.isObject()) {
        DBG("Error: Invalid JSON format in preset: " << presetNames[index]);
        return;
    }

    juce::var synthParams = parsedJson.getProperty("SimdSynth", juce::var());
    if (!synthParams.isObject()) {
        DBG("Error: 'SimdSynth' object not found in preset: " << presetNames[index]);
        for (const auto& paramId : paramIds) {
            if (auto* param = parameters.getParameter(paramId)) {
                if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param)) {
                    float value = defaultParamValues[paramId];
                    floatParam->setValueNotifyingHost(floatParam->convertTo0to1(value));
                }
            }
        }
        return;
    }

    bool anyParamUpdated = false;
    for (int p = 0; p < paramIds.size(); ++p) {
        juce::String paramId = paramIds[p];  // Explicit String, indexed by int
        if (auto* param = parameters.getParameter(paramId)) {
            if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param)) {
                float value = defaultParamValues[paramId];  // Default start
                juce::var prop = synthParams.getProperty(paramId, juce::var());  // Safe getProperty with default
                if (!prop.isVoid()) {
                    if (prop.isDouble()) {
                        value = static_cast<float>(prop);
                    } else if (prop.isInt()) {
                        value = static_cast<float>(prop);
                    } else if (prop.isInt64()) {
                        value = static_cast<float>(prop);
                    } else {
                        // Non-numeric: log and stick with default
                        DBG("Warning: Non-numeric value for " << paramId << " in preset; using default " << value);
                    }
                }
                if (paramId == "wavetable" || paramId == "unison") {
                    value = std::round(value);
                }
                value = juce::jlimit(floatParam->getNormalisableRange().start, floatParam->getNormalisableRange().end, value);
                floatParam->setValueNotifyingHost(floatParam->convertTo0to1(value));
                DBG("Setting " << paramId << " to " << value);
                anyParamUpdated = true;
            }
        }
    }

    if (!anyParamUpdated) {
        DBG("Warning: No parameters updated for preset: " << presetNames[index]);
    } else {
        setParametersChanged(); // Flag for update
    }

    // Update filter resonance
    filter.resonance = *resonanceParam;

    // Update parameters for inactive voices
    updateVoiceParameters(filter.sampleRate * oversampling->getOversamplingFactor(), true);

    // Reset smoothed values to match preset
    smoothedGain.setCurrentAndTargetValue(*gainParam);
    smoothedCutoff.setCurrentAndTargetValue(*cutoffParam);
    smoothedResonance.setCurrentAndTargetValue(*resonanceParam);
    smoothedLfoRate.setCurrentAndTargetValue(*lfoRateParam);
    smoothedLfoDepth.setCurrentAndTargetValue(*lfoDepthParam);
    smoothedSubMix.setCurrentAndTargetValue(*subMixParam);
    smoothedSubTune.setCurrentAndTargetValue(*subTuneParam);
    smoothedSubTrack.setCurrentAndTargetValue(*subTrackParam);
    smoothedOsc2Mix.setCurrentAndTargetValue(*osc2MixParam);
    smoothedOsc2Tune.setCurrentAndTargetValue(*osc2TuneParam);
    smoothedOsc2Track.setCurrentAndTargetValue(*osc2TrackParam);
    smoothedDetune.setCurrentAndTargetValue(*detuneParam);
    // After loading, reset smoothed curves:
    smoothedAttackCurve.setCurrentAndTargetValue(*attackCurveParam);  // New
    smoothedReleaseCurve.setCurrentAndTargetValue(*releaseCurveParam); // New

    // Notify editor to update preset display
    if (auto* editor = getActiveEditor()) {
        if (auto* synthEditor = dynamic_cast<SimdSynthAudioProcessorEditor*>(editor)) {
            synthEditor->updatePresetComboBox();
        }
    }

}

// Return the name of a preset by index
const juce::String SimdSynthAudioProcessor::getProgramName(int index) {
    if (index >= 0 && index < presetNames.size()) {
        return presetNames[index];
    }
    return "Default";
}

// Rename a preset
void SimdSynthAudioProcessor::changeProgramName(int index, const juce::String& newName) {
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
float SimdSynthAudioProcessor::midiToFreq(int midiNote) {
    return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
}

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
    __m128 result = _mm_add_ps(xWrapped, _mm_add_ps(_mm_mul_ps(c3, x3), _mm_add_ps(_mm_mul_ps(c5, x5), _mm_mul_ps(c7, x7))));
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
    float32x4_t result = vaddq_f32(xWrapped, vaddq_f32(vmulq_f32(c3, x3), vaddq_f32(vmulq_f32(c5, x5), vmulq_f32(c7, x7))));
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
        const float* table;
        switch (static_cast<int>(tempTypes[i])) {
        case 0: table = sineTable.data(); break;
        case 1: table = sawTable.data(); break;
        case 2: table = squareTable.data(); break;
        default: table = sineTable.data(); tempOut[i] = 0.0f; continue;
        }
        tempOut[i] = table[idx] + f * (table[idx + 1] - table[idx]); // Linear interpolation
    }
    return SIMD_LOAD(tempOut);
}

void SimdSynthAudioProcessor::applyLadderFilter(Voice* voices, int voiceOffset, SIMD_TYPE input, Filter& filter, SIMD_TYPE& output) {
    if (filter.sampleRate <= 0.0f) {
        output = SIMD_SET1(0.0f);
        return;
    }

    // Reset filter states for new voices
    // Initialize filter states for new voices
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        if (idx < MAX_VOICE_POLYPHONY && voices[idx].active && voices[idx].noteOnTime == currentTime) {
            float inputValue;
            SIMD_GET_LANE(inputValue, input, i); // Extract i-th lane
            for (int j = 0; j < 4; j++) {
                voices[idx].filterStates[j] = inputValue * 0.25f; // Smooth transition
            }
        }
    }

    // Vectorized cutoff computation (log EG mod)
    alignas(32) float tempCutoffs[4], tempEnvMods[4], tempResonances[4];
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        tempCutoffs[i] = idx < MAX_VOICE_POLYPHONY && voices[idx].active ? voices[idx].smoothedCutoff.getNextValue() : 1000.0f;
        // Logarithmic EG: multiplier on cutoff
        float egMod = idx < MAX_VOICE_POLYPHONY && voices[idx].active ? voices[idx].smoothedFilterEnv.getNextValue() * voices[idx].smoothedFegAmount.getNextValue() : 0.0f;
        egMod = juce::jlimit(-1.0f, 1.0f, egMod); // Reduced range for stability
        tempEnvMods[i] = tempCutoffs[i] * (powf(2.0f, egMod * 0.5f) - 1.0f); // Softer modulation
        tempCutoffs[i] += tempEnvMods[i];  // Add to base
        tempResonances[i] = idx < MAX_VOICE_POLYPHONY && voices[idx].active ? voices[idx].resonance : 0.7f;
        // Resonance gain comp
        float resComp = 1.0f / std::sqrt(1.0f + tempResonances[i] * tempResonances[i]);
        tempResonances[i] *= resComp;  // New
        tempResonances[i] = juce::jlimit(0.0f, 0.9f, tempResonances[i]); // Lower max resonance
    }

    SIMD_TYPE modulatedCutoffs = SIMD_LOAD(tempCutoffs);
    modulatedCutoffs = SIMD_MAX(SIMD_SET1(20.0f), SIMD_MIN(modulatedCutoffs, SIMD_SET1(filter.sampleRate * 0.48f)));
    float tempModulated[4];
    SIMD_STORE(tempModulated, modulatedCutoffs);
    for (int i = 0; i < 4; i++) {
        tempCutoffs[i] = 2.0f * sinf(juce::MathConstants<float>::pi * tempModulated[i] / filter.sampleRate);
        tempCutoffs[i] = std::tanh(tempCutoffs[i] * 0.8f); // Softer tanh
        if (std::isnan(tempCutoffs[i]) || !std::isfinite(tempCutoffs[i])) tempCutoffs[i] = 0.0f;
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
    SIMD_TYPE states[4];
    for (int i = 0; i < 4; i++) {
        float temp[4] = {
            voiceOffset < MAX_VOICE_POLYPHONY ? voices[voiceOffset].filterStates[i] : 0.0f,
            voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].filterStates[i] : 0.0f,
            voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].filterStates[i] : 0.0f,
            voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].filterStates[i] : 0.0f
        };
        states[i] = SIMD_LOAD(temp);
    }
    SIMD_TYPE feedback = SIMD_MUL(states[3], resonance);
    SIMD_TYPE filterInput = SIMD_SUB(input, feedback);
    states[0] = SIMD_ADD(states[0], SIMD_MUL(alpha, SIMD_SUB(filterInput, states[0])));
    states[1] = SIMD_ADD(states[1], SIMD_MUL(alpha, SIMD_SUB(states[0], states[1])));
    states[2] = SIMD_ADD(states[2], SIMD_MUL(alpha, SIMD_SUB(states[1], states[2])));
    states[3] = SIMD_ADD(states[3], SIMD_MUL(alpha, SIMD_SUB(states[2], states[3])));
    output = states[3];

    // Softer nonlinearity: Cubic soft-clip on output
    float tempOut[4];
    SIMD_STORE(tempOut, output);
    for (int i = 0; i < 4; i++) {
        float over = std::abs(tempOut[i]) > 2.0f ? (tempOut[i] > 0 ? 2.0f : -2.0f) : tempOut[i];
        tempOut[i] = over - (over * over * over) / 3.0f;  // Cubic approx
        if (!std::isfinite(tempOut[i])) {
            tempOut[i] = 0.0f;
        }
        // DC offset correction (stronger)
        tempOut[i] -= 0.001f * tempOut[i];  // Increased from 0.0001f
    }
    output = SIMD_LOAD(tempOut);

    if (std::isnan(tempOut[0]) || std::isnan(tempOut[1]) || std::isnan(tempOut[2]) || std::isnan(tempOut[3])) {
        DBG("Filter output nan at voiceOffset " << voiceOffset << ": {" << tempOut[0] << ", " << tempOut[1] << ", "
                                                << tempOut[2] << ", " << tempOut[3] << "}");
    }
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
        if (voices[i].released) {
            priority = 1000.0f + (voices[i].amplitude > 0.001f ? voices[i].releaseStartAmplitude : 0.0f);
        } else if (!voices[i].isHeld) {
            priority = 500.0f + voices[i].voiceAge;
        } else {
            priority = voices[i].voiceAge;
        }

        if (priority > highestPriority) {
            highestPriority = priority;
            voiceToSteal = i;
        }
    }

    if (voices[voiceToSteal].active && !voices[voiceToSteal].released) {
        voices[voiceToSteal].smoothedAmplitude.setTargetValue(0.0f);
        voices[voiceToSteal].smoothedAmplitude.reset(filter.sampleRate * oversampling->getOversamplingFactor(), 0.005);
        voices[voiceToSteal].smoothedFilterEnv.setTargetValue(0.0f);
        voices[voiceToSteal].smoothedFilterEnv.reset(filter.sampleRate * oversampling->getOversamplingFactor(), 0.01);
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
        float attack = juce::jmax(voices[i].attack, 0.01f);
        float decay = std::max(voices[i].decay, 0.01f);
        float sustain = juce::jlimit(0.0f, 1.0f, voices[i].sustain);
        float release = std::max(voices[i].release, 0.01f);

        // Velocity scaling for attack
        float velScale = 1.0f / (0.3f + 0.7f * voices[i].velocity);
        attack *= velScale;

        float attackCurve = juce::jlimit(0.5f, 5.0f, voices[i].attackCurve);
        const float decayCurve = 1.5f;
        const float releaseCurve = voices[i].releaseCurve;

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
            // Smoother release with linear fade
            amplitude = voices[i].releaseStartAmplitude * (1.0f - std::pow(releasePhase, releaseCurve));
            if (amplitude <= 0.001f) {
                amplitude = 0.0f;
                voices[i].active = false;
                voices[i].smoothedAmplitude.setCurrentAndTargetValue(0.0f);
                voices[i].smoothedFilterEnv.setCurrentAndTargetValue(0.0f);
                for (int j = 0; j < 4; j++) {
                    voices[i].filterStates[j] *= 0.999f;
                }
            }
        }

        voices[i].amplitude = juce::jlimit(0.0f, 1.0f, amplitude);
        // Adjust smoothing ramp based on envelope stage
        float rampTime = voices[i].released ? 0.01f : 0.005f; // Shorter for attack, longer for release
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

void SimdSynthAudioProcessor::updateVoiceParameters(float sampleRate, bool forceUpdate = false) {
    sampleRate = std::max(sampleRate, 44100.0f);
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        if (!voices[i].active && !forceUpdate) continue; // Skip inactive voices unless forced
        // Update only changed parameters
        if (voices[i].attack != *attackTimeParam) voices[i].attack = *attackTimeParam;
        if (voices[i].decay != *decayTimeParam) voices[i].decay = *decayTimeParam;
        if (voices[i].sustain != *sustainLevelParam) voices[i].sustain = *sustainLevelParam;
        if (voices[i].release != *releaseTimeParam) voices[i].release = *releaseTimeParam;
        if (voices[i].attackCurve != smoothedAttackCurve.getCurrentValue()) voices[i].attackCurve = smoothedAttackCurve.getCurrentValue();
        if (voices[i].releaseCurve != smoothedReleaseCurve.getCurrentValue()) voices[i].releaseCurve = smoothedReleaseCurve.getCurrentValue();
        if (voices[i].cutoff != *cutoffParam) voices[i].cutoff = *cutoffParam;
        if (voices[i].resonance != *resonanceParam) voices[i].resonance = *resonanceParam;
        if (voices[i].fegAttack != *fegAttackParam) voices[i].fegAttack = *fegAttackParam;
        if (voices[i].fegDecay != *fegDecayParam) voices[i].fegDecay = *fegDecayParam;
        if (voices[i].fegSustain != *fegSustainParam) voices[i].fegSustain = *fegSustainParam;
        if (voices[i].fegRelease != *fegReleaseParam) voices[i].fegRelease = *fegReleaseParam;
        if (voices[i].fegAmount != *fegAmountParam) voices[i].fegAmount = *fegAmountParam;
        if (voices[i].lfoRate != smoothedLfoRate.getCurrentValue()) voices[i].lfoRate = smoothedLfoRate.getCurrentValue();
        if (voices[i].lfoDepth != smoothedLfoDepth.getCurrentValue()) voices[i].lfoDepth = smoothedLfoDepth.getCurrentValue();
        if (voices[i].lfoPitchAmt != *lfoPitchAmtParam) voices[i].lfoPitchAmt = *lfoPitchAmtParam;
        if (voices[i].subTune != smoothedSubTune.getCurrentValue()) voices[i].subTune = smoothedSubTune.getCurrentValue();
        if (voices[i].subMix != smoothedSubMix.getCurrentValue()) voices[i].subMix = smoothedSubMix.getCurrentValue();
        if (voices[i].subTrack != smoothedSubTrack.getCurrentValue()) voices[i].subTrack = smoothedSubTrack.getCurrentValue();
        if (voices[i].osc2Tune != smoothedOsc2Tune.getCurrentValue()) voices[i].osc2Tune = smoothedOsc2Tune.getCurrentValue();
        if (voices[i].osc2Mix != smoothedOsc2Mix.getCurrentValue()) voices[i].osc2Mix = smoothedOsc2Mix.getCurrentValue();
        if (voices[i].osc2Track != smoothedOsc2Track.getCurrentValue()) voices[i].osc2Track = smoothedOsc2Track.getCurrentValue();
        if (voices[i].detune != smoothedDetune.getCurrentValue()) voices[i].detune = smoothedDetune.getCurrentValue();
        if (voices[i].wavetableType != static_cast<int>(*wavetableTypeParam)) voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
        voices[i].smoothedCutoff.setTargetValue(*cutoffParam);
        voices[i].smoothedFegAmount.setTargetValue(*fegAmountParam);
        if (voices[i].detune != *detuneParam || voices[i].unison != static_cast<int>(*unisonParam)) {
            voices[i].unison = juce::jlimit(1, maxUnison, static_cast<int>(*unisonParam));
            voices[i].detune = *detuneParam;
            for (int u = 0; u < voices[i].unison; ++u) {
                float detuneCents = voices[i].detune * (u - (voices[i].unison - 1) / 2.0f) /
                                    (voices[i].unison - 1 + 0.0001f);
                voices[i].detuneFactors[u] = powf(2.0f, detuneCents / 12.0f);
                if (voices[i].unisonPhases[u] == 0.0f) {
                    voices[i].unisonPhases[u] = getRandomFloatAudioThread() * 0.01f;
                }            }
        }
        if (voices[i].active) {
            voices[i].phaseIncrement = voices[i].frequency / sampleRate;
            const float twoPi = 2.0f * juce::MathConstants<float>::pi;
            voices[i].subPhaseIncrement = voices[i].frequency * powf(2.0f, voices[i].subTune / 12.0f) *
                                          voices[i].subTrack / sampleRate * twoPi;
            voices[i].osc2PhaseIncrement = voices[i].frequency * powf(2.0f, voices[i].osc2Tune / 12.0f) *
                                           voices[i].osc2Track / sampleRate * twoPi;
        }
    }
}

void SimdSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    filter.sampleRate = static_cast<float>(sampleRate);
    currentTime = 0.0;
    int oversamplingFactor = (samplesPerBlock < 256) ? 1 : 2;
    if (!oversampling || oversampling->getOversamplingFactor() != oversamplingFactor) {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
            2, oversamplingFactor, juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR, true, true);
        oversampling->initProcessing(samplesPerBlock);
    }
    // Initialize smoothed parameters
    smoothedGain.reset(sampleRate, 0.05);
    smoothedCutoff.reset(sampleRate, 0.05);
    smoothedResonance.reset(sampleRate, 0.05);
    smoothedLfoRate.reset(sampleRate, 0.05);
    smoothedLfoDepth.reset(sampleRate, 0.05);
    smoothedSubMix.reset(sampleRate, 0.05);
    smoothedSubTune.reset(sampleRate, 0.05);
    smoothedSubTrack.reset(sampleRate, 0.05);
    smoothedDetune.reset(sampleRate, 0.05);
    smoothedOsc2Mix.reset(sampleRate, 0.05);
    smoothedOsc2Tune.reset(sampleRate, 0.05);
    smoothedOsc2Track.reset(sampleRate, 0.05);
    smoothedAttackCurve.reset(sampleRate, 0.05);
    smoothedReleaseCurve.reset(sampleRate, 0.05);

    // Reset all voices
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
        voices[i].lfoPitchAmt = *lfoPitchAmtParam; // Use parameter value instead of hard-coded 0.05f
        voices[i].mainLPState = 0.0f;
        voices[i].subLPState = 0.0f;
        voices[i].osc2LPState = 0.0f;
        voices[i].dcState = 0.0f;
        voices[i].smoothedAmplitude.reset(sampleRate * oversamplingFactor, 0.05);
        voices[i].smoothedAmplitude.setCurrentAndTargetValue(0.0f);
        voices[i].smoothedFilterEnv.reset(sampleRate * oversamplingFactor, 0.05);
        voices[i].smoothedFilterEnv.setCurrentAndTargetValue(0.0f);
        for (int j = 0; j < 4; ++j) {
            voices[i].filterStates[j] = 0.0f;
        }
        voices[i].smoothedCutoff.reset(sampleRate * oversamplingFactor, 0.05);
        voices[i].smoothedCutoff.setCurrentAndTargetValue(*cutoffParam);
        voices[i].smoothedFegAmount.reset(sampleRate * oversamplingFactor, 0.05);
        voices[i].smoothedFegAmount.setCurrentAndTargetValue(*fegAmountParam);
    }

    // Update all voice parameters to match current state
    updateVoiceParameters(static_cast<float>(sampleRate) * oversamplingFactor, true);
}


// Release resources
void SimdSynthAudioProcessor::releaseResources() {
    oversampling->reset();
}

// Process audio and MIDI with oversampling
void SimdSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    buffer.clear();

    // Prepare oversampled buffer
    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(block);

    float sampleRate = filter.sampleRate * oversampling->getOversamplingFactor();
    double blockStartTime = currentTime;

    // Update filter resonance
    filter.resonance = smoothedResonance.getNextValue();

    // Update smoothed parameter targets (add new curves)
    smoothedGain.setTargetValue(*gainParam);
    smoothedCutoff.setTargetValue(*cutoffParam);
    smoothedResonance.setTargetValue(*resonanceParam);
    smoothedLfoRate.setTargetValue(*lfoRateParam);
    smoothedLfoDepth.setTargetValue(*lfoDepthParam);
    smoothedSubMix.setTargetValue(*subMixParam);
    smoothedSubTune.setTargetValue(*subTuneParam);
    smoothedSubTrack.setTargetValue(*subTrackParam);
    smoothedOsc2Mix.setTargetValue(*osc2MixParam);
    smoothedOsc2Tune.setTargetValue(*osc2TuneParam);
    smoothedOsc2Track.setTargetValue(*osc2TrackParam);
    smoothedDetune.setTargetValue(*detuneParam);
    smoothedAttackCurve.setTargetValue(*attackCurveParam);  // New
    smoothedReleaseCurve.setTargetValue(*releaseCurveParam); // New

    // Update voice parameters only if needed
    if (parametersChanged.exchange(false, std::memory_order_acquire)) {
        updateVoiceParameters(sampleRate);
    }

    int activeCount = 0;
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        if (voices[i].active) {
            ++activeCount;
        }
    }
    float voiceScaling = (activeCount > 0) ? (1.0f / std::sqrt(static_cast<float>(activeCount))) : 1.0f;

    // Scalar wavetable lookup for per-voice processing (unchanged)
    auto wavetable_lookup_scalar = [&](float ph, float wt) -> float {
        ph = std::fmod(ph, 1.0f);
        if (ph < 0.0f) ph += 1.0f;
        float index = ph * static_cast<float>(WAVETABLE_SIZE - 1);
        int idx = static_cast<int>(std::floor(index));
        float frac = index - static_cast<float>(idx);
        idx = juce::jlimit(0, WAVETABLE_SIZE - 2, idx);
        const float* table;
        switch (static_cast<int>(wt)) {
            case 0: table = sineTable.data(); break;
            case 1: table = sawTable.data(); break;
            case 2: table = squareTable.data(); break;
            default: table = sineTable.data(); break;
        }
        return table[idx] + frac * (table[idx + 1] - table[idx]);
    };

    // Process MIDI and audio
    int sampleIndex = 0;
    juce::MidiBuffer oversampledMidi;
    for (const auto metadata : midiMessages) {
        oversampledMidi.addEvent(metadata.getMessage(), metadata.samplePosition * oversampling->getOversamplingFactor());
    }

    // Main processing loop (applies to both MIDI loop and remaining samples)
    for (const auto metadata : oversampledMidi) {  // This is the MIDI loop; duplicate for remaining below
        auto msg = metadata.getMessage();
        int samplePosition = metadata.samplePosition;

        while (sampleIndex < samplePosition && sampleIndex < oversampledBlock.getNumSamples()) {
            processSingleSample(sampleIndex, oversampledBlock, blockStartTime, sampleRate, voiceScaling, totalNumOutputChannels, wavetable_lookup_scalar);  // Refactored for brevity
            sampleIndex++;
        }

        // Note-on/off/program change handling (modified for random detune)
if (msg.isNoteOn()) {
    int note = msg.getNoteNumber();
    float velocity = 0.7f + (msg.getVelocity() / 127.0f) * 0.3f;
    int voiceIndex = -1;
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        if (!voices[i].active) {
            voiceIndex = i;
            break;
        }
    }
    if (voiceIndex == -1) {
        voiceIndex = findVoiceToSteal();
    }
    bool wasActive = voices[voiceIndex].active;
    voices[voiceIndex].active = true;
    voices[voiceIndex].released = false;
    voices[voiceIndex].isHeld = true;
    voices[voiceIndex].smoothedAmplitude.setCurrentAndTargetValue(0.0f);
    voices[voiceIndex].smoothedAmplitude.reset(sampleRate, 0.01);
    voices[voiceIndex].smoothedFilterEnv.setCurrentAndTargetValue(0.0f);
    voices[voiceIndex].smoothedFilterEnv.reset(sampleRate, 0.01);
    voices[voiceIndex].frequency = midiToFreq(note);
    voices[voiceIndex].phaseIncrement = voices[voiceIndex].frequency / sampleRate;
    if (!wasActive) {
        // Set small random offset to avoid zero-crossing, but preserve continuity where possible
        float initialOffset = getRandomFloatAudioThread() * 0.01f;
        voices[voiceIndex].phase = initialOffset; // Random small offset
        voices[voiceIndex].subPhase = initialOffset * 2.0f * juce::MathConstants<float>::pi;
        voices[voiceIndex].osc2Phase = initialOffset * 2.0f * juce::MathConstants<float>::pi;
        // Filter states initialized later in applyLadderFilter
    }
    voices[voiceIndex].lfoPhase = getRandomFloatAudioThread() * 2.0f * juce::MathConstants<float>::pi;
    voices[voiceIndex].noteNumber = note;
    voices[voiceIndex].velocity = velocity;
    voices[voiceIndex].voiceAge = 0.0f;
    voices[voiceIndex].noteOnTime = static_cast<float>(blockStartTime + static_cast<double>(samplePosition) / sampleRate);
    voices[voiceIndex].releaseStartAmplitude = 0.0f;
    const float twoPi = 2.0f * juce::MathConstants<float>::pi;
    voices[voiceIndex].subPhaseIncrement = voices[voiceIndex].frequency * powf(2.0f, voices[voiceIndex].subTune / 12.0f) *
                                           voices[voiceIndex].subTrack / sampleRate * twoPi;
    voices[voiceIndex].osc2PhaseIncrement = voices[voiceIndex].frequency * powf(2.0f, voices[voiceIndex].osc2Tune / 12.0f) *
                                            voices[voiceIndex].osc2Track / sampleRate * twoPi;
    for (int u = 0; u < voices[voiceIndex].unison; ++u) {
        jassert(u < voices[voiceIndex].unisonPhases.size());
        float baseDetune = voices[voiceIndex].detune * (u - (voices[voiceIndex].unison - 1) / 2.0f) /
                           (voices[voiceIndex].unison - 1 + 0.0001f);
        float randVar = 1.0f + (getRandomFloatAudioThread() - 0.5f) * 0.1f;
        float detuneCents = baseDetune * randVar;
        voices[voiceIndex].detuneFactors[u] = powf(2.0f, detuneCents / 12.0f);
    }
    voices[voiceIndex].mainLPState = 0.0f;
    voices[voiceIndex].subLPState = 0.0f;
    voices[voiceIndex].osc2LPState = 0.0f;
    voices[voiceIndex].dcState = 0.0f;
} else if (msg.isNoteOff()) {
            int note = msg.getNoteNumber();
            for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
                if (voices[i].active && voices[i].noteNumber == note) {
                    voices[i].released = true;
                    voices[i].isHeld = false;
                    voices[i].releaseStartAmplitude = voices[i].smoothedAmplitude.getCurrentValue();
                    voices[i].noteOffTime = static_cast<float>(blockStartTime + static_cast<double>(samplePosition) / sampleRate);
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

        // Age voices per oversampled sample
        const float ageInc = 1.0f / sampleRate;
        for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
            if (voices[i].active) voices[i].voiceAge += ageInc;
        }
    }

    // Process remaining samples (duplicate the while loop here, calling processSingleSample)
    while (sampleIndex < oversampledBlock.getNumSamples()) {
        processSingleSample(sampleIndex, oversampledBlock, blockStartTime, sampleRate, voiceScaling, totalNumOutputChannels, wavetable_lookup_scalar);
        sampleIndex++;
    }

    // Downsample back to original rate
    oversampling->processSamplesDown(block);

    // Update current time
    currentTime = blockStartTime + static_cast<double>(buffer.getNumSamples()) / filter.sampleRate;
}

// Process audio and MIDI with oversampling
void SimdSynthAudioProcessor::processSingleSample(int sampleIndex, juce::dsp::AudioBlock<float>& oversampledBlock, double blockStartTime, float sampleRate, float voiceScaling, int totalNumOutputChannels, std::function<float(float, float)> wavetable_lookup_scalar) {
    float t = static_cast<float>(blockStartTime + static_cast<double>(sampleIndex) / sampleRate);
    updateEnvelopes(t);
    float outputSampleL = 0.0f, outputSampleR = 0.0f;  // Stereo
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

        // Scalar per-voice processing
        alignas(32) float batchCombined[SIMD_WIDTH] = {0.0f};
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

            // Update LFO (scalar)
            lfoPhase += lfoRate * twoPiScalar / sampleRate;
            lfoPhase = std::fmod(lfoPhase, twoPiScalar);
            float lfoVal = std::sin(lfoPhase) * lfoDepth;
            float phaseMod_cycles = lfoVal / twoPiScalar;

            // LFO pitch mod (new: vibrato)
            float lfoPitchMod = lfoVal * voices[idx].lfoPitchAmt;
            float effectiveIncr = increment * (1.0f + lfoPitchMod);

            // Unison processing (with random detune and per-unison pan)
            float unisonOutputL = 0.0f, unisonOutputR = 0.0f;  // Stereo per voice
            int unisonVoices = voices[idx].unison;
            for (int u = 0; u < unisonVoices; ++u) {
                float detuneFactor = voices[idx].detuneFactors[u];
                float detunedPhase = (phase + phaseMod_cycles + voices[idx].unisonPhases[u]) * detuneFactor;
                float phasesNorm = detunedPhase - std::floor(detunedPhase);
                float mainVal = wavetable_lookup_scalar(phasesNorm, static_cast<float>(wavetableType));

                // Dynamic band-limiting (1-pole LP) with separate state
                float fc = voices[idx].frequency * detuneFactor * 0.45f;  // Per-detune fc
                float alphaLP = std::exp(-2.0f * juce::MathConstants<float>::pi * fc / sampleRate);
                float filteredMain = alphaLP * voices[idx].mainLPState + (1.0f - alphaLP) * mainVal;
                voices[idx].mainLPState = filteredMain;

                // Unison stereo spread
                float uPan = (static_cast<float>(u % 2) * 2.0f - 1.0f) * (voices[idx].detune / 0.1f);  // ± based on detune
                float leftGain = (1.0f - uPan) * 0.5f + 0.5f;
                float rightGain = (1.0f + uPan) * 0.5f + 0.5f;
                unisonOutputL += filteredMain * leftGain / static_cast<float>(unisonVoices);
                unisonOutputR += filteredMain * rightGain / static_cast<float>(unisonVoices);
            }

            // Normalize mix
            float totalMix = 1.0f + subMix + osc2Mix;
            totalMix = std::max(totalMix, 1e-6f);
            float mainMix = 1.0f / totalMix;
            float subMixNorm = subMix / totalMix;
            float osc2MixNorm = osc2Mix / totalMix;
            unisonOutputL *= amp * mainMix;
            unisonOutputR *= amp * mainMix;

            // Sub-oscillator with LFO mod (apply LP too, separate state)
            float subPhasesMod = subPhase + phaseMod_cycles * twoPiScalar;
            float subSinVal = std::sin(subPhasesMod);
            float fcSub = voices[idx].frequency * powf(2.0f, voices[idx].subTune / 12.0f) * 0.25f;
            float alphaSub = std::exp(-2.0f * juce::MathConstants<float>::pi * fcSub / sampleRate);
            float filteredSub = alphaSub * voices[idx].subLPState + (1.0f - alphaSub) * subSinVal;
            voices[idx].subLPState = filteredSub;
            filteredSub *= amp * subMixNorm;

            // 2nd oscillator with LFO mod (apply LP, separate state)
            float osc2PhasesMod = osc2Phase + phaseMod_cycles * twoPiScalar;
            float osc2PhasesCycles = std::fmod(osc2PhasesMod / twoPiScalar, 1.0f);
            if (osc2PhasesCycles < 0.0f) osc2PhasesCycles += 1.0f;
            float osc2Val = wavetable_lookup_scalar(osc2PhasesCycles, static_cast<float>(wavetableType));
            float fcOsc2 = voices[idx].frequency * powf(2.0f, voices[idx].osc2Tune / 12.0f) * 0.25f;
            float alphaOsc2 = std::exp(-2.0f * juce::MathConstants<float>::pi * fcOsc2 / sampleRate);
            float filteredOsc2 = alphaOsc2 * voices[idx].osc2LPState + (1.0f - alphaOsc2) * osc2Val;
            voices[idx].osc2LPState = filteredOsc2;
            filteredOsc2 *= amp * osc2MixNorm;

            // Combine mono for filter, but accumulate stereo pre-filter (or post if preferred; here pre for osc separation)
            float combinedMono = (unisonOutputL + unisonOutputR) * 0.5f + filteredSub + filteredOsc2;  // Avg for mono
            batchCombined[j] = combinedMono;

            // Accumulate full stereo (pre-filter for now; adjust if post preferred)
            outputSampleL += unisonOutputL + filteredSub + filteredOsc2;
            outputSampleR += unisonOutputR + filteredSub + filteredOsc2;

            // Update phases with effectiveIncr for pitch mod
            voices[idx].phase = std::fmod(phase + effectiveIncr, 1.0f);
            if (voices[idx].phase < 0.0f) voices[idx].phase += 1.0f;
            voices[idx].subPhase = std::fmod(subPhase + subIncrement, twoPiScalar);
            if (voices[idx].subPhase < 0.0f) voices[idx].subPhase += twoPiScalar;
            voices[idx].osc2Phase = std::fmod(osc2Phase + osc2Increment, twoPiScalar);
            if (voices[idx].osc2Phase < 0.0f) voices[idx].osc2Phase += twoPiScalar;
            voices[idx].lfoPhase = lfoPhase;
        }

        // Apply batched filter on combined inputs (mono)
        if (anyActive) {
            SIMD_TYPE combinedValues = SIMD_LOAD(batchCombined);
            SIMD_TYPE filteredOutput;
            applyLadderFilter(voices, voiceOffset, combinedValues, filter, filteredOutput);
            float temp[4];
            SIMD_STORE(temp, filteredOutput);
            for (int k = 0; k < SIMD_WIDTH && voiceOffset + k < MAX_VOICE_POLYPHONY; ++k) {
                if (voices[voiceOffset + k].active) {
                    float filtered = temp[k];
                    // DC blocker per voice (simple 20Hz HP)
                    float dcCutoff = juce::jmin(20.0f, sampleRate / 500.0f);
                    float alphaDC = std::exp(-2.0f * juce::MathConstants<float>::pi * dcCutoff / sampleRate);                    float dcOut = filtered - alphaDC * voices[voiceOffset + k].dcState;
                    voices[voiceOffset + k].dcState = dcOut;
                    filtered = dcOut;
                    // Post-filter saturation
                    filtered = std::tanh(filtered * 1.2f);  // 20% drive
                    // Post-filter pan (voice-level for width)
                    float pan = (static_cast<int>(voiceOffset + k) % 2 * 2.0f - 1.0f) * 0.5f * (voices[voiceOffset + k].unison / 8.0f);
                    float leftGain = (1.0f - pan) * 0.5f + 0.5f;
                    float rightGain = (1.0f + pan) * 0.5f + 0.5f;
                    outputSampleL += filtered * leftGain;
                    outputSampleR += filtered * rightGain;
                }
            }
        }
    }

    // Scale and gain (stereo)
    outputSampleL *= voiceScaling * smoothedGain.getNextValue();
    outputSampleR *= voiceScaling * smoothedGain.getNextValue();

    if (std::isnan(outputSampleL) || !std::isfinite(outputSampleL)) {
        outputSampleL = 0.0f;
    }
    if (std::isnan(outputSampleR) || !std::isfinite(outputSampleR)) {
        outputSampleR = 0.0f;
    }

    // Set stereo channels
    if (totalNumOutputChannels > 0) {
        oversampledBlock.setSample(0, sampleIndex, outputSampleL);  // Left
    }
    if (totalNumOutputChannels > 1) {
        oversampledBlock.setSample(1, sampleIndex, outputSampleR);  // Right
    }
}



// Save plugin state
void SimdSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute("currentProgram", currentProgram);
    copyXmlToBinary(*xml, destData);
}

// Load plugin state
void SimdSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
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
juce::AudioProcessorEditor* SimdSynthAudioProcessor::createEditor() {
    return new SimdSynthAudioProcessorEditor(*this);
}

// Plugin creation function
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SimdSynthAudioProcessor();
}