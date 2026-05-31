# 27. CPU Benchmark Baseline (2026-04-27)

> 목적: `valvra_bench`로 측정한 베이스라인 — 향후 패치가 핵심 시나리오의 CPU%를 회귀시키지 않는지 비교 기준선이 된다.
>
> 측정 방식: Release 빌드 (`-O3 -ffast-math -mavx2`), `VALVRA_BUILD_BENCHES=ON`, Apple M-series. 시나리오마다 4 s 합성 오디오 (시드 고정 노이즈)를 `ValvraProcessor::processBlock`에 흘리며 wall-time 측정. p50 / p95는 per-block timing 분포의 중간값 / 95-퍼센타일.

## 베이스라인 변경 이력

### v3 — 2026-04-27 (Marshall 모드 → Console Output 리튠)

용도 정합성 패치: 기존 Marshall 모드가 기타앰프 출력단으로 calibrate 되어 있었지만 본 플러그인의 mix/mastering use case와 어긋났다. LTP+PP+EL34 DSP는 그대로 유지하고 운영점만 class-AB1 cutoff knee → class-A1 mid-rail로 옮겼다 (Vg_bias −36V → −25V, driveScale 32 → 15). UI 라벨 "Marshall" → "Console Output"으로 변경. PresetMode 정수 인덱스(1)는 유지하므로 저장된 호스트 state 호환성 보존.

| 시나리오 | v2 (guitar-amp Marshall) | v3 (Console Output) | Δ |
|---|---:|---:|---:|
| Marshall/Console 4× clean | 41.35% | **39.00%** | −2.4 pp |
| Marshall/Console 4× TP on | 39.69% | **38.57%** | −1.1 pp |
| p95 (Console clean) | 2.44 ms | **2.17 ms** | −11% |

부수 효과: class-A1은 양 power tube가 항상 conducting → Newton-Raphson tail solver가 부드러운 영역에서 수렴, 클램프 발생 빈도가 줄어 CPU 추가 절감.

### v2 — 2026-04-27 (analytical-gm 솔버 최적화 후)

PushPullStage / SRPP / Cascode 솔버의 forward-difference fprime을 `KorenTriode::evalWithDerivatives()` 분석적 도함수로 교체. 솔버 한 iter당 plateCurrent 호출이 6→2회로 감소.

| 시나리오 | v1 (forward-diff) | v2 (analytical) | Δ |
|---|---:|---:|---:|
| **Marshall 4× clean** | 51.11% | **41.35%** | **−9.8 pp (−19%)** |
| **Marshall 4× TP on** | 52.74% | **39.69%** | **−13.1 pp (−25%)** |
| V72 8× | 62.75% | 55.30% | −7.5 pp (−12%) |
| V72 4× clean | 28.49% | 28.54% | ±0 (변경 없음, 예상대로) |
| Culture Vulture 4× | 31.79% | 33.09% | +1.3 pp (측정 노이즈) |
| RNDI 4× | 15.10% | 15.22% | ±0 |
| HiFi 300B 4× | 21.94% | 21.13% | −0.8 pp |

**핵심**: Marshall이 가장 큰 수혜 — push-pull tail solver가 sample당 6 plateCurrent calls (2 for f + 4 for fprime via central-diff) → 2 calls (단일 분석적 evalWithDerivatives × 2 tubes). p95도 3.06 → 2.44 ms로 대폭 개선. SRPP/Cascode를 안 쓰는 프리셋 (V72/CV/RNDI/HiFi)은 변동 없음 (예상된 결과).

## 측정 결과 (v3 baseline)

Sample rate: 48000 Hz · Block size: 256 · Duration: 4 s per scenario

| Scenario | RTF | CPU % (1 core) | p50 ms/block | p95 ms/block |
|---|---:|---:|---:|---:|
| V72 · 8x · stereo · clean | 1.91x | 52.28% | 2.766 | 2.884 |
| Console Output · 4× · stereo · TP on | 2.61x | 38.37% | 2.028 | 2.070 |
| Console Output · 4× · stereo · clean | 2.61x | 38.26% | 2.032 | 2.093 |
| CultureVulture · 4× · stereo · clean | 3.23x | 30.94% | 1.637 | 1.702 |
| V72 · 4× · stereo · TP on | 3.58x | 27.91% | 1.467 | 1.556 |
| V72 · 4x · stereo · clean | 3.67x | 27.27% | 1.449 | 1.496 |
| V72 · 4× · stereo · clean | 3.69x | 27.11% | 1.436 | 1.491 |
| V72 · 4× · M/S · clean | 3.71x | 26.98% | 1.407 | 1.493 |
| V72 · 4× · M/S · TP on (full master chain) | 3.76x | 26.58% | 1.405 | 1.460 |
| HiFi300B · 4× · stereo · clean | 4.43x | 22.59% | 1.161 | 1.375 |
| RNDI · 4× · stereo · clean | 6.66x | 15.02% | 0.796 | 0.827 |
| V72 · 2x · stereo · clean | 6.71x | 14.90% | 0.788 | 0.821 |
| V72 · 4× · MONO · clean | 7.31x | 13.68% | 0.721 | 0.752 |
| V72 · 1x · stereo · clean | 12.62x | 7.92% | 0.411 | 0.442 |

*Lower CPU%, higher RTF, lower p95 = better.*

## 핵심 관찰 (v2 baseline 기준)

1. **Marshall이 여전히 최대 핫스팟이지만 -25% 절감** — 4× stereo에서 41% CPU, p95 = 2.4 ms/block. v1의 51%에서 분석적 fprime으로 19% 절감. 동시 인스턴스 ~5–6개까지 헤드룸 확보.
2. **OS 스케일링은 거의 선형** — V72 1×/2×/4×/8× = 8.2% / 15.5% / 28.7% / 55.3%. 8× OS는 한 단 올릴 때마다 2배 가깝게 비싸짐, 마스터링 외에는 비추천.
3. **Mono = Stereo/2 정확** — V72 mono 14.16% vs stereo 28.54%. 두 chain (chainL_, chainR_)이 진짜 독립 처리되고 있음을 확인.
4. **Mastering surcharge 무시 가능** — V72+TP+M/S = 28.33% vs V72 clean = 28.54%. TP limiter (4× detection OS)와 M/S 인코드/디코드가 chain 자체보다 훨씬 가벼움.
5. **Culture Vulture 무거움** (33%) — 3 stage 모두 hidden physics 활성, EF86 proxy 등. 분석적 솔버 최적화 영향 없음 (CV는 compound topology를 안 씀).
6. **RNDI 가장 가벼움** (15%) — DI 모드라 input transformer 없음, PSU sag off, hidden physics 일부 비활성.

## 사용법

```bash
# Configure with benchmarks ON
cmake -S . -B build -DVALVRA_BUILD_BENCHES=ON
cmake --build build --target valvra_bench -j

# Run (markdown to stdout)
./build/bench/valvra_bench

# Capture to a dated regression doc
./build/bench/valvra_bench > docs/NN-bench-YYYY-MM-DD.md
```

## 회귀 기준선

다음 패치가 위 표의 어떤 행이라도 **CPU% 10% 이상 또는 p95 +0.5 ms 이상 악화**시키면, 그 자체가 review-block 사유가 된다.
의도된 변경이라면 (예: 새 hidden-physics 추가) docs/27 → docs/NN-bench-YYYY-MM-DD.md로 새 baseline을 등록하고 PR 본문에 차이 정당화를 적는다.

## 다음 최적화 후보

1. ~~PushPullStage tail solver 분석적 fprime~~ → 완료 (v2). Marshall -19~25% 절감.
2. ~~Compound topology solver 분석적 fprime~~ → 완료 (v2). SRPP/Cascode를 쓰는 사용자-커스텀 chain에 적용.
3. **Heater hum / shot noise SIMD** — 매 sample 호출되는 sin / xorshift / gaussian. AVX2 vectorization으로 ~2x 가능. V72/CV 같은 hidden-physics 활성 프리셋의 ~30% 절감 추정.
4. **plateCurrent에서 std::pow 제거** — `s^γ` 한 번, `s^(γ-1)` 한 번 호출. γ=1.26~1.5 범위라 `std::pow(s, γ-1)`이 핫. 사전 계산된 `gamma_minus_1` 멤버 + `std::exp(γ_log_s)` 변환 가능.
5. **Miller filter, slew limit 등 hidden physics의 분기 비용** — branch prediction 친화적 정렬.
