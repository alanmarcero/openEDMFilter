# openEDMFilter

[Install with OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest)

Dual multimode filter. Serial or parallel filters, MIDI and step modulation, four auto-sweep slots (cutoff, resonance, bitcrush, glitch), a per-filter LFO and envelope follower, drive, and a lookahead limiter. Release packages contain VST3, LV2 and a standalone app on macOS, Windows and Linux; macOS also includes AU.

![openEDMFilter native editor](assets/native-ui.png)

*Captured from the native editor. [Capture details and provenance](docs/SCREENSHOTS.md).*

## Features

- Two filters in series (A into B) or parallel. Parallel A/B Blend selects A at 0%, B at 100%, or a blend between them; both receive the same input. Routing and blend changes are smoothed. Each filter has low-pass, high-pass, band-pass and notch, slopes of 12, 24, 48 or 96 dB/oct, resonance, and resonance width. Width also sets the band for band-pass and notch.
- BYPASS on a filter crossfades over 10 ms. A bypassed filter does no processing, and its curve leaves the display.
- HOST SYNC follows the host transport. Off, the internal clock runs from 40 to 240 BPM. Time signatures: 4/4, 3/4, 2/4, 5/4, 6/4, 6/8, 7/8, 9/8, 12/8. The 4/4 entry uses the host signature when the host sends one.
- Auto-sweep lengths are 1, 2, 4, 8, 16, 32 and 64 bars. The BARS menu sets the length for the selected slot. Modes are Bar Window, Every Bar, Up & Down, and Bar + Macro. Targets are Filter A, Filter B, Both A+B, and A/B Mirror. The default length is 8 bars.
- Slots A, B, C and D each store length, target, mode and curves. The panel buttons and the Auto Mode parameter select the slot. Slot A keeps the original parameter IDs.
- Each slot has Start, End, Slope and Onset for the bitcrush mix and the glitch mix. The graph draws cutoff, resonance, bitcrush and glitch. Glitch repeats the first sixteenth-note slice of each beat, with short fades at the slice edges. Very slow tempos use a shorter synced subdivision so the slice fits the history buffer. Sweep bypass removes the automatic effects and leaves the manual crusher.
- Drag the start, midpoint and end handles on the graph, or use Cutoff and Resonance Start, End and Slope. Slope above 0 bends the curve up in octaves (exponential); slope below 0 bends it the other way (logarithmic); 0 is linear. Double-click a midpoint to reset that slope to 0.
- The preset menu and the slope preset menu in the Auto Sweep panel include low-pass, high-pass, band-pass and notch sweeps, plus 32-bar and 64-bar builds.
- Each filter has an LFO: sine, triangle, saw up, saw down, square. Rates run from 1/32 to 8 bars, with triplets. Depth is bipolar. While the host is playing, the phase locks to the transport.
- Each filter has an envelope follower with source frequency, attack, release and bipolar depth. It listens to a band of the input around the source frequency.
- Cutoff FM uses sine, triangle, saw up, saw down or square, up to audio rate. FM Destination selects Both, Filter A or Filter B. Existing sessions retain sine FM on both cutoffs.
- Drive is tanh saturation, before or after the filters.
- Crush is a sample-rate reducer and bit crusher, before or after the filters. LINK A makes the crush rate follow filter A's cutoff modulation.
- The ring modulator has frequency and mix.
- Input Gain trims the signal by -24 to +24 dB before the effects; changes are smoothed and the trim is ignored during master bypass.
- Input and output peak meters show post-trim input and final output level with a 350 ms release. They turn red above 0 dBFS.
- Output has dry/wet, gain, and a stereo-linked lookahead peak limiter with a ceiling.
- Filter inertia uses a 30 ms smoother on cutoff moves for 48 and 96 dB slopes. Off, those slopes use a 2 ms smoother.
- There are 58 factory presets in the preset menu and as host programs, and 12 slope presets in the Auto Sweep panel.
- The response display shows each filter curve and the combined curve, including the auto-sweep, LFO, envelope and FM. Drag a band marker horizontally for cutoff and vertically for resonance. A live 128-bin log spectrum shows the louder channel in each frequency bin after input trim and before filtering; SPECTRUM hides it and FREEZE holds the trace.
- The signal before the limiter runs at twice the host sample rate. Reported latency is 110 samples: 47 from the oversampler and 63 from the limiter lookahead.
- Every parameter can be automated.

VST2 and AAX are not built by this project.

## Modulation and routing controls

Click **MOD / ROUTING** in the native editor to open the scrollable native parameter controls, including all routing and modulation settings. Click **CLOSE CONTROLS** to return to the main panels. All controls support host automation and session recall. Factory presets reset the new controls to their defaults; older sessions retain serial routing and linked stereo, with MIDI and steps off.

Each filter has MIDI Source (Off, Velocity, Aftertouch, CC), a CC number (0–127), and bipolar MIDI Depth. All MIDI channels are accepted. Velocity uses the highest velocity among held notes; releasing a note removes it. Aftertouch uses the highest channel or polyphonic pressure. CC uses the latest value of the selected controller. All Notes Off and All Sound Off clear notes and pressure on their MIDI channel; Reset All Controllers clears controller and pressure values only on its MIDI channel. A selected CC then falls back to the most recent value from another channel, or zero if none remains. MIDI events take effect at their sample offsets. Route MIDI to this audio effect in your host; the standalone app also accepts MIDI input.

Step Mode selects Off, Steps or Euclidean. Each filter has 16 editable bipolar step values, length 1–16, rotation 0–15, beats per step (1/16 to 4 quarter-note beats), and bipolar Step Depth. Euclidean mode distributes Pulses evenly over Length; extra pulses clamp to Length. Pulses output +1 and rests output −1. Rotation moves the first step forward. Both modes follow host tempo and PPQ, including negative positions and seeks, and hold on a stopped host playhead. With no playhead, they follow the internal clock. Step Depth 0 is neutral.

Modulation Channels selects Linked (the historical default), Left/Right or Mid/Side. The existing depths control L or M. Each band's R/S Mod Depth scales its LFO, envelope, MIDI and step modulation independently (including inversion), R/S Phase Offset shifts its LFO and step pattern, and R/S Cutoff Offset moves its cutoff by up to six octaves. Envelope followers listen to their own L/R or M/S lane in independent modes. Linked ignores the R/S settings. M/S uses an energy-preserving matrix around the filters; drive, crusher, ring modulation and output remain in L/R. Mono uses the first lane. Stereo mode changes use a smoothed matrix, and cutoff changes retain the existing smoothing. The response graph shows the L/M lane and the phase-aware serial or parallel combined response.

## Auto mode

Turn on AUTO SWEEP, select A, B, C or D, and edit that slot. BARS sets 1 Bar, 2 Bars, 4 Bars, 8 Bars, 16 Bars, 32 Bars or 64 Bars for the selected slot. Auto Mode and Sweep Bars can be automated.

With HOST SYNC on, playback locks the sweep to the current tempo and time signature. Phase steps per sample from the block-start playhead. A stopped host holds the sweep on that playhead. HOST SYNC off, or a host that sends no playhead, uses the internal clock. One bar is the quarter-note length of the active signature (four quarters in 4/4). The 4/4 menu entry follows the host signature when the host sends one. A session saved before the 1-bar and 2-bar choices keeps its 4, 8, 16, 32 or 64 bar length. The default is 8 bars.

Cutoff, resonance and effect-mix changes are smoothed. The session stores all four slots. An older session loads its sweep into A with the automatic bitcrush and glitch mixes at 0.

To bring an effect in late, set Start to 0%, Onset to 75% or later, then raise End and set Slope. Onset is a position in the sweep: Every Bar repeats it each bar, and Up & Down retraces it on the way back. Bitcrush still uses Crush Rate, Bits and Position; the sweep mix is applied on top of the manual mix. Glitch follows the host clock or the internal clock. The slope preset menu edits the selected slot only.

These presets write all four slots. Slots B, C and D raise the cutoff slope, resonance and effect-mix ends from the slot A values:

- Silk to Static (8 Bar): low-pass, 80 Hz to 18 kHz, 10-bit crush. Crush and glitch start at 0 and rise after 72% and 85% of the sweep.
- Neon Staircase (16 Bar): low-pass, 100 Hz to 19 kHz, 6-bit crush. Crush and glitch rise after 68% and 80%.
- Last Bar Breakdown (8 Bar): low-pass, 18 kHz down to 350 Hz, 5-bit crush. Both mixes rise after 87.5%.
- Mainstage Fracture (32 Bar): high-pass, 30 Hz to 4.8 kHz, 4-bit crush. Crush rises after 75%; glitch rises after 87.5% and ends at full mix on slot D.
- Trance Climax Static Burst (16 Bar): low-pass, 85 Hz to 19.5 kHz, 8-bit crush. Crush rises after 75%; glitch rises after 85%.

## Install

1. Download and install [OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest) for macOS or Windows.
2. Open the manager and refresh the catalog.
3. Choose openEDMFilter, select VST3 (or AU on macOS), and install.
4. Rescan plugins in your DAW. If you chose a custom install folder, add it to the DAW's search paths.

Install the current packages from the [v1.5.0 release](https://github.com/alanmarcero/openEDMFilter/releases/tag/v1.5.0): `openEDMFilter-macOS.zip`, `openEDMFilter-Windows.zip`, and `openEDMFilter-Linux.zip`. The macOS package is universal (Apple Silicon/Intel), with a macOS 11 deployment target. Windows and Linux packages are x64. v1.3.0 remains available at its original tag.

| Format | macOS | Windows | Linux |
| --- | --- | --- | --- |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` | `C:\Program Files\Common Files\VST3\` | `~/.vst3/` |
| AU | `~/Library/Audio/Plug-Ins/Components/` | n/a | n/a |
| LV2 | `~/Library/Audio/Plug-Ins/LV2/` | Your host's LV2 search folder | `~/.lv2/` |

The AU now registers as a MIDI-controlled effect (`aumf`, Music Effect) so compatible hosts can route MIDI modulation to it. Its bundle ID, manufacturer and subtype remain unchanged. Rescan Audio Units after upgrading. Hosts identify AUs by type as well as manufacturer/subtype, so sessions saved with the earlier `aufx` AU may need the plugin manually relinked or replaced and its saved settings reloaded. VST3, LV2 and standalone identities are unchanged.

Extract and copy the entire bundle to the appropriate folder, then rescan your host. OpenVST Manager installs VST3/AU; install LV2 and run the included standalone app manually. Standalone mode uses its own clock because there is no DAW transport.

## Build

You need CMake 3.22+, Git and a C++20 compiler (Xcode command-line tools on macOS, Visual Studio 2022 C++ tools on Windows, or GCC/Clang on Linux). CMake fetches JUCE 8.0.9 and Catch2 3.9.1. Linux also needs ALSA/JACK, FreeType/fontconfig, X11 and OpenGL development packages; the exact Ubuntu package list is in `.github/workflows/build.yml`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The plugins are written to `build/OpenEDMFilter_artefacts/Release/`. Add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0` for a universal macOS build, or `-A x64` with the Visual Studio generator on Windows.

## CI

`.github/workflows/build.yml` builds and tests on macOS 15 (universal arm64 and x86_64), Windows and Linux on code pushes to `main` and pull requests. It runs pluginval at strictness 5 on macOS and Windows, then uploads a zip for each OS. Push a `v*` tag to publish those zips as a GitHub release.

The macOS builds have an ad hoc signature only. On first use, run `xattr -dr com.apple.quarantine <plugin>` or allow the plugin in System Settings.

## Layout

- `Source/Core`: DSP with no JUCE dependency (filters, modulators, effects, and the engine that chains them). The unit tests cover this code.
- `Source/Plugin`: the JUCE processor, parameters, presets, and editor.
- `Tests`: Catch2 tests.
