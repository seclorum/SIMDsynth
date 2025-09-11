
SIMDsynth

This is an example project to highlight various SIMD techniques that can be used for producing multi-timbral/polyphonic audio synthesis.

An attempt is made to use SIMD instructions on both ARM and X86.

A basic "C" version of the synth is implemeneted in simdsynth.cpp, which can be tested with the "make test" target of the Makefile (requires sox to play samples from piped output), and as well a more sophisticated JUCE-based plugin wrapper is implemented to highlight the SIMD techniques in a JUCE context.

High-Level Overview

	Supports up to 8 polyphonic voices (notes played at once).
	Uses SIMD to generate and filter 4 voices in parallel (2 groups of 4).

		Applies:

			Amplitude envelopes (AD): Attack-Decay
			Filter envelopes (ADSR): Attack-Decay-Sustain-Release
			LFO (Low Frequency Oscillator): Used to modulate the pitch
			4-pole ladder filters per voice group.

	Plays a sequence of Debussy-style chords over 24 seconds.
	Uses SSE or NEON depending on platform (x86_64 or ARM64).

(c) 2025, seclorum
