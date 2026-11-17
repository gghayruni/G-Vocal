# G-Vocal 0.1 — Hybrid Vocal Processor (VST3)

**Open-source prototype for Ableton Live 12 on Windows x64.** Independent audio effect written in C++ with JUCE, no external iZotope/Arturia plugin requirement. The interface is a native JUCE UI.

## Functions

- **8 real DSP stages**: CLEAN (high-pass + soft gate), SMART EQ (3-band), COMPRESSOR, DE-ESSER (simple broadband reduction based on high-frequency detection), SATURATION, REVERB, DELAY, OUTPUT (gain + soft clipping).
- **ANALYZE**: measures 7 seconds of *playing audio*, collects RMS, sample peak and a crude brightness proxy, then sets suggested thresholds and tonal adjustment. **Heuristic only**, not AI/ML, not full noise analysis.
- **5 styles**: NATURAL / POP / BALLAD / WARM / POWERFUL.
- Every stage may be enabled/disabled, locked from analysis/preset changes, and manually adjusted. "RESET STAGE" restores the selected stage to a style preset.
- **ENGINE ON / BYPASS** with ~12 ms crossfade; output and input peak indicators. Parameters and state are saved by the DAW.

This is an **alpha prototype**, not a guaranteed polished commercial mix processor. Algorithm quality, preset calibration and plugin validation should be tested with real vocals. No pitch correction, source separation, true-peak limiting or AI denoising.

## Build with GitHub (no Windows compiler locally)

1. Create a GitHub repository named `g-vocal` (prefer **private** if you don't want your drafts visible). The JUCE GPL license obligations must still be followed if distributing the built binary.
2. Upload **all contents** of this folder to the repository root, including hidden `.github/workflows/windows-vst3.yml`.
3. In the repository open **Actions** → **Build G-Vocal Windows VST3 + installer** → **Run workflow** (or push a commit to `main`).
4. Wait until the workflow has finished successfully. Open the workflow run → **Artifacts** → **G-Vocal-Windows-VST3-and-Setup** and download the ZIP.
5. Extract the artifact and run `G-Vocal-Setup-0.1.0.exe` as administrator on Windows. If Windows SmartScreen warns about an unsigned development installer, validate the source and build before choosing to run it.
6. Restart Ableton Live 12, enable **VST3** in Settings → Plug-Ins, rescan, add **G-Vocal** to an audio track.

**IMPORTANT:** A VST3 is a *bundle directory* on Windows, with a DLL-like binary at `Contents/x86_64-win/G-Vocal.vst3`; do not rename or extract this inner binary out of its bundle.

## Build manually on Windows

Requires CMake 3.22+, Visual Studio 2022 with Desktop C++ tooling, Git and internet on the first configure:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target GVocal_VST3 --parallel 4
```

The installer uses [NSIS](https://nsis.sourceforge.io/) and can be built via:

```powershell
mkdir dist
makensis installer/g-vocal.nsi
```

## License / distribution

The project is provided as an original early-stage source prototype. JUCE 8 is a third-party framework: read its [licensing terms](https://juce.com/legal/juce-8-licence/) carefully. A GPL-compatible open-source distribution requires compliance with JUCE's GPL terms, corresponding source distribution and notices. A proprietary closed-source release requires an appropriate JUCE commercial license. This ZIP does **not** include JUCE source files; CMake fetches pinned JUCE tag `8.0.17-security`.

## Known limitations

- Tested for structure and static consistency in this package, **not compiled with Windows MSVC in the authoring environment**. GitHub Actions is the first real compile test; fix any reported build errors before using.
- The analyzer requires 7 seconds of non-silent audio during playback. In silence the settings may be meaningless.
- Audio-quality features are deliberately basic; de-esser uses broadband attenuation and bypass for each stage is immediate (global engine BYPASS is smoothed).
- Delay internal feedback continues across block boundaries, with a capped maximum delay line.
- Installer is unsigned; code signing requires a separate trusted certificate.
