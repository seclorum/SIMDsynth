#include "SimdSynthProcessor.h"

SimdSynthProcessor::SimdSynthProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("SimdSynth"), {})
{
}

SimdSynthProcessor::~SimdSynthProcessor() {}

void SimdSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    synth.prepareToPlay(sampleRate, samplesPerBlock);
}

void SimdSynthProcessor::releaseResources() {
    synth.releaseResources();
}

bool SimdSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void SimdSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    synth.processBlock(buffer, midiMessages);
}

juce::AudioProcessorEditor* SimdSynthProcessor::createEditor() {
    return new juce::GenericAudioProcessorEditor(*this);
}

bool SimdSynthProcessor::hasEditor() const { return true; }

const juce::String SimdSynthProcessor::getName() const { return "SimdSynth"; }

bool SimdSynthProcessor::acceptsMidi() const { return true; }
bool SimdSynthProcessor::producesMidi() const { return false; }
bool SimdSynthProcessor::isMidiEffect() const { return false; }
double SimdSynthProcessor::getTailLengthSeconds() const { return 0.0; }

int SimdSynthProcessor::getNumPrograms() { return 1; }
int SimdSynthProcessor::getCurrentProgram() { return 0; }
void SimdSynthProcessor::setCurrentProgram(int) {}
const juce::String SimdSynthProcessor::getProgramName(int) { return {}; }
void SimdSynthProcessor::changeProgramName(int, const juce::String&) {}

void SimdSynthProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SimdSynthProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SimdSynthProcessor();
}
