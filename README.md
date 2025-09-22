# SIMDsynth

This is a SIMD-accelerated audio synthesizer written in C++, using vectorized math to simulate polyphonic audio synthesis with filters, envelopes, LFOs, and sine and saw wavetable oscillators. 

It's optimized for both x86-64 (SSE) and ARM64 (NEON) platforms.

A basic "C" version of the synth is implemented in `simdsynth.cpp`, which can be tested during development with the `make test` target of the Makefile (requires `sox` to play samples from piped output). 

This project includes a more sophisticated JUCE-based plugin wrapper to highlight the SIMD techniques in a JUCE context - however development is first done on the simdsynth.cpp command-line/standalone version of the synth, for the sake of simplicity - the JUCE plugin will sometimes lag behind the standalone version, as features get implemented more rapidly in the cli version, then ported into the SimdSynthJUCE Plugin project.

Some rudimentary factory presets are included in the SimdSynthJUCE Plugin module - they will be created automatically on first execution of the plugin.

To listen to a demonstration of the current capabilities, download the demo.mp3 file:

<audio controls>
	<source src="https://raw.githubusercontent.com/seclorum/SIMDsynth/main/demo_sine.mp3" type="audio/mpeg">
	<a href="https://raw.githubusercontent.com/seclorum/SIMDsynth/main/demo_sine.mp3">Sine wavetable demo.</a>
	<source src="https://raw.githubusercontent.com/seclorum/SIMDsynth/main/demo_saw.mp3" type="audio/mpeg">
	<a href="https://raw.githubusercontent.com/seclorum/SIMDsynth/main/demo_saw.mp3">Sawtooth wavetable demo.</a>
</audio>


## High-Level Overview

- Supports up to 8 polyphonic voices (notes played at once).
- Uses SIMD to generate and filter 4 voices in parallel (2 groups of 4).
- Applies:
  - Sine and Saw Wavetable Oscillators
  - Amplitude envelopes (AD): Attack-Decay
  - Filter envelopes (ADSR): Attack-Decay-Sustain-Release
  - LFO (Low Frequency Oscillator): Used to modulate the pitch
  - 4-pole ladder filters per voice group
  - Sub-oscillator per voice
- Plays a sequence of Debussy-style chords over 24 seconds.
- Uses SSE or NEON depending on platform (x86_64 or ARM64).
- Includes a basic set of Factory Presets for testing purposes.


(c) 2025, seclorum
