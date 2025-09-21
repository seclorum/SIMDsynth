
#ifndef SIMDSYNTH_SIMDSYNTHCORE_H
#define SIMDSYNTH_SIMDSYNTHCORE_H
#pragma once

#include <iostream>
#include <cmath>
#include <cstdint>
#include <vector>
#include <cstdlib>
#include <ctime>

#include <JuceHeader.h>
#include <vector>

#define MAX_VOICE_POLYPHONY 8

// Voice structure
struct VoiceState {
    float frequency;      // Oscillator frequency (Hz)
    float phase;         // Oscillator phase (radians)
    float phaseIncrement;// Per-sample phase increment
    float amplitude;     // Oscillator amplitude (from envelope)
    float cutoff;        // Filter cutoff frequency (Hz)
    float filterEnv;     // Filter envelope output
    float filterStates[4]; // 4-pole filter states
    float fegAttack;     // Filter envelope attack time (s)
    float fegDecay;      // Filter envelope decay time (s)
    float fegSustain;    // Filter envelope sustain level (0–1)
    float fegRelease;    // Filter envelope release time (s)
    float lfoRate;       // LFO rate (Hz)
    float lfoDepth;      // LFO depth (radians)
    float lfoPhase;      // LFO phase (radians)
    double noteStartTime;// For envelope timing
    bool active;         // Voice active state
};

// Filter structure
struct FilterState {
    float resonance; // Resonance (0 to 1)
};

// Chord structure for demo mode
struct Chord {
    std::vector<float> frequencies;
    float startTime;
    float duration;
};


float midiToFreq(int midiNote);
float randomize(float base, float var);
void updateEnvelopes(VoiceState* voices, int numVoices, float attackTime, float decayTime, float chordDuration, float sampleRate, int sampleIndex, float currentTime);

#ifdef __x86_64__
#include <xmmintrin.h>
#include <emmintrin.h>
__m128 fast_sin_ps(__m128 x);
#ifdef __SSE4_1__
#include <smmintrin.h>
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
#include <arm_neon.h>
// SIMD type for 128-bit vector of four 32-bit floats, suitable for Apple M4
#define SIMD_TYPE float32x4_t
// Set all lanes to the same value (equivalent to _mm_set1_ps)
#define SIMD_SET1 vdupq_n_f32
// Set four lanes to individual values (equivalent to _mm_set_ps)
// Uses vcreate_f32 and vcombine_f32 for efficiency, avoiding temporary array
#define SIMD_SET(__a, __b, __c, __d) vcombine_f32( \
vcreate_f32((uint64_t)(*(uint32_t*)&(__d)) | ((uint64_t)(*(uint32_t*)&(__c)) << 32)), \
vcreate_f32((uint64_t)(*(uint32_t*)&(__b)) | ((uint64_t)(*(uint32_t*)&(__a)) << 32)))
// Load four floats from memory (equivalent to _mm_load_ps)
#define SIMD_LOAD vld1q_f32
// Element-wise addition (equivalent to _mm_add_ps)
#define SIMD_ADD vaddq_f32
// Element-wise subtraction (equivalent to _mm_sub_ps)
#define SIMD_SUB vsubq_f32
// Element-wise multiplication (equivalent to _mm_mul_ps)
#define SIMD_MUL vmulq_f32
// Element-wise division (no direct NEON equivalent to _mm_div_ps)
// Uses reciprocal approximation with Newton-Raphson iteration for accuracy
#define SIMD_DIV simd_div_ps
// Floor operation (equivalent to _mm_floor_ps)
// Uses vrndmq_f32 for hardware floor (round to minus infinity)
#define SIMD_FLOOR vrndmq_f32
// Store four floats to memory (equivalent to _mm_store_ps)
#define SIMD_STORE vst1q_f32
// Fast sine approximation (provided in original code)
#define SIMD_SIN fast_sin_ps

// Custom division function for NEON
inline float32x4_t simd_div_ps(float32x4_t a, float32x4_t b)
{
    // Initial reciprocal estimate
    float32x4_t recip = vrecpeq_f32(b);
    // One or two Newton-Raphson iterations for better accuracy
    recip = vmulq_f32(recip, vrecpsq_f32(b, recip));
    // Optional: second iteration for higher precision (uncomment if needed)
    // recip = vmulq_f32(recip, vrecpsq_f32(b, recip));
    return vmulq_f32(a, recip);
}

float32x4_t my_floor_ps(float32x4_t x);

inline float32x4_t fast_sin_ps(float32x4_t x)
{
    const float32x4_t twoPi = vdupq_n_f32(2.0f * M_PI);
    const float32x4_t invTwoPi = vdupq_n_f32(1.0f / (2.0f * M_PI));
    const float32x4_t piOverTwo = vdupq_n_f32(M_PI / 2.0f);

    // Wrap x to [-π, π]
    float32x4_t q = vmulq_f32(x, invTwoPi);
    q = SIMD_FLOOR(q); // Now uses vrndmq_f32
    float32x4_t xWrapped = vsubq_f32(x, vmulq_f32(q, twoPi));

    // Further reduce to [-π/2, π/2] for better polynomial accuracy
    float32x4_t sign = vdupq_n_f32(1.0f);
    float32x4_t absX = vmaxq_f32(xWrapped, vsubq_f32(vdupq_n_f32(0.0f), xWrapped));
    uint32x4_t gtPiOverTwo = vcgtq_f32(absX, piOverTwo);
    sign = vbslq_f32(gtPiOverTwo, vdupq_n_f32(-1.0f), vdupq_n_f32(1.0f));
    xWrapped = vsubq_f32(xWrapped, vbslq_f32(gtPiOverTwo, vmulq_f32(piOverTwo, vdupq_n_f32(2.0f)), vdupq_n_f32(0.0f)));

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

void applyLadderFilter(VoiceState* voices, int voiceOffset, SIMD_TYPE input, FilterState& filter, SIMD_TYPE& output, float sampleRate);

// SimdSynthVoice class
class SimdSynthVoice : public juce::SynthesiserVoice {
public:
    SimdSynthVoice();

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;
    void renderNextBlock(juce::AudioBuffer<double>& outputBuffer, int startSample, int numSamples) override;

    void setDemoMode(bool enabled);

private:
    VoiceState states[8];
    FilterState filter;
    float sampleRate;
    float ampAttack, ampDecay;
    std::vector<Chord> demoChords;
    bool demoActive;
    double demoTime;
    int demoIndex;

    std::vector<Chord> getDebussyChords();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimdSynthVoice)
};

// Main SimdSynth class
class SimdSynth {
public:
    SimdSynth();
    ~SimdSynth();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

private:
    juce::Synthesiser synth;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimdSynth)
};

#endif
