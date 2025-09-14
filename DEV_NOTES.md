# Noob_Tools — Dev Notes (session recall)

Purpose: Running log of decisions, setup, features, and next steps so anyone can quickly recall the project state.

## Repo + Build
- Name: Noob_Tools (renamed from SeratoLikeSampler)
- Targets: `Noob_Tools_Standalone`, `Noob_Tools_VST3`
- Outputs: `build/x64/Release/Noob_Tools.exe`, `build/x64/Release/VST3/Noob_Tools.vst3`
- CI: GitHub Actions (Ubuntu CMake build) — `.github/workflows/ci.yml`
- README: Root quick-start with CMake commands
- Git LFS: Audio patterns tracked in `.gitattributes` (install LFS to enable)

Build (VS 2022)
- Configure: Open `SeratoLikeSampler` in VS (CMake project)
- Startup: Set `Noob_Tools_Standalone` as startup item; F5 to run
- Options: `USE_SIGNALSMITH=ON` (default), `USE_RUBBERBAND=OFF`, `USE_AUBIO=OFF`
- Manual CMake: `cmake -S SeratoLikeSampler -B build -G "Visual Studio 17 2022" -A x64 -D JUCE_FETCH_VST3_SDK=ON`

## Audio Engine + UI Status
- Slicing: Spectral-flux auto-slicing; manual “Tap Slice” remains
- Edit Mode: Toggle to reveal slice list and enable real-time chop creation
  - Create slice: Click a pad or press mapped key while preview plays
  - Quantize toggle: Snap new slice to nearest transient
  - User-mapped slices are stored per MIDI note and take priority on playback
- Per-slice controls: Pitch (semitones), Time (ratio), Reverse, Gain
  - UI in `SliceListComponent`
  - Reverse supported; stretch currently forward-only (reverse stretch TBD)
- Time/Pitch:
  - High-quality path: SignalSmith stretch via FetchContent (when `USE_SIGNALSMITH=ON`)
  - Fallback: Linear resampling (always available)
- Global controls: Attack/Release, Filter (SVF), Gain; Choke, Gate, Loop Preview, Zoom

## CMake Options (SeratoLikeSampler/CMakeLists.txt)
- `USE_SIGNALSMITH` (default ON): Fetch `signalsmith-stretch` and use it
- `USE_RUBBERBAND` (OFF): Link if available via package manager
- `USE_AUBIO` (OFF): Link if available (for onset/tempo/key later)

## Keyboard + Pads
- Keyboard map (16 pads): `1 2 3 4 5 6 7 8 9 0 q w e r t y`
- Edit Mode: Key press/pad click assigns a new slice to that pad’s MIDI note
- Play Mode: Pads/keys trigger playback (MIDI note-on internally)

## Next Steps (proposed roadmap)
- BPM + beat grid: Onset strength → tempo + beat tracking; show grid; quantize to beats
- Key detection: Chroma/HPCP + key profiles; display detected key
- “Find Samples”: Score slices by rhythm/tonality; suggest best 8–16
- Per-slice filter: Move SVF per slice (cutoff/reso) and UI controls
- Keyboard Mode: Play selected slice chromatically across keys
- MIDI Learn: Map external pads and CCs to pads/params; save mappings
- Reverse stretch: Feed reversed audio through SignalSmith path
- Performance: Zero allocs in `processBlock`, lock-free queues, background analysis threads
- Stems (optional): External/offline (Demucs/Spleeter) workflow with cache

## Session Recall Tips (“dev hacks”)
- Keep this `DEV_NOTES.md` updated — decisions, flags, TODOs
- Use small, descriptive commits; tag milestones (e.g., `v0.1-edit-mode`)
- Open short GitHub issues for each feature/bug with checklists
- Pin configs with CMake presets or VS `CMakeSettings.json`
- For experiments, create branches named `exp/<topic>` and PR to main

## Recent Milestones
- Repo init and rename → Noob_Tools; .gitignore/.gitattributes; CI added
- CMake: plugin target renamed; SignalSmith via FetchContent; feature flags added
- Time/pitch: linear fallback implemented; SignalSmith path integrated behind flag
- Per-slice UI: Pitch/Time/Reverse/Gain in SliceListComponent
- Edit Mode + Quantize: Real-time slice assignment to pad notes

---
Last updated: 2025-09-14
