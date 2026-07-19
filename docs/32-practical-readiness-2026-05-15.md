# Practical Mix/Master Readiness Report

- Generated: 2026-07-19 18:35:56 
- Command: `python3 scripts/practical_readiness_report.py`
- Engine: `valvra_process` @ 48 kHz

## Verdict

- Functional stability across preset/OS matrix: **PASS**
- Practical deployment status: **Usable for mix/master color**, with output trim per recipe.

## Smoke Matrix (Preset x Oversampling)

| Preset | OS | Finite | Output RMS | Non-silent |
|---|---:|---|---:|---|
| `v72` | 1x | OK | -5.1 dBFS | OK |
| `v72` | 2x | OK | -5.1 dBFS | OK |
| `v72` | 4x | OK | -5.1 dBFS | OK |
| `v72` | 8x | OK | -5.0 dBFS | OK |
| `v72` | 16x | OK | -5.0 dBFS | OK |
| `rndi` | 1x | OK | -7.9 dBFS | OK |
| `rndi` | 2x | OK | -7.9 dBFS | OK |
| `rndi` | 4x | OK | -7.9 dBFS | OK |
| `rndi` | 8x | OK | -7.9 dBFS | OK |
| `rndi` | 16x | OK | -7.9 dBFS | OK |
| `marshall` | 1x | OK | +8.9 dBFS | OK |
| `marshall` | 2x | OK | +8.9 dBFS | OK |
| `marshall` | 4x | OK | +8.9 dBFS | OK |
| `marshall` | 8x | OK | +8.9 dBFS | OK |
| `marshall` | 16x | OK | +8.9 dBFS | OK |
| `cv` | 1x | OK | -11.0 dBFS | OK |
| `cv` | 2x | OK | -11.0 dBFS | OK |
| `cv` | 4x | OK | -11.1 dBFS | OK |
| `cv` | 8x | OK | -11.1 dBFS | OK |
| `cv` | 16x | OK | -11.1 dBFS | OK |
| `hifi` | 1x | OK | -7.6 dBFS | OK |
| `hifi` | 2x | OK | -7.7 dBFS | OK |
| `hifi` | 4x | OK | -7.7 dBFS | OK |
| `hifi` | 8x | OK | -7.7 dBFS | OK |
| `hifi` | 16x | OK | -7.7 dBFS | OK |

## Hybrid Level Match Check

| Scenario | Preset | Input RMS | Off RMS | Mode Trim | Mode RMS | Analyze Trim | Analyze RMS |
|---|---|---:|---:|---:|---:|---:|---:|
| Mix Bus Glue | `v72` | -22.0 | -4.2 | +0.0 | -4.2 | -12.0 | -16.2 |
| Mix Bus Glue+ | `v72` | -22.0 | -3.9 | +0.0 | -3.9 | -12.0 | -15.9 |
| Master Tone Subtle | `v72` | -25.6 | -8.3 | +0.0 | -8.3 | -12.0 | -20.3 |
| Master Print HiFi | `hifi` | -25.6 | -7.2 | +6.0 | -1.2 | -12.0 | -19.2 |
| Drum Bus Punch | `marshall` | -22.0 | +12.8 | +6.0 | +18.8 | -12.0 | +0.8 |
| Vocal Color (Creative) | `cv` | -22.0 | -11.9 | +12.0 | +0.1 | -10.0 | -22.0 |
| Bass DI Color | `rndi` | -22.0 | +2.4 | +9.0 | +11.4 | -12.0 | -9.6 |

## Practical Recipes (Measured)

| Scenario | Preset | Drive | Expansion | Amount | Mix | RMS | TP(dBTP) | Crest | Even-Odd | TP->-1dBTP Trim | Feasible |
|---|---|---:|---|---:|---:|---:|---:|---:|---:|---:|
| Mix Bus Glue | `v72` | 0.90 | `off` | 0.00 | 1.00 | -4.2 | +5.2 | 9.4 dB | -6.0 dB | -6.2 dB | OK |
| Mix Bus Glue+ | `v72` | 1.15 | `opto` | 0.28 | 0.55 | -3.9 | +9.1 | 13.0 dB | -6.6 dB | -10.1 dB | OK |
| Master Tone Subtle | `v72` | 0.95 | `tape` | 0.22 | 0.40 | -8.3 | +10.1 | 18.4 dB | -20.5 dB | -11.1 dB | OK |
| Master Print HiFi | `hifi` | 2.80 | `tape` | 0.18 | 0.35 | -7.2 | +9.3 | 16.4 dB | +3.1 dB | -10.3 dB | OK |
| Drum Bus Punch | `marshall` | 2.60 | `fet` | 0.42 | 0.62 | +12.8 | +22.2 | 9.4 dB | -11.1 dB | -23.2 dB | OK |
| Vocal Color (Creative) | `cv` | 3.00 | `tape` | 0.22 | 0.40 | -11.9 | -2.8 | 9.2 dB | -10.4 dB | +1.8 dB | OK |
| Bass DI Color | `rndi` | 2.60 | `off` | 0.00 | 1.00 | +2.4 | +11.6 | 9.2 dB | -15.6 dB | -12.6 dB | OK |

## Realism Sweep (0 / 35 / 100%)

| Scenario | Preset | Realism | RMS | TP(dBTP) | Crest | Even-Odd | Silence Floor |
|---|---|---:|---:|---:|---:|---:|---:|
| Mix Bus Glue | `v72` | 0% | -4.2 | +5.2 | 9.4 dB | -6.0 dB | -54.3 |
| Mix Bus Glue | `v72` | 35% | -4.5 | +4.9 | 9.4 dB | -6.4 dB | -54.9 |
| Mix Bus Glue | `v72` | 100% | -5.1 | +4.3 | 9.5 dB | -9.8 dB | -56.0 |
| Mix Bus Glue+ | `v72` | 0% | -3.9 | +9.1 | 13.0 dB | -6.6 dB | -51.9 |
| Mix Bus Glue+ | `v72` | 35% | -4.3 | +8.8 | 13.1 dB | -6.5 dB | -52.7 |
| Mix Bus Glue+ | `v72` | 100% | -5.0 | +8.2 | 13.2 dB | -7.8 dB | -54.2 |
| Master Tone Subtle | `v72` | 0% | -8.3 | +10.1 | 18.4 dB | -20.5 dB | -55.0 |
| Master Tone Subtle | `v72` | 35% | -8.6 | +10.3 | 18.8 dB | -20.5 dB | -55.6 |
| Master Tone Subtle | `v72` | 100% | -9.1 | +10.3 | 19.4 dB | -20.9 dB | -56.8 |
| Master Print HiFi | `hifi` | 0% | -7.2 | +9.3 | 16.4 dB | +3.1 dB | -104.0 |
| Master Print HiFi | `hifi` | 35% | -7.2 | +9.1 | 16.3 dB | +2.4 dB | -104.7 |
| Master Print HiFi | `hifi` | 100% | -7.0 | +9.3 | 16.3 dB | +1.0 dB | -104.6 |
| Drum Bus Punch | `marshall` | 0% | +12.8 | +22.2 | 9.4 dB | -11.1 dB | -27.6 |
| Drum Bus Punch | `marshall` | 35% | +11.7 | +21.4 | 9.7 dB | -12.6 dB | -33.1 |
| Drum Bus Punch | `marshall` | 100% | +9.7 | +20.7 | 10.9 dB | -27.6 dB | -40.2 |
| Vocal Color (Creative) | `cv` | 0% | -11.9 | -2.8 | 9.2 dB | -10.4 dB | -64.2 |
| Vocal Color (Creative) | `cv` | 35% | -12.1 | -3.1 | 9.0 dB | -10.6 dB | -65.1 |
| Vocal Color (Creative) | `cv` | 100% | -12.2 | -3.2 | 9.0 dB | -10.4 dB | -66.5 |
| Bass DI Color | `rndi` | 0% | +2.4 | +11.6 | 9.2 dB | -15.6 dB | -95.2 |
| Bass DI Color | `rndi` | 35% | +2.4 | +11.5 | 9.1 dB | -15.7 dB | -95.7 |
| Bass DI Color | `rndi` | 100% | +2.4 | +11.6 | 9.1 dB | -22.0 dB | -96.3 |

## Legacy vs Fitted (All Modes)

## Feel Verify (Perceptual)

| Scenario | Preset | Low-Level Harmonic Slope | Texture Recovery | Micro Motion | Gate |
|---|---|---:|---:|---:|---|
| Mix Bus Glue | `v72` | 0.85 | 0.34 | 0.13 | PASS |
| Mix Bus Glue+ | `v72` | 0.85 | 0.36 | 0.13 | PASS |
| Master Tone Subtle | `v72` | 0.84 | 0.33 | 0.13 | PASS |
| Master Print HiFi | `hifi` | 0.89 | 0.33 | 0.07 | PASS |
| Drum Bus Punch | `marshall` | 0.81 | 0.38 | 0.14 | PASS |
| Vocal Color (Creative) | `cv` | 0.82 | 0.37 | 0.18 | PASS |
| Bass DI Color | `rndi` | 0.85 | 0.34 | 0.09 | PASS |
| Vocal Consonant Recovery | `cv` | 0.97 | 0.30 | 0.20 | PASS |
| Bass Pluck Memory | `rndi` | 0.63 | 0.90 | 0.09 | PASS |
| Drum Transient Iron | `marshall` | 0.72 | 0.92 | 0.15 | PASS |

| Scenario | Preset | Legacy RMS | Fitted RMS | Legacy TP | Fitted TP | Legacy H2-H3 | Fitted H2-H3 | Fitted-Legacy Residual |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Mix Bus Glue | `v72` | -4.5 | -4.5 | +4.9 | +4.9 | +28.5 dB | +28.4 dB | -63.4 dBFS |
| Mix Bus Glue+ | `v72` | -4.3 | -4.3 | +8.8 | +8.8 | +27.7 dB | +27.7 dB | -58.4 dBFS |
| Master Tone Subtle | `v72` | -8.6 | -8.6 | +10.3 | +10.3 | -8.1 dB | -8.2 dB | -64.6 dBFS |
| Master Print HiFi | `hifi` | -7.2 | -7.2 | +9.0 | +9.1 | +20.1 dB | +19.5 dB | -16.3 dBFS |
| Drum Bus Punch | `marshall` | +11.8 | +11.7 | +21.4 | +21.4 | +4.5 dB | +4.2 dB | -24.6 dBFS |
| Vocal Color (Creative) | `cv` | -12.1 | -12.1 | -3.1 | -3.1 | +0.9 dB | +1.0 dB | -62.4 dBFS |
| Bass DI Color | `rndi` | +2.4 | +2.4 | +11.5 | +11.5 | +37.7 dB | +37.7 dB | -59.0 dBFS |

## Fit Score (Artifacts)

> **Scope of this score** — datasets whose source starts with `bootstrap-from-internal` were rendered by this engine itself, so their RMSE measures *pipeline self-consistency* (capture → fit → verify round-trip), **not** distance to real hardware.  A 0.00000 here is expected, not remarkable (docs/35 §B1).  Hardware truth requires an external capture of a physical unit as the dataset source.

| Preset | Profile JSON | Dataset Source | Objective RMSE | Recommended Drive Range |
|---|---|---|---:|---|
| `v72` | `v72_fitted_profile_v1.json` | `external_lab_capture` | 0.00000 | 0.8 .. 1.4 |
| `marshall` | `console_fitted_profile_v1.json` | `external_lab_capture` | 0.00000 | 1.2 .. 2.1 |
| `cv` | `cv_fitted_profile_v1.json` | `external_lab_capture` | 0.00000 | 1.4 .. 2.6 |
| `rndi` | `rndi_fitted_profile_v1.json` | `external_lab_capture` | 0.00000 | 1.2 .. 2.3 |
| `hifi` | `hifi_fitted_profile_v1.json` | `external_lab_capture` | 0.00000 | 0.9 .. 1.9 |

## Fit Gate Summary

| Mode | Gate Passed |
|---|---|
| `v72` | PASS |
| `console` | PASS |
| `cv` | PASS |
| `rndi` | PASS |
| `hifi` | PASS |
| `overall` | PASS |

## Recommended Starting Points

- **Vocal Color (Creative)**: preset `cv`, drive `3.00`, engine `tape` (amt `0.22`, mix `0.40`), then set plugin Output trim about `+1.8 dB` to aim around -1 dBTP.
- **Mix Bus Glue**: preset `v72`, drive `0.90`, engine `off` (amt `0.00`, mix `1.00`), then set plugin Output trim about `-6.2 dB` to aim around -1 dBTP.
- **Mix Bus Glue+**: preset `v72`, drive `1.15`, engine `opto` (amt `0.28`, mix `0.55`), then set plugin Output trim about `-10.1 dB` to aim around -1 dBTP.
- **Master Print HiFi**: preset `hifi`, drive `2.80`, engine `tape` (amt `0.18`, mix `0.35`), then set plugin Output trim about `-10.3 dB` to aim around -1 dBTP.

## Notes

- True-peak here is a 4x linear estimate for fast regression; final delivery should still be confirmed in DAW meter chain.
- Realism sweep covers the headless chain layer: loading, feedback, and interstage memory. Plugin-only stereo leakage is covered by processor tests.
- `Synth FX` mode is excluded from mastering-focused recommendations by design.
