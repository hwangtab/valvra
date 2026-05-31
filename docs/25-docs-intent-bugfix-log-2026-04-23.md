# Docs-Intent 버그 수정 내역 (2026-04-23)

## 1) 목적
- 기준 문서(`docs/20`, `docs/24`)와 코드 동작이 어긋난 **의도불일치 버그(미구현 제외)**를 수정한다.
- 이번 수정은 API/파일 포맷 변경 없이 런타임 동작과 회귀 테스트 보강에 집중한다.

## 2) 수정한 버그

### A. Marshall 모드 PSU sag 의도 불일치
- 문서 의도: Marshall은 SS rect 기반의 **약하지만 0은 아닌 sag**.
- 기존 코드: `enablePSUSag = false`로 사실상 sag 비활성.
- 수정: `enablePSUSag = true`로 변경.
- 변경 위치:
  - `src/dsp/TubeAmpChain.h` (`chain_presets::MarshallMode`)

### B. Marshall 모드 cathode bounce 의도 불일치
- 문서 의도: Marshall fixed-bias 성격으로 cathode bounce 없음.
- 기존 코드: stage 설정상 cathode bounce 경로가 활성 상태로 동작 가능.
- 수정:
  - `TubeStageConfig`에 `enableCathodeBounce` 플래그 추가(기본 `true`)
  - `marshallStage1`, `marshallStage2`에서 `enableCathodeBounce = false`
  - DSP 처리 경로에서 플래그 기반 분기 적용
- 변경 위치:
  - `src/dsp/TubeStage.h`

### C. RNDI 모드 cathode bounce 의도 불일치
- 문서 의도: RNDI(DI) 모드에서 cathode bounce 없음.
- 기존 코드: stage2가 `v72Stage2()` 재사용되어 bounce 성격이 섞일 수 있음.
- 수정: `RNDIMode`에서 stage2에 `enableCathodeBounce = false` 명시.
- 변경 위치:
  - `src/dsp/TubeAmpChain.h` (`chain_presets::RNDIMode`)

## 3) 코드 변경 상세

### TubeStage 경로 분기
- `enableCathodeBounce`가 `false`이면:
  - 바이어스 계산 시 `bounce_.currentBias()` 대신 `Vk_rest_` 사용
  - 상태 업데이트 시 `bounce_.process(Ip)` 대신 `Vk_rest_` 유지
- 효과:
  - cathode-bypass 메모리(느린 bias wobble) 비활성화
  - 기존 기능과의 호환을 위해 기본값은 `true` 유지

## 4) 회귀 테스트 추가/보정

### 추가된 테스트 (`tests/test_tube_amp_chain.cpp`)
1. `TubeAmpChain: Marshall preset keeps PSU sag enabled`
2. `TubeAmpChain: Marshall preset disables cathode bounce on all stages`
3. `TubeAmpChain: RNDI preset disables cathode bounce on stage 2`

### 보정된 기존 테스트 (`tests/test_tube_stage.cpp`)
- `TubeStage: grid conduction leaves a post-burst recovery tail`
- 이유: Marshall preset 의도 변경(기본 bounce off)으로 테스트 전제가 바뀌어,
  해당 테스트 내부에서만 `cfg.enableCathodeBounce = true`를 명시해
  grid-conduction 검증을 독립적으로 유지.

## 5) 검증 결과
- 일반 빌드:
  - `cmake --build build -j4` 성공
  - `ctest --test-dir build --output-on-failure` 성공 (81/81)
- sanitizer 빌드:
  - `cmake --build build-sanitizer -j4` 성공
  - `ctest --test-dir build-sanitizer --output-on-failure` 성공 (80/80)

## 6) 변경 파일 목록
- `src/dsp/TubeStage.h`
- `src/dsp/TubeAmpChain.h`
- `tests/test_tube_amp_chain.cpp`
- `tests/test_tube_stage.cpp`


---

## Addendum (2026-04-27): Marshall 모드 → Console Output 리튠

### 7) 의도 변경의 배경
- 사용자가 본 플러그인의 정체를 재확인 — **mix / mastering용 진공관 색깔 프로세서**이지 기타앰프 시뮬레이션이 아님.
- 기존 Marshall 모드는 §2-A/B의 fixed-bias / cathode-bounce off 정정 이후에도 운영점이 **class-AB1 cutoff knee** (Vg_bias = −36 V) 에 앉아 있어, 디폴트 Drive=1.0에서 H3 dominant 기타-크런치 톤이 나왔다. 이는 mix/master use case와 어긋남.

### 8) 수정 내용
- **회로 / DSP는 전부 유지** — `PushPullStage` (Newton-Raphson tail solver), `params::kEL34_TriodeStrapped`, UTC OPT, 분석적 gm 솔버 모두 그대로.
- **운영점만 mix/master 정합으로 이동**:
  - `pp.Vg_bias`: −36 V → **−25 V** (class-A1 mid-rail)
  - `pp.driveScale`: 32 → **15 V/unit**
  - `outputTrafoConfig.drive`: 0.8 → **0.55**
  - `marshallStage1/2` `Vg_bias` / `inputVoltageSwing` 도 mix-friendly로 재조정
  - `enableCathodeBounce`: false → **true** (느린 envelope dynamics 활성)
  - `enableMicrophonics`: true → **false** (chassis 아닌 rack-mount 가정)
- **UI 라벨**: "Marshall" → **"Console Output"** (`createLayout`, `presetBox`, `ChainBuilderView` 모두 갱신)
- **`PresetMode::Marshall` → `PresetMode::ConsoleOutput`** — 정수 인덱스(1) 유지 → 저장된 호스트 state 호환성 100% 보존.

### 9) 결과
- Drive=1.0: H2 dominant 짝수배음 워밍 (mix-friendly, drum bus / master glue)
- Drive ≥ 2.5: H3 dominant class-AB cutoff (guitar coloration on tap)
- CPU: 41.35% → **38.26%** (-2.4 pp 추가 절감 — class-A1 솔버 수렴이 더 부드러움)

### 10) 회귀 테스트 보정
- `TubeAmpChain: Marshall preset disables cathode bounce on all stages` →
  `TubeAmpChain: Console Output preset uses class-A1 push-pull power stage` 로 의도 변경 반영. 새 검증 항목:
  - `enableCathodeBounce == true` (was false)
  - `usePushPullOutputStage == true`
  - `pushPullConfig.Vg_bias ∈ (−30, −20)` V — class-A1 mid-rail
  - `pushPullConfig.driveScale < 25` V/unit — 부드러운 디폴트
- 기존 feature 테스트 3개 (`grid conduction`, `thermal drift`, `microphonics`) — `marshallStage1/2`의 옛 hot-drive 운영점에 의존하던 부분을 **자체 hot config 구성**으로 분리. preset의 voicing 결정과 디커플링.

### 11) 변경 파일 목록 (Addendum)
- `src/dsp/TubeAmpChain.h` (`MarshallMode()`)
- `src/dsp/TubeStage.h` (`marshallStage1`, `marshallStage2`)
- `src/plugin/PluginProcessor.cpp` (Mode StringArray)
- `src/plugin/PluginEditor.cpp` (presetBox, `chainLabelsForPreset`)
- `src/plugin/FactoryPresets.h` (`PresetMode` enum, factory preset drive 재조정)
- `tests/test_tube_stage.cpp` (3 feature 테스트 디커플링)
- `tests/test_tube_amp_chain.cpp` (intent 테스트 갱신)
- `docs/00-README.md`, `docs/20-mvp-scope-decision.md`, `docs/24-tube-amp-target-hardware.md`, `docs/26`, `docs/27` (모드 표기 / 사용 정체 정합)

### 12) 검증 결과
- `cmake --build build -j4` 성공
- `ctest`: 114/114 통과
- `python3 scripts/audit_checks.py`, `python3 scripts/validate_harmonics.py` 통과
- `python3 scripts/preset_harmonic_report.py` — Console Output (mode 인덱스 1) 의 신규 하모닉 프로파일 확인됨
