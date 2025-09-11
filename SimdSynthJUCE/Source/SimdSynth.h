#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <cstdint>
#include <vector>
#include <cstdlib>
#include <ctime>

// Architecture-specific SIMD macros
#ifdef __x86_64__
#include <xmmintrin.h>
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
#include <arm_neon.h>
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

// Forward declarations for custom functions
#ifdef __arm64__
inline float32x4_t my_floor_ps(float32x4_t);
inline float32x4_t my_sin_ps(float32x4_t);
#endif
#ifdef __x86_64__
inline __m128 _mm_sin_ps(__m128);
#endif

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

// SimdSynthVoice class
class SimdSynthVoice : public juce::SynthesiserVoice {
public:
    SimdSynthVoice();

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, const juce::MidiBuffer& midiMessages, int startSample, int numSamples);

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
