/*simdsynth - a playground for experimenting with SIMD-based audio
        synthesis, with polyphonic main and sub-oscillator,
        filter, envelopes, and LFO per voice, up to 8 voices.

MIT Licensed, (c) 2025, seclorum
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_core/juce_core.h> // For File and JSON handling
#include <juce_dsp/juce_dsp.h> // For oversampling and DSP utilities// Constructor: Initializes the audio processor with stereo output and enhanced parameters

SimdSynthAudioProcessor::SimdSynthAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("SimdSynth"),
                 {std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"wavetable", parameterVersion}, "Wavetable Type", 0.0f, 2.0f, 0.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"attack", parameterVersion}, "Attack Time", 0.01f, 5.0f, 0.1f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"decay", parameterVersion}, "Decay Time", 0.1f, 5.0f, 0.5f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"sustain", parameterVersion}, "Sustain Level", 0.0f, 1.0f, 0.8f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"release", parameterVersion}, "Release Time", 0.01f, 5.0f, 0.2f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"cutoff", parameterVersion}, "Filter Cutoff", 20.0f, 20000.0f, 1000.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"resonance", parameterVersion}, "Filter Resonance", 0.0f, 1.0f, 0.7f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegAttack", parameterVersion}, "Filter EG Attack", 0.01f, 5.0f, 0.1f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegDecay", parameterVersion}, "Filter EG Decay", 0.1f, 5.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegSustain", parameterVersion}, "Filter EG Sustain", 0.0f, 1.0f, 0.5f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegRelease", parameterVersion}, "Filter EG Release", 0.01f, 5.0f, 0.2f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegAmount", parameterVersion}, "Filter EG Amount", -1.0f, 1.0f, 0.5f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lfoRate", parameterVersion}, "LFO Rate", 0.0f, 20.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lfoDepth", parameterVersion}, "LFO Depth", 0.0f, 0.5f, 0.05f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subTune", parameterVersion}, "Sub Osc Tune", -24.0f, 24.0f, -12.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subMix", parameterVersion}, "Sub Osc Mix", 0.0f, 1.0f, 0.5f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subTrack", parameterVersion}, "Sub Osc Track", 0.0f, 1.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain", parameterVersion}, "Output Gain", 0.0f, 2.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"unison", parameterVersion}, "Unison Voices", 1.0f, 8.0f, 1.0f),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"detune", parameterVersion}, "Unison Detune", 0.0f, 0.1f, 0.01f)}),
      currentTime(0.0), oversampling(std::make_unique<juce::dsp::Oversampling<float>>(
                            2, 1, juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR, true, true)) {
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
    gainParam = parameters.getRawParameterValue("gain");
    unisonParam = parameters.getRawParameterValue("unison");
    detuneParam = parameters.getRawParameterValue("detune");

    // Ensure safe initial values
    if (auto *param = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("gain"))) {
        param->setValueNotifyingHost(param->convertTo0to1(1.0f));
    }
    if (auto *param = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("sustain"))) {
        param->setValueNotifyingHost(param->convertTo0to1(0.8f));
    }
    if (auto *param = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("subMix"))) {
        param->setValueNotifyingHost(param->convertTo0to1(0.5f));
    }

    // Initialize voices
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
        voices[i].resonance = *resonanceParam;
        voices[i].sampleRate = 44100.0f;
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
        voices[i].unison = static_cast<int>(*unisonParam);
        voices[i].detune = *detuneParam;
        voices[i].releaseStartAmplitude = 0.0f;
        voices[i].releaseStartFilterEnv = 0.0f;
        voices[i].attackCurve = 2.0f;
        voices[i].releaseCurve = 3.0f;
        DBG("Voice " << i << " initialized: gain=" << *gainParam << ", sustain=" << *sustainLevelParam
            << ", subMix=" << *subMixParam);
    }

    // Initialize LookupTableTransform objects with debug logging
    DBG("Initializing sineTableTransform: min=0.0, max=1.0, size=" << WAVETABLE_SIZE);
    sineTableTransform.initialise([](float x) { return sinf(2.0f * M_PI * x); }, 0.0f, 1.0f, WAVETABLE_SIZE);
    DBG("Initializing sawTableTransform: min=0.0, max=1.0, size=" << WAVETABLE_SIZE);
    sawTableTransform.initialise([](float x) { return 2.0f * x - 1.0f; }, 0.0f, 1.0f, WAVETABLE_SIZE);
    DBG("Initializing squareTableTransform: min=0.0, max=1.0, size=" << WAVETABLE_SIZE);
    squareTableTransform.initialise([](float x) { return x < 0.5f ? 1.0f : -1.0f; }, 0.0f, 1.0f, WAVETABLE_SIZE);

    // Initialize wavetables and presets
    initWavetables();
    presetManager.createDefaultPresets();
    loadPresetsFromDirectory();
}
int SimdSynthAudioProcessor::getPreferredBufferSize() const {
    return 512; // Suggest a larger buffer size to reduce underflow risk
} // Destructor: Clean up oversampling
SimdSynthAudioProcessor::~SimdSynthAudioProcessor() { oversampling.reset(); } // Load presets from directory
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
} // Return the number of available presets

int SimdSynthAudioProcessor::getNumPrograms() { return presetNames.size(); } // Return the current preset index

int SimdSynthAudioProcessor::getCurrentProgram() { return currentProgram; }

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
        presetManager.createDefaultPresets();
        return;
    }

    auto jsonString = presetFile.loadFileAsString();
    auto parsedJson = juce::JSON::parse(jsonString);

    if (!parsedJson.isObject()) {
        DBG("Error: Invalid JSON format in preset: " << presetNames[index]);
        presetManager.createDefaultPresets();
        return;
    }

    juce::var synthParams = parsedJson.getProperty("SimdSynth", juce::var());
    if (!synthParams.isObject()) {
        DBG("Error: 'SimdSynth' object not found in preset: " << presetNames[index]);
        presetManager.createDefaultPresets();
        return;
    }

    std::map<juce::String, float> defaultValues = {
        {"wavetable", 0.0f}, {"attack", 0.1f}, {"decay", 0.5f}, {"sustain", 0.8f}, {"release", 0.2f},
        {"cutoff", 1000.0f}, {"resonance", 0.7f}, {"fegAttack", 0.1f}, {"fegDecay", 1.0f}, {"fegSustain", 0.5f},
        {"fegRelease", 0.2f}, {"fegAmount", 0.5f}, {"lfoRate", 1.0f}, {"lfoDepth", 0.05f}, {"subTune", -12.0f},
        {"subMix", 0.5f}, {"subTrack", 1.0f}, {"gain", 1.0f}, {"unison", 1.0f}, {"detune", 0.01f}};

    juce::StringArray paramIds = {"wavetable", "attack", "decay", "sustain", "release",
                                  "cutoff", "resonance", "fegAttack", "fegDecay", "fegSustain",
                                  "fegRelease", "fegAmount", "lfoRate", "lfoDepth", "subTune",
                                  "subMix", "subTrack", "gain", "unison", "detune"};

    bool anyParamUpdated = false;
    for (const auto &paramId : paramIds) {
        if (auto *param = parameters.getParameter(paramId)) {
            if (auto *floatParam = dynamic_cast<juce::AudioParameterFloat *>(param)) {
                float value = synthParams.hasProperty(paramId)
                                  ? static_cast<float>(synthParams.getProperty(paramId, defaultValues[paramId]))
                                  : defaultValues[paramId];
                if (paramId == "wavetable" || paramId == "unison") {
                    value = std::round(value);
                }
                value = juce::jlimit(floatParam->getNormalisableRange().start, floatParam->getNormalisableRange().end, value);
                // Ensure non-silencing values
                if (paramId == "gain") value = std::max(0.5f, value);
                if (paramId == "sustain") value = std::max(0.5f, value);
                if (paramId == "subMix") value = juce::jlimit(0.0f, 0.7f, value); // Allow main oscillator
                floatParam->setValueNotifyingHost(floatParam->convertTo0to1(value));
                DBG("Setting " << paramId << " to " << value);
                anyParamUpdated = true;
            }
        }
    }

    if (!anyParamUpdated) {
        DBG("Warning: No parameters updated for preset: " << presetNames[index]);
        presetManager.createDefaultPresets();
    }

    // Update voice parameters
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
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
        voices[i].lfoRate = *lfoRateParam;
        voices[i].lfoDepth = *lfoDepthParam;
        voices[i].subTune = *subTuneParam;
        voices[i].subMix = *subMixParam;
        voices[i].subTrack = *subTrackParam;
        voices[i].unison = static_cast<int>(*unisonParam);
        voices[i].detune = *detuneParam;
        if (voices[i].active) {
            voices[i].phase = 0.0f;
            voices[i].subPhase = 0.0f;
            voices[i].lfoPhase = 0.0f;
            for (int j = 0; j < 4; j++) {
#if defined(__aarch64__) || defined(__arm64__)
                voices[i].filterStates[j] = vdupq_n_f32(0.0f);
#else
                voices[i].filterStates[j] = 0.0f;
#endif
            }
        }
        DBG("Voice " << i << " updated: gain=" << *gainParam << ", sustain=" << *sustainLevelParam
            << ", subMix=" << *subMixParam);
    }

    // Update editor
    if (auto *editor = getActiveEditor()) {
        if (auto *synthEditor = dynamic_cast<SimdSynthAudioProcessorEditor *>(editor)) {
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
} // Initialize wavetables (added square wave)
void SimdSynthAudioProcessor::initWavetables() {
    for (int i = 0; i < WAVETABLE_SIZE; ++i) {
        float t = static_cast<float>(i) / WAVETABLE_SIZE;
        sineTable[i] = sinf(2.0f * M_PI * t);     // Sine wave: [-1, 1]
        sawTable[i] = 2.0f * t - 1.0f;            // Sawtooth: [-1, 1]
        squareTable[i] = t < 0.5f ? 1.0f : -1.0f; // Square wave: [-1, 1]
    }
} // Convert MIDI note number to frequency
float SimdSynthAudioProcessor::midiToFreq(int midiNote) {
    return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
} // Randomize a value within a variation range
float SimdSynthAudioProcessor::randomize(float base, float var) {
    float r = static_cast<float>(rand()) / RAND_MAX;
    return base * (1.0f - var + r * 2.0f * var);
}

#ifdef __x86_64__
// Compute SIMD sine approximation using Taylor series with x^9 term for improved accuracy
__m128 SimdSynthAudioProcessor::fast_sin_ps(__m128 x) {
    const __m128 twoPi = _mm_set1_ps(2.0f * M_PI);
    const __m128 invTwoPi = _mm_set1_ps(1.0f / (2.0f * M_PI));
    const __m128 piOverTwo = _mm_set1_ps(M_PI / 2.0f);
    const __m128 c3 = _mm_set1_ps(-1.0f / 6.0f);
    const __m128 c5 = _mm_set1_ps(1.0f / 120.0f);
    const __m128 c7 = _mm_set1_ps(-1.0f / 5040.0f);
    const __m128 c9 = _mm_set1_ps(1.0f / 362880.0f);
    __m128 q = _mm_mul_ps(x, invTwoPi);
    q = SIMD_FLOOR(q);
    __m128 xWrapped = _mm_sub_ps(x, _mm_mul_ps(q, twoPi));

    __m128 sign = _mm_set1_ps(1.0f);
    __m128 absX = _mm_max_ps(xWrapped, _mm_sub_ps(_mm_setzero_ps(), xWrapped));
    __m128 gtPiOverTwo = _mm_cmpgt_ps(absX, piOverTwo);
    sign = _mm_or_ps(_mm_and_ps(gtPiOverTwo, _mm_set1_ps(-1.0f)), _mm_andnot_ps(gtPiOverTwo, _mm_set1_ps(1.0f)));
    xWrapped = _mm_sub_ps(xWrapped, _mm_and_ps(gtPiOverTwo, _mm_mul_ps(piOverTwo, _mm_set1_ps(2.0f))));

    __m128 x2 = _mm_mul_ps(xWrapped, xWrapped);
    __m128 x3 = _mm_mul_ps(x2, xWrapped);
    __m128 x5 = _mm_mul_ps(x3, x2);
    __m128 x7 = _mm_mul_ps(x5, x2);
    __m128 x9 = _mm_mul_ps(x7, x2);

    __m128 result = xWrapped;
    result = _mm_add_ps(result, _mm_mul_ps(c3, x3));
    result = _mm_add_ps(result, _mm_mul_ps(c5, x5));
    result = _mm_add_ps(result, _mm_mul_ps(c7, x7));
    result = _mm_add_ps(result, _mm_mul_ps(c9, x9));

    result = _mm_mul_ps(result, sign);
    result = _mm_max_ps(_mm_set1_ps(-1.0f), _mm_min_ps(result, _mm_set1_ps(1.0f)));

    return result;
}
#endif

#if defined(__aarch64__) || defined(__arm64__)
// Compute SIMD sine approximation using Taylor series with x^9 term for improved accuracy
float32x4_t SimdSynthAudioProcessor::fast_sin_ps(float32x4_t x) {
    const float32x4_t twoPi = vdupq_n_f32(2.0f * M_PI);
    const float32x4_t invTwoPi = vdupq_n_f32(1.0f / (2.0f * M_PI));
    const float32x4_t piOverTwo = vdupq_n_f32(M_PI / 2.0f);
    const float32x4_t c3 = vdupq_n_f32(-1.0f / 6.0f);
    const float32x4_t c5 = vdupq_n_f32(1.0f / 120.0f);
    const float32x4_t c7 = vdupq_n_f32(-1.0f / 5040.0f);
    const float32x4_t c9 = vdupq_n_f32(1.0f / 362880.0f);
    float32x4_t q = vmulq_f32(x, invTwoPi);
    q = SIMD_FLOOR(q);
    float32x4_t xWrapped = vsubq_f32(x, vmulq_f32(q, twoPi));

    float32x4_t sign = vdupq_n_f32(1.0f);
    float32x4_t absX = vmaxq_f32(xWrapped, vsubq_f32(vdupq_n_f32(0.0f), xWrapped));
    uint32x4_t gtPiOverTwo = vcgtq_f32(absX, piOverTwo);
    sign = vbslq_f32(gtPiOverTwo, vdupq_n_f32(-1.0f), vdupq_n_f32(1.0f));
    xWrapped = vsubq_f32(xWrapped, vbslq_f32(gtPiOverTwo, vmulq_f32(piOverTwo, vdupq_n_f32(2.0f)), vdupq_n_f32(0.0f)));

    float32x4_t x2 = vmulq_f32(xWrapped, xWrapped);
    float32x4_t x3 = vmulq_f32(x2, xWrapped);
    float32x4_t x5 = vmulq_f32(x3, x2);
    float32x4_t x7 = vmulq_f32(x5, x2);
    float32x4_t x9 = vmulq_f32(x7, x2);

    float32x4_t result = xWrapped;
    result = vaddq_f32(result, vmulq_f32(c3, x3));
    result = vaddq_f32(result, vmulq_f32(c5, x5));
    result = vaddq_f32(result, vmulq_f32(c7, x7));
    result = vaddq_f32(result, vmulq_f32(c9, x9));

    result = vmulq_f32(result, sign);
    result = vmaxq_f32(vdupq_n_f32(-1.0f), vminq_f32(result, vdupq_n_f32(1.0f)));

    return result;
}
#endif // Perform wavetable lookup with vectorized selection for SIMD efficiency

// Perform wavetable lookup with vectorized selection for SIMD efficiency
SIMD_TYPE SimdSynthAudioProcessor::wavetable_lookup_ps(SIMD_TYPE phase, SIMD_TYPE wavetableTypes) {
    // Normalize phase to [0, 1] for wavetable lookup
    SIMD_TYPE phases_norm = SIMD_SUB(phase, SIMD_FLOOR(phase));
    float tempIn[4], tempOut[4], tempTypes[4];
    SIMD_STORE(tempIn, phases_norm);       // Store normalized phases for scalar processing
    SIMD_STORE(tempTypes, wavetableTypes); // Store wavetable types for per-voice selection

    // Process each voice individually to support different wavetable types
    for (int i = 0; i < 4; ++i) {
        int type = static_cast<int>(tempTypes[i]); // Get wavetable type for this voice
        switch (type) {
        case 0: // Sine wave
            tempOut[i] = sineTableTransform(tempIn[i]);
            break;
        case 1: // Saw wave
            tempOut[i] = sawTableTransform(tempIn[i]);
            break;
        case 2: // Square wave
            tempOut[i] = squareTableTransform(tempIn[i]);
            break;
        default:
            DBG("Warning: Invalid wavetable type " << type << " for voice lane " << i << ", defaulting to sine");
            tempOut[i] = sineTableTransform(tempIn[i]); // Fallback to sine for invalid types
            break;
        }
    }

    // Load results into SIMD vector for further processing
    return SIMD_LOAD(tempOut);
}

// Apply a four-stage ladder filter with SIMD optimizations, ensuring stability and per-voice sample rate handling
void SimdSynthAudioProcessor::applyLadderFilter(Voice *voices, int voiceOffset, SIMD_TYPE input, SIMD_TYPE &output) {
    // Compute vectorized cutoff and resonance for the SIMD batch
    float tempCutoffs[4], tempEnvMods[4], tempResonances[4], tempSampleRates[4];
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        tempCutoffs[i] = idx < MAX_VOICE_POLYPHONY ? voices[idx].cutoff : 0.0f;
        tempEnvMods[i] = idx < MAX_VOICE_POLYPHONY ? voices[idx].fegAmount * voices[idx].filterEnv * 12000.0f : 0.0f;
        tempResonances[i] = idx < MAX_VOICE_POLYPHONY ? voices[idx].resonance : 0.0f;
        tempSampleRates[i] = idx < MAX_VOICE_POLYPHONY ? voices[idx].sampleRate : 44100.0f;
    } // Modulate cutoff and clamp to safe range [20 Hz, Nyquist - 1 Hz]
    SIMD_TYPE modulatedCutoffs = SIMD_LOAD(tempCutoffs);
    modulatedCutoffs = SIMD_ADD(modulatedCutoffs, SIMD_LOAD(tempEnvMods));
    modulatedCutoffs =
        SIMD_MAX(SIMD_SET1(20.0f), SIMD_MIN(modulatedCutoffs, SIMD_SUB(SIMD_LOAD(tempSampleRates), SIMD_SET1(1.0f))));

    // Compute filter alpha coefficients per voice
    float tempModulated[4], tempAlphas[4];
    SIMD_STORE(tempModulated, modulatedCutoffs);
    for (int i = 0; i < 4; i++) {
        float sampleRate = tempSampleRates[i];
        tempAlphas[i] = 1.0f - expf(-2.0f * M_PI * tempModulated[i] / sampleRate);
        tempAlphas[i] = std::max(0.0f, std::min(1.0f, tempAlphas[i])); // Prevent NaN or invalid coefficients
    }
    SIMD_TYPE alpha = SIMD_SET(tempAlphas[0], tempAlphas[1], tempAlphas[2], tempAlphas[3]);
    SIMD_TYPE resonance = SIMD_SET(tempResonances[0], tempResonances[1], tempResonances[2], tempResonances[3]);
    resonance = SIMD_MIN(SIMD_MUL(resonance, SIMD_SET1(3.5f)), SIMD_SET1(3.5f)); // Cap resonance for stability

    // Skip processing if no voices in the batch are active
    bool anyActive = false;
    for (int i = 0; i < 4; i++) {
        if (voiceOffset + i < MAX_VOICE_POLYPHONY && voices[voiceOffset + i].active) {
            anyActive = true;
            break;
        }
    }
    if (!anyActive) {
        output = SIMD_SET1(0.0f);
        return;
    }

    // Initialize filter states for active voices
    SIMD_TYPE states[4] = {SIMD_SET1(0.0f), SIMD_SET1(0.0f), SIMD_SET1(0.0f), SIMD_SET1(0.0f)};
    float temp[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 4; i++) {     // For each filter stage
        for (int j = 0; j < 4; j++) { // For each voice in the SIMD batch
            int idx = voiceOffset + j;
            if (idx < MAX_VOICE_POLYPHONY && voices[idx].active) {
#if defined(__aarch64__) || defined(__arm64__)
                temp[j] = vgetq_lane_f32(voices[idx].filterStates[i], 0);
#else
                temp[j] = voices[idx].filterStates[i];
#endif
            }
        }
        states[i] = SIMD_LOAD(temp);
    }

    // Apply filter with tanh saturation and reduced feedback gain
    SIMD_TYPE feedback = SIMD_MUL(states[3], resonance);
    feedback = SIMD_MUL(feedback, SIMD_SET1(0.9f));                          // Reduce feedback gain to prevent clipping
    feedback = juce::dsp::FastMathApproximations::tanh<SIMD_TYPE>(feedback); // Stabilize with tanh
    SIMD_TYPE filterInput = SIMD_SUB(input, feedback);
    states[0] = SIMD_ADD(states[0], SIMD_MUL(alpha, SIMD_SUB(filterInput, states[0])));
    states[1] = SIMD_ADD(states[1], SIMD_MUL(alpha, SIMD_SUB(states[0], states[1])));
    states[2] = SIMD_ADD(states[2], SIMD_MUL(alpha, SIMD_SUB(states[1], states[2])));
    states[3] = SIMD_ADD(states[3], SIMD_MUL(alpha, SIMD_SUB(states[2], states[3])));
    output = states[3];

    // Clamp output to [-1, 1] using SIMD operations
    output = SIMD_MAX(SIMD_SET1(-1.0f), SIMD_MIN(output, SIMD_SET1(1.0f)));

    // Debug NaN or infinite values
    float tempOut[4];
    SIMD_STORE(tempOut, output);
    if (std::isnan(tempOut[0]) || std::isnan(tempOut[1]) || std::isnan(tempOut[2]) || std::isnan(tempOut[3]) ||
        !std::isfinite(tempOut[0]) || !std::isfinite(tempOut[1]) || !std::isfinite(tempOut[2]) ||
        !std::isfinite(tempOut[3])) {
        DBG("Filter output NaN/Inf at voiceOffset " << voiceOffset << ": {" << tempOut[0] << ", " << tempOut[1] << ", "
                                                    << tempOut[2] << ", " << tempOut[3] << "}");
        output = SIMD_SET1(0.0f); // Reset output to zero if invalid
    }

    // Store updated filter states
    for (int i = 0; i < 4; i++) {
        SIMD_STORE(temp, states[i]);
        for (int j = 0; j < 4; j++) {
            int idx = voiceOffset + j;
            if (idx < MAX_VOICE_POLYPHONY) {
#if defined(__aarch64__) || defined(__arm64__)
                voices[idx].filterStates[i] = vsetq_lane_f32(temp[j], voices[idx].filterStates[i], 0);
#else
                voices[idx].filterStates[i] = temp[j];
#endif
            }
        }
    }
} // Find the best voice to steal for a new note, prioritizing released voices or oldest voices
int SimdSynthAudioProcessor::findVoiceToSteal() {
    int bestVoice = 0;
    float maxAge = -1.0f;
    bool foundReleased = false;
    int oldestReleasedVoice = -1;
    float lowestAmplitude = std::numeric_limits<float>::max();
    int lowestAmplitudeVoice = -1; // First, look for a released voice (preferred to minimize audible artifacts)
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        if (voices[i].active && voices[i].released && !voices[i].isHeld) {
            if (!foundReleased || voices[i].voiceAge > maxAge) {
                foundReleased = true;
                oldestReleasedVoice = i;
                maxAge = voices[i].voiceAge;
            }
        }
    }

    // If a released voice is found, use it
    if (foundReleased) {
        // Reset filter states to prevent clicks
        for (int j = 0; j < 4; ++j) {
#if defined(__aarch64__) || defined(__arm64__)
            voices[oldestReleasedVoice].filterStates[j] = vdupq_n_f32(0.0f);
#else
            voices[oldestReleasedVoice].filterStates[j] = 0.0f;
#endif
        }
        return oldestReleasedVoice;
    }

    // If no released voices, look for the voice with the lowest amplitude
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        if (voices[i].active && voices[i].amplitude < lowestAmplitude) {
            lowestAmplitude = voices[i].amplitude;
            lowestAmplitudeVoice = i;
        }
    }

    // If a voice with low amplitude is found, use it
    if (lowestAmplitudeVoice != -1) {
        // Reset filter states to prevent clicks
        for (int j = 0; j < 4; ++j) {
#if defined(__aarch64__) || defined(__arm64__)
            voices[lowestAmplitudeVoice].filterStates[j] = vdupq_n_f32(0.0f);
#else
            voices[lowestAmplitudeVoice].filterStates[j] = 0.0f;
#endif
        }
        return lowestAmplitudeVoice;
    }

    // Fallback: Use the oldest active voice
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        if (voices[i].active && voices[i].voiceAge > maxAge) {
            maxAge = voices[i].voiceAge;
            bestVoice = i;
        }
    }

    // Reset filter states for the chosen voice to prevent clicks
    for (int j = 0; j < 4; ++j) {
#if defined(__aarch64__) || defined(__arm64__)
        voices[bestVoice].filterStates[j] = vdupq_n_f32(0.0f);
#else
        voices[bestVoice].filterStates[j] = 0.0f;
#endif
    }

    return bestVoice;
}

// Update amplitude and filter envelopes with configurable curves
void SimdSynthAudioProcessor::updateEnvelopes(float t) {
    for (int i = 0; i < MAX_VOICE_POLYPHONY; i++) {
        if (!voices[i].active) {
            voices[i].amplitude = 0.0f;
            voices[i].filterEnv = 0.0f;
            for (int j = 0; j < 4; j++) {
#if defined(__aarch64__) || defined(__arm64__)
                voices[i].filterStates[j] = vdupq_n_f32(0.0f);
#else
                voices[i].filterStates[j] = 0.0f;
#endif
            }
            continue;
        }

        float localTime = t - voices[i].noteOnTime;
        DBG("Voice " << i << ": t=" << t << ", noteOnTime=" << voices[i].noteOnTime << ", localTime=" << localTime);

        // Use per-voice envelope parameters with minimum bounds
        float attack = std::max(std::min(voices[i].attack * voices[i].timeScale, 1.0f), 0.001f); // Clamp attack time
        float decay = std::max(voices[i].decay * voices[i].timeScale, 0.001f);
        float sustain = std::max(voices[i].sustain, 0.5f); // Ensure non-zero sustain
        float release = std::max(voices[i].release * voices[i].timeScale, 0.001f);
        float releaseFactor = release;

        const float attackCurve = voices[i].attackCurve; // e.g., 2.0f for convex curve
        const float decayCurve = 1.5f;
        const float releaseCurve = voices[i].releaseCurve; // e.g., 3.0f

        // Amplitude envelope (ADSR)
        if (localTime < 0.0f) {
            voices[i].amplitude = 0.0f;
            DBG("Voice " << i << ": localTime < 0, amplitude=0");
        } else if (localTime < attack) {
            float attackPhase = localTime / attack;
            voices[i].amplitude = std::pow(attackPhase, attackCurve);
            DBG("Voice " << i << ": attackPhase=" << attackPhase << ", amplitude=" << voices[i].amplitude);
        } else if (localTime < attack + decay) {
            float decayPhase = (localTime - attack) / decay;
            float decayExp = std::pow(decayPhase, decayCurve);
            voices[i].amplitude = 1.0f - (decayExp * (1.0f - sustain));
        } else if (!voices[i].released) {
            voices[i].amplitude = sustain;
        } else {
            float releaseTime = t - voices[i].noteOffTime;
            float releasePhase = releaseTime / releaseFactor;
            float releaseExp = std::exp(-releasePhase * releaseCurve);
            voices[i].amplitude = voices[i].releaseStartAmplitude * releaseExp;

            // Increase threshold to allow longer release tails
            if (voices[i].amplitude <= 0.01f) { // Changed from 0.001f to 0.01f
                voices[i].amplitude = 0.0f;
                voices[i].active = false;
                voices[i].filterEnv = 0.0f;
                for (int j = 0; j < 4; j++) {
#if defined(__aarch64__) || defined(__arm64__)
                    voices[i].filterStates[j] = vdupq_n_f32(0.0f);
#else
                    voices[i].filterStates[j] = 0.0f;
#endif
                }
                DBG("Voice " << i << ": deactivated, amplitude=" << voices[i].amplitude);
            }
        }

        voices[i].amplitude = std::max(0.0f, std::min(1.0f, voices[i].amplitude));

        // Filter envelope (ADSR)
        float fegAttack = std::max(std::min(voices[i].fegAttack * voices[i].timeScale, 1.0f), 0.001f);
        float fegDecay = std::max(voices[i].fegDecay * voices[i].timeScale, 0.001f);
        float fegSustain = std::max(voices[i].fegSustain, 0.5f);
        float fegRelease = std::max(voices[i].fegRelease * voices[i].timeScale, 0.001f);
        float fegReleaseFactor = fegRelease;

        if (localTime < 0.0f) {
            voices[i].filterEnv = 0.0f;
        } else if (localTime < fegAttack) {
            float attackPhase = localTime / fegAttack;
            voices[i].filterEnv = std::pow(attackPhase, attackCurve);
        } else if (localTime < fegAttack + fegDecay) {
            float decayPhase = (localTime - fegAttack) / fegDecay;
            float decayExp = std::pow(decayPhase, decayCurve);
            voices[i].filterEnv = 1.0f - (decayExp * (1.0f - fegSustain));
        } else if (!voices[i].released) {
            voices[i].filterEnv = fegSustain;
        } else {
            float releaseTime = t - voices[i].noteOffTime;
            float releasePhase = releaseTime / fegReleaseFactor;
            float releaseExp = std::exp(-releasePhase * releaseCurve);
            voices[i].filterEnv = voices[i].releaseStartFilterEnv * releaseExp;
        }

        voices[i].filterEnv = std::max(0.0f, std::min(1.0f, voices[i].filterEnv));

        if (voices[i].active) {
            DBG("Voice " << i << ": amplitude=" << voices[i].amplitude << ", filterEnv=" << voices[i].filterEnv
                         << ", released=" << std::to_string(voices[i].released)
                         << ", noteOnTime=" << voices[i].noteOnTime << ", localTime=" << localTime);
        }
    }
}

// Prepare audio processing with oversampling
void SimdSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentTime = 0.0;
    int newOversamplingFactor = (samplesPerBlock < 256) ? 1 : 2;

    if (oversampling->getOversamplingFactor() != newOversamplingFactor) {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
            2, newOversamplingFactor, juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR, true,
            true);
        oversampling->initProcessing(samplesPerBlock);
    }

    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i].active = false;
        voices[i].released = false;
        voices[i].isHeld = false;
        voices[i].amplitude = 0.0f;
        voices[i].velocity = 1.0f; // Default velocity to avoid muting
        voices[i].noteOnTime = 0.0f;
        voices[i].noteOffTime = 0.0f;
        voices[i].sampleRate = static_cast<float>(sampleRate);
        voices[i].sustain = std::max(sustainLevelParam->load(), 0.5f); // Ensure non-zero sustain
        voices[i].subMix = juce::jlimit(0.0f, 0.7f, subMixParam->load()); // Ensure valid subMix
        for (int j = 0; j < 4; ++j) {
#if defined(__aarch64__) || defined(__arm64__)
            voices[i].filterStates[j] = vdupq_n_f32(0.0f);
#else
            voices[i].filterStates[j] = 0.0f;
#endif
        }
    }
} // Release resources
void SimdSynthAudioProcessor::releaseResources() { oversampling->reset(); }

void SimdSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const SIMD_TYPE twoPi = SIMD_SET1(2.0f * M_PI);
    constexpr int NUM_BATCHES = (MAX_VOICE_POLYPHONY + SIMD_WIDTH - 1) / SIMD_WIDTH;

    buffer.clear();

    // Log parameters
    DBG("processBlock parameters: gain=" << *gainParam << ", sustain=" << *sustainLevelParam
        << ", subMix=" << *subMixParam << ", wavetable=" << *wavetableTypeParam);

    // Setup oversampling and sample rate
    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(block);
    float oversamplingFactor = oversampling->getOversamplingFactor();
    float sampleRate = voices[0].sampleRate * oversamplingFactor;
    double blockStartTime = currentTime;

    // Update sample rate for all voices
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i].sampleRate = sampleRate;
        if (voices[i].active) {
            voices[i].phaseIncrement = voices[i].frequency / sampleRate;
            float baseSubFreq = voices[i].frequency * powf(2.0f, voices[i].subTune / 12.0f);
            float fixedFreq = midiToFreq(69);
            float subFreq = baseSubFreq * voices[i].subTrack + fixedFreq * (1.0f - voices[i].subTrack);
            voices[i].subPhaseIncrement = subFreq / sampleRate;
        }
    }

    // Update parameters for all voices
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
        voices[i].attack = *attackTimeParam;
        voices[i].decay = *decayTimeParam;
        voices[i].sustain = std::max(sustainLevelParam->load(), 0.5f);
        voices[i].release = *releaseTimeParam;
        voices[i].cutoff = *cutoffParam;
        voices[i].resonance = *resonanceParam;
        voices[i].fegAttack = *fegAttackParam;
        voices[i].fegDecay = *fegDecayParam;
        voices[i].fegSustain = *fegSustainParam;
        voices[i].fegRelease = *fegReleaseParam;
        voices[i].fegAmount = *fegAmountParam;
        voices[i].lfoRate = *lfoRateParam;
        voices[i].lfoDepth = *lfoDepthParam;
        voices[i].subTune = *subTuneParam;
        voices[i].subMix = juce::jlimit(0.0f, 0.7f, subMixParam->load());
        voices[i].subTrack = *subTrackParam;
        voices[i].unison = static_cast<int>(*unisonParam);
        voices[i].detune = *detuneParam;
    }

    // Prepare oversampled MIDI events
    juce::MidiBuffer oversampledMidi;
    for (const auto metadata : midiMessages) {
        oversampledMidi.addEvent(metadata.getMessage(), metadata.samplePosition * oversamplingFactor);
        auto msg = metadata.getMessage();
        DBG("MIDI: type=" << (msg.isNoteOn() ? "NoteOn" : msg.isNoteOff() ? "NoteOff" : "Other")
            << ", note=" << msg.getNoteNumber() << ", velocity=" << msg.getVelocity()
            << ", sample=" << metadata.samplePosition);
    }

    // Process MIDI events
    for (const auto metadata : oversampledMidi) {
        auto msg = metadata.getMessage();
        int samplePosition = metadata.samplePosition;
        float t = blockStartTime + samplePosition / sampleRate; // Use oversampled sampleRate
        if (msg.isNoteOn()) {
            int noteNumber = msg.getNoteNumber();
            float velocity = msg.getVelocity() / 127.0f;
            if (velocity == 0.0f) velocity = 1.0f; // Fallback for zero velocity
            bool voiceAssigned = false;

            for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
                if (!voices[i].active) {
                    voices[i].active = true;
                    voices[i].released = false;
                    voices[i].isHeld = true;
                    voices[i].frequency = midiToFreq(noteNumber);
                    voices[i].sampleRate = sampleRate; // Set sampleRate before calculating increments
                    voices[i].phase = 0.0f;
                    voices[i].subPhase = 0.0f;
                    voices[i].lfoPhase = 0.0f;
                    voices[i].amplitude = 0.0f;
                    voices[i].filterEnv = 0.0f;
                    voices[i].noteNumber = noteNumber;
                    voices[i].velocity = velocity;
                    voices[i].noteOnTime = t;
                    voices[i].noteOffTime = 0.0f;
                    voices[i].voiceAge = 0.0f;
                    voices[i].releaseStartAmplitude = 0.0f;
                    voices[i].releaseStartFilterEnv = 0.0f;
                    voices[i].phaseIncrement = voices[i].frequency / sampleRate;
                    float baseSubFreq = voices[i].frequency * powf(2.0f, voices[i].subTune / 12.0f);
                    float fixedFreq = midiToFreq(69);
                    float subFreq = baseSubFreq * voices[i].subTrack + fixedFreq * (1.0f - voices[i].subTrack);
                    voices[i].subPhaseIncrement = subFreq / sampleRate;
                    for (int j = 0; j < 4; ++j) {
#if defined(__aarch64__) || defined(__arm64__)
                        voices[i].filterStates[j] = vdupq_n_f32(0.0f);
#else
                        voices[i].filterStates[j] = 0.0f;
#endif
                    }
                    voiceAssigned = true;
                    DBG("Assigned voice " << i << " for note " << noteNumber << " at time " << t
                        << ", velocity=" << velocity << ", phaseIncrement=" << voices[i].phaseIncrement
                        << ", subPhaseIncrement=" << voices[i].subPhaseIncrement);
                    break;
                }
            }

            if (!voiceAssigned) {
                int voiceToSteal = findVoiceToSteal();
                voices[voiceToSteal].active = true;
                voices[voiceToSteal].released = false;
                voices[voiceToSteal].isHeld = true;
                voices[voiceToSteal].frequency = midiToFreq(noteNumber);
                voices[voiceToSteal].sampleRate = sampleRate; // Set sampleRate before calculating increments
                voices[voiceToSteal].phase = 0.0f;
                voices[voiceToSteal].subPhase = 0.0f;
                voices[voiceToSteal].lfoPhase = 0.0f;
                voices[voiceToSteal].amplitude = 0.0f;
                voices[voiceToSteal].filterEnv = 0.0f;
                voices[voiceToSteal].noteNumber = noteNumber;
                voices[voiceToSteal].velocity = velocity;
                voices[voiceToSteal].noteOnTime = t;
                voices[voiceToSteal].noteOffTime = 0.0f;
                voices[voiceToSteal].voiceAge = 0.0f;
                voices[voiceToSteal].releaseStartAmplitude = 0.0f;
                voices[voiceToSteal].releaseStartFilterEnv = 0.0f;
                voices[voiceToSteal].phaseIncrement = voices[voiceToSteal].frequency / sampleRate;
                float baseSubFreq = voices[voiceToSteal].frequency * powf(2.0f, voices[voiceToSteal].subTune / 12.0f);
                float fixedFreq = midiToFreq(69);
                float subFreq = baseSubFreq * voices[voiceToSteal].subTrack + fixedFreq * (1.0f - voices[voiceToSteal].subTrack);
                voices[voiceToSteal].subPhaseIncrement = subFreq / sampleRate;
                for (int j = 0; j < 4; ++j) {
#if defined(__aarch64__) || defined(__arm64__)
                    voices[voiceToSteal].filterStates[j] = vdupq_n_f32(0.0f);
#else
                    voices[voiceToSteal].filterStates[j] = 0.0f;
#endif
                }
                DBG("Stole voice " << voiceToSteal << " for note " << noteNumber << " at time " << t
                    << ", velocity=" << velocity << ", phaseIncrement=" << voices[voiceToSteal].phaseIncrement
                    << ", subPhaseIncrement=" << voices[voiceToSteal].subPhaseIncrement);
            }
        } else if (msg.isNoteOff()) {
            int noteNumber = msg.getNoteNumber();
            for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
                if (voices[i].active && voices[i].noteNumber == noteNumber && voices[i].isHeld) {
                    voices[i].released = true;
                    voices[i].isHeld = false;
                    voices[i].noteOffTime = t;
                    voices[i].releaseStartAmplitude = voices[i].amplitude;
                    voices[i].releaseStartFilterEnv = voices[i].filterEnv;
                    DBG("Note off for voice " << i << ", note " << noteNumber
                        << ", releaseStartAmplitude=" << voices[i].releaseStartAmplitude
                        << ", releaseStartFilterEnv=" << voices[i].releaseStartFilterEnv);
                }
            }
        }
    }

    auto processSample = [&](int samplePosition, float t) {
        updateEnvelopes(t);
        float outputSample = 0.0f;

        for (int batch = 0; batch < NUM_BATCHES; batch++) {
            const int voiceOffset = batch * SIMD_WIDTH;
            if (voiceOffset >= MAX_VOICE_POLYPHONY) continue;

            bool anyActive = false;
            for (int j = 0; j < SIMD_WIDTH; ++j) {
                int idx = voiceOffset + j;
                if (idx < MAX_VOICE_POLYPHONY && voices[idx].active) {
                    anyActive = true;
                    DBG("Voice " << idx << " active: amplitude=" << voices[idx].amplitude
                        << ", velocity=" << voices[idx].velocity << ", note=" << voices[idx].noteNumber);
                    break;
                }
            }
            if (!anyActive) continue;

            for (int j = 0; j < SIMD_WIDTH; ++j) {
                tempAmps[j] = 0.0f;
                tempPhases[j] = 0.0f;
                tempIncrements[j] = 0.0f;
                tempLfoPhases[j] = 0.0f;
                tempLfoRates[j] = 0.0f;
                tempLfoDepths[j] = 0.0f;
                tempSubPhases[j] = 0.0f;
                tempSubIncrements[j] = 0.0f;
                tempSubMixes[j] = 0.0f;
                tempWavetableTypes[j] = 0.0f;
                tempSubTracks[j] = 0.0f;
                tempUnisonCounts[j] = 0.0f;
                tempDetunes[j] = 0.0f;
                tempSampleRates[j] = 0.0f;
                tempUnisonOutput[j] = 0.0f;
                tempVoiceOutput[j] = 0.0f;
                tempFilterOutput[j] = 0.0f;
            }

            for (int j = 0; j < SIMD_WIDTH; ++j) {
                int idx = voiceOffset + j;
                if (idx >= MAX_VOICE_POLYPHONY || !voices[idx].active) continue;
                tempAmps[j] = voices[idx].amplitude * voices[idx].velocity;
                tempPhases[j] = voices[idx].phase;
                tempIncrements[j] = voices[idx].phaseIncrement;
                tempLfoPhases[j] = voices[idx].lfoPhase;
                tempLfoRates[j] = voices[idx].lfoRate;
                tempLfoDepths[j] = voices[idx].lfoDepth;
                tempSubPhases[j] = voices[idx].subPhase;
                tempSubIncrements[j] = voices[idx].subPhaseIncrement;
                tempSubMixes[j] = voices[idx].subMix; // Fixed typo: was voices[i]
                tempWavetableTypes[j] = static_cast<float>(voices[idx].wavetableType);
                tempSubTracks[j] = voices[idx].subTrack;
                tempUnisonCounts[j] = static_cast<float>(voices[idx].unison);
                tempDetunes[j] = voices[idx].detune;
                tempSampleRates[j] = voices[idx].sampleRate;
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
            SIMD_TYPE wavetableTypes = SIMD_LOAD(tempWavetableTypes);
            SIMD_TYPE subTracks = SIMD_LOAD(tempSubTracks);
            SIMD_TYPE unisonCounts = SIMD_LOAD(tempUnisonCounts);
            SIMD_TYPE detunes = SIMD_LOAD(tempDetunes);
            SIMD_TYPE sampleRates = SIMD_LOAD(tempSampleRates);

            SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRates, SIMD_DIV(SIMD_SET1(2.0f * M_PI), sampleRates));
            lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
            lfoPhases = SIMD_SUB(lfoPhases, SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)));
            lfoPhases = SIMD_MAX(lfoPhases, SIMD_SET1(0.0f));
            lfoPhases = SIMD_MIN(lfoPhases, twoPi);
            SIMD_TYPE lfoValues = fast_sin_ps(lfoPhases);
            lfoValues = SIMD_MUL(lfoValues, lfoDepths);
            SIMD_TYPE phaseMod = SIMD_DIV(lfoValues, twoPi);

            SIMD_TYPE mainValues = wavetable_lookup_ps(SIMD_ADD(phases, phaseMod), wavetableTypes);
            float tempMainValues[SIMD_WIDTH];
            SIMD_STORE(tempMainValues, mainValues);
            DBG("Main values: " << tempMainValues[0] << ", " << tempMainValues[1] << ", "
                                << tempMainValues[2] << ", " << tempMainValues[3]);

            SIMD_TYPE unisonOutput = SIMD_SET1(0.0f);
            for (int j = 0; j < SIMD_WIDTH; ++j) {
                int idx = voiceOffset + j;
                if (idx >= MAX_VOICE_POLYPHONY || !voices[idx].active) continue;
                int unison = static_cast<int>(tempUnisonCounts[j]);
                float detune = tempDetunes[j];
                float normFactor = unison > 0 ? 1.0f / unison : 1.0f;
                SIMD_TYPE voiceOutput = SIMD_SET1(0.0f);
                for (int u = 0; u < unison; ++u) {
                    float detuneAmount = detune * (u - (unison - 1) / 2.0f) / (unison - 1 + 0.0001f);
                    float detuneFactor = powf(2.0f, detuneAmount / 12.0f);
                    SIMD_TYPE detunedPhases = SIMD_MUL(SIMD_ADD(SIMD_SET1(tempPhases[j]), phaseMod), SIMD_SET1(detuneFactor));
                    detunedPhases = SIMD_SUB(detunedPhases, SIMD_FLOOR(SIMD_DIV(detunedPhases, twoPi)));
                    SIMD_TYPE detunedValues = wavetable_lookup_ps(detunedPhases, SIMD_SET1(tempWavetableTypes[j]));
                    voiceOutput = SIMD_ADD(voiceOutput, SIMD_MUL(detunedValues, SIMD_SET1(normFactor)));
                }
                SIMD_STORE(tempVoiceOutput, voiceOutput);
                tempUnisonOutput[j] = tempVoiceOutput[j];
            }
            unisonOutput = SIMD_LOAD(tempUnisonOutput);
            unisonOutput = SIMD_MUL(unisonOutput, amplitudes);
            unisonOutput = SIMD_MUL(unisonOutput, SIMD_SUB(SIMD_SET1(1.0f), subMixes));
            float tempUnisonValues[SIMD_WIDTH];
            SIMD_STORE(tempUnisonValues, unisonOutput);
            DBG("Unison output: " << tempUnisonValues[0] << ", " << tempUnisonValues[1] << ", "
                                  << tempUnisonValues[2] << ", " << tempUnisonValues[3]);

            SIMD_TYPE subSinValues = fast_sin_ps(subPhases);
            subSinValues = SIMD_MUL(subSinValues, amplitudes);
            subSinValues = SIMD_MUL(subSinValues, subMixes);
            float tempSubValues[SIMD_WIDTH];
            SIMD_STORE(tempSubValues, subSinValues);
            DBG("Sub values: " << tempSubValues[0] << ", " << tempSubValues[1] << ", "
                               << tempSubValues[2] << ", " << tempSubValues[3]);

            SIMD_TYPE combinedValues = SIMD_ADD(unisonOutput, subSinValues);
            float tempCombinedValues[SIMD_WIDTH];
            SIMD_STORE(tempCombinedValues, combinedValues);
            DBG("Combined values: " << tempCombinedValues[0] << ", " << tempCombinedValues[1] << ", "
                                    << tempCombinedValues[2] << ", " << tempCombinedValues[3]);

            // Fallback: Ensure some output if amplitudes are zero
            bool allAmplitudesZero = true;
            for (int j = 0; j < SIMD_WIDTH; ++j) {
                int idx = voiceOffset + j;
                if (idx < MAX_VOICE_POLYPHONY && voices[idx].active && tempAmps[j] > 0.0f) {
                    allAmplitudesZero = false;
                    break;
                }
            }
            if (allAmplitudesZero) {
                combinedValues = SIMD_SET1(0.0f); // Mute the batch if all amplitudes are zero
                DBG("Warning: Zero amplitudes detected in batch " << batch << ", muting batch");
            }

            SIMD_TYPE filteredOutput;
            applyLadderFilter(voices, voiceOffset, combinedValues, filteredOutput);
            float tempFilteredValues[SIMD_WIDTH];
            SIMD_STORE(tempFilteredValues, filteredOutput);
            DBG("Filtered output: " << tempFilteredValues[0] << ", " << tempFilteredValues[1] << ", "
                                    << tempFilteredValues[2] << ", " << tempFilteredValues[3]);

            SIMD_STORE(tempFilterOutput, filteredOutput);
            for (int j = 0; j < SIMD_WIDTH; ++j) {
                int idx = voiceOffset + j;
                if (idx < MAX_VOICE_POLYPHONY && voices[idx].active && tempAmps[j] > 0.0f) {
                    outputSample += tempFilterOutput[j] * voices[idx].velocity; // Include velocity scaling
                    DBG("Voice " << idx << ": amp=" << tempAmps[j] << ", unison=" << tempUnisonOutput[j]
                        << ", sub=" << tempSubValues[j] << ", filter=" << tempFilteredValues[j]
                        << " at sample " << samplePosition);
                } else {
                    tempFilterOutput[j] = 0.0f; // Explicitly zero out inactive voices
                }
            }

            phases = SIMD_ADD(phases, increments);
            phases = SIMD_SUB(phases, SIMD_FLOOR(SIMD_DIV(phases, twoPi)));
            SIMD_STORE(tempPhases, phases);
            for (int j = 0; j < SIMD_WIDTH; ++j) {
                int idx = voiceOffset + j;
                if (idx < MAX_VOICE_POLYPHONY) {
                    voices[idx].phase = tempPhases[j];
                }
            }

            subPhases = SIMD_ADD(subPhases, subIncrements);
            subPhases = SIMD_SUB(subPhases, SIMD_FLOOR(SIMD_DIV(subPhases, twoPi)));
            SIMD_STORE(tempSubPhases, subPhases);
            for (int j = 0; j < SIMD_WIDTH; ++j) {
                int idx = voiceOffset + j;
                if (idx < MAX_VOICE_POLYPHONY) {
                    voices[idx].subPhase = tempSubPhases[j];
                }
            }

            lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
            lfoPhases = SIMD_SUB(lfoPhases, SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)));
            SIMD_STORE(tempLfoPhases, lfoPhases);
            for (int j = 0; j < SIMD_WIDTH; ++j) {
                int idx = voiceOffset + j;
                if (idx < MAX_VOICE_POLYPHONY) {
                    voices[idx].lfoPhase = tempLfoPhases[j];
                }
            }
        }

        outputSample *= std::max(gainParam->load(), 0.5f);
        DBG("Final outputSample before clamp: " << outputSample);
        if (std::isnan(outputSample) || !std::isfinite(outputSample)) {
            DBG("Invalid output sample at position " << samplePosition << ": " << outputSample);
            outputSample = 0.0f;
        }
        outputSample = std::max(-1.0f, std::min(1.0f, outputSample));
        DBG("Final outputSample after clamp: " << outputSample);

        for (int channel = 0; channel < totalNumOutputChannels; ++channel) {
            oversampledBlock.getChannelPointer(channel)[samplePosition] = outputSample;
        }

        for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
            if (voices[i].active) {
                voices[i].voiceAge = t - voices[i].noteOnTime;
            }
        }
    };

    for (int sampleIndex = 0; sampleIndex < oversampledBlock.getNumSamples(); ++sampleIndex) {
        float t = blockStartTime + sampleIndex / sampleRate;
        processSample(sampleIndex, t);
    }

    oversampling->processSamplesDown(block);
    currentTime = blockStartTime + oversampledBlock.getNumSamples() / sampleRate;
}

// Save plugin state
void SimdSynthAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
    auto state = parameters.copyState();
    state.setProperty("currentProgram", currentProgram, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

// Restore plugin state
void SimdSynthAudioProcessor::setStateInformation(const void *data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr) {
        juce::ValueTree state = juce::ValueTree::fromXml(*xmlState);
        parameters.replaceState(state);
        if (state.hasProperty("currentProgram")) {
            setCurrentProgram(state.getProperty("currentProgram"));
        }
    }
}

// Create the editor for the plugin
juce::AudioProcessorEditor *SimdSynthAudioProcessor::createEditor() { return new SimdSynthAudioProcessorEditor(*this); }

// Create plugin instance
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new SimdSynthAudioProcessor(); }