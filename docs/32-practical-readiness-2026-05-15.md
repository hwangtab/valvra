# Practical Mix/Master Readiness Report

- Generated: 2026-06-01 00:39:20
- Command: `python3 scripts/practical_readiness_report.py`
- Engine: `valvra_process` @ 48 kHz

## Verdict

- Functional stability across preset/OS matrix: **PASS**
- Practical deployment status: **Usable for mix/master color**, with output trim per recipe.

## Smoke Matrix (Preset x Oversampling)

| Preset | OS | Finite | Output RMS | Non-silent |
|---|---:|---|---:|---|
| `v72` | 1x | OK | -17.2 dBFS | OK |
| `v72` | 2x | OK | -17.1 dBFS | OK |
| `v72` | 4x | OK | -17.1 dBFS | OK |
| `v72` | 8x | OK | -17.1 dBFS | OK |
| `v72` | 16x | OK | -17.1 dBFS | OK |
| `rndi` | 1x | OK | -47.5 dBFS | OK |
| `rndi` | 2x | OK | -52.3 dBFS | OK |
| `rndi` | 4x | OK | -50.9 dBFS | OK |
| `rndi` | 8x | OK | -45.4 dBFS | OK |
| `rndi` | 16x | OK | -41.5 dBFS | OK |
| `marshall` | 1x | OK | -36.2 dBFS | OK |
| `marshall` | 2x | OK | -36.3 dBFS | OK |
| `marshall` | 4x | OK | -36.3 dBFS | OK |
| `marshall` | 8x | OK | -36.3 dBFS | OK |
| `marshall` | 16x | OK | -36.9 dBFS | OK |
| `cv` | 1x | OK | -65.0 dBFS | OK |
| `cv` | 2x | OK | -69.2 dBFS | OK |
| `cv` | 4x | OK | -62.8 dBFS | OK |
| `cv` | 8x | OK | -56.6 dBFS | OK |
| `cv` | 16x | OK | -50.3 dBFS | OK |
| `hifi` | 1x | OK | -62.2 dBFS | OK |
| `hifi` | 2x | OK | -62.7 dBFS | OK |
| `hifi` | 4x | OK | -57.3 dBFS | OK |
| `hifi` | 8x | OK | -51.5 dBFS | OK |
| `hifi` | 16x | OK | -45.9 dBFS | OK |

## Hybrid Level Match Check

| Scenario | Preset | Input RMS | Off RMS | Mode Trim | Mode RMS | Analyze Trim | Analyze RMS |
|---|---|---:|---:|---:|---:|---:|---:|
| Mix Bus Glue | `v72` | -22.0 | -20.4 | +0.0 | -20.4 | -1.5 | -22.0 |
| Mix Bus Glue+ | `v72` | -22.0 | -15.0 | +0.0 | -15.0 | -7.0 | -22.0 |
| Master Tone Subtle | `v72` | -25.6 | -21.2 | +0.0 | -21.2 | -4.4 | -25.6 |
| Master Print HiFi | `hifi` | -25.6 | -15.1 | +6.0 | -9.1 | -10.5 | -25.6 |
| Drum Bus Punch | `marshall` | -22.0 | -23.6 | +6.0 | -17.6 | +1.7 | -22.0 |
| Vocal Color (Creative) | `cv` | -22.0 | -31.9 | +12.0 | -19.9 | +9.9 | -22.0 |
| Bass DI Color | `rndi` | -22.0 | -26.8 | +9.0 | -17.8 | +4.8 | -22.0 |

## Practical Recipes (Measured)

| Scenario | Preset | Drive | Expansion | Amount | Mix | RMS | TP(dBTP) | Crest | Even-Odd | TP->-1dBTP Trim | Feasible |
|---|---|---:|---|---:|---:|---:|---:|---:|---:|---:|
| Mix Bus Glue | `v72` | 0.90 | `off` | 0.00 | 1.00 | -20.4 | -14.9 | 5.5 dB | -14.0 dB | +13.9 dB | OK |
| Mix Bus Glue+ | `v72` | 1.15 | `opto` | 0.28 | 0.55 | -15.0 | -9.4 | 5.6 dB | -6.9 dB | +8.4 dB | OK |
| Master Tone Subtle | `v72` | 0.95 | `tape` | 0.22 | 0.40 | -21.2 | -7.1 | 14.0 dB | -21.7 dB | +6.1 dB | OK |
| Master Print HiFi | `hifi` | 2.80 | `tape` | 0.18 | 0.35 | -15.1 | -3.3 | 11.8 dB | -4.5 dB | +2.3 dB | OK |
| Drum Bus Punch | `marshall` | 2.60 | `fet` | 0.42 | 0.62 | -23.6 | -12.7 | 10.9 dB | +2.0 dB | +11.7 dB | OK |
| Vocal Color (Creative) | `cv` | 3.00 | `tape` | 0.22 | 0.40 | -31.9 | -18.7 | 13.1 dB | +5.3 dB | +17.7 dB | OK |
| Bass DI Color | `rndi` | 2.60 | `off` | 0.00 | 1.00 | -26.8 | -21.5 | 5.3 dB | -5.5 dB | +20.5 dB | OK |

## Realism Sweep (0 / 35 / 100%)

| Scenario | Preset | Realism | RMS | TP(dBTP) | Crest | Even-Odd | Silence Floor |
|---|---|---:|---:|---:|---:|---:|---:|
| Mix Bus Glue | `v72` | 0% | -20.4 | -14.9 | 5.5 dB | -14.0 dB | -61.5 |
| Mix Bus Glue | `v72` | 35% | -19.7 | -13.9 | 5.7 dB | -12.9 dB | -61.0 |
| Mix Bus Glue | `v72` | 100% | -18.4 | -12.3 | 6.1 dB | -11.0 dB | -60.1 |
| Mix Bus Glue+ | `v72` | 0% | -15.0 | -9.4 | 5.6 dB | -6.9 dB | -62.8 |
| Mix Bus Glue+ | `v72` | 35% | -14.5 | -8.8 | 5.7 dB | -7.3 dB | -62.3 |
| Mix Bus Glue+ | `v72` | 100% | -14.1 | -8.1 | 5.9 dB | -6.8 dB | -61.3 |
| Master Tone Subtle | `v72` | 0% | -21.2 | -7.1 | 14.0 dB | -21.7 dB | -56.4 |
| Master Tone Subtle | `v72` | 35% | -20.2 | -6.5 | 13.7 dB | -20.5 dB | -55.9 |
| Master Tone Subtle | `v72` | 100% | -18.4 | -5.4 | 13.0 dB | -20.2 dB | -54.9 |
| Master Print HiFi | `hifi` | 0% | -15.1 | -3.3 | 11.8 dB | -4.5 dB | -69.8 |
| Master Print HiFi | `hifi` | 35% | -14.7 | -3.0 | 11.7 dB | -3.1 dB | -70.0 |
| Master Print HiFi | `hifi` | 100% | -13.0 | -2.3 | 10.7 dB | -3.6 dB | -70.2 |
| Drum Bus Punch | `marshall` | 0% | -23.6 | -12.7 | 10.9 dB | +2.0 dB | -73.2 |
| Drum Bus Punch | `marshall` | 35% | -22.9 | -12.5 | 10.4 dB | +1.6 dB | -72.7 |
| Drum Bus Punch | `marshall` | 100% | -21.4 | -11.6 | 9.8 dB | +0.8 dB | -71.5 |
| Vocal Color (Creative) | `cv` | 0% | -31.9 | -18.7 | 13.1 dB | +5.3 dB | -80.0 |
| Vocal Color (Creative) | `cv` | 35% | -32.4 | -19.4 | 13.0 dB | +8.7 dB | -80.8 |
| Vocal Color (Creative) | `cv` | 100% | -34.6 | -21.0 | 13.6 dB | -2.3 dB | -83.2 |
| Bass DI Color | `rndi` | 0% | -26.8 | -21.5 | 5.3 dB | -5.5 dB | -69.6 |
| Bass DI Color | `rndi` | 35% | -26.4 | -21.1 | 5.3 dB | -5.7 dB | -70.1 |
| Bass DI Color | `rndi` | 100% | -25.5 | -20.2 | 5.4 dB | -6.0 dB | -70.9 |

## Legacy vs Fitted (All Modes)

## Feel Verify (Perceptual)

| Scenario | Preset | Low-Level Harmonic Slope | Texture Recovery | Micro Motion | Gate |
|---|---|---:|---:|---:|---|
| Mix Bus Glue | `v72` | 0.64 | 0.37 | 0.08 | PASS |
| Mix Bus Glue+ | `v72` | 0.59 | 0.37 | 0.12 | PASS |
| Master Tone Subtle | `v72` | 0.67 | 0.44 | 0.09 | PASS |
| Master Print HiFi | `hifi` | 0.46 | 0.47 | 0.05 | PASS |
| Drum Bus Punch | `marshall` | 0.40 | 0.38 | 0.51 | PASS |
| Vocal Color (Creative) | `cv` | 0.51 | 0.40 | 0.12 | PASS |
| Bass DI Color | `rndi` | 0.35 | 0.37 | 0.28 | PASS |
| Vocal Consonant Recovery | `cv` | 1.00 | 0.40 | 0.16 | PASS |
| Bass Pluck Memory | `rndi` | 0.50 | 0.94 | 0.28 | PASS |
| Drum Transient Iron | `marshall` | 0.70 | 0.95 | 0.51 | PASS |

| Scenario | Preset | Legacy RMS | Fitted RMS | Legacy TP | Fitted TP | Legacy H2-H3 | Fitted H2-H3 | Fitted-Legacy Residual |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Mix Bus Glue | `v72` | -19.8 | -19.7 | -14.0 | -13.9 | +5.7 dB | +5.8 dB | -58.3 dBFS |
| Mix Bus Glue+ | `v72` | -14.6 | -14.5 | -8.9 | -8.8 | +9.9 dB | +9.9 dB | -54.0 dBFS |
| Master Tone Subtle | `v72` | -20.2 | -20.2 | -6.5 | -6.5 | -0.5 dB | -0.5 dB | -57.9 dBFS |
| Master Print HiFi | `hifi` | -14.7 | -14.7 | -3.0 | -3.0 | +13.0 dB | +13.0 dB | -60.0 dBFS |
| Drum Bus Punch | `marshall` | -23.0 | -22.9 | -12.5 | -12.5 | +17.1 dB | +17.0 dB | -69.7 dBFS |
| Vocal Color (Creative) | `cv` | -32.4 | -32.4 | -19.3 | -19.4 | +19.4 dB | +18.5 dB | -76.3 dBFS |
| Bass DI Color | `rndi` | -26.4 | -26.4 | -21.1 | -21.1 | +22.0 dB | +22.0 dB | -78.6 dBFS |

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

- **Master Print HiFi**: preset `hifi`, drive `2.80`, engine `tape` (amt `0.18`, mix `0.35`), then set plugin Output trim about `+2.3 dB` to aim around -1 dBTP.
- **Master Tone Subtle**: preset `v72`, drive `0.95`, engine `tape` (amt `0.22`, mix `0.40`), then set plugin Output trim about `+6.1 dB` to aim around -1 dBTP.
- **Mix Bus Glue+**: preset `v72`, drive `1.15`, engine `opto` (amt `0.28`, mix `0.55`), then set plugin Output trim about `+8.4 dB` to aim around -1 dBTP.
- **Drum Bus Punch**: preset `marshall`, drive `2.60`, engine `fet` (amt `0.42`, mix `0.62`), then set plugin Output trim about `+11.7 dB` to aim around -1 dBTP.

## Notes

- True-peak here is a 4x linear estimate for fast regression; final delivery should still be confirmed in DAW meter chain.
- Realism sweep covers the headless chain layer: loading, feedback, and interstage memory. Plugin-only stereo leakage is covered by processor tests.
- `Synth FX` mode is excluded from mastering-focused recommendations by design.
