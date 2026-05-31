# Valvra — Living Tube Amp Colour

> Open-source VST3/AU plugin that re-creates what competitor tube emulations refuse to model: **time-varying behaviour**, **per-instance Monte Carlo variation**, and **real-time Jiles-Atherton transformer hysteresis**.

[![Build & Test](https://github.com/hwangtab/valvra/actions/workflows/build.yml/badge.svg)](https://github.com/hwangtab/valvra/actions/workflows/build.yml)
[![License: GPL-3](https://img.shields.io/badge/License-GPL--3-blue.svg)](LICENSE)
![Tests](https://img.shields.io/badge/Tests-162%2F162-brightgreen)
![ASAN+UBSAN](https://img.shields.io/badge/ASAN%2BUBSAN-clean-brightgreen)
![Formats](https://img.shields.io/badge/Formats-VST3%20|%20AU%20|%20Standalone-informational)
![Hidden physics](https://img.shields.io/badge/Hidden_physics-12_mechanisms-orange)
![JUCE](https://img.shields.io/badge/JUCE-8-blueviolet)
![C++](https://img.shields.io/badge/C%2B%2B-20-blue)

## What makes this different

Most analog emulation plugins (UAD, Waves, Slate, Softube, Soundtoys, …) compute a static `y = f(x)` — the same input always gives the same output. Real analog hardware computes

```
y = f(x, t, T_tube(t), V_cathode(t), M_core(t, history), θ_instance)
```

Valvra implements every one of those dimensions:

- 🔥 **Cathode bounce** — bias point drifts with signal level (natural tube compression)
- 🌡️ **Thermal warm-up** — first 30 s of sound subtly matures (g_m ramps 85% → 100%)
- ⚡ **PSU sag** — B+ rail dips under heavy signal (vintage "squish", tube vs SS rectifier presets)
- 🧲 **Jiles-Atherton transformer** — authentic magnetic hysteresis with RK4 integration
- 🎲 **Per-instance Monte Carlo** — every plugin instance sounds slightly different, the way two real Pultecs sit in a rack

Plus:

- **5 signature modes** — V72 Preamp · Console Output · Culture Vulture · RNDI DI · HiFi 300B SE — all calibrated for mix / mastering workflows. Console Output borrows the JCM800-class push-pull EL34 + UTC OPT physics but is biased squarely in class-A1 (gentle even-harmonic warming at the default Drive); pushing Drive past ~2 walks it into class-AB territory if you want guitar-amp coloration on tap.
- **Live visualisations** — real-time B–H hysteresis loop + harmonic meter (H₂–H₈)
- **Null-test button** — A/B hear exactly what the plugin adds
- **Reroll** — new Monte Carlo seed, live, click-free

## Status

**Tier 4+ core implemented.** The DSP engine is stable and validated, the
plugin builds as VST3/AU/Standalone, and expansion cores plus dedicated
Compressor/Tape targets are integrated.

- [x] Research: 25 documents, 13,000+ lines ([docs/](docs/))
- [x] Tier 0 — core DSP: Koren + Jiles-Atherton + oversampling
- [x] Tier 1 — integration: TubeStage, TubeAmpChain, signature presets
- [x] Tier 2 — VST3/AU/Standalone, chain builder, per-stage editing, B-H view, harmonic meter, null test, reroll timeline, warmup/drift UI
- [x] Tier 3 — M/S routing, true-peak safety modes, TPDF dither, loudness-matched snapshot A/B compare (A/B+C/D/E, 32-step undo/redo), GR meter, Monte Carlo Lock/distribution presets, 16x oversampling, Culture Vulture T/P1/P2 modes, Neural layer
- [x] Tier 4+ core — expansion engines (Opto/FET/Tape/SynthFX) + dedicated `ValvraCompressorPlugin` / `ValvraTapePlugin` packaging

Current validation baseline:

- `ctest`: 162/162 passing
- `scripts/audit_checks.py`: passing
- `scripts/validate_harmonics.py`: passing
- `scripts/preset_harmonic_report.py`: passing

Known remaining gaps versus the full document ambition:

- Some release-gate items in the docs are operational (DAW field validation,
  user feedback milestones, publish KPIs) rather than code implementation.

## Install

### macOS

```bash
./scripts/install_macos.sh     # copies .vst3 / .component into ~/Library/...
```

Or manually:

```bash
cp -R build/src/plugin/ValvraPlugin_artefacts/Release/VST3/Valvra.vst3 \
      ~/Library/Audio/Plug-Ins/VST3/
cp -R build/src/plugin/ValvraPlugin_artefacts/Release/AU/Valvra.component \
      ~/Library/Audio/Plug-Ins/Components/
```

**Gatekeeper first-launch**: the binary is self-signed. Right-click the .app → "Open", confirm once, done.

### Windows

Copy `build\src\plugin\ValvraPlugin_artefacts\Release\VST3\Valvra.vst3` to `C:\Program Files\Common Files\VST3\`. SmartScreen: "More info" → "Run anyway".

### Linux

Build from source, copy the VST3 bundle to `~/.vst3/`.

## Build from source

Requirements: **CMake ≥ 3.25**, **C++20 compiler** (Clang 13+, GCC 11+, MSVC 2022+), **Git**.

```bash
git clone <this-repo>.git
cd tubeamp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

First configure pulls JUCE 8 via FetchContent (~50 s one-time).

### Build targets

| Target | Artefact |
|--------|----------|
| `ValvraPlugin_VST3` | VST3 bundle |
| `ValvraPlugin_AU` | Audio Unit (macOS) |
| `ValvraPlugin_Standalone` | Standalone host app |
| `valvra_process` | Headless CLI (WAV + stdin/stdout) |
| `valvra_tests` | Unit test suite |

### Regression

```bash
(cd build && ctest)                            # 162 tests
python scripts/audit_checks.py                 # thread-safety + Monte Carlo
python scripts/validate_harmonics.py           # FFT harmonic validation
python scripts/preset_harmonic_report.py       # per-preset tone report
```

For release builds also run the ASAN+UBSAN sweep — see [`build-sanitizer/`](#) notes in docs.

## Command-line usage

```bash
# WAV mode
./valvra_process --input=in.wav --output=out.wav \
                 --preset=rndi --drive=1.5 --seed=42 --os=4

# Pipe mode (raw float32)
python generate.py | ./valvra_process --preset=v72 > out.raw

./valvra_process --help                        # all flags
```

Presets: `v72` · `rndi` · `marshall` · `cv` (Culture Vulture)  
Oversample: `1` · `2` · `4` · `8` · `16`

## Licence

**GPL-3** ([LICENSE](LICENSE)). Valvra links against JUCE 8 under its GPL-3 open-source terms, which is why the whole project is GPL-3. In plain English:

- ✅ Use it on any production, commercial or otherwise
- ✅ Share and modify freely
- ❌ Closed-source derivative plugins can't build on Valvra's sources

## Credits

DSP physical models build on academic research:

- Dempwolf, Holters, Zölzer (2011) *A Physically-Motivated Triode Model for Circuit Simulations* — DAFx-11
- Jiles, Atherton (1986) *Theory of ferromagnetic hysteresis* — JMMM 61
- Holters, Zölzer (2016) *Circuit Simulation with Inductors and Transformers Based on the Jiles-Atherton Model* — DAFx-16
- Rutt (1984) *Vacuum Tube Triode Nonlinearity as Part of the Electric Guitar Sound* — AES Preprint 2141
- Pakarinen, Yeh (2009) *A Review of Digital Techniques for Modeling Vacuum-Tube Guitar Amplifiers* — CMJ 33(2)

Complete bibliography in [docs/22-academic-quantitative-data.md](docs/22-academic-quantitative-data.md).

Third-party libraries:

- [JUCE 8](https://juce.com) — GPL-3
- [dr_wav](https://github.com/mackron/dr_libs) — public domain / MIT-0
- [Catch2](https://github.com/catchorg/Catch2) — BSL-1.0 (tests only)

---

*Valvra — because "sounds the same on every channel" isn't analog.*
