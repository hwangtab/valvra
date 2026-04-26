# 23. 개인용 오픈소스 리포지셔닝 — 상업 제약 해제

> **⚠️ 방향 정정 (2026-04-19):** 본 문서는 라이선스·배포 전략(GPL-3, $0 예산, 개인·주변 배포)은 유효하나, **타깃 제품은 Pultec EQ가 아니라 진공관 앰프 색깔 유닛**으로 바뀜. 현행 제품 스코프는 [20 MVP v2.0](./20-mvp-scope-decision.md)과 [24 진공관 앰프 타깃](./24-tube-amp-target-hardware.md) 참조.
>
> **유효한 부분:** GPL-3 선택, $0 예산, 일정 유연성, 상업 압박 부재, 주변 프로듀서 배포.
> **무효해진 부분:** "Pultec EQP-1A" 언급 전부. 체인 빌더·시그니처 UI는 20번 v2.0에 정식 포함되어 범위 축소 금지로 선언됨.

---

> **작성일:** 2026-04-18 · **상태:** 🟢 라이선스/배포 가이드 (제품 스코프는 20번 v2.0이 주)
> **결정권자:** 프로젝트 오너
> **연관 문서:** [17 법률](./17-legal-and-licensing.md) · [19 리스크](./19-risk-analysis.md) · [20 MVP v2.0 진공관 앰프](./20-mvp-scope-decision.md)

---

## 1. 핵심 패러다임 전환

### 1.1 사용자의 결정 (2026-04-18)
- ❌ **판매 목적이 아님**
- ❌ **어떤 라이선스/장비도 구매하지 않음**
- ✅ **개인 믹싱 용도 + 주변 프로듀서에게 무료 배포**
- ✅ **"최고의 플러그인"이라는 기술적 야심은 유지**

### 1.2 이 한 문장으로 바뀌는 것

**"상업이 아니므로 최고의 기술을 자유롭게 구현할 수 있다."**

|  | 기존 (상업) | 현재 (개인/배포) |
|---|---|---|
| JUCE 라이선스 | Indie $800 영구 구매 필수 | **GPL-3 무료** |
| chowdsp_utils GPL 모듈 | Clean-room 재구현 | **그대로 사용** |
| 상표 ("Pultec", "Neve") | 완전 회피 | **교육·비상업 fair use** |
| DRM | iLok/자체 구현 | **없음** |
| EULA / Privacy Policy | 변호사 $1,500 | **GPL-3 기본 조항** |
| 코드 서명 EV cert | Sectigo $280/년 | **ad-hoc 또는 self-signed** |
| Apple Developer | $99/년 | **필요 없음** (unsigned .pkg 배포) |
| 예산 | $10,629 | **$0** |
| 일정 압박 | 12개월 내 v1.0 | **"될 때까지"** |
| 베타 테스터 NDA | 법률 자문 필요 | **주변 프로듀서 직접** |
| 마케팅 | 필요 | **필요 없음** |

### 1.3 해방된 기술적 범위

기존 MVP에서 v1.1, v2.0, v3.0으로 미뤘던 모든 기능을 **자유롭게 시도 가능**:

- ✅ Pultec EQP-1A (기존 MVP 타깃)
- ✅ LA-2A 스타일 opto compression (v2.5 → **바로**)
- ✅ 1176 FET compression (v3.0 → **바로**)
- ✅ Fairchild variable-mu (v3.0 → **바로**)
- ✅ Tape saturation (별도 제품 → **같은 플러그인 내**)
- ✅ Neural hybrid layer (v1.1 → **처음부터 가능**)
- ✅ 실험적 기능 (Hysteresis loop 시각화 등)

---

## 2. 재설계된 프로젝트 정체성

### 2.1 새 이름 제안

상표 걱정 없이 자유롭게:
- **"Living Analog Suite"** — 추상적
- **"Tubeamp-LA"** — Living Analog 약자
- **"AnalogLab-OSS"** — 오픈소스 명시
- **"ValvraLab"** — 오리지널 이름
- **"Resonance"** — 미니멀

권장: **"Valvra"** (독자 이름, 확장 가능)

### 2.2 새 포지셔닝 문장

> **"상업 플러그인이 놓치는 살아있는 아날로그 특성을 정확히 구현하는, 믹싱 엔지니어와 프로듀서를 위한 오픈소스 DSP 연구 플러그인."**

### 2.3 Primary Goals

1. **내가 직접 쓰기에 좋음** — 현재 쓰는 UAD/Waves/Slate 대체 가능 수준
2. **주변 프로듀서가 "와 이거 좋네"** — 즉각 실익
3. **기술적 정당성** — 연구 문서 22개 기반, 학술 수준 정확도
4. **오픈소스 기여** — GitHub 공개 시 개발자 커뮤니티 가치

### 2.4 Non-Goals

- ❌ 매출·수익
- ❌ 시장 점유율
- ❌ 상업적 리뷰
- ❌ UI 고급화 (기능 우선)
- ❌ 완벽한 문서화 (코드가 1차)

---

## 3. 재설계된 기술 스택 (전부 무료)

### 3.1 Core Framework

```
JUCE 8 GPL-3 (무료)
├── VST3 (Steinberg SDK GPL-3 + proprietary dual)
├── AU (Apple Audio Unit SDK, 무료)
├── CLAP (MIT, Bitwig 기본 지원)
└── Standalone (테스트용)
```

**AAX는 제외:** Avid 서명 필수 + NDA. Pro Tools 사용자는 VST3로도 가능.

### 3.2 DSP 라이브러리 (모두 자유 사용)

```
chowdsp_utils (GPL-3/BSD 혼합) — 모든 모듈 사용 가능
├── chowdsp_filters (GPL-3) — biquad, butterworth
├── chowdsp_dsp_utils (GPL-3) — 오버샘플, SIMD
├── chowdsp_compressor (GPL-3) — 옵토/FET 참조 구현
├── chowdsp_waveshapers (GPL-3) — 비선형 참조
├── chowdsp_wdf — Wave Digital Filter 프레임워크
└── 기타 모듈 자유 사용

RTNeural (BSD-3) — Neural hybrid
Eigen (MPL-2) — 선형대수
xsimd (BSD-3) — SIMD 추상화
Dear ImGui (MIT) — 실험용 UI
```

### 3.3 학습/데이터 (v1.1+ Neural Hybrid)

```
NAM 엔진 (MIT) — 자유 포크
Open-Amp 합성 데이터 (Apache-2.0) — Tube nonlinearity 사전학습
Wright et al. 2020 데이터 (MIT) — Valve amp fine-tuning
SignalTrain LA2A (CC-BY) — Opto 잔차 학습
```

### 3.4 빌드·배포

```
CMake 3.25+ (BSD-3)
GitHub Actions (무료, public repo)
GitHub Releases (무료)
README + 이슈 트래커
```

### 3.5 코드 서명 전략

**macOS:**
- Option A: unsigned .pkg + "환경설정 > 보안 > 열기" 수동 승인 (사용자 가이드)
- Option B: ad-hoc signing (무료, Gatekeeper 통과 안 됨)
- Option C: 자체 빌드 CMake 안내 (가장 안전)

**Windows:**
- Option A: unsigned .msi + SmartScreen 경고 수용
- Option B: self-signed (인증서 경고)
- Option C: 자체 빌드 안내

**현실:** 주변 프로듀서 대상이면 "처음 설치 시 주의사항" 한 줄 가이드로 충분.

---

## 4. 스코프 재설계 — 야심찬 "Living Analog Suite"

### 4.1 단일 플러그인으로 모든 것 (권장)

**"Valvra" 하나에 토폴로지 선택 방식:**

```
[Input] → [Tube Stage 선택] → [Transformer 선택] → [Comp 선택] → [Output]
               │                      │                    │
       12AX7/12AU7/EL34        Marinair/UTC/Jensen     Opto/FET/Vari-Mu
       ECC88/6SN7/None          Triad/Off              Off
```

### 4.2 Tier 재설계 (일정 유연화)

**Tier 0 — "Does it work?" (목표: 1-2개월)**
- [ ] CMake + JUCE GPL 프로젝트 스캐폴딩
- [ ] Koren triode model (Dempwolf 2011 파라미터)
- [ ] Static Jiles-Atherton 트랜스포머
- [ ] 4× polyphase oversampling
- [ ] 단일 tube stage → transformer 체인
- [ ] 기본 UI (노브 5-6개, 필요 최소)
- [ ] Standalone 앱 또는 VST3 로드 가능

**검증:** 본인이 DAW에 로드해서 실제로 쓸 수 있는가?

**Tier 1 — "Pultec EQ 모드" (목표: +2-3개월)**
- [ ] Pultec passive LC 섹션 (WDF or direct filter)
- [ ] 12AX7 + 12AU7 2단 체인
- [ ] 정확한 EQ 커브 (Gyraf BOM 기반)
- [ ] Boost/Atten/Bandwidth 제어
- [ ] "Pultec Trick" 작동 확인

**검증:** 상용 Pultec 플러그인과 비슷한 색깔이 나오는가?

**Tier 2 — "살아있는" 차별점 (목표: +2-3개월)**
- [ ] **Cathode bounce** (τ=37.5ms, Jones/Blencowe 파라미터)
- [ ] **PSU sag** (GZ34 3-5%, 50-150ms recovery)
- [ ] **Monte Carlo per-instance seed** (Dempwolf μ 17% 편차 기반)
- [ ] 열 드리프트 (10-15s warmup, 선택적)

**검증:** 
- Null test: 같은 seed 재현성 ≥ -90 dBFS
- 다른 seed 개체차: -40 ~ -20 dBFS
- 30초 내 시변성 확인 가능

**Tier 3 — "확장" (목표: +3-6개월, 관심에 따라)**
- [ ] LA-2A 스타일 opto 모드
- [ ] 1176 스타일 FET 모드
- [ ] Mid/Side 처리
- [ ] True Peak 안전 limiter
- [ ] Linear-phase 옵션

**Tier 4 — "실험" (관심 있으면)**
- [ ] Neural hybrid residual (NAM engine + Wright 2020 fine-tune)
- [ ] Hysteresis loop 실시간 시각화
- [ ] Tape saturation 모드

### 4.3 일정: "될 때까지"

- Tier 0: ~2026년 6월 (2개월)
- Tier 1: ~2026년 9월 (+3개월)
- Tier 2: ~2026년 12월 (+3개월)
- Tier 3: 2027년 중 (관심에 따라)
- Tier 4: 미정

**중단 조건:** 본인이 "이미 충분히 좋다"고 느끼는 시점. 상업적 압박 없음.

---

## 5. 배포 전략 (GitHub 중심)

### 5.1 저장소 구조

```
github.com/<user>/valvra
├── src/
├── tests/
├── resources/
├── docs/            ← 현재 22개 문서 (공개)
├── .github/workflows/
│   └── build.yml    ← GitHub Actions 자동 빌드
├── README.md        ← 스크린샷 + 설치 가이드
├── LICENSE          ← GPL-3
├── CMakeLists.txt
└── releases/        ← 바이너리 (GitHub Releases)
```

### 5.2 README 핵심 구조 (주변 프로듀서 대상)

```markdown
# Valvra — Living Analog EQ

실제 아날로그 장비의 시변 특성을 재현하는 오픈소스 플러그인.
UAD/Waves 같은 정적 시뮬레이션과 다른 점:
- 시간에 따라 소리가 살짝 변함 (워밍업, 캐소드 바운스)
- 인스턴스마다 개체차 (같은 설정이라도 살짝 다름)

## Install (macOS)
1. [Releases](...)에서 .pkg 다운로드
2. 우클릭 → 열기 → 확인
3. DAW 재시작

## Install (Windows)
1. .msi 다운로드
2. SmartScreen "추가 정보" → "실행"
3. DAW 재시작

## 사용법
[스크린샷 + 30초 영상]
```

### 5.3 배포 채널

1. **GitHub Releases** (메인)
2. **개인 Discord / Telegram 그룹** — 주변 프로듀서
3. **한국 프로듀서 커뮤니티** — 밤샘작업, 레코딩 포럼 등 선택적
4. **영어권 (선택):** KVR Audio "Freeware" 섹션, r/audioengineering 주 1회 홍보

### 5.4 피드백 루프

- GitHub Issues (버그 리포트)
- GitHub Discussions (기능 요청)
- Discord DM (주변 프로듀서 직접 피드백)

---

## 6. 법적 / 라이선스 재검토

### 6.1 GPL-3 수용의 의미

**전체 플러그인이 GPL-3로 공개됨:**
- ✅ 누구나 소스 코드 읽기/수정/재배포 가능
- ✅ 파생 작품도 GPL-3 (copyleft)
- ⚠️ **나중에 상업화 못 함** (이미 GPL 배포한 코드는 되돌릴 수 없음)
- ✅ 본인은 저작권 보유, 듀얼 라이선싱 가능 (미래 상업화 대비)

**현실적 판단:** 이 프로젝트는 "평생 무료"라고 결정해도 손실 없음.

### 6.2 상표 사용 가이드 (비상업)

비상업적 사용에서 더 관대하지만, 여전히 주의:

**안전한 표현:**
- "Inspired by classic 1960s tube program equalizers"
- "Pultec-style EQ" (설명적, fair use 범위)
- 문서에 원리 설명 시 "Pultec EQP-1A" 언급 가능

**피해야 할 표현:**
- 제품명을 "Pultec Clone" 식으로
- 로고 / 트레이드 드레스 모방
- 상표권자가 상업적으로 오해할 표현

**실제 위험:** 무료 오픈소스 프로젝트에 대한 상표권 소송은 거의 없음. Pulse Techniques, AMS-Neve 등은 상업 플러그인을 주로 타깃.

### 6.3 학술 인용 가치

23개 문서 + 소스 코드가 **학술 참고 자료**로 가치 있음:
- DAFx 발표 가능
- AES 학생 논문 자료로 활용
- 한국 대학 오디오 프로그래밍 교재 가능

---

## 7. 품질 기준 재정의

### 7.1 상업적 기준 (폐기)

- ❌ 유료 유저 200+
- ❌ 매출 $30K+
- ❌ 주요 리뷰

### 7.2 새로운 성공 기준

**Tier 0 성공:**
- 본인이 실제 믹스 세션에서 1회 이상 사용
- 크래시 없이 30분 재생

**Tier 1 성공:**
- 본인이 "Pultec 플러그인 대신 이걸 쓰겠다" 결심
- 주변 프로듀서 1명이 "와 이거 좋네" 반응

**Tier 2 성공:**
- Null test로 시변성 정량 증명
- Monte Carlo 개체차 실제로 들림
- 주변 프로듀서 3명 이상이 자발 사용

**Tier 3 성공:**
- GitHub Star 50+ (1년 내)
- 외부 기여자(pull request) 1명 이상

### 7.3 품질 기준

**반드시:**
- 크래시 없음 (Denormal, NaN 방지)
- CPU < 10% (128샘플 @ 44.1kHz, 기본 품질)
- 실제 사용에서 "이상한 소리" 없음

**희망:**
- Dempwolf Fig. 10 하모닉 재현 (±5dB)
- 시각적 피드백 (실용 수준)

---

## 8. 재계산된 예산

### 8.1 현금 예산: **$0**

- JUCE GPL: $0
- chowdsp: $0
- 의존성: $0
- GitHub: $0
- 도메인/호스팅: $0 (GitHub Pages 무료)
- 코드 서명: $0 (사용자 수동 승인)
- 측정 장비: $0 (실물 불가, 학술 데이터로)

### 8.2 투자되는 것

- **시간**: 취미 수준 ~ 주 10-20시간
- **학습 기회**: C++, JUCE, DSP 수학, 오디오 프로그래밍

### 8.3 리스크 재평가 (19번 문서 업데이트 필요)

| 기존 리스크 | 현재 상태 |
|------------|---------|
| R-M1 시장 검증 부족 | **해소** (시장 아님) |
| R-L2 GPL 감염 | **의도적 수용** |
| R-L1 상표권 | **거의 무관** (비상업) |
| R-M2 경쟁사 대응 | **무관** |
| R-M3 가격 실패 | **무관** (무료) |
| R-M4 판매 채널 | **무관** |
| R-E1/2/3 배포 플랫폼 | **크게 완화** |
| **R-O1 Burnout** | **여전히 P1** (일정 유연화로 완화) |
| **R-T1 WDF 수렴** | **여전히 P2** (기술적 문제는 동일) |
| **R-T2 CPU 성능** | **여전히 P3** |

**P1 리스크가 3개 → 1개 (Burnout)로 감소.** 프로젝트 성공 가능성 크게 상승.

---

## 9. 즉시 실행 Action (Week 1-2)

### Week 1 (2026-04-18 ~ 04-25) — Setup

- [ ] GitHub public 저장소 생성 (`valvra` 또는 후보명)
- [ ] JUCE 8 GitHub 복사 + CMake 프로젝트 스캐폴딩
- [ ] 기본 VST3 템플릿 빌드 성공 (Hello World 수준)
- [ ] `.github/workflows/build.yml` 작성 (macOS + Windows 자동 빌드)
- [ ] README 초기 버전 ("개발 중, 아직 쓰지 마세요")

### Week 2 (2026-04-25 ~ 05-02) — Koren triode 구현

- [ ] `src/dsp/KorenTriode.h/.cpp` 구현 (Dempwolf 2011 모델)
- [ ] RSD-1 파라미터 하드코딩
- [ ] Unit test (Google Test 또는 Catch2)
- [ ] SPICE와 정적 플레이트 곡선 비교 (Python matplotlib)
- [ ] 단일 사인파 입력 → H2/H3 측정 (Dempwolf Fig.10 ±5 dB 일치 검증)

### Week 3-4 — JA 트랜스포머 + 오버샘플

- [ ] `src/dsp/JilesAtherton.h/.cpp` (02번 RK4 코드 기반)
- [ ] Ni-Permalloy 기본 파라미터 (22번 C.2)
- [ ] 4x 폴리페이즈 오버샘플 (chowdsp_dsp_utils 활용 가능)
- [ ] Chain: Input → Koren → JA → Output 통합

### Week 5-6 — 최소 UI + 첫 스탠드얼론

- [ ] 5개 노브 JUCE UI (Drive, Character, Transformer, Output, Seed)
- [ ] Standalone 앱으로 WAV 재생 테스트
- [ ] VST3 빌드 + Reaper에서 로드

### M0 (2026-05-30) — Tier 0 완료 리뷰

본인이 DAW에 로드해서 실제 트랙에 걸어봄. 평가:
- ✓ 소리가 납작하지 않게 나오는가
- ✓ CPU 합리적인가
- ✓ 크래시 없는가

→ 통과 시 Tier 1 (Pultec EQ 모드) 진행.

---

## 10. 문서 재정리

### 10.1 유지되는 문서 (23개 중 대부분)

- **01-10**: 물리·이론 기반 (전혀 변경 없음, 오히려 더 가치 상승)
- **11**: 타깃 하드웨어 (여러 개 구현 가능으로 확장)
- **12**: 비-진공관 비선형성 (적극 활용 가능)
- **13**: 마스터링 기능 (선택적 구현)
- **14**: 회로 토폴로지 (여러 개 시도 가능)
- **15-16**: 사용·UI (참고용)
- **17**: 법률 (이번 문서가 대체)
- **18**: 측정 인프라 (실물 측정 불가로 주석)
- **19**: 리스크 (이번 문서가 대체)
- **21**: Pultec 측정 (기준 데이터)
- **22**: 학술 정량 데이터 (★ MVP 구현 직접 활용)

### 10.2 상태 변경

- **20 MVP 스코프 (상업 버전)**: 📦 **Archive** — "이렇게 상업화하려 했었다"의 기록으로 보존
- **23 개인용 리포지셔닝 (이 문서)**: 🟢 **현행 가이드**

---

## 11. 최종 메시지

### 11.1 해방

> "상업 제약이 없다는 것은, 연구 문서 22개를 **전부** 구현해볼 수 있다는 뜻이다. 상업 버전은 MVP로 1개만 만들 수 있었지만, 개인 버전은 시간만 있으면 모두 만들 수 있다."

### 11.2 현실적 기대

- 이 프로젝트는 **취미 + 학습**으로 시작
- Tier 0까지는 2개월 투자면 충분
- 즐거워야 지속됨. 재미 없으면 중단 OK
- 완성되면 **주변 프로듀서가 진짜 쓰는 도구**
- 잘 되면 **오픈소스 커뮤니티에 기여**

### 11.3 기술적 자신감

**23개 연구 문서 + 학술 정량 데이터 + 오픈소스 생태계 = 단독 개발자 범위 내 실행 가능**

---

## 문서 버전

- **v1.0.0** (2026-04-18): 상업 제약 해제에 따른 완전 재설계. 20번 MVP 문서를 대체. 예산 $0, 일정 유연, 야심찬 기술 범위 확정.

---

*이 문서는 본 프로젝트의 현행 가이드다. 상충 시 이 문서가 우선한다.*
*최종 업데이트: 2026-04-18*
