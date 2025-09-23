/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 8 voices.
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
      parameters(
          *this, nullptr, juce::Identifier("SimdSynth"),
          {
              std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"wavetable", parameterVersion},
                                                          "Wavetable Type", 0.0f, 2.0f, 0.0f), // Sine, saw, square
              std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"attack", parameterVersion}, "Attack Time",
                                                          0.01f, 5.0f, 0.1f),
              std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"decay", parameterVersion}, "Decay Time",
                                                          0.1f, 5.0f, 0.5f),
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
              std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lfoRate", parameterVersion}, "LFO Rate",
                                                          0.0f, 20.0f, 1.0f),
              std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lfoDepth", parameterVersion}, "LFO Depth",
                                                          0.0f, 0.5f, 0.05f),
              std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subTune", parameterVersion},
                                                          "Sub Osc Tune", -24.0f, 24.0f, -12.0f),
              std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subMix", parameterVersion}, "Sub Osc Mix",
                                                          0.0f, 1.0f, 0.5f),
              std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subTrack", parameterVersion},
                                                          "Sub Osc Track", 0.0f, 1.0f, 1.0f),
              std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain", parameterVersion}, "Output Gain",
                                                          0.0f, 2.0f, 1.0f),
              std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"unison", parameterVersion},
                                                          "Unison Voices", 1.0f, 8.0f, 1.0f),
              std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"detune", parameterVersion},
                                                          "Unison Detune", 0.0f, 0.1f, 0.01f)
          }),
      currentTime(0.0),
      oversampling(std::make_unique<juce::dsp::Oversampling<float>>(
          2, 1, juce::dsp::Oversampling<float>::FilterType::filterHalfBandPolyphaseIIR, true, true))
 {
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

    // Initialize voices with default parameter values and released state
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
    }

    // Set initial filter resonance
    filter.resonance = *resonanceParam;

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

// Destructor: Clean up oversampling
SimdSynthAudioProcessor::~SimdSynthAudioProcessor() { oversampling.reset(); }

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

// Load a preset by index
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

    // Extended default parameter values
    std::map<juce::String, float> defaultValues = {
        {"wavetable", 0.0f},  {"attack", 0.1f},    {"decay", 0.5f},     {"sustain", 0.8f},   {"release", 0.2f},
        {"cutoff", 1000.0f},  {"resonance", 0.7f}, {"fegAttack", 0.1f}, {"fegDecay", 1.0f},  {"fegSustain", 0.5f},
        {"fegRelease", 0.2f}, {"fegAmount", 0.5f}, {"lfoRate", 1.0f},   {"lfoDepth", 0.05f}, {"subTune", -12.0f},
        {"subMix", 0.5f},     {"subTrack", 1.0f},  {"gain", 1.0f},      {"unison", 1.0f},    {"detune", 0.01f}};

    juce::StringArray paramIds = {"wavetable",  "attack",    "decay",     "sustain",  "release",
                                  "cutoff",     "resonance", "fegAttack", "fegDecay", "fegSustain",
                                  "fegRelease", "fegAmount", "lfoRate",   "lfoDepth", "subTune",
                                  "subMix",     "subTrack",  "gain",      "unison",   "detune"};

    bool anyParamUpdated = false;
    for (const auto &paramId : paramIds) {
        if (auto *param = parameters.getParameter(paramId)) {
            if (auto *floatParam = dynamic_cast<juce::AudioParameterFloat *>(param)) {
                float value = synthParams.hasProperty(paramId)
                                  ? static_cast<float>(synthParams.getProperty(paramId, defaultValues[paramId]))
                                  : defaultValues[paramId];
                if (paramId == "wavetable" || paramId == "unison") {
                    value = std::round(value); // Quantize for discrete parameters
                }
                value = juce::jlimit(floatParam->getNormalisableRange().start, floatParam->getNormalisableRange().end,
                                     value);
                floatParam->setValueNotifyingHost(floatParam->convertTo0to1(value));
                DBG("Setting " << paramId << " to " << value);
                anyParamUpdated = true;
            }
        }
    }

    if (!anyParamUpdated) {
        DBG("Warning: No parameters updated for preset: " << presetNames[index]);
    }

    // Update voice parameters
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
        voices[i].attack = *attackTimeParam;
        voices[i].decay = *decayTimeParam;
        voices[i].sustain = *sustainLevelParam;
        voices[i].release = *releaseTimeParam;
        voices[i].cutoff = *cutoffParam;
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
    }
    filter.resonance = *resonanceParam;

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
            presetNames.set(index, newName); // Use set() to avoid const issues
        }
    }
}

// Initialize wavetables (added square wave)
void SimdSynthAudioProcessor::initWavetables() {
    for (int i = 0; i < WAVETABLE_SIZE; ++i) {
        float t = static_cast<float>(i) / WAVETABLE_SIZE;
        sineTable[i] = sinf(2.0f * M_PI * t);     // Sine wave: [-1, 1]
        sawTable[i] = 2.0f * t - 1.0f;            // Sawtooth: [-1, 1]
        squareTable[i] = t < 0.5f ? 1.0f : -1.0f; // Square wave: [-1, 1]
    }
}

// Convert MIDI note number to frequency
float SimdSynthAudioProcessor::midiToFreq(int midiNote) { return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f); }

// Randomize a value within a variation range
float SimdSynthAudioProcessor::randomize(float base, float var) {
    float r = static_cast<float>(rand()) / RAND_MAX;
    return base * (1.0f - var + r * 2.0f * var);
}

// SIMD sine approximation for x86_64
#ifdef __x86_64__
__m128 SimdSynthAudioProcessor::fast_sin_ps(__m128 x) {
    const __m128 twoPi = _mm_set1_ps(2.0f * M_PI);
    const __m128 invTwoPi = _mm_set1_ps(1.0f / (2.0f * M_PI));
    __m128 q = _mm_mul_ps(x, invTwoPi);
    q = SIMD_FLOOR(q); // Use SIMD_FLOOR (maps to _mm_floor_ps)
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
    const float32x4_t twoPi = vdupq_n_f32(2.0f * M_PI);
    const float32x4_t invTwoPi = vdupq_n_f32(1.0f / (2.0f * M_PI));
    const float32x4_t piOverTwo = vdupq_n_f32(M_PI / 2.0f);
    float32x4_t q = vmulq_f32(x, invTwoPi);
    q = SIMD_FLOOR(q); // Use SIMD_FLOOR (maps to my_floorq_f32)
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

// Perform wavetable lookup using JUCE's LookupTableTransform for SIMD-friendly interpolation
SIMD_TYPE SimdSynthAudioProcessor::wavetable_lookup_ps(SIMD_TYPE phase, int wavetableType) {
    float tempIn[4], tempOut[4];
    SIMD_STORE(tempIn, phase);
    for (int i = 0; i < 4; ++i) {
        // Ensure phase is in [0, 1]
        tempIn[i] = tempIn[i] - std::floor(tempIn[i]);
        switch (wavetableType) {
            case 0:
                tempOut[i] = sineTableTransform(tempIn[i]);
                break;
            case 1:
                tempOut[i] = sawTableTransform(tempIn[i]);
                break;
            case 2:
                tempOut[i] = squareTableTransform(tempIn[i]);
                break;
            default:
                tempOut[i] = 0.0f;
                break;
        }
    }
    return SIMD_LOAD(tempOut);
}

// Apply ladder filter with vectorized cutoff computation
void SimdSynthAudioProcessor::applyLadderFilter(Voice *voices, int voiceOffset, SIMD_TYPE input, Filter &filter,
                                                SIMD_TYPE &output) {
    // Vectorized cutoff computation
    float tempCutoffs[4], tempEnvMods[4];
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        tempCutoffs[i] = idx < MAX_VOICE_POLYPHONY ? voices[idx].cutoff : 0.0f;
        tempEnvMods[i] = idx < MAX_VOICE_POLYPHONY ? voices[idx].fegAmount * voices[idx].filterEnv * 12000.0f : 0.0f;
    }
    SIMD_TYPE modulatedCutoffs = SIMD_ADD(SIMD_LOAD(tempCutoffs), SIMD_LOAD(tempEnvMods));
    modulatedCutoffs = SIMD_MAX(SIMD_SET1(20.0f), SIMD_MIN(modulatedCutoffs, SIMD_SET1(filter.sampleRate / 2.0f)));
    float tempModulated[4];
    SIMD_STORE(tempModulated, modulatedCutoffs);
    for (int i = 0; i < 4; i++) {
        tempCutoffs[i] = 1.0f - expf(-2.0f * M_PI * tempModulated[i] / filter.sampleRate);
        if (std::isnan(tempCutoffs[i]) || !std::isfinite(tempCutoffs[i])) tempCutoffs[i] = 0.0f;
    }
    SIMD_TYPE alpha = SIMD_SET(tempCutoffs[0], tempCutoffs[1], tempCutoffs[2], tempCutoffs[3]);
    SIMD_TYPE resonance = SIMD_SET1(std::min(filter.resonance * 4.0f, 4.0f));
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
#if defined(__aarch64__) || defined(__arm64__)
    for (int i = 0; i < 4; i++) {
        states[i] = voiceOffset < MAX_VOICE_POLYPHONY ? voices[voiceOffset].filterStates[i] : vdupq_n_f32(0.0f);
    }
#else
    for (int i = 0; i < 4; i++) {
        float temp[4] = {voiceOffset < MAX_VOICE_POLYPHONY ? voices[voiceOffset].filterStates[i] : 0.0f,
                         voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].filterStates[i] : 0.0f,
                         voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].filterStates[i] : 0.0f,
                         voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].filterStates[i] : 0.0f};
        states[i] = SIMD_LOAD(temp);
    }
#endif
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
            tempCheck[i] = std::max(-1.0f, std::min(1.0f, tempCheck[i]));
        }
    }
    output = SIMD_LOAD(tempCheck);
    float tempOut[4];
    SIMD_STORE(tempOut, output);
    if (std::isnan(tempOut[0]) || std::isnan(tempOut[1]) || std::isnan(tempOut[2]) || std::isnan(tempOut[3])) {
        DBG("Filter output nan at voiceOffset " << voiceOffset << ": {" << tempOut[0] << ", " << tempOut[1] << ", "
                                                << tempOut[2] << ", " << tempOut[3] << "}");
    }
#if defined(__aarch64__) || defined(__arm64__)
    for (int i = 0; i < 4; i++) {
        if (voiceOffset < MAX_VOICE_POLYPHONY) voices[voiceOffset].filterStates[i] = states[i];
    }
#else
    for (int i = 0; i < 4; i++) {
        float temp[4];
        SIMD_STORE(temp, states[i]);
        if (voiceOffset < MAX_VOICE_POLYPHONY) voices[voiceOffset].filterStates[i] = temp[0];
        if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].filterStates[i] = temp[1];
        if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].filterStates[i] = temp[2];
        if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].filterStates[i] = temp[3];
    }
#endif
}

// Update amplitude and filter envelopes with full ADSR
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
        float attack = std::max(voices[i].attack, 0.001f);
        float decay = std::max(voices[i].decay, 0.001f);
        float sustain = std::max(voices[i].sustain, 0.0f);
        float release = std::max(voices[i].release, 0.001f);
        float fegAttack = std::max(voices[i].fegAttack, 0.001f);
        float fegDecay = std::max(voices[i].fegDecay, 0.001f);
        float fegSustain = std::max(voices[i].fegSustain, 0.0f);
        float fegRelease = std::max(voices[i].fegRelease, 0.001f);
        // Amplitude envelope (ADSR)
        if (localTime < 0.0f) {
            voices[i].amplitude = 0.0f;
        } else if (localTime < attack) {
            voices[i].amplitude = localTime / attack;
        } else if (localTime < attack + decay) {
            voices[i].amplitude = 1.0f - ((localTime - attack) / decay) * (1.0f - sustain);
        } else if (!voices[i].released) {
            voices[i].amplitude = sustain;
        } else {
            float releaseTime = t - voices[i].noteOffTime;
            voices[i].amplitude = sustain * (1.0f - (releaseTime / release));
            if (voices[i].amplitude <= 0.0f) {
                voices[i].amplitude = 0.0f;
                voices[i].active = false;
                for (int j = 0; j < 4; j++) {
#if defined(__aarch64__) || defined(__arm64__)
                    voices[i].filterStates[j] = vdupq_n_f32(0.0f);
#else
                    voices[i].filterStates[j] = 0.0f;
#endif
                }
            }
        }
        voices[i].amplitude = std::max(0.0f, std::min(1.0f, voices[i].amplitude));
        // Filter envelope (ADSR)
        if (localTime < 0.0f) {
            voices[i].filterEnv = 0.0f;
        } else if (localTime < fegAttack) {
            voices[i].filterEnv = localTime / fegAttack;
        } else if (localTime < fegAttack + fegDecay) {
            voices[i].filterEnv = 1.0f - ((localTime - fegAttack) / fegDecay) * (1.0f - fegSustain);
        } else if (!voices[i].released) {
            voices[i].filterEnv = fegSustain;
        } else {
            float releaseTime = t - voices[i].noteOffTime;
            voices[i].filterEnv = fegSustain * (1.0f - (releaseTime / fegRelease));
            if (voices[i].filterEnv <= 0.0f) {
                voices[i].filterEnv = 0.0f;
                voices[i].active = false;
                for (int j = 0; j < 4; j++) {
#if defined(__aarch64__) || defined(__arm64__)
                    voices[i].filterStates[j] = vdupq_n_f32(0.0f);
#else
                    voices[i].filterStates[j] = 0.0f;
#endif
                }
            }
        }
        voices[i].filterEnv = std::max(0.0f, std::min(1.0f, voices[i].filterEnv));
    }
}

// Prepare audio processing with oversampling
void SimdSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    filter.sampleRate = static_cast<float>(sampleRate);
    currentTime = 0.0;
    oversampling->initProcessing(samplesPerBlock);
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i].active = false;
        voices[i].released = false;
        voices[i].amplitude = 0.0f;
        voices[i].velocity = 0.0f;
        voices[i].noteOnTime = 0.0f;
        voices[i].noteOffTime = 0.0f;
        for (int j = 0; j < 4; ++j) {
#if defined(__aarch64__) || defined(__arm64__)
            voices[i].filterStates[j] = vdupq_n_f32(0.0f);
#else
            voices[i].filterStates[j] = 0.0f;
#endif
        }
    }
}

// Release resources
void SimdSynthAudioProcessor::releaseResources() { oversampling->reset(); }

// Process audio and MIDI with oversampling
void SimdSynthAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    buffer.clear();

    // Prepare oversampled buffer
    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(block);
    float sampleRate = filter.sampleRate * oversampling->getOversamplingFactor();
    double blockStartTime = currentTime;

    // Update parameters for inactive voices
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        if (!voices[i].active) {
            voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
            voices[i].attack = *attackTimeParam;
            voices[i].decay = *decayTimeParam;
            voices[i].sustain = *sustainLevelParam;
            voices[i].release = *releaseTimeParam;
            voices[i].cutoff = *cutoffParam;
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
        }
    }
    filter.resonance = *resonanceParam;

    // Process MIDI and audio
    int sampleIndex = 0;
    juce::MidiBuffer oversampledMidi;
    for (const auto metadata : midiMessages) {
        oversampledMidi.addEvent(metadata.getMessage(),
                                 metadata.samplePosition * oversampling->getOversamplingFactor());
    }

    for (const auto metadata : oversampledMidi) {
        auto msg = metadata.getMessage();
        int samplePosition = metadata.samplePosition;

        while (sampleIndex < samplePosition && sampleIndex < oversampledBlock.getNumSamples()) {
            float t = static_cast<float>(blockStartTime + static_cast<double>(sampleIndex) / sampleRate);
            updateEnvelopes(t);
            float outputSample = 0.0f;
            const SIMD_TYPE twoPi = SIMD_SET1(2.0f * M_PI);

            for (int group = 0; group < (MAX_VOICE_POLYPHONY + 3) / 4; group++) {
                int voiceOffset = group * 4;
                if (voiceOffset >= MAX_VOICE_POLYPHONY) continue;

                float tempAmps[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempPhases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempIncrements[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempLfoPhases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempLfoRates[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempLfoDepths[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempSubPhases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempSubIncrements[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempSubMixes[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                float tempWavetableTypes[4] = {0.0f, 0.0f, 0.0f, 0.0f};

                // Handle unison voices
                for (int j = 0; j < 4; ++j) {
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
                    tempSubMixes[j] = voices[idx].subMix;
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
                SIMD_TYPE wavetableTypePs = SIMD_LOAD(tempWavetableTypes);

                // Update LFO
                SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRates, SIMD_SET1(2.0f * M_PI / sampleRate));
                lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
                lfoPhases = SIMD_SUB(lfoPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)), twoPi));
                SIMD_TYPE lfoValues = SIMD_SIN(lfoPhases);
                lfoValues = SIMD_MUL(lfoValues, lfoDepths);
                SIMD_TYPE phaseMod = SIMD_DIV(lfoValues, twoPi);

                // Unison processing
                SIMD_TYPE unisonOutput = SIMD_SET1(0.0f);
                for (int u = 0; u < static_cast<int>(*unisonParam); ++u) {
                    float detune = *detuneParam * (u - (static_cast<int>(*unisonParam) - 1) / 2.0f) /
                                   (static_cast<int>(*unisonParam) - 1 + 0.0001f);
                    SIMD_TYPE detuneFactor = SIMD_SET1(powf(2.0f, detune / 12.0f));
                    SIMD_TYPE detunedPhases = SIMD_ADD(phases, phaseMod);
                    detunedPhases = SIMD_MUL(detunedPhases, detuneFactor);
                    SIMD_TYPE phases_norm = SIMD_SUB(detunedPhases, SIMD_FLOOR(detunedPhases));

                    // Generate oscillator signal using lookup tables
                    SIMD_TYPE mainValues = SIMD_SET1(0.0f);
                    for (int j = 0; j < 4; ++j) {
                        int idx = voiceOffset + j;
                        if (idx >= MAX_VOICE_POLYPHONY || !voices[idx].active) continue;
                        mainValues = wavetable_lookup_ps(phases_norm, voices[idx].wavetableType);
                    }
                    mainValues =
                        SIMD_MUL(mainValues,
                                 SIMD_SET1(1.0f / static_cast<float>(*unisonParam))); // Normalize unison amplitude
                    unisonOutput = SIMD_ADD(unisonOutput, mainValues);
                }

                unisonOutput = SIMD_MUL(unisonOutput, amplitudes);
                unisonOutput = SIMD_MUL(unisonOutput, SIMD_SUB(SIMD_SET1(1.0f), subMixes));

                SIMD_TYPE subSinValues = SIMD_SIN(subPhases);
                subSinValues = SIMD_MUL(subSinValues, amplitudes);
                subSinValues = SIMD_MUL(subSinValues, subMixes);

                SIMD_TYPE combinedValues = SIMD_ADD(unisonOutput, subSinValues);
                SIMD_TYPE filteredOutput;
                applyLadderFilter(voices, voiceOffset, combinedValues, filter, filteredOutput);
                float temp[4];
                SIMD_STORE(temp, filteredOutput);
                outputSample += (temp[0] + temp[1] + temp[2] + temp[3]);

                phases = SIMD_ADD(phases, increments);
                phases = SIMD_SUB(phases, SIMD_FLOOR(phases));
                SIMD_STORE(temp, phases);
                if (voiceOffset < MAX_VOICE_POLYPHONY) voices[voiceOffset].phase = temp[0];
                if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].phase = temp[1];
                if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].phase = temp[2];
                if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].phase = temp[3];

                subPhases = SIMD_ADD(subPhases, subIncrements);
                subPhases = SIMD_SUB(subPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(subPhases, twoPi)), twoPi));
                SIMD_STORE(temp, subPhases);
                if (voiceOffset < MAX_VOICE_POLYPHONY) voices[voiceOffset].subPhase = temp[0];
                if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].subPhase = temp[1];
                if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].subPhase = temp[2];
                if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].subPhase = temp[3];

                SIMD_STORE(temp, lfoPhases);
                if (voiceOffset < MAX_VOICE_POLYPHONY) voices[voiceOffset].lfoPhase = temp[0];
                if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].lfoPhase = temp[1];
                if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].lfoPhase = temp[2];
                if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].lfoPhase = temp[3];
            }

            outputSample *= *gainParam; // Apply user-controlled gain
            if (std::isnan(outputSample) || !std::isfinite(outputSample)) {
                outputSample = 0.0f;
            }

            for (int channel = 0; channel < totalNumOutputChannels; ++channel) {
                oversampledBlock.setSample(channel, sampleIndex, outputSample);
            }
            sampleIndex++;
        }

        if (msg.isNoteOn()) {
            int note = msg.getNoteNumber();
            float velocity = 0.7f + (msg.getVelocity() / 127.0f) * 0.3f;
            for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
                if (!voices[i].active) {
                    voices[i].active = true;
                    voices[i].released = false;
                    voices[i].frequency = midiToFreq(note);
                    voices[i].phaseIncrement = voices[i].frequency / sampleRate;
                    voices[i].phase = 0.0f;
                    voices[i].lfoPhase = 0.0f;
                    voices[i].amplitude = 0.0f;
                    voices[i].noteNumber = note;
                    voices[i].velocity = velocity;
                    voices[i].noteOnTime =
                        static_cast<float>(blockStartTime + static_cast<double>(samplePosition) / sampleRate);
                    voices[i].noteOffTime = 0.0f;
                    float subFreq = voices[i].frequency * powf(2.0f, voices[i].subTune / 12.0f) * voices[i].subTrack;
                    voices[i].subFrequency = subFreq;
                    voices[i].subPhaseIncrement = (2.0f * M_PI * subFreq) / sampleRate;
                    voices[i].subPhase = 0.0f;
                    break;
                }
            }
        } else if (msg.isNoteOff()) {
            int note = msg.getNoteNumber();
            for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
                if (voices[i].active && voices[i].noteNumber == note) {
                    voices[i].released = true;
                    voices[i].noteOffTime =
                        static_cast<float>(blockStartTime + static_cast<double>(samplePosition) / sampleRate);
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

    while (sampleIndex < oversampledBlock.getNumSamples()) {
        float t = static_cast<float>(blockStartTime + static_cast<double>(sampleIndex) / sampleRate);
        updateEnvelopes(t);
        float outputSample = 0.0f;
        const SIMD_TYPE twoPi = SIMD_SET1(2.0f * M_PI);

        for (int group = 0; group < (MAX_VOICE_POLYPHONY + 3) / 4; group++) {
            int voiceOffset = group * 4;
            if (voiceOffset >= MAX_VOICE_POLYPHONY) continue;

            float tempAmps[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempPhases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempIncrements[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempLfoPhases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempLfoRates[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempLfoDepths[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempSubPhases[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempSubIncrements[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempSubMixes[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float tempWavetableTypes[4] = {0.0f, 0.0f, 0.0f, 0.0f};

            for (int j = 0; j < 4; ++j) {
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
                tempSubMixes[j] = voices[idx].subMix;
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
            SIMD_TYPE wavetableTypePs = SIMD_LOAD(tempWavetableTypes);

            SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRates, SIMD_SET1(2.0f * M_PI / sampleRate));
            lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
            lfoPhases = SIMD_SUB(lfoPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)), twoPi));
            SIMD_TYPE lfoValues = SIMD_SIN(lfoPhases);
            lfoValues = SIMD_MUL(lfoValues, lfoDepths);
            SIMD_TYPE phaseMod = SIMD_DIV(lfoValues, twoPi);

            SIMD_TYPE unisonOutput = SIMD_SET1(0.0f);
            for (int u = 0; u < static_cast<int>(*unisonParam); ++u) {
                float detune = *detuneParam * (u - (static_cast<int>(*unisonParam) - 1) / 2.0f) /
                               (static_cast<int>(*unisonParam) - 1 + 0.0001f);
                SIMD_TYPE detuneFactor = SIMD_SET1(powf(2.0f, detune / 12.0f));
                SIMD_TYPE detunedPhases = SIMD_ADD(phases, phaseMod);
                detunedPhases = SIMD_MUL(detunedPhases, detuneFactor);
                SIMD_TYPE phases_norm = SIMD_SUB(detunedPhases, SIMD_FLOOR(detunedPhases));

                SIMD_TYPE mainValues = SIMD_SET1(0.0f);
                for (int j = 0; j < 4; ++j) {
                    int idx = voiceOffset + j;
                    if (idx >= MAX_VOICE_POLYPHONY || !voices[idx].active) continue;
                    mainValues = wavetable_lookup_ps(phases_norm, voices[idx].wavetableType);
                }
                mainValues = SIMD_MUL(mainValues, SIMD_SET1(1.0f / static_cast<float>(*unisonParam)));
                unisonOutput = SIMD_ADD(unisonOutput, mainValues);
            }

            unisonOutput = SIMD_MUL(unisonOutput, amplitudes);
            unisonOutput = SIMD_MUL(unisonOutput, SIMD_SUB(SIMD_SET1(1.0f), subMixes));

            SIMD_TYPE subSinValues = SIMD_SIN(subPhases);
            subSinValues = SIMD_MUL(subSinValues, amplitudes);
            subSinValues = SIMD_MUL(subSinValues, subMixes);

            SIMD_TYPE combinedValues = SIMD_ADD(unisonOutput, subSinValues);
            SIMD_TYPE filteredOutput;
            applyLadderFilter(voices, voiceOffset, combinedValues, filter, filteredOutput);
            float temp[4];
            SIMD_STORE(temp, filteredOutput);
            outputSample += (temp[0] + temp[1] + temp[2] + temp[3]);

            phases = SIMD_ADD(phases, increments);
            phases = SIMD_SUB(phases, SIMD_FLOOR(phases));
            SIMD_STORE(temp, phases);
            if (voiceOffset < MAX_VOICE_POLYPHONY) voices[voiceOffset].phase = temp[0];
            if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].phase = temp[1];
            if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].phase = temp[2];
            if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].phase = temp[3];

            subPhases = SIMD_ADD(subPhases, subIncrements);
            subPhases = SIMD_SUB(subPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(subPhases, twoPi)), twoPi));
            SIMD_STORE(temp, subPhases);
            if (voiceOffset < MAX_VOICE_POLYPHONY) voices[voiceOffset].subPhase = temp[0];
            if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].subPhase = temp[1];
            if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].subPhase = temp[2];
            if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].subPhase = temp[3];

            SIMD_STORE(temp, lfoPhases);
            if (voiceOffset < MAX_VOICE_POLYPHONY) voices[voiceOffset].lfoPhase = temp[0];
            if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].lfoPhase = temp[1];
            if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].lfoPhase = temp[2];
            if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].lfoPhase = temp[3];
        }

        outputSample *= *gainParam;
        if (std::isnan(outputSample) || !std::isfinite(outputSample)) {
            outputSample = 0.0f;
        }

        for (int channel = 0; channel < totalNumOutputChannels; ++channel) {
            oversampledBlock.setSample(channel, sampleIndex, outputSample);
        }
        sampleIndex++;
    }

    oversampling->processSamplesDown(block);
    currentTime += static_cast<double>(buffer.getNumSamples()) / filter.sampleRate;
}

// Create the editor for the plugin
juce::AudioProcessorEditor *SimdSynthAudioProcessor::createEditor() { return new SimdSynthAudioProcessorEditor(*this); }

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

// Create plugin instance
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new SimdSynthAudioProcessor(); }
