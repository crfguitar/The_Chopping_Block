# Noob_Tools

A JUCE-based sampler plugin and standalone app. Drag-and-drop audio, auto-slice to pads, tweak with APVTS controls, and export slices.

## Quick Build (Windows, Visual Studio 2022)

```bat
:: From the repo root
cmake -S SeratoLikeSampler -B build -G "Visual Studio 17 2022" -A x64 -D JUCE_FETCH_VST3_SDK=ON
cmake --build build --config Release
```

Outputs
- Standalone: `build\x64\Release\Noob_Tools.exe`
- VST3: `build\x64\Release\VST3\Noob_Tools.vst3`

Notes
- JUCE: Fetched automatically via CMake’s FetchContent.
- ASIO (optional): Set `-DASIO_SDK_DIR="C:/SDKs/ASIOSDK"` if you have the SDK; otherwise, Windows uses default APIs.
- Git LFS: Audio patterns (`*.wav`, `*.aif`, `*.aiff`, `*.flac`, `*.mp3`) are tracked. Install LFS and run `git lfs install` to enable.

## Repo Layout
- `SeratoLikeSampler/`: CMake project root for the plugin/app sources
- `SeratoLikeSampler/Source/`: C++ sources
- `SeratoLikeSampler/Resources/`: assets bundled or copied next to builds

## Building in IDE
- Open the `build` folder as a CMake project in Visual Studio after configuring, or use the CMake “Open Folder” workflow in VS and select `SeratoLikeSampler`.

```text
Minimum CMake 3.22, C++17, JUCE 7.x
```
