

SIMDsynth


This is an example project to highlight various SIMD techniques that can be used for producing multi-timbral polyphone audio synthesis.

An attempt is made to use SIMD instructions on both ARM and X86.

A basic "C" version of the synth is implemeneted in simdsynth.cpp, and as well a more sophisticated JUCE-based plugin wrapper is implemented to highlight the SIMD techniques in a JUCE context.

The 8-voice synth includes basic oscillator, envelopes, filter and LFO capabilities.

