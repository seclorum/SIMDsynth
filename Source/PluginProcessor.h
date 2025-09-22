/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 8 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

// This directive ensures the header file is included only once during
// compilation to prevent multiple inclusion errors.
#pragma once

// Include JUCE headers for audio processing and DSP functionalities.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Standard library includes for data structures and algorithms used in the
// processor.
#include <vector>
#include <string>
#include <algorithm>
#include <map>

// Include custom PresetManager header for handling presets.
#include "PresetManager.h"

// Platform-specific SIMD includes and macro definitions.
// For x86_64 architecture (Intel/AMD), use SSE intrinsics.
#ifdef __x86_64__
#include <immintrin.h>
#define SIMD_TYPE __m128                         // Define SIMD type as 128-bit vector (4 floats).
#define SIMD_SET1 _mm_set1_ps                    // Macro for setting all elements to a single value.
#define SIMD_SET _mm_set_ps                      // Macro for setting individual elements.
#define SIMD_ADD _mm_add_ps                      // Macro for vector addition.
#define SIMD_SUB _mm_sub_ps                      // Macro for vector subtraction.
#define SIMD_MUL _mm_mul_ps                      // Macro for vector multiplication.
#define SIMD_DIV _mm_div_ps                      // Macro for vector division.
#define SIMD_SIN fast_sin_ps                     // Macro for fast sine approximation (custom function).
#define SIMD_FLOOR _mm_floor_ps                  // Macro for floor operation.
#define SIMD_LOAD _mm_loadu_ps                   // Macro for unaligned load.
#define SIMD_STORE _mm_storeu_ps                 // Macro for unaligned store.
#define SIMD_CMP_EQ(a, b) _mm_cmpeq_ps((a), (b)) // Macro for equality comparison.
// For ARM64 architecture (e.g., Apple Silicon), use NEON intrinsics.
#elif defined(__arm64__)
#include <arm_neon.h>
#define SIMD_TYPE float32x4_t // Define SIMD type as 128-bit vector (4 floats).
#define SIMD_SET1 vdupq_n_f32 // Macro for setting all elements to a single value.
#define SIMD_SET(a, b, c, d)                                                                                           \
    (float32x4_t) {                                                                                                    \
        d, c, b, a                                                                                                     \
    } // Macro for setting individual elements (note reverse order due to NEON
      // conventions).
#define SIMD_ADD vaddq_f32   // Macro for vector addition.
#define SIMD_SUB vsubq_f32   // Macro for vector subtraction.
#define SIMD_MUL vmulq_f32   // Macro for vector multiplication.
#define SIMD_DIV vdivq_f32   // Macro for vector division.
#define SIMD_SIN fast_sin_ps // Macro for fast sine approximation (custom function).
#define SIMD_FLOOR                                                                                                     \
    my_floor_ps                               // Macro for custom floor operation (NEON lacks direct floor
                                              // intrinsic).
#define SIMD_LOAD vld1q_f32                   // Macro for load.
#define SIMD_STORE vst1q_f32                  // Macro for store.
#define SIMD_CMP_EQ(a, b) vceqq_f32((a), (b)) // Macro for equality comparison.
#endif

// Define constants for wavetable size and maximum polyphony.
#define WAVETABLE_SIZE 2048   // Size of wavetables for oscillators.
#define MAX_VOICE_POLYPHONY 8 // Maximum number of simultaneous voices.

// Structure representing a single voice in the synthesizer.
// Each voice handles its own oscillator, sub-oscillator, envelopes, LFO, and
// filter state.
struct Voice {
        bool active = false;                              // Flag indicating if the voice is currently playing.
        bool released = false;                            // Flag indicating if the note has been released
                                                          // (entering release phase).
        float frequency = 0.0f;                           // Main oscillator frequency in Hz.
        float phase = 0.0f;                               // Current phase of the main oscillator (0 to 1).
        float phaseIncrement = 0.0f;                      // Phase increment per sample for the main oscillator.
        float lfoPhase = 0.0f;                            // Current phase of the LFO.
        float amplitude = 0.0f;                           // Current amplitude from the ADSR envelope.
        int noteNumber = 0;                               // MIDI note number assigned to this voice.
        float velocity = 0.0f;                            // MIDI velocity (0 to 1).
        float noteOnTime = 0.0f;                          // Time when note was triggered.
        float noteOffTime = 0.0f;                         // Time when note was released.
        float filterStates[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // State variables for the ladder filter (4 stages).
        float filterEnv = 0.0f;                           // Current value of the filter envelope.
        float subFrequency = 0.0f;                        // Sub-oscillator frequency in Hz.
        float subPhase = 0.0f;                            // Current phase of the sub-oscillator.
        float subPhaseIncrement = 0.0f;                   // Phase increment per sample for the sub-oscillator.
        int wavetableType = 0;                            // Type of wavetable for the main oscillator
                                                          // (e.g., 0=sine, 1=saw, etc.).
        float attack = 0.1f;                              // Attack time for amplitude envelope (in seconds).
        float decay = 0.5f;                               // Decay time for amplitude envelope (in seconds).
        float sustain = 0.8f;                             // Sustain level for amplitude envelope (0 to 1).
        float release = 0.2f;                             // Release time for amplitude envelope (in seconds).
        float cutoff = 1000.0f;                           // Base cutoff frequency for the filter.
        float fegAttack = 0.1f;                           // Attack time for filter envelope.
        float fegDecay = 1.0f;                            // Decay time for filter envelope.
        float fegSustain = 0.5f;                          // Sustain level for filter envelope.
        float fegRelease = 0.2f;                          // Release time for filter envelope.
        float fegAmount = 0.5f;                           // Amount of filter envelope modulation (0 to 1).
        float lfoRate = 1.0f;                             // LFO rate in Hz.
        float lfoDepth = 0.05f;                           // LFO modulation depth (applied to pitch or other targets).
        float subTune = -12.0f;                           // Sub-oscillator tuning offset in semitones.
        float subMix = 0.5f;                              // Mix level for sub-oscillator (0 to 1).
        float subTrack = 1.0f;                            // Sub-oscillator keyboard tracking amount (1 =
                                                          // full tracking).
        int unison = 1;                                   // Number of unison voices (1 to 8).
        float detune = 0.01f;                             // Detune amount for unison voices.
};

// Structure for filter parameters shared across voices.
struct Filter {
        float resonance = 0.7f;      // Resonance (Q) value for the filter.
        float sampleRate = 48000.0f; // Current sample rate.
};

// Main audio processor class for the synthesizer plugin.
class SimdSynthAudioProcessor : public juce::AudioProcessor {
    public:
        // Constructor for the processor.
        SimdSynthAudioProcessor();
        // Destructor for the processor.
        ~SimdSynthAudioProcessor() override;

        // Prepare the processor for playback (set sample rate, etc.).
        void prepareToPlay(double sampleRate, int samplesPerBlock) override;
        // Release any allocated resources.
        void releaseResources() override;
        // Main processing function: process audio and MIDI.
        void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

        // Create the editor GUI.
        juce::AudioProcessorEditor *createEditor() override;
        // Indicate that this processor has an editor.
        bool hasEditor() const override { return true; }

        // Get the name of the plugin.
        const juce::String getName() const override { return "SimdSynth"; }
        // Accepts MIDI input.
        bool acceptsMidi() const override { return true; }
        // Does not produce MIDI output.
        bool producesMidi() const override { return false; }
        // Not a MIDI effect plugin.
        bool isMidiEffect() const override { return false; }
        // No tail length (no reverb or delay tails).
        double getTailLengthSeconds() const override { return 0.0; }

        // Get the number of presets/programs.
        int getNumPrograms() override;
        // Get the current preset index.
        int getCurrentProgram() override;
        // Set the current preset.
        void setCurrentProgram(int index) override;
        // Get the name of a preset.
        const juce::String getProgramName(int index) override;
        // Change the name of a preset.
        void changeProgramName(int index, const juce::String &newName) override;

        // Save processor state (parameters) to a memory block.
        void getStateInformation(juce::MemoryBlock &destData) override;
        // Load processor state from data.
        void setStateInformation(const void *data, int sizeInBytes) override;
        // Update envelope states for a given time.
        void updateEnvelopes(float t);

        // Added public methods for preset management
        // Save a preset with given name and parameters.
        void savePreset(const juce::String &presetName, const juce::var &parameters) {
            presetManager.writePresetFile(presetName, parameters);
        }
        // Load all presets.
        void loadPresets() { loadPresetsFromDirectory(); }
        // Get the parameter tree.
        juce::AudioProcessorValueTreeState &getParameters() { return parameters; }

        // Get list of preset names.
        juce::StringArray getPresetNames() const {
            juce::StringArray presets;
            // Populate with your preset names, e.g.:
            presets.add("Bass");
            presets.add("Strings2");
            // Add more presets as needed
            return presets;
        }

        // Version number for parameters (for compatibility).
        static const int parameterVersion = 1;

    private:
        // Parameter tree for hosting parameters.
        juce::AudioProcessorValueTreeState parameters;
        // Atomic pointers to parameters for thread-safe access.
        std::atomic<float> *wavetableTypeParam = nullptr; // Wavetable type parameter.
        std::atomic<float> *attackTimeParam = nullptr;    // Amplitude envelope attack.
        std::atomic<float> *decayTimeParam = nullptr;     // Amplitude envelope decay.
        std::atomic<float> *sustainLevelParam = nullptr;  // Amplitude envelope sustain.
        std::atomic<float> *releaseTimeParam = nullptr;   // Amplitude envelope release.
        std::atomic<float> *cutoffParam = nullptr;        // Filter cutoff.
        std::atomic<float> *resonanceParam = nullptr;     // Filter resonance.
        std::atomic<float> *fegAttackParam = nullptr;     // Filter envelope attack.
        std::atomic<float> *fegDecayParam = nullptr;      // Filter envelope decay.
        std::atomic<float> *fegSustainParam = nullptr;    // Filter envelope sustain.
        std::atomic<float> *fegReleaseParam = nullptr;    // Filter envelope release.
        std::atomic<float> *fegAmountParam = nullptr;     // Filter envelope amount.
        std::atomic<float> *lfoRateParam = nullptr;       // LFO rate.
        std::atomic<float> *lfoDepthParam = nullptr;      // LFO depth.
        std::atomic<float> *subTuneParam = nullptr;       // Sub-oscillator tune.
        std::atomic<float> *subMixParam = nullptr;        // Sub-oscillator mix.
        std::atomic<float> *subTrackParam = nullptr;      // Sub-oscillator track.
        std::atomic<float> *gainParam = nullptr;          // Output gain.
        std::atomic<float> *unisonParam = nullptr;        // Unison count.
        std::atomic<float> *detuneParam = nullptr;        // Unison detune.

        // Current time counter (for envelopes, etc.).
        double currentTime;
        // Wavetable arrays for different waveforms.
        float sineTable[WAVETABLE_SIZE];   // Sine wavetable.
        float sawTable[WAVETABLE_SIZE];    // Sawtooth wavetable.
        float squareTable[WAVETABLE_SIZE]; // Square wavetable.
        // Array of voices.
        Voice voices[MAX_VOICE_POLYPHONY];
        // Shared filter parameters.
        Filter filter;
        // Oversampling object to reduce aliasing (4x).
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

        // Initialize wavetables with precomputed values.
        void initWavetables();
        // Convert MIDI note to frequency.
        float midiToFreq(int midiNote);
        // Randomize a value around a base with variance.
        float randomize(float base, float var);
#ifdef __arm64__
        // Custom floor function for NEON (since no intrinsic).
        float32x4_t my_floor_ps(float32x4_t x);
#endif
#ifdef __x86_64__
        // Fast sine approximation for SSE.
        __m128 fast_sin_ps(__m128 x);
#endif
#ifdef __arm64__
        // Fast sine approximation for NEON.
        float32x4_t fast_sin_ps(float32x4_t x);
#endif
        // SIMD wavetable lookup with linear interpolation.
        SIMD_TYPE wavetable_lookup_ps(SIMD_TYPE phase, const float *table);
        // Apply ladder filter to voices using SIMD.
        void applyLadderFilter(Voice *voices, int voiceOffset, SIMD_TYPE input, Filter &filter, SIMD_TYPE &output);

        // Preset management
        PresetManager presetManager;           // Manager for loading/saving presets.
        std::vector<juce::String> presetNames; // List of preset names.
        int currentProgram = 0;                // Current preset index.
        // Load presets from directory.
        void loadPresetsFromDirectory();

        // JUCE macro to prevent copying and enable leak detection.
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimdSynthAudioProcessor)
};

// Declare the plugin creation function with C++ linkage (required for JUCE
// plugins).
juce::AudioProcessor *createPluginFilter();