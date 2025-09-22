# SIMDsynth

This is a SIMD-accelerated audio synthesizer written in C++ with the JUCE framework, using vectorized math to simulate polyphonic audio synthesis with filters, envelopes, LFOs, and sine and saw wavetable oscillators. 

It's optimized for both x86-64 (SSE) and ARM64 (NEON) platforms, in order to highlight the SIMD techniques in a JUCE context.

Some rudimentary factory presets are included in the plugin - they will be created automatically on first execution of the plugin.

## High-Level Overview

- Supports up to 8 polyphonic voices (notes played at once).
- Uses SIMD to generate and filter 4 voices in parallel (2 groups of 4).
- Applies:
  - Sine and Saw Wavetable Oscillators with phase and sub-phase parameters
  - Amplitude envelopes (ADSR): Attack-Decay-Sustain-Release
  - Filter envelopes (ADSR): Attack-Decay-Sustain-Release
  - LFO (Low Frequency Oscillator): Used to modulate the pitch
  - 4-pole ladder filters per voice group
  - Sub-oscillator per voice, with tune, mix and track parameters
- Uses SSE or NEON depending on platform (x86_64 or ARM64).
- Includes a basic set of Factory Presets for testing purposes.


(c) 2025, seclorum
