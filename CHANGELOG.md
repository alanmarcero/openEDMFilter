# Changelog

## v1.14.0

Core DSP engine upgrades across filters, nonlinearities, routing, and dynamics:

- **4-pole ZDF ladder filter:** Musical self-oscillation at full resonance across LP, BP, and HP modes.
- **Per-sample log-frequency SVF smoothing:** Prevents high-Q modulation spikes while preserving zero-delay audio-rate cutoff FM.
- **Pole-staggered Butterworth Q distribution:** Preserves exact -3 dB cutoff and bandwidth across all filter slopes (12–96 dB/oct).
- **ADAA saturator:** First-order antiderivative antialiasing saturation to suppress folded upper harmonics.
- **Pre-filtered bit crusher:** Anti-aliased bit/sample-rate crushing to suppress above-Nyquist foldover.
- **Hilbert single-sideband ring modulator:** Quadrature Hilbert SSB frequency shifting with ultrasonic foldover suppression.
- **Oversampled limiter with true-peak guard:** Peak limiting in the oversampled domain with inter-sample true-peak protection under the ceiling.
- **LR4 sub preserve crossover:** Linkwitz-Riley 4th-order crossover for phase-coherent sub-bass preservation without cancellation notches.
- **Equal-power routing & topology morphs:** State-warmed equal-power crossfades for filter topology changes and phase-safe serial/parallel routing morphs.
- **Matched sidechain envelope timing:** Sidechain envelope follower attack and release timing aligned with the main input envelope.
