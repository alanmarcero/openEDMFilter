# openEDMFilter

[Install with OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest)

A dual multimode filter plugin for EDM production. It provides two serial filters, tempo and envelope modulation, distortion and output limiting. Release packages contain VST3, LV2 and a standalone app on macOS, Windows and Linux; macOS also includes AU.

![openEDMFilter native editor](assets/native-ui.png)

*Captured from the released v1.1.0 VST3 editor. [Capture details and provenance](docs/SCREENSHOTS.md).*

## Features

- Two filters in series (A into B). Each one has low-pass, high-pass, band-pass and notch modes, 12, 24, 48 or 96 dB/oct slopes, resonance, and resonance width. Width also sets the band for band-pass and notch.
- Full bypass per filter. The BYPASS button crossfades over 10 ms so it does not click. A fully bypassed filter does no processing, and its curve leaves the display.
- Tempo-synced LFO per filter. Five shapes (sine, triangle, saw up, saw down, square), rates from 1/32 to 8 bars with triplets, and bipolar depth. While the host plays, the phase locks to the host transport, so the LFO lands on the same point of the bar on every pass.
- Envelope follower per filter. It has a source frequency (it listens to a band of the input), attack, release, and bipolar depth.
- Cutoff FM. A sine oscillator modulates both cutoffs, up to audio rate.
- Drive. tanh saturation, before or after the filters.
- Crush. A sample-rate reducer and bit crusher, before or after the filters. LINK A makes the crush rate follow filter A's cutoff modulation.
- Ring modulator with frequency and mix controls.
- Output section. Dry/wet, gain, and a stereo-linked lookahead peak limiter with a ceiling.
- Filter inertia. This smooths cutoff moves on 48 and 96 dB slopes to stop clicks. Turn it off for faster, harder gating.
- 18 factory presets, available from the preset menu and as host programs.
- Live response display. It shows both curves and the combined curve, and follows the LFO, envelope and FM in real time. Drag a band's marker to set its cutoff (left and right) and resonance (up and down).
- 2x oversampling. Everything before the limiter runs at twice the host rate, so drive, ring mod and fast FM alias less. The plugin reports 110 samples of latency (47 for the oversampler, 63 for the limiter lookahead), so hosts compensate for it.
- All parameters can be automated in the host.

VST2 and AAX are not built by this project.

## Install

1. Download and install [OpenVST Manager](https://github.com/alanmarcero/alan-m-OpenVSTManager/releases/latest) for macOS or Windows.
2. Open the manager and refresh the catalog.
3. Choose **openEDMFilter**, select **VST3** (or **AU** on macOS), and install.
4. Rescan plugins in your DAW. If you chose a custom install folder, add it to the DAW's search paths.

For manual installation, download your platform ZIP from [v1.1.0 Releases](https://github.com/alanmarcero/openEDMFilter/releases/tag/v1.1.0). The macOS package is universal (Apple Silicon/Intel), with a macOS 11 deployment target. Windows and Linux packages are x64.

| Format | macOS | Windows | Linux |
| --- | --- | --- | --- |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` | `C:\Program Files\Common Files\VST3\` | `~/.vst3/` |
| AU | `~/Library/Audio/Plug-Ins/Components/` | — | — |
| LV2 | `~/Library/Audio/Plug-Ins/LV2/` | Your host's LV2 search folder | `~/.lv2/` |

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

`.github/workflows/build.yml` builds and tests on macOS (universal binary), Windows and Linux on code pushes to `main` and pull requests. It runs pluginval at strictness 5 on macOS and Windows, then uploads a zip for each OS. Push a `v*` tag to publish those zips as a GitHub release.

The macOS builds have an ad hoc signature only. On first use, run `xattr -dr com.apple.quarantine <plugin>` or allow the plugin in System Settings.

## Layout

- `Source/Core`: DSP with no JUCE dependency (filters, modulators, effects, and the engine that chains them). The unit tests cover this code.
- `Source/Plugin`: the JUCE processor, parameters, presets, and editor.
- `Tests`: Catch2 tests.
