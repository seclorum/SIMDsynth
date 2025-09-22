#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_core/juce_core.h> // For File and JSON handling

// Constructor: Initializes the audio processor with stereo output and sets up parameters
SimdSynthAudioProcessor::SimdSynthAudioProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          parameters(*this, nullptr, juce::Identifier("SimdSynth"),
                     {
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"wavetable", parameterVersion}, "Wavetable Type", 0.0f, 1.0f, 0.0f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"attack", parameterVersion}, "Attack Time", 0.01f, 2.0f, 0.1f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"decay", parameterVersion}, "Decay Time", 0.1f, 5.0f, 1.9f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"cutoff", parameterVersion}, "Filter Cutoff", 200.0f, 8000.0f, 1000.0f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"resonance", parameterVersion}, "Filter Resonance", 0.0f, 1.0f, 0.7f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegAttack", parameterVersion}, "Filter EG Attack", 0.01f, 2.0f, 0.1f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegDecay", parameterVersion}, "Filter EG Decay", 0.1f, 5.0f, 1.0f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegSustain", parameterVersion}, "Filter EG Sustain", 0.0f, 1.0f, 0.5f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"fegRelease", parameterVersion}, "Filter EG Release", 0.01f, 2.0f, 0.2f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lfoRate", parameterVersion}, "LFO Rate", 0.0f, 20.0f, 1.0f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lfoDepth", parameterVersion}, "LFO Depth", 0.0f, 0.1f, 0.01f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subTune", parameterVersion}, "Sub Osc Tune", -24.0f, 0.0f, -12.0f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subMix", parameterVersion}, "Sub Osc Mix", 0.0f, 1.0f, 0.5f),
                             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"subTrack", parameterVersion}, "Sub Osc Track", 0.0f, 1.0f, 1.0f)
                     }),
          currentTime(0.0) {  // Initialize absolute time tracker
    // Initialize parameter pointers
    wavetableTypeParam = parameters.getRawParameterValue("wavetable");
    attackTimeParam = parameters.getRawParameterValue("attack");
    decayTimeParam = parameters.getRawParameterValue("decay");
    cutoffParam = parameters.getRawParameterValue("cutoff");
    resonanceParam = parameters.getRawParameterValue("resonance");
    fegAttackParam = parameters.getRawParameterValue("fegAttack");
    fegDecayParam = parameters.getRawParameterValue("fegDecay");
    fegSustainParam = parameters.getRawParameterValue("fegSustain");
    fegReleaseParam = parameters.getRawParameterValue("fegRelease");
    lfoRateParam = parameters.getRawParameterValue("lfoRate");
    lfoDepthParam = parameters.getRawParameterValue("lfoDepth");
    subTuneParam = parameters.getRawParameterValue("subTune");
    subMixParam = parameters.getRawParameterValue("subMix");
    subTrackParam = parameters.getRawParameterValue("subTrack");

    // Initialize voices with default parameter values and released state
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i] = Voice();
        voices[i].active = false;
        voices[i].released = false;  // Fix: Add released flag for gated envelopes
        voices[i].noteOnTime = 0.0f;
        voices[i].noteOffTime = 0.0f;  // Fix: Add noteOffTime for release phase
        voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
        voices[i].attack = *attackTimeParam;
        voices[i].decay = *decayTimeParam;
        voices[i].cutoff = *cutoffParam;
        voices[i].fegAttack = *fegAttackParam;
        voices[i].fegDecay = *fegDecayParam;
        voices[i].fegSustain = *fegSustainParam;
        voices[i].fegRelease = *fegReleaseParam;
        voices[i].lfoRate = *lfoRateParam;
        voices[i].lfoDepth = *lfoDepthParam;
        voices[i].subTune = *subTuneParam;
        voices[i].subMix = *subMixParam;
        voices[i].subTrack = *subTrackParam;
    }

    // Set initial filter resonance
    filter.resonance = *resonanceParam;

    // Initialize wavetables and presets
    initWavetables();
    presetManager.createDefaultPresets();
    loadPresetsFromDirectory();
}

// Destructor: No specific cleanup required
SimdSynthAudioProcessor::~SimdSynthAudioProcessor() {}

// Load presets from directory
void SimdSynthAudioProcessor::loadPresetsFromDirectory() {
    // Clear existing preset names
    presetNames.clear();
    // Define preset directory path
    juce::File presetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("SimdSynth/Presets");

    // Create directory if it doesn't exist
    if (!presetDir.exists()) {
        presetDir.createDirectory();
        presetManager.createDefaultPresets();
    }

    // Find all .json preset files
    juce::Array<juce::File> presetFiles;
    presetDir.findChildFiles(presetFiles, juce::File::findFiles, false, "*.json");

    // Add preset names to the list
    for (const auto& file : presetFiles) {
        presetNames.push_back(file.getFileNameWithoutExtension());
    }

    // Ensure at least a default preset exists
    if (presetNames.empty()) {
        DBG("No presets found in directory: " << presetDir.getFullPathName());
        presetNames.push_back("Default");
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

// Load a preset by index
void SimdSynthAudioProcessor::setCurrentProgram(int index) {
    // Validate preset index
    if (index < 0 || index >= presetNames.size()) {
        DBG("Error: Invalid preset index: " << index << ", presetNames size: " << presetNames.size());
        return;
    }

    // Update current program index
    currentProgram = index;
    // Construct preset file path
    juce::File presetFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("SimdSynth/Presets")
        .getChildFile(presetNames[index] + ".json");

    // Check if preset file exists
    if (!presetFile.existsAsFile()) {
        DBG("Error: Preset file not found: " << presetFile.getFullPathName());
        return;
    }

    // Load and parse JSON preset
    auto jsonString = presetFile.loadFileAsString();
    auto parsedJson = juce::JSON::parse(jsonString);

    DBG("Loading preset: " << presetNames[index] << ", File: " << presetFile.getFullPathName());
    DBG("JSON: " << jsonString);

    // Validate JSON structure
    if (!parsedJson.isObject()) {
        DBG("Error: Invalid JSON format in preset: " << presetNames[index]);
        return;
    }

    // Extract SimdSynth parameters from JSON
    juce::var synthParams = parsedJson.getProperty("SimdSynth", juce::var());
    if (!synthParams.isObject()) {
        DBG("Error: 'SimdSynth' object not found in preset: " << presetNames[index]);
        return;
    }

    // Define default parameter values
    std::map<juce::String, float> defaultValues = {
        {"wavetable", 0.0f}, {"attack", 0.1f}, {"decay", 1.9f}, {"cutoff", 1000.0f},
        {"resonance", 0.7f}, {"fegAttack", 0.1f}, {"fegDecay", 1.0f}, {"fegSustain", 0.5f},
        {"fegRelease", 0.2f}, {"lfoRate", 1.0f}, {"lfoDepth", 0.01f}, {"subTune", -12.0f},
        {"subMix", 0.5f}, {"subTrack", 1.0f}
    };

    // List of parameter IDs
    juce::StringArray paramIds = {
        "wavetable", "attack", "decay", "cutoff", "resonance",
        "fegAttack", "fegDecay", "fegSustain", "fegRelease",
        "lfoRate", "lfoDepth", "subTune", "subMix", "subTrack"
    };

    // Flag to track if any parameter was updated
    bool anyParamUpdated = false;
    // Update each parameter from JSON or use default
    for (const auto& paramId : paramIds) {
        if (auto* param = parameters.getParameter(paramId)) {
            if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param)) {
                float value = synthParams.hasProperty(paramId)
                    ? static_cast<float>(synthParams.getProperty(paramId, defaultValues[paramId]))
                    : defaultValues[paramId];
                // Quantize wavetable to 0.0 or 1.0
                if (paramId == "wavetable") {
                    value = (value >= 0.5f) ? 1.0f : 0.0f;
                }
                // Clamp value to parameter range
                value = juce::jlimit(floatParam->getNormalisableRange().start,
                                     floatParam->getNormalisableRange().end, value);
                floatParam->setValueNotifyingHost(floatParam->convertTo0to1(value));
                DBG("Setting " << paramId << " to " << value);
                anyParamUpdated = true;
            }
        }
    }

    // Warn if no parameters were updated
    if (!anyParamUpdated) {
        DBG("Warning: No parameters updated for preset: " << presetNames[index]);
    }

    // Update voice parameters
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
        voices[i].attack = *attackTimeParam;
        voices[i].decay = *decayTimeParam;
        voices[i].cutoff = *cutoffParam;
        voices[i].fegAttack = *fegAttackParam;
        voices[i].fegDecay = *fegDecayParam;
        voices[i].fegSustain = *fegSustainParam;
        voices[i].fegRelease = *fegReleaseParam;
        voices[i].lfoRate = *lfoRateParam;
        voices[i].lfoDepth = *lfoDepthParam;
        voices[i].subTune = *subTuneParam;
        voices[i].subMix = *subMixParam;
        voices[i].subTrack = *subTrackParam;
    }
    // Update filter resonance
    filter.resonance = *resonanceParam;

    // Notify editor to update UI
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
        // Construct paths for old and new preset files
        juce::File oldFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("SimdSynth/Presets")
                .getChildFile(presetNames[index] + ".json");
        juce::File newFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("SimdSynth/Presets")
                .getChildFile(newName + ".json");

        // Move file to rename it
        if (oldFile.existsAsFile()) {
            oldFile.moveFileTo(newFile);
            presetNames[index] = newName;
        }
    }
}

// Initialize wavetables (sine and sawtooth)
void SimdSynthAudioProcessor::initWavetables() {
    for (int i = 0; i < WAVETABLE_SIZE; ++i) {
        sineTable[i] = sinf(2.0f * M_PI * i / WAVETABLE_SIZE); // Sine wave: [-1, 1]
        sawTable[i] = 2.0f * (i / static_cast<float>(WAVETABLE_SIZE)) - 1.0f; // Sawtooth: [-1, 1]
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

// SIMD floor function for ARM64
#ifdef __arm64__
float32x4_t SimdSynthAudioProcessor::my_floor_ps(float32x4_t x) {
    float temp[4];
    vst1q_f32(temp, x);
    temp[0] = floorf(temp[0]);
    temp[1] = floorf(temp[1]);
    temp[2] = floorf(temp[2]);
    temp[3] = floorf(temp[3]);
    return vld1q_f32(temp);
}
#endif

// SIMD sine approximation for x86_64
#ifdef __x86_64__
__m128 SimdSynthAudioProcessor::fast_sin_ps(__m128 x) {
    const __m128 twoPi = _mm_set1_ps(2.0f * M_PI);
    const __m128 invTwoPi = _mm_set1_ps(1.0f / (2.0f * M_PI));
    const __m128 piOverTwo = _mm_set1_ps(M_PI / 2.0f);

    // Wrap phase to [0, 2π]
    __m128 q = _mm_mul_ps(x, invTwoPi);
    q = _mm_floor_ps(q);
    __m128 xWrapped = _mm_sub_ps(x, _mm_mul_ps(q, twoPi));

    // Determine sign and adjust phase
    __m128 sign = _mm_set1_ps(1.0f);
    __m128 absX = _mm_max_ps(xWrapped, _mm_sub_ps(_mm_setzero_ps(), xWrapped));
    __m128 gtPiOverTwo = _mm_cmpgt_ps(absX, piOverTwo);
    sign = _mm_or_ps(_mm_and_ps(gtPiOverTwo, _mm_set1_ps(-1.0f)),
                     _mm_andnot_ps(gtPiOverTwo, _mm_set1_ps(1.0f)));
    xWrapped = _mm_sub_ps(xWrapped,
                          _mm_and_ps(gtPiOverTwo,
                                     _mm_mul_ps(piOverTwo, _mm_set1_ps(2.0f))));

    // Taylor series approximation for sine
    const __m128 c3 = _mm_set1_ps(-1.0f / 6.0f);
    const __m128 c5 = _mm_set1_ps(1.0f / 120.0f);
    const __m128 c7 = _mm_set1_ps(-1.0f / 5040.0f);

    __m128 x2 = _mm_mul_ps(xWrapped, xWrapped);
    __m128 x3 = _mm_mul_ps(x2, xWrapped);
    __m128 x5 = _mm_mul_ps(x3, x2);
    __m128 x7 = _mm_mul_ps(x5, x2);

    __m128 result = _mm_add_ps(
        xWrapped,
        _mm_add_ps(_mm_mul_ps(c3, x3),
                   _mm_add_ps(_mm_mul_ps(c5, x5), _mm_mul_ps(c7, x7))));
    return _mm_mul_ps(result, sign);
}
#endif

// SIMD sine approximation for ARM64
#ifdef __arm64__
float32x4_t SimdSynthAudioProcessor::fast_sin_ps(float32x4_t x) {
    const float32x4_t twoPi = vdupq_n_f32(2.0f * M_PI);
    const float32x4_t invTwoPi = vdupq_n_f32(1.0f / (2.0f * M_PI));
    const float32x4_t piOverTwo = vdupq_n_f32(M_PI / 2.0f);

    // Wrap phase to [0, 2π]
    float32x4_t q = vmulq_f32(x, invTwoPi);
    q = my_floor_ps(q);
    float32x4_t xWrapped = vsubq_f32(x, vmulq_f32(q, twoPi));

    // Determine sign and adjust phase
    float32x4_t sign = vdupq_n_f32(1.0f);
    float32x4_t absX = vmaxq_f32(xWrapped, vsubq_f32(vdupq_n_f32(0.0f), xWrapped));
    uint32x4_t gtPiOverTwo = vcgtq_f32(absX, piOverTwo);
    sign = vbslq_f32(gtPiOverTwo, vdupq_n_f32(-1.0f), vdupq_n_f32(1.0f));
    xWrapped = vsubq_f32(xWrapped,
                         vbslq_f32(gtPiOverTwo,
                                   vmulq_f32(piOverTwo, vdupq_n_f32(2.0f)),
                                   vdupq_n_f32(0.0f)));

    // Taylor series approximation for sine
    const float32x4_t c3 = vdupq_n_f32(-1.0f / 6.0f);
    const float32x4_t c5 = vdupq_n_f32(1.0f / 120.0f);
    const float32x4_t c7 = vdupq_n_f32(-1.0f / 5040.0f);

    float32x4_t x2 = vmulq_f32(xWrapped, xWrapped);
    float32x4_t x3 = vmulq_f32(x2, xWrapped);
    float32x4_t x5 = vmulq_f32(x3, x2);
    float32x4_t x7 = vmulq_f32(x5, x2);

    float32x4_t result = vaddq_f32(
            xWrapped,
            vaddq_f32(vmulq_f32(c3, x3),
                      vaddq_f32(vmulq_f32(c5, x5), vmulq_f32(c7, x7))));
    return vmulq_f32(result, sign);
}
#endif

// Perform wavetable lookup with linear interpolation
SIMD_TYPE SimdSynthAudioProcessor::wavetable_lookup_ps(SIMD_TYPE phase, const float* table) {
    const SIMD_TYPE tableSize = SIMD_SET1(static_cast<float>(WAVETABLE_SIZE));
    // Scale phase to table index
    SIMD_TYPE index = SIMD_MUL(phase, tableSize);
    SIMD_TYPE indexFloor = SIMD_FLOOR(index);
    SIMD_TYPE frac = SIMD_SUB(index, indexFloor);
    // Wrap index to table size
    indexFloor = SIMD_SUB(indexFloor, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(indexFloor, tableSize)), tableSize));

    // Extract indices
    float tempIndices[4];
    SIMD_STORE(tempIndices, indexFloor);
    int indices[4] = {
            static_cast<int>(tempIndices[0]),
            static_cast<int>(tempIndices[1]),
            static_cast<int>(tempIndices[2]),
            static_cast<int>(tempIndices[3])
    };

    // Lookup samples and interpolate
    float samples1[4], samples2[4];
    for (int i = 0; i < 4; ++i) {
        int idx = indices[i];
        samples1[i] = table[idx];
        samples2[i] = table[(idx + 1) % WAVETABLE_SIZE];
    }

    SIMD_TYPE value1 = SIMD_LOAD(samples1);
    SIMD_TYPE value2 = SIMD_LOAD(samples2);
    return SIMD_ADD(value1, SIMD_MUL(frac, SIMD_SUB(value2, value1)));
}

// Apply ladder filter to input signal
void SimdSynthAudioProcessor::applyLadderFilter(Voice* voices, int voiceOffset, SIMD_TYPE input, Filter& filter, SIMD_TYPE& output) {
    // Calculate modulated cutoff frequencies
    float cutoffs[4];
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        float modulatedCutoff = voices[idx].cutoff + voices[idx].filterEnv * 2000.0f;
        modulatedCutoff = std::max(200.0f, std::min(modulatedCutoff, filter.sampleRate / 2.0f));
        cutoffs[i] = 1.0f - expf(-2.0f * M_PI * modulatedCutoff / filter.sampleRate);
        if (std::isnan(cutoffs[i]) || !std::isfinite(cutoffs[i]))
            cutoffs[i] = 0.0f;
    }
#ifdef __x86_64__
    SIMD_TYPE alpha = SIMD_SET(cutoffs[0], cutoffs[1], cutoffs[2], cutoffs[3]);
#elif defined(__arm64__)
    float temp[4] = {cutoffs[0], cutoffs[1], cutoffs[2], cutoffs[3]};
    SIMD_TYPE alpha = SIMD_LOAD(temp);
#endif
    // Scale resonance
    SIMD_TYPE resonance = SIMD_SET1(std::min(filter.resonance * 4.0f, 4.0f));

    // Check if any voices in the group are active
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

    // Load filter states
    SIMD_TYPE states[4];
    for (int i = 0; i < 4; i++) {
        float temp[4] = {
                voices[voiceOffset].filterStates[i],
                voices[voiceOffset + 1].filterStates[i],
                voices[voiceOffset + 2].filterStates[i],
                voices[voiceOffset + 3].filterStates[i]
        };
        states[i] = SIMD_LOAD(temp);
    }

    // Apply feedback and filter stages
    SIMD_TYPE feedback = SIMD_MUL(states[3], resonance);
    SIMD_TYPE filterInput = SIMD_SUB(input, feedback);

    states[0] = SIMD_ADD(states[0], SIMD_MUL(alpha, SIMD_SUB(filterInput, states[0])));
    states[1] = SIMD_ADD(states[1], SIMD_MUL(alpha, SIMD_SUB(states[0], states[1])));
    states[2] = SIMD_ADD(states[2], SIMD_MUL(alpha, SIMD_SUB(states[1], states[2])));
    states[3] = SIMD_ADD(states[3], SIMD_MUL(alpha, SIMD_SUB(states[2], states[3])));

    output = states[3];

    // Clamp output to prevent instability
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

    // Log NaN errors
    float tempOut[4];
    SIMD_STORE(tempOut, output);
    if (std::isnan(tempOut[0]) || std::isnan(tempOut[1]) ||
        std::isnan(tempOut[2]) || std::isnan(tempOut[3])) {
        DBG("Filter output nan at voiceOffset " << voiceOffset << ": {"
                  << tempOut[0] << ", " << tempOut[1] << ", " << tempOut[2]
                  << ", " << tempOut[3] << "}");
    }

    // Store updated filter states
    for (int i = 0; i < 4; i++) {
        float temp[4];
        SIMD_STORE(temp, states[i]);
        voices[voiceOffset].filterStates[i] = temp[0];
        voices[voiceOffset + 1].filterStates[i] = temp[1];
        voices[voiceOffset + 2].filterStates[i] = temp[2];
        voices[voiceOffset + 3].filterStates[i] = temp[3];
    }
}

// Update amplitude and filter envelopes (fixed to use gated ADSR logic for amp and filter)
void SimdSynthAudioProcessor::updateEnvelopes(float t) {  // Pass absolute time t
    for (int i = 0; i < MAX_VOICE_POLYPHONY; i++) {
        if (!voices[i].active) {
            // Reset inactive voices
            voices[i].amplitude = 0.0f;
            voices[i].filterEnv = 0.0f;
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] = 0.0f;
            }
            continue;
        }

        // Calculate time since note-on
        float localTime = t - voices[i].noteOnTime;

#if DEBUG
        if (i == 0) {
            DBG("Voice[0] envelope: localTime=" << localTime
                << ", attack=" << voices[i].attack
                << ", decay=" << voices[i].decay
                << ", amplitude=" << voices[i].amplitude);
        }
#endif

        // Ensure minimum times to prevent division by zero
        float attack = std::max(voices[i].attack, 0.001f);
        float decay = std::max(voices[i].decay, 0.001f);
        float fegAttack = std::max(voices[i].fegAttack, 0.001f);
        float fegDecay = std::max(voices[i].fegDecay, 0.001f);
        float fegRelease = std::max(voices[i].fegRelease, 0.001f);

        // Amplitude envelope (fixed to gated: attack to 1, sustain at 1 until released, then decay as release)
        if (localTime < 0.0f) {
            voices[i].amplitude = 0.0f;
        } else if (localTime < attack) {
            voices[i].amplitude = localTime / attack;
        } else if (!voices[i].released) {
            voices[i].amplitude = 1.0f;
        } else {
            float releaseTime = t - voices[i].noteOffTime;
            voices[i].amplitude = 1.0f - (releaseTime / decay);
            if (voices[i].amplitude <= 0.0f) {
                voices[i].amplitude = 0.0f;
                voices[i].active = false;
                for (int j = 0; j < 4; j++) {
                    voices[i].filterStates[j] = 0.0f;
                }
            }
        }
        voices[i].amplitude = std::max(0.0f, std::min(1.0f, voices[i].amplitude)); // Clamp amplitude

        // Filter envelope (fixed to standard gated ADSR)
        if (localTime < 0.0f) {
            voices[i].filterEnv = 0.0f;
        } else if (localTime < fegAttack) {
            voices[i].filterEnv = localTime / fegAttack;
        } else if (localTime < fegAttack + fegDecay) {
            voices[i].filterEnv = 1.0f - ((localTime - fegAttack) / fegDecay) * (1.0f - voices[i].fegSustain);
        } else if (!voices[i].released) {
            voices[i].filterEnv = voices[i].fegSustain;
        } else {
            float releaseTime = t - voices[i].noteOffTime;
            voices[i].filterEnv = voices[i].fegSustain * (1.0f - (releaseTime / fegRelease));
            if (voices[i].filterEnv <= 0.0f) {
                voices[i].filterEnv = 0.0f;
                voices[i].active = false;
                for (int j = 0; j < 4; j++) {
                    voices[i].filterStates[j] = 0.0f;
                }
            }
        }
        voices[i].filterEnv = std::max(0.0f, std::min(1.0f, voices[i].filterEnv)); // Clamp filter envelope
    }
}

// Prepare audio processing
void SimdSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    filter.sampleRate = static_cast<float>(sampleRate);
    currentTime = 0.0;  // Reset absolute time
    // Reset all voices
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i].active = false;
        voices[i].released = false;
        voices[i].amplitude = 0.0f;
        voices[i].velocity = 0.0f;
        voices[i].noteOnTime = 0.0f;
        voices[i].noteOffTime = 0.0f;
        for (int j = 0; j < 4; ++j) {
            voices[i].filterStates[j] = 0.0f;
        }
    }
}

// Release resources (empty for now)
void SimdSynthAudioProcessor::releaseResources() {}

// Process audio and MIDI
void SimdSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals; // Prevent denormal numbers for performance
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    buffer.clear(); // Clear output buffer

    double blockStartTime = currentTime;  // Fix: Track absolute time for the block start
    float sampleRate = filter.sampleRate;  // Cache sample rate

    // Update parameters for inactive voices
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        if (!voices[i].active) {
            voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
            voices[i].attack = *attackTimeParam;
            voices[i].decay = *decayTimeParam;
            voices[i].cutoff = *cutoffParam;
            voices[i].fegAttack = *fegAttackParam;
            voices[i].fegDecay = *fegDecayParam;
            voices[i].fegSustain = *fegSustainParam;
            voices[i].fegRelease = *fegReleaseParam;
            voices[i].lfoRate = *lfoRateParam;
            voices[i].lfoDepth = *lfoDepthParam;
            voices[i].subTune = *subTuneParam;
            voices[i].subMix = *subMixParam;
            voices[i].subTrack = *subTrackParam;
        }
    }
    filter.resonance = *resonanceParam;

#if DEBUG
    // Log parameters for first active voice
    if (voices[0].active) {
        DBG("Voice[0] parameters: wavetableType=" << voices[0].wavetableType
            << ", attack=" << voices[0].attack
            << ", decay=" << voices[0].decay
            << ", cutoff=" << voices[0].cutoff
            << ", fegAttack=" << voices[0].fegAttack
            << ", velocity=" << voices[0].velocity); // Added velocity logging
    }
#endif

    // Process MIDI and audio in sub-blocks
    int sampleIndex = 0;

    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();
        int samplePosition = metadata.samplePosition;

        // Process audio up to the MIDI event
        while (sampleIndex < samplePosition && sampleIndex < buffer.getNumSamples()) {
            float t = static_cast<float>(blockStartTime + static_cast<double>(sampleIndex) / sampleRate);  // Fix: Use absolute time
            updateEnvelopes(t); // Update envelopes for all voices
            float outputSample = 0.0f;
            const SIMD_TYPE twoPi = SIMD_SET1(2.0f * M_PI);

            int activeVoices = 0;  // Fix: Reset activeVoices per sample to count total active voices correctly

            // Process voices in groups of 4 for SIMD optimization
            for (int group = 0; group < (MAX_VOICE_POLYPHONY + 3) / 4; group++) {
                int voiceOffset = group * 4;
                if (voiceOffset >= MAX_VOICE_POLYPHONY) continue;

                // Count active voices for normalization
                for (int j = 0; j < 4; ++j) {
                    if (voiceOffset + j < MAX_VOICE_POLYPHONY && voices[voiceOffset + j].active) {
                        activeVoices++;
                    }
                }

                // Load voice parameters into arrays
                float tempAmps[4] = {
                    voices[voiceOffset].amplitude * voices[voiceOffset].velocity,
                    (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].amplitude * voices[voiceOffset + 1].velocity : 0.0f),
                    (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].amplitude * voices[voiceOffset + 2].velocity : 0.0f),
                    (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].amplitude * voices[voiceOffset + 3].velocity : 0.0f)
                };
                float tempPhases[4] = {
                    voices[voiceOffset].phase,
                    (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].phase : 0.0f),
                    (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].phase : 0.0f),
                    (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].phase : 0.0f)
                };
                float tempIncrements[4] = {
                    voices[voiceOffset].phaseIncrement,
                    (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].phaseIncrement : 0.0f),
                    (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].phaseIncrement : 0.0f),
                    (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].phaseIncrement : 0.0f)
                };
                float tempLfoPhases[4] = {
                    voices[voiceOffset].lfoPhase,
                    (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].lfoPhase : 0.0f),
                    (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].lfoPhase : 0.0f),
                    (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].lfoPhase : 0.0f)
                };
                float tempLfoRates[4] = {
                    voices[voiceOffset].lfoRate,
                    (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].lfoRate : 0.0f),
                    (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].lfoRate : 0.0f),
                    (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].lfoRate : 0.0f)
                };
                float tempLfoDepths[4] = {
                    voices[voiceOffset].lfoDepth,
                    (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].lfoDepth : 0.0f),
                    (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].lfoDepth : 0.0f),
                    (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].lfoDepth : 0.0f)
                };
                float tempSubPhases[4] = {
                    voices[voiceOffset].subPhase,
                    (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].subPhase : 0.0f),
                    (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].subPhase : 0.0f),
                    (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].subPhase : 0.0f)
                };
                float tempSubIncrements[4] = {
                    voices[voiceOffset].subPhaseIncrement,
                    (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].subPhaseIncrement : 0.0f),
                    (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].subPhaseIncrement : 0.0f),
                    (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].subPhaseIncrement : 0.0f)
                };
                float tempSubMixes[4] = {
                    voices[voiceOffset].subMix,
                    (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].subMix : 0.0f),
                    (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].subMix : 0.0f),
                    (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].subMix : 0.0f)
                };
                float tempWavetableTypes[4] = {
                    static_cast<float>(voices[voiceOffset].wavetableType),
                    (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? static_cast<float>(voices[voiceOffset + 1].wavetableType) : 0.0f),
                    (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? static_cast<float>(voices[voiceOffset + 2].wavetableType) : 0.0f),
                    (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? static_cast<float>(voices[voiceOffset + 3].wavetableType) : 0.0f)
                };

                // Load parameters into SIMD registers
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

                // Update LFO phase and apply modulation
                SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRates, SIMD_SET1(2.0f * M_PI / sampleRate));
                lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
                lfoPhases = SIMD_SUB(lfoPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)), twoPi));
                SIMD_TYPE lfoValues = SIMD_SIN(lfoPhases);
                lfoValues = SIMD_MUL(lfoValues, lfoDepths);
                phases = SIMD_ADD(phases, SIMD_DIV(lfoValues, twoPi));

                // Normalize phases to [0, 1)
                SIMD_TYPE phases_norm = SIMD_SUB(phases, SIMD_FLOOR(phases));

                // Generate main oscillator signal (fixed: vectorized wavetable selection for efficiency)
                SIMD_TYPE sine_values = wavetable_lookup_ps(phases_norm, sineTable);
                SIMD_TYPE saw_values = wavetable_lookup_ps(phases_norm, sawTable);
                SIMD_TYPE mask = SIMD_CMP_EQ(wavetableTypePs, SIMD_SET1(0.0f));  // Mask for sine (true if type == 0)
#ifdef __x86_64__
                SIMD_TYPE mainValues = _mm_blendv_ps(saw_values, sine_values, mask);  // Select sine if mask < 0, saw otherwise
#elif defined(__arm64__)
                SIMD_TYPE mainValues = vbslq_f32(mask, sine_values, saw_values);  // Select sine if mask true, saw otherwise
#endif
                mainValues = SIMD_MUL(mainValues, amplitudes);
                mainValues = SIMD_MUL(mainValues, SIMD_SUB(SIMD_SET1(1.0f), subMixes));

                // Generate sub-oscillator signal
                SIMD_TYPE subSinValues = SIMD_SIN(subPhases);
                subSinValues = SIMD_MUL(subSinValues, amplitudes);
                subSinValues = SIMD_MUL(subSinValues, subMixes);

                // Combine main and sub-oscillator signals
                SIMD_TYPE combinedValues = SIMD_ADD(mainValues, subSinValues);

                // Apply ladder filter
                SIMD_TYPE filteredOutput;
                applyLadderFilter(voices, voiceOffset, combinedValues, filter, filteredOutput);

                // Sum filtered output
                float temp[4];
                SIMD_STORE(temp, filteredOutput);
                outputSample += (temp[0] + temp[1] + temp[2] + temp[3]);

#if DEBUGAUDIO
                // Log audio processing details for first group
                if (voiceOffset == 0) {
                    float tempAmpsDbg[4], tempMainDbg[4], tempSubDbg[4], tempFiltDbg[4];
                    SIMD_STORE(tempAmpsDbg, amplitudes);
                    SIMD_STORE(tempMainDbg, mainValues);
                    SIMD_STORE(tempSubDbg, subSinValues);
                    SIMD_STORE(tempFiltDbg, filteredOutput);
                    DBG("Audio: voiceOffset=" << voiceOffset
                        << ", amplitudes={" << tempAmpsDbg[0] << ", " << tempAmpsDbg[1] << ", " << tempAmpsDbg[2] << ", " << tempAmpsDbg[3]
                        << "}, mainValues={" << tempMainDbg[0] << ", " << tempMainDbg[1] << ", " << tempMainDbg[2] << ", " << tempMainDbg[3]
                        << "}, subSinValues={" << tempSubDbg[0] << ", " << tempSubDbg[1] << ", " << tempSubDbg[2] << ", " << tempSubDbg[3]
                        << "}, filteredOutput={" << tempFiltDbg[0] << ", " << tempFiltDbg[1] << ", " << tempFiltDbg[2] << ", " << tempFiltDbg[3]
                        << "}, outputSample=" << outputSample);
                }
#endif

                // Update phases
                phases = SIMD_ADD(phases, increments);
                phases = SIMD_SUB(phases, SIMD_FLOOR(phases));
                SIMD_STORE(temp, phases);
                voices[voiceOffset].phase = temp[0];
                if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].phase = temp[1];
                if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].phase = temp[2];
                if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].phase = temp[3];

                subPhases = SIMD_ADD(subPhases, subIncrements);
                subPhases = SIMD_SUB(subPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(subPhases, twoPi)), twoPi));
                SIMD_STORE(temp, subPhases);
                voices[voiceOffset].subPhase = temp[0];
                if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].subPhase = temp[1];
                if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].subPhase = temp[2];
                if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].subPhase = temp[3];

                SIMD_STORE(temp, lfoPhases);
                voices[voiceOffset].lfoPhase = temp[0];
                if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].lfoPhase = temp[1];
                if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].lfoPhase = temp[2];
                if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].lfoPhase = temp[3];
            }

            // Fix: Removed normalization by activeVoices to make synth louder (allows summing voices); increased global gain
            outputSample *= 5.0f;  // Fix: Increased gain to make synth louder (was 2.0f)

            // Prevent NaN or infinite values
            if (std::isnan(outputSample) || !std::isfinite(outputSample)) {
                outputSample = 0.0f;
            }

            // Write to output buffer
            for (int channel = 0; channel < totalNumOutputChannels; ++channel) {
                buffer.addSample(channel, sampleIndex, outputSample);
            }
            sampleIndex++;
        }

        // Process MIDI event
        if (msg.isNoteOn()) {
            int note = msg.getNoteNumber();
            // Linear velocity scaling with floor to boost low velocities
            float velocity = 0.7f + (msg.getVelocity() / 127.0f) * 0.3f; // Maps MIDI 1 to 0.7, 127 to 1.0
            for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
                if (!voices[i].active) {
                    voices[i].active = true;
                    voices[i].released = false;  // Fix: Reset released on note on
                    voices[i].frequency = midiToFreq(note);
                    voices[i].phaseIncrement = voices[i].frequency / sampleRate;
                    voices[i].phase = 0.0f;
                    voices[i].lfoPhase = 0.0f;
                    voices[i].amplitude = 0.0f;
                    voices[i].noteNumber = note;
                    voices[i].velocity = velocity;
                    voices[i].noteOnTime = static_cast<float>(blockStartTime + static_cast<double>(samplePosition) / sampleRate);  // Fix: Use absolute time
                    voices[i].noteOffTime = 0.0f;
                    float subFreq = voices[i].frequency * powf(2.0f, voices[i].subTune / 12.0f) * voices[i].subTrack;
                    voices[i].subFrequency = subFreq;
                    voices[i].subPhaseIncrement = (2.0f * M_PI * subFreq) / sampleRate;
                    voices[i].subPhase = 0.0f;
#if DEBUG
                    DBG("NoteOn: voice=" << i << ", noteOnTime=" << voices[i].noteOnTime
                        << ", samplePosition=" << samplePosition << ", velocity=" << velocity);
#endif
                    break;
                }
            }
        } else if (msg.isNoteOff()) {
            int note = msg.getNoteNumber();
            for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
                if (voices[i].active && voices[i].noteNumber == note) {
                    voices[i].released = true;  // Fix: Set released instead of deactivating immediately
                    voices[i].noteOffTime = static_cast<float>(blockStartTime + static_cast<double>(samplePosition) / sampleRate);  // Fix: Set absolute note off time
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

    // Process remaining samples
    while (sampleIndex < buffer.getNumSamples()) {
        float t = static_cast<float>(blockStartTime + static_cast<double>(sampleIndex) / sampleRate);  // Fix: Use absolute time
        updateEnvelopes(t);
        float outputSample = 0.0f;
        const SIMD_TYPE twoPi = SIMD_SET1(2.0f * M_PI);

        int activeVoices = 0;  // Fix: Reset activeVoices per sample

        for (int group = 0; group < (MAX_VOICE_POLYPHONY + 3) / 4; group++) {
            int voiceOffset = group * 4;
            if (voiceOffset >= MAX_VOICE_POLYPHONY) continue;

            // Count active voices
            for (int j = 0; j < 4; ++j) {
                if (voiceOffset + j < MAX_VOICE_POLYPHONY && voices[voiceOffset + j].active) {
                    activeVoices++;
                }
            }

            // Load voice parameters (similar to above, with boundary checks)
            float tempAmps[4] = {
                voices[voiceOffset].amplitude * voices[voiceOffset].velocity,
                (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].amplitude * voices[voiceOffset + 1].velocity : 0.0f),
                (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].amplitude * voices[voiceOffset + 2].velocity : 0.0f),
                (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].amplitude * voices[voiceOffset + 3].velocity : 0.0f)
            };
            float tempPhases[4] = {
                voices[voiceOffset].phase,
                (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].phase : 0.0f),
                (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].phase : 0.0f),
                (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].phase : 0.0f)
            };
            float tempIncrements[4] = {
                voices[voiceOffset].phaseIncrement,
                (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].phaseIncrement : 0.0f),
                (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].phaseIncrement : 0.0f),
                (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].phaseIncrement : 0.0f)
            };
            float tempLfoPhases[4] = {
                voices[voiceOffset].lfoPhase,
                (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].lfoPhase : 0.0f),
                (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].lfoPhase : 0.0f),
                (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].lfoPhase : 0.0f)
            };
            float tempLfoRates[4] = {
                voices[voiceOffset].lfoRate,
                (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].lfoRate : 0.0f),
                (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].lfoRate : 0.0f),
                (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].lfoRate : 0.0f)
            };
            float tempLfoDepths[4] = {
                voices[voiceOffset].lfoDepth,
                (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].lfoDepth : 0.0f),
                (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].lfoDepth : 0.0f),
                (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].lfoDepth : 0.0f)
            };
            float tempSubPhases[4] = {
                voices[voiceOffset].subPhase,
                (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].subPhase : 0.0f),
                (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].subPhase : 0.0f),
                (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].subPhase : 0.0f)
            };
            float tempSubIncrements[4] = {
                voices[voiceOffset].subPhaseIncrement,
                (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].subPhaseIncrement : 0.0f),
                (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].subPhaseIncrement : 0.0f),
                (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].subPhaseIncrement : 0.0f)
            };
            float tempSubMixes[4] = {
                voices[voiceOffset].subMix,
                (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 1].subMix : 0.0f),
                (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 2].subMix : 0.0f),
                (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? voices[voiceOffset + 3].subMix : 0.0f)
            };
            float tempWavetableTypes[4] = {
                static_cast<float>(voices[voiceOffset].wavetableType),
                (voiceOffset + 1 < MAX_VOICE_POLYPHONY ? static_cast<float>(voices[voiceOffset + 1].wavetableType) : 0.0f),
                (voiceOffset + 2 < MAX_VOICE_POLYPHONY ? static_cast<float>(voices[voiceOffset + 2].wavetableType) : 0.0f),
                (voiceOffset + 3 < MAX_VOICE_POLYPHONY ? static_cast<float>(voices[voiceOffset + 3].wavetableType) : 0.0f)
            };

            // Load parameters into SIMD registers
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

            // Update LFO phase and apply modulation
            SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRates, SIMD_SET1(2.0f * M_PI / sampleRate));
            lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
            lfoPhases = SIMD_SUB(lfoPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)), twoPi));
            SIMD_TYPE lfoValues = SIMD_SIN(lfoPhases);
            lfoValues = SIMD_MUL(lfoValues, lfoDepths);
            phases = SIMD_ADD(phases, SIMD_DIV(lfoValues, twoPi));

            // Normalize phases to [0, 1)
            SIMD_TYPE phases_norm = SIMD_SUB(phases, SIMD_FLOOR(phases));

            // Generate main oscillator signal (vectorized)
            SIMD_TYPE sine_values = wavetable_lookup_ps(phases_norm, sineTable);
            SIMD_TYPE saw_values = wavetable_lookup_ps(phases_norm, sawTable);
            SIMD_TYPE mask = SIMD_CMP_EQ(wavetableTypePs, SIMD_SET1(0.0f));
#ifdef __x86_64__
            SIMD_TYPE mainValues = _mm_blendv_ps(saw_values, sine_values, mask);
#elif defined(__arm64__)
            SIMD_TYPE mainValues = vbslq_f32(mask, sine_values, saw_values);
#endif
            mainValues = SIMD_MUL(mainValues, amplitudes);
            mainValues = SIMD_MUL(mainValues, SIMD_SUB(SIMD_SET1(1.0f), subMixes));

            // Generate sub-oscillator signal
            SIMD_TYPE subSinValues = SIMD_SIN(subPhases);
            subSinValues = SIMD_MUL(subSinValues, amplitudes);
            subSinValues = SIMD_MUL(subSinValues, subMixes);

            // Combine signals
            SIMD_TYPE combinedValues = SIMD_ADD(mainValues, subSinValues);

            // Apply ladder filter
            SIMD_TYPE filteredOutput;
            applyLadderFilter(voices, voiceOffset, combinedValues, filter, filteredOutput);

            // Sum filtered output
            float temp[4];
            SIMD_STORE(temp, filteredOutput);
            outputSample += (temp[0] + temp[1] + temp[2] + temp[3]);

#if DEBUGAUDIO
            // Log audio processing details
            if (voiceOffset == 0) {
                float tempAmpsDbg[4], tempMainDbg[4], tempSubDbg[4], tempFiltDbg[4];
                SIMD_STORE(tempAmpsDbg, amplitudes);
                SIMD_STORE(tempMainDbg, mainValues);
                SIMD_STORE(tempSubDbg, subSinValues);
                SIMD_STORE(tempFiltDbg, filteredOutput);
                DBG("Audio: voiceOffset=" << voiceOffset
                    << ", amplitudes={" << tempAmpsDbg[0] << ", " << tempAmpsDbg[1] << ", " << tempAmpsDbg[2] << ", " << tempAmpsDbg[3]
                    << "}, mainValues={" << tempMainDbg[0] << ", " << tempMainDbg[1] << ", " << tempMainDbg[2] << ", " << tempMainDbg[3]
                    << "}, subSinValues={" << tempSubDbg[0] << ", " << tempSubDbg[1] << ", " << tempSubDbg[2] << ", " << tempSubDbg[3]
                    << "}, filteredOutput={" << tempFiltDbg[0] << ", " << tempFiltDbg[1] << ", " << tempFiltDbg[2] << ", " << tempFiltDbg[3]
                    << "}, outputSample=" << outputSample);
            }
#endif

            // Update phases
            phases = SIMD_ADD(phases, increments);
            phases = SIMD_SUB(phases, SIMD_FLOOR(phases));
            SIMD_STORE(temp, phases);
            voices[voiceOffset].phase = temp[0];
            if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].phase = temp[1];
            if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].phase = temp[2];
            if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].phase = temp[3];

            subPhases = SIMD_ADD(subPhases, subIncrements);
            subPhases = SIMD_SUB(subPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(subPhases, twoPi)), twoPi));
            SIMD_STORE(temp, subPhases);
            voices[voiceOffset].subPhase = temp[0];
            if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].subPhase = temp[1];
            if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].subPhase = temp[2];
            if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].subPhase = temp[3];

            SIMD_STORE(temp, lfoPhases);
            voices[voiceOffset].lfoPhase = temp[0];
            if (voiceOffset + 1 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 1].lfoPhase = temp[1];
            if (voiceOffset + 2 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 2].lfoPhase = temp[2];
            if (voiceOffset + 3 < MAX_VOICE_POLYPHONY) voices[voiceOffset + 3].lfoPhase = temp[3];
        }

        // Removed normalization; increased gain for louder synth
        outputSample *= 5.0f;

        // Prevent NaN or infinite values
        if (std::isnan(outputSample) || !std::isfinite(outputSample)) {
            outputSample = 0.0f;
        }

        // Write to output buffer
        for (int channel = 0; channel < totalNumOutputChannels; ++channel) {
            buffer.addSample(channel, sampleIndex, outputSample);
        }
        sampleIndex++;
    }

    // Advance absolute time
    currentTime += static_cast<double>(buffer.getNumSamples()) / sampleRate;
}

// Create the editor for the plugin
juce::AudioProcessorEditor* SimdSynthAudioProcessor::createEditor() {
    return new SimdSynthAudioProcessorEditor(*this);
}

// Save plugin state
void SimdSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    state.setProperty("currentProgram", currentProgram, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

// Restore plugin state
void SimdSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
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
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SimdSynthAudioProcessor();
}