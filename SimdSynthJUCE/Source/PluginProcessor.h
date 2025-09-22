// PluginProcessor.h
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <string>
#include <algorithm>
#include <map>

#include "PresetManager.h"


#ifdef __x86_64__
#include <immintrin.h>
#define SIMD_TYPE __m128
#define SIMD_SET1 _mm_set1_ps
#define SIMD_SET _mm_set_ps
#define SIMD_ADD _mm_add_ps
#define SIMD_SUB _mm_sub_ps
#define SIMD_MUL _mm_mul_ps
#define SIMD_DIV _mm_div_ps
#define SIMD_SIN fast_sin_ps
#define SIMD_FLOOR _mm_floor_ps
#define SIMD_LOAD _mm_loadu_ps
#define SIMD_STORE _mm_storeu_ps
#elif defined(__arm64__)
#include <arm_neon.h>
#define SIMD_TYPE float32x4_t
#define SIMD_SET1 vdupq_n_f32
#define SIMD_SET(a, b, c, d) (float32x4_t){d, c, b, a}
#define SIMD_ADD vaddq_f32
#define SIMD_SUB vsubq_f32
#define SIMD_MUL vmulq_f32
#define SIMD_DIV vdivq_f32
#define SIMD_SIN fast_sin_ps
#define SIMD_FLOOR my_floor_ps
#define SIMD_LOAD vld1q_f32
#define SIMD_STORE vst1q_f32
#endif

#define WAVETABLE_SIZE 2048
#define MAX_VOICE_POLYPHONY 16

struct Voice {
    bool active = false;
    float frequency = 0.0f;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    float amplitude = 0.0f;
    int noteNumber = 0;
    int velocity = 0;
    float noteOnTime = 0.0f;
    float filterEnv = 0.0f;
    float filterStates[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float cutoff = 1000.0f;
    float fegAttack = 0.1f;
    float fegDecay = 1.0f;
    float fegSustain = 0.5f;
    float fegRelease = 0.2f;
    float lfoPhase = 0.0f;
    float lfoRate = 1.0f;
    float lfoDepth = 0.01f;
    float subFrequency = 0.0f;
    float subPhase = 0.0f;
    float subPhaseIncrement = 0.0f;
    float subTune = -12.0f;
    float subMix = 0.5f;
    float subTrack = 1.0f;
    int wavetableType = 0;
};

struct Filter {
    float resonance = 0.7f;
    float sampleRate = 48000.0f;
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

    const juce::String getName() const override { return "SimdSynth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static const int parameterVersion = 1;
// Public access to parameters for the editor
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }

private:
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* wavetableTypeParam = nullptr;
    std::atomic<float>* attackTimeParam = nullptr;
    std::atomic<float>* decayTimeParam = nullptr;
    std::atomic<float>* cutoffParam = nullptr;
    std::atomic<float>* resonanceParam = nullptr;
    std::atomic<float>* fegAttackParam = nullptr;
    std::atomic<float>* fegDecayParam = nullptr;
    std::atomic<float>* fegSustainParam = nullptr;
    std::atomic<float>* fegReleaseParam = nullptr;
    std::atomic<float>* lfoRateParam = nullptr;
    std::atomic<float>* lfoDepthParam = nullptr;
    std::atomic<float>* subTuneParam = nullptr;
    std::atomic<float>* subMixParam = nullptr;
    std::atomic<float>* subTrackParam = nullptr;

    float sineTable[WAVETABLE_SIZE];
    float sawTable[WAVETABLE_SIZE];
    Voice voices[MAX_VOICE_POLYPHONY];
    Filter filter;

    void initWavetables();
    float midiToFreq(int midiNote);
    float randomize(float base, float var);
#ifdef __arm64__
    float32x4_t my_floor_ps(float32x4_t x);
#endif
#ifdef __x86_64__
    __m128 fast_sin_ps(__m128 x);
#endif
#ifdef __arm64__
    float32x4_t fast_sin_ps(float32x4_t x);
#endif
    SIMD_TYPE wavetable_lookup_ps(SIMD_TYPE phase, const float* table);
    void applyLadderFilter(Voice* voices, int voiceOffset, SIMD_TYPE input, Filter& filter, SIMD_TYPE& output);
    void updateEnvelopes(int sampleIndex);

    // Preset management
    PresetManager presetManager; // Added
    std::vector<juce::String> presetNames;
    int currentProgram = 0;
    void loadPresetsFromDirectory();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimdSynthAudioProcessor)
};

// Declare the plugin creation function with C++ linkage
juce::AudioProcessor* createPluginFilter();