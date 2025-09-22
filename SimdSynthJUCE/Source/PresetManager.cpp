//
// Created by Jay Vaughan on 22.09.25.
//

#include "PresetManager.h"

PresetManager::PresetManager() {}

juce::File PresetManager::getPresetDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("SimdSynth/Presets");
}

void PresetManager::writePresetFile(const juce::String& presetName, const juce::var& parameters) {
    juce::File presetFile = getPresetDirectory().getChildFile(presetName + ".json");
    if (!presetFile.existsAsFile()) { // Only write if file doesn't exist
        if (presetFile.replaceWithText(juce::JSON::toString(parameters))) {
            DBG("Created preset: " << presetFile.getFullPathName());
        } else {
            DBG("Failed to create preset: " << presetFile.getFullPathName());
        }
    }
}

void PresetManager::createDefaultPresets() {
    juce::File presetDir = getPresetDirectory();
    if (!presetDir.exists()) {
        presetDir.createDirectory();
        DBG("Created preset directory: " << presetDir.getFullPathName());
    }

    auto makeSimdSynthPatch = [](const juce::String& name, std::map<juce::String, float> values, PresetManager& manager) {
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        juce::DynamicObject::Ptr synth = new juce::DynamicObject();

        for (const auto& [key, value] : values) {
            synth->setProperty(key, value);
        }

        root->setProperty("SimdSynth", juce::var(synth.get()));
        manager.writePresetFile(name, juce::var(root.get()));
    };

    makeSimdSynthPatch("Clavichord", {
            {"wavetable", 0.75f}, {"attack", 0.01f}, {"decay", 0.3f},
            {"cutoff", 4000.0f}, {"resonance", 0.4f}, {"fegAttack", 0.01f},
            {"fegDecay", 0.2f}, {"fegSustain", 0.0f}, {"fegRelease", 0.05f},
            {"lfoRate", 0.0f}, {"lfoDepth", 0.0f}, {"subTune", -12.0f},
            {"subMix", 0.2f}, {"subTrack", 1.0f}
    }, *this);

    makeSimdSynthPatch("Bass", {
            {"wavetable", 0.25f}, {"attack", 0.01f}, {"decay", 0.5f},
            {"cutoff", 800.0f}, {"resonance", 0.6f}, {"fegAttack", 0.01f},
            {"fegDecay", 0.4f}, {"fegSustain", 0.2f}, {"fegRelease", 0.1f},
            {"lfoRate", 0.0f}, {"lfoDepth", 0.0f}, {"subTune", -24.0f},
            {"subMix", 0.8f}, {"subTrack", 1.0f}
    }, *this);

    makeSimdSynthPatch("Pad", {
            {"wavetable", 0.5f}, {"attack", 1.5f}, {"decay", 3.0f},
            {"cutoff", 2000.0f}, {"resonance", 0.3f}, {"fegAttack", 1.0f},
            {"fegDecay", 2.0f}, {"fegSustain", 0.8f}, {"fegRelease", 1.5f},
            {"lfoRate", 0.5f}, {"lfoDepth", 0.05f}, {"subTune", -12.0f},
            {"subMix", 0.4f}, {"subTrack", 1.0f}
    }, *this);

    makeSimdSynthPatch("Strings1", {
            {"wavetable", 0.5f}, {"attack", 1.0f}, {"decay", 2.0f},
            {"cutoff", 1500.0f}, {"resonance", 0.2f}, {"fegAttack", 0.8f},
            {"fegDecay", 1.5f}, {"fegSustain", 0.9f}, {"fegRelease", 1.0f},
            {"lfoRate", 0.3f}, {"lfoDepth", 0.03f}, {"subTune", -12.0f},
            {"subMix", 0.3f}, {"subTrack", 1.0f}
    }, *this);

    makeSimdSynthPatch("Strings2", {
            {"wavetable", 0.75f}, {"attack", 0.8f}, {"decay", 1.5f},
            {"cutoff", 3000.0f}, {"resonance", 0.5f}, {"fegAttack", 0.6f},
            {"fegDecay", 1.0f}, {"fegSustain", 0.7f}, {"fegRelease", 0.8f},
            {"lfoRate", 0.4f}, {"lfoDepth", 0.04f}, {"subTune", -12.0f},
            {"subMix", 0.2f}, {"subTrack", 1.0f}
    }, *this);

    makeSimdSynthPatch("SciFiSweep", {
            {"wavetable", 1.0f}, {"attack", 0.2f}, {"decay", 1.0f},
            {"cutoff", 5000.0f}, {"resonance", 0.8f}, {"fegAttack", 0.1f},
            {"fegDecay", 0.5f}, {"fegSustain", 0.3f}, {"fegRelease", 0.3f},
            {"lfoRate", 5.0f}, {"lfoDepth", 0.08f}, {"subTune", -24.0f},
            {"subMix", 0.5f}, {"subTrack", 0.0f}
    }, *this);

    makeSimdSynthPatch("MetallicDrone", {
            {"wavetable", 0.9f}, {"attack", 1.0f}, {"decay", 4.0f},
            {"cutoff", 1000.0f}, {"resonance", 0.9f}, {"fegAttack", 1.0f},
            {"fegDecay", 3.0f}, {"fegSustain", 0.8f}, {"fegRelease", 1.5f},
            {"lfoRate", 0.2f}, {"lfoDepth", 0.06f}, {"subTune", -24.0f},
            {"subMix", 0.6f}, {"subTrack", 0.0f}
    }, *this);

    makeSimdSynthPatch("GlitchPulse", {
            {"wavetable", 1.0f}, {"attack", 0.01f}, {"decay", 0.2f},
            {"cutoff", 6000.0f}, {"resonance", 0.7f}, {"fegAttack", 0.01f},
            {"fegDecay", 0.1f}, {"fegSustain", 0.0f}, {"fegRelease", 0.05f},
            {"lfoRate", 10.0f}, {"lfoDepth", 0.1f}, {"subTune", -12.0f},
            {"subMix", 0.3f}, {"subTrack", 1.0f}
    }, *this);

    makeSimdSynthPatch("SpaceAmbience", {
            {"wavetable", 0.3f}, {"attack", 2.0f}, {"decay", 5.0f},
            {"cutoff", 800.0f}, {"resonance", 0.3f}, {"fegAttack", 1.5f},
            {"fegDecay", 4.0f}, {"fegSustain", 0.9f}, {"fegRelease", 2.0f},
            {"lfoRate", 0.1f}, {"lfoDepth", 0.07f}, {"subTune", -24.0f},
            {"subMix", 0.5f}, {"subTrack", 0.0f}
    }, *this);

    makeSimdSynthPatch("LaserZap", {
            {"wavetable", 0.8f}, {"attack", 0.01f}, {"decay", 0.3f},
            {"cutoff", 7000.0f}, {"resonance", 0.8f}, {"fegAttack", 0.01f},
            {"fegDecay", 0.2f}, {"fegSustain", 0.0f}, {"fegRelease", 0.1f},
            {"lfoRate", 15.0f}, {"lfoDepth", 0.09f}, {"subTune", -12.0f},
            {"subMix", 0.2f}, {"subTrack", 1.0f}
    }, *this);

}