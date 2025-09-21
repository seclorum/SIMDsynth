#include <JuceHeader.h>
#include "PluginProcessor.h"

juce::AudioProcessor* createPluginFilter() {
    return new SimdSynthAudioProcessor();
}