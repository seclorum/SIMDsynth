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
                                                              "LFO Depth", 0.0f, 0.5f, 0.08f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subTune", parameterVersion},
                                                              "Sub Osc Tune", -24.0f, 24.0f, -12.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subMix", parameterVersion},
                                                              "Sub Osc Mix", 0.0f, 1.0f, 0.5f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subTrack", parameterVersion},
                                                              "Sub Osc Track", 0.0f, 1.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"osc2Tune", parameterVersion},
                                                                 "Osc 2 Tune", -12.0f, 12.0f, 0.0f), // Changed from -24.0f to 24.0f
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
          2, 1, juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR, true, true)),
      random(juce::Time::getMillisecondCounterHiRes()), // Initialize random generator
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
      smoothedOsc2Track(1.0f)
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
    smoothedOsc2Mix.reset(44100.0, 0.05);    // Initialize new smoothed value
    smoothedOsc2Tune.reset(44100.0, 0.05);   // Initialize new smoothed value
    smoothedOsc2Track.reset(44100.0, 0.05);  // Initialize new smoothed value

    // Initialize parameter pointers
    wavetableTypeParam = parameters.getRawParameterValue("wavetable");
    attackTimeParam = parameters.getRawParameterValue("attack");
    decayTimeParam = parameters.getRawParameterValue("decay");
    sustainLevelParam = parameters.getRawParameterValue("sustain");
    releaseTimeParam = parameters.getRawParameterValue("release");
    cutoffParam = parameters.getRawParameterValue("cutoff");
    resonanceParam = parameters.getRawParameterValue("resonance");
    fegAttackParam = parameters.getRawParameterValue("fegAttack");
    fegDecayParam = parameters.getRawParameterValue("fegDecay");
    fegSustainParam = parameters.getRawParameterValue("fegSustain");
    fegReleaseParam = parameters.getRawParameterValue("fegRelease");
    fegAmountParam = parameters.getRawParameterValue("fegAmount");
    lfoRateParam = parameters.getRawParameterValue("lfoRate");
    lfoDepthParam = parameters.getRawParameterValue("lfoDepth");
    subTuneParam = parameters.getRawParameterValue("subTune");
    subMixParam = parameters.getRawParameterValue("subMix");
    subTrackParam = parameters.getRawParameterValue("subTrack");
    osc2TuneParam = parameters.getRawParameterValue("osc2Tune");   // New parameter pointer
    osc2MixParam = parameters.getRawParameterValue("osc2Mix");     // New parameter pointer
    osc2TrackParam = parameters.getRawParameterValue("osc2Track"); // New parameter pointer
    gainParam = parameters.getRawParameterValue("gain");
    unisonParam = parameters.getRawParameterValue("unison");
    detuneParam = parameters.getRawParameterValue("detune");

    // Initialize wavetables
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
    // Normalize saw and square tables
    float maxSaw = *std::max_element(sawTable.begin(), sawTable.end());
    float maxSquare = *std::max_element(squareTable.begin(), squareTable.end());
    for (int i = 0; i < WAVETABLE_SIZE; ++i) {
        sawTable[i] /= maxSaw; // Normalize to [-1, 1]
        squareTable[i] /= maxSquare;
    }

    // Initialize voices with default parameter values
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
        voices[i].cutoff = *cutoffParam;
        voices[i].resonance = *resonanceParam; // Initialize per-voice resonance
        voices[i].fegAttack = *fegAttackParam;
        voices[i].fegDecay = *fegDecayParam;
        voices[i].fegSustain = *fegSustainParam;
        voices[i].fegRelease = *fegReleaseParam;
        voices[i].fegAmount = *fegAmountParam;
        voices[i].lfoRate = *lfoRateParam;
        voices[i].lfoDepth = *lfoDepthParam;
        voices[i].subTune = *subTuneParam;
        voices[i].subMix = *subMixParam;
        voices[i].subTrack = *subTrackParam;
        voices[i].osc2Tune = *osc2TuneParam;
        voices[i].osc2Mix = *osc2MixParam;
        voices[i].osc2Track = *osc2TrackParam;
        voices[i].osc2PhaseOffset = 0.0f;
        voices[i].unison = static_cast<int>(*unisonParam);
        voices[i].detune = *detuneParam;
        voices[i].releaseStartAmplitude = 0.0f;
        voices[i].attackCurve = 2.0f;  // Default attack curve
        voices[i].releaseCurve = 3.0f; // Default release curve
        voices[i].smoothedAmplitude.reset(44100.0, 0.05); // Initialize amplitude smoothing
        voices[i].smoothedAmplitude.setCurrentAndTargetValue(0.0f); // Set initial value
        voices[i].smoothedFilterEnv.reset(44100.0, 0.05);
        voices[i].smoothedFilterEnv.setCurrentAndTargetValue(0.0f); // Set initial value
        voices[i].detuneFactors.resize(static_cast<int>(*unisonParam));
        for (int u = 0; u < voices[i].unison; ++u) {
            float detune = voices[i].detune * (u - (voices[i].unison - 1) / 2.0f) /
                           (voices[i].unison - 1 + 0.0001f);
            voices[i].detuneFactors[u] = powf(2.0f, detune / 12.0f);
        }
        voices[i].smoothedCutoff.reset(44100.0, 0.05);
        voices[i].smoothedCutoff.setCurrentAndTargetValue(*cutoffParam);
        voices[i].smoothedFegAmount.reset(44100.0, 0.05);
        voices[i].smoothedFegAmount.setCurrentAndTargetValue(*fegAmountParam);
    }

    // Set initial filter resonance
    filter.resonance = *resonanceParam;

    // Initialize presets
    presetManager.createDefaultPresets();
    loadPresetsFromDirectory();
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
        return;
    }

    std::map<juce::String, float> defaultValues = {
        {"wavetable", 0.0f},  {"attack", 0.1f},    {"decay", 0.5f},     {"sustain", 0.8f},   {"release", 0.2f},
        {"cutoff", 1000.0f},  {"resonance", 0.7f}, {"fegAttack", 0.1f}, {"fegDecay", 1.0f},  {"fegSustain", 0.5f},
        {"fegRelease", 0.2f}, {"fegAmount", 0.5f}, {"lfoRate", 1.0f},   {"lfoDepth", 0.05f}, {"subTune", -12.0f},
        {"subMix", 0.5f},     {"subTrack", 1.0f},  {"osc2Tune", -24.0f}, {"osc2Mix", 0.3f},   {"osc2Track", 1.0f},
        {"gain", 1.0f},       {"unison", 1.0f},    {"detune", 0.01f}
    };

    juce::StringArray paramIds = {
        "wavetable",  "attack",    "decay",     "sustain",  "release",
        "cutoff",     "resonance", "fegAttack", "fegDecay", "fegSustain",
        "fegRelease", "fegAmount", "lfoRate",   "lfoDepth", "subTune",
        "subMix",     "subTrack",  "osc2Tune",  "osc2Mix",  "osc2Track",
        "gain",       "unison",    "detune"
    };

    bool anyParamUpdated = false;
    for (const auto& paramId : paramIds) {
        if (auto* param = parameters.getParameter(paramId)) {
            if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param)) {
                float value = synthParams.hasProperty(paramId)
                                  ? static_cast<float>(synthParams.getProperty(paramId, defaultValues[paramId]))
                                  : defaultValues[paramId];
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
    }

    // Update filter resonance
    filter.resonance = *resonanceParam;

    // Update parameters for inactive voices
    updateVoiceParameters(filter.sampleRate * oversampling->getOversamplingFactor());

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
    float r = static_cast<float>(rand()) / RAND_MAX;
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
    // Reset filter states for new voices
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        if (idx < MAX_VOICE_POLYPHONY && voices[idx].active && voices[idx].noteOnTime == currentTime) {
            for (int j = 0; j < 4; j++) {
                voices[idx].filterStates[j] = 0.0f;
            }
        }
    }

    // Vectorized cutoff computation
    float tempCutoffs[4], tempEnvMods[4], tempResonances[4];
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        tempCutoffs[i] = idx < MAX_VOICE_POLYPHONY && voices[idx].active ? voices[idx].smoothedCutoff.getNextValue() : 1000.0f;
        tempEnvMods[i] = idx < MAX_VOICE_POLYPHONY && voices[idx].active ? voices[idx].smoothedFegAmount.getNextValue() * voices[idx].smoothedFilterEnv.getNextValue() * 12000.0f : 0.0f;
        tempResonances[i] = idx < MAX_VOICE_POLYPHONY && voices[idx].active ? voices[idx].resonance * 1.5f : 0.7f * 1.5f;
        tempResonances[i] = std::max(0.0f, std::min(tempResonances[i], 1.4f));
    }

    SIMD_TYPE modulatedCutoffs = SIMD_ADD(SIMD_LOAD(tempCutoffs), SIMD_LOAD(tempEnvMods));
    modulatedCutoffs = SIMD_MAX(SIMD_SET1(20.0f), SIMD_MIN(modulatedCutoffs, SIMD_SET1(filter.sampleRate * 0.48f)));
    float tempModulated[4];
    SIMD_STORE(tempModulated, modulatedCutoffs);
    for (int i = 0; i < 4; i++) {
        tempCutoffs[i] = 2.0f * sinf(juce::MathConstants<float>::pi * tempModulated[i] / filter.sampleRate);
        tempCutoffs[i] = std::tanh(tempCutoffs[i]);
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
    float tempCheck[4];
    SIMD_STORE(tempCheck, output);
    for (int i = 0; i < 4; i++) {
        if (!std::isfinite(tempCheck[i])) {
            tempCheck[i] = 0.0f;
        } else {
            tempCheck[i] = std::tanh(tempCheck[i]);
        }
        // Apply DC offset correction
        tempCheck[i] -= 0.0001f * tempCheck[i];
    }
    output = SIMD_LOAD(tempCheck);
    float tempOut[4];
    SIMD_STORE(tempOut, output);
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

    // Apply fade-out to stolen voice
    if (voices[voiceToSteal].active && !voices[voiceToSteal].released) {
        voices[voiceToSteal].smoothedAmplitude.setTargetValue(0.0f); // Fade to zero
        voices[voiceToSteal].smoothedAmplitude.reset(filter.sampleRate * oversampling->getOversamplingFactor(), 0.005); // Short fade
        voices[voiceToSteal].smoothedFilterEnv.setTargetValue(0.0f);
        voices[voiceToSteal].smoothedFilterEnv.reset(filter.sampleRate * oversampling->getOversamplingFactor(), 0.005); // Fade filter envelope
        for (int j = 0; j < 4; j++) {
            voices[voiceToSteal].filterStates[j] = 0.0f; // Reset filter states
        }
    }

    return voiceToSteal;
}

void SimdSynthAudioProcessor::updateEnvelopes(float t) {
    for (int i = 0; i < MAX_VOICE_POLYPHONY; i++) {
        if (!voices[i].active) {
            voices[i].amplitude = 0.0f;
            voices[i].filterEnv = 0.0f;
            voices[i].smoothedAmplitude.setCurrentAndTargetValue(0.0f); // Ensure reset
            voices[i].smoothedFilterEnv.setCurrentAndTargetValue(0.0f);
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] *= 0.999f;
            }
            continue;
        }

        float localTime = std::max(0.0f, t - voices[i].noteOnTime);
        float attack = std::max(voices[i].attack, 0.01f);
        float decay = std::max(voices[i].decay, 0.01f);
        float sustain = juce::jlimit(0.0f, 1.0f, voices[i].sustain);
        float release = std::max(voices[i].release, 0.01f);

        const float attackCurve = voices[i].attackCurve;
        const float decayCurve = 1.5f;
        const float releaseCurve = voices[i].releaseCurve;

        // Amplitude envelope
        float amplitude;
        if (localTime < attack) {
            float attackPhase = localTime / attack;
            amplitude = std::pow(attackPhase, attackCurve);
        } else if (localTime < attack + decay) {
            float decayPhase = (localTime - attack) / decay;
            amplitude = 1.0f - std::pow(decayPhase, decayCurve) * (1.0f - sustain);
        } else if (!voices[i].released) {
            amplitude = sustain;
        } else {
            float releaseTime = std::max(0.0f, t - voices[i].noteOffTime);
            float releasePhase = releaseTime / release;
            amplitude = voices[i].releaseStartAmplitude * std::exp(-releasePhase * releaseCurve);
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
            filterEnv = voices[i].fegSustain * std::exp(-releasePhase * releaseCurve);

        }

        voices[i].filterEnv = juce::jlimit(0.0f, 1.0f, filterEnv);
        voices[i].smoothedFilterEnv.setTargetValue(voices[i].filterEnv);
    }
}

void SimdSynthAudioProcessor::updateVoiceParameters(float sampleRate) {
    sampleRate = std::max(sampleRate, 44100.0f); // Safeguard against invalid sample rate

    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        // Update parameters for all voices (active or inactive)
        voices[i].attack = *attackTimeParam;
        voices[i].decay = *decayTimeParam;
        voices[i].sustain = *sustainLevelParam;
        voices[i].release = *releaseTimeParam;
        voices[i].cutoff = *cutoffParam;
        voices[i].resonance = *resonanceParam;
        voices[i].fegAttack = *fegAttackParam;
        voices[i].fegDecay = *fegDecayParam;
        voices[i].fegSustain = *fegSustainParam;
        voices[i].fegRelease = *fegReleaseParam;
        voices[i].fegAmount = *fegAmountParam;
        voices[i].smoothedCutoff.setTargetValue(*cutoffParam);
        voices[i].smoothedFegAmount.setTargetValue(*fegAmountParam);
        voices[i].lfoRate = smoothedLfoRate.getCurrentValue();
        voices[i].lfoDepth = smoothedLfoDepth.getCurrentValue();
        voices[i].subTune = smoothedSubTune.getCurrentValue();
        voices[i].subMix = smoothedSubMix.getCurrentValue();
        voices[i].subTrack = smoothedSubTrack.getCurrentValue();
        voices[i].osc2Tune = smoothedOsc2Tune.getCurrentValue();
        voices[i].osc2Mix = smoothedOsc2Mix.getCurrentValue();
        voices[i].osc2Track = smoothedOsc2Track.getCurrentValue();
        voices[i].detune = smoothedDetune.getCurrentValue();
        voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
        voices[i].unison = static_cast<int>(*unisonParam);
        voices[i].detuneFactors.resize(voices[i].unison);
        for (int u = 0; u < voices[i].unison; ++u) {
            float detune = voices[i].detune * (u - (voices[i].unison - 1) / 2.0f) /
                           (voices[i].unison - 1 + 0.0001f);
            voices[i].detuneFactors[u] = powf(2.0f, detune / 12.0f);
        }

        // Update phase increments for active voices
        if (voices[i].active) {
            voices[i].phaseIncrement = voices[i].frequency / sampleRate;
            voices[i].subPhaseIncrement = voices[i].frequency * powf(2.0f, voices[i].subTune / 12.0f) *
                                          voices[i].subTrack / sampleRate;
            voices[i].osc2PhaseIncrement = voices[i].frequency * powf(2.0f, voices[i].osc2Tune / 12.0f) *
                                           voices[i].osc2Track / sampleRate;
        }
    }
}

// Prepare audio processing with oversampling
void SimdSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    filter.sampleRate = static_cast<float>(sampleRate);
    currentTime = 0.0;
    oversampling->reset();
    int oversamplingFactor = (samplesPerBlock < 256) ? 1 : 2;
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, oversamplingFactor, juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing(samplesPerBlock);

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
        voices[i].smoothedAmplitude.reset(sampleRate * oversamplingFactor, 0.05); // Reset with oversampled rate
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

    // Update smoothed parameter targets
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

    // Update voice parameters before processing
    updateVoiceParameters(sampleRate);

    int activeCount = 0;
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        if (voices[i].active) {
            ++activeCount;
        }
    }
    float voiceScaling = (activeCount > 0) ? (1.0f / static_cast<float>(activeCount)) : 1.0f;

    // Process MIDI and audio
    int sampleIndex = 0;
    juce::MidiBuffer oversampledMidi;
    for (const auto metadata : midiMessages) {
        oversampledMidi.addEvent(metadata.getMessage(), metadata.samplePosition * oversampling->getOversamplingFactor());
    }

    for (const auto metadata : oversampledMidi) {
        auto msg = metadata.getMessage();
        int samplePosition = metadata.samplePosition;

        while (sampleIndex < samplePosition && sampleIndex < oversampledBlock.getNumSamples()) {
            float t = static_cast<float>(blockStartTime + static_cast<double>(sampleIndex) / sampleRate);
            updateEnvelopes(t);
            float outputSample = 0.0f;
            const SIMD_TYPE twoPi = SIMD_SET1(2.0f * juce::MathConstants<float>::pi);

            constexpr int SIMD_WIDTH = (sizeof(SIMD_TYPE) / sizeof(float));
            constexpr int NUM_BATCHES = (MAX_VOICE_POLYPHONY + SIMD_WIDTH - 1) / SIMD_WIDTH;

            for (int batch = 0; batch < NUM_BATCHES; batch++) {
                const int voiceOffset = batch * SIMD_WIDTH;
                if (voiceOffset >= MAX_VOICE_POLYPHONY) continue;
                bool anyActive = false;
                for (int j = 0; j < 4 && voiceOffset + j < MAX_VOICE_POLYPHONY; ++j) {
                    if (voices[voiceOffset + j].active) {
                        anyActive = true;
                        break;
                    }
                }
                if (!anyActive) continue;

                alignas(32) float tempAmps[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                alignas(32) float tempPhases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempIncrements[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempLfoPhases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempLfoRates[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempLfoDepths[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempSubPhases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempSubIncrements[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempSubMixes[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempOsc2Phases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempOsc2Increments[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempOsc2Mixes[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempWavetableTypes[4] = {0.0f, 0.0f, 0.0f, 0.0f};

                for (int j = 0; j < 4; ++j) {
                    int idx = voiceOffset + j;
                    if (idx >= MAX_VOICE_POLYPHONY || !voices[idx].active) continue;
                    tempAmps[j] = voices[idx].smoothedAmplitude.getNextValue() * voices[idx].velocity; // Use smoothed amplitude
                    tempPhases[j] = voices[idx].phase;
                    tempIncrements[j] = voices[idx].phaseIncrement;
                    tempLfoPhases[j] = voices[idx].lfoPhase;
                    tempLfoRates[j] = voices[idx].lfoRate;
                    tempLfoDepths[j] = voices[idx].lfoDepth;
                    tempSubPhases[j] = voices[idx].subPhase;
                    tempSubIncrements[j] = voices[idx].subPhaseIncrement;
                    tempSubMixes[j] = voices[idx].subMix;
                    tempOsc2Phases[j] = voices[idx].osc2Phase;
                    tempOsc2Increments[j] = voices[idx].osc2PhaseIncrement;
                    tempOsc2Mixes[j] = voices[idx].osc2Mix;
                    tempWavetableTypes[j] = static_cast<float>(voices[idx].wavetableType);
                }

                SIMD_TYPE amplitudes = SIMD_LOAD(tempAmps);
                SIMD_TYPE phases = SIMD_LOAD(tempPhases);
                SIMD_TYPE increments = SIMD_LOAD(tempIncrements);
                SIMD_TYPE lfoPhases = SIMD_LOAD(tempLfoPhases);
                SIMD_TYPE lfoRates = SIMD_LOAD(tempLfoRates);
                SIMD_TYPE lfoDepths = SIMD_LOAD(tempLfoDepths);
                SIMD_TYPE subPhases = SIMD_LOAD(tempSubPhases);
                SIMD_TYPE subIncrements = SIMD_LOAD(tempSubIncrements);
                SIMD_TYPE subMixes = SIMD_LOAD(tempSubMixes);
                SIMD_TYPE osc2Phases = SIMD_LOAD(tempOsc2Phases);
                SIMD_TYPE osc2Increments = SIMD_LOAD(tempOsc2Increments);
                SIMD_TYPE osc2Mixes = SIMD_LOAD(tempOsc2Mixes);
                SIMD_TYPE wavetableTypePs = SIMD_LOAD(tempWavetableTypes);

                // Update LFO with smoothed rate
                SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRates, SIMD_SET1(2.0f * juce::MathConstants<float>::pi / sampleRate));
                lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
                lfoPhases = SIMD_SUB(lfoPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)), twoPi));
                SIMD_TYPE lfoValues = SIMD_SIN(lfoPhases);
                lfoValues = SIMD_MUL(lfoValues, lfoDepths);
                SIMD_TYPE phaseMod = SIMD_DIV(lfoValues, twoPi);

                // Unison processing with precomputed detune factors
                SIMD_TYPE unisonOutput = SIMD_SET1(0.0f);
                int unisonVoices = static_cast<int>(*unisonParam);
                for (int u = 0; u < unisonVoices; ++u) {
                    SIMD_TYPE detuneFactor = SIMD_SET1(voices[voiceOffset].detuneFactors[u]);
                    SIMD_TYPE detunedPhases = SIMD_ADD(phases, phaseMod);
                    detunedPhases = SIMD_MUL(detunedPhases, detuneFactor);
                    SIMD_TYPE phases_norm = SIMD_SUB(detunedPhases, SIMD_FLOOR(detunedPhases));
                    SIMD_TYPE mainValues = wavetable_lookup_ps(phases_norm, wavetableTypePs);
                    mainValues = SIMD_MUL(mainValues, SIMD_SET1(1.0f / static_cast<float>(unisonVoices)));
                    unisonOutput = SIMD_ADD(unisonOutput, mainValues);
                }

                // Normalize mix to maintain consistent loudness
                SIMD_TYPE totalMix = SIMD_ADD(SIMD_ADD(SIMD_SET1(1.0f), subMixes), osc2Mixes);
                totalMix = SIMD_MAX(totalMix, SIMD_SET1(1e-6f)); // Prevent division by zero
                SIMD_TYPE mainMix = SIMD_DIV(SIMD_SET1(1.0f), totalMix);
                SIMD_TYPE subMixNorm = SIMD_DIV(subMixes, totalMix);
                SIMD_TYPE osc2MixNorm = SIMD_DIV(osc2Mixes, totalMix);
                unisonOutput = SIMD_MUL(unisonOutput, amplitudes);
                unisonOutput = SIMD_MUL(unisonOutput, mainMix);

                // Sub-oscillator
                SIMD_TYPE subSinValues = SIMD_SIN(subPhases);
                subSinValues = SIMD_MUL(subSinValues, amplitudes);
                subSinValues = SIMD_MUL(subSinValues, subMixNorm);

                // 2nd oscillator
                SIMD_TYPE osc2Values = wavetable_lookup_ps(osc2Phases, wavetableTypePs);
                osc2Values = SIMD_MUL(osc2Values, amplitudes);
                osc2Values = SIMD_MUL(osc2Values, osc2MixNorm);

                // Combine oscillators
                SIMD_TYPE combinedValues = SIMD_ADD(SIMD_ADD(unisonOutput, subSinValues), osc2Values);
                SIMD_TYPE filteredOutput;
                applyLadderFilter(voices, voiceOffset, combinedValues, filter, filteredOutput);
                float temp[4];
                SIMD_STORE(temp, filteredOutput);
                for (int j = 0; j < 4; ++j) {
                    int idx = voiceOffset + j;
                    if (idx < MAX_VOICE_POLYPHONY && voices[idx].active) {
                        outputSample += temp[j];
                    }
                }

                // Update phases
                float tempPhase[4];
                phases = SIMD_ADD(phases, increments);
                phases = SIMD_SUB(phases, SIMD_FLOOR(phases));
                SIMD_STORE(tempPhase, phases);
                subPhases = SIMD_ADD(subPhases, subIncrements);
                subPhases = SIMD_SUB(subPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(subPhases, twoPi)), twoPi));
                SIMD_STORE(temp, subPhases);
                osc2Phases = SIMD_ADD(osc2Phases, osc2Increments);
                osc2Phases = SIMD_SUB(osc2Phases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(osc2Phases, twoPi)), twoPi));
                float tempOsc2[4];
                SIMD_STORE(tempOsc2, osc2Phases);
                SIMD_STORE(temp, lfoPhases);
                for (int j = 0; j < 4; ++j) {
                    int idx = voiceOffset + j;
                    if (idx < MAX_VOICE_POLYPHONY && voices[idx].active) {
                        voices[idx].phase = tempPhase[j];
                        voices[idx].subPhase = temp[j];
                        voices[idx].osc2Phase = tempOsc2[j];
                        voices[idx].lfoPhase = temp[j];
                    }
                }
            }

            outputSample *= voiceScaling;
            outputSample *= smoothedGain.getNextValue();

            if (std::isnan(outputSample) || !std::isfinite(outputSample)) {
                outputSample = 0.0f;
            }

            if (std::abs(outputSample) > 1.0f) {
                DBG("High output at sample " << sampleIndex << ": " << outputSample);
                for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
                    if (voices[i].active) {
                        DBG("Voice " << i << ": amp=" << voices[i].smoothedAmplitude.getCurrentValue()
                            << ", filterEnv=" << voices[i].smoothedFilterEnv.getCurrentValue());
                    }
                }
            }

            for (int channel = 0; channel < totalNumOutputChannels; ++channel) {
                oversampledBlock.setSample(channel, sampleIndex, outputSample);
            }
            sampleIndex++;
        }

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
                if (voiceIndex == -1) {
                    voiceIndex = findVoiceToSteal();
                    if (voices[voiceIndex].active) {
                        voices[voiceIndex].released = true;
                        voices[voiceIndex].isHeld = false;
                        voices[voiceIndex].noteOffTime = static_cast<float>(blockStartTime + static_cast<double>(samplePosition) / sampleRate);
                        voices[voiceIndex].releaseStartAmplitude = voices[voiceIndex].smoothedAmplitude.getCurrentValue();
                        // Preserve phase continuity for stolen voice
                        // Do not reset phases to avoid discontinuities
                    }
                }
            }
            // Initialize new voice
            bool wasActive = voices[voiceIndex].active;  // Detect if this is a stolen (previously active) voice

            voices[voiceIndex].active = true;
            voices[voiceIndex].released = false;
            voices[voiceIndex].isHeld = true;
            voices[voiceIndex].smoothedAmplitude.setCurrentAndTargetValue(0.0f); // Reset amplitude
            voices[voiceIndex].smoothedAmplitude.reset(sampleRate, 0.005); // Ensure smooth ramp
            voices[voiceIndex].smoothedFilterEnv.setCurrentAndTargetValue(0.0f); // Reset filter envelope
            voices[voiceIndex].smoothedFilterEnv.reset(sampleRate, 0.005); // Ensure smooth ramp
            voices[voiceIndex].frequency = midiToFreq(note);
            voices[voiceIndex].phaseIncrement = voices[voiceIndex].frequency / sampleRate;
            if (!wasActive) {
                voices[voiceIndex].phase = random.nextFloat() * 2.0f * juce::MathConstants<float>::pi; // Randomize only for new/idle voices
            }

            voices[voiceIndex].subPhase = voices[voiceIndex].phase; // Sync sub-oscillator phase
            voices[voiceIndex].osc2Phase = voices[voiceIndex].phase; // Sync osc2 phase
            voices[voiceIndex].lfoPhase = random.nextFloat() * 2.0f * juce::MathConstants<float>::pi;
            voices[voiceIndex].noteNumber = note;
            voices[voiceIndex].velocity = velocity;
            voices[voiceIndex].voiceAge = 0.0f;
            voices[voiceIndex].noteOnTime = static_cast<float>(blockStartTime + static_cast<double>(samplePosition) / sampleRate);
            voices[voiceIndex].releaseStartAmplitude = 0.0f;
            voices[voiceIndex].subPhaseIncrement = voices[voiceIndex].frequency * powf(2.0f, smoothedSubTune.getCurrentValue() / 12.0f) *
                                                   smoothedSubTrack.getCurrentValue() / sampleRate;
            voices[voiceIndex].osc2PhaseIncrement = voices[voiceIndex].frequency * powf(2.0f, smoothedOsc2Tune.getCurrentValue() / 12.0f) *
                                                    smoothedOsc2Track.getCurrentValue() / sampleRate;
            voices[voiceIndex].wavetableType = static_cast<int>(*wavetableTypeParam);
            voices[voiceIndex].attack = *attackTimeParam;
            voices[voiceIndex].decay = *decayTimeParam;
            voices[voiceIndex].sustain = *sustainLevelParam;
            voices[voiceIndex].release = *releaseTimeParam;
            voices[voiceIndex].cutoff = *cutoffParam;
            voices[voiceIndex].resonance = *resonanceParam;
            voices[voiceIndex].fegAttack = *fegAttackParam;
            voices[voiceIndex].fegDecay = *fegDecayParam;
            voices[voiceIndex].fegSustain = *fegSustainParam;
            voices[voiceIndex].fegRelease = *fegReleaseParam;
            voices[voiceIndex].fegAmount = *fegAmountParam;
            voices[voiceIndex].lfoRate = smoothedLfoRate.getCurrentValue();
            voices[voiceIndex].lfoDepth = smoothedLfoDepth.getCurrentValue();
            voices[voiceIndex].subTune = smoothedSubTune.getCurrentValue();
            voices[voiceIndex].subMix = smoothedSubMix.getCurrentValue();
            voices[voiceIndex].subTrack = smoothedSubTrack.getCurrentValue();
            voices[voiceIndex].osc2Tune = smoothedOsc2Tune.getCurrentValue();
            voices[voiceIndex].osc2Mix = smoothedOsc2Mix.getCurrentValue();
            voices[voiceIndex].osc2Track = smoothedOsc2Track.getCurrentValue();
            voices[voiceIndex].detune = smoothedDetune.getCurrentValue();
            voices[voiceIndex].unison = static_cast<int>(*unisonParam);
            voices[voiceIndex].detuneFactors.resize(voices[voiceIndex].unison);
            for (int u = 0; u < voices[voiceIndex].unison; ++u) {
                float detune = voices[voiceIndex].detune * (u - (voices[voiceIndex].unison - 1) / 2.0f) /
                               (voices[voiceIndex].unison - 1 + 0.0001f);
                voices[voiceIndex].detuneFactors[u] = powf(2.0f, detune / 12.0f);
            }
            for (int j = 0; j < 4; ++j) {
                voices[voiceIndex].filterStates[j] = 0.0f;
            }
        } else if (msg.isNoteOff()) {
            int note = msg.getNoteNumber();
            for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
                if (voices[i].active && voices[i].noteNumber == note) {
                    voices[i].released = true;
                    voices[i].isHeld = false;
                    voices[i].releaseStartAmplitude = voices[i].amplitude;
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

        for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
            if (voices[i].active) {
                voices[i].voiceAge += buffer.getNumSamples() / sampleRate;
            }
        }
    }

    // Process remaining samples
    while (sampleIndex < oversampledBlock.getNumSamples()) {
        float t = static_cast<float>(blockStartTime + static_cast<double>(sampleIndex) / sampleRate);
        updateEnvelopes(t);
        float outputSample = 0.0f;
        const SIMD_TYPE twoPi = SIMD_SET1(2.0f * juce::MathConstants<float>::pi);

        constexpr int SIMD_WIDTH = (sizeof(SIMD_TYPE) / sizeof(float));
        constexpr int NUM_BATCHES = (MAX_VOICE_POLYPHONY + SIMD_WIDTH - 1) / SIMD_WIDTH;

        for (int batch = 0; batch < NUM_BATCHES; batch++) {
            const int voiceOffset = batch * SIMD_WIDTH;
            if (voiceOffset >= MAX_VOICE_POLYPHONY) continue;
            bool anyActive = false;
            for (int j = 0; j < 4 && voiceOffset + j < MAX_VOICE_POLYPHONY; ++j) {
                if (voices[voiceOffset + j].active) {
                    anyActive = true;
                    break;
                }
            }
            if (!anyActive) continue;

            alignas(32) float tempAmps[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            alignas(32) float tempPhases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempIncrements[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempLfoPhases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempLfoRates[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempLfoDepths[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempSubPhases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempSubIncrements[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempSubMixes[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempOsc2Phases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempOsc2Increments[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempOsc2Mixes[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempWavetableTypes[4] = {0.0f, 0.0f, 0.0f, 0.0f};

            for (int j = 0; j < 4; ++j) {
                int idx = voiceOffset + j;
                if (idx >= MAX_VOICE_POLYPHONY || !voices[idx].active) continue;
                tempAmps[j] = voices[idx].smoothedAmplitude.getNextValue() * voices[idx].velocity; // Use smoothed amplitude
                tempPhases[j] = voices[idx].phase;
                tempIncrements[j] = voices[idx].phaseIncrement;
                tempLfoPhases[j] = voices[idx].lfoPhase;
                tempLfoRates[j] = voices[idx].lfoRate;
                tempLfoDepths[j] = voices[idx].lfoDepth;
                tempSubPhases[j] = voices[idx].subPhase;
                tempSubIncrements[j] = voices[idx].subPhaseIncrement;
                tempSubMixes[j] = voices[idx].subMix;
                tempOsc2Phases[j] = voices[idx].osc2Phase;
                tempOsc2Increments[j] = voices[idx].osc2PhaseIncrement;
                tempOsc2Mixes[j] = voices[idx].osc2Mix;
                tempWavetableTypes[j] = static_cast<float>(voices[idx].wavetableType);
            }

            SIMD_TYPE amplitudes = SIMD_LOAD(tempAmps);
            SIMD_TYPE phases = SIMD_LOAD(tempPhases);
            SIMD_TYPE increments = SIMD_LOAD(tempIncrements);
            SIMD_TYPE lfoPhases = SIMD_LOAD(tempLfoPhases);
            SIMD_TYPE lfoRates = SIMD_LOAD(tempLfoRates);
            SIMD_TYPE lfoDepths = SIMD_LOAD(tempLfoDepths);
            SIMD_TYPE subPhases = SIMD_LOAD(tempSubPhases);
            SIMD_TYPE subIncrements = SIMD_LOAD(tempSubIncrements);
            SIMD_TYPE subMixes = SIMD_LOAD(tempSubMixes);
            SIMD_TYPE osc2Phases = SIMD_LOAD(tempOsc2Phases);
            SIMD_TYPE osc2Increments = SIMD_LOAD(tempOsc2Increments);
            SIMD_TYPE osc2Mixes = SIMD_LOAD(tempOsc2Mixes);
            SIMD_TYPE wavetableTypePs = SIMD_LOAD(tempWavetableTypes);

            // Update LFO with smoothed rate
            SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRates, SIMD_SET1(2.0f * juce::MathConstants<float>::pi / sampleRate));
            lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
            lfoPhases = SIMD_SUB(lfoPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)), twoPi));
            SIMD_TYPE lfoValues = SIMD_SIN(lfoPhases);
            lfoValues = SIMD_MUL(lfoValues, lfoDepths);
            SIMD_TYPE phaseMod = SIMD_DIV(lfoValues, twoPi);

            // Unison processing with precomputed detune factors
            SIMD_TYPE unisonOutput = SIMD_SET1(0.0f);
            int unisonVoices = static_cast<int>(*unisonParam);
            for (int u = 0; u < unisonVoices; ++u) {
                SIMD_TYPE detuneFactor = SIMD_SET1(voices[voiceOffset].detuneFactors[u]);
                SIMD_TYPE detunedPhases = SIMD_ADD(phases, phaseMod); // Apply LFO modulation
                detunedPhases = SIMD_MUL(detunedPhases, detuneFactor);
                SIMD_TYPE phases_norm = SIMD_SUB(detunedPhases, SIMD_FLOOR(detunedPhases));
                SIMD_TYPE mainValues = wavetable_lookup_ps(phases_norm, wavetableTypePs);
                mainValues = SIMD_MUL(mainValues, SIMD_SET1(1.0f / static_cast<float>(unisonVoices)));
                unisonOutput = SIMD_ADD(unisonOutput, mainValues);
            }

            // Normalize mix to maintain consistent loudness
            SIMD_TYPE totalMix = SIMD_ADD(SIMD_ADD(SIMD_SET1(1.0f), subMixes), osc2Mixes);
            totalMix = SIMD_MAX(totalMix, SIMD_SET1(1e-6f));
            SIMD_TYPE mainMix = SIMD_DIV(SIMD_SET1(1.0f), totalMix);
            SIMD_TYPE subMixNorm = SIMD_DIV(subMixes, totalMix);
            SIMD_TYPE osc2MixNorm = SIMD_DIV(osc2Mixes, totalMix);
            unisonOutput = SIMD_MUL(unisonOutput, amplitudes);
            unisonOutput = SIMD_MUL(unisonOutput, mainMix);

            // Sub-oscillator with LFO modulation
            SIMD_TYPE subPhasesMod = SIMD_ADD(subPhases, phaseMod);
            SIMD_TYPE subSinValues = SIMD_SIN(subPhasesMod);
            subSinValues = SIMD_MUL(subSinValues, amplitudes);
            subSinValues = SIMD_MUL(subSinValues, subMixNorm);

            // 2nd oscillator with LFO modulation
            SIMD_TYPE osc2PhasesMod = SIMD_ADD(osc2Phases, phaseMod);
            SIMD_TYPE osc2Values = wavetable_lookup_ps(osc2PhasesMod, wavetableTypePs);
            osc2Values = SIMD_MUL(osc2Values, amplitudes);
            osc2Values = SIMD_MUL(osc2Values, osc2MixNorm);

            // Combine oscillators
            SIMD_TYPE combinedValues = SIMD_ADD(SIMD_ADD(unisonOutput, subSinValues), osc2Values);
            SIMD_TYPE filteredOutput;
            applyLadderFilter(voices, voiceOffset, combinedValues, filter, filteredOutput);
            float temp[4];
            SIMD_STORE(temp, filteredOutput);
            for (int j = 0; j < 4; ++j) {
                int idx = voiceOffset + j;
                if (idx < MAX_VOICE_POLYPHONY && voices[idx].active) {
                    outputSample += temp[j];
                }
            }

            // Update phases
            float tempPhase[4];
            phases = SIMD_ADD(phases, increments);
            phases = SIMD_SUB(phases, SIMD_FLOOR(phases));
            SIMD_STORE(tempPhase, phases);
            subPhases = SIMD_ADD(subPhases, subIncrements);
            subPhases = SIMD_SUB(subPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(subPhases, twoPi)), twoPi));
            SIMD_STORE(temp, subPhases);
            osc2Phases = SIMD_ADD(osc2Phases, osc2Increments);
            osc2Phases = SIMD_SUB(osc2Phases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(osc2Phases, twoPi)), twoPi));
            float tempOsc2[4];
            SIMD_STORE(tempOsc2, osc2Phases);
            SIMD_STORE(temp, lfoPhases);
            for (int j = 0; j < 4; ++j) {
                int idx = voiceOffset + j;
                if (idx < MAX_VOICE_POLYPHONY && voices[idx].active) {
                    voices[idx].phase = tempPhase[j];
                    voices[idx].subPhase = temp[j];
                    voices[idx].osc2Phase = tempOsc2[j];
                    voices[idx].lfoPhase = temp[j];
                }
            }
        }

        outputSample *= voiceScaling;
        outputSample *= smoothedGain.getNextValue();

        if (std::isnan(outputSample) || !std::isfinite(outputSample)) {
            outputSample = 0.0f;
        }

        for (int channel = 0; channel < totalNumOutputChannels; ++channel) {
            oversampledBlock.setSample(channel, sampleIndex, outputSample);
        }
        sampleIndex++;
    }

    // Downsample back to original rate
    oversampling->processSamplesDown(block);

    // Update current time
    currentTime = blockStartTime + static_cast<double>(buffer.getNumSamples()) / filter.sampleRate;
}


// Create the editor
juce::AudioProcessorEditor* SimdSynthAudioProcessor::createEditor() {
    return new SimdSynthAudioProcessorEditor(*this);
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
            int program = xmlState->getIntAttribute("currentProgram", 0);
            if (program >= 0 && program < getNumPrograms()) {
                setCurrentProgram(program);
            }
        }
    }
}

// Plugin creation function
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SimdSynthAudioProcessor();
}