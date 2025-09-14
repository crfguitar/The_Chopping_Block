
# Noob_Tools (JUCE CMake Starter)

- Drag/drop audio to load
- Spectral-flux auto-slice to 16 pads
- APVTS macros (attack, release, cutoff, reso, gain)
- StateVariableTPT low-pass filter (cutoff + resonance)
- Time-stretch stub

## Build

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -D JUCE_FETCH_VST3_SDK=ON
cmake --build build --config Release
```
