# openEDMFilter v1.9.1

A dual filter effect for shaping transitions, resonant builds, and moving sounds. Combine two filters, tempo-synced sweeps, and modulation.

[Install with OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest) · [Download v1.9.1](https://github.com/alanmarcero/openEDMFilter/releases/tag/v1.9.1)

![openEDMFilter editor](assets/native-ui.png)

*Native editor capture. [See capture details](docs/SCREENSHOTS.md).*

## Features

- **Two filters:** Route Filter A and Filter B in series or parallel. Choose low-pass, high-pass, band-pass, or notch. Set slope from 12 to 96 dB per octave, resonance, and width. A/B Blend sets the parallel balance. Each filter has its own bypass.
- **LFO and envelope:** Each filter has an LFO that moves its cutoff, an envelope follower that responds to input, and cutoff FM. LFOs offer sine, triangle, rising saw, falling saw, and square. Rates run from 1/32 note to 8 bars with triplets. Set bipolar depth. Choose the envelope source frequency, attack, release, and bipolar depth.
- **Cutoff FM:** Choose sine, triangle, rising saw, falling saw, or square. Set the rate up to audio rate. Choose Filter A, Filter B, or both filters as the destination.
- **Steps and MIDI:** Each filter has Off, Steps, or Euclidean mode. Steps mode has 16 bipolar values. Euclidean mode spaces pulses evenly. Set length, rotation, step rate from 1/16 note to 4 beats, pulse count, and bipolar depth. MIDI modulation can use velocity, aftertouch, or a selected CC.
- **Stereo modes:** Choose Linked, Left/Right, or Mid/Side processing. Set separate modulation depth, phase offset, and cutoff offset for the right or side lane.
- **Auto Sweep:** Save four independent slots, A through D. Set each slot to 1–64 bars and choose Filter A, Filter B, both filters, or mirrored filters. Modes include Bar Window, Every Bar, Up & Down, and Bar + Macro.
- **Sweep curves:** Shape cutoff and resonance with the graph or Start, End, and Slope controls. Set Start, End, Slope, and Onset for the bitcrush and glitch effects. Choose when each effect enters the sweep. Sweep bypass turns off automatic bitcrush and glitch sweeps. Manual Crush remains available.
- **Other effects:** Add Drive, Crush, and ring modulation before or after the filters. Set the ring modulator frequency and mix. Link Crush rate to Filter A modulation. Set input gain from −24 to +24 dB, dry/wet mix, output gain, and a stereo-linked peak limiter with an adjustable ceiling.
- **Meters and display:** View input and output peaks, filter response curves, and a live spectrum. Freeze or hide the spectrum. Drag the response marker to adjust cutoff and resonance.
- **Presets and sessions:** Choose from 58 factory presets and 12 sweep-curve presets. Controls support host automation and session recall.

HOST SYNC follows host tempo and time signature. The internal clock supports 40–240 BPM. Select 2/4, 3/4, 4/4, 5/4, 6/4, 6/8, 7/8, 9/8, or 12/8. The standalone app accepts MIDI and uses the internal clock. The plugin reports 110 samples of latency.

## Install and compatibility

The v1.9.1 release includes VST3, AU, LV2, and standalone packages for macOS, Windows, and Linux. The macOS package supports Apple Silicon and Intel. The Windows and Linux packages are x64.

| Platform | Formats |
| --- | --- |
| macOS | VST3, AU, LV2, Standalone |
| Windows, x64 | VST3, LV2, Standalone |
| Linux, x64 | VST3, LV2, Standalone |

OpenVST Manager installs VST3 and AU. Install LV2 and the standalone app from the [release packages](https://github.com/alanmarcero/openEDMFilter/releases/tag/v1.9.1). Rescan your host after installation. Some hosts may ask you to select the plugin again in older AU sessions. The macOS AU now accepts MIDI.

## License

openEDMFilter uses the MIT license. See [LICENSE](LICENSE).
