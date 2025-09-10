#include <iostream>
#include <cmath>
#include <cstdint>

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
#define SIMD_SET vld1q_f32 // Corrected for loading 4 floats
#define SIMD_LOAD vld1q_f32
#define SIMD_ADD vaddq_f32
#define SIMD_SUB vsubq_f32
#define SIMD_MUL vmulq_f32
#define SIMD_DIV vdivq_f32
#define SIMD_FLOOR my_floor_ps
#define SIMD_STORE vst1q_f32 // Fixed for NEON store
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
};

// 4-pole low-pass ladder filter state
struct Filter {
    float resonance; // Resonance (0 to 1)
    float sampleRate; // Sample rate (Hz)
};

// Update amplitude and filter envelopes
void updateEnvelopes(Voice* voices, float attackTime, float decayTime, float sampleRate, int sampleIndex) {
    float t = sampleIndex / sampleRate; // Time in seconds
    for (int i = 0; i < 4; i++) {
        // Amplitude envelope
        if (t < attackTime) {
            voices[i].amplitude = t / attackTime; // Linear attack
        } else if (t < attackTime + decayTime) {
            voices[i].amplitude = 1.0f - (t - attackTime) / decayTime; // Linear decay
        } else {
            voices[i].amplitude = 0.0f;
        }
        // Filter envelope (modulates cutoff)
        if (t < attackTime) {
            voices[i].filterEnv = t / attackTime;
        } else if (t < attackTime + decayTime) {
            voices[i].filterEnv = 1.0f - (t - attackTime) / decayTime;
        } else {
            voices[i].filterEnv = 0.0f;
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
void applyLadderFilter(Voice* voices, SIMD_TYPE input, Filter& filter, SIMD_TYPE& output) {
    // Compute alpha = 1 - e^(-2π * cutoff / sampleRate) for each voice
    float cutoffs[4];
    for (int i = 0; i < 4; i++) {
        // Modulate cutoff with envelope (base cutoff + envelope amount)
        float modulatedCutoff = voices[i].cutoff + voices[i].filterEnv * 2000.0f;
        modulatedCutoff = std::max(20.0f, std::min(modulatedCutoff, filter.sampleRate / 2.0f)); // Clamp
        cutoffs[i] = 1.0f - expf(-2.0f * M_PI * modulatedCutoff / filter.sampleRate);
    }
    SIMD_TYPE alpha = SIMD_SET(cutoffs);
    SIMD_TYPE oneMinusAlpha = SIMD_SUB(SIMD_SET1(1.0f), alpha);
    SIMD_TYPE resonance = SIMD_SET1(filter.resonance * 4.0f); // Scale resonance for feedback

    // Load filter states
    SIMD_TYPE states[4];
    for (int i = 0; i < 4; i++) {
        float temp[4] = { voices[0].filterStates[i], voices[1].filterStates[i],
                          voices[2].filterStates[i], voices[3].filterStates[i] };
        states[i] = SIMD_LOAD(temp);
    }

    // Apply feedback (simplified, resonance affects input)
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
        voices[0].filterStates[i] = temp[0];
        voices[1].filterStates[i] = temp[1];
        voices[2].filterStates[i] = temp[2];
        voices[3].filterStates[i] = temp[3];
    }
}

// Generate sine waves and apply filter for 4 voices
void generateSineSamples(Voice* voices, int numSamples, Filter& filter) {
    // Constants for oscillator
    const SIMD_TYPE twoPi = SIMD_SET1(2.0f * M_PI);
    const SIMD_TYPE one = SIMD_SET1(1.0f);

    // Envelope parameters
    float attackTime = 0.1f; // 100 ms attack
    float decayTime = 1.9f;  // 1.9 s decay (total note = 2 s)

    for (int i = 0; i < numSamples; i++) {
        // Update envelopes
        updateEnvelopes(voices, attackTime, decayTime, filter.sampleRate, i);

        // Load amplitudes and phases
        float tempAmps[4] = { voices[0].amplitude, voices[1].amplitude,
                              voices[2].amplitude, voices[3].amplitude };
        float tempPhases[4] = { voices[0].phase, voices[1].phase,
                                voices[2].phase, voices[3].phase };
        float tempIncrements[4] = { voices[0].phaseIncrement, voices[1].phaseIncrement,
                                    voices[2].phaseIncrement, voices[3].phaseIncrement };
        SIMD_TYPE amplitudes = SIMD_LOAD(tempAmps);
        SIMD_TYPE phases = SIMD_LOAD(tempPhases);
        SIMD_TYPE increments = SIMD_LOAD(tempIncrements);

        // Compute sine for 4 voices
        SIMD_TYPE sinValues = SIMD_SIN(phases);
        sinValues = SIMD_MUL(sinValues, amplitudes);

        // Apply 4-pole ladder filter
        SIMD_TYPE filteredOutput;
        applyLadderFilter(voices, sinValues, filter, filteredOutput);

        // Sum the 4 voices
        float temp[4];
        SIMD_STORE(temp, filteredOutput);
        float outputSample = (temp[0] + temp[1] + temp[2] + temp[3]) * 0.25f; // Scale for volume

        // Write raw float sample to stdout
        fwrite(&outputSample, sizeof(float), 1, stdout);

        // Update phases
        phases = SIMD_ADD(phases, increments);
        SIMD_TYPE wrap = SIMD_SUB(phases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(phases, twoPi)), twoPi));
        SIMD_STORE(temp, wrap);
        voices[0].phase = temp[0];
        voices[1].phase = temp[1];
        voices[2].phase = temp[2];
        voices[3].phase = temp[3];
    }
}

int main() {
    // Initialize 4 voices (A4, C5, E5, A5)
    const float sampleRate = 48000.0f;
    Voice voices[4];
    voices[0].frequency = 440.0f; // A4
    voices[1].frequency = 523.25f; // C5
    voices[2].frequency = 659.25f; // E5
    voices[3].frequency = 880.0f; // A5
    for (int i = 0; i < 4; i++) {
        voices[i].phase = 0.0f;
        voices[i].phaseIncrement = (2.0f * M_PI * voices[i].frequency) / sampleRate;
        voices[i].amplitude = 0.0f;
        voices[i].cutoff = 500.0f; // Base cutoff frequency
        voices[i].filterEnv = 0.0f;
        for (int j = 0; j < 4; j++) {
            voices[i].filterStates[j] = 0.0f; // Initialize filter states
        }
    }

    // Initialize filter
    Filter filter;
    filter.resonance = 0.7f; // Moderate resonance for DFM-1-like sound
    filter.sampleRate = sampleRate;

    // Generate 2 seconds of audio
    int numSamples = static_cast<int>(2.0f * sampleRate);
    generateSineSamples(voices, numSamples, filter);

    return 0;
}
