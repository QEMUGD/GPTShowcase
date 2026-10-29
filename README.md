# GPT Showcase Macro — Geode source prototype

Target intent: Geometry Dash 2.2081, Geode SDK 5.8.2, Windows + Android only (iOS intentionally excluded).

## Important status

This is a **source prototype**, not a finished or compiled `.geode` release. It contains:
- Geode project metadata and CMake setup;
- a pause-menu button and macro popup UI prototype;
- a simple `.gpt` v1 text format;
- safe `.gpt` save/load parsing helpers.

The UI buttons for recording, stopping, continuing, and enable/disable are currently placeholders. The actual game input hooks, playback engine, and TPS/frame-timing integration are **not implemented yet**. Do not treat this as a working macro bot until those parts are implemented and tested on each platform.

## `.gpt` format v1

Example:

```text
GPT 1
# time_seconds button pressed(0|1)
0.000 1 1
0.125 1 0
0.300 2 1
0.425 2 0
```

- `time_seconds`: seconds from recording start
- `button`: `1` primary input, `2` secondary input
- `pressed`: `1` press, `0` release

This is a custom format for this project, not an established Geometry Dash macro standard.

## Build

1. Install the Geode SDK compatible with GD 2.2081 and the required platform toolchain.
2. Set `GEODE_SDK` to the SDK directory.
3. Configure/build with CMake using the Geode SDK's recommended workflow.

```sh
cmake -S . -B build
cmake --build build --config Release
```

Build separate Windows and Android binaries using the matching Geode SDK/toolchains. A Windows build does not produce an Android build.

## Next implementation tasks

1. Connect real press/release recording hooks and a monotonic timer.
2. Make Continue append new events with timestamps offset from the existing duration.
3. Implement safe playback with stop-on-death/restart and clean input release.
4. Add a file picker/list for loading `.gpt` files.
5. Add a configurable TPS/frame-step layer using supported Geode/game hooks; test each platform separately.
6. Test pause-menu placement and touch interaction on mobile.

Use macro tools only for private practice/showcases; do not submit bot-assisted runs as legitimate records.
