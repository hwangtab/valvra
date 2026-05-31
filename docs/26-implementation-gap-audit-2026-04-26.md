# 26. 구현 갭 감사 (2026-04-26)

> 목적: `docs/20`과 `docs/24`가 정의한 "센세이셔널 진공관 앰프 색깔 프로세서" 기준에 대해 현재 코드가 어디까지 왔는지 추적한다. 이 문서는 과장 없는 구현 기준선이며, 다음 개발 순서를 정하는 체크리스트다.

## 1. 현재 판정

**상태:** Tier 3 구현 완료 + Tier 4+ 엔진 코어 구현(2026-05-07 업데이트)

- DSP 코어는 안정적이다.
- 플러그인은 VST3/AU/Standalone으로 빌드된다.
- 시그니처 UI Tier 2 체크리스트는 코드 기준으로 충족했다.
- `docs/20`의 Tier 3 구현 체크리스트는 코드/문서 기준으로 충족했다.

## 2. 검증 기준선

2026-05-08 현재 로컬 검증:

- `cmake --build build --target valvra_tests valvra_process -j4`: 성공
- `(cd build && ctest --output-on-failure)`: 157/157 통과
- `python3 scripts/audit_checks.py`: 통과
- `python3 scripts/validate_harmonics.py`: 통과
- `python3 scripts/preset_harmonic_report.py`: 통과

## 3. 완료된 핵심 축

| 영역 | 상태 | 근거 |
|---|---:|---|
| Koren/Dempwolf triode model | 완료 | `src/dsp/KorenTriode.*`, unit tests |
| Jiles-Atherton transformer | 완료 | `src/dsp/JilesAtherton.*`, `TransformerStage.h` |
| Cathode bounce | 완료 | `src/dsp/CathodeBounce.*` |
| PSU sag | 완료 | `src/dsp/PowerSupplySag.*` |
| Thermal warmup/drift | 완료 | `TubeStage.h`, `DriftRecorderView`, warmup trigger |
| Monte Carlo variation | 완료 | `ComponentVariation.*`, reroll/recall, Lock, Modern/Vintage/Worn/Wild distribution presets |
| Signature mode presets | 완료 | `TubeAmpChain.h` |
| Oversampling | 완료 | `PolyphaseOversampler.h`, 1x/2x/4x/8x/16x |
| VST3/AU/Standalone | 완료 | JUCE plugin target |
| Chain Builder UI baseline | 완료 | `PluginEditor.*`, `PluginProcessor.*` |
| Per-stage editor (tube/topology/drive/bias) | 완료 | `StageEditorPanel`, `kStageParams[4]` |
| Ideal phase split + push-pull EL34 power stage | 완료 | `PushPullStage.h`, Marshall preset |
| Full LTP phase-splitter internals | 완료 | `TubeStage::LongTailedPair` + `PushPullStage.h` LTP 경로 (`useLtpPhaseSplitter`, tail solver, mismatch/CMRR) |
| EL34 / 6L6GC triode-strapped Dempwolf params | 완료 | `params::kEL34_TriodeStrapped` 등 |
| 6SN7 / 300B / EF86 (triode-strap) Dempwolf params | 완료 | `params::k6SN7`, `k300B`, `kEF86_TriodeStrapped` |
| HiFi 300B SE preset (5번째 모드) | 완료 | `chain_presets::HiFi300BMode()` |
| CPU benchmark harness (회귀 측정 인프라) | 완료 | `bench/valvra_bench.cpp`, [docs/27](27-bench-baseline-2026-04-27.md) |
| 분석적 gm 솔버 최적화 (PushPull / SRPP / Cascode) | 완료 | `KorenTriode::evalWithDerivatives()`, Marshall -25% CPU |
| Marshall → Console Output 리튠 (mix/master use case 정합) | 완료 | `MarshallMode()` Vg_bias=−25 V class-A1; `PresetMode::ConsoleOutput`; UI 라벨 갱신 |
| SRPP (active-load stacked-triode preamp topology) | 완료 | `TubeStage.h` SRPP branch |
| Cascode (Miller-suppressed wideband topology) | 완료 | `TubeStage.h` Cascode branch |
| B-H loop view | 완료 | `SignatureViews.h`, `PluginEditor.*` (60 Hz 업데이트 + OpenGL 렌더링 경로) |
| Harmonic meter | 완료 | `HarmonicAnalyzer.h`, `SignatureViews.h` (H2~H7) |
| Null test | 완료 | `PluginProcessor.*`, `SignatureViews.h` |
| Warmup HUD + Drift Recorder (sag/warmup/thermal) | 완료 | `DriftRecorderView`, `readDriftState()` |
| Warmup trigger button | 완료 | `triggerWarmup()`, `simulateWarmup()` |
| Reroll Timeline (last 10 seeds, click-to-recall) | 완료 | `RerollTimelinePanel`, `recallSeed()` |
| True Peak limiter (4× ITU-R BS.1770-5) | 완료 | `TruePeakLimiter.h`, `MasteringPanel`; Off/Soft/Brick-wall, -3.0~-0.1 dBTP, 1~10 ms lookahead |
| TPDF dither (16/20/24-bit) | 완료 | `PluginProcessor.cpp` 출력단 |
| A/B compare (A/B + C/D/E snapshot + 32-step undo/redo) | 완료 | `PluginProcessor.*`, `PluginEditor.*`; integrated K-weighted LUFS match + 10 ms graph transition |
| GR meter | 완료 | `GainReductionMeter`, `gainReductionDb()` |
| Mid/Side independent processing | 완료 | `kParamMSMode`, energy-preserving encode/decode in processBlock |
| Linear-phase oversampling | 이미 충족 | Kaiser-windowed FIR (`PolyphaseOversampler`) |
| CLI validation tool | 완료 | `src/cli/valvra_process.cpp` |
| Tier 4+ Expansion 엔진 코어 (Opto/FET/Tape/SynthFX) | 완료 | `src/dsp/ExpansionRack.h`, plugin `expansionMode`/`expansionAmount`/`expansionMix`, CLI `--expansion`, `--expansion-amount`, `--expansion-mix` |
| Tier 4+ 별도 제품 패키징 (Compressor/Tape) | 완료 | `src/plugin/CMakeLists.txt`의 `ValvraCompressorPlugin`, `ValvraTapePlugin` |

## 4. 주요 갭

### A. Chain Builder UI

문서 요구:

- 1~4단 체인 자유 구성
- 각 단별 tube/topology/bias 선택
- input/output transformer 선택
- 4개 모드는 프리셋 체인으로 로드

현재:

- 내부 `TubeAmpChain`은 최대 4단을 처리한다.
- 플러그인 UI는 4개 고정 모드와 각 모드의 `Input -> Stage(s) -> Output` 체인 가시화를 제공한다.
- 사용자는 `Stage Count`, `Input Transformer`, `Output Transformer`를 직접 편집할 수 있다.
- **신규: `StageEditorPanel`** — Stage 1~4를 선택해 각 단의 tube (12AX7 RSD-1/RSD-2/EHX, 12AU7), topology (Common Cathode / Cathode Follower), drive trim (±12 dB), bias offset (±0.8 V) 직접 편집. 각 컨트롤은 별도 AVPTS 파라미터로 노출되어 호스트 자동화 가능.

판정: **완료**

### B. 실제 토폴로지

문서 요구:

- Common-cathode
- Cathode follower
- SRPP
- LTP
- Cascode

현재:

- Common-cathode, Cathode follower **(완료)**.
- **LTP/ideal phase split selectable + class-AB push-pull power stage** — `PushPullStage` 클래스가 ideal anti-phase 또는 LTP 경로(`useLtpPhaseSplitter`)로 grid drive를 생성하고, 두 power triode + shared cathode/tail resistor 결합을 sample-by-sample Newton-Raphson으로 풀어낸다. EL34/6L6GC triode-strapped Dempwolf 파라미터 추가 (`params::kEL34_TriodeStrapped`, `params::k6L6GC_TriodeStrapped`).
- Marshall 모드는 PP+EL34 출력단으로 교체됨. H3가 H2를 압도하는 class-AB 시그니처가 명확히 나타난다.
- **신규: SRPP (Shunt Regulated Push-Pull)** — TubeStage 내부에서 두 triode가 직렬 stack, V_mid 노드에 대한 Newton-Raphson 풀이로 KCL 보존. Upper grid가 lower plate를 따라가서 Vgk_upper가 거의 일정 → upper가 high-impedance current source처럼 동작 (active load). Output은 V_mid junction.
- **신규: Cascode** — 같은 stack 구조에 upper grid는 fixed bias. Output은 upper plate (Vb − Ip·Rp_upper). Lower plate가 V_mid에 묶여 거의 흔들리지 않으므로 Miller multiplication 자동 억제 → 넓은 대역폭.
- Setup 시 (V_mid, Vk_lower) joint fixed-point iteration으로 self-consistent 동작점 산출.

판정: **완료** — Common-Cathode / Cathode Follower / SRPP / LTP / Cascode가 모두 구현되어 있으며, Marshall 경로에서도 LTP split 경로를 선택해 사용할 수 있다.

### C. 튜브/하드웨어 정확도

문서 요구:

- 12AX7, 12AU7, 6SN7, EL34, 6L6GC, 300B
- V72, Marshall, Culture Vulture, RNDI에 맞춘 회로 근거

현재:

- 12AX7 (RSD-1, RSD-2, EHX 3종 — Dempwolf 2011 measured), 12AU7 (Koren ECC82 fit) **(완료)**
- **EL34 / 6L6GC triode-strapped** Dempwolf 파라미터 (실제 push-pull stage에서 활용)
- **신규: 6SN7 / 300B / EF86 (triode-strapped)** Dempwolf 파라미터 — Koren 모델 데이터시트 fit → Dempwolf softplus 형식 매핑. 6SN7 (μ≈20, HiFi smooth), 300B (μ≈3.85, legendary HiFi power triode), EF86 (μ≈38, Vox AC30 input).
- Per-stage editor에서 "Preset / 12AX7 RSD-1 / RSD-2 / EHX / 12AU7 / 6SN7 / 300B / EF86 (triode) / EL34 / 6L6GC" 선택 가능.
- 6AS6는 초기 variable-mu proxy 구현에서 확장되어, 현재는 `KorenPentode` 기반 full pentode 경로(screen/suppressor 포함)로 동작한다. Culture Vulture T/P1/P2는 triode-strap/pentode 전환, screen supply RC, suppressor bias/drive law를 실제로 분기한다.
- EF86 입력단도 full pentode 경로로 구성되어 Culture Vulture 체인의 stage-level screen droop/suppressor 응답이 샘플 단위로 반영된다.

판정: **구현 완료** (12AX7×3, 12AU7, EL34, 6L6GC, 6SN7, 300B, EF86, 6AS6, Culture Vulture T/P1/P2 + full pentode screen/suppressor solver).

### D. Signature UI 6종

문서 요구:

- B-H Loop
- Harmonic Meter
- Null Test
- Warmup Simulation
- Drift Recorder
- Reroll Timeline

현재:

- B-H Loop, Harmonic Meter, Null Test, Reroll button **(완료)**
- **신규: Warmup 트리거 버튼** — `triggerWarmup()` → 모든 stage의 `simulateWarmup()` 재실행, 30초 워밍업 envelope 재시작.
- **신규: Drift Recorder** — `DriftRecorderView`가 60초 동안 PSU sag %, gm warmup %, thermal bias drift V를 동시 trace. processor가 매 processBlock 끝에 atomic 3개로 publish.
- **신규: Reroll Timeline** — 최근 10개 seed의 hex 16비트 fragment를 가로 셀로 표시. 클릭하면 `recallSeed()`로 즉시 그 시점의 캐릭터 복원. 현재 seed는 강조 표시.

판정: **완료**

### E. Mastering 기능

문서 요구:

- Mid/Side
- True Peak safety
- A/B compare
- Linear-phase option
- TPDF dither

현재:

- **True Peak safety:** ITU-R BS.1770-5 4× upsampled detection + lookahead brickwall limiter. Off/Soft/Brick-wall mode, -3.0~-0.1 dBTP ceiling, 1~10 ms lookahead, GR 미터, null-test 모드에서 자동 우회.
- **TPDF dither:** 16/20/24-bit 선택, 채널 독립 노이즈 스트림으로 mix-bus 합산 시 +3 dB 흡수.
- **A/B compare:** A/B 비교 재생 + C/D/E 스냅샷 뱅크 + A/B 워크플로우 전용 32-step Undo/Redo. A/B 전환은 integrated K-weighted LUFS 기준(±12 dB clamp)으로 매칭되며 graph rebuild는 10 ms fade-protected 전환으로 처리.
- **Mid/Side independent processing:** L+R 입력을 M=(L+R)/sqrt(2), S=(L-R)/sqrt(2)로 분해, 두 chain이 각각 처리, 다시 인코딩. M/S 모드에서는 두 chain이 동일 seed + 독립 PSU rail (shared-PSU coupling이 side를 mono 합에 누출시키지 않도록 자동 우회).
- **Linear-phase oversampling:** 이미 충족. `PolyphaseOversampler`는 Kaiser-windowed symmetric FIR (linear phase by construction). docs/20 §4.8의 "5~50 ms latency tradeoff" 옵션은 본 OS 구조에서는 불필요한 별도 토글로 판단 — OS factor 자체가 이미 latency tradeoff (1×=0ms, 16×=0.06ms).

판정: **완료** — TP / Dither / integrated-LUFS snapshot A/B / M/S / Linear-phase FIR와 관련 UI surface가 구현됨.

### F. Neural Foundation Layer

문서 요구:

- RTNeural 통합
- Neural Blend
- 물리 모델 residual 학습

현재:

- RTNeural JSON 모델 로드/언로드 경로 구현 (`loadNeuralModelFile`, UI `Load NN`/`Unload NN`)
- Neural Blend 파라미터/슬라이더 구현 (`neuralBlend`)
- 모델 핫스왑 시 neural branch만 10 ms 크로스페이드 처리
- 모델 경로 project state 저장/복원 (`valvra_neural_model_path`)
- Bootstrap residual MLP fallback 경로 유지
- Open-Amp/Wright 기반 학습 플레이북 및 스크립트 추가 (`docs/30`, `scripts/train_neural_residual.py`)

판정: **구현 완료(학습 데이터/모델 자산은 운영 과제)**

## 5. 추천 작업 순서

1. ~~README와 문서 상태 정합성 유지~~
2. ~~Chain Builder UI 최소판~~ → per-stage 편집까지 포함해 완료 (2026-04-26 추가 패치)
3. ~~Marshall Output Stage 개선~~ → `PushPullStage` + `TubeStage::LongTailedPair` 경로 통합 완료 (ideal/LTP split selectable, class-AB EL34 pair, tail-resistor Newton-Raphson)
4. ~~Warmup/Sag/Bias/Drift 상태 UI 노출~~ → DriftRecorderView + Warmup 트리거 + Reroll Timeline 완료 (2026-04-26 추가 패치)
5. Dempwolf 단일-stage 하모닉 정량 검증 강화
6. ~~CPU benchmark 추가~~ → `valvra_bench` + 베이스라인 [docs/27](27-bench-baseline-2026-04-27.md) 완료 (2026-04-27 추가 패치). Marshall이 4× stereo에서 51% CPU로 최대 핫스팟; PP solver 분석적 fprime이 다음 최적화 후보.
7. ~~SRPP/Cascode~~ → 둘 다 완료 (V_mid Newton-Raphson 솔버, 2026-04-26 추가 패치). 6SN7/300B/EF86/6AS6와 EL34/6L6GC triode-strapped 모델도 추가됨. Culture Vulture T/P1/P2 모드 전환도 완료.
8. Mastering 기능 → True Peak limiter, TPDF dither, integrated-LUFS snapshot A/B compare, GR meter, Mid/Side 분리 처리, 16x oversampling, Monte Carlo Lock/distribution 프리셋 구현. Linear-phase는 기존 Kaiser FIR로 충족.
9. Neural Foundation Layer → 모델 학습/큐레이션 파이프라인 운영 고도화

## 6. 다음 구현 기준

Tier 3 구현은 완료로 닫고, 다음 패치의 현실적인 목표는 v1.0 운영 게이트 검증(M3)이다.

최소 수용 기준:

- Neural Foundation Layer의 문서/코드 상태를 일치시키고, 모델 로드/핫스왑/블렌드 경로 회귀 테스트를 유지한다.
- Console Output LTP 통합의 하드웨어 정합도 검증(청감/계측)을 운영 게이트로 관리한다.
