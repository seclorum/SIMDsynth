#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <vector>

#define MAX_VOICE_POLYPHONY 8
#define WAVETABLE_SIZE 1024

#ifdef __x86_64__
#include <xmmintrin.h>
#include <emmintrin.h>
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

// Structure for a synthesizer voice
struct Voice {
    float frequency;
    float phase;
    float phaseIncrement;
    float amplitude;
    float cutoff;
    float filterEnv;
    float filterStates[4];
    bool active;
    float fegAttack;
    float fegDecay;
    float fegSustain;
    float fegRelease;
    float lfoRate;
    float lfoDepth;
    float lfoPhase;
    float subFrequency;
    float subPhase;
    float subPhaseIncrement;
    float subTune;
    float subMix;
    float subTrack;
    int wavetableType;
    uint8_t noteNumber;
    uint8_t velocity;
    double noteOnTime;
};

// Structure for filter parameters
struct Filter {
    float resonance;
    float sampleRate;
};

class SimdSynthAudioProcessor : public juce::AudioProcessor {
public:
    SimdSynthAudioProcessor();
    ~SimdSynthAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    // Wavetables
    float sineTable[WAVETABLE_SIZE];
    float sawTable[WAVETABLE_SIZE];

    // Voices and filter
    Voice voices[MAX_VOICE_POLYPHONY];
    Filter filter;

    // Parameters
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* wavetableTypeParam;
    std::atomic<float>* attackTimeParam;
    std::atomic<float>* decayTimeParam;
    std::atomic<float>* cutoffParam;
    std::atomic<float>* resonanceParam;
    std::atomic<float>* fegAttackParam;
    std::atomic<float>* fegDecayParam;
    std::atomic<float>* fegSustainParam;
    std::atomic<float>* fegReleaseParam;
    std::atomic<float>* lfoRateParam;
    std::atomic<float>* lfoDepthParam;
    std::atomic<float>* subTuneParam;
    std::atomic<float>* subMixParam;
    std::atomic<float>* subTrackParam;

    // MIDI handling
    juce::Synthesiser synth;

    // SIMD functions
    void initWavetables();
    float midiToFreq(int midiNote);
    float randomize(float base, float var);
    void updateEnvelopes(int sampleIndex);
    void applyLadderFilter(Voice* voices, int voiceOffset, SIMD_TYPE input, Filter& filter, SIMD_TYPE& output);
    SIMD_TYPE wavetable_lookup_ps(SIMD_TYPE phase, const float* table);

#ifdef __arm64__
    float32x4_t my_floor_ps(float32x4_t x);
#endif
#ifdef __x86_64__
    __m128 fast_sin_ps(__m128 x);
#endif
#ifdef __arm64__
    float32x4_t fast_sin_ps(float32x4_t x);
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimdSynthAudioProcessor)
};

// Declare the plugin creation function with C++ linkage
juce::AudioProcessor* createPluginFilter();