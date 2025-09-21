#include "SimdSynthPluginProcessor.h"
#include "SimdSynthPluginEditor.h"

//==============================================================================
// Custom SynthesiserSound class (applies to all notes and channels)
class SimdSynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int /*midiNoteNumber*/) override { return true; }
    bool appliesToChannel(int /*midiChannel*/) override { return true; }
};

//==============================================================================
// Custom SynthesiserVoice class (integrates simdsynth logic)
class SimdSynthVoice : public juce::SynthesiserVoice
{
public:
    VoiceState voices[MAX_VOICE_POLYPHONY];
    Filter filter;

    SimdSynthVoice(float& attack, float& decay, float& resonance, float& cutoff)
        : attackTime(attack), decayTime(decay), filterResonance(resonance), filterCutoff(cutoff)
    {
        filter.sampleRate = 48000.0f; // Default, updated in prepareToPlay
        filter.resonance = resonance;
        for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i)
        {
            voices[i].frequency = 0.0f;
            voices[i].phase = 0.0f;
            voices[i].phaseIncrement = 0.0f;
            voices[i].amplitude = 0.0f;
            voices[i].cutoff = cutoff;
            voices[i].filterEnv = 0.0f;
            voices[i].active = false;
            voices[i].fegAttack = 0.1f;
            voices[i].fegDecay = 1.0f;
            voices[i].fegSustain = 0.5f;
            voices[i].fegRelease = 0.2f;
            voices[i].lfoRate = 0.0f; // Disabled LFO as in original
            voices[i].lfoDepth = 0.0f;
            voices[i].lfoPhase = 0.0f;
            for (int j = 0; j < 4; ++j)
                voices[i].filterStates[j] = 0.0f;
        }
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SimdSynthSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* /*sound*/, int /*currentPitchWheelPosition*/) override
    {
        for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i)
        {
            if (!voices[i].active)
            {
                voices[i].active = true;
                voices[i].frequency = midiToFreq(midiNoteNumber);
                voices[i].phaseIncrement = (2.0f * juce::MathConstants<float>::pi * voices[i].frequency) / filter.sampleRate;
                voices[i].phase = 0.0f;
                voices[i].amplitude = velocity;
                voices[i].cutoff = filterCutoff;
                voices[i].fegAttack = randomize(0.1f, 0.2f);
                voices[i].fegDecay = randomize(1.0f, 0.2f);
                voices[i].fegSustain = randomize(0.5f, 0.2f);
                voices[i].fegRelease = randomize(0.2f, 0.2f);
                voices[i].lfoPhase = 0.0f;
                voices[i].noteStartTime = static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.001);
                break;
            }
        }
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            // Let voices continue through release phase
            for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i)
            {
                if (voices[i].active)
                    voices[i].isReleasing = true;
            }
        }
        else
        {
            for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i)
            {
                voices[i].active = false;
                voices[i].amplitude = 0.0f;
                voices[i].filterEnv = 0.0f;
                for (int j = 0; j < 4; ++j)
                    voices[i].filterStates[j] = 0.0f;
            }
        }
    }

    void pitchWheelMoved(int /*newPitchWheelValue*/) override {}
    void controllerMoved(int /*controllerNumber*/, int /*newControllerValue*/) override {}

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        juce::ScopedNoDenormals noDenormals;
        outputBuffer.clear(startSample, numSamples);
#if 0
        // Process MIDI events
        for (const auto metadata : midiMessages)
        {
            auto m = metadata.getMessage();
            if (m.isNoteOn())
                startNote(m.getNoteNumber(), m.getFloatVelocity(), nullptr, 0);
            else if (m.isNoteOff())
                stopNote(m.getFloatVelocity(), true);
        }
#endif

        // Update filter parameters
        filter.resonance = filterResonance;
        for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i)
            voices[i].cutoff = filterCutoff;

        // Synthesis loop (adapted from generateSineSamples)
        const SIMD_TYPE twoPi = SIMD_SET1(2.0f * juce::MathConstants<float>::pi);
        for (int i = startSample; i < startSample + numSamples; ++i)
        {
            float t = i / filter.sampleRate;
            updateEnvelopes(voices, MAX_VOICE_POLYPHONY, attackTime, decayTime, 2.0f, filter.sampleRate, i, 0.0f);

            float outputSample = 0.0f;
            for (int group = 0; group < (MAX_VOICE_POLYPHONY + 3) / 4; ++group)
            {
                int voiceOffset = group * 4;
                if (voiceOffset >= MAX_VOICE_POLYPHONY)
                    continue;

                float tempAmps[4] = { voices[voiceOffset].amplitude, voices[voiceOffset + 1].amplitude,
                                      voices[voiceOffset + 2].amplitude, voices[voiceOffset + 3].amplitude };
                float tempPhases[4] = { voices[voiceOffset].phase, voices[voiceOffset + 1].phase,
                                        voices[voiceOffset + 2].phase, voices[voiceOffset + 3].phase };
                float tempIncrements[4] = { voices[voiceOffset].phaseIncrement, voices[voiceOffset + 1].phaseIncrement,
                                            voices[voiceOffset + 2].phaseIncrement, voices[voiceOffset + 3].phaseIncrement };
                float tempLfoPhases[4] = { voices[voiceOffset].lfoPhase, voices[voiceOffset + 1].lfoPhase,
                                           voices[voiceOffset + 2].lfoPhase, voices[voiceOffset + 3].lfoPhase };
                float tempLfoRates[4] = { voices[voiceOffset].lfoRate, voices[voiceOffset + 1].lfoRate,
                                          voices[voiceOffset + 2].lfoRate, voices[voiceOffset + 3].lfoRate };
                float tempLfoDepths[4] = { voices[voiceOffset].lfoDepth, voices[voiceOffset + 1].lfoDepth,
                                           voices[voiceOffset + 2].lfoDepth, voices[voiceOffset + 3].lfoDepth };

                SIMD_TYPE amplitudes = SIMD_LOAD(tempAmps);
                SIMD_TYPE phases = SIMD_LOAD(tempPhases);
                SIMD_TYPE increments = SIMD_LOAD(tempIncrements);
                SIMD_TYPE lfoPhases = SIMD_LOAD(tempLfoPhases);
                SIMD_TYPE lfoRates = SIMD_LOAD(tempLfoRates);
                SIMD_TYPE lfoDepths = SIMD_LOAD(tempLfoDepths);

                SIMD_TYPE lfoIncrements = SIMD_MUL(lfoRates, SIMD_SET1(2.0f * juce::MathConstants<float>::pi / filter.sampleRate));
                lfoPhases = SIMD_ADD(lfoPhases, lfoIncrements);
                lfoPhases = SIMD_SUB(lfoPhases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(lfoPhases, twoPi)), twoPi));
                SIMD_TYPE lfoValues = SIMD_SIN(lfoPhases);
                lfoValues = SIMD_MUL(lfoValues, lfoDepths);
                phases = SIMD_ADD(phases, lfoValues);

                SIMD_TYPE sinValues = SIMD_SIN(phases);
                sinValues = SIMD_MUL(sinValues, amplitudes);

                SIMD_TYPE filteredOutput;
                applyLadderFilter(voices, voiceOffset, sinValues, filter, filteredOutput, filter.sampleRate);

                float temp[4];
                SIMD_STORE(temp, filteredOutput);
                outputSample += (temp[0] + temp[1] + temp[2] + temp[3]) * 0.5f;

                phases = SIMD_ADD(phases, increments);
                SIMD_TYPE wrap = SIMD_SUB(phases, SIMD_MUL(SIMD_FLOOR(SIMD_DIV(phases, twoPi)), twoPi));
                SIMD_STORE(temp, wrap);
                voices[voiceOffset].phase = temp[0];
                voices[voiceOffset + 1].phase = temp[1];
                voices[voiceOffset + 2].phase = temp[2];
                voices[voiceOffset + 3].phase = temp[3];
                SIMD_STORE(temp, lfoPhases);
                voices[voiceOffset].lfoPhase = temp[0];
                voices[voiceOffset + 1].lfoPhase = temp[1];
                voices[voiceOffset + 2].lfoPhase = temp[2];
                voices[voiceOffset + 3].lfoPhase = temp[3];
            }

            if (outputBuffer.getNumChannels() > 0)
            {
                for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
                    outputBuffer.addSample(ch, i, outputSample);
            }
        }
    }

    void renderNextBlock(juce::AudioBuffer<double>& outputBuffer, int startSample, int numSamples) override
    {
        juce::AudioBuffer<float> tempBuffer(outputBuffer.getNumChannels(), numSamples);
        tempBuffer.clear();
#if 0
        renderNextBlock(tempBuffer, midiMessages, 0, numSamples);
#endif
        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
        {
            auto* dest = outputBuffer.getWritePointer(ch, startSample);
            auto* src = tempBuffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                dest[i] = static_cast<double>(src[i]);
        }
    }

    void setSampleRate(float newSampleRate) { filter.sampleRate = newSampleRate; }

private:
    float& attackTime;
    float& decayTime;
    float& filterResonance;
    float& filterCutoff;
};

//==============================================================================
SimdSynthPluginProcessor::SimdSynthPluginProcessor()
    : AudioProcessor(BusesProperties()
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    parameters(*this, nullptr, juce::Identifier("SimdSynth"), // Initialize parameters
                 {
                     std::make_unique<juce::AudioParameterFloat>("attack", "Attack", juce::NormalisableRange<float>(0.01f, 2.0f, 0.01f), 0.1f),
                     std::make_unique<juce::AudioParameterFloat>("decay", "Decay", juce::NormalisableRange<float>(0.1f, 4.0f, 0.01f), 1.9f),
                     std::make_unique<juce::AudioParameterFloat>("resonance", "Resonance", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f),
                     std::make_unique<juce::AudioParameterFloat>("cutoff", "Cutoff", juce::NormalisableRange<float>(50.0f, 5000.0f, 1.0f), 1000.0f),
                     std::make_unique<juce::AudioParameterBool>("demoMode", "Demo Mode", false)
                 })
{

    // Initialize parameter pointers
    attackParam = parameters.getParameter("attack")->getValue();
    decayParam = parameters.getParameter("decay")->getValue();
    resonanceParam = parameters.getParameter("resonance")->getValue();
    cutoffParam = parameters.getParameter("cutoff")->getValue();
    demoModeParam = parameters.getParameter("demoMode")->getValue();

    // Initialize synthesizer
    synth.addSound(new SimdSynthSound());
    for (int i = 0; i < MAX_VOICE_POLYPHONY; ++i)
        synth.addVoice(new SimdSynthVoice(attackParam, decayParam, resonanceParam, cutoffParam));

    // Initialize chords (from main())
    initializeChords();
}

SimdSynthPluginProcessor::~SimdSynthPluginProcessor()
{
}

//==============================================================================
void SimdSynthPluginProcessor::initializeChords()
{
    chords.emplace_back(Chord{{midiToFreq(49), midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63)}, 0.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(54), midiToFreq(58), midiToFreq(61), midiToFreq(65)}, 2.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68)}, 4.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67)}, 6.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67)}, 8.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(51), midiToFreq(55), midiToFreq(58), midiToFreq(62), midiToFreq(65)}, 10.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(60), midiToFreq(63), midiToFreq(67), midiToFreq(70)}, 12.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(54), midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68)}, 14.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(61), midiToFreq(65), midiToFreq(68), midiToFreq(72)}, 16.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(58), midiToFreq(61), midiToFreq(65), midiToFreq(68), midiToFreq(72)}, 18.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(53), midiToFreq(56), midiToFreq(60), midiToFreq(63)}, 20.0f, 2.0f});
    chords.emplace_back(Chord{{midiToFreq(56), midiToFreq(60), midiToFreq(63), midiToFreq(67), midiToFreq(70)}, 22.0f, 2.0f});
}

void SimdSynthPluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SimdSynthVoice*>(synth.getVoice(i)))
            voice->setSampleRate(static_cast<float>(sampleRate));
}

void SimdSynthPluginProcessor::releaseResources()
{
}

bool SimdSynthPluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SimdSynthPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    if (demoModeParam)
    {
        // Demo mode: play chords
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* voice = dynamic_cast<SimdSynthVoice*>(synth.getVoice(i)))
            {
                float t = demoTime + (buffer.getNumSamples() / synth.getSampleRate());
                if (demoIndex < chords.size() && t >= chords[demoIndex].startTime + chords[demoIndex].duration)
                {
                    demoIndex++;
                    if (demoIndex < chords.size())
                    {
                        for (size_t v = 0; v < MAX_VOICE_POLYPHONY; ++v)
                        {
                            voice->voices[v].active = v < chords[demoIndex].frequencies.size();
                            if (voice->voices[v].active)
                            {
                                voice->voices[v].frequency = chords[demoIndex].frequencies[v];
                                voice->voices[v].phaseIncrement = (2.0f * juce::MathConstants<float>::pi * voice->voices[v].frequency) / voice->filter.sampleRate;
                                voice->voices[v].phase = 0.0f;
                                voice->voices[v].lfoPhase = 0.0f;
                                voice->voices[v].fegAttack = randomize(0.1f, 0.2f);
                                voice->voices[v].fegDecay = randomize(1.0f, 0.2f);
                                voice->voices[v].fegSustain = randomize(0.5f, 0.2f);
                                voice->voices[v].fegRelease = randomize(0.2f, 0.2f);
                                voice->voices[v].noteStartTime = t;
                            }
                        }
                    }
                }
            }
        }
        demoTime += (buffer.getNumSamples() / synth.getSampleRate());
    }

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

void SimdSynthPluginProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::AudioBuffer<float> tempBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    tempBuffer.clear();
    processBlock(tempBuffer, midiMessages);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* dest = buffer.getWritePointer(ch);
        auto* src = tempBuffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            dest[i] = static_cast<double>(src[i]);
    }
}

juce::AudioProcessorEditor* SimdSynthPluginProcessor::createEditor()
{
    return new SimdSynthEditor(*this);
}

void SimdSynthPluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SimdSynthPluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}


//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimdSynthPluginProcessor();
}
