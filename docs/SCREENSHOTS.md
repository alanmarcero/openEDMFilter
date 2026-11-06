# Native UI screenshot

![Native plugin editor](../assets/native-ui.png)

`assets/native-ui.png` shows the v1.4.0 native JUCE editor at 1100 × 940, captured from the release build on 2026-11-06. It displays the **Neon Staircase** curves in **Auto Mode D** with the **BARS** selector set to **1 Bar**, and the playhead at 90% of that one-bar window. This is a component render of the actual editor, not a mockup or a DAW screenshot.

Reproduce on macOS from the repository root:

```sh
cmake --build build --target openedm_plugin_tests --parallel
OEF_CAPTURE="$PWD/assets/native-ui.png" build/openedm_plugin_tests '[ui]'
```

The capture test also exercises A/B/C/D button selection, host-driven selection, editing the selected slot, and changing slots during a knob drag without corrupting automation gestures.
