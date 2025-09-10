#include <iostream>
#include <cmath>
#include <cstdint>
#include <vector>

#ifdef __x86_64__
#include <xmmintrin.h> // SSE for x86/x64
#define SIMD_TYPE __m128
#define SIMD_SET1 _mm_set1_ps
#define SIMD_SET _mm_set_ps
#define SIMD_LOAD _mm_load_ps
#define SIMD_ADD _mm_add_ps
#define SIMD_SUB _mm_sub_ps
#define SIMD_MUL _mm_mul_ps
#define SIMD_DIV _mm_div_ps
#define SIMD_FLOOR _mm_floor_ps
#define SIMD_STORE _mm_store_ps
#define SIMD_SIN _mm_sin_ps
#elif defined(__arm64__)
#include <arm_neon.h> // NEON for ARM
#define SIMD_TYPE float32x4_t
#define SIMD_SET1 vdupq_n_f32
#define SIMD_SET vld1q_f32
#define SIMD_LOAD vld1q_f32
#define SIMD_ADD vaddq_f32
#define SIMD_SUB vsubq_f32
#define SIMD_MUL vmulq_f32
#define SIMD_DIV vdivq_f32
#define SIMD_FLOOR my_floor_ps
#define SIMD_STORE vst1q_f32
#define SIMD_SIN my_sin_ps
#else
#error "Unsupported architecture: Requires x86_64 or arm64"
#endif

// Simulate a synthesizer voice
struct Voice {
    float frequency;      // Oscillator frequency (Hz)
    float phase;         // Oscillator phase (radians)
    float phaseIncrement;// Per-sample phase increment
    float amplitude;     // Oscillator amplitude (from envelope)
    float cutoff;        // Filter cutoff frequency (Hz)
    float filterEnv;     // Filter envelope amount
    float filterStates[4]; // 4-pole filter states
    bool active;         // Voice active state
};

// Filter state
struct Filter {
    float resonance; // Resonance (0 to 1)
    float sampleRate; // Sample rate (Hz)
};

// Chord structure
struct Chord {
    std::vector<float> frequencies; // Frequencies for the chord
    float startTime; // Start time in seconds
    float duration;  // Duration in seconds
};

// Update amplitude and filter envelopes
void updateEnvelopes(Voice* voices, int numVoices, float attackTime, float decayTime, float sampleRate, int sampleIndex, float currentTime) {
    float t = sampleIndex / sampleRate; // Time in seconds
    for (int i = 0; i < numVoices; i++) {
        if (!voices[i].active) {
            voices[i].amplitude = 0.0f;
            voices[i].filterEnv = 0.0f;
            continue;
        }
        float localTime = t - currentTime; // Time relative to chord start
        if (localTime < attackTime) {
            voices[i].amplitude = localTime / attackTime; // Linear attack
            voices[i].filterEnv = localTime / attackTime;
        } else if (localTime < attackTime + decayTime) {
            voices[i].amplitude = 1.0f - (localTime - attackTime) / decayTime; // Linear decay
            voices[i].filterEnv = 1.0f - (localTime - attackTime) / decayTime;
        } else {
            voices[i].amplitude = 0.0f;
            voices[i].filterEnv = 0.0f;
            voices[i].active = false;
        }
    }
}

// Custom floor for NEON
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
#endif

// Custom sine for NEON/SSE (placeholder)
#ifdef __arm64__
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

// Compute 4-pole ladder filter for 4 voices
void applyLadderFilter(Voice* voices, int voiceOffset, SIMD_TYPE input, Filter& filter, SIMD_TYPE& output) {
    // Compute alpha = 1 - e^(-2π * cutoff / sampleRate)
    float cutoffs[4];
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        float modulatedCutoff = voices[idx].cutoff + voices[idx].filterEnv * 2000.0f;
        modulatedCutoff = std::max(20.0f, std::min(modulatedCutoff, filter.sampleRate / 2.0f));
        cutoffs[i] = 1.0f - expf(-2.0f * M_PI * modulatedCutoff / filter.sampleRate);
    }
    SIMD_TYPE alpha = SIMD_SET(cutoffs);
    SIMD_TYPE oneMinusAlpha = SIMD_SUB(SIMD_SET1(1.0f), alpha);
    SIMD_TYPE resonance = SIMD_SET1(filter.resonance * 4.0f);

    // Load filter states
    SIMD_TYPE states[4];
    for (int i = 0; i < 4; i++) {
        float temp[4] = { voices[voiceOffset].filterStates[i], voices[voiceOffset + 1].filterStates[i],
                          voices[voiceOffset + 2].filterStates[i], voices[voiceOffset + 3].filterStates[i] };
        states[i] = SIMD_LOAD(temp);
    }

    // Apply feedback
    SIMD_TYPE feedback = SIMD_MUL(states[3], resonance);
    SIMD_TYPE filterInput = SIMD_SUB(input, feedback);

    // Cascade four one-pole filters
    states[0] = SIMD_ADD(states[0], SIMD_MUL(alpha, SIMD_SUB(filterInput, states[0])));
    states[1] = SIMD_ADD(states[1], SIMD_MUL(alpha, SIMD_SUB(states[0], states[1])));
    states[2] = SIMD_ADD(states[2], SIMD_MUL(alpha, SIMD_SUB(states[1], states[2])));
    states[3] = SIMD_ADD(states[3], SIMD_MUL(alpha, SIMD_SUB(states[2], states[3])));

    // Output is the final stage
    output = states[3];

    // Store updated states
    for (int i = 0; i < 4; i++) {
        float temp[4];
        SIMD_STORE(temp, states[i]);
        voices[voiceOffset].filterStates[i] = temp[0];
        voices[voiceOffset + 1].filterStates[i] = temp[1];
        voices[voiceOffset + 2].filterStates[i] = temp[2];
        voices[voiceOffset + 3].filterStates[i] = temp[3];
    }
}

// Generate sine waves and apply filter for 8 voices
void generateSineSamples(Voice* voices, int numSamples, Filter& filter, const std::vector<Chord>& chords) {
    const SIMD_TYPE twoPi = SIMD_SET1(2.0f * M_PI);
    const SIMD_TYPE one = SIMD_SET1(1.0f);
    float attackTime = 0.1f; // 100 ms attack
    float decayTime = 1.9f;  // 1.9 s decay
    float currentTime = 0.0f;
    int currentChord = 0;

    for (int i = 0; i < numSamples; i++) {
        float t = i / filter.sampleRate;

        // Update chord if needed
        if (currentChord < chords.size() && t >= chords[currentChord].startTime + chords[currentChord].duration) {
            currentChord++;
            currentTime = t;
            if (currentChord < chords.size()) {
                // Assign new frequencies
                for (int v = 0; v < 8; v++) {
                    voices[v].active = v < chords[currentChord].frequencies.size();
                    if (voices[v].active) {
                        voices[v].frequency = chords[currentChord].frequencies[v];
                        voices[v].phaseIncrement = (2.0f * M_PI * voices[v].frequency) / filter.sampleRate;
                        voices[v].phase = 0.0f; // Reset phase for new note
                    }
                }
            }
        }

        // Update envelopes
        updateEnvelopes(voices, 8, attackTime, decayTime, filter.sampleRate, i, currentTime);

        // Process two groups of 4 voices
        float outputSample = 0.0f;
        for (int group = 0; group < 2; group++) {
            int voiceOffset = group * 4;

            // Load amplitudes and phases
            float tempAmps[4] = { voices[voiceOffset].amplitude, voices[voiceOffset + 1].amplitude,
                                  voices[voiceOffset + 2].amplitude, voices[voiceOffset + 3].amplitude };
            float tempPhases[4] = { voices[voiceOffset].phase, voices[voiceOffset + 1].phase,
                                    voices[voiceOffset + 2].phase, voices[voiceOffset + 3].phase };
            float tempIncrements[4] = { voices[voiceOffset].phaseIncrement, voices[voiceOffset + 1].phaseIncrement,
                                        voices[voiceOffset + 2].phaseIncrement, voices[voiceOffset + 3].phaseIncrement };
            SIMD_TYPE amplitudes = SIMD_LOAD(tempAmps);
            SIMD_TYPE phases = SIMD_LOAD(tempPhases);
            SIMD_TYPE increments = SIMD_LOAD(tempIncrements);

            // Compute sine for 4 voices
            SIMD_TYPE sinValues = SIMD_SIN(phases);
            sinValues = SIMD_MUL(sinValues, amplitudes);

            // Apply 4-pole ladder filter
            SIMD_TYPE filteredOutput;
            applyLadderFilter(voices, voiceOffset, sinValues, filter, filteredOutput);

            // Sum the 4 voices
            float temp[4];
            SIMD_STORE(temp, filteredOutput);
            outputSample += (temp[0] + temp[1] + temp[2] + temp[3]) * 0.125f; // Scale for 8 voices

            // Update phases
            phases = SIMD_ADD(phases, increments);
            SIMD_TYPE wrap = SIMD_SUB(phases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(phases, twoPi)), twoPi));
            SIMD_STORE(temp, wrap);
            voices[voiceOffset].phase = temp[0];
            voices[voiceOffset + 1].phase = temp[1];
            voices[voiceOffset + 2].phase = temp[2];
            voices[voiceOffset + 3].phase = temp[3];
        }

        // Write raw float sample to stdout
        fwrite(&outputSample, sizeof(float), 1, stdout);
    }
}

int main() {
    // Initialize 8 voices
    const float sampleRate = 48000.0f;
    Voice voices[8];
    for (int i = 0; i < 8; i++) {
        voices[i].frequency = 0.0f;
        voices[i].phase = 0.0f;
        voices[i].phaseIncrement = 0.0f;
        voices[i].amplitude = 0.0f;
        voices[i].cutoff = 500.0f; // Base cutoff frequency
        voices[i].filterEnv = 0.0f;
        voices[i].active = false;
        for (int j = 0; j < 4; j++) {
            voices[i].filterStates[j] = 0.0f;
        }
    }

    // Initialize filter
    Filter filter;
    filter.resonance = 0.7f;
    filter.sampleRate = sampleRate;

    // Define Debussy-inspired chords (frequencies in Hz, MIDI note to Hz)
    auto midiToFreq = [](int midiNote) { return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f); };
    std::vector<Chord> chords = {
        // Chord 1: D♭ major 9 (D♭3, F3, A♭3, C4, E♭4)
        { { midiToFreq(49), midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63) }, 0.0f, 2.0f },
        // Chord 2: G♭ major 7 (G♭3, B♭3, D♭4, F4)
        { { midiToFreq(54), midiToFreq(58), midiToFreq(61), midiToFreq(65) }, 2.0f, 2.0f },
        // Chord 3: B♭ minor 7 (B♭3, D♭4, F4, A♭4)
        { { midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68) }, 4.0f, 2.0f },
        // Chord 4: F minor 9 (F3, A♭3, C4, E♭4, G4)
        { { midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67) }, 6.0f, 2.0f }
    };

    // Generate 8 seconds of audio
    int numSamples = static_cast<int>(8.0f * sampleRate);
    generateSineSamples(voices, numSamples, filter, chords);

    return 0;
}
