/*
 *
 *	simdsynth - a playground for experimenting with SIMD-based audio
 *			    synthesis, with simple polyphonic oscillator, filter, 
 *				envelopes and LFO per voice, up to 8 voices.
 *
 */

#include <iostream>
#include <cmath>
#include <cstdint>
#include <vector>
#include <cstdlib>
#include <ctime>

#define MAX_VOICE_POLYPHONY 8

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
    float filterEnv;     // Filter envelope output
    float filterStates[4]; // 4-pole filter states
    bool active;         // Voice active state
    // FEG parameters
    float fegAttack;     // Filter envelope attack time (s)
    float fegDecay;      // Filter envelope decay time (s)
    float fegSustain;    // Filter envelope sustain level (0–1)
    float fegRelease;    // Filter envelope release time (s)
    // LFO parameters
    float lfoRate;       // LFO rate (Hz)
    float lfoDepth;      // LFO depth (radians)
    float lfoPhase;      // LFO phase (radians)
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
void updateEnvelopes(Voice* voices, int numVoices, float attackTime, float decayTime, float sampleRate, int sampleIndex, float currentTime) {
    float t = sampleIndex / sampleRate; // Time in seconds
    for (int i = 0; i < numVoices; i++) {
        if (!voices[i].active) {
            voices[i].amplitude = 0.0f;
            voices[i].filterEnv = 0.0f;
            continue;
        }
        float localTime = t - currentTime; // Time relative to chord start

        // Amplitude envelope (AD, no sustain)
        if (localTime < attackTime) {
            voices[i].amplitude = localTime / attackTime; // Linear attack
        } else if (localTime < attackTime + decayTime) {
            voices[i].amplitude = 1.0f - (localTime - attackTime) / decayTime; // Linear decay
        } else {
            voices[i].amplitude = 0.0f;
            voices[i].active = false;
        }

        // Filter envelope (ADSR)
        if (localTime < voices[i].fegAttack) {
            voices[i].filterEnv = localTime / voices[i].fegAttack; // Attack
        } else if (localTime < voices[i].fegAttack + voices[i].fegDecay) {
            voices[i].filterEnv = 1.0f - (localTime - voices[i].fegAttack) / voices[i].fegDecay * (1.0f - voices[i].fegSustain); // Decay to sustain
        } else if (localTime < attackTime + decayTime) {
            voices[i].filterEnv = voices[i].fegSustain; // Sustain
        } else if (localTime < attackTime + decayTime + voices[i].fegRelease) {
            voices[i].filterEnv = voices[i].fegSustain * (1.0f - (localTime - (attackTime + decayTime)) / voices[i].fegRelease); // Release
        } else {
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
        float modulatedCutoff = voices[idx].cutoff + voices[idx].filterEnv * 2000.0f; // 500–2500 Hz sweep
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

// Generate sine waves and apply filter for MAX_VOICE_POLYPHONY voices
void generateSineSamples(Voice* voices, int numSamples, Filter& filter, const std::vector<Chord>& chords) {
    const SIMD_TYPE twoPi = SIMD_SET1(2.0f * M_PI);
    const SIMD_TYPE one = SIMD_SET1(1.0f);
    float attackTime = 0.1f; // 100 ms attack for amplitude
    float decayTime = 1.9f;  // 1.9 s decay for amplitude
    float currentTime = 0.0f;
    int currentChord = 0;

    for (int i = 0; i < numSamples; i++) {
        float t = i / filter.sampleRate;

        // Update chord if needed
        if (currentChord < chords.size() && t >= chords[currentChord].startTime + chords[currentChord].duration) {
            currentChord++;
            currentTime = t;
            if (currentChord < chords.size()) {
                // Assign new frequencies and randomize FEG and LFO parameters
                for (int v = 0; v < MAX_VOICE_POLYPHONY; v++) {
                    voices[v].active = v < chords[currentChord].frequencies.size();
                    if (voices[v].active) {
                        voices[v].frequency = chords[currentChord].frequencies[v];
                        voices[v].phaseIncrement = (2.0f * M_PI * voices[v].frequency) / filter.sampleRate;
                        voices[v].phase = 0.0f; // Reset phase for new note
                        voices[v].lfoPhase = 0.0f; // Reset LFO phase
                        // Randomize FEG parameters
                        voices[v].fegAttack = randomize(0.1f, 0.2f);  // 80–120 ms
                        voices[v].fegDecay = randomize(1.0f, 0.2f);   // 800–1200 ms
                        voices[v].fegSustain = randomize(0.5f, 0.2f); // 0.4–0.6
                        voices[v].fegRelease = randomize(0.2f, 0.2f); // 160–240 ms
                        // Randomize LFO parameters
                        voices[v].lfoRate = randomize(1.0f, 0.125f);    // 0.8–1.2 Hz
                        voices[v].lfoDepth = randomize(0.1f, 0.125f);  // 0.008–0.012 radians
                    }
                }
            }
        }

        // Update envelopes
        updateEnvelopes(voices, MAX_VOICE_POLYPHONY, attackTime, decayTime, filter.sampleRate, i, currentTime);

        // Process groups of 4 voices
        float outputSample = 0.0f;
        for (int group = 0; group < (MAX_VOICE_POLYPHONY / 2); group++) {
            int voiceOffset = group * 4;

            // Load amplitudes, phases, and increments
            float tempAmps[4] = { voices[voiceOffset].amplitude, voices[voiceOffset + 1].amplitude,
                                  voices[voiceOffset + 2].amplitude, voices[voiceOffset + 3].amplitude };
            float tempPhases[4] = { voices[voiceOffset].phase, voices[voiceOffset + 1].phase,
                                    voices[voiceOffset + 2].phase, voices[voiceOffset + 3].phase };
            float tempIncrements[4] = { voices[voiceOffset].phaseIncrement, voices[voiceOffset + 1].phaseIncrement,
                                        voices[voiceOffset + 2].phaseIncrement, voices[voiceOffset + 3].phaseIncrement };
            float tempLfoPhases[4] = { voices[voiceOffset].lfoPhase, voices[voiceOffset + 1].lfoPhase,
                                       voices[voiceOffset + 2].lfoPhase, voices[voiceOffset + 3].lfoPhase };
            float tempLfoRates[4] = { voices[voiceOffset].lfoRate, voices[voiceOffset + 1].lfoRate,
                                      voices[voiceOffset + 2].lfoRate, voices[voiceOffset + 3].lfoRate };
            float tempLfoDepths[4] = { voices[voiceOffset].lfoDepth, voices[voiceOffset + 1].lfoDepth,
                                       voices[voiceOffset + 2].lfoDepth, voices[voiceOffset + 3].lfoDepth };
            SIMD_TYPE amplitudes = SIMD_LOAD(tempAmps);
            SIMD_TYPE phases = SIMD_LOAD(tempPhases);
            SIMD_TYPE increments = SIMD_LOAD(tempIncrements);
            SIMD_TYPE lfoPhases = SIMD_LOAD(tempLfoPhases);
            SIMD_TYPE lfoRates = SIMD_LOAD(tempLfoRates);
            SIMD_TYPE lfoDepths = SIMD_LOAD(tempLfoDepths);

            // Compute LFO and modulate phases
            SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRates, SIMD_SET1(2.0f * M_PI / filter.sampleRate));
            lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
            lfoPhases = SIMD_SUB(lfoPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)), twoPi)); // Wrap LFO phase
            SIMD_TYPE lfoValues = SIMD_SIN(lfoPhases);
            lfoValues = SIMD_MUL(lfoValues, lfoDepths);
            phases = SIMD_ADD(phases, lfoValues); // Modulate oscillator phase

            // Compute sine for 4 voices
            SIMD_TYPE sinValues = SIMD_SIN(phases);
            sinValues = SIMD_MUL(sinValues, amplitudes);

            // Apply 4-pole ladder filter
            SIMD_TYPE filteredOutput;
            applyLadderFilter(voices, voiceOffset, sinValues, filter, filteredOutput);

            // Sum the 4 voices
            float temp[4];
            SIMD_STORE(temp, filteredOutput);
            outputSample += (temp[0] + temp[1] + temp[2] + temp[3]) * 0.125f; // Scale for MAX_VOICE_POLYPHONY voices

            // Update phases and LFO phases
            phases = SIMD_ADD(phases, increments);
            SIMD_TYPE wrap = SIMD_SUB(phases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(phases, twoPi)), twoPi));
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

        // Write raw float sample to stdout
        fwrite(&outputSample, sizeof(float), 1, stdout);
    }
}

int main() {
    // Seed random number generator for reproducibility
    srand(1234);

    // Initialize the voices
    const float sampleRate = 48000.0f;
    Voice voices[MAX_VOICE_POLYPHONY];
    for (int i = 0; i < MAX_VOICE_POLYPHONY; i++) {
        voices[i].frequency = 0.0f;
        voices[i].phase = 0.0f;
        voices[i].phaseIncrement = 0.0f;
        voices[i].amplitude = 0.0f;
        voices[i].cutoff = 500.0f; // Base cutoff frequency
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

    // Initialize filter
    Filter filter;
    filter.resonance = 0.7f;
    filter.sampleRate = sampleRate;

    // Define Debussy-inspired chords (frequencies in Hz, MIDI note to Hz)
    std::vector<Chord> chords;
    chords.emplace_back(Chord{{midiToFreq(49), midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63)}, 0.0f, 2.0f});  // D♭ major 9
    chords.emplace_back(Chord{{midiToFreq(54), midiToFreq(58), midiToFreq(61), midiToFreq(65)}, 2.0f, 2.0f});           // G♭ major 7
    chords.emplace_back(Chord{{midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68)}, 4.0f, 2.0f});           // B♭ minor 7
    chords.emplace_back(Chord{{midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67)}, 6.0f, 2.0f}); // F minor 9
    chords.emplace_back(Chord{{midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67)}, 8.0f, 2.0f});           // A♭ major 7
    chords.emplace_back(Chord{{midiToFreq(51), midiToFreq(55), midiToFreq(58), midiToFreq(62), midiToFreq(65)}, 10.0f, 2.0f}); // E♭ major 9
    chords.emplace_back(Chord{{midiToFreq(60), midiToFreq(63), midiToFreq(67), midiToFreq(70)}, 12.0f, 2.0f});          // C minor 7
    chords.emplace_back(Chord{{midiToFreq(54), midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68)}, 14.0f, 2.0f}); // G♭ major 9
    chords.emplace_back(Chord{{midiToFreq(61), midiToFreq(65), midiToFreq(68), midiToFreq(72)}, 16.0f, 2.0f});          // D♭ major 7
    chords.emplace_back(Chord{{midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68), midiToFreq(72)}, 18.0f, 2.0f}); // B♭ minor 9
    chords.emplace_back(Chord{{midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63)}, 20.0f, 2.0f});          // F minor 7
    chords.emplace_back(Chord{{midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67), midiToFreq(70)}, 22.0f, 2.0f}); // A♭ major 9

    // Generate 24 seconds of audio
    int numSamples = static_cast<int>(24.0f * sampleRate);
    generateSineSamples(voices, numSamples, filter, chords);

    return 0;
}
