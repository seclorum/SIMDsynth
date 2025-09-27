/*
 * simdsynth - a playground for experimenting with SIMD-based audio
 *             synthesis, with polyphonic main and sub-oscillator,
 *             filter, envelopes, and LFO per voice, up to 8 voices.
 *
 * MIT Licensed, (c) 2025, seclorum
 */

#include "PresetManager.h"

// !J! Turn this off .. eventually .. but for now, just always re-create factor presets on launch ..
static const bool alwaysOverWritePresetsDuringDevelopment = true;

PresetManager::PresetManager() {}

juce::File PresetManager::getPresetDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("SimdSynth/Presets");
}

void PresetManager::writePresetFile(const juce::String &presetName, const juce::var &parameters) {
    juce::File presetFile = getPresetDirectory().getChildFile(presetName + ".json");
    if (!presetFile.existsAsFile() || alwaysOverWritePresetsDuringDevelopment) { // Only write if file doesn't exist
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

    auto makeSimdSynthPatch = [](const juce::String &name, std::map<juce::String, float> values,
                                 PresetManager &manager) {
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        juce::DynamicObject::Ptr synth = new juce::DynamicObject();

        for (const auto &[key, value] : values) {
            synth->setProperty(key, value);
        }

        root->setProperty("SimdSynth", juce::var(synth.get()));
        manager.writePresetFile(name, juce::var(root.get()));
    };

        // Updated presets with harmonic osc2Tune, lower osc2Mix, capped lfoDepth, and safe resonance
    makeSimdSynthPatch("Clavichord",
                       {{"wavetable", 2.0f}, {"attack", 0.01f},    {"decay", 0.3f},      {"sustain", 0.0f},
                        {"release", 0.05f},   {"cutoff", 4000.0f},  {"resonance", 0.4f},  {"fegAttack", 0.01f},
                        {"fegDecay", 0.2f},   {"fegSustain", 0.0f}, {"fegRelease", 0.05f}, {"fegAmount", 0.3f},
                        {"lfoRate", 1.5f},    {"lfoDepth", 0.03f},  {"subTune", -12.0f},  {"subMix", 0.3f},
                        {"subTrack", 1.0f},   {"osc2Tune", 7.0f},   {"osc2Mix", 0.25f},   {"osc2Track", 1.0f},
                        {"gain", 1.0f},       {"unison", 3.0f},     {"detune", 0.025f}},
                       *this);

    makeSimdSynthPatch("Bass",
                       {{"wavetable", 1.0f}, {"attack", 0.01f},    {"decay", 0.5f},      {"sustain", 0.8f},
                        {"release", 0.2f},    {"cutoff", 800.0f},   {"resonance", 0.6f},  {"fegAttack", 0.01f},
                        {"fegDecay", 0.4f},   {"fegSustain", 0.2f}, {"fegRelease", 0.1f}, {"fegAmount", 0.5f},
                        {"lfoRate", 0.8f},    {"lfoDepth", 0.04f},  {"subTune", -24.0f},  {"subMix", 0.9f},
                        {"subTrack", 1.0f},   {"osc2Tune", -12.0f}, {"osc2Mix", 0.4f},    {"osc2Track", 1.0f},
                        {"gain", 1.2f},       {"unison", 2.0f},     {"detune", 0.02f}},
                       *this);

    makeSimdSynthPatch("Pad",
                       {{"wavetable", 0.0f}, {"attack", 1.5f},     {"decay", 3.0f},      {"sustain", 0.9f},
                        {"release", 2.0f},    {"cutoff", 2000.0f},  {"resonance", 0.3f},  {"fegAttack", 1.0f},
                        {"fegDecay", 2.0f},   {"fegSustain", 0.8f}, {"fegRelease", 1.5f}, {"fegAmount", 0.4f},
                        {"lfoRate", 0.5f},    {"lfoDepth", 0.04f},  {"subTune", -12.0f},  {"subMix", 0.5f},
                        {"subTrack", 1.0f},   {"osc2Tune", 12.0f},  {"osc2Mix", 0.4f},    {"osc2Track", 1.0f},
                        {"gain", 0.8f},       {"unison", 4.0f},     {"detune", 0.04f}},
                       *this);

    makeSimdSynthPatch("Strings1",
                       {{"wavetable", 1.0f}, {"attack", 1.0f},     {"decay", 2.0f},      {"sustain", 0.9f},
                        {"release", 1.0f},    {"cutoff", 1500.0f},  {"resonance", 0.2f},  {"fegAttack", 0.8f},
                        {"fegDecay", 1.5f},   {"fegSustain", 0.9f}, {"fegRelease", 1.0f}, {"fegAmount", 0.3f},
                        {"lfoRate", 0.3f},    {"lfoDepth", 0.03f},  {"subTune", -12.0f},  {"subMix", 0.4f},
                        {"subTrack", 1.0f},   {"osc2Tune", 19.0f},  {"osc2Mix", 0.35f},   {"osc2Track", 1.0f},
                        {"gain", 0.9f},       {"unison", 3.0f},     {"detune", 0.025f}},
                       *this);

    makeSimdSynthPatch("Strings2",
                       {{"wavetable", 2.0f}, {"attack", 0.8f},     {"decay", 1.5f},      {"sustain", 0.8f},
                        {"release", 0.8f},    {"cutoff", 3000.0f},  {"resonance", 0.5f},  {"fegAttack", 0.6f},
                        {"fegDecay", 1.0f},   {"fegSustain", 0.7f}, {"fegRelease", 0.8f}, {"fegAmount", 0.4f},
                        {"lfoRate", 0.4f},    {"lfoDepth", 0.03f},  {"subTune", -12.0f},  {"subMix", 0.3f},
                        {"subTrack", 1.0f},   {"osc2Tune", 14.0f},  {"osc2Mix", 0.35f},   {"osc2Track", 1.0f},
                        {"gain", 0.9f},       {"unison", 3.0f},     {"detune", 0.03f}},
                       *this);

    makeSimdSynthPatch("SciFiSweep",
                       {{"wavetable", 1.0f}, {"attack", 0.2f},     {"decay", 1.0f},      {"sustain", 0.5f},
                        {"release", 0.3f},    {"cutoff", 5000.0f},  {"resonance", 0.8f},  {"fegAttack", 0.1f},
                        {"fegDecay", 0.5f},   {"fegSustain", 0.3f}, {"fegRelease", 0.3f}, {"fegAmount", 0.7f},
                        {"lfoRate", 5.0f},    {"lfoDepth", 0.08f},  {"subTune", -24.0f},  {"subMix", 0.6f},
                        {"subTrack", 0.0f},   {"osc2Tune", 5.0f},   {"osc2Mix", 0.3f},    {"osc2Track", 0.0f},
                        {"gain", 1.0f},       {"unison", 2.0f},     {"detune", 0.02f}},
                       *this);

    makeSimdSynthPatch("MetallicDrone",
                       {{"wavetable", 2.0f}, {"attack", 1.0f},     {"decay", 4.0f},      {"sustain", 1.0f},
                        {"release", 1.5f},    {"cutoff", 1000.0f},  {"resonance", 0.8f},  {"fegAttack", 1.0f},
                        {"fegDecay", 3.0f},   {"fegSustain", 0.8f}, {"fegRelease", 1.5f}, {"fegAmount", 0.6f},
                        {"lfoRate", 0.2f},    {"lfoDepth", 0.06f},  {"subTune", -24.0f},  {"subMix", 0.7f},
                        {"subTrack", 0.0f},   {"osc2Tune", -19.0f}, {"osc2Mix", 0.4f},    {"osc2Track", 0.0f},
                        {"gain", 0.8f},       {"unison", 4.0f},     {"detune", 0.05f}},
                       *this);

    makeSimdSynthPatch("GlitchPulse",
                       {{"wavetable", 2.0f}, {"attack", 0.01f},    {"decay", 0.2f},      {"sustain", 0.0f},
                        {"release", 0.05f},   {"cutoff", 6000.0f},  {"resonance", 0.7f},  {"fegAttack", 0.01f},
                        {"fegDecay", 0.1f},   {"fegSustain", 0.0f}, {"fegRelease", 0.05f}, {"fegAmount", 0.5f},
                        {"lfoRate", 10.0f},   {"lfoDepth", 0.1f},   {"subTune", -12.0f},  {"subMix", 0.4f},
                        {"subTrack", 1.0f},   {"osc2Tune", 12.0f},  {"osc2Mix", 0.25f},   {"osc2Track", 1.0f},
                        {"gain", 1.0f},       {"unison", 2.0f},     {"detune", 0.025f}},
                       *this);

    makeSimdSynthPatch("SpaceAmbience",
                       {{"wavetable", 0.0f}, {"attack", 2.0f},     {"decay", 5.0f},      {"sustain", 1.0f},
                        {"release", 2.0f},    {"cutoff", 800.0f},   {"resonance", 0.3f},  {"fegAttack", 1.5f},
                        {"fegDecay", 4.0f},   {"fegSustain", 0.9f}, {"fegRelease", 2.0f}, {"fegAmount", 0.2f},
                        {"lfoRate", 0.1f},    {"lfoDepth", 0.07f},  {"subTune", -24.0f},  {"subMix", 0.6f},
                        {"subTrack", 0.0f},   {"osc2Tune", 19.0f},  {"osc2Mix", 0.4f},    {"osc2Track", 0.0f},
                        {"gain", 0.7f},       {"unison", 5.0f},     {"detune", 0.055f}},
                       *this);

    makeSimdSynthPatch("LaserZap",
                       {{"wavetable", 1.0f}, {"attack", 0.01f},    {"decay", 0.3f},      {"sustain", 0.0f},
                        {"release", 0.1f},    {"cutoff", 7000.0f},  {"resonance", 0.8f},  {"fegAttack", 0.01f},
                        {"fegDecay", 0.2f},   {"fegSustain", 0.0f}, {"fegRelease", 0.1f}, {"fegAmount", 0.6f},
                        {"lfoRate", 15.0f},   {"lfoDepth", 0.09f},  {"subTune", -12.0f},  {"subMix", 0.3f},
                        {"subTrack", 1.0f},   {"osc2Tune", 5.0f},   {"osc2Mix", 0.25f},   {"osc2Track", 1.0f},
                        {"gain", 1.0f},       {"unison", 2.0f},     {"detune", 0.02f}},
                       *this);

    makeSimdSynthPatch("Organ",
                       {{"wavetable", 2.0f}, {"attack", 0.02f},    {"decay", 0.1f},      {"sustain", 1.0f},
                        {"release", 0.05f},   {"cutoff", 5000.0f},  {"resonance", 0.3f},  {"fegAttack", 0.02f},
                        {"fegDecay", 0.1f},   {"fegSustain", 0.8f}, {"fegRelease", 0.05f}, {"fegAmount", 0.2f},
                        {"lfoRate", 6.0f},    {"lfoDepth", 0.03f},  {"subTune", -12.0f},  {"subMix", 0.4f},
                        {"subTrack", 1.0f},   {"osc2Tune", 14.0f},  {"osc2Mix", 0.35f},   {"osc2Track", 1.0f},
                        {"gain", 0.9f},       {"unison", 4.0f},     {"detune", 0.035f}},
                       *this);

    makeSimdSynthPatch("Piano",
                       {{"wavetable", 2.0f}, {"attack", 0.01f},    {"decay", 0.8f},      {"sustain", 0.2f},
                        {"release", 0.2f},    {"cutoff", 3000.0f},  {"resonance", 0.4f},  {"fegAttack", 0.01f},
                        {"fegDecay", 0.5f},   {"fegSustain", 0.3f}, {"fegRelease", 0.2f}, {"fegAmount", 0.4f},
                        {"lfoRate", 0.5f},    {"lfoDepth", 0.02f},  {"subTune", -12.0f},  {"subMix", 0.25f},
                        {"subTrack", 1.0f},   {"osc2Tune", 7.0f},   {"osc2Mix", 0.25f},   {"osc2Track", 1.0f},
                        {"gain", 1.1f},       {"unison", 3.0f},     {"detune", 0.025f}},
                       *this);

    makeSimdSynthPatch("Drum",
                       {{"wavetable", 0.0f}, {"attack", 0.001f},   {"decay", 0.1f},      {"sustain", 0.0f},
                        {"release", 0.05f},   {"cutoff", 8000.0f},  {"resonance", 0.6f},  {"fegAttack", 0.001f},
                        {"fegDecay", 0.05f},  {"fegSustain", 0.0f}, {"fegRelease", 0.05f}, {"fegAmount", 0.8f},
                        {"lfoRate", 0.0f},    {"lfoDepth", 0.0f},   {"subTune", -24.0f},  {"subMix", 0.6f},
                        {"subTrack", 1.0f},   {"osc2Tune", -12.0f}, {"osc2Mix", 0.3f},    {"osc2Track", 1.0f},
                        {"gain", 1.3f},       {"unison", 1.0f},     {"detune", 0.0f}},
                       *this);

    makeSimdSynthPatch("Flute",
                       {{"wavetable", 0.0f}, {"attack", 0.1f},     {"decay", 0.5f},      {"sustain", 0.9f},
                        {"release", 0.2f},    {"cutoff", 2000.0f},  {"resonance", 0.2f},  {"fegAttack", 0.1f},
                        {"fegDecay", 0.3f},   {"fegSustain", 0.7f}, {"fegRelease", 0.2f}, {"fegAmount", 0.1f},
                        {"lfoRate", 4.0f},    {"lfoDepth", 0.03f},  {"subTune", -12.0f},  {"subMix", 0.2f},
                        {"subTrack", 1.0f},   {"osc2Tune", 12.0f},  {"osc2Mix", 0.15f},   {"osc2Track", 1.0f},
                        {"gain", 0.8f},       {"unison", 2.0f},     {"detune", 0.015f}},
                       *this);

    makeSimdSynthPatch("FunkyBass",
                       {{"wavetable", 1.0f}, {"attack", 0.01f},    {"decay", 0.3f},      {"sustain", 0.7f},
                        {"release", 0.1f},    {"cutoff", 1200.0f},  {"resonance", 0.7f},  {"fegAttack", 0.01f},
                        {"fegDecay", 0.2f},   {"fegSustain", 0.4f}, {"fegRelease", 0.1f}, {"fegAmount", 0.6f},
                        {"lfoRate", 2.0f},    {"lfoDepth", 0.06f},  {"subTune", -24.0f},  {"subMix", 0.8f},
                        {"subTrack", 1.0f},   {"osc2Tune", -7.0f},  {"osc2Mix", 0.4f},    {"osc2Track", 1.0f},
                        {"gain", 1.2f},       {"unison", 3.0f},     {"detune", 0.035f}},
                       *this);

    makeSimdSynthPatch("303bass",
                       {{"wavetable", 1.0f}, {"attack", 0.01f},    {"decay", 0.5f},      {"sustain", 0.7f},
                        {"release", 0.1f},    {"cutoff", 800.0f},   {"resonance", 0.8f},  {"fegAttack", 0.01f},
                        {"fegDecay", 0.3f},   {"fegSustain", 0.5f}, {"fegRelease", 0.1f}, {"fegAmount", 0.8f},
                        {"lfoRate", 1.0f},    {"lfoDepth", 0.05f},  {"subTune", -12.0f},  {"subMix", 0.4f},
                        {"subTrack", 1.0f},   {"osc2Tune", -12.0f}, {"osc2Mix", 0.35f},   {"osc2Track", 1.0f},
                        {"gain", 1.0f},       {"unison", 2.0f},     {"detune", 0.02f}},
                       *this);

    makeSimdSynthPatch("thinPads",
                       {{"wavetable", 0.0f}, {"attack", 2.0f},     {"decay", 2.0f},      {"sustain", 0.8f},
                        {"release", 2.0f},    {"cutoff", 4000.0f},  {"resonance", 0.3f},  {"fegAttack", 2.0f},
                        {"fegDecay", 2.0f},   {"fegSustain", 0.7f}, {"fegRelease", 2.0f}, {"fegAmount", 0.3f},
                        {"lfoRate", 0.5f},    {"lfoDepth", 0.1f},   {"subTune", -12.0f},  {"subMix", 0.2f},
                        {"subTrack", 1.0f},   {"osc2Tune", 7.0f},   {"osc2Mix", 0.35f},   {"osc2Track", 1.0f},
                        {"gain", 0.8f},       {"unison", 4.0f},     {"detune", 0.035f}},
                       *this);

    makeSimdSynthPatch("fatPads",
                       {{"wavetable", 1.0f}, {"attack", 3.0f},     {"decay", 3.0f},      {"sustain", 0.9f},
                        {"release", 3.5f},    {"cutoff", 2000.0f},  {"resonance", 0.5f},  {"fegAttack", 3.0f},
                        {"fegDecay", 3.0f},   {"fegSustain", 0.8f}, {"fegRelease", 3.5f}, {"fegAmount", 0.6f},
                        {"lfoRate", 0.3f},    {"lfoDepth", 0.1f},   {"subTune", -24.0f},  {"subMix", 0.5f},
                        {"subTrack", 1.0f},   {"osc2Tune", 19.0f},  {"osc2Mix", 0.4f},    {"osc2Track", 1.0f},
                        {"gain", 0.9f},       {"unison", 6.0f},     {"detune", 0.06f}},
                       *this);

    makeSimdSynthPatch("BrassStab",
                       {{"wavetable", 2.0f}, {"attack", 0.01f},    {"decay", 0.4f},      {"sustain", 0.3f},
                        {"release", 0.15f},   {"cutoff", 5000.0f},  {"resonance", 0.6f},  {"fegAttack", 0.01f},
                        {"fegDecay", 0.3f},   {"fegSustain", 0.4f}, {"fegRelease", 0.15f}, {"fegAmount", 0.5f},
                        {"lfoRate", 1.5f},    {"lfoDepth", 0.04f},  {"subTune", -12.0f},  {"subMix", 0.3f},
                        {"subTrack", 1.0f},   {"osc2Tune", 14.0f},  {"osc2Mix", 0.25f},   {"osc2Track", 1.0f},
                        {"gain", 1.2f},       {"unison", 4.0f},     {"detune", 0.025f}},
                       *this);

    makeSimdSynthPatch("electroPiano",
                       {{"wavetable", 0.0f}, {"attack", 0.02f},    {"decay", 1.0f},      {"sustain", 0.3f},
                        {"release", 0.3f},    {"cutoff", 3500.0f},  {"resonance", 0.4f},  {"fegAttack", 0.02f},
                        {"fegDecay", 0.7f},   {"fegSustain", 0.3f}, {"fegRelease", 0.3f}, {"fegAmount", 0.3f},
                        {"lfoRate", 2.0f},    {"lfoDepth", 0.03f},  {"subTune", -12.0f},  {"subMix", 0.2f},
                        {"subTrack", 1.0f},   {"osc2Tune", 7.0f},   {"osc2Mix", 0.25f},   {"osc2Track", 1.0f},
                        {"gain", 1.0f},       {"unison", 3.0f},     {"detune", 0.02f}},
                       *this);

    makeSimdSynthPatch("MoroderSweep",
                       {{"wavetable", 1.0f}, {"attack", 0.05f},    {"decay", 1.5f},      {"sustain", 0.6f},
                        {"release", 1.5f},    {"cutoff", 1000.0f},  {"resonance", 0.8f},  {"fegAttack", 2.0f},
                        {"fegDecay", 2.0f},   {"fegSustain", 0.5f}, {"fegRelease", 1.5f}, {"fegAmount", 0.7f},
                        {"lfoRate", 0.2f},    {"lfoDepth", 0.1f},   {"subTune", -12.0f},  {"subMix", 0.4f},
                        {"subTrack", 1.0f},   {"osc2Tune", 14.0f},  {"osc2Mix", 0.35f},   {"osc2Track", 1.0f},
                        {"gain", 0.9f},       {"unison", 3.0f},     {"detune", 0.035f}},
                       *this);

    makeSimdSynthPatch("longResoFX",
                       {{"wavetable", 2.0f}, {"attack", 1.0f},     {"decay", 2.0f},      {"sustain", 0.7f},
                        {"release", 4.0f},    {"cutoff", 1500.0f},  {"resonance", 0.8f},  {"fegAttack", 1.5f},
                        {"fegDecay", 2.0f},   {"fegSustain", 0.6f}, {"fegRelease", 4.0f}, {"fegAmount", 0.8f},
                        {"lfoRate", 0.1f},    {"lfoDepth", 0.1f},   {"subTune", -24.0f},  {"subMix", 0.5f},
                        {"subTrack", 1.0f},   {"osc2Tune", 19.0f},  {"osc2Mix", 0.4f},    {"osc2Track", 1.0f},
                        {"gain", 0.8f},       {"unison", 5.0f},     {"detune", 0.045f}},
                       *this);

    makeSimdSynthPatch("robotFart",
                       {{"wavetable", 1.0f}, {"attack", 0.01f},    {"decay", 0.3f},      {"sustain", 0.2f},
                        {"release", 0.15f},   {"cutoff", 600.0f},   {"resonance", 0.7f},  {"fegAttack", 0.01f},
                        {"fegDecay", 0.2f},   {"fegSustain", 0.3f}, {"fegRelease", 0.15f}, {"fegAmount", 0.6f},
                        {"lfoRate", 5.0f},    {"lfoDepth", 0.1f},   {"subTune", -12.0f},  {"subMix", 0.6f},
                        {"subTrack", 1.0f},   {"osc2Tune", -7.0f},  {"osc2Mix", 0.25f},   {"osc2Track", 1.0f},
                        {"gain", 1.0f},       {"unison", 2.0f},     {"detune", 0.02f}},
                       *this);

    makeSimdSynthPatch("jellyBand",
                       {{"wavetable", 0.0f}, {"attack", 0.1f},     {"decay", 0.7f},      {"sustain", 0.5f},
                        {"release", 0.5f},    {"cutoff", 2500.0f},  {"resonance", 0.5f},  {"fegAttack", 0.1f},
                        {"fegDecay", 0.6f},   {"fegSustain", 0.4f}, {"fegRelease", 0.5f}, {"fegAmount", 0.4f},
                        {"lfoRate", 2.0f},    {"lfoDepth", 0.1f},   {"subTune", -12.0f},  {"subMix", 0.4f},
                        {"subTrack", 1.0f},   {"osc2Tune", 14.0f},  {"osc2Mix", 0.35f},   {"osc2Track", 1.0f},
                        {"gain", 1.0f},       {"unison", 4.0f},     {"detune", 0.035f}},
                       *this);

    makeSimdSynthPatch("grokGrokGrok",
                       {{"wavetable", 2.0f}, {"attack", 0.05f},    {"decay", 0.5f},      {"sustain", 0.4f},
                        {"release", 0.3f},    {"cutoff", 2000.0f},  {"resonance", 0.8f},  {"fegAttack", 0.05f},
                        {"fegDecay", 0.4f},   {"fegSustain", 0.5f}, {"fegRelease", 0.3f}, {"fegAmount", 0.7f},
                        {"lfoRate", 3.0f},    {"lfoDepth", 0.1f},   {"subTune", -12.0f},  {"subMix", 0.3f},
                        {"subTrack", 1.0f},   {"osc2Tune", 5.0f},   {"osc2Mix", 0.35f},   {"osc2Track", 1.0f},
                        {"gain", 1.1f},       {"unison", 3.0f},     {"detune", 0.025f}},
                       *this);

    makeSimdSynthPatch("BrightLead",
                       {{"wavetable", 1.0f}, {"attack", 0.01f},    {"decay", 0.5f},      {"sustain", 0.8f},
                        {"release", 0.2f},    {"cutoff", 8000.0f},  {"resonance", 0.5f},  {"fegAttack", 0.01f},
                        {"fegDecay", 0.3f},   {"fegSustain", 0.6f}, {"fegRelease", 0.2f}, {"fegAmount", 0.4f},
                        {"lfoRate", 5.0f},    {"lfoDepth", 0.04f},  {"subTune", -12.0f},  {"subMix", 0.3f},
                        {"subTrack", 1.0f},   {"osc2Tune", 12.0f},  {"osc2Mix", 0.35f},   {"osc2Track", 1.0f},
                        {"gain", 1.0f},       {"unison", 2.0f},     {"detune", 0.02f}},
                       *this);

    makeSimdSynthPatch("DeepArp",
                       {{"wavetable", 2.0f}, {"attack", 0.05f},    {"decay", 0.3f},      {"sustain", 0.5f},
                        {"release", 0.1f},    {"cutoff", 3000.0f},  {"resonance", 0.7f},  {"fegAttack", 0.05f},
                        {"fegDecay", 0.2f},   {"fegSustain", 0.4f}, {"fegRelease", 0.1f}, {"fegAmount", 0.6f},
                        {"lfoRate", 1.5f},    {"lfoDepth", 0.05f},  {"subTune", -24.0f},  {"subMix", 0.5f},
                        {"subTrack", 0.0f},   {"osc2Tune", -12.0f}, {"osc2Mix", 0.3f},    {"osc2Track", 0.0f},
                        {"gain", 1.0f},       {"unison", 3.0f},     {"detune", 0.025f}},
                       *this);

    makeSimdSynthPatch("AmbientWash",
                       {{"wavetable", 0.0f}, {"attack", 3.0f},     {"decay", 4.0f},      {"sustain", 1.0f},
                        {"release", 4.0f},    {"cutoff", 1000.0f},  {"resonance", 0.3f},  {"fegAttack", 2.0f},
                        {"fegDecay", 3.0f},   {"fegSustain", 0.9f}, {"fegRelease", 4.0f}, {"fegAmount", 0.5f},
                        {"lfoRate", 0.2f},    {"lfoDepth", 0.1f},   {"subTune", -24.0f},  {"subMix", 0.7f},
                        {"subTrack", 0.0f},   {"osc2Tune", 19.0f},  {"osc2Mix", 0.4f},    {"osc2Track", 0.0f},
                        {"gain", 0.7f},       {"unison", 6.0f},     {"detune", 0.065f}},
                       *this);

    makeSimdSynthPatch("PluckySynth",
                       {{"wavetable", 0.0f}, {"attack", 0.01f},    {"decay", 0.4f},      {"sustain", 0.0f},
                        {"release", 0.1f},    {"cutoff", 6000.0f},  {"resonance", 0.4f},  {"fegAttack", 0.01f},
                        {"fegDecay", 0.2f},   {"fegSustain", 0.0f}, {"fegRelease", 0.1f}, {"fegAmount", 0.7f},
                        {"lfoRate", 3.0f},    {"lfoDepth", 0.04f},  {"subTune", -12.0f},  {"subMix", 0.4f},
                        {"subTrack", 1.0f},   {"osc2Tune", 7.0f},   {"osc2Mix", 0.25f},   {"osc2Track", 1.0f},
                        {"gain", 1.1f},       {"unison", 3.0f},     {"detune", 0.025f}},
                       *this);

    makeSimdSynthPatch("GrittyBass",
                       {{"wavetable", 1.0f}, {"attack", 0.01f},    {"decay", 0.3f},      {"sustain", 0.6f},
                        {"release", 0.2f},    {"cutoff", 1000.0f},  {"resonance", 0.8f},  {"fegAttack", 0.01f},
                        {"fegDecay", 0.3f},   {"fegSustain", 0.5f}, {"fegRelease", 0.2f}, {"fegAmount", 0.8f},
                        {"lfoRate", 3.0f},    {"lfoDepth", 0.07f},  {"subTune", -24.0f},  {"subMix", 0.6f},
                        {"subTrack", 1.0f},   {"osc2Tune", -7.0f},  {"osc2Mix", 0.4f},    {"osc2Track", 1.0f},
                        {"gain", 1.3f},       {"unison", 4.0f},     {"detune", 0.045f}},
                       *this);
}