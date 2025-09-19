# SIMDsynth

This is a SIMD-accelerated audio synthesizer written in C++, using vectorized math to simulate polyphonic audio synthesis with filters, envelopes, LFOs, and sine wave oscillators. It's optimized for both x86-64 (SSE) and ARM64 (NEON) platforms.

A basic "C" version of the synth is implemented in `simdsynth.cpp`, which can be tested during development with the `make test` target of the Makefile (requires `sox` to play samples from piped output). 

This project will include a more sophisticated JUCE-based plugin wrapper to highlight the SIMD techniques in a JUCE context - however development is first done on the simdsynth.cpp command-line/standalone version of the synth, for the sake of simplicity - the JUCE plugin will be brought up to date once a few more features have been implemented in the standalone code.

To listen to a demonstration of the current capabilities, download the demo.mp3 file:

<audio controls>
	<source src="https://raw.githubusercontent.com/seclorum/SIMDsynth/main/demo.mp3" type="audio/mpeg">
	<a href="https://raw.githubusercontent.com/seclorum/SIMDsynth/main/demo.mp3">Download the demo.mp3 to play locally.</a>
</audio>


## High-Level Overview

- Supports up to 8 polyphonic voices (notes played at once).
- Uses SIMD to generate and filter 4 voices in parallel (2 groups of 4).
- Applies:
  - Amplitude envelopes (AD): Attack-Decay
  - Filter envelopes (ADSR): Attack-Decay-Sustain-Release
  - LFO (Low Frequency Oscillator): Used to modulate the pitch
  - 4-pole ladder filters per voice group
  - Sub-oscillator per voice
- Plays a sequence of Debussy-style chords over 24 seconds.
- Uses SSE or NEON depending on platform (x86_64 or ARM64).


(c) 2025, seclorum
