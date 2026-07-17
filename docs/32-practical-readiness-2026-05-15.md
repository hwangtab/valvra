# Practical Mix/Master Readiness Report

- Generated: 2026-07-18 03:41:01 
- Command: `python3 scripts/practical_readiness_report.py`
- Engine: `valvra_process` @ 48 kHz

## Verdict

- Functional stability across preset/OS matrix: **PASS**
- Practical deployment status: **Usable for mix/master color**, with output trim per recipe.

## Smoke Matrix (Preset x Oversampling)

| Preset | OS | Finite | Output RMS | Non-silent |
|---|---:|---|---:|---|
| `v72` | 1x | OK | -5.2 dBFS | OK |
| `v72` | 2x | OK | -5.1 dBFS | OK |
| `v72` | 4x | OK | -5.1 dBFS | OK |
| `v72` | 8x | OK | -5.1 dBFS | OK |
| `v72` | 16x | OK | -5.1 dBFS | OK |
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
| `cv` | 1x | OK | -13.1 dBFS | OK |
| `cv` | 2x | OK | -12.9 dBFS | OK |
| `cv` | 4x | OK | -12.8 dBFS | OK |
| `cv` | 8x | OK | -12.8 dBFS | OK |
| `cv` | 16x | OK | -12.8 dBFS | OK |
| `hifi` | 1x | OK | -7.6 dBFS | OK |
| `hifi` | 2x | OK | -7.7 dBFS | OK |
| `hifi` | 4x | OK | -7.7 dBFS | OK |
| `hifi` | 8x | OK | -7.7 dBFS | OK |
| `hifi` | 16x | OK | -7.7 dBFS | OK |

## Hybrid Level Match Check

| Scenario | Preset | Input RMS | Off RMS | Mode Trim | Mode RMS | Analyze Trim | Analyze RMS |
|---|---|---:|---:|---:|---:|---:|---:|
| Mix Bus Glue | `v72` | -22.0 | -4.1 | +0.0 | -4.1 | -12.0 | -16.1 |
| Mix Bus Glue+ | `v72` | -22.0 | -3.9 | +0.0 | -3.9 | -12.0 | -15.9 |
| Master Tone Subtle | `v72` | -25.6 | -8.3 | +0.0 | -8.3 | -12.0 | -20.3 |
| Master Print HiFi | `hifi` | -25.6 | -7.2 | +6.0 | -1.2 | -12.0 | -19.2 |
| Drum Bus Punch | `marshall` | -22.0 | +12.8 | +6.0 | +18.8 | -12.0 | +0.8 |
| Vocal Color (Creative) | `cv` | -22.0 | -13.5 | +12.0 | -1.5 | -8.4 | -22.0 |
| Bass DI Color | `rndi` | -22.0 | +2.4 | +9.0 | +11.4 | -12.0 | -9.6 |

## Practical Recipes (Measured)

| Scenario | Preset | Drive | Expansion | Amount | Mix | RMS | TP(dBTP) | Crest | Even-Odd | TP->-1dBTP Trim | Feasible |
|---|---|---:|---|---:|---:|---:|---:|---:|---:|---:|
| Mix Bus Glue | `v72` | 0.90 | `off` | 0.00 | 1.00 | -4.1 | +5.2 | 9.4 dB | -6.2 dB | -6.2 dB | OK |
| Mix Bus Glue+ | `v72` | 1.15 | `opto` | 0.28 | 0.55 | -3.9 | +9.1 | 13.0 dB | -6.8 dB | -10.1 dB | OK |
| Master Tone Subtle | `v72` | 0.95 | `tape` | 0.22 | 0.40 | -8.3 | +10.2 | 18.4 dB | -20.4 dB | -11.2 dB | OK |
| Master Print HiFi | `hifi` | 2.80 | `tape` | 0.18 | 0.35 | -7.2 | +9.5 | 16.7 dB | +4.0 dB | -10.5 dB | OK |
| Drum Bus Punch | `marshall` | 2.60 | `fet` | 0.42 | 0.62 | +12.8 | +22.1 | 9.3 dB | -11.1 dB | -23.1 dB | OK |
| Vocal Color (Creative) | `cv` | 3.00 | `tape` | 0.22 | 0.40 | -13.5 | -4.3 | 9.3 dB | -12.3 dB | +3.3 dB | OK |
| Bass DI Color | `rndi` | 2.60 | `off` | 0.00 | 1.00 | +2.4 | +11.6 | 9.2 dB | -16.5 dB | -12.6 dB | OK |

## Realism Sweep (0 / 35 / 100%)

| Scenario | Preset | Realism | RMS | TP(dBTP) | Crest | Even-Odd | Silence Floor |
|---|---|---:|---:|---:|---:|---:|---:|
| Mix Bus Glue | `v72` | 0% | -4.1 | +5.2 | 9.4 dB | -6.2 dB | -54.2 |
| Mix Bus Glue | `v72` | 35% | -4.5 | +4.9 | 9.4 dB | -6.6 dB | -54.8 |
| Mix Bus Glue | `v72` | 100% | -5.1 | +4.3 | 9.5 dB | -10.0 dB | -55.9 |
| Mix Bus Glue+ | `v72` | 0% | -3.9 | +9.1 | 13.0 dB | -6.8 dB | -51.8 |
| Mix Bus Glue+ | `v72` | 35% | -4.3 | +8.8 | 13.1 dB | -6.6 dB | -52.6 |
| Mix Bus Glue+ | `v72` | 100% | -5.0 | +8.2 | 13.2 dB | -7.9 dB | -54.1 |
| Master Tone Subtle | `v72` | 0% | -8.3 | +10.2 | 18.4 dB | -20.4 dB | -54.9 |
| Master Tone Subtle | `v72` | 35% | -8.6 | +10.3 | 18.9 dB | -20.6 dB | -55.5 |
| Master Tone Subtle | `v72` | 100% | -9.1 | +10.3 | 19.4 dB | -21.3 dB | -56.7 |
| Master Print HiFi | `hifi` | 0% | -7.2 | +9.5 | 16.7 dB | +4.0 dB | -104.0 |
| Master Print HiFi | `hifi` | 35% | -7.2 | +9.0 | 16.2 dB | +1.1 dB | -104.6 |
| Master Print HiFi | `hifi` | 100% | -7.0 | +9.6 | 16.6 dB | +2.1 dB | -103.7 |
| Drum Bus Punch | `marshall` | 0% | +12.8 | +22.1 | 9.3 dB | -11.1 dB | -27.5 |
| Drum Bus Punch | `marshall` | 35% | +11.7 | +21.3 | 9.6 dB | -12.6 dB | -33.2 |
| Drum Bus Punch | `marshall` | 100% | +9.8 | +20.6 | 10.8 dB | -27.4 dB | -40.3 |
| Vocal Color (Creative) | `cv` | 0% | -13.5 | -4.3 | 9.3 dB | -12.3 dB | -70.5 |
| Vocal Color (Creative) | `cv` | 35% | -13.7 | -4.6 | 9.1 dB | -12.4 dB | -69.4 |
| Vocal Color (Creative) | `cv` | 100% | -13.8 | -4.6 | 9.2 dB | -12.6 dB | -66.1 |
| Bass DI Color | `rndi` | 0% | +2.4 | +11.6 | 9.2 dB | -16.5 dB | -95.3 |
| Bass DI Color | `rndi` | 35% | +2.4 | +11.5 | 9.1 dB | -16.6 dB | -95.9 |
| Bass DI Color | `rndi` | 100% | +2.4 | +11.6 | 9.1 dB | -22.3 dB | -96.4 |

## Legacy vs Fitted (All Modes)

## Feel Verify (Perceptual)

| Scenario | Preset | Low-Level Harmonic Slope | Texture Recovery | Micro Motion | Gate |
|---|---|---:|---:|---:|---|
| Mix Bus Glue | `v72` | 0.85 | 0.34 | 0.14 | PASS |
| Mix Bus Glue+ | `v72` | 0.85 | 0.36 | 0.13 | PASS |
| Master Tone Subtle | `v72` | 0.84 | 0.33 | 0.13 | PASS |
| Master Print HiFi | `hifi` | 0.90 | 0.33 | 0.07 | PASS |
| Drum Bus Punch | `marshall` | 0.81 | 0.38 | 0.14 | PASS |
| Vocal Color (Creative) | `cv` | 0.83 | 0.37 | 0.20 | PASS |
| Bass DI Color | `rndi` | 0.85 | 0.34 | 0.09 | PASS |
| Vocal Consonant Recovery | `cv` | 1.00 | 0.33 | 0.18 | PASS |
| Bass Pluck Memory | `rndi` | 0.70 | 0.90 | 0.09 | PASS |
| Drum Transient Iron | `marshall` | 0.90 | 0.62 | 0.14 | PASS |

| Scenario | Preset | Legacy RMS | Fitted RMS | Legacy TP | Fitted TP | Legacy H2-H3 | Fitted H2-H3 | Fitted-Legacy Residual |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Mix Bus Glue | `v72` | -4.5 | -4.5 | +4.9 | +4.9 | +28.2 dB | +28.1 dB | -63.3 dBFS |
| Mix Bus Glue+ | `v72` | -4.3 | -4.3 | +8.8 | +8.8 | +27.6 dB | +27.6 dB | -58.3 dBFS |
| Master Tone Subtle | `v72` | -8.6 | -8.6 | +10.3 | +10.3 | -8.9 dB | -8.9 dB | -64.4 dBFS |
| Master Print HiFi | `hifi` | -7.2 | -7.2 | +9.0 | +9.0 | +22.7 dB | +19.7 dB | -16.5 dBFS |
| Drum Bus Punch | `marshall` | +11.8 | +11.7 | +21.4 | +21.3 | +4.3 dB | +4.0 dB | -24.6 dBFS |
| Vocal Color (Creative) | `cv` | -13.7 | -13.7 | -4.6 | -4.6 | -5.9 dB | -5.9 dB | -64.6 dBFS |
| Bass DI Color | `rndi` | +2.4 | +2.4 | +11.5 | +11.5 | +35.8 dB | +35.9 dB | -59.0 dBFS |

## Fit Score (Artifacts)

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

- **Vocal Color (Creative)**: preset `cv`, drive `3.00`, engine `tape` (amt `0.22`, mix `0.40`), then set plugin Output trim about `+3.3 dB` to aim around -1 dBTP.
- **Mix Bus Glue**: preset `v72`, drive `0.90`, engine `off` (amt `0.00`, mix `1.00`), then set plugin Output trim about `-6.2 dB` to aim around -1 dBTP.
- **Mix Bus Glue+**: preset `v72`, drive `1.15`, engine `opto` (amt `0.28`, mix `0.55`), then set plugin Output trim about `-10.1 dB` to aim around -1 dBTP.
- **Master Print HiFi**: preset `hifi`, drive `2.80`, engine `tape` (amt `0.18`, mix `0.35`), then set plugin Output trim about `-10.5 dB` to aim around -1 dBTP.

## Notes

- True-peak here is a 4x linear estimate for fast regression; final delivery should still be confirmed in DAW meter chain.
- Realism sweep covers the headless chain layer: loading, feedback, and interstage memory. Plugin-only stereo leakage is covered by processor tests.
- `Synth FX` mode is excluded from mastering-focused recommendations by design.
