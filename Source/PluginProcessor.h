/*
 * simdsynth - A playground for experimenting with SIMD-based audio synthesis,
 *             featuring polyphonic main and sub-oscillators, filters, envelopes,
 *             and LFOs per voice, supporting up to 16 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h> // For MathConstants
#include <juce_dsp/juce_dsp.h>   // For DSP utilities
#include "PresetManager.h"        // Preset management

// Architecture-specific SIMD definitions
#ifdef __x86_64__
#include <immintrin.h>
#define SIMD_TYPE __m128
#define SIMD_SET1 _mm_set1_ps
#define SIMD_ADD _mm_add_ps
#define SIMD_SUB _mm_sub_ps
#define SIMD_MUL _mm_mul_ps
#define SIMD_DIV _mm_div_ps
#define SIMD_LOAD _mm_load_ps
#define SIMD_STORE _mm_store_ps
#define SIMD_SET _mm_set_ps
#define SIMD_SIN fast_sin_ps
#define SIMD_FLOOR my_floorq_f32
#define SIMD_MAX _mm_max_ps
#define SIMD_MIN _mm_min_ps
#define SIMD_SET_LANE _mm_set_ps
#elif defined(__aarch64__) || defined(__arm64__)
#include <arm_neon.h>
#define SIMD_TYPE float32x4_t
#define SIMD_SET1 vdupq_n_f32
#define SIMD_ADD vaddq_f32
#define SIMD_SUB vsubq_f32
#define SIMD_MUL vmulq_f32
#define SIMD_DIV vdivq_f32
#define SIMD_LOAD vld1q_f32
#define SIMD_STORE vst1q_f32
#define SIMD_SET(a, b, c, d) vsetq_lane_f32(a, vsetq_lane_f32(b, vsetq_lane_f32(c, vdupq_n_f32(d), 2), 1), 0)
#define SIMD_SIN fast_sin_ps
#define SIMD_FLOOR my_floorq_f32
#define SIMD_MAX vmaxq_f32
#define SIMD_MIN vminq_f32
#define SIMD_SET_LANE(a, b, lane) vsetq_lane_f32(b, a, lane)
#else
#error "Unsupported architecture"
#endif

// Constants for wavetable size and polyphony
static constexpr int WAVETABLE_SIZE = 8192;    // Size of wavetable lookup tables
static constexpr int MAX_VOICE_POLYPHONY = 16; // Maximum number of simultaneous voices

// Voice structure to hold per-voice synthesis parameters and state
struct Voice {
    bool active = false;              // Is the voice currently active?
    bool released = false;            // Has the voice been released (note-off)?
    bool isHeld = false;              // Is the note currently held?
    float frequency = 0.0f;           // Base frequency of the note (Hz)
    float phase = 0.0f;               // Main oscillator phase (0 to 1)
    float phaseIncrement = 0.0f;      // Main oscillator phase increment per sample
    int noteNumber = 0;               // MIDI note number
    float velocity = 0.0f;            // Note velocity (0 to 1)
    float amplitude = 0.0f;           // Current amplitude from envelope
    float voiceAge = 0.0f;           // Age of the voice (seconds)
    float noteOnTime = 0.0f;         // Time when note was triggered
    float noteOffTime = 0.0f;        // Time when note was released
    float releaseStartAmplitude = 0.0f; // Amplitude at release start
    float subPhase = 0.0f;           // Sub-oscillator phase (radians)
    float subPhaseIncrement = 0.0f;  // Sub-oscillator phase increment per sample
    float osc2Phase = 0.0f; // New oscillator phase
    float osc2PhaseIncrement = 0.0f; // New oscillator phase increment
    float lfoPhase = 0.0f;           // LFO phase (radians)
    float filterEnv = 0.0f;          // Filter envelope value (0 to 1)
    float attackCurve = 2.0f;        // Attack curve exponent
    float releaseCurve = 3.0f;       // Release curve exponent
    int wavetableType = 0;           // Wavetable type (0=sine, 1=saw, 2=square)
    float attack = 0.1f;             // Amplitude envelope attack time (seconds)
    float decay = 0.5f;              // Amplitude envelope decay time (seconds)
    float sustain = 0.8f;            // Amplitude envelope sustain level (0 to 1)
    float release = 0.2f;            // Amplitude envelope release time (seconds)
    float cutoff = 1000.0f;          // Filter cutoff frequency (Hz)
    float resonance = 0.7f;          // Filter resonance (0 to 1)
    float fegAttack = 0.1f;          // Filter envelope attack time (seconds)
    float fegDecay = 1.0f;           // Filter envelope decay time (seconds)
    float fegSustain = 0.5f;         // Filter envelope sustain level (0 to 1)
    float fegRelease = 0.2f;         // Filter envelope release time (seconds)
    float fegAmount = 0.5f;          // Filter envelope modulation amount (-1 to 1)
    float lfoRate = 1.0f;            // LFO rate (Hz)
    float lfoDepth = 0.05f;          // LFO depth (0 to 0.5)
    float subTune = -12.0f;          // Sub-oscillator tuning (semitones)
    float subMix = 0.5f;             // Sub-oscillator mix (0 to 1)
    float subTrack = 1.0f;           // Sub-oscillator keyboard tracking (0 to 1)
    float osc2Tune = -24.0f; // New oscillator tuning (default: -2 octaves)
    float osc2Mix = 0.3f;   // New oscillator mix (default: 0.3)
    float osc2Track = 1.0f;  // New oscillator tracking (default: full tracking)
    int unison = 1;                  // Number of unison voices (1 to 8)
    float detune = 0.01f;            // Unison detune amount (0 to 0.1)
    float filterStates[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // Filter state array (4 stages)
    float crossfade = 0.0f;          // Crossfade progress for wavetable changes (0 to 1)
};

// Structure to hold shared filter parameters for the ladder filter
struct Filter {
    float sampleRate = 44100.0f; // Sample rate for filter calculations
    float resonance = 0.7f;      // Resonance parameter (scaled in applyLadderFilter)
};

// Main audio processor class for SimdSynth
class SimdSynthAudioProcessor : public juce::AudioProcessor {
private:
    // Structure to hold pending preset parameters for new voices
    struct PendingVoiceParameters {
        int wavetableType = 0;
        float attack = 0.1f;
        float decay = 0.5f;
        float sustain = 0.8f;
        float release = 0.2f;
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
    };

    // Pending parameters for new voices
    PendingVoiceParameters pendingParameters;

public:
    // Constructor and destructor
    SimdSynthAudioProcessor();
    ~SimdSynthAudioProcessor() override;

    // Preferred buffer size for optimal performance
    int getPreferredBufferSize() const;

    // Audio processor overrides
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
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

    // SIMD floor function declaration
#if defined(__aarch64__) || defined(__arm64__)
    inline float32x4_t my_floorq_f32(float32x4_t x);
#else
    inline __m128 my_floorq_f32(__m128 x);
#endif

    // Voice management and envelope processing
    int findVoiceToSteal();                    // Select a voice for stealing when polyphony is exceeded
    void updateEnvelopes(float t);             // Update amplitude and filter envelopes for all voices
    void updateVoiceParameters();              // Update parameters for all voices

    // Preset management
    void savePreset(const juce::String& presetName, const juce::var& parameters) {
        presetManager.writePresetFile(presetName, parameters);
    }
    void loadPresets() { loadPresetsFromDirectory(); }
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    juce::StringArray getPresetNames() const { return presetNames; }

private:
    // Parameter management
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> *wavetableTypeParam, *attackTimeParam, *decayTimeParam, *sustainLevelParam,
        *releaseTimeParam, *cutoffParam, *resonanceParam, *fegAttackParam, *fegDecayParam, *fegSustainParam,
        *fegReleaseParam, *fegAmountParam, *lfoRateParam, *lfoDepthParam, *subTuneParam, *subMixParam,
        *subTrackParam, *osc2TuneParam, *osc2MixParam, *osc2TrackParam, *gainParam, *unisonParam, *detuneParam;

    // Smoothed parameters for reducing zipper noise
    juce::LinearSmoothedValue<float> smoothedGain;       // Smoothed output gain
    juce::LinearSmoothedValue<float> smoothedCutoff;     // Smoothed filter cutoff
    juce::LinearSmoothedValue<float> smoothedResonance;  // Smoothed filter resonance
    juce::LinearSmoothedValue<float> smoothedLfoRate;   // Smoothed LFO rate
    juce::LinearSmoothedValue<float> smoothedLfoDepth;  // Smoothed LFO depth
    juce::LinearSmoothedValue<float> smoothedSubMix;    // Smoothed sub-oscillator mix
    juce::LinearSmoothedValue<float> smoothedSubTune;   // Smoothed sub-oscillator tuning
    juce::LinearSmoothedValue<float> smoothedSubTrack;  // Smoothed sub-oscillator tracking
    juce::LinearSmoothedValue<float> smoothedDetune;    // Smoothed unison detune
    juce::LinearSmoothedValue<float> smoothedOsc2Mix;    // New smoothed value
    juce::LinearSmoothedValue<float> smoothedOsc2Tune; // New smoothed value
    juce::LinearSmoothedValue<float> smoothedOsc2Track;   // New smoothed value

    // Voice and filter data
    Voice voices[MAX_VOICE_POLYPHONY];                            // Array of polyphonic voices
    Filter filter;                                                // Shared filter instance
    double currentTime = 0.0;                                     // Current processing time
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling; // Oversampling for anti-aliasing
    PresetManager presetManager;                                  // Manages preset loading/saving
    juce::StringArray presetNames;                                // List of preset names
    int currentProgram = 0;                                       // Current preset index
    static constexpr int parameterVersion = 1;                    // Parameter version for state saving

    // Lookup tables for oscillator waveforms
    juce::dsp::LookupTableTransform<float> sineTableTransform;   // Sine wavetable
    juce::dsp::LookupTableTransform<float> sawTableTransform;    // Sawtooth wavetable
    juce::dsp::LookupTableTransform<float> squareTableTransform; // Square wavetable

    // Utility functions
    void loadPresetsFromDirectory();                                   // Load presets from directory
    float midiToFreq(int midiNote);                                    // Convert MIDI note to frequency
    float randomize(float base, float var);                            // Randomize a value within a range
    SIMD_TYPE wavetable_lookup_ps(SIMD_TYPE phase, SIMD_TYPE wavetableTypes); // Wavetable lookup
    void applyLadderFilter(Voice* voices, int voiceOffset, SIMD_TYPE input, Filter& filter,
                           SIMD_TYPE& output); // Apply ladder filter with SIMD

    // Architecture-specific SIMD functions
#if defined(__x86_64__)
    SIMD_TYPE fast_sin_ps(SIMD_TYPE x);                   // Fast sine approximation for x86_64
    const SIMD_TYPE piOverTwo = _mm_set1_ps(juce::MathConstants<float>::pi / 2.0f); // Pi/2 constant
#elif defined(__aarch64__) || defined(__arm64__)
    SIMD_TYPE fast_sin_ps(SIMD_TYPE x);                   // Fast sine approximation for ARM64
    const SIMD_TYPE piOverTwo = vdupq_n_f32(juce::MathConstants<float>::pi / 2.0f); // Pi/2 constant
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimdSynthAudioProcessor)
};

// Plugin creation function
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();