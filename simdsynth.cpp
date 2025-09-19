/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 8 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

#include <iostream>
#include <cmath>
#include <cstdint>
#include <vector>
#include <cstdlib>
#include <ctime>

#undef DEBUG_OUTPUT

#define MAX_VOICE_POLYPHONY 8

#ifdef __x86_64__
#include <xmmintrin.h> // SSE
#include <emmintrin.h> // SSE2
#ifdef __SSE4_1__
#include <smmintrin.h> // SSE4.1
#endif
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
#define SIMD_SIN fast_sin_ps
#elif defined(__arm64__)
#include <arm_neon.h> // NEON
#define SIMD_TYPE float32x4_t
#define SIMD_SET1 vdupq_n_f32
#define SIMD_SET(__a, __b, __c, __d) vld1q_f32((float[4]){__a, __b, __c, __d})
#define SIMD_LOAD vld1q_f32
#define SIMD_ADD vaddq_f32
#define SIMD_SUB vsubq_f32
#define SIMD_MUL vmulq_f32
#define SIMD_DIV vdivq_f32
#define SIMD_FLOOR my_floor_ps
#define SIMD_STORE vst1q_f32
#define SIMD_SIN fast_sin_ps
#else
#error "Unsupported architecture: Requires x86_64 or arm64"
#endif

// Simulate a synthesizer voice with main and sub-oscillator
struct Voice {
        float frequency;         // Main oscillator frequency (Hz)
        float phase;             // Main oscillator phase (radians)
        float phaseIncrement;    // Main oscillator per-sample phase increment
        float amplitude;         // Oscillator amplitude (from envelope)
        float cutoff;            // Filter cutoff frequency (Hz)
        float filterEnv;         // Filter envelope output
        float filterStates[4];   // 4-pole filter states
        bool active;             // Voice active state
        float fegAttack;         // Filter envelope attack time (s)
        float fegDecay;          // Filter envelope decay time (s)
        float fegSustain;        // Filter envelope sustain level (0–1)
        float fegRelease;        // Filter envelope release time (s)
        float lfoRate;           // LFO rate (Hz)
        float lfoDepth;          // LFO depth (radians)
        float lfoPhase;          // LFO phase (radians)
        float subFrequency;      // Sub-oscillator frequency (Hz)
        float subPhase;          // Sub-oscillator phase (radians)
        float subPhaseIncrement; // Sub-oscillator per-sample phase increment
        float subTune; // Sub-oscillator tuning offset in semitones (-12 to +12)
        float subMix;  // Sub-oscillator mix level (0 to 1)
        float subTrack; // Sub-oscillator pitch tracking (0 to 1, 1 = full
                        // tracking)
};

// Filter state
struct Filter {
        float resonance;  // Resonance (0 to 1)
        float sampleRate; // Sample rate (Hz)
};

// Chord structure
struct Chord {
        std::vector<float> frequencies; // Frequencies for the chord
        float startTime;                // Start time in seconds
        float duration;                 // Duration in seconds
};

// MIDI note to frequency conversion
float midiToFreq(int midiNote) {
    return 440.0f * powf(2.0f, (midiNote - 69) /
                                   12.0f); // Convert MIDI note to frequency
}

// Random float in range [base * (1 - var), base * (1 + var)]
float randomize(float base, float var) {
    float r = static_cast<float>(rand()) /
              RAND_MAX; // Generate random value between 0 and 1
    return base *
           (1.0f - var +
            r * 2.0f * var); // Scale to range [base*(1-var), base*(1+var)]
}

// Update amplitude and filter envelopes
void updateEnvelopes(Voice *voices, int numVoices, float attackTime,
                     float decayTime, float chordDuration, float sampleRate,
                     int sampleIndex, float currentTime) {
    float t = sampleIndex / sampleRate; // Current time in seconds
    for (int i = 0; i < numVoices; i++) {
        if (!voices[i].active) {
            voices[i].amplitude = 0.0f; // Clear amplitude for inactive voice
            voices[i].filterEnv =
                0.0f; // Clear filter envelope for inactive voice
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] =
                    0.0f; // Clear filter states for inactive voice
            }
            continue;
        }
        float localTime = t - currentTime; // Time relative to chord start

        // Amplitude envelope (AD, no sustain)
        if (localTime < attackTime) {
            voices[i].amplitude = localTime / attackTime; // Linear attack ramp
        } else if (localTime < attackTime + decayTime) {
            voices[i].amplitude =
                1.0f - (localTime - attackTime) / decayTime; // Linear decay
        } else {
            voices[i].amplitude = 0.0f; // Clear amplitude after decay
            voices[i].active = false;   // Deactivate voice
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] = 0.0f; // Clear filter states
            }
        }

        // Filter envelope (ADSR)
        if (localTime < voices[i].fegAttack) {
            voices[i].filterEnv =
                localTime / voices[i].fegAttack; // Linear attack
        } else if (localTime < voices[i].fegAttack + voices[i].fegDecay) {
            voices[i].filterEnv =
                1.0f -
                (localTime - voices[i].fegAttack) / voices[i].fegDecay *
                    (1.0f - voices[i].fegSustain); // Decay to sustain level
        } else if (localTime < chordDuration) {
            voices[i].filterEnv =
                voices[i].fegSustain; // Sustain for full chord duration
        } else if (localTime < chordDuration + voices[i].fegRelease) {
            voices[i].filterEnv =
                voices[i].fegSustain *
                (1.0f - (localTime - chordDuration) /
                            voices[i].fegRelease); // Linear release
        } else {
            voices[i].filterEnv = 0.0f; // Clear filter envelope
            voices[i].active = false;   // Deactivate voice
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] = 0.0f; // Clear filter states
            }
        }
    }
}

// Custom floor for NEON
#ifdef __arm64__
inline float32x4_t my_floor_ps(float32x4_t x) {
    float temp[4];
    vst1q_f32(temp, x);        // Store SIMD vector to array
    temp[0] = floorf(temp[0]); // Floor first element
    temp[1] = floorf(temp[1]); // Floor second element
    temp[2] = floorf(temp[2]); // Floor third element
    temp[3] = floorf(temp[3]); // Floor fourth element
    return vld1q_f32(temp);    // Load back to SIMD vector
}
#endif

// Fixed fast sine for x86_64 (SSE)
#ifdef __x86_64__
inline __m128 fast_sin_ps(__m128 x) {
    const __m128 twoPi =
        _mm_set1_ps(2.0f * M_PI); // 2π constant for phase wrapping
    const __m128 invTwoPi =
        _mm_set1_ps(1.0f / (2.0f * M_PI)); // 1/(2π) for phase normalization
    const __m128 piOverTwo =
        _mm_set1_ps(M_PI / 2.0f); // π/2 for range reduction

    // Wrap x to [-π, π]
    __m128 q = _mm_mul_ps(x, invTwoPi); // Scale to cycles
    q = _mm_floor_ps(q);                // Floor to integer cycles
    __m128 xWrapped =
        _mm_sub_ps(x, _mm_mul_ps(q, twoPi)); // Subtract integer cycles

    // Further reduce to [-π/2, π/2] for better polynomial accuracy
    __m128 sign = _mm_set1_ps(1.0f); // Initialize sign to positive
    __m128 absX = _mm_max_ps(
        xWrapped, _mm_sub_ps(_mm_setzero_ps(), xWrapped)); // Absolute value
    __m128 gtPiOverTwo = _mm_cmpgt_ps(absX, piOverTwo);    // Check if |x| > π/2
    sign =
        _mm_or_ps(_mm_and_ps(gtPiOverTwo, _mm_set1_ps(-1.0f)),
                  _mm_andnot_ps(gtPiOverTwo,
                                _mm_set1_ps(1.0f))); // Set sign based on range
    xWrapped = _mm_sub_ps(
        xWrapped,
        _mm_and_ps(
            gtPiOverTwo,
            _mm_mul_ps(piOverTwo, _mm_set1_ps(2.0f)))); // Reduce to [-π/2, π/2]

    // Taylor series: sin(x) ≈ x - x³/6 + x⁵/120 - x⁷/5040
    const __m128 c3 = _mm_set1_ps(-1.0f / 6.0f);    // Coefficient for x³ term
    const __m128 c5 = _mm_set1_ps(1.0f / 120.0f);   // Coefficient for x⁵ term
    const __m128 c7 = _mm_set1_ps(-1.0f / 5040.0f); // Coefficient for x⁷ term

    __m128 x2 = _mm_mul_ps(xWrapped, xWrapped); // x²
    __m128 x3 = _mm_mul_ps(x2, xWrapped);       // x³
    __m128 x5 = _mm_mul_ps(x3, x2);             // x⁵
    __m128 x7 = _mm_mul_ps(x5, x2);             // x⁷

    __m128 result = _mm_add_ps(
        xWrapped,
        _mm_add_ps(_mm_mul_ps(c3, x3),
                   _mm_add_ps(_mm_mul_ps(c5, x5),
                              _mm_mul_ps(c7, x7)))); // Sum Taylor series terms
    return _mm_mul_ps(result, sign);                 // Apply sign to result
}
#endif

// Fixed fast sine for ARM64 (NEON)
#ifdef __arm64__
inline float32x4_t fast_sin_ps(float32x4_t x) {
    const float32x4_t twoPi =
        vdupq_n_f32(2.0f * M_PI); // 2π constant for phase wrapping
    const float32x4_t invTwoPi =
        vdupq_n_f32(1.0f / (2.0f * M_PI)); // 1/(2π) for phase normalization
    const float32x4_t piOverTwo =
        vdupq_n_f32(M_PI / 2.0f); // π/2 for range reduction

    // Wrap x to [-π, π]
    float32x4_t q = vmulq_f32(x, invTwoPi); // Scale to cycles
    q = my_floor_ps(q);                     // Floor to integer cycles
    float32x4_t xWrapped =
        vsubq_f32(x, vmulq_f32(q, twoPi)); // Subtract integer cycles

    // Further reduce to [-π/2, π/2] for better polynomial accuracy
    float32x4_t sign = vdupq_n_f32(1.0f); // Initialize sign to positive
    float32x4_t absX = vmaxq_f32(
        xWrapped, vsubq_f32(vdupq_n_f32(0.0f), xWrapped)); // Absolute value
    uint32x4_t gtPiOverTwo = vcgtq_f32(absX, piOverTwo);   // Check if |x| > π/2
    sign = vbslq_f32(gtPiOverTwo, vdupq_n_f32(-1.0f),
                     vdupq_n_f32(1.0f)); // Set sign based on range
    xWrapped = vsubq_f32(xWrapped,
                         vbslq_f32(gtPiOverTwo,
                                   vmulq_f32(piOverTwo, vdupq_n_f32(2.0f)),
                                   vdupq_n_f32(0.0f))); // Reduce to [-π/2, π/2]

    // Taylor series: sin(x) ≈ x - x³/6 + x⁵/120 - x⁷/5040
    const float32x4_t c3 = vdupq_n_f32(-1.0f / 6.0f); // Coefficient for x³ term
    const float32x4_t c5 =
        vdupq_n_f32(1.0f / 120.0f); // Coefficient for x⁵ term
    const float32x4_t c7 =
        vdupq_n_f32(-1.0f / 5040.0f); // Coefficient for x⁷ term

    float32x4_t x2 = vmulq_f32(xWrapped, xWrapped); // x²
    float32x4_t x3 = vmulq_f32(x2, xWrapped);       // x³
    float32x4_t x5 = vmulq_f32(x3, x2);             // x⁵
    float32x4_t x7 = vmulq_f32(x5, x2);             // x⁷

    float32x4_t result = vaddq_f32(
        xWrapped,
        vaddq_f32(vmulq_f32(c3, x3),
                  vaddq_f32(vmulq_f32(c5, x5),
                            vmulq_f32(c7, x7)))); // Sum Taylor series terms
    return vmulq_f32(result, sign);               // Apply sign to result
}
#endif

// Compute 4-pole ladder filter for 4 voices
void applyLadderFilter(Voice *voices, int voiceOffset, SIMD_TYPE input,
                       Filter &filter, SIMD_TYPE &output) {
    float cutoffs[4];
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        float modulatedCutoff =
            voices[idx].cutoff +
            voices[idx].filterEnv * 2000.0f; // Modulate cutoff with envelope
        modulatedCutoff = std::max(
            200.0f, std::min(modulatedCutoff,
                             filter.sampleRate / 2.0f)); // Clamp cutoff
        cutoffs[i] =
            1.0f - expf(-2.0f * M_PI * modulatedCutoff /
                        filter.sampleRate); // Compute filter coefficient
        if (std::isnan(cutoffs[i]) || !std::isfinite(cutoffs[i]))
            cutoffs[i] = 0.0f; // Prevent nan
    }
#ifdef __x86_64__
    SIMD_TYPE alpha = SIMD_SET(cutoffs[0], cutoffs[1], cutoffs[2],
                               cutoffs[3]); // Load filter coefficients (x86_64)
#elif defined(__arm64__)
    float temp[4] = {cutoffs[0], cutoffs[1], cutoffs[2], cutoffs[3]};
    SIMD_TYPE alpha = SIMD_LOAD(temp); // Load filter coefficients (ARM64)
#endif
    SIMD_TYPE resonance =
        SIMD_SET1(std::min(filter.resonance * 4.0f, 4.0f)); // Clamp resonance

    // Check if any voices in the group are active
    bool anyActive = false;
    for (int i = 0; i < 4; i++) {
        if (voiceOffset + i < MAX_VOICE_POLYPHONY &&
            voices[voiceOffset + i].active) {
            anyActive = true; // At least one voice is active
        }
    }
    if (!anyActive) {
        output = SIMD_SET1(0.0f); // Output zero for inactive voice group
        return;
    }

    SIMD_TYPE states[4];
    for (int i = 0; i < 4; i++) {
        float temp[4] = {
            voices[voiceOffset].filterStates[i],
            voices[voiceOffset + 1].filterStates[i],
            voices[voiceOffset + 2].filterStates[i],
            voices[voiceOffset + 3].filterStates[i]}; // Load filter states
        states[i] = SIMD_LOAD(temp);
    }

    SIMD_TYPE feedback = SIMD_MUL(states[3], resonance); // Compute feedback
    SIMD_TYPE filterInput =
        SIMD_SUB(input, feedback); // Subtract feedback from input

    // Four-pole ladder filter stages
    states[0] =
        SIMD_ADD(states[0], SIMD_MUL(alpha, SIMD_SUB(filterInput, states[0])));
    states[1] =
        SIMD_ADD(states[1], SIMD_MUL(alpha, SIMD_SUB(states[0], states[1])));
    states[2] =
        SIMD_ADD(states[2], SIMD_MUL(alpha, SIMD_SUB(states[1], states[2])));
    states[3] =
        SIMD_ADD(states[3], SIMD_MUL(alpha, SIMD_SUB(states[2], states[3])));

    output = states[3]; // Filter output is final stage

    // Clamp output to prevent nan or infinite values
    float tempCheck[4];
    SIMD_STORE(tempCheck, output);
    for (int i = 0; i < 4; i++) {
        if (!std::isfinite(tempCheck[i])) {
            tempCheck[i] = 0.0f; // Replace non-finite values with 0
        } else {
            tempCheck[i] = std::max(
                -1.0f, std::min(1.0f, tempCheck[i])); // Clamp to [-1, 1]
        }
    }
    output = SIMD_LOAD(tempCheck); // Load clamped output

    // Debug filter output
    float tempOut[4];
    SIMD_STORE(tempOut, output);
    if (std::isnan(tempOut[0]) || std::isnan(tempOut[1]) ||
        std::isnan(tempOut[2]) || std::isnan(tempOut[3])) {
        std::cerr << "Filter output nan at voiceOffset " << voiceOffset << ": {"
                  << tempOut[0] << ", " << tempOut[1] << ", " << tempOut[2]
                  << ", " << tempOut[3] << "}" << std::endl; // Log nan outputs
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

// Generate sine samples with main and sub-oscillator
void generateSineSamples(Voice *voices, int numSamples, Filter &filter,
                         const std::vector<Chord> &chords) {
    const SIMD_TYPE twoPi =
        SIMD_SET1(2.0f * M_PI); // Constant for phase wrapping
    float attackTime = 0.1f;    // Amplitude envelope attack time (seconds)
    float decayTime = 1.9f;     // Amplitude envelope decay time (seconds)
    float currentTime = 0.0f;   // Current chord start time (seconds)
    size_t currentChord = 0;    // Index of current chord

    for (int i = 0; i < numSamples; i++) {
        float t = i / filter.sampleRate; // Current time in seconds

        // Check if it's time to start a new chord
        if (currentChord < chords.size() &&
            t >= chords[currentChord].startTime) {
            if (currentTime != chords[currentChord].startTime) {
                currentTime =
                    chords[currentChord].startTime; // Update chord start time
                for (size_t v = 0; v < MAX_VOICE_POLYPHONY; v++) {
                    voices[v].active =
                        v <
                        chords[currentChord]
                            .frequencies.size(); // Activate voices for chord
                    if (voices[v].active) {
                        voices[v].frequency =
                            chords[currentChord]
                                .frequencies[v]; // Set main oscillator
                                                 // frequency
                        voices[v].phaseIncrement =
                            (2.0f * M_PI * voices[v].frequency) /
                            filter
                                .sampleRate; // Main oscillator phase increment
                        voices[v].phase = 0.0f; // Reset main oscillator phase
                        voices[v].lfoPhase = 0.0f; // Reset LFO phase
                        voices[v].fegAttack = randomize(
                            0.1f, 0.2f); // Randomize filter envelope attack
                        voices[v].fegDecay = randomize(
                            1.0f, 0.2f); // Randomize filter envelope decay
                        voices[v].fegSustain = randomize(
                            0.5f, 0.2f); // Randomize filter envelope sustain
                        voices[v].fegRelease = randomize(
                            0.2f, 0.2f); // Randomize filter envelope release
                        voices[v].lfoRate = 0;  // Disable LFO for simplicity
                        voices[v].lfoDepth = 0; // Disable LFO for simplicity
                        // Initialize sub-oscillator parameters
                        voices[v].subTune =
                            -12.0f; // Default: one octave below (-12 semitones)
                        voices[v].subMix =
                            0.5f; // Default: equal mix with main oscillator
                        voices[v].subTrack =
                            1.0f; // Default: full pitch tracking
                        // Calculate sub-oscillator frequency with tuning and
                        // tracking
                        float subFreq = voices[v].frequency *
                                        powf(2.0f, voices[v].subTune / 12.0f) *
                                        voices[v].subTrack;
                        voices[v].subFrequency =
                            subFreq; // Set sub-oscillator frequency
                        voices[v].subPhaseIncrement =
                            (2.0f * M_PI * subFreq) /
                            filter.sampleRate; // Sub-oscillator phase increment
                        voices[v].subPhase = 0.0f; // Reset sub-oscillator phase
                    } else {
                        voices[v].amplitude =
                            0.0f; // Clear amplitude for inactive voice
                        voices[v].filterEnv =
                            0.0f; // Clear filter envelope for inactive voice
                        for (int j = 0; j < 4; j++) {
                            voices[v].filterStates[j] =
                                0.0f; // Clear filter states
                        }
                    }
                }
            }
        }

        // Advance to next chord after current chord finishes
        if (currentChord < chords.size() &&
            t >= chords[currentChord].startTime +
                     chords[currentChord].duration) {
            currentChord++; // Move to next chord
        }

        float chordDuration = (currentChord < chords.size())
                                  ? chords[currentChord].duration
                                  : 2.0f; // Default duration if no chord
        updateEnvelopes(voices, MAX_VOICE_POLYPHONY, attackTime, decayTime,
                        chordDuration, filter.sampleRate, i,
                        currentTime); // Update envelopes

        float outputSample = 0.0f; // Initialize output sample
#ifdef DEBUG_OUTPUT
        int activeVoices = 0; // Reset active voice count for debug
        for (int v = 0; v < MAX_VOICE_POLYPHONY; v++) {
            if (voices[v].active) activeVoices++; // Count active voices
        }
#endif

        for (int group = 0; group < (MAX_VOICE_POLYPHONY + 3) / 4; group++) {
            int voiceOffset =
                group * 4; // Calculate voice offset for SIMD group
            if (voiceOffset >= MAX_VOICE_POLYPHONY) {
                std::cerr << "Error: voiceOffset " << voiceOffset
                          << " exceeds MAX_VOICE_POLYPHONY"
                          << std::endl; // Prevent out-of-bounds
                continue;
            }

            // Load main oscillator parameters
            float tempAmps[4] = {
                voices[voiceOffset].amplitude,
                voices[voiceOffset + 1].amplitude,
                voices[voiceOffset + 2].amplitude,
                voices[voiceOffset + 3].amplitude}; // Amplitudes for 4 voices
            float tempPhases[4] = {
                voices[voiceOffset].phase, voices[voiceOffset + 1].phase,
                voices[voiceOffset + 2].phase,
                voices[voiceOffset + 3].phase}; // Main oscillator phases
            float tempIncrements[4] = {
                voices[voiceOffset].phaseIncrement,
                voices[voiceOffset + 1].phaseIncrement,
                voices[voiceOffset + 2].phaseIncrement,
                voices[voiceOffset + 3]
                    .phaseIncrement}; // Main oscillator increments
            float tempLfoPhases[4] = {
                voices[voiceOffset].lfoPhase, voices[voiceOffset + 1].lfoPhase,
                voices[voiceOffset + 2].lfoPhase,
                voices[voiceOffset + 3].lfoPhase}; // LFO phases
            float tempLfoRates[4] = {
                voices[voiceOffset].lfoRate, voices[voiceOffset + 1].lfoRate,
                voices[voiceOffset + 2].lfoRate,
                voices[voiceOffset + 3].lfoRate}; // LFO rates
            float tempLfoDepths[4] = {
                voices[voiceOffset].lfoDepth, voices[voiceOffset + 1].lfoDepth,
                voices[voiceOffset + 2].lfoDepth,
                voices[voiceOffset + 3].lfoDepth}; // LFO depths
            // Load sub-oscillator parameters
            float tempSubPhases[4] = {
                voices[voiceOffset].subPhase, voices[voiceOffset + 1].subPhase,
                voices[voiceOffset + 2].subPhase,
                voices[voiceOffset + 3].subPhase}; // Sub-oscillator phases
            float tempSubIncrements[4] = {
                voices[voiceOffset].subPhaseIncrement,
                voices[voiceOffset + 1].subPhaseIncrement,
                voices[voiceOffset + 2].subPhaseIncrement,
                voices[voiceOffset + 3]
                    .subPhaseIncrement}; // Sub-oscillator increments
            float tempSubMixes[4] = {
                voices[voiceOffset].subMix, voices[voiceOffset + 1].subMix,
                voices[voiceOffset + 2].subMix,
                voices[voiceOffset + 3].subMix}; // Sub-oscillator mix levels
            SIMD_TYPE amplitudes = SIMD_LOAD(tempAmps); // Load amplitudes
            SIMD_TYPE phases =
                SIMD_LOAD(tempPhases); // Load main oscillator phases
            SIMD_TYPE increments =
                SIMD_LOAD(tempIncrements); // Load main oscillator increments
            SIMD_TYPE lfoPhases = SIMD_LOAD(tempLfoPhases); // Load LFO phases
            SIMD_TYPE lfoRates = SIMD_LOAD(tempLfoRates);   // Load LFO rates
            SIMD_TYPE lfoDepths = SIMD_LOAD(tempLfoDepths); // Load LFO depths
            SIMD_TYPE subPhases =
                SIMD_LOAD(tempSubPhases); // Load sub-oscillator phases
            SIMD_TYPE subIncrements =
                SIMD_LOAD(tempSubIncrements); // Load sub-oscillator increments
            SIMD_TYPE subMixes =
                SIMD_LOAD(tempSubMixes); // Load sub-oscillator mix levels

            // Compute LFO values (currently disabled with lfoRate = 0)
            SIMD_TYPE lfoIncrements = SIMD_MUL(
                lfoRates, SIMD_SET1(2.0f * M_PI /
                                    filter.sampleRate)); // LFO phase increments
            lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements); // Update LFO phases
            lfoPhases = SIMD_SUB(
                lfoPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)),
                                    twoPi)); // Wrap LFO phases
            SIMD_TYPE lfoValues =
                SIMD_SIN(lfoPhases); // Compute LFO sine values
            lfoValues = SIMD_MUL(lfoValues, lfoDepths); // Apply LFO depth
            phases = SIMD_ADD(phases,
                              lfoValues); // Apply LFO to main oscillator phases

            // Compute main oscillator output
            SIMD_TYPE mainSinValues =
                SIMD_SIN(phases); // Main oscillator sine values
            mainSinValues =
                SIMD_MUL(mainSinValues, amplitudes); // Apply amplitude envelope
            mainSinValues = SIMD_MUL(
                mainSinValues,
                SIMD_SUB(SIMD_SET1(1.0f), subMixes)); // Scale by (1 - subMix)

            // Compute sub-oscillator output
            SIMD_TYPE subSinValues =
                SIMD_SIN(subPhases); // Sub-oscillator sine values
            subSinValues =
                SIMD_MUL(subSinValues, amplitudes); // Apply amplitude envelope
            subSinValues = SIMD_MUL(subSinValues, subMixes); // Scale by subMix

            // Combine main and sub-oscillator outputs
            SIMD_TYPE sinValues =
                SIMD_ADD(mainSinValues,
                         subSinValues); // Sum main and sub-oscillator signals

            // Apply filter
            SIMD_TYPE filteredOutput;
            applyLadderFilter(
                voices, voiceOffset, sinValues, filter,
                filteredOutput); // Filter combined oscillator output

#ifdef DEBUG_OUTPUT
            float tempDebugSin[4], tempDebugFiltered[4], tempDebugAmps[4];
            SIMD_STORE(tempDebugSin, sinValues);
            SIMD_STORE(tempDebugAmps, amplitudes);
            SIMD_STORE(tempDebugFiltered, filteredOutput);
            if (i % 1000 == 0) {
                std::cerr << "Sample " << i << ": sinValues = {"
                          << tempDebugSin[0] << ", " << tempDebugSin[1] << ", "
                          << tempDebugSin[2] << ", " << tempDebugSin[3]
                          << "}, amplitudes = {" << tempDebugAmps[0] << ", "
                          << tempDebugAmps[1] << ", " << tempDebugAmps[2]
                          << ", " << tempDebugAmps[3] << "}, filteredOutput = {"
                          << tempDebugFiltered[0] << ", "
                          << tempDebugFiltered[1] << ", "
                          << tempDebugFiltered[2] << ", "
                          << tempDebugFiltered[3] << "}"
                          << std::endl; // Debug signal path
            }
#endif

            float temp[4];
            SIMD_STORE(temp, filteredOutput); // Store filtered output
            outputSample +=
                (temp[0] + temp[1] + temp[2] + temp[3]); // Sum filtered outputs

            // Update main oscillator phases
            phases =
                SIMD_ADD(phases, increments); // Advance main oscillator phases
            SIMD_TYPE wrap = SIMD_SUB(
                phases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(phases, twoPi)),
                                 twoPi)); // Wrap main oscillator phases
            SIMD_STORE(temp, wrap);
            voices[voiceOffset].phase = temp[0];
            voices[voiceOffset + 1].phase = temp[1];
            voices[voiceOffset + 2].phase = temp[2];
            voices[voiceOffset + 3].phase = temp[3];

            // Update sub-oscillator phases
            subPhases = SIMD_ADD(
                subPhases, subIncrements); // Advance sub-oscillator phases
            SIMD_TYPE subWrap = SIMD_SUB(
                subPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(subPhases, twoPi)),
                                    twoPi)); // Wrap sub-oscillator phases
            SIMD_STORE(temp, subWrap);
            voices[voiceOffset].subPhase = temp[0];
            voices[voiceOffset + 1].subPhase = temp[1];
            voices[voiceOffset + 2].subPhase = temp[2];
            voices[voiceOffset + 3].subPhase = temp[3];

            // Update LFO phases
            SIMD_STORE(temp, lfoPhases);
            voices[voiceOffset].lfoPhase = temp[0];
            voices[voiceOffset + 1].lfoPhase = temp[1];
            voices[voiceOffset + 2].lfoPhase = temp[2];
            voices[voiceOffset + 3].lfoPhase = temp[3];
        }

        outputSample *= 0.5f; // Scale output for audible level

        // Debug print every 1000 samples
#ifdef DEBUG_OUTPUT
        if (i % 1000 == 0) {
            std::cerr << "Sample " << i << ": outputSample = " << outputSample
                      << ", activeVoices = " << activeVoices
                      << std::endl; // Debug output sample
        }
#endif

        // Prevent nan output
        if (std::isnan(outputSample) || !std::isfinite(outputSample)) {
            outputSample = 0.0f; // Replace non-finite output with 0
            std::cerr << "Prevented NAN output!"
                      << std::endl; // Log nan prevention
        }

        fwrite(&outputSample, sizeof(float), 1, stdout); // Output sample
    }
}

int main() {
    srand(1234);                       // Seed random number generator
    const float sampleRate = 48000.0f; // Sample rate (Hz)
    Voice voices[MAX_VOICE_POLYPHONY];
    for (int i = 0; i < MAX_VOICE_POLYPHONY; i++) {
        voices[i].frequency = 0.0f; // Initialize main oscillator frequency
        voices[i].phase = 0.0f;     // Initialize main oscillator phase
        voices[i].phaseIncrement =
            0.0f; // Initialize main oscillator phase increment
        voices[i].amplitude = 0.0f;    // Initialize amplitude
        voices[i].cutoff = 1000.0f;    // Initialize filter cutoff
        voices[i].filterEnv = 0.0f;    // Initialize filter envelope
        voices[i].active = false;      // Initialize voice as inactive
        voices[i].fegAttack = 0.1f;    // Initialize filter envelope attack
        voices[i].fegDecay = 1.0f;     // Initialize filter envelope decay
        voices[i].fegSustain = 0.5f;   // Initialize filter envelope sustain
        voices[i].fegRelease = 0.2f;   // Initialize filter envelope release
        voices[i].lfoRate = 1.0f;      // Initialize LFO rate
        voices[i].lfoDepth = 0.01f;    // Initialize LFO depth
        voices[i].lfoPhase = 0.0f;     // Initialize LFO phase
        voices[i].subFrequency = 0.0f; // Initialize sub-oscillator frequency
        voices[i].subPhase = 0.0f;     // Initialize sub-oscillator phase
        voices[i].subPhaseIncrement =
            0.0f; // Initialize sub-oscillator phase increment
        voices[i].subTune =
            -12.0f; // Initialize sub-oscillator tuning (one octave below)
        voices[i].subMix =
            0.5f; // Initialize sub-oscillator mix (equal with main)
        voices[i].subTrack =
            1.0f; // Initialize sub-oscillator tracking (full tracking)
        for (int j = 0; j < 4; j++) {
            voices[i].filterStates[j] = 0.0f; // Initialize filter states
        }
    }

    Filter filter;
    filter.resonance = 0.7f;        // Set filter resonance
    filter.sampleRate = sampleRate; // Set filter sample rate

    std::vector<Chord> chords;
    chords.emplace_back(Chord{{midiToFreq(49), midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63)}, 0.0f, 2.0f}); // Chord 1
    chords.emplace_back( Chord{{midiToFreq(54), midiToFreq(58), midiToFreq(61), midiToFreq(65)}, 2.0f, 2.0f}); // Chord 2
    chords.emplace_back( Chord{{midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68)}, 4.0f, 2.0f}); // Chord 3
    chords.emplace_back(Chord{{midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67)}, 6.0f, 2.0f}); // Chord 4
    chords.emplace_back( Chord{{midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67)}, 8.0f, 2.0f}); // Chord 5
    chords.emplace_back(Chord{{midiToFreq(51), midiToFreq(55), midiToFreq(58), midiToFreq(62), midiToFreq(65)}, 10.0f, 2.0f}); // Chord 6
    chords.emplace_back( Chord{{midiToFreq(60), midiToFreq(63), midiToFreq(67), midiToFreq(70)}, 12.0f, 2.0f}); // Chord 7
    chords.emplace_back(Chord{{midiToFreq(54), midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68)}, 14.0f, 2.0f}); // Chord 8
    chords.emplace_back( Chord{{midiToFreq(61), midiToFreq(65), midiToFreq(68), midiToFreq(72)}, 16.0f, 2.0f}); // Chord 9
    chords.emplace_back(Chord{{midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68), midiToFreq(72)}, 18.0f, 2.0f}); // Chord 10
    chords.emplace_back( Chord{{midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63)}, 20.0f, 2.0f}); // Chord 11
    chords.emplace_back(Chord{{midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67), midiToFreq(70)}, 22.0f, 2.0f}); // Chord 12

#if DEBUG_OUTPUT
    // Debug sine function
    float test[4] = {0.0f, M_PI / 4, M_PI / 2, 3 * M_PI / 4};
    SIMD_TYPE x = SIMD_LOAD(test);
    SIMD_TYPE sin_x = SIMD_SIN(x);
    float result[4];
    SIMD_STORE(result, sin_x);
    for (int i = 0; i < 4; ++i)
        std::cerr << "sin(" << test[i] << ") = " << result[i]
                  << std::endl; // Debug sine function output
#endif

    int numSamples =
        static_cast<int>(24.0f * sampleRate); // Total samples for 24 seconds
    generateSineSamples(voices, numSamples, filter, chords); // Generate audio

    return 0;
}
