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

