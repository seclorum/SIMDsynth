//
// Created by Jay Vaughan on 22.09.25.
//

#ifndef SIMDSYNTH_PRESETMANAGER_H
#define SIMDSYNTH_PRESETMANAGER_H

// PresetManager.h
#pragma once

#include <juce_core/juce_core.h>

class PresetManager {
public:
    PresetManager();
    void createDefaultPresets();

private:
    juce::File getPresetDirectory();
    void writePresetFile(const juce::String& presetName, const juce::var& parameters);
};

#endif //SIMDSYNTH_PRESETMANAGER_H
