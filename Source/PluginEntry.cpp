/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 8 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

#include <JuceHeader.h>
#include "PluginProcessor.h"

juce::AudioProcessor *createPluginFilter() { return new SimdSynthAudioProcessor(); }
