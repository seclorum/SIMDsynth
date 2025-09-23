/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 8 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

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
#define SIMD_CMP_EQ(a, b) _mm_cmpeq_ps((a), (b))
#define SIMD_MIN _mm_min_ps
#define SIMD_MAX _mm_max_ps

#elif defined(__aarch64__) || defined(__arm64__)
#include <arm_neon.h>
#define SIMD_TYPE float32x4_t
#define SIMD_SET1 vdupq_n_f32
#define SIMD_SET(a, b, c, d)                                                                                           \
    ({                                                                                                                 \
        float temp[4] = {a, b, c, d};                                                                                  \
        vld1q_f32(temp);                                                                                               \
    })
#define SIMD_ADD vaddq_f32
#define SIMD_SUB vsubq_f32
#define SIMD_MUL vmulq_f32
#define SIMD_DIV my_divq_f32
#define SIMD_SIN fast_sin_ps
#define SIMD_FLOOR my_floorq_f32
#define SIMD_LOAD vld1q_f32
#define SIMD_STORE vst1q_f32
#define SIMD_CMP_EQ(a, b) vceqq_f32((a), (b))
#define SIMD_MIN vminq_f32
#define SIMD_MAX vmaxq_f32

inline float32x4_t my_divq_f32(float32x4_t a, float32x4_t b) {
    float32x4_t recip = vrecpeq_f32(b);
    recip = vmulq_f32(recip, vrecpsq_f32(b, recip));
    recip = vmulq_f32(recip, vrecpsq_f32(b, recip));
    return vmulq_f32(a, recip);
}

inline float32x4_t my_floorq_f32(float32x4_t x) {
    int32x4_t i = vcvtq_s32_f32(x);
    float32x4_t trunc = vcvtq_f32_s32(i);
    uint32x4_t lt = vcltq_f32(x, trunc);
    float32x4_t adjust = vcvtq_f32_s32(vreinterpretq_s32_u32(lt));
    adjust = vmulq_f32(adjust, vdupq_n_f32(-1.0f));
    return vaddq_f32(trunc, adjust);
}
#else
#error "Unsupported architecture: only x86_64, aarch64, and arm64 are supported."
#endif

#define WAVETABLE_SIZE 2048
#define MAX_VOICE_POLYPHONY 16

struct Voice {
        bool active = false;
        bool released = false;
        float frequency = 0.0f;
        float phase = 0.0f;
        float phaseIncrement = 0.0f;
        float lfoPhase = 0.0f;
        float amplitude = 0.0f;
        int noteNumber = 0;
        float velocity = 0.0f;
        float noteOnTime = 0.0f;
        float noteOffTime = 0.0f;
        float filterEnv = 0.0f;
        float subFrequency = 0.0f;
        float subPhase = 0.0f;
        float subPhaseIncrement = 0.0f;
        int wavetableType = 0;
        float voiceAge = 0.0f;
        bool isHeld = false;
        float releaseStart = 0.0;
        float attack = 0.1f;
        float decay = 0.5f;
        float sustain = 0.8f;
        float release = 0.2f;
        float attackCurve = 2.0f;    // Shape of attack curve (1.0 = linear, >1 = more exponential)
        float releaseCurve = 3.0f;   // Shape of release curve
        float releaseStartAmplitude = 0.0f; // Store amplitude at note-off
        float timeScale = 0.7f;      // Time scaling factor for envelope stages
        float cutoff = 1000.0f;
        float fegAttack = 0.1f;
        float fegDecay = 1.0f;
        float fegSustain = 0.5f;
        float fegRelease = 0.2f;
        float fegAmount = 0.5f;
        float lfoRate = 1.0f;
        float lfoDepth = 0.05f;
        float subTune = -12.0f;
        float subMix = 0.5f;
        float subTrack = 1.0f;
        int unison = 1;
        float detune = 0.01f;
#if defined(__aarch64__) || defined(__arm64__)
        float32x4_t filterStates[4] = {vdupq_n_f32(0.0f), vdupq_n_f32(0.0f), vdupq_n_f32(0.0f), vdupq_n_f32(0.0f)};
#else
        float filterStates[4] = {0.0f, 0.0f, 0.0f, 0.0f};
#endif
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
        void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

        juce::AudioProcessorEditor *createEditor() override;
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
        void changeProgramName(int index, const juce::String &newName) override;

        void getStateInformation(juce::MemoryBlock &destData) override;
        void setStateInformation(const void *data, int sizeInBytes) override;
        int findVoiceToSteal();
        void updateEnvelopes(float t);

        void savePreset(const juce::String &presetName, const juce::var &parameters) {
            presetManager.writePresetFile(presetName, parameters);
        }
        void loadPresets() { loadPresetsFromDirectory(); }
        juce::AudioProcessorValueTreeState &getParameters() { return parameters; }

        juce::StringArray getPresetNames() const { return presetNames; }

        static const int parameterVersion = 1;

    private:
        juce::AudioProcessorValueTreeState parameters;
        std::atomic<float> *wavetableTypeParam = nullptr;
        std::atomic<float> *attackTimeParam = nullptr;
        std::atomic<float> *decayTimeParam = nullptr;
        std::atomic<float> *sustainLevelParam = nullptr;
        std::atomic<float> *releaseTimeParam = nullptr;
        std::atomic<float> *cutoffParam = nullptr;
        std::atomic<float> *resonanceParam = nullptr;
        std::atomic<float> *fegAttackParam = nullptr;
        std::atomic<float> *fegDecayParam = nullptr;
        std::atomic<float> *fegSustainParam = nullptr;
        std::atomic<float> *fegReleaseParam = nullptr;
        std::atomic<float> *fegAmountParam = nullptr;
        std::atomic<float> *lfoRateParam = nullptr;
        std::atomic<float> *lfoDepthParam = nullptr;
        std::atomic<float> *subTuneParam = nullptr;
        std::atomic<float> *subMixParam = nullptr;
        std::atomic<float> *subTrackParam = nullptr;
        std::atomic<float> *gainParam = nullptr;
        std::atomic<float> *unisonParam = nullptr;
        std::atomic<float> *detuneParam = nullptr;

        double currentTime;

        alignas(16) float sineTable[WAVETABLE_SIZE];
        alignas(16) float sawTable[WAVETABLE_SIZE];
        alignas(16) float squareTable[WAVETABLE_SIZE];
        juce::dsp::LookupTableTransform<float> sineTableTransform;
        juce::dsp::LookupTableTransform<float> sawTableTransform;
        juce::dsp::LookupTableTransform<float> squareTableTransform;

        Voice voices[MAX_VOICE_POLYPHONY];
        Filter filter;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

        void initWavetables();
        float midiToFreq(int midiNote);
        float randomize(float base, float var);

#if defined(__x86_64__)
        __m128 fast_sin_ps(__m128 x);
#elif defined(__aarch64__) || defined(__arm64__)
        float32x4_t fast_sin_ps(float32x4_t x);
#endif

        SIMD_TYPE wavetable_lookup_ps(SIMD_TYPE phase, int wavetableType);
        void applyLadderFilter(Voice *voices, int voiceOffset, SIMD_TYPE input, Filter &filter, SIMD_TYPE &output);

        PresetManager presetManager;
        juce::StringArray presetNames;
        int currentProgram = 0;
        void loadPresetsFromDirectory();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimdSynthAudioProcessor)
};

juce::AudioProcessor *createPluginFilter();
