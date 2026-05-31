# Valvra User Manual (EN)

## 1. Overview
Valvra is a tube/transformer coloration plugin for mixing and mastering.
Core modes:
- V72 Preamp
- Console Output
- Culture Vulture
- RNDI DI
- HiFi 300B SE

## 2. Quick Start
1. Insert Valvra on a track or bus
2. Select a `Mode`
3. Dial `Drive`, `Output`, and `Mix`
4. Increase `Quality` (oversampling) if needed
5. On master bus, set `TP Mode` and `Ceiling` in Mastering

## 3. Core Features

### 3.1 Chain Builder / Stage Editor
- Build chains via `Stage Count` and `Input/Output Transformer`
- Edit per-stage Tube/Topology/Drive/Bias in Stage Editor

### 3.2 Signature Views
- B-H Hysteresis: live output-transformer hysteresis trace
- Harmonics: H2~H7 meter
- Drift Recorder: 60s Sag/Warmup/Thermal-drift timeline
- Reroll Timeline: instant seed recall

### 3.3 A/B Compare
- `A|B` compare switching
- `A->B`, `B->A`, `Reset AB`
- `C/D/E` snapshot bank (click=load, Shift+click=store)
- 32-step Undo/Redo for A/B workflow
- Blind mode

### 3.4 Mastering
- TP Safety Mode: Off / Soft / Brick-wall
- TP Ceiling: -3.0 to -0.1 dBTP
- TP Lookahead: 1 to 10 ms
- TPDF Dither: 16/20/24-bit
- Mid/Side routing

### 3.5 Neural (RTNeural)
- `Neural` knob: blend neural residual against physics path
- `Load NN`: load RTNeural JSON model
- `Unload NN`: disable model (pure physics path)
- Model path is persisted with project state

## 4. Recommended Workflow
- Track color: Drive 0.8~1.5, Mix 20~60%
- Bus/master: Drive 0.3~1.0, Mix 100%, TP Brick-wall (around -1 dBTP)
- For decisions: keep A/B in LUFS-matched condition

## 5. Troubleshooting
- Too harsh: reduce Drive, increase Quality
- High CPU: lower Quality (16x→8x/4x), reduce stage count
- Neural load failure: verify JSON path/format, `Unload NN` then retry
