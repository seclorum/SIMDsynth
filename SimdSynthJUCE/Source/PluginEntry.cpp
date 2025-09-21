#include "PluginProcessor.h"

extern "C" JUCE_EXPORT juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SimdSynthAudioProcessor();
}
