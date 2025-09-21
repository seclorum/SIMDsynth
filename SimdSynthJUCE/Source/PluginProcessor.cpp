#include "PluginProcessor.h"

void SimdSynthAudioProcessor::initWavetables() {
    for (int i = 0; i < WAVETABLE_SIZE; ++i) {
        sineTable[i] = sinf(2.0f * M_PI * i / WAVETABLE_SIZE);
        sawTable[i] = 2.0f * (i / static_cast<float>(WAVETABLE_SIZE)) - 1.0f;
    }
}

float SimdSynthAudioProcessor::midiToFreq(int midiNote) {
    return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
}

float SimdSynthAudioProcessor::randomize(float base, float var) {
    float r = static_cast<float>(rand()) / RAND_MAX;
    return base * (1.0f - var + r * 2.0f * var);
}

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

#ifdef __x86_64__
__m128 SimdSynthAudioProcessor::fast_sin_ps(__m128 x) {
    const __m128 twoPi = _mm_set1_ps(2.0f * M_PI);
    const __m128 invTwoPi = _mm_set1_ps(1.0f / (2.0f * M_PI));
    const __m128 piOverTwo = _mm_set1_ps(M_PI / 2.0f);

    __m128 q = _mm_mul_ps(x, invTwoPi);
    q = _mm_floor_ps(q);
    __m128 xWrapped = _mm_sub_ps(x, _mm_mul_ps(q, twoPi));

    __m128 sign = _mm_set1_ps(1.0f);
    __m128 absX = _mm_max_ps(xWrapped, _mm_sub_ps(_mm_setzero_ps(), xWrapped));
    __m128 gtPiOverTwo = _mm_cmpgt_ps(absX, piOverTwo);
    sign = _mm_or_ps(_mm_and_ps(gtPiOverTwo, _mm_set1_ps(-1.0f)),
                     _mm_andnot_ps(gtPiOverTwo, _mm_set1_ps(1.0f)));
    xWrapped = _mm_sub_ps(xWrapped,
                          _mm_and_ps(gtPiOverTwo,
                                     _mm_mul_ps(piOverTwo, _mm_set1_ps(2.0f))));

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

#ifdef __arm64__
float32x4_t SimdSynthAudioProcessor::fast_sin_ps(float32x4_t x) {
    const float32x4_t twoPi = vdupq_n_f32(2.0f * M_PI);
    const float32x4_t invTwoPi = vdupq_n_f32(1.0f / (2.0f * M_PI));
    const float32x4_t piOverTwo = vdupq_n_f32(M_PI / 2.0f);

    float32x4_t q = vmulq_f32(x, invTwoPi);
    q = my_floor_ps(q);
    float32x4_t xWrapped = vsubq_f32(x, vmulq_f32(q, twoPi));

    float32x4_t sign = vdupq_n_f32(1.0f);
    float32x4_t absX = vmaxq_f32(xWrapped, vsubq_f32(vdupq_n_f32(0.0f), xWrapped));
    uint32x4_t gtPiOverTwo = vcgtq_f32(absX, piOverTwo);
    sign = vbslq_f32(gtPiOverTwo, vdupq_n_f32(-1.0f), vdupq_n_f32(1.0f));
    xWrapped = vsubq_f32(xWrapped,
                         vbslq_f32(gtPiOverTwo,
                                   vmulq_f32(piOverTwo, vdupq_n_f32(2.0f)),
                                   vdupq_n_f32(0.0f)));

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

SIMD_TYPE SimdSynthAudioProcessor::wavetable_lookup_ps(SIMD_TYPE phase, const float* table) {
    const SIMD_TYPE tableSize = SIMD_SET1(static_cast<float>(WAVETABLE_SIZE));
    SIMD_TYPE index = SIMD_MUL(phase, tableSize);
    SIMD_TYPE indexFloor = SIMD_FLOOR(index);
    SIMD_TYPE frac = SIMD_SUB(index, indexFloor);
    indexFloor = SIMD_SUB(indexFloor, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(indexFloor, tableSize)), tableSize));

    float tempIndices[4];
    SIMD_STORE(tempIndices, indexFloor);
    int indices[4] = {
        static_cast<int>(tempIndices[0]),
        static_cast<int>(tempIndices[1]),
        static_cast<int>(tempIndices[2]),
        static_cast<int>(tempIndices[3])
    };

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

void SimdSynthAudioProcessor::applyLadderFilter(Voice* voices, int voiceOffset, SIMD_TYPE input, Filter& filter, SIMD_TYPE& output) {
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
    for (int i = 0; i < 4; i++) {
        float temp[4] = {
            voices[voiceOffset].filterStates[i],
            voices[voiceOffset + 1].filterStates[i],
            voices[voiceOffset + 2].filterStates[i],
            voices[voiceOffset + 3].filterStates[i]
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
            tempCheck[i] = std::max(-1.0f, std::min(1.0f, tempCheck[i]));
        }
    }
    output = SIMD_LOAD(tempCheck);

    float tempOut[4];
    SIMD_STORE(tempOut, output);
    if (std::isnan(tempOut[0]) || std::isnan(tempOut[1]) ||
        std::isnan(tempOut[2]) || std::isnan(tempOut[3])) {
        std::cerr << "Filter output nan at voiceOffset " << voiceOffset << ": {"
                  << tempOut[0] << ", " << tempOut[1] << ", " << tempOut[2]
                  << ", " << tempOut[3] << "}" << std::endl;
    }

    for (int i = 0; i < 4; i++) {
        float temp[4];
        SIMD_STORE(temp, states[i]);
        voices[voiceOffset].filterStates[i] = temp[0];
        voices[voiceOffset + 1].filterStates[i] = temp[1];
        voices[voiceOffset + 2].filterStates[i] = temp[2];
        voices[voiceOffset + 3].filterStates[i] = temp[3];
    }
}

void SimdSynthAudioProcessor::updateEnvelopes(int sampleIndex) {
    float attackTime = *attackTimeParam;
    float decayTime = *decayTimeParam;
    float chordDuration = 2.0f; // Fixed duration for MIDI-triggered notes
    float t = sampleIndex / filter.sampleRate;

    for (int i = 0; i < MAX_VOICE_POLYPHONY; i++) {
        if (!voices[i].active) {
            voices[i].amplitude = 0.0f;
            voices[i].filterEnv = 0.0f;
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] = 0.0f;
            }
            continue;
        }

        float localTime = t - voices[i].noteOnTime;

        if (localTime < attackTime) {
            voices[i].amplitude = localTime / attackTime;
        } else if (localTime < attackTime + decayTime) {
            voices[i].amplitude = 1.0f - (localTime - attackTime) / decayTime;
        } else {
            voices[i].amplitude = 0.0f;
            voices[i].active = false;
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] = 0.0f;
            }
        }

        if (localTime < voices[i].fegAttack) {
            voices[i].filterEnv = localTime / voices[i].fegAttack;
        } else if (localTime < voices[i].fegAttack + voices[i].fegDecay) {
            voices[i].filterEnv = 1.0f - (localTime - voices[i].fegAttack) /
                                        voices[i].fegDecay * (1.0f - voices[i].fegSustain);
        } else if (localTime < chordDuration) {
            voices[i].filterEnv = voices[i].fegSustain;
        } else if (localTime < chordDuration + voices[i].fegRelease) {
            voices[i].filterEnv = voices[i].fegSustain * (1.0f - (localTime - chordDuration) / voices[i].fegRelease);
        } else {
            voices[i].filterEnv = 0.0f;
            voices[i].active = false;
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] = 0.0f;
            }
        }
    }
}

SimdSynthAudioProcessor::SimdSynthAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("SimdSynth"),
                 {
                     std::make_unique<juce::AudioParameterFloat>("wavetable", "Wavetable Type", 0.0f, 1.0f, 0.0f),
                     std::make_unique<juce::AudioParameterFloat>("attack", "Attack Time", 0.01f, 2.0f, 0.1f),
                     std::make_unique<juce::AudioParameterFloat>("decay", "Decay Time", 0.1f, 5.0f, 1.9f),
                     std::make_unique<juce::AudioParameterFloat>("cutoff", "Filter Cutoff", 200.0f, 8000.0f, 1000.0f),
                     std::make_unique<juce::AudioParameterFloat>("resonance", "Filter Resonance", 0.0f, 1.0f, 0.7f),
                     std::make_unique<juce::AudioParameterFloat>("fegAttack", "Filter EG Attack", 0.01f, 2.0f, 0.1f),
                     std::make_unique<juce::AudioParameterFloat>("fegDecay", "Filter EG Decay", 0.1f, 5.0f, 1.0f),
                     std::make_unique<juce::AudioParameterFloat>("fegSustain", "Filter EG Sustain", 0.0f, 1.0f, 0.5f),
                     std::make_unique<juce::AudioParameterFloat>("fegRelease", "Filter EG Release", 0.01f, 2.0f, 0.2f),
                     std::make_unique<juce::AudioParameterFloat>("lfoRate", "LFO Rate", 0.0f, 20.0f, 1.0f),
                     std::make_unique<juce::AudioParameterFloat>("lfoDepth", "LFO Depth", 0.0f, 0.1f, 0.01f),
                     std::make_unique<juce::AudioParameterFloat>("subTune", "Sub Osc Tune", -24.0f, 0.0f, -12.0f),
                     std::make_unique<juce::AudioParameterFloat>("subMix", "Sub Osc Mix", 0.0f, 1.0f, 0.5f),
                     std::make_unique<juce::AudioParameterFloat>("subTrack", "Sub Osc Track", 0.0f, 1.0f, 1.0f)
                 }) {
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

    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i] = Voice();
        voices[i].active = false;
        voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
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

    filter.resonance = *resonanceParam;
    filter.sampleRate = 48000.0f;

    initWavetables();
}

SimdSynthAudioProcessor::~SimdSynthAudioProcessor() {}

void SimdSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    filter.sampleRate = static_cast<float>(sampleRate);
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i].active = false;
        for (int j = 0; j < 4; ++j) {
            voices[i].filterStates[j] = 0.0f;
        }
    }
}

void SimdSynthAudioProcessor::releaseResources() {}

void SimdSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    buffer.clear();

    // Update parameters
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
        voices[i].wavetableType = static_cast<int>(*wavetableTypeParam);
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
    filter.resonance = *resonanceParam;

    // Process MIDI
    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            int note = msg.getNoteNumber();
            float velocity = msg.getVelocity() / 127.0f;
            for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
                if (!voices[i].active) {
                    voices[i].active = true;
                    voices[i].frequency = midiToFreq(note);
                    voices[i].phaseIncrement = voices[i].frequency / filter.sampleRate;
                    voices[i].phase = 0.0f;
                    voices[i].lfoPhase = 0.0f;
                    voices[i].amplitude = velocity;
                    voices[i].noteNumber = note;
                    voices[i].velocity = msg.getVelocity();
                    voices[i].noteOnTime = getSampleRate() * metadata.samplePosition;
                    float subFreq = voices[i].frequency * powf(2.0f, voices[i].subTune / 12.0f) * voices[i].subTrack;
                    voices[i].subFrequency = subFreq;
                    voices[i].subPhaseIncrement = (2.0f * M_PI * subFreq) / filter.sampleRate;
                    voices[i].subPhase = 0.0f;
                    break;
                }
            }
        } else if (msg.isNoteOff()) {
            int note = msg.getNoteNumber();
            for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i) {
                if (voices[i].active && voices[i].noteNumber == note) {
                    voices[i].active = false;
                    voices[i].amplitude = 0.0f;
                    voices[i].filterEnv = 0.0f;
                    for (int j = 0; j < 4; ++j) {
                        voices[i].filterStates[j] = 0.0f;
                    }
                }
            }
        }
    }

    // Process audio
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        updateEnvelopes(sample);
        float outputSample = 0.0f;
        const SIMD_TYPE twoPi = SIMD_SET1(2.0f * M_PI);

        for (int group = 0; group < (MAX_VOICE_POLYPHONY + 3) / 4; group++) {
            int voiceOffset = group * 4;
            if (voiceOffset >= MAX_VOICE_POLYPHONY) continue;

            float tempAmps[4] = {
                voices[voiceOffset].amplitude,
                voices[voiceOffset + 1].amplitude,
                voices[voiceOffset + 2].amplitude,
                voices[voiceOffset + 3].amplitude
            };
            float tempPhases[4] = {
                voices[voiceOffset].phase,
                voices[voiceOffset + 1].phase,
                voices[voiceOffset + 2].phase,
                voices[voiceOffset + 3].phase
            };
            float tempIncrements[4] = {
                voices[voiceOffset].phaseIncrement,
                voices[voiceOffset + 1].phaseIncrement,
                voices[voiceOffset + 2].phaseIncrement,
                voices[voiceOffset + 3].phaseIncrement
            };
            float tempLfoPhases[4] = {
                voices[voiceOffset].lfoPhase,
                voices[voiceOffset + 1].lfoPhase,
                voices[voiceOffset + 2].lfoPhase,
                voices[voiceOffset + 3].lfoPhase
            };
            float tempLfoRates[4] = {
                voices[voiceOffset].lfoRate,
                voices[voiceOffset + 1].lfoRate,
                voices[voiceOffset + 2].lfoRate,
                voices[voiceOffset + 3].lfoRate
            };
            float tempLfoDepths[4] = {
                voices[voiceOffset].lfoDepth,
                voices[voiceOffset + 1].lfoDepth,
                voices[voiceOffset + 2].lfoDepth,
                voices[voiceOffset + 3].lfoDepth
            };
            float tempSubPhases[4] = {
                voices[voiceOffset].subPhase,
                voices[voiceOffset + 1].subPhase,
                voices[voiceOffset + 2].subPhase,
                voices[voiceOffset + 3].subPhase
            };
            float tempSubIncrements[4] = {
                voices[voiceOffset].subPhaseIncrement,
                voices[voiceOffset + 1].subPhaseIncrement,
                voices[voiceOffset + 2].subPhaseIncrement,
                voices[voiceOffset + 3].subPhaseIncrement
            };
            float tempSubMixes[4] = {
                voices[voiceOffset].subMix,
                voices[voiceOffset + 1].subMix,
                voices[voiceOffset + 2].subMix,
                voices[voiceOffset + 3].subMix
            };
            int wavetableTypes[4] = {
                voices[voiceOffset].wavetableType,
                voices[voiceOffset + 1].wavetableType,
                voices[voiceOffset + 2].wavetableType,
                voices[voiceOffset + 3].wavetableType
            };

            SIMD_TYPE amplitudes = SIMD_LOAD(tempAmps);
            SIMD_TYPE phases = SIMD_LOAD(tempPhases);
            SIMD_TYPE increments = SIMD_LOAD(tempIncrements);
            SIMD_TYPE lfoPhases = SIMD_LOAD(tempLfoPhases);
            SIMD_TYPE lfoRates = SIMD_LOAD(tempLfoRates);
            SIMD_TYPE lfoDepths = SIMD_LOAD(tempLfoDepths);
            SIMD_TYPE subPhases = SIMD_LOAD(tempSubPhases);
            SIMD_TYPE subIncrements = SIMD_LOAD(tempSubIncrements);
            SIMD_TYPE subMixes = SIMD_LOAD(tempSubMixes);

            SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRates, SIMD_SET1(2.0f * M_PI / filter.sampleRate));
            lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
            lfoPhases = SIMD_SUB(lfoPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)), twoPi));
            SIMD_TYPE lfoValues = SIMD_SIN(lfoPhases);
            lfoValues = SIMD_MUL(lfoValues, lfoDepths);
            phases = SIMD_ADD(phases, SIMD_DIV(lfoValues, twoPi));

            SIMD_TYPE mainValues = SIMD_SET1(0.0f);
            for (int j = 0; j < 4; ++j) {
                int idx = voiceOffset + j;
                if (idx >= MAX_VOICE_POLYPHONY || !voices[idx].active) continue;

                float phase = tempPhases[j];
                phase = phase - floorf(phase);
                float value;
                if (wavetableTypes[j] == 0) {
                    value = wavetable_lookup_ps(SIMD_SET1(phase), sineTable)[j % 4];
                } else {
                    value = wavetable_lookup_ps(SIMD_SET1(phase), sawTable)[j % 4];
                }
                float tempMain[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                tempMain[j] = value;
                mainValues = SIMD_ADD(mainValues, SIMD_LOAD(tempMain));
            }
            mainValues = SIMD_MUL(mainValues, amplitudes);
            mainValues = SIMD_MUL(mainValues, SIMD_SUB(SIMD_SET1(1.0f), subMixes));

            SIMD_TYPE subSinValues = SIMD_SIN(subPhases);
            subSinValues = SIMD_MUL(subSinValues, amplitudes);
            subSinValues = SIMD_MUL(subSinValues, subMixes);

            SIMD_TYPE combinedValues = SIMD_ADD(mainValues, subSinValues);

            SIMD_TYPE filteredOutput;
            applyLadderFilter(voices, voiceOffset, combinedValues, filter, filteredOutput);

            float temp[4];
            SIMD_STORE(temp, filteredOutput);
            outputSample += (temp[0] + temp[1] + temp[2] + temp[3]);

            phases = SIMD_ADD(phases, increments);
            phases = SIMD_SUB(phases, SIMD_FLOOR(phases));
            SIMD_STORE(temp, phases);
            voices[voiceOffset].phase = temp[0];
            voices[voiceOffset + 1].phase = temp[1];
            voices[voiceOffset + 2].phase = temp[2];
            voices[voiceOffset + 3].phase = temp[3];

            subPhases = SIMD_ADD(subPhases, subIncrements);
            subPhases = SIMD_SUB(subPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(subPhases, twoPi)), twoPi));
            SIMD_STORE(temp, subPhases);
            voices[voiceOffset].subPhase = temp[0];
            voices[voiceOffset + 1].subPhase = temp[1];
            voices[voiceOffset + 2].subPhase = temp[2];
            voices[voiceOffset + 3].subPhase = temp[3];

            SIMD_STORE(temp, lfoPhases);
            voices[voiceOffset].lfoPhase = temp[0];
            voices[voiceOffset + 1].lfoPhase = temp[1];
            voices[voiceOffset + 2].lfoPhase = temp[2];
            voices[voiceOffset + 3].lfoPhase = temp[3];
        }

        outputSample *= 0.5f;

        if (std::isnan(outputSample) || !std::isfinite(outputSample)) {
            outputSample = 0.0f;
        }

        for (int channel = 0; channel < totalNumOutputChannels; ++channel) {
            buffer.addSample(channel, sample, outputSample);
        }
    }
}

juce::AudioProcessorEditor* SimdSynthAudioProcessor::createEditor() {
    return new juce::GenericAudioProcessorEditor(*this);
}

void SimdSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SimdSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr) {
        juce::ValueTree state = juce::ValueTree::fromXml(*xmlState);
        parameters.replaceState(state);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SimdSynthAudioProcessor();
}
