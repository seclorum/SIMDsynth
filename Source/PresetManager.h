/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 8 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

#ifndef SIMDSYNTH_PRESETMANAGER_H
#define SIMDSYNTH_PRESETMANAGER_H

// PresetManager.h
#pragma once

#include <juce_core/juce_core.h>

class PresetManager {
    public:
        PresetManager();
        void createDefaultPresets();
        void writePresetFile(const juce::String &presetName, const juce::var &parameters);

    private:
        juce::File getPresetDirectory();
};

#endif // SIMDSYNTH_PRESETMANAGER_H
