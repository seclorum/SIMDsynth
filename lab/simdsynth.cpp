/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 8 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

// Standard library includes for I/O, math, and data structures
#include <iostream>  // For console output (e.g., error messages)
#include <cmath>     // For mathematical functions (sin, pow, exp)
#include <cstdint>   // For fixed-width integer types
#include <vector>    // For dynamic arrays (e.g., chord frequencies)
#include <cstdlib>   // For random number generation (rand)
#include <ctime>     // For seeding random number generator
#include <string>    // For command-line argument parsing

// Disable debug output by default to reduce console clutter
#undef DEBUG_OUTPUT

// Define constants for synthesizer configuration
#define MAX_VOICE_POLYPHONY 8  // Maximum number of simultaneous voices (polyphony)
#define WAVETABLE_SIZE 1024    // Size of wavetable arrays for main oscillator

// Platform-specific SIMD (Single Instruction, Multiple Data) configuration
#ifdef __x86_64__
// Include headers for x86_64 SIMD instructions (SSE and SSE2)
#include <xmmintrin.h> // SSE instructions for floating-point operations
#include <emmintrin.h> // SSE2 instructions for additional SIMD operations
#ifdef __SSE4_1__
#include <smmintrin.h> // SSE4.1 for advanced SIMD operations (if available)
#endif
// Define SIMD macros for x86_64 (SSE)
#define SIMD_TYPE __m128         // 128-bit SIMD type for 4 floats
#define SIMD_SET1 _mm_set1_ps    // Set all 4 lanes to a single float value
#define SIMD_SET _mm_set_ps      // Set 4 lanes to individual float values
#define SIMD_LOAD _mm_load_ps    // Load 4 floats from aligned memory
#define SIMD_ADD _mm_add_ps      // Add 4 floats in parallel
#define SIMD_SUB _mm_sub_ps      // Subtract 4 floats in parallel
#define SIMD_MUL _mm_mul_ps      // Multiply 4 floats in parallel
#define SIMD_DIV _mm_div_ps      // Divide 4 floats in parallel
#define SIMD_FLOOR _mm_floor_ps  // Floor 4 floats in parallel
#define SIMD_STORE _mm_store_ps  // Store 4 floats to aligned memory
#define SIMD_SIN fast_sin_ps     // Custom SIMD sine function for x86_64
#elif defined(__arm64__)
// Include header for ARM64 NEON SIMD instructions
#include <arm_neon.h> // NEON instructions for ARM64
// Define SIMD macros for ARM64 (NEON)
#define SIMD_TYPE float32x4_t           // 128-bit SIMD type for 4 floats
#define SIMD_SET1 vdupq_n_f32           // Set all 4 lanes to a single float value
#define SIMD_SET(__a, __b, __c, __d) vld1q_f32((float[4]){__a, __b, __c, __d}) // Set 4 lanes to individual float values
#define SIMD_LOAD vld1q_f32             // Load 4 floats from memory
#define SIMD_ADD vaddq_f32              // Add 4 floats in parallel
#define SIMD_SUB vsubq_f32              // Subtract 4 floats in parallel
#define SIMD_MUL vmulq_f32              // Multiply 4 floats in parallel
#define SIMD_DIV vdivq_f32              // Divide 4 floats in parallel
#define SIMD_FLOOR my_floor_ps          // Custom floor function for NEON
#define SIMD_STORE vst1q_f32            // Store 4 floats to memory
#define SIMD_SIN fast_sin_ps            // Custom SIMD sine function for ARM64
#else
#error "Unsupported architecture: Requires x86_64 or arm64"
#endif

// Define wavetable arrays for main oscillator (1024 samples each)
static float sineTable[WAVETABLE_SIZE]; // Sine wave wavetable
static float sawTable[WAVETABLE_SIZE];  // Sawtooth wave wavetable

// Initialize wavetables at startup
void initWavetables() {
    // Populate wavetables with precomputed waveforms
    for (int i = 0; i < WAVETABLE_SIZE; ++i) {
        // Sine wavetable: one cycle of sin(2π * i / WAVETABLE_SIZE)
        sineTable[i] = sinf(2.0f * M_PI * i / WAVETABLE_SIZE);
        // Sawtooth wavetable: linear ramp from -1 to 1 over one cycle
        sawTable[i] = 2.0f * (i / static_cast<float>(WAVETABLE_SIZE)) - 1.0f;
    }
}

// Structure to represent a single synthesizer voice
struct Voice {
    float frequency;         // Main oscillator frequency in Hz
    float phase;             // Main oscillator phase (0 to 1 for wavetable lookup)
    float phaseIncrement;    // Per-sample phase increment for main oscillator
    float amplitude;         // Oscillator amplitude (controlled by envelope)
    float cutoff;            // Filter cutoff frequency in Hz
    float filterEnv;         // Filter envelope output (modulates cutoff)
    float filterStates[4];   // State variables for 4-pole ladder filter
    bool active;             // Whether the voice is currently active
    float fegAttack;         // Filter envelope attack time in seconds
    float fegDecay;          // Filter envelope decay time in seconds
    float fegSustain;        // Filter envelope sustain level (0 to 1)
    float fegRelease;        // Filter envelope release time in seconds
    float lfoRate;           // Low-frequency oscillator (LFO) rate in Hz
    float lfoDepth;          // LFO depth in radians (modulates main oscillator phase)
    float lfoPhase;          // LFO phase in radians
    float subFrequency;      // Sub-oscillator frequency in Hz
    float subPhase;          // Sub-oscillator phase in radians
    float subPhaseIncrement; // Per-sample phase increment for sub-oscillator
    float subTune;           // Sub-oscillator tuning offset in semitones
    float subMix;            // Sub-oscillator mix level (0 to 1)
    float subTrack;          // Sub-oscillator pitch tracking ratio (0 to 1)
    int wavetableType;       // Wavetable type for main oscillator: 0=sine, 1=sawtooth
};

// Structure to represent filter parameters
struct Filter {
    float resonance;  // Filter resonance (0 to 1, controls feedback strength)
    float sampleRate; // Audio sample rate in Hz (e.g., 48000)
};

// Structure to represent a chord in the sequence
struct Chord {
    std::vector<float> frequencies; // List of frequencies for the chord's notes
    float startTime;                // Start time of the chord in seconds
    float duration;                 // Duration of the chord in seconds
};

// Convert MIDI note number to frequency in Hz
float midiToFreq(int midiNote) {
    // A4 (MIDI note 69) = 440 Hz, with 12 semitones per octave
    return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
}

// Generate a random float in range [base * (1 - var), base * (1 + var)]
float randomize(float base, float var) {
    // Generate random value between 0 and 1
    float r = static_cast<float>(rand()) / RAND_MAX;
    // Scale and shift to desired range
    return base * (1.0f - var + r * 2.0f * var);
}

// Update amplitude and filter envelopes for all voices
void updateEnvelopes(Voice *voices, int numVoices, float attackTime,
                     float decayTime, float chordDuration, float sampleRate,
                     int sampleIndex, float currentTime) {
    // Calculate current time in seconds from sample index
    float t = sampleIndex / sampleRate;
    // Process each voice
    for (int i = 0; i < numVoices; i++) {
        // Skip inactive voices and reset their parameters
        if (!voices[i].active) {
            voices[i].amplitude = 0.0f;         // Reset amplitude
            voices[i].filterEnv = 0.0f;         // Reset filter envelope
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] = 0.0f; // Reset filter states
            }
            continue;
        }
        // Calculate time relative to chord start
        float localTime = t - currentTime;

        // Amplitude envelope (Attack-Decay, no sustain)
        if (localTime < attackTime) {
            // Linear attack phase: ramp from 0 to 1
            voices[i].amplitude = localTime / attackTime;
        } else if (localTime < attackTime + decayTime) {
            // Linear decay phase: ramp from 1 to 0
            voices[i].amplitude = 1.0f - (localTime - attackTime) / decayTime;
        } else {
            // After decay, disable voice
            voices[i].amplitude = 0.0f;
            voices[i].active = false;
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] = 0.0f; // Reset filter states
            }
        }

        // Filter envelope (Attack-Decay-Sustain-Release, ADSR)
        if (localTime < voices[i].fegAttack) {
            // Linear attack phase: ramp from 0 to 1
            voices[i].filterEnv = localTime / voices[i].fegAttack;
        } else if (localTime < voices[i].fegAttack + voices[i].fegDecay) {
            // Linear decay phase: ramp from 1 to sustain level
            voices[i].filterEnv = 1.0f - (localTime - voices[i].fegAttack) /
                                        voices[i].fegDecay *
                                        (1.0f - voices[i].fegSustain);
        } else if (localTime < chordDuration) {
            // Sustain phase: hold at sustain level
            voices[i].filterEnv = voices[i].fegSustain;
        } else if (localTime < chordDuration + voices[i].fegRelease) {
            // Linear release phase: ramp from sustain to 0
            voices[i].filterEnv = voices[i].fegSustain *
                                  (1.0f - (localTime - chordDuration) /
                                              voices[i].fegRelease);
        } else {
            // After release, disable voice
            voices[i].filterEnv = 0.0f;
            voices[i].active = false;
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] = 0.0f; // Reset filter states
            }
        }
    }
}

// Custom floor function for ARM64 NEON (no native floor instruction)
#ifdef __arm64__
inline float32x4_t my_floor_ps(float32x4_t x) {
    // Extract 4 floats from SIMD register
    float temp[4];
    vst1q_f32(temp, x);
    // Apply scalar floor to each element
    temp[0] = floorf(temp[0]);
    temp[1] = floorf(temp[1]);
    temp[2] = floorf(temp[2]);
    temp[3] = floorf(temp[3]);
    // Load results back into SIMD register
    return vld1q_f32(temp);
}
#endif

// Fast sine approximation for x86_64 (SSE)
#ifdef __x86_64__
inline __m128 fast_sin_ps(__m128 x) {
    // Constants for sine approximation
    const __m128 twoPi = _mm_set1_ps(2.0f * M_PI);          // 2π for phase wrapping
    const __m128 invTwoPi = _mm_set1_ps(1.0f / (2.0f * M_PI)); // 1/(2π) for phase normalization
    const __m128 piOverTwo = _mm_set1_ps(M_PI / 2.0f);      // π/2 for quadrant detection

    // Wrap phase to [0, 2π)
    __m128 q = _mm_mul_ps(x, invTwoPi); // Normalize phase
    q = _mm_floor_ps(q);                // Integer part for wrapping
    __m128 xWrapped = _mm_sub_ps(x, _mm_mul_ps(q, twoPi)); // Wrapped phase

    // Determine sign based on quadrant
    __m128 sign = _mm_set1_ps(1.0f);
    __m128 absX = _mm_max_ps(xWrapped, _mm_sub_ps(_mm_setzero_ps(), xWrapped));
    __m128 gtPiOverTwo = _mm_cmpgt_ps(absX, piOverTwo); // Check if |x| > π/2
    sign = _mm_or_ps(_mm_and_ps(gtPiOverTwo, _mm_set1_ps(-1.0f)),
                     _mm_andnot_ps(gtPiOverTwo, _mm_set1_ps(1.0f)));
    // Adjust phase for sine calculation
    xWrapped = _mm_sub_ps(xWrapped,
                          _mm_and_ps(gtPiOverTwo,
                                     _mm_mul_ps(piOverTwo, _mm_set1_ps(2.0f))));

    // Taylor series coefficients for sine approximation
    const __m128 c3 = _mm_set1_ps(-1.0f / 6.0f);        // x^3 term
    const __m128 c5 = _mm_set1_ps(1.0f / 120.0f);       // x^5 term
    const __m128 c7 = _mm_set1_ps(-1.0f / 5040.0f);     // x^7 term

    // Compute Taylor series: sin(x) ≈ x - x^3/6 + x^5/120 - x^7/5040
    __m128 x2 = _mm_mul_ps(xWrapped, xWrapped);
    __m128 x3 = _mm_mul_ps(x2, xWrapped);
    __m128 x5 = _mm_mul_ps(x3, x2);
    __m128 x7 = _mm_mul_ps(x5, x2);

    __m128 result = _mm_add_ps(
        xWrapped,
        _mm_add_ps(_mm_mul_ps(c3, x3),
                   _mm_add_ps(_mm_mul_ps(c5, x5), _mm_mul_ps(c7, x7))));
    // Apply sign to result
    return _mm_mul_ps(result, sign);
}
#endif

// Fast sine approximation for ARM64 (NEON)
#ifdef __arm64__
inline float32x4_t fast_sin_ps(float32x4_t x) {
    // Constants for sine approximation
    const float32x4_t twoPi = vdupq_n_f32(2.0f * M_PI);        // 2π for phase wrapping
    const float32x4_t invTwoPi = vdupq_n_f32(1.0f / (2.0f * M_PI)); // 1/(2π) for phase normalization
    const float32x4_t piOverTwo = vdupq_n_f32(M_PI / 2.0f);    // π/2 for quadrant detection

    // Wrap phase to [0, 2π)
    float32x4_t q = vmulq_f32(x, invTwoPi); // Normalize phase
    q = my_floor_ps(q);                     // Integer part for wrapping
    float32x4_t xWrapped = vsubq_f32(x, vmulq_f32(q, twoPi)); // Wrapped phase

    // Determine sign based on quadrant
    float32x4_t sign = vdupq_n_f32(1.0f);
    float32x4_t absX = vmaxq_f32(xWrapped, vsubq_f32(vdupq_n_f32(0.0f), xWrapped));
    uint32x4_t gtPiOverTwo = vcgtq_f32(absX, piOverTwo); // Check if |x| > π/2
    sign = vbslq_f32(gtPiOverTwo, vdupq_n_f32(-1.0f), vdupq_n_f32(1.0f));
    // Adjust phase for sine calculation
    xWrapped = vsubq_f32(xWrapped,
                         vbslq_f32(gtPiOverTwo,
                                   vmulq_f32(piOverTwo, vdupq_n_f32(2.0f)),
                                   vdupq_n_f32(0.0f)));

    // Taylor series coefficients for sine approximation
    const float32x4_t c3 = vdupq_n_f32(-1.0f / 6.0f);    // x^3 term
    const float32x4_t c5 = vdupq_n_f32(1.0f / 120.0f);   // x^5 term
    const float32x4_t c7 = vdupq_n_f32(-1.0f / 5040.0f); // x^7 term

    // Compute Taylor series: sin(x) ≈ x - x^3/6 + x^5/120 - x^7/5040
    float32x4_t x2 = vmulq_f32(xWrapped, xWrapped);
    float32x4_t x3 = vmulq_f32(x2, xWrapped);
    float32x4_t x5 = vmulq_f32(x3, x2);
    float32x4_t x7 = vmulq_f32(x5, x2);

    float32x4_t result = vaddq_f32(
        xWrapped,
        vaddq_f32(vmulq_f32(c3, x3),
                  vaddq_f32(vmulq_f32(c5, x5), vmulq_f32(c7, x7))));
    // Apply sign to result
    return vmulq_f32(result, sign);
}
#endif

// Perform SIMD-based wavetable lookup with linear interpolation
inline SIMD_TYPE wavetable_lookup_ps(SIMD_TYPE phase, const float* table) {
    // Constant for wavetable size
    const SIMD_TYPE tableSize = SIMD_SET1(static_cast<float>(WAVETABLE_SIZE));

    // Scale phase (0 to 1) to wavetable index
    SIMD_TYPE index = SIMD_MUL(phase, tableSize);
    SIMD_TYPE indexFloor = SIMD_FLOOR(index); // Integer part of index
    SIMD_TYPE frac = SIMD_SUB(index, indexFloor); // Fractional part for interpolation

    // Wrap indices to [0, WAVETABLE_SIZE) to handle phase overflow
    indexFloor = SIMD_SUB(indexFloor, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(indexFloor, tableSize)), tableSize));

    // Extract indices for array access
    float tempIndices[4];
    SIMD_STORE(tempIndices, indexFloor);
    int indices[4] = {
        static_cast<int>(tempIndices[0]),
        static_cast<int>(tempIndices[1]),
        static_cast<int>(tempIndices[2]),
        static_cast<int>(tempIndices[3])
    };

    // Load wavetable samples for current and next indices
    float samples1[4], samples2[4];
    for (int i = 0; i < 4; ++i) {
        int idx = indices[i];
        samples1[i] = table[idx];                    // Sample at floor index
        samples2[i] = table[(idx + 1) % WAVETABLE_SIZE]; // Sample at next index
    }

    // Load samples into SIMD registers
    SIMD_TYPE value1 = SIMD_LOAD(samples1);
    SIMD_TYPE value2 = SIMD_LOAD(samples2);

    // Perform linear interpolation: value1 + frac * (value2 - value1)
    return SIMD_ADD(value1, SIMD_MUL(frac, SIMD_SUB(value2, value1)));
}

// Apply 4-pole ladder filter to four voices simultaneously
void applyLadderFilter(Voice *voices, int voiceOffset, SIMD_TYPE input,
                       Filter &filter, SIMD_TYPE &output) {
    // Calculate filter cutoff frequencies for four voices
    float cutoffs[4];
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        // Modulate cutoff with filter envelope
        float modulatedCutoff = voices[idx].cutoff + voices[idx].filterEnv * 2000.0f;
        // Clamp cutoff to valid range
        modulatedCutoff = std::max(200.0f, std::min(modulatedCutoff, filter.sampleRate / 2.0f));
        // Compute filter coefficient (alpha) for low-pass filter
        cutoffs[i] = 1.0f - expf(-2.0f * M_PI * modulatedCutoff / filter.sampleRate);
        // Handle numerical instability
        if (std::isnan(cutoffs[i]) || !std::isfinite(cutoffs[i]))
            cutoffs[i] = 0.0f;
    }
    // Load cutoff coefficients into SIMD register
#ifdef __x86_64__
    SIMD_TYPE alpha = SIMD_SET(cutoffs[0], cutoffs[1], cutoffs[2], cutoffs[3]);
#elif defined(__arm64__)
    float temp[4] = {cutoffs[0], cutoffs[1], cutoffs[2], cutoffs[3]};
    SIMD_TYPE alpha = SIMD_LOAD(temp);
#endif
    // Compute resonance value, scaled to avoid instability
    SIMD_TYPE resonance = SIMD_SET1(std::min(filter.resonance * 4.0f, 4.0f));

    // Check if any of the four voices are active
    bool anyActive = false;
    for (int i = 0; i < 4; i++) {
        if (voiceOffset + i < MAX_VOICE_POLYPHONY && voices[voiceOffset + i].active) {
            anyActive = true;
        }
    }
    // If no voices are active, output zero and return
    if (!anyActive) {
        output = SIMD_SET1(0.0f);
        return;
    }

    // Load filter states for four voices
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

    // Apply feedback based on resonance
    SIMD_TYPE feedback = SIMD_MUL(states[3], resonance);
    SIMD_TYPE filterInput = SIMD_SUB(input, feedback);

    // Process four-pole ladder filter (four cascaded low-pass stages)
    states[0] = SIMD_ADD(states[0], SIMD_MUL(alpha, SIMD_SUB(filterInput, states[0])));
    states[1] = SIMD_ADD(states[1], SIMD_MUL(alpha, SIMD_SUB(states[0], states[1])));
    states[2] = SIMD_ADD(states[2], SIMD_MUL(alpha, SIMD_SUB(states[1], states[2])));
    states[3] = SIMD_ADD(states[3], SIMD_MUL(alpha, SIMD_SUB(states[2], states[3])));

    // Output is the final stage
    output = states[3];

    // Check for numerical stability and clamp output
    float tempCheck[4];
    SIMD_STORE(tempCheck, output);
    for (int i = 0; i < 4; i++) {
        if (!std::isfinite(tempCheck[i])) {
            tempCheck[i] = 0.0f; // Reset non-finite values
        } else {
            tempCheck[i] = std::max(-1.0f, std::min(1.0f, tempCheck[i])); // Clamp to [-1, 1]
        }
    }
    output = SIMD_LOAD(tempCheck);

    // Log any NaN outputs for debugging
    float tempOut[4];
    SIMD_STORE(tempOut, output);
    if (std::isnan(tempOut[0]) || std::isnan(tempOut[1]) ||
        std::isnan(tempOut[2]) || std::isnan(tempOut[3])) {
        std::cerr << "Filter output nan at voiceOffset " << voiceOffset << ": {"
                  << tempOut[0] << ", " << tempOut[1] << ", " << tempOut[2]
                  << ", " << tempOut[3] << "}" << std::endl;
    }

    // Store updated filter states back to voices
    for (int i = 0; i < 4; i++) {
        float temp[4];
        SIMD_STORE(temp, states[i]);
        voices[voiceOffset].filterStates[i] = temp[0];
        voices[voiceOffset + 1].filterStates[i] = temp[1];
        voices[voiceOffset + 2].filterStates[i] = temp[2];
        voices[voiceOffset + 3].filterStates[i] = temp[3];
    }
}

// Generate audio samples using wavetable for main oscillator and sine for sub-oscillator
void generateSineSamples(Voice *voices, int numSamples, Filter &filter,
                         const std::vector<Chord> &chords) {
    // Constant for phase calculations
    const SIMD_TYPE twoPi = SIMD_SET1(2.0f * M_PI);
    // Envelope parameters
    float attackTime = 0.1f; // Attack time for amplitude envelope (seconds)
    float decayTime = 1.9f;  // Decay time for amplitude envelope (seconds)
    float currentTime = 0.0f; // Current chord start time
    size_t currentChord = 0;  // Index of current chord in sequence

    // Process each sample
    for (int i = 0; i < numSamples; i++) {
        // Calculate current time in seconds
        float t = i / filter.sampleRate;

        // Activate new chord if current time reaches its start time
        if (currentChord < chords.size() && t >= chords[currentChord].startTime) {
            if (currentTime != chords[currentChord].startTime) {
                currentTime = chords[currentChord].startTime;
                // Initialize voices for the chord
                for (size_t v = 0; v < MAX_VOICE_POLYPHONY; v++) {
                    // Activate voice if within chord's frequency count
                    voices[v].active = v < chords[currentChord].frequencies.size();
                    if (voices[v].active) {
                        // Set main oscillator frequency
                        voices[v].frequency = chords[currentChord].frequencies[v];
                        // Calculate phase increment for wavetable (0 to 1)
                        voices[v].phaseIncrement = voices[v].frequency / filter.sampleRate;
                        voices[v].phase = 0.0f; // Reset phase
                        voices[v].lfoPhase = 0.0f; // Reset LFO phase
                        // Randomize filter envelope parameters
                        voices[v].fegAttack = randomize(0.1f, 0.2f);
                        voices[v].fegDecay = randomize(1.0f, 0.2f);
                        voices[v].fegSustain = randomize(0.5f, 0.2f);
                        voices[v].fegRelease = randomize(0.2f, 0.2f);
                        voices[v].lfoRate = 0; // Disable LFO by default
                        voices[v].lfoDepth = 0;
                        // Configure sub-oscillator
                        voices[v].subTune = -12.0f; // One octave below
                        voices[v].subMix = 0.5f;    // 50% mix with main oscillator
                        voices[v].subTrack = 1.0f;  // Full pitch tracking
                        // Calculate sub-oscillator frequency
                        float subFreq = voices[v].frequency *
                                        powf(2.0f, voices[v].subTune / 12.0f) *
                                        voices[v].subTrack;
                        voices[v].subFrequency = subFreq;
                        // Calculate sub-oscillator phase increment (radians)
                        voices[v].subPhaseIncrement = (2.0f * M_PI * subFreq) / filter.sampleRate;
                        voices[v].subPhase = 0.0f; // Reset sub-oscillator phase
                    } else {
                        // Reset inactive voices
                        voices[v].amplitude = 0.0f;
                        voices[v].filterEnv = 0.0f;
                        for (int j = 0; j < 4; j++) {
                            voices[v].filterStates[j] = 0.0f;
                        }
                    }
                }
            }
        }

        // Move to next chord if current chord's duration has elapsed
        if (currentChord < chords.size() &&
            t >= chords[currentChord].startTime + chords[currentChord].duration) {
            currentChord++;
        }

        // Default chord duration if no chord is active
        float chordDuration = (currentChord < chords.size()) ? chords[currentChord].duration : 2.0f;
        // Update amplitude and filter envelopes
        updateEnvelopes(voices, MAX_VOICE_POLYPHONY, attackTime, decayTime,
                        chordDuration, filter.sampleRate, i, currentTime);

        // Initialize output sample for this time step
        float outputSample = 0.0f;

#ifdef DEBUG_OUTPUT
        // Count active voices for debugging
        int activeVoices = 0;
        for (int v = 0; v < MAX_VOICE_POLYPHONY; v++) {
            if (voices[v].active) activeVoices++;
        }
#endif

        // Process voices in groups of 4 for SIMD efficiency
        for (int group = 0; group < (MAX_VOICE_POLYPHONY + 3) / 4; group++) {
            int voiceOffset = group * 4; // Base index for current group
            // Check for valid voice offset
            if (voiceOffset >= MAX_VOICE_POLYPHONY) {
                std::cerr << "Error: voiceOffset " << voiceOffset
                          << " exceeds MAX_VOICE_POLYPHONY" << std::endl;
                continue;
            }

            // Load voice parameters into arrays for SIMD processing
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

            // Update LFO phase
            SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRates, SIMD_SET1(2.0f * M_PI / filter.sampleRate));
            lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
            lfoPhases = SIMD_SUB(lfoPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)), twoPi));
            // Compute LFO values and apply to main oscillator phase
            SIMD_TYPE lfoValues = SIMD_SIN(lfoPhases);
            lfoValues = SIMD_MUL(lfoValues, lfoDepths);
            phases = SIMD_ADD(phases, SIMD_DIV(lfoValues, twoPi)); // Scale LFO to wavetable phase range

            // Compute main oscillator output using wavetables
            SIMD_TYPE mainValues = SIMD_SET1(0.0f);
            for (int j = 0; j < 4; ++j) {
                int idx = voiceOffset + j;
                if (idx >= MAX_VOICE_POLYPHONY || !voices[idx].active) continue;

                // Ensure phase is in [0, 1) for wavetable lookup
                float phase = tempPhases[j];
                phase = phase - floorf(phase);
                float value;
                // Select wavetable based on type
                if (wavetableTypes[j] == 0) {
                    value = wavetable_lookup_ps(SIMD_SET1(phase), sineTable)[j % 4];
                } else {
                    value = wavetable_lookup_ps(SIMD_SET1(phase), sawTable)[j % 4];
                }
                // Accumulate value for active voice
                float tempMain[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                tempMain[j] = value;
                mainValues = SIMD_ADD(mainValues, SIMD_LOAD(tempMain));
            }
            // Apply amplitude and balance with sub-oscillator
            mainValues = SIMD_MUL(mainValues, amplitudes);
            mainValues = SIMD_MUL(mainValues, SIMD_SUB(SIMD_SET1(1.0f), subMixes));

            // Compute sub-oscillator output (sine wave)
            SIMD_TYPE subSinValues = SIMD_SIN(subPhases);
            subSinValues = SIMD_MUL(subSinValues, amplitudes);
            subSinValues = SIMD_MUL(subSinValues, subMixes);

            // Combine main and sub-oscillator outputs
            SIMD_TYPE combinedValues = SIMD_ADD(mainValues, subSinValues);

            // Apply ladder filter
            SIMD_TYPE filteredOutput;
            applyLadderFilter(voices, voiceOffset, combinedValues, filter, filteredOutput);

#ifdef DEBUG_OUTPUT
            // Debug output for monitoring oscillator and filter values
            float tempDebugMain[4], tempDebugSub[4], tempDebugFiltered[4], tempDebugAmps[4];
            SIMD_STORE(tempDebugMain, mainValues);
            SIMD_STORE(tempDebugSub, subSinValues);
            SIMD_STORE(tempDebugFiltered, filteredOutput);
            SIMD_STORE(tempDebugAmps, amplitudes);
            if (i % 1000 == 0) {
                std::cerr << "Sample " << i << ": mainValues = {"
                          << tempDebugMain[0] << ", " << tempDebugMain[1] << ", "
                          << tempDebugMain[2] << ", " << tempDebugMain[3]
                          << "}, subValues = {"
                          << tempDebugSub[0] << ", " << tempDebugSub[1] << ", "
                          << tempDebugSub[2] << ", " << tempDebugSub[3]
                          << "}, amplitudes = {" << tempDebugAmps[0] << ", "
                          << tempDebugAmps[1] << ", " << tempDebugAmps[2] << ", "
                          << tempDebugAmps[3] << "}, filteredOutput = {"
                          << tempDebugFiltered[0] << ", " << tempDebugFiltered[1]
                          << ", " << tempDebugFiltered[2] << ", "
                          << tempDebugFiltered[3] << "}"
                          << std::endl;
            }
#endif

            // Sum filtered outputs from four voices
            float temp[4];
            SIMD_STORE(temp, filteredOutput);
            outputSample += (temp[0] + temp[1] + temp[2] + temp[3]);

            // Update main oscillator phase
            phases = SIMD_ADD(phases, increments);
            phases = SIMD_SUB(phases, SIMD_FLOOR(phases));
            SIMD_STORE(temp, phases);
            voices[voiceOffset].phase = temp[0];
            voices[voiceOffset + 1].phase = temp[1];
            voices[voiceOffset + 2].phase = temp[2];
            voices[voiceOffset + 3].phase = temp[3];

            // Update sub-oscillator phase
            subPhases = SIMD_ADD(subPhases, subIncrements);
            subPhases = SIMD_SUB(subPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(subPhases, twoPi)), twoPi));
            SIMD_STORE(temp, subPhases);
            voices[voiceOffset].subPhase = temp[0];
            voices[voiceOffset + 1].subPhase = temp[1];
            voices[voiceOffset + 2].subPhase = temp[2];
            voices[voiceOffset + 3].subPhase = temp[3];

            // Update LFO phase
            SIMD_STORE(temp, lfoPhases);
            voices[voiceOffset].lfoPhase = temp[0];
            voices[voiceOffset + 1].lfoPhase = temp[1];
            voices[voiceOffset + 2].lfoPhase = temp[2];
            voices[voiceOffset + 3].lfoPhase = temp[3];
        }

        // Attenuate output to prevent clipping
        outputSample *= 0.5f;

#ifdef DEBUG_OUTPUT
        // Log sample value and active voices for debugging
        if (i % 1000 == 0) {
            std::cerr << "Sample " << i << ": outputSample = " << outputSample
                      << ", activeVoices = " << activeVoices << std::endl;
        }
#endif

        // Handle numerical errors in output
        if (std::isnan(outputSample) || !std::isfinite(outputSample)) {
            outputSample = 0.0f;
            std::cerr << "Prevented NAN output!" << std::endl;
        }

        // Write sample to stdout as raw float data
        fwrite(&outputSample, sizeof(float), 1, stdout);
    }
}

// Main entry point
int main(int argc, char* argv[]) {
    // Seed random number generator for consistent randomization
    srand(1234);
    // Set sample rate for audio synthesis
    const float sampleRate = 48000.0f;
    // Default to sine wavetable
    int wavetableType = 0;

    // Parse command-line argument for wavetable type
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "saw") {
            wavetableType = 1; // Select sawtooth wavetable
        } else if (arg != "sine") {
            // Warn if invalid wavetable type is provided
            std::cerr << "Invalid wavetable type. Use 'sine' or 'saw'. Defaulting to sine." << std::endl;
        }
    }

    // Initialize wavetables before synthesis
    initWavetables();

    // Initialize voices
    Voice voices[MAX_VOICE_POLYPHONY];
    for (int i = 0; i < MAX_VOICE_POLYPHONY; i++) {
        voices[i].frequency = 0.0f;         // No initial frequency
        voices[i].phase = 0.0f;             // Reset phase
        voices[i].phaseIncrement = 0.0f;    // No phase increment
        voices[i].amplitude = 0.0f;         // No amplitude
        voices[i].cutoff = 1000.0f;         // Default filter cutoff
        voices[i].filterEnv = 0.0f;         // Reset filter envelope
        voices[i].active = false;           // Voice is inactive
        voices[i].fegAttack = 0.1f;         // Default filter envelope attack
        voices[i].fegDecay = 1.0f;          // Default filter envelope decay
        voices[i].fegSustain = 0.5f;        // Default filter envelope sustain
        voices[i].fegRelease = 0.2f;        // Default filter envelope release
        voices[i].lfoRate = 1.0f;           // Default LFO rate
        voices[i].lfoDepth = 0.01f;         // Default LFO depth
        voices[i].lfoPhase = 0.0f;          // Reset LFO phase
        voices[i].subFrequency = 0.0f;      // No sub-oscillator frequency
        voices[i].subPhase = 0.0f;          // Reset sub-oscillator phase
        voices[i].subPhaseIncrement = 0.0f; // No sub-oscillator phase increment
        voices[i].subTune = -12.0f;         // Sub-oscillator one octave below
        voices[i].subMix = 0.5f;            // 50% sub-oscillator mix
        voices[i].subTrack = 1.0f;          // Full pitch tracking
        voices[i].wavetableType = wavetableType; // Set wavetable type
        for (int j = 0; j < 4; j++) {
            voices[i].filterStates[j] = 0.0f; // Reset filter states
        }
    }

    // Initialize filter parameters
    Filter filter;
    filter.resonance = 0.7f;    // Moderate resonance for warm sound
    filter.sampleRate = sampleRate; // Set filter sample rate

    // Define chord sequence (MIDI notes converted to frequencies)
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

#ifdef DEBUG_OUTPUT
    // Test SIMD sine function
    float test[4] = {0.0f, M_PI / 4, M_PI / 2, 3 * M_PI / 4};
    SIMD_TYPE x = SIMD_LOAD(test);
    SIMD_TYPE sin_x = SIMD_SIN(x);
    float result[4];
    SIMD_STORE(result, sin_x);
    for (int i = 0; i < 4; ++i)
        std::cerr << "sin(" << test[i] << ") = " << result[i] << std::endl;
#endif

    // Generate 24 seconds of audio samples
    int numSamples = static_cast<int>(24.0f * sampleRate);
    generateSineSamples(voices, numSamples, filter, chords);

    return 0;
}
