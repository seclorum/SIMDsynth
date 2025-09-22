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
            {"wavetable_1", 0.75f}, {"attack_1", 0.01f}, {"decay_1", 0.3f},
            {"cutoff_1", 4000.0f}, {"resonance_1", 0.4f}, {"fegAttack_1", 0.01f},
            {"fegDecay_1", 0.2f}, {"fegSustain_1", 0.0f}, {"fegRelease_1", 0.05f},
            {"lfoRate_1", 0.0f}, {"lfoDepth_1", 0.0f}, {"subTune_1", -12.0f},
            {"subMix_1", 0.2f}, {"subTrack_1", 1.0f}
    }, *this);

    makeSimdSynthPatch("Bass", {
            {"wavetable_1", 0.25f}, {"attack_1", 0.01f}, {"decay_1", 0.5f},
            {"cutoff_1", 800.0f}, {"resonance_1", 0.6f}, {"fegAttack_1", 0.01f},
            {"fegDecay_1", 0.4f}, {"fegSustain_1", 0.2f}, {"fegRelease_1", 0.1f},
            {"lfoRate_1", 0.0f}, {"lfoDepth_1", 0.0f}, {"subTune_1", -24.0f},
            {"subMix_1", 0.8f}, {"subTrack_1", 1.0f}
    }, *this);

    makeSimdSynthPatch("Pad", {
            {"wavetable_1", 0.5f}, {"attack_1", 1.5f}, {"decay_1", 3.0f},
            {"cutoff_1", 2000.0f}, {"resonance_1", 0.3f}, {"fegAttack_1", 1.0f},
            {"fegDecay_1", 2.0f}, {"fegSustain_1", 0.8f}, {"fegRelease_1", 1.5f},
            {"lfoRate_1", 0.5f}, {"lfoDepth_1", 0.05f}, {"subTune_1", -12.0f},
            {"subMix_1", 0.4f}, {"subTrack_1", 1.0f}
    }, *this);

    makeSimdSynthPatch("Strings1", {
            {"wavetable_1", 0.5f}, {"attack_1", 1.0f}, {"decay_1", 2.0f},
            {"cutoff_1", 1500.0f}, {"resonance_1", 0.2f}, {"fegAttack_1", 0.8f},
            {"fegDecay_1", 1.5f}, {"fegSustain_1", 0.9f}, {"fegRelease_1", 1.0f},
            {"lfoRate_1", 0.3f}, {"lfoDepth_1", 0.03f}, {"subTune_1", -12.0f},
            {"subMix_1", 0.3f}, {"subTrack_1", 1.0f}
    }, *this);

    makeSimdSynthPatch("Strings2", {
            {"wavetable_1", 0.75f}, {"attack_1", 0.8f}, {"decay_1", 1.5f},
            {"cutoff_1", 3000.0f}, {"resonance_1", 0.5f}, {"fegAttack_1", 0.6f},
            {"fegDecay_1", 1.0f}, {"fegSustain_1", 0.7f}, {"fegRelease_1", 0.8f},
            {"lfoRate_1", 0.4f}, {"lfoDepth_1", 0.04f}, {"subTune_1", -12.0f},
            {"subMix_1", 0.2f}, {"subTrack_1", 1.0f}
    }, *this);

    makeSimdSynthPatch("SciFiSweep", {
            {"wavetable_1", 1.0f}, {"attack_1", 0.2f}, {"decay_1", 1.0f},
            {"cutoff_1", 5000.0f}, {"resonance_1", 0.8f}, {"fegAttack_1", 0.1f},
            {"fegDecay_1", 0.5f}, {"fegSustain_1", 0.3f}, {"fegRelease_1", 0.3f},
            {"lfoRate_1", 5.0f}, {"lfoDepth_1", 0.08f}, {"subTune_1", -24.0f},
            {"subMix_1", 0.5f}, {"subTrack_1", 0.0f}
    }, *this);

    makeSimdSynthPatch("MetallicDrone", {
            {"wavetable_1", 0.9f}, {"attack_1", 1.0f}, {"decay_1", 4.0f},
            {"cutoff_1", 1000.0f}, {"resonance_1", 0.9f}, {"fegAttack_1", 1.0f},
            {"fegDecay_1", 3.0f}, {"fegSustain_1", 0.8f}, {"fegRelease_1", 1.5f},
            {"lfoRate_1", 0.2f}, {"lfoDepth_1", 0.06f}, {"subTune_1", -24.0f},
            {"subMix_1", 0.6f}, {"subTrack_1", 0.0f}
    }, *this);

    makeSimdSynthPatch("GlitchPulse", {
            {"wavetable_1", 1.0f}, {"attack_1", 0.01f}, {"decay_1", 0.2f},
            {"cutoff_1", 6000.0f}, {"resonance_1", 0.7f}, {"fegAttack_1", 0.01f},
            {"fegDecay_1", 0.1f}, {"fegSustain_1", 0.0f}, {"fegRelease_1", 0.05f},
            {"lfoRate_1", 10.0f}, {"lfoDepth_1", 0.1f}, {"subTune_1", -12.0f},
            {"subMix_1", 0.3f}, {"subTrack_1", 1.0f}
    }, *this);

    makeSimdSynthPatch("SpaceAmbience", {
            {"wavetable_1", 0.3f}, {"attack_1", 2.0f}, {"decay_1", 5.0f},
            {"cutoff_1", 800.0f}, {"resonance_1", 0.3f}, {"fegAttack_1", 1.5f},
            {"fegDecay_1", 4.0f}, {"fegSustain_1", 0.9f}, {"fegRelease_1", 2.0f},
            {"lfoRate_1", 0.1f}, {"lfoDepth_1", 0.07f}, {"subTune_1", -24.0f},
            {"subMix_1", 0.5f}, {"subTrack_1", 0.0f}
    }, *this);

    makeSimdSynthPatch("LaserZap", {
            {"wavetable_1", 0.8f}, {"attack_1", 0.01f}, {"decay_1", 0.3f},
            {"cutoff_1", 7000.0f}, {"resonance_1", 0.8f}, {"fegAttack_1", 0.01f},
            {"fegDecay_1", 0.2f}, {"fegSustain_1", 0.0f}, {"fegRelease_1", 0.1f},
            {"lfoRate_1", 15.0f}, {"lfoDepth_1", 0.09f}, {"subTune_1", -12.0f},
            {"subMix_1", 0.2f}, {"subTrack_1", 1.0f}
    }, *this);

}