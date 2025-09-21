#include "SimdSynth.h"

#ifdef __cplusplus__
extern "C" {
#endif

// Custom SIMD functions
#ifdef __arm64__
inline float32x4_t my_floor_ps(float32x4_t x) {
    float temp[4];
    vst1q_f32(temp, x);
    temp[0] = floorf(temp[0]);
    temp[1] = floorf(temp[1]);
    temp[2] = floorf(temp[2]);
    temp[3] = floorf(temp[3]);
    return vld1q_f32(temp);
}

inline float32x4_t my_sin_ps(float32x4_t x) {
    float temp[4];
    vst1q_f32(temp, x);
    temp[0] = sinf(temp[0]);
    temp[1] = sinf(temp[1]);
    temp[2] = sinf(temp[2]);
    temp[3] = sinf(temp[3]);
    return vld1q_f32(temp);
}
#endif

#ifdef __x86_64__
inline __m128 _mm_sin_ps(__m128 x) {
    float temp[4];
    _mm_store_ps(temp, x);
    temp[0] = sinf(temp[0]);
    temp[1] = sinf(temp[1]);
    temp[2] = sinf(temp[2]);
    temp[3] = sinf(temp[3]);
    return _mm_load_ps(temp);
}
#endif

// Random helper
inline float randomize(float base, float var) {
    static bool seeded = false;
    if (!seeded) {
        srand(1234);
        seeded = true;
    }
    float r = static_cast<float>(rand()) / RAND_MAX;
    return base * (1.0f - var + r * 2.0f * var);
}

// MIDI note to frequency
inline float midiToFreq(int midiNote) {
    return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
}

// 4-pole ladder filter
void applyLadderFilter(VoiceState* voices, int voiceOffset, SIMD_TYPE input, FilterState& filter, SIMD_TYPE& output, float sampleRate) {
    float cutoffs[4];
    for (int i = 0; i < 4; ++i) {
        int idx = voiceOffset + i;
        float modulatedCutoff = voices[idx].cutoff + voices[idx].filterEnv * 2000.0f;
        modulatedCutoff = std::max(20.0f, std::min(modulatedCutoff, sampleRate / 2.0f));
        cutoffs[i] = 1.0f - expf(-2.0f * M_PI * modulatedCutoff / sampleRate);
    }
#ifdef __x86_64__
    SIMD_TYPE alpha = SIMD_SET(cutoffs[0], cutoffs[1], cutoffs[2], cutoffs[3]);
#elif defined(__arm64__)
    float temp[4] = {cutoffs[0], cutoffs[1], cutoffs[2], cutoffs[3]};
    SIMD_TYPE alpha = SIMD_LOAD(temp);
#endif
    SIMD_TYPE resonance = SIMD_SET1(filter.resonance * 4.0f);

    SIMD_TYPE states[4];
    for (int i = 0; i < 4; ++i) {
        float temp[4] = { voices[voiceOffset].filterStates[i], voices[voiceOffset + 1].filterStates[i],
                          voices[voiceOffset + 2].filterStates[i], voices[voiceOffset + 3].filterStates[i] };
        states[i] = SIMD_LOAD(temp);
    }

    SIMD_TYPE feedback = SIMD_MUL(states[3], resonance);
    SIMD_TYPE filterInput = SIMD_SUB(input, feedback);

    states[0] = SIMD_ADD(states[0], SIMD_MUL(alpha, SIMD_SUB(filterInput, states[0])));
    states[1] = SIMD_ADD(states[1], SIMD_MUL(alpha, SIMD_SUB(states[0], states[1])));
    states[2] = SIMD_ADD(states[2], SIMD_MUL(alpha, SIMD_SUB(states[1], states[2])));
    states[3] = SIMD_ADD(states[3], SIMD_MUL(alpha, SIMD_SUB(states[2], states[3])));

    output = states[3];

    for (int i = 0; i < 4; ++i) {
        float temp[4];
        SIMD_STORE(temp, states[i]);
        voices[voiceOffset].filterStates[i] = temp[0];
        voices[voiceOffset + 1].filterStates[i] = temp[1];
        voices[voiceOffset + 2].filterStates[i] = temp[2];
        voices[voiceOffset + 3].filterStates[i] = temp[3];
    }
}

// Update envelopes
void updateEnvelopes(VoiceState* voices, int numVoices, float ampAttack, float ampDecay, float sampleRate, int sampleIndex, float currentTime) {
    for (int i = 0; i < numVoices; ++i) {
        if (!voices[i].active) {
            voices[i].amplitude = 0.0f;
            voices[i].filterEnv = 0.0f;
            continue;
        }
        float localTime = currentTime - voices[i].noteStartTime;

        if (localTime < ampAttack) {
            voices[i].amplitude = localTime / ampAttack;
        } else if (localTime < ampAttack + ampDecay) {
            voices[i].amplitude = 1.0f - (localTime - ampAttack) / ampDecay;
        } else {
            voices[i].amplitude = 0.0f;
            voices[i].active = false;
        }

        if (localTime < voices[i].fegAttack) {
            voices[i].filterEnv = localTime / voices[i].fegAttack;
        } else if (localTime < voices[i].fegAttack + voices[i].fegDecay) {
            voices[i].filterEnv = 1.0f - (localTime - voices[i].fegAttack) / voices[i].fegDecay * (1.0f - voices[i].fegSustain);
        } else if (localTime < ampAttack + ampDecay) {
            voices[i].filterEnv = voices[i].fegSustain;
        } else if (localTime < ampAttack + ampDecay + voices[i].fegRelease) {
            voices[i].filterEnv = voices[i].fegSustain * (1.0f - (localTime - (ampAttack + ampDecay)) / voices[i].fegRelease);
        } else {
            voices[i].filterEnv = 0.0f;
            voices[i].active = false;
        }
    }
}

#ifdef __cplusplus__
    }
#endif


// Dummy sound class
class SimdSynthSound : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

// SimdSynthVoice implementation
SimdSynthVoice::SimdSynthVoice() : sampleRate(48000.0f), ampAttack(0.1f), ampDecay(1.9f) {
    filter.resonance = 0.7f;
    for (int i = 0; i < 8; ++i) {
        states[i].cutoff = 500.0f;
        states[i].active = false;
        states[i].fegAttack = 0.1f;
        states[i].fegDecay = 1.0f;
        states[i].fegSustain = 0.5f;
        states[i].fegRelease = 0.2f;
        states[i].lfoRate = 1.0f;
        states[i].lfoDepth = 0.01f;
        states[i].lfoPhase = 0.0f;
        for (int j = 0; j < 4; ++j) {
            states[i].filterStates[j] = 0.0f;
        }
    }
    demoChords = getDebussyChords();
    demoActive = false;
    demoTime = 0.0;
    demoIndex = 0;
}

bool SimdSynthVoice::canPlaySound(juce::SynthesiserSound* sound) {
    return dynamic_cast<SimdSynthSound*>(sound) != nullptr;
}

void SimdSynthVoice::startNote(int midiNoteNumber, float /*velocity*/, juce::SynthesiserSound* /*sound*/, int /*currentPitchWheelPosition*/) {
    int voiceIdx = -1;
    double oldestTime = std::numeric_limits<float>::max();;
    for (int i = 0; i < 8; ++i) {
        if (!states[i].active) {
            voiceIdx = i;
            break;
        }
        if (states[i].noteStartTime < oldestTime) {
            oldestTime = states[i].noteStartTime;
            voiceIdx = i;
        }
    }
    if (voiceIdx >= 0) {
        states[voiceIdx].active = true;
        states[voiceIdx].frequency = midiToFreq(midiNoteNumber);
        states[voiceIdx].phaseIncrement = (2.0f * juce::MathConstants<float>::twoPi * states[voiceIdx].frequency) / sampleRate;
        states[voiceIdx].phase = 0.0f;
        states[voiceIdx].lfoPhase = 0.0f;
        states[voiceIdx].noteStartTime = demoTime;
        states[voiceIdx].fegAttack = randomize(0.1f, 0.2f);
        states[voiceIdx].fegDecay = randomize(1.0f, 0.2f);
        states[voiceIdx].fegSustain = randomize(0.5f, 0.2f);
        states[voiceIdx].fegRelease = randomize(0.2f, 0.2f);
        states[voiceIdx].lfoRate = randomize(1.0f, 0.2f);
        states[voiceIdx].lfoDepth = randomize(0.01f, 0.2f);
    }
}

void SimdSynthVoice::stopNote(float /*velocity*/, bool allowTailOff) {
    if (allowTailOff) {
        for (int i = 0; i < 8; ++i) {
            if (states[i].active) {
                states[i].active = false;
            }
        }
    } else {
        for (int i = 0; i < 8; ++i) {
            states[i].active = false;
            states[i].amplitude = 0.0f;
            states[i].filterEnv = 0.0f;
        }
    }
}

void SimdSynthVoice::pitchWheelMoved(int) {}
void SimdSynthVoice::controllerMoved(int, int) {}

void SimdSynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)  {
    juce::ScopedNoDenormals noDenormals; // Add for JUCE 8.0.2 to prevent denormalized floating-point issues
    outputBuffer.clear(startSample, numSamples); // Clear all channels efficiently
#if 0
    if (!demoActive) {
        for (const auto& metadata : midiMessages) {
            auto m = metadata.getMessage();
            if (m.isNoteOn()) {
                startNote(m.getNoteNumber(), m.getFloatVelocity(), nullptr, 0);
            } else if (m.isNoteOff()) {
                stopNote(m.getFloatVelocity(), true);
            }
        }
    }
#endif

    if (demoActive) {
        float currentTime = demoTime + (startSample / sampleRate);
        if (demoIndex < demoChords.size() && currentTime >= demoChords[demoIndex].startTime + demoChords[demoIndex].duration) {
            demoIndex++;
            if (demoIndex < demoChords.size()) {
                for (size_t v = 0; v < 8; ++v) { // Use size_t for sign safety
                    states[v].active = v < demoChords[demoIndex].frequencies.size();
                    if (states[v].active) {
                        states[v].frequency = demoChords[demoIndex].frequencies[v];
                        states[v].phaseIncrement = (2.0f * juce::MathConstants<float>::pi * states[v].frequency) / sampleRate;
                        states[v].phase = 0.0f;
                        states[v].lfoPhase = 0.0f;
                        states[v].noteStartTime = currentTime;
                        states[v].fegAttack = randomize(0.1f, 0.2f);
                        states[v].fegDecay = randomize(1.0f, 0.2f);
                        states[v].fegSustain = randomize(0.5f, 0.2f);
                        states[v].fegRelease = randomize(0.2f, 0.2f);
                        states[v].lfoRate = randomize(1.0f, 0.2f);
                        states[v].lfoDepth = randomize(0.01f, 0.2f);
                    }
                }
            }
        }
        demoTime += (numSamples / sampleRate);
    }

    for (int sample = 0; sample < numSamples; ++sample) {
        int globalIndex = startSample + sample;
        float currentTime = globalIndex / sampleRate;

        updateEnvelopes(states, 8, ampAttack, ampDecay, sampleRate, sample, currentTime);

        float outputSample = 0.0f;
        for (int group = 0; group < 2; ++group) {
            int offset = group * 4;

            float amps[4] = { states[offset].amplitude, states[offset + 1].amplitude,
                              states[offset + 2].amplitude, states[offset + 3].amplitude };
            float phases[4] = { states[offset].phase, states[offset + 1].phase,
                                states[offset + 2].phase, states[offset + 3].phase };
            float increments[4] = { states[offset].phaseIncrement, states[offset + 1].phaseIncrement,
                                    states[offset + 2].phaseIncrement, states[offset + 3].phaseIncrement };
            float lfoPhases[4] = { states[offset].lfoPhase, states[offset + 1].lfoPhase,
                                   states[offset + 2].lfoPhase, states[offset + 3].lfoPhase };
            float lfoRates[4] = { states[offset].lfoRate, states[offset + 1].lfoRate,
                                  states[offset + 2].lfoRate, states[offset + 3].lfoRate };
            float lfoDepths[4] = { states[offset].lfoDepth, states[offset + 1].lfoDepth,
                                   states[offset + 2].lfoDepth, states[offset + 3].lfoDepth };

            SIMD_TYPE amplitudes = SIMD_LOAD(amps);
            SIMD_TYPE phasesSIMD = SIMD_LOAD(phases);
            SIMD_TYPE incrementsSIMD = SIMD_LOAD(increments);
            SIMD_TYPE lfoPhasesSIMD = SIMD_LOAD(lfoPhases);
            SIMD_TYPE lfoRatesSIMD = SIMD_LOAD(lfoRates);
            SIMD_TYPE lfoDepthsSIMD = SIMD_LOAD(lfoDepths);

            SIMD_TYPE twoPi = SIMD_SET1(2.0f * juce::MathConstants<float>::pi);
            SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRatesSIMD, SIMD_SET1(2.0f * juce::MathConstants<float>::pi / sampleRate));
            lfoPhasesSIMD = SIMD_ADD(lfoPhasesSIMD, lfoIncrements);
            lfoPhasesSIMD = SIMD_SUB(lfoPhasesSIMD, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhasesSIMD, twoPi)), twoPi));
            SIMD_TYPE lfoValues = SIMD_SIN(lfoPhasesSIMD);
            lfoValues = SIMD_MUL(lfoValues, lfoDepthsSIMD);
            phasesSIMD = SIMD_ADD(phasesSIMD, lfoValues);

            SIMD_TYPE sinValues = SIMD_SIN(phasesSIMD);
            sinValues = SIMD_MUL(sinValues, amplitudes);

            SIMD_TYPE filtered;
            applyLadderFilter(states, offset, sinValues, filter, filtered, sampleRate);

            float temp[4];
            SIMD_STORE(temp, filtered);
            outputSample += (temp[0] + temp[1] + temp[2] + temp[3]) * 0.125f;

            phasesSIMD = SIMD_ADD(phasesSIMD, incrementsSIMD);
            SIMD_TYPE wrap = SIMD_SUB(phasesSIMD, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(phasesSIMD, twoPi)), twoPi));
            SIMD_STORE(temp, wrap);
            states[offset].phase = temp[0];
            states[offset + 1].phase = temp[1];
            states[offset + 2].phase = temp[2];
            states[offset + 3].phase = temp[3];

            SIMD_STORE(temp, lfoPhasesSIMD);
            states[offset].lfoPhase = temp[0];
            states[offset + 1].lfoPhase = temp[1];
            states[offset + 2].lfoPhase = temp[2];
            states[offset + 3].lfoPhase = temp[3];
        }

        if (outputBuffer.getNumChannels() > 0) {
            outputBuffer.addSample(0, startSample + sample, outputSample);
            for (int ch = 1; ch < outputBuffer.getNumChannels(); ++ch) {
                outputBuffer.addSample(ch, startSample + sample, outputSample);
            }
        }
    }
}

void SimdSynthVoice::renderNextBlock(juce::AudioBuffer<double>& outputBuffer, int startSample, int numSamples) {
    juce::ScopedNoDenormals noDenormals; // Add for JUCE 8.0.2
    outputBuffer.clear(startSample, numSamples); // Clear all channels
#if 0
    if (!demoActive) {
        for (const auto& metadata : midiMessages) {
            auto m = metadata.getMessage();
            if (m.isNoteOn()) {
                startNote(m.getNoteNumber(), m.getFloatVelocity(), nullptr, 0);
            } else if (m.isNoteOff()) {
                stopNote(m.getFloatVelocity(), true);
            }
        }
    }
#endif

    if (demoActive) {
        float currentTime = demoTime + (startSample / sampleRate);
        if (demoIndex < demoChords.size() && currentTime >= demoChords[demoIndex].startTime + demoChords[demoIndex].duration) {
            demoIndex++;
            if (demoIndex < demoChords.size()) {
                for (size_t v = 0; v < 8; ++v) {
                    states[v].active = v < demoChords[demoIndex].frequencies.size();
                    if (states[v].active) {
                        states[v].frequency = demoChords[demoIndex].frequencies[v];
                        states[v].phaseIncrement = (2.0f * juce::MathConstants<float>::pi * states[v].frequency) / sampleRate;
                        states[v].phase = 0.0f;
                        states[v].lfoPhase = 0.0f;
                        states[v].noteStartTime = currentTime;
                        states[v].fegAttack = randomize(0.1f, 0.2f);
                        states[v].fegDecay = randomize(1.0f, 0.2f);
                        states[v].fegSustain = randomize(0.5f, 0.2f);
                        states[v].fegRelease = randomize(0.2f, 0.2f);
                        states[v].lfoRate = randomize(1.0f, 0.2f);
                        states[v].lfoDepth = randomize(0.01f, 0.2f);
                    }
                }
            }
        }
        demoTime += (numSamples / sampleRate);
    }

    for (int sample = 0; sample < numSamples; ++sample) {
        int globalIndex = startSample + sample;
        float currentTime = globalIndex / sampleRate;

        updateEnvelopes(states, 8, ampAttack, ampDecay, sampleRate, sample, currentTime);

        double outputSample = 0.0; // Use double for precision
        for (int group = 0; group < 2; ++group) {
            int offset = group * 4;

            float amps[4] = { states[offset].amplitude, states[offset + 1].amplitude,
                              states[offset + 2].amplitude, states[offset + 3].amplitude };
            float phases[4] = { states[offset].phase, states[offset + 1].phase,
                                states[offset + 2].phase, states[offset + 3].phase };
            float increments[4] = { states[offset].phaseIncrement, states[offset + 1].phaseIncrement,
                                    states[offset + 2].phaseIncrement, states[offset + 3].phaseIncrement };
            float lfoPhases[4] = { states[offset].lfoPhase, states[offset + 1].lfoPhase,
                                   states[offset + 2].lfoPhase, states[offset + 3].lfoPhase };
            float lfoRates[4] = { states[offset].lfoRate, states[offset + 1].lfoRate,
                                  states[offset + 2].lfoRate, states[offset + 3].lfoRate };
            float lfoDepths[4] = { states[offset].lfoDepth, states[offset + 1].lfoDepth,
                                   states[offset + 2].lfoDepth, states[offset + 3].lfoDepth };

            SIMD_TYPE amplitudes = SIMD_LOAD(amps);
            SIMD_TYPE phasesSIMD = SIMD_LOAD(phases);
            SIMD_TYPE incrementsSIMD = SIMD_LOAD(increments);
            SIMD_TYPE lfoPhasesSIMD = SIMD_LOAD(lfoPhases);
            SIMD_TYPE lfoRatesSIMD = SIMD_LOAD(lfoRates);
            SIMD_TYPE lfoDepthsSIMD = SIMD_LOAD(lfoDepths);

            SIMD_TYPE twoPi = SIMD_SET1(2.0f * juce::MathConstants<float>::pi);
            SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRatesSIMD, SIMD_SET1(2.0f * juce::MathConstants<float>::pi / sampleRate));
            lfoPhasesSIMD = SIMD_ADD(lfoPhasesSIMD, lfoIncrements);
            lfoPhasesSIMD = SIMD_SUB(lfoPhasesSIMD, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhasesSIMD, twoPi)), twoPi));
            SIMD_TYPE lfoValues = SIMD_SIN(lfoPhasesSIMD);
            lfoValues = SIMD_MUL(lfoValues, lfoDepthsSIMD);
            phasesSIMD = SIMD_ADD(phasesSIMD, lfoValues);

            SIMD_TYPE sinValues = SIMD_SIN(phasesSIMD);
            sinValues = SIMD_MUL(sinValues, amplitudes);

            SIMD_TYPE filtered;
            applyLadderFilter(states, offset, sinValues, filter, filtered, sampleRate);

            float temp[4];
            SIMD_STORE(temp, filtered);
            outputSample += (temp[0] + temp[1] + temp[2] + temp[3]) * 0.125;

            phasesSIMD = SIMD_ADD(phasesSIMD, incrementsSIMD);
            SIMD_TYPE wrap = SIMD_SUB(phasesSIMD, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(phasesSIMD, twoPi)), twoPi));
            SIMD_STORE(temp, wrap);
            states[offset].phase = temp[0];
            states[offset + 1].phase = temp[1];
            states[offset + 2].phase = temp[2];
            states[offset + 3].phase = temp[3];

            SIMD_STORE(temp, lfoPhasesSIMD);
            states[offset].lfoPhase = temp[0];
            states[offset + 1].lfoPhase = temp[1];
            states[offset + 2].lfoPhase = temp[2];
            states[offset + 3].lfoPhase = temp[3];
        }

        if (outputBuffer.getNumChannels() > 0) {
            outputBuffer.addSample(0, startSample + sample, outputSample);
            for (int ch = 1; ch < outputBuffer.getNumChannels(); ++ch) {
                outputBuffer.addSample(ch, startSample + sample, outputSample);
            }
        }
    }
}

std::vector<Chord> SimdSynthVoice::getDebussyChords() {
    std::vector<Chord> chords;
    chords.emplace_back(Chord{{midiToFreq(49), midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63)}, 0.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(54), midiToFreq(58), midiToFreq(61), midiToFreq(65)}, 2.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68)}, 4.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67)}, 6.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67)}, 8.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(51), midiToFreq(55), midiToFreq(58), midiToFreq(62), midiToFreq(65)}, 10.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(60), midiToFreq(63), midiToFreq(67), midiToFreq(70)}, 12.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(54), midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68)}, 14.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(61), midiToFreq(65), midiToFreq(68), midiToFreq(72)}, 16.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68), midiToFreq(72)}, 18.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63)}, 20.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67), midiToFreq(70)}, 22.0f, 2.0f});
    return chords;
}

void SimdSynthVoice::setDemoMode(bool enabled) {
    demoActive = enabled;
    if (enabled) {
        demoTime = 0.0;
        demoIndex = 0;
    }
}

// SimdSynth implementation
SimdSynth::SimdSynth() {
    for (int i = 0; i < 8; ++i) {
        synth.addVoice(new SimdSynthVoice());
    }
    synth.addSound(new SimdSynthSound());
}

SimdSynth::~SimdSynth() {
    synth.clearVoices();
    synth.clearSounds();
}

void SimdSynth::prepareToPlay(double sampleRate, int samplesPerBlock) {
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void SimdSynth::releaseResources() {
    synth.clearVoices();
    synth.clearSounds();
}

void SimdSynth::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = buffer.getNumChannels();
    auto totalNumOutputChannels = buffer.getNumChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}
