/*
 *
 *	simdsynth - a playground for experimenting with SIMD-based audio
 *			    synthesis, with simple polyphonic oscillator,
 * 				filter, envelopes and LFO per voice, up to 8 voices.
 *
 *	MIT Licensed, (c) 2025, seclorum
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

// Simulate a synthesizer voice
struct Voice {
        float frequency;       // Oscillator frequency (Hz)
        float phase;           // Oscillator phase (radians)
        float phaseIncrement;  // Per-sample phase increment
        float amplitude;       // Oscillator amplitude (from envelope)
        float cutoff;          // Filter cutoff frequency (Hz)
        float filterEnv;       // Filter envelope output
        float filterStates[4]; // 4-pole filter states
        bool active;           // Voice active state
        float fegAttack;       // Filter envelope attack time (s)
        float fegDecay;        // Filter envelope decay time (s)
        float fegSustain;      // Filter envelope sustain level (0–1)
        float fegRelease;      // Filter envelope release time (s)
        float lfoRate;         // LFO rate (Hz)
        float lfoDepth;        // LFO depth (radians)
        float lfoPhase;        // LFO phase (radians)
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
    return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
}

// Random float in range [base * (1 - var), base * (1 + var)]
float randomize(float base, float var) {
    float r = static_cast<float>(rand()) / RAND_MAX; // 0 to 1
    return base * (1.0f - var + r * 2.0f * var);
}

// Update amplitude and filter envelopes
void updateEnvelopes(Voice *voices, int numVoices, float attackTime,
                     float decayTime, float chordDuration, float sampleRate,
                     int sampleIndex, float currentTime) {
    float t = sampleIndex / sampleRate; // Time in seconds
    for (int i = 0; i < numVoices; i++) {
        if (!voices[i].active) {
            voices[i].amplitude = 0.0f;
            voices[i].filterEnv = 0.0f;
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] =
                    0.0f; // Clear filter states for inactive voices
            }
            continue;
        }
        float localTime = t - currentTime; // Time relative to chord start

        // Amplitude envelope (AD, no sustain)
        if (localTime < attackTime) {
            voices[i].amplitude = localTime / attackTime; // Linear attack
        } else if (localTime < attackTime + decayTime) {
            voices[i].amplitude =
                1.0f - (localTime - attackTime) / decayTime; // Linear decay
        } else {
            voices[i].amplitude = 0.0f;
            voices[i].active = false;
            for (int j = 0; j < 4; j++) {
                voices[i].filterStates[j] = 0.0f; // Clear filter states
            }
        }

        // Filter envelope (ADSR)
        if (localTime < voices[i].fegAttack) {
            voices[i].filterEnv = localTime / voices[i].fegAttack; // Attack
        } else if (localTime < voices[i].fegAttack + voices[i].fegDecay) {
            voices[i].filterEnv =
                1.0f - (localTime - voices[i].fegAttack) / voices[i].fegDecay *
                           (1.0f - voices[i].fegSustain); // Decay to sustain
        } else if (localTime < chordDuration) {
            voices[i].filterEnv =
                voices[i].fegSustain; // Sustain for full chord duration
        } else if (localTime < chordDuration + voices[i].fegRelease) {
            voices[i].filterEnv = voices[i].fegSustain *
                                  (1.0f - (localTime - chordDuration) /
                                              voices[i].fegRelease); // Release
        } else {
            voices[i].filterEnv = 0.0f;
            voices[i].active = false;
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
    vst1q_f32(temp, x);
    temp[0] = floorf(temp[0]);
    temp[1] = floorf(temp[1]);
    temp[2] = floorf(temp[2]);
    temp[3] = floorf(temp[3]);
    return vld1q_f32(temp);
}
#endif

// Fixed fast sine for x86_64 (SSE)
#ifdef __x86_64__
inline __m128 fast_sin_ps(__m128 x) {
    const __m128 twoPi = _mm_set1_ps(2.0f * M_PI);
    const __m128 invTwoPi = _mm_set1_ps(1.0f / (2.0f * M_PI));
    const __m128 piOverTwo = _mm_set1_ps(M_PI / 2.0f);

    // Wrap x to [-π, π]
    __m128 q = _mm_mul_ps(x, invTwoPi);
    q = _mm_floor_ps(q);
    __m128 xWrapped = _mm_sub_ps(x, _mm_mul_ps(q, twoPi));

    // Further reduce to [-π/2, π/2] for better polynomial accuracy
    __m128 sign = _mm_set1_ps(1.0f);
    __m128 absX = _mm_max_ps(xWrapped, _mm_sub_ps(_mm_setzero_ps(), xWrapped));
    __m128 gtPiOverTwo = _mm_cmpgt_ps(absX, piOverTwo);
    sign = _mm_or_ps(_mm_and_ps(gtPiOverTwo, _mm_set1_ps(-1.0f)),
                     _mm_andnot_ps(gtPiOverTwo, _mm_set1_ps(1.0f)));
    xWrapped = _mm_sub_ps(
        xWrapped,
        _mm_and_ps(gtPiOverTwo, _mm_mul_ps(piOverTwo, _mm_set1_ps(2.0f))));

    // Taylor series: sin(x) ≈ x - x³/6 + x⁵/120 - x⁷/5040
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

// Fixed fast sine for ARM64 (NEON)
#ifdef __arm64__
inline float32x4_t fast_sin_ps(float32x4_t x) {
    const float32x4_t twoPi = vdupq_n_f32(2.0f * M_PI);
    const float32x4_t invTwoPi = vdupq_n_f32(1.0f / (2.0f * M_PI));
    const float32x4_t piOverTwo = vdupq_n_f32(M_PI / 2.0f);

    // Wrap x to [-π, π]
    float32x4_t q = vmulq_f32(x, invTwoPi);
    q = my_floor_ps(q);
    float32x4_t xWrapped = vsubq_f32(x, vmulq_f32(q, twoPi));

    // Further reduce to [-π/2, π/2] for better polynomial accuracy
    float32x4_t sign = vdupq_n_f32(1.0f);
    float32x4_t absX =
        vmaxq_f32(xWrapped, vsubq_f32(vdupq_n_f32(0.0f), xWrapped));
    uint32x4_t gtPiOverTwo = vcgtq_f32(absX, piOverTwo);
    sign = vbslq_f32(gtPiOverTwo, vdupq_n_f32(-1.0f), vdupq_n_f32(1.0f));
    xWrapped =
        vsubq_f32(xWrapped, vbslq_f32(gtPiOverTwo,
                                      vmulq_f32(piOverTwo, vdupq_n_f32(2.0f)),
                                      vdupq_n_f32(0.0f)));

    // Taylor series: sin(x) ≈ x - x³/6 + x⁵/120 - x⁷/5040
    const float32x4_t c3 = vdupq_n_f32(-1.0f / 6.0f);
    const float32x4_t c5 = vdupq_n_f32(1.0f / 120.0f);
    const float32x4_t c7 = vdupq_n_f32(-1.0f / 5040.0f);

    float32x4_t x2 = vmulq_f32(xWrapped, xWrapped);
    float32x4_t x3 = vmulq_f32(x2, xWrapped);
    float32x4_t x5 = vmulq_f32(x3, x2);
    float32x4_t x7 = vmulq_f32(x5, x2);

    float32x4_t result = vaddq_f32(
        xWrapped, vaddq_f32(vmulq_f32(c3, x3),
                            vaddq_f32(vmulq_f32(c5, x5), vmulq_f32(c7, x7))));
    return vmulq_f32(result, sign);
}
#endif

// Compute 4-pole ladder filter for 4 voices
void applyLadderFilter(Voice *voices, int voiceOffset, SIMD_TYPE input,
                       Filter &filter, SIMD_TYPE &output) {
    float cutoffs[4];
    for (int i = 0; i < 4; i++) {
        int idx = voiceOffset + i;
        float modulatedCutoff =
            voices[idx].cutoff + voices[idx].filterEnv * 2000.0f;
        modulatedCutoff = std::max(
            200.0f, std::min(modulatedCutoff, filter.sampleRate / 2.0f));
        cutoffs[i] =
            1.0f - expf(-2.0f * M_PI * modulatedCutoff / filter.sampleRate);
        if (std::isnan(cutoffs[i]) || !std::isfinite(cutoffs[i]))
            cutoffs[i] = 0.0f; // Prevent nan
    }
#ifdef __x86_64__
    SIMD_TYPE alpha = SIMD_SET(cutoffs[0], cutoffs[1], cutoffs[2], cutoffs[3]);
#elif defined(__arm64__)
    float temp[4] = {cutoffs[0], cutoffs[1], cutoffs[2], cutoffs[3]};
    SIMD_TYPE alpha = SIMD_LOAD(temp);
#endif
    SIMD_TYPE resonance =
        SIMD_SET1(std::min(filter.resonance * 4.0f, 4.0f)); // Clamp resonance

    // Check if any voices in the group are active
    bool anyActive = false;
    for (int i = 0; i < 4; i++) {
        if (voiceOffset + i < MAX_VOICE_POLYPHONY &&
            voices[voiceOffset + i].active) {
            anyActive = true;
        }
    }
    if (!anyActive) {
        output = SIMD_SET1(0.0f);
        return;
    }

    SIMD_TYPE states[4];
    for (int i = 0; i < 4; i++) {
        float temp[4] = {voices[voiceOffset].filterStates[i],
                         voices[voiceOffset + 1].filterStates[i],
                         voices[voiceOffset + 2].filterStates[i],
                         voices[voiceOffset + 3].filterStates[i]};
        states[i] = SIMD_LOAD(temp);
    }

    SIMD_TYPE feedback = SIMD_MUL(states[3], resonance);
    SIMD_TYPE filterInput = SIMD_SUB(input, feedback);

    states[0] =
        SIMD_ADD(states[0], SIMD_MUL(alpha, SIMD_SUB(filterInput, states[0])));
    states[1] =
        SIMD_ADD(states[1], SIMD_MUL(alpha, SIMD_SUB(states[0], states[1])));
    states[2] =
        SIMD_ADD(states[2], SIMD_MUL(alpha, SIMD_SUB(states[1], states[2])));
    states[3] =
        SIMD_ADD(states[3], SIMD_MUL(alpha, SIMD_SUB(states[2], states[3])));

    output = states[3];

    // Clamp output to prevent nan or infinite values
    float tempCheck[4];
    SIMD_STORE(tempCheck, output);
    for (int i = 0; i < 4; i++) {
        if (!std::isfinite(tempCheck[i])) {
            tempCheck[i] = 0.0f;
        } else {
            tempCheck[i] =
                std::max(-1.0f, std::min(1.0f, tempCheck[i])); // Clamp output
        }
    }
    output = SIMD_LOAD(tempCheck);

    // Debug filter output
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

// Generate sine samples
void generateSineSamples(Voice *voices, int numSamples, Filter &filter,
                         const std::vector<Chord> &chords) {
    const SIMD_TYPE twoPi = SIMD_SET1(2.0f * M_PI);
    float attackTime = 0.1f;
    float decayTime = 1.9f;
    float currentTime = 0.0f;
    size_t currentChord = 0;

    for (int i = 0; i < numSamples; i++) {
        float t = i / filter.sampleRate;

        if (currentChord < chords.size() &&
            t >= chords[currentChord].startTime) {
            if (currentTime != chords[currentChord].startTime) {
                currentTime = chords[currentChord].startTime;
                for (size_t v = 0; v < MAX_VOICE_POLYPHONY; v++) {
                    voices[v].active =
                        v < chords[currentChord].frequencies.size();
                    if (voices[v].active) {
                        voices[v].frequency =
                            chords[currentChord].frequencies[v];
                        voices[v].phaseIncrement =
                            (2.0f * M_PI * voices[v].frequency) /
                            filter.sampleRate;
                        voices[v].phase = 0.0f;
                        voices[v].lfoPhase = 0.0f;
                        voices[v].fegAttack = randomize(0.1f, 0.2f);
                        voices[v].fegDecay = randomize(1.0f, 0.2f);
                        voices[v].fegSustain = randomize(0.5f, 0.2f);
                        voices[v].fegRelease = randomize(0.2f, 0.2f);
                        voices[v].lfoRate = 0;  // randomize(1.0f, 0.125f);
                        voices[v].lfoDepth = 0; // randomize(0.1f, 0.125f);
                    } else {
                        voices[v].amplitude = 0.0f;
                        voices[v].filterEnv = 0.0f;
                        for (int j = 0; j < 4; j++) {
                            voices[v].filterStates[j] = 0.0f;
                        }
                    }
                }
            }
        }

        // Advance to next chord after current chord finishes
        if (currentChord < chords.size() &&
            t >= chords[currentChord].startTime +
                     chords[currentChord].duration) {
            currentChord++;
        }

        float chordDuration = (currentChord < chords.size())
                                  ? chords[currentChord].duration
                                  : 2.0f;
        updateEnvelopes(voices, MAX_VOICE_POLYPHONY, attackTime, decayTime,
                        chordDuration, filter.sampleRate, i, currentTime);

        float outputSample = 0.0f;
#ifdef DEBUG_OUTPUT
        int activeVoices = 0; // Reset per sample
        for (int v = 0; v < MAX_VOICE_POLYPHONY; v++) {
            if (voices[v].active) activeVoices++;
        }
#endif

        for (int group = 0; group < (MAX_VOICE_POLYPHONY + 3) / 4; group++) {
            int voiceOffset = group * 4;
            if (voiceOffset >= MAX_VOICE_POLYPHONY) {
                std::cerr << "Error: voiceOffset " << voiceOffset
                          << " exceeds MAX_VOICE_POLYPHONY" << std::endl;
                continue;
            }

            float tempAmps[4] = {voices[voiceOffset].amplitude,
                                 voices[voiceOffset + 1].amplitude,
                                 voices[voiceOffset + 2].amplitude,
                                 voices[voiceOffset + 3].amplitude};
            float tempPhases[4] = {
                voices[voiceOffset].phase, voices[voiceOffset + 1].phase,
                voices[voiceOffset + 2].phase, voices[voiceOffset + 3].phase};
            float tempIncrements[4] = {voices[voiceOffset].phaseIncrement,
                                       voices[voiceOffset + 1].phaseIncrement,
                                       voices[voiceOffset + 2].phaseIncrement,
                                       voices[voiceOffset + 3].phaseIncrement};
            float tempLfoPhases[4] = {voices[voiceOffset].lfoPhase,
                                      voices[voiceOffset + 1].lfoPhase,
                                      voices[voiceOffset + 2].lfoPhase,
                                      voices[voiceOffset + 3].lfoPhase};
            float tempLfoRates[4] = {voices[voiceOffset].lfoRate,
                                     voices[voiceOffset + 1].lfoRate,
                                     voices[voiceOffset + 2].lfoRate,
                                     voices[voiceOffset + 3].lfoRate};
            float tempLfoDepths[4] = {voices[voiceOffset].lfoDepth,
                                      voices[voiceOffset + 1].lfoDepth,
                                      voices[voiceOffset + 2].lfoDepth,
                                      voices[voiceOffset + 3].lfoDepth};
            SIMD_TYPE amplitudes = SIMD_LOAD(tempAmps);
            SIMD_TYPE phases = SIMD_LOAD(tempPhases);
            SIMD_TYPE increments = SIMD_LOAD(tempIncrements);
            SIMD_TYPE lfoPhases = SIMD_LOAD(tempLfoPhases);
            SIMD_TYPE lfoRates = SIMD_LOAD(tempLfoRates);
            SIMD_TYPE lfoDepths = SIMD_LOAD(tempLfoDepths);

            SIMD_TYPE lfoIncrements =
                SIMD_MUL(lfoRates, SIMD_SET1(2.0f * M_PI / filter.sampleRate));
            lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
            lfoPhases = SIMD_SUB(
                lfoPhases,
                SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)), twoPi));
            SIMD_TYPE lfoValues = SIMD_SIN(lfoPhases);
            lfoValues = SIMD_MUL(lfoValues, lfoDepths);
            phases = SIMD_ADD(phases, lfoValues);

            SIMD_TYPE sinValues = SIMD_SIN(phases);
            sinValues = SIMD_MUL(sinValues, amplitudes);

            SIMD_TYPE filteredOutput;
            applyLadderFilter(voices, voiceOffset, sinValues, filter,
                              filteredOutput);

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
                          << tempDebugFiltered[3] << "}" << std::endl;
            }
#endif

            float temp[4];
            SIMD_STORE(temp, filteredOutput);
            outputSample += (temp[0] + temp[1] + temp[2] + temp[3]);

            // Update phases
            phases = SIMD_ADD(phases, increments);
            SIMD_TYPE wrap = SIMD_SUB(
                phases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(phases, twoPi)), twoPi));
            SIMD_STORE(temp, wrap);
            voices[voiceOffset].phase = temp[0];
            voices[voiceOffset + 1].phase = temp[1];
            voices[voiceOffset + 2].phase = temp[2];
            voices[voiceOffset + 3].phase = temp[3];
            SIMD_STORE(temp, lfoPhases);
            voices[voiceOffset].lfoPhase = temp[0];
            voices[voiceOffset + 1].lfoPhase = temp[1];
            voices[voiceOffset + 2].lfoPhase = temp[2];
            voices[voiceOffset + 3].lfoPhase = temp[3];
        }

        // Fixed scaling
        outputSample *= 0.5f; // Increased scaling for audible output

        // Debug print every 1000 samples
#ifdef DEBUG_OUTPUT
        if (i % 1000 == 0) {
            std::cerr << "Sample " << i << ": outputSample = " << outputSample
                      << ", activeVoices = " << activeVoices << std::endl;
        }
#endif

        // Prevent nan output
        if (std::isnan(outputSample) || !std::isfinite(outputSample)) {
            outputSample = 0.0f;
            std::cerr << "Prevented NAN output!" << std::endl;
        }

        fwrite(&outputSample, sizeof(float), 1, stdout);
    }
}

int main() {
    srand(1234);
    const float sampleRate = 48000.0f;
    Voice voices[MAX_VOICE_POLYPHONY];
    for (int i = 0; i < MAX_VOICE_POLYPHONY; i++) {
        voices[i].frequency = 0.0f;
        voices[i].phase = 0.0f;
        voices[i].phaseIncrement = 0.0f;
        voices[i].amplitude = 0.0f;
        voices[i].cutoff = 1000.0f; // Increased to reduce attenuation
        voices[i].filterEnv = 0.0f;
        voices[i].active = false;
        voices[i].fegAttack = 0.1f;
        voices[i].fegDecay = 1.0f;
        voices[i].fegSustain = 0.5f;
        voices[i].fegRelease = 0.2f;
        voices[i].lfoRate = 1.0f;
        voices[i].lfoDepth = 0.01f;
        voices[i].lfoPhase = 0.0f;
        for (int j = 0; j < 4; j++) {
            voices[i].filterStates[j] = 0.0f;
        }
    }

    Filter filter;
    filter.resonance = 0.7f;
    filter.sampleRate = sampleRate;

    std::vector<Chord> chords;
    chords.emplace_back(Chord{{midiToFreq(49), midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63)}, 0.0f, 2.0f});
    chords.emplace_back( Chord{{midiToFreq(54), midiToFreq(58), midiToFreq(61), midiToFreq(65)}, 2.0f, 2.0f});
    chords.emplace_back( Chord{{midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68)}, 4.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67)}, 6.0f, 2.0f});
    chords.emplace_back( Chord{{midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67)}, 8.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(51), midiToFreq(55), midiToFreq(58), midiToFreq(62), midiToFreq(65)}, 10.0f, 2.0f});
    chords.emplace_back( Chord{{midiToFreq(60), midiToFreq(63), midiToFreq(67), midiToFreq(70)}, 12.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(54), midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68)}, 14.0f, 2.0f});
    chords.emplace_back( Chord{{midiToFreq(61), midiToFreq(65), midiToFreq(68), midiToFreq(72)}, 16.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68), midiToFreq(72)}, 18.0f, 2.0f});
    chords.emplace_back( Chord{{midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63)}, 20.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67), midiToFreq(70)}, 22.0f, 2.0f});

#if DEBUG_OUTPUT
    // Debug sine function
    float test[4] = {0.0f, M_PI / 4, M_PI / 2, 3 * M_PI / 4};
    SIMD_TYPE x = SIMD_LOAD(test);
    SIMD_TYPE sin_x = SIMD_SIN(x);
    float result[4];
    SIMD_STORE(result, sin_x);
    for (int i = 0; i < 4; ++i)
        std::cerr << "sin(" << test[i] << ") = " << result[i] << std::endl;
#endif

    int numSamples = static_cast<int>(24.0f * sampleRate);
    generateSineSamples(voices, numSamples, filter, chords);

    return 0;
}
