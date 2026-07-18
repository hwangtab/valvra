# TubeAmp / Valvra — 센세이셔널 진공관 앰프 플러그인 연구

> **제품 정체:** **Tube Amp Coloration Plugin** — 진공관 + 트랜스포머를 통과한 효과를 재현. EQ 아님. 디지털 믹스의 "차갑고 납작한" 사운드를 "살아있는 아날로그"로 변환.

> **상태 (2026-05-09):** 🟢 **Tier 4+ 코어 구현 완료** — Tier 3 + Expansion 엔진 코어 + 별도 Compressor/Tape 타깃까지 반영됨. 현행 MVP 가이드: [20 MVP v2.0](./20-mvp-scope-decision.md) + [24 진공관 앰프 타깃](./24-tube-amp-target-hardware.md). 실제 구현 갭 기준선: [26 구현 갭 감사](./26-implementation-gap-audit-2026-04-26.md).

> **사용 정체:** 본 플러그인은 **믹싱 / 마스터링용 진공관 색깔 프로세서**다. 5개 모드 (V72 Preamp · Console Output · Culture Vulture · RNDI DI · HiFi 300B SE) 모두 mix-bus / 트랙 / 마스터 버스 워크플로 정합. 24번 문서가 *물리적 회로 참조* 로 Marshall JCM800 / Fender Twin / Vox AC30 같은 하드웨어를 인용하는 것은 운영점·트랜스포머·전원부 calibration의 출처이지, 본 플러그인의 사용 시나리오가 기타앰프 시뮬레이션이라는 뜻이 아니다.

> **핵심 차별점 (시중 UAD/Waves/Softube/Slate/Soundtoys 전원 없음):** Jiles-Atherton 히스테리시스 · 캐소드 바운스 · PSU Sag · 열 드리프트 · Monte Carlo 개체차 · 실시간 B-H 루프 시각화 · Null Test 내장 · 다단 체인 빌더 · Neural Foundation Layer

---

## 차별화의 4축

| 축 | 핵심 질문 | 관련 문서 |
|----|---------|---------|
| **초정밀 회로 모델링** | 진공관과 트랜스포머를 SPICE 수준으로 정확히 시뮬레이션할 수 있는가? | 01, 02, 04, 07, 14 |
| **시변 다이나믹** | 시간이 지남에 따라 소리가 미묘하게 변하는 "살아있음"을 구현할 수 있는가? | 03, 04 |
| **확률적 개체차** | 각 플러그인 인스턴스가 서로 미세하게 다른 개성을 가질 수 있는가? | 06 |
| **신호 의존 적응** | 입력 신호의 특성(레벨, 주파수, 밀도)에 따라 회로 응답이 변하는가? | 03, 04, 05 |

---

## 문서 맵 (30개)

### Part 1 — 물리·이론 기반
- **[01 진공관 물리](./01-vacuum-tube-physics.md)** — Child-Langmuir, Koren 모델(Ex 지수 포함), 빔 사극관
- **[02 트랜스포머 왜곡](./02-transformer-physics-and-distortion.md)** — Jiles-Atherton 완전판, 히스테리시스 RK4
- **[03 시변 비선형성 ★★★](./03-time-varying-nonlinearities.md)** — 열적 드리프트, 캐소드 바운스, Volterra
- **[04 임피던스 상호작용](./04-circuit-interactions-and-impedance.md)** — Miller, 단 간 임피던스 대화
- **[05 배음 & 심리음향](./05-harmonic-spectrum-and-psychoacoustics.md)** — 짝수/홀수 배음의 청각적 의미

### Part 2 — 설계 기반
- **[06 확률적 개체차 ★★★](./06-stochastic-component-modeling.md)** — Monte Carlo, per-instance seed
- **[07 구현 전략](./07-implementation-strategies.md)** — WDF(trapezoidal/BE 비교), DK, Neural, 수치 안정성(§7.5, 7.6)
- **[14 회로 토폴로지 보완](./14-missing-circuit-topologies.md)** — LTP, SRPP, UL, Cascode, Phase splitter 등
- **[30 Neural Training Playbook](./30-neural-training-playbook.md)** — Open-Amp/Wright 기반 residual 학습 경로

### Part 3 — 스코프·타깃
- **[24 진공관 앰프 타깃 하드웨어 ★★★](./24-tube-amp-target-hardware.md)** 🟢 **현행 MVP 기준** — 5개 모드 (V72 / Console Output / Culture Vulture / RNDI / HiFi 300B SE) 의 *물리적 회로 참조*. Console Output은 Marshall JCM800 / Fender Twin 같은 power-tube 출력단에서 운영점만 mix/master 친화로 옮긴 voicing.
- **[11 타깃 하드웨어 카탈로그](./11-target-hardware-catalog.md)** — 일반 참조 (Neve, Pultec, Fairchild, LA-2A, 1176, Manley 등 10종)
- **[12 진공관 너머의 비선형성](./12-analog-beyond-tubes.md)** — Opto, FET, Tape, Diode, Op-amp
- **[13 마스터링 기능](./13-mastering-features.md)** — M/S, True Peak, Linear-phase, A/B, LUFS

### Part 4 — 검증
- **[08 경쟁 분석](./08-competitive-analysis.md)** — UAD/Waves/Slate 기술 한계 매트릭스
- **[09 측정 방법론](./09-measurement-and-validation.md)** — Null test, THD, Phase/GD, True Peak, LUFS, Farina
- **[18 측정 인프라](./18-measurement-infrastructure.md)** — BOM, 하드웨어 확보, Multi-unit 프로토콜

### Part 5 — 실무·제품화
- **[15 사용 시나리오 가이드](./15-use-case-guide.md)** — Drum/Vocal/Bass/Master 파라미터 추천
- **[16 UI/UX 스펙](./16-ui-ux-spec.md)** — Easy/Standard/Expert 3모드, 시각화 패널
- **[28 사용자 매뉴얼 (KO)](./28-user-manual-ko.md)** — 기능/워크플로/문제해결
- **[29 User Manual (EN)](./29-user-manual-en.md)** — feature/workflow/troubleshooting

### Part 6 — 비즈니스·리스크
- **[17 법적 및 라이선스](./17-legal-and-licensing.md)** — 상표, GPL 감염, JUCE 라이선스 (AGPL-3)
- **[19 리스크 분석](./19-risk-analysis.md)** — 21개 리스크 Register, Pivot 시나리오

### Part 7 — 의사 결정 ★★★
- **[20 MVP v2.0 — 센세이셔널 진공관 앰프](./20-mvp-scope-decision.md)** 🟢 **현행 MVP** — 15개 전면 차별 기능, 4개 모드, Chain Builder, Neural Layer, 범위 축소 금지 선언
- **[23 비상업 오픈소스 리포지셔닝](./23-noncommercial-redirect.md)** — 라이선스/배포 전략 (GPL-3, $0 예산, 일정 유연)
- **[26 구현 갭 감사](./26-implementation-gap-audit-2026-04-26.md)** — 코드가 문서상 v1.0에 얼마나 근접했는지와 다음 작업 순서 (2026-04 기준)
- **[33 아날로그 물리 전면 정비](./33-analog-physics-overhaul-2026-06-12.md)** 🟢 **현행 구현 기준선** — 로드라인/CF/자속 트랜스포머/리저버 PSU/레일 래더/펜토드 보존 등 물리 재구현 기록 (2026-06-12)
- **[34 아날로그 물리 갭 감사 · 설계 보강안](./34-analog-physics-gap-audit-2026-07-03.md)** 🟢 **W1–W8 전 항목 구현 완료** — 33 이후 잔존 정합성 결함(P0) + 블록 경계 물리(NFB 실루프/OPT-플레이트 결합/AB 부하 킹크/파워단 블로킹) + 디바이스·아키텍처 보강, §7에 웨이브별 구현 로그 (2026-07-03~13)
- **[35 진행 전수감사 · 차기 개선전략](./35-progress-audit-and-strategy-2026-07-18.md)** 🟡 **차기 작업 기준** — W1–W8 완료 검증 + 신규 발견(CI nightly 실패·플러그인 미게이트, fit 게이트 자기참조성, 골든 렌더 부재) + S1–S4 전략(검증 인프라 → 물리 마감 → 보이싱/실측 → 성능) (2026-07-18)

### Part 8 — 실측·학술 데이터 아카이브 ★★★
- **[22 학술 논문 기반 정량 데이터](./22-academic-quantitative-data.md)** 🟢 **현행 기반** — Dempwolf 2011 12AX7 실측 파라미터, Ni-Permalloy JA 추정, 시변 효과 학술 수치, 공개 데이터셋 라이선스
- **[21 Pultec 측정 데이터](./21-pultec-measurement-archive.md)** 📦 *ARCHIVED* — Pultec EQ 특화. v1.0 범위 외. 트랜스포머·진공관 스펙은 참고용

### 레퍼런스
- **[10 용어집 & 참고문헌](./10-glossary-and-references.md)** — 한영 용어, 학술 논문 (DOI/URL 첨부), 오픈소스

★★★ = 가장 독창적인 차별점, 읽기 우선 권장

---

## 추천 읽기 순서

### 믹싱 엔지니어 (기술 배경 불필요)
1. **[20 MVP v2.0](./20-mvp-scope-decision.md)** — "어떤 플러그인인가" 한 눈에
2. **[24 진공관 앰프 타깃](./24-tube-amp-target-hardware.md)** — 5개 모드(V72 / Console Output / Culture Vulture / RNDI / HiFi 300B SE)의 회로 참조
3. **[05 배음 & 심리음향](./05-harmonic-spectrum-and-psychoacoustics.md)** — "왜 짝수 배음이 따뜻하게 들리는가"
4. **[08 경쟁 분석](./08-competitive-analysis.md)** — "왜 기존 플러그인이 부족한가"
5. **[15 사용 시나리오](./15-use-case-guide.md)** — 실제 워크플로 가이드

### DSP 개발자
1. **[01 진공관 물리](./01-vacuum-tube-physics.md)** — 수식 기반 모델 (Koren tiny-terror 라이브러리 B-column 기준)
2. **[02 트랜스포머](./02-transformer-physics-and-distortion.md)** — Jiles-Atherton 결합 dM/dH 형태
3. **[14 회로 토폴로지](./14-missing-circuit-topologies.md)** — LTP, SRPP, UL, Cascode
4. **[07 구현 전략](./07-implementation-strategies.md)** — WDF, Neural, 수치 안정성
5. **[09 측정 방법론](./09-measurement-and-validation.md)** — 검증 프로토콜
6. **[03 시변 비선형성](./03-time-varying-nonlinearities.md)** — 상태 변수 설계

### 프로덕트 매니저 / 창업자
1. **[20 MVP 스코프 확정](./20-mvp-scope-decision.md)** — 현재 진행 중인 MVP 결정 기록 (진공관 앰프 v2.0 기준)
2. **[19 리스크 분석](./19-risk-analysis.md)** — 프로젝트 전체 리스크 (P1: 시장 검증, GPL 감염, Burnout)
3. **[17 법률 및 라이선스](./17-legal-and-licensing.md)** — JUCE AGPL-3, chowdsp 모듈별 라이선스
4. **[11 타깃 하드웨어](./11-target-hardware-catalog.md)** — 하드웨어 선정 기준
5. **[13 마스터링 기능](./13-mastering-features.md)** — 마스터링 시장 진입
6. **[16 UI/UX 스펙](./16-ui-ux-spec.md)** — 제품 경험

### 측정 엔지니어 / QA
1. **[09 측정 방법론](./09-measurement-and-validation.md)** — 테스트 설계 (ITU-R BS.1770-5 기반)
2. **[18 측정 인프라](./18-measurement-infrastructure.md)** — 장비·프로토콜 (Phase 1 예산 $3,500)
3. **[06 개체차 모델링](./06-stochastic-component-modeling.md)** — 통계 검증

---

## 핵심 인사이트 요약

### 왜 기존 플러그인은 "납작하게" 들리는가

모든 아날로그 에뮬레이션 플러그인은 본질적으로 **시간에 무관한 함수** $y = f(x)$를 계산한다.

실제 진공관 앰프는 $y = f(x,\; t,\; T_{tube},\; V_{cathode}(t),\; M_{core}(t),\; \theta_{instance})$를 계산한다.

이 차이가 전부다.

### 본 프로젝트가 다른 이유

| 기존 | 본 프로젝트 |
|------|----------|
| $y = \tanh(\text{drive} \cdot x)$ | $y = \text{Koren}(x,\; V_{bias}(t),\; T(t))$ |
| 트랜스포머 = LPF + HPF | 트랜스포머 = Jiles-Atherton + 공진 + eddy loss |
| 모든 인스턴스 동일 | 각 인스턴스 Monte Carlo 개체차 |
| drive 높이면 모든 주파수 동일 | 주파수 의존적 비선형성 (중역 지배) |
| 사용 중 소리 고정 | 워밍업, 캐소드 바운스, PSU sag로 살아있음 |
| 진공관만 에뮬 | Tube + Transformer + (옵션) Opto/FET/Diode |
| Mix 용 only | Mix + Master (M/S, TP, LUFS, Linear-phase 지원) |

---

## 검증 상태 (v2.0.0, 2026-04-18)

본 문서군은 **2단계 팩트체킹**을 거쳤다:

| 클러스터 | 1차 검증 | 2차 검증 | 최종 등급 |
|---------|---------|---------|---------|
| 학술 인용 (10) | 13개 수정 | 6개 추가 수정 | **A** (93% 정확, 1개 경고) |
| 물리·수식 (01,02,07,14) | 구조적 오류 수정 | 소수점 수준 일치 확인 | **A+** (95%+) |
| 하드웨어 (11) | 주요 오류 수정 | 10/10 유닛 재검증 | **A** |
| 비즈니스 (17,18,19) | 라이선스 재확인 | 2026-04 기준 재확인 | **A** |
| 문서 간 일관성 | — | 5개 파일 추가 수정 | **A** (90%+) |
| 부가 (12,13,15,16) | 기본 검증 | 3개 A, 1개 A- | **A** |

**총 수정된 사실·수식 오류: ~50개**

### 인용 시 주의 사항

다음 항목은 "추정치" 또는 "1차 출처 추적 필요"로 주석 처리되어 있다. 인용 시 해당 문서의 경고 문구 확인 필수:

1. **RCA BA-6A 완전 BOM** ([11](./11-target-hardware-catalog.md)) — schematic 1차 출처 확인 필요
2. **T4 cell memory effect τ₃=10-60s** ([12](./12-analog-beyond-tubes.md)) — Bonhoeffer AES 2013 재확인 권장
3. **12AX7 파라미터 표준편차** ([06](./06-stochastic-component-modeling.md)) — 공식 논문 없음, "합리적 추정치"
4. **Neve 1073/API 312 하모닉 수치** ([05](./05-harmonic-spectrum-and-psychoacoustics.md)) — 복합 출처 경향치
5. **Rodríguez-Serrano 2021 MS-20 VCF 논문** ([10](./10-glossary-and-references.md)) — 1차 출처 미확인, 인용 금지
6. **APx555B 공식 가격** ([18](./18-measurement-infrastructure.md)) — 비공개, ±15% 편차
7. **Plugin Alliance/Waves 매출 점유율** ([19](./19-risk-analysis.md)) — 공식 통계 미존재

---

## 프로젝트 상태 (2026-04-18)

### 연구 단계 (완료)
- [x] **Phase 1** — 핵심 물리·DSP 이론 문서 10개 (01–10)
- [x] **Phase 2** — 기술 오류 수정 (Koren, JA, WDF, 수치안정성, 측정 확장)
- [x] **Phase 3** — 스코프·실무·비즈니스 문서 9개 (11–19)
- [x] **Phase 4 — 1차 팩트체킹** (15개 문서 수정, 학술 인용·라이선스·수치 확인)
- [x] **Phase 5 — 2차 팩트체킹** (10개 문서 추가 수정, Citation-Ready 판정)

### Phase 6 — MVP 스코프 확정 (완료 2026-04-18)
- [x] **MVP 타깃 결정:** Pultec EQP-1A (진공관 정체성 + 구현 가능성 + 차별화 공간)
- [x] **Tier 0–3 증분 개발 계획** (12개월, $5,800 예산)
- [x] **성공/중단 기준 명시**
- [x] [20 MVP 스코프 확정](./20-mvp-scope-decision.md) 문서화

### Phase 7 — Pultec 실측 아카이브 (완료 2026-04-18)
- [x] 5개 영역 병렬 웹 리서치 (EQ 커브, THD/하모닉, 트랜스포머, Koren, 경쟁 벤치마크)
- [x] Pulse Techniques 공식 스펙, Triad/Peerless 트랜스포머, 12AX7/12AU7 Koren B 계열 확정
- [x] 경쟁 8개 플러그인 모두 "시변·개체차 전무" 검증 → 차별화 정당성 확보
- [x] **9개 P1 측정 공백 식별** — 실물 1대(Warm Audio EQP-WA 권장) 측정 필요
- [x] [21 Pultec 측정 아카이브](./21-pultec-measurement-archive.md) 문서화

### Phase 8 — 학술 논문 정량 데이터 보강 (완료 2026-04-18)
- [x] 5개 병렬 학술 리서치 (진공관 논문, 실측 하모닉, 트랜스포머 재료, Pultec 회로, 시변 효과)
- [x] **Dempwolf 2011 12AX7 3개 실측 피팅 확보** (μ 17% 편차, 개체차 학술 근거)
- [x] **Gyraf G-Pultec BOM 완전 수집** (캡/인덕터/저항/동작점)
- [x] Ni-Permalloy/Mu-metal JA 파라미터 문헌 추정
- [x] 시변 효과 수치 확정 (Cathode bounce τ=37.5ms, PSU sag 3-15%, 열 드리프트 10-15s)
- [x] 공개 데이터셋 라이선스 분석 (Wright 2020, Open-Amp, NAM MIT)
- [x] [22 학술 정량 데이터](./22-academic-quantitative-data.md) 문서화
- [x] **실물 측정 없이 MVP 구현 가능 수준 도달** ✓

### Phase 9 — 비상업 오픈소스 리포지셔닝 (완료 2026-04-18)
- [x] 판매 포기 결정 → GPL-3 오픈소스 방향 전환
- [x] 예산 $5,800 → **$0** (JUCE Indie, 코드 서명, 변호사, 측정 장비 모두 제거)
- [x] 기술 범위 확장 (상업 MVP 1개 → Pultec + LA-2A + 1176 모두 가능)
- [x] [23 비상업 리포지셔닝](./23-noncommercial-redirect.md) 문서화
- [x] 20번 문서 Archive 처리 (참고용 보존)

### 구현 단계 (Tier 기반, [23번 문서 참조](./23-noncommercial-redirect.md))

**현행 MVP 타깃: 진공관 앰프 색깔 유닛** (v2.0, [20번 문서](./20-mvp-scope-decision.md))

**Tier 0 (Engine Proof, 2개월):** Koren + JA + 시변 5종 + 4× OS 단일 진공관 단 청취 가능  
**Tier 1 (Single Full Stage, 3개월):** Preamp 모드 완성 (V72 스타일), Monte Carlo 개체차, 기본 UI  
**Tier 2 (Chain Builder + Signature UI, 4개월):** 1~4단 체인 빌더, 4개 모드, **실시간 B-H 루프 시각화**, Harmonic meter, Null Test 버튼  
**Tier 3 (Neural + Mastering + Polish, 3~4개월):** Neural Foundation Layer, M/S, True Peak, Linear-phase, v1.0 출시

**성공 기준 (센세이셔널 유지):**
- Tier 1 (M1): 본인이 다른 진공관 프리앰프 플러그인 대신 이걸 3회 이상 선택
- Tier 2 (M2): 주변 프로듀서 3명 이상이 "첫 30초 사운드 변화"를 자발 언급
- Tier 3 (M3): GitHub 공개 후 "이걸 무료로 오픈소스로 풀었다니?" 반응 획득
- Dempwolf 2011 하모닉 프로파일 재현 오차 ≤ ±3 dB
- 시변·개체차 null test로 객관 증명 가능

---

## 기여 가이드라인

- 수식은 LaTeX 문법 사용 (`$inline$`, `$$block$$`)
- 모든 주장에 출처 명시 (학술 논문 DOI or 측정 데이터 출처)
- 구현 함의 섹션은 의사코드 또는 C++ 예시 포함
- 용어 처음 등장 시 한국어와 영문 병기
- 수치·가격·라이선스 변경이 잦은 항목은 "확인 날짜" 주석 필수
- 외부 인용 시 해당 문서의 "주의" / "출처 확인 필요" 주석 먼저 확인

---

## 문서 버전 이력

- **v1.0.0** (2026-04-17): 초기 10개 문서 작성 (01–10)
- **v1.1.0** (2026-04-17): 기술 오류 수정 + 9개 신규 문서 추가 (11–19). 총 20개 문서, ~10,500줄
- **v2.0.0** (2026-04-18): **2단계 팩트체킹 완료**. 총 ~50개 사실·수식 오류 수정. Citation-Ready 판정
  - **주요 수정:** Neve 1073 트랜스포머 모델 번호, Pultec 출시 연도, Fairchild 670 BOM, Manley Vari-Mu 연도 체계, Culture Vulture 3모드, JUCE 8 AGPL-3 전환, chowdsp_utils 모듈별 라이선스, ITU-R BS.1770-5 (2023-11), CA/B Forum 코드 서명 460일, Hawley 2019 저자 수정, Yeh-Smith DAFx-06 연도 정정
- **v2.1.0** (2026-04-18): **MVP 스코프 확정** — [20 MVP Scope Decision](./20-mvp-scope-decision.md) 문서 추가. Pultec EQP-1A 타깃, Tier 0–3 증분 개발, 12개월 일정, $5,800 예산 확정. 연구 → 개발 전환 완료.
- **v2.2.0** (2026-04-18): **Pultec 측정 아카이브** — [21 Pultec Measurement Archive](./21-pultec-measurement-archive.md) 문서 추가. 5개 영역 병렬 웹 리서치로 공개 측정 데이터 통합. **핵심 발견:** 경쟁 8개 제품 모두 정적 LTI 필터 + 정적 waveshaper로 시변·개체차 전무 → 본 MVP 차별화 시장 기회 검증. 9개 P1 측정 공백 식별 (실물 측정 권장). 12AX7/12AU7 Koren 파라미터 B 계열 확정.
- **v2.3.0** (2026-04-18): **학술 논문 정량 데이터 아카이브** — [22 Academic Quantitative Data](./22-academic-quantitative-data.md) 문서 추가. 실물 측정 불가 상황을 학술 문헌으로 대체. **핵심 성과:** Dempwolf 2011 12AX7 실측 파라미터 3종 (RSD-1, RSD-2, EHX-1), μ 17% 편차 증거, Rutt 1984 grid current 실측 fit, Gyraf BOM 완전 BOM, Ni-Permalloy/Mu-metal JA 추정, Jones/Blencowe 시변 효과 정량치, NAM MIT 통합 전략. **실물 측정 없이 MVP 구현 가능한 정량 수준 도달.**
- **v2.4.0** (2026-04-18): **비상업 오픈소스 리포지셔닝** — [23 Non-Commercial Redirect](./23-noncommercial-redirect.md) 문서 추가. 판매 포기 → GPL-3 오픈소스. **해방 효과:** 예산 $5,800 → $0, JUCE GPL-3 무료, chowdsp GPL 모듈 자유 사용, 일정 유연, 기술 범위 확장. P1 리스크 3개 → 1개(Burnout) 감소.
- **v3.0.0** (2026-04-19): **🔥 방향 전면 재설정 — 진공관 앰프로 복귀.** 사용자 원 요청 재확인 결과 v1.0~v2.4의 "Pultec EQP-1A 이퀄라이저" 타깃이 **부적합**으로 판명. 원 요청은 "진공관 앰프 통과 효과 = 색깔·포화·드라이브". **수정 내용:** (1) [20 MVP v2.0](./20-mvp-scope-decision.md) 전면 재작성 — 센세이셔널 진공관 앰프 색깔 유닛, 15개 전면 차별 기능, 범위 축소 금지 선언, (2) [24 진공관 앰프 타깃](./24-tube-amp-target-hardware.md) 신규 — V72/Marshall/Culture Vulture/RNDI 4개 모드 회로 분석, (3) 21번·22번·23번에 방향 정정 주석 추가, 21번 ARCHIVED 표시. **핵심 차별 15종:** Jiles-Atherton 히스테리시스 + 캐소드 바운스 + PSU Sag + 열 드리프트 + Monte Carlo 개체차 + Reroll + 실시간 B-H 루프 시각화 + Harmonic meter + Null Test 버튼 + Warmup Simulation + Drift Recorder + Reroll Timeline + 1~4단 Chain Builder + 5개 토폴로지 + Neural Foundation Layer. DSP 코어 5종 (KorenTriode, JilesAtherton, CathodeBounce, PowerSupplySag, ComponentVariation) 그대로 재사용.
- **v3.1.0** (2026-04-27): **모드 정합성 패치.** (1) 4개 → **5개 모드**: HiFi 300B SE (audiophile mastering 전용, 6SN7 → 6SN7 follower → 300B SE → Lundahl OPT) 추가. (2) **Marshall → Console Output 리튠**: 본 플러그인은 mix / mastering용이지 기타앰프 시뮬레이션이 아님이 확인됨. LTP+PP+EL34 회로(Newton-Raphson tail solver, EL34 triode-strap Dempwolf 모델, UTC OPT)는 **DSP 그대로 유지**, 운영점만 class-AB1 cutoff knee → class-A1 mid-rail로 이동 (Vg_bias −36 → −25 V, driveScale 32 → 15). 결과: default Drive=1.0에서 mix-friendly 짝수배음 워밍, Drive ≥ 2.5에서 class-AB로 진입해 기타 크런치 캐릭터도 그대로 사용 가능. UI 라벨 "Marshall" → "Console Output". `PresetMode` 정수 인덱스(1)는 유지 → 저장된 호스트 state 100% 호환. (3) 분석적 gm 솔버 최적화 (`KorenTriode::evalWithDerivatives()`)로 PushPull / SRPP / Cascode 솔버의 plateCurrent 호출 6→2회. Console Output CPU 51% → 38% (-25%). (4) `bench/valvra_bench` 회귀 측정 인프라 추가 ([docs/27](./27-bench-baseline-2026-04-27.md)).

---

*최종 업데이트: 2026-04-27*
*문서 버전: 3.1.0 (5개 모드 + Console Output 리튠, mix/master use case 정합)*
