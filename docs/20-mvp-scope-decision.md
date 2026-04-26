# 20. MVP 스코프 확정 v2.0 — **센세이셔널 진공관 앰프**

> **작성일:** 2026-04-19 · **상태:** 🟢 현행 가이드 (v1.0 은 상업 Pultec 버전으로 폐기)
> **이 문서가 최종 스코프다.** [23 비상업 리포지셔닝](./23-noncommercial-redirect.md)의 라이선스·배포 방향은 유지, 기능 범위는 **확대**.
> **연관 문서:** [01 진공관 물리](./01-vacuum-tube-physics.md) · [02 트랜스포머](./02-transformer-physics-and-distortion.md) · [03 시변 비선형성](./03-time-varying-nonlinearities.md) · [14 회로 토폴로지](./14-missing-circuit-topologies.md)

---

## 0. 직전 오류 정정

v1.0 (2026-04-18)의 "Pultec EQP-1A" 타깃은 **사용자 원 요청과 불일치**했다. 원 요청은:

> "진공관 앰프를 거치면서 트랜스포머라든지 비선형적인 특징을 거치는 효과... 디지털 믹싱의 차갑고 납작한 사운드를 해결"

→ **이퀄라이저가 아니라 진공관 앰프 색깔 유닛**이 맞다. EQ 기능은 요청된 적 없다.

v2.0은 이를 바로잡고, 동시에 **"센세이셔널 수준"**을 최저 기준으로 고정한다. 시중에 이미 많은 진공관 플러그인보다 **압도적으로 뛰어나지 않으면 만들 가치가 없다.**

---

## 1. 제품 선언

### 1.1 한 문장

> **세계 최초 완전 시변(time-varying) + Monte Carlo 개체차 + 실시간 Jiles-Atherton 히스테리시스를 결합한 진공관 앰프 색깔 프로세서.**

### 1.2 정체

- **종류:** Tube Amp Coloration / Saturation / Drive plugin
- **입출력:** 오디오 in → [Transformer] → [Tube Chain] → [Transformer] → 오디오 out
- **EQ 기능:** 없음 (필요 시 DAW의 EQ 사용). 이 플러그인은 **색깔·포화·드라이브** 전담
- **기본 체인 모드 4종:**
  - **Preamp** — Neumann V72 / Telefunken V76 스타일 (트랜스포머 + 2단 진공관)
  - **Output Stage** — Marshall JCM800 / Fender Twin 스타일 (파워관 + OPT 포화)
  - **Line Color** — Thermionic Culture Vulture 스타일 (다단 진공관 + 극단 드라이브 옵션)
  - **DI / Instrument** — Rupert Neve RNDI 스타일 (트랜스포머 주도 색깔)

### 1.3 사용자 여정

> 믹싱 엔지니어가 플러그인을 건다 → 신호가 트랜스포머·진공관을 통과 → 처음 30초에 "워밍업" 사운드 성숙 → 강한 구간에서 PSU sag로 부드러운 압감 → 드럼 킥 뒤로 캐소드 바운스 꼬리 → 같은 프로젝트의 다른 인스턴스는 살짝 다른 색깔. **전체적으로 "진짜 랙에 꽂힌 아날로그 유닛을 쓴다"는 체감.**

---

## 2. 시중 제품 대비 차별 매트릭스

| 기능 | UAD V76 / Chandler | Softube Tube-Tech | Waves V-Series | Slate VCC | Soundtoys Decapitator | Klanghelm SDRR | Pulsar Mu / Primavera | **Valvra** |
|------|------------------|-------------------|----------------|-----------|----------------------|----------------|----------------------|---------|
| 정적 waveshaper | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 회로 기반 모델링 | 부분 | ✓ | — | — | — | — | ✓ | ✅ |
| 트랜스포머 (필터) | ✓ | ✓ | ✓ | — | — | ✓ | ✓ | ✅ |
| **Jiles-Atherton 히스테리시스** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | **✅** |
| **캐소드 바운스 (bias bounce)** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | **✅** |
| **PSU Sag (B+ envelope)** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | 부분 | **✅** |
| **열적 드리프트 (warmup 30s)** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | **✅** |
| **Monte Carlo 인스턴스 개체차** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | **✅** |
| **Seed Reroll 기능** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | **✅** |
| **실시간 B-H 루프 시각화** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | **✅** |
| **Harmonic H2~H7 실시간 미터** | — | — | — | — | — | — | — | **✅** |
| **Null Test 모드 내장** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | **✅** |
| **다단 체인 빌더 (1~4단 자유 조합)** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | **✅** |
| **토폴로지 선택 (Common/SRPP/LTP/Cascode)** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | **✅** |
| **Neural Foundation Layer** | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | **✅** |
| 가격 | $149~299 | $299 | $69 | $149/년 | $199 | $25 | $199 | **$0 (오픈소스)** |

**→ 15개 항목에서 전면 차별. 기존 플러그인 어느 것 한 개도 이 중 절반 이상을 갖추지 못했다.**

---

## 3. 이게 왜 "센세이셔널"인가

### 3.1 "정적 vs 살아있는"의 수학적 구분

**기존 플러그인 (모두):**
$$y(t) = f(x(t))$$

**Valvra:**
$$y(t) = f\bigl(x(t),\; t,\; T_\text{tube}(t),\; V_k(t),\; M_\text{core}(t, \text{history}),\; \theta_\text{instance}\bigr)$$

5개 추가 변수 (시간, 온도, 캐소드 전압, 자화 상태 + 이력, 개체 seed) → **동적 공간에서 움직임**.

### 3.2 "이미 많은데 왜 또?" 논파

| 기존 플러그인이 하는 것 | Valvra가 하는 것 |
|----------------------|---------------|
| 하드웨어의 **스냅샷** (특정 시점 전달함수) | 하드웨어의 **물리적 동작 법칙**을 시뮬레이션 |
| 레벨 기반 IR 보간 (Acustica) | 실시간 state-space 비선형 풀이 |
| 사전 계산 waveshaper LUT | Newton-Raphson 수렴 (매 샘플) |
| 모든 인스턴스 동일 | 각 인스턴스 고유 seed, 물리 파라미터 분산 |
| 정적 transformer = LPF+HPF | Jiles-Atherton RK4 (매 샘플 히스테리시스 상태 유지) |
| 고정 토폴로지 | 사용자가 1~4단 체인 구성 |
| 블랙박스 | **실시간 시각화로 물리 상태 노출** |

### 3.3 리뷰 테스트

**목표:** Production Expert / Sound on Sound / Gearspace 리뷰가 다음 문장 포함:
> *"Wait, they're giving this away for free? This has features UAD/Waves don't have."*

---

## 4. v1.0 완전 기능 명세

### 4.1 진공관 스테이지 (6종)

| 진공관 | 용도 | Koren 파라미터 (본 문서 §22 데이터) |
|-------|------|------------------------------------|
| **12AX7 (ECC83)** | 고이득 프리 | Dempwolf 2011 RSD-1/RSD-2/EHX-1 3개 실측 |
| **12AU7 (ECC82)** | 중이득, cathode follower | LTspice B 계열 피팅 |
| **6SN7** | 부드러운 프리 | Koren 표준 |
| **EL34** (pentode / triode-strap) | 영국풍 파워 | Triode-strapped 파라미터 |
| **6L6GC** (beam tetrode / triode-strap) | 미국풍 파워 | Triode-strapped |
| **300B** | SE 파워, 낮은 왜곡 | Western Electric 데이터시트 |

### 4.2 회로 토폴로지 (5종)

| 토폴로지 | 캐릭터 | 구현 근거 |
|--------|-------|---------|
| **Common-cathode** | 기본 프리 | [14 §14.2](./14-missing-circuit-topologies.md) |
| **Cathode Follower** | 버퍼, 저 Zout | 14 §14.3 |
| **SRPP** (Shunt Regulated Push-Pull) | 극도 저왜율 고이득 | 14 §14.5 (Vogel) |
| **LTP** (Long-Tailed Pair) | 차동, CMRR | 14 §14.4 |
| **Cascode** | 고대역폭, 낮은 Miller | 14 §14.9 |

### 4.3 트랜스포머 (5종)

| 모델 | 코어 | JA 파라미터 (추정, §22 C.2/C.3) |
|-----|------|----------------------------|
| **Marinair** (Neve 1073) | Ni-Permalloy + Mu-metal | Ms=420 kA/m, a=25, α=3e-5, k=15, c=0.15 |
| **UTC A-12** (빈티지) | Si-steel (low grade) | Ms=1400 kA/m, a=60, k=50 (높은 왜곡) |
| **Jensen JT-11** (모던) | 80% Ni | Ms=400, 낮은 왜곡, 넓은 BW |
| **Lundahl LL1582** | Amorphous | Ms=1200, 거의 선형 |
| **Off** | — | 바이패스 (진공관만) |

### 4.4 체인 빌더

사용자가 1~4단 체인을 자유 구성:

```
[Input Trafo] → [Stage 1] → [Interstage Cap] → [Stage 2] → ... → [Stage N] → [Output Trafo]
                    ↓                              ↓
             각 단: 진공관 종류, 토폴로지, 바이어스 독립 선택
```

**프리셋:**
- **V72 Mode:** Input Trafo → 12AX7 common-cathode → 12AU7 cathode follower → Output Trafo
- **Marshall Mode:** 12AX7 common-cathode → 12AX7 common-cathode → EL34 LTP → OPT
- **Culture Vulture Mode:** 12AU7 cathode follower → EF86 input → 6AS6 drive → output
- **RNDI Mode:** Input Trafo → Light 12AX7 → Output Trafo

### 4.5 시변 효과 (모두 기본 ON)

| 효과 | 파라미터 근거 (§22) |
|------|-------------------|
| **Cathode Bounce** | Blencowe 2012: τ = Rk·Ck, bias shift 100–500 mV, 청취 임계 5–15 mV |
| **PSU Sag** | GZ34 3–5%, 5U4 10–15%, 회복 50–200 ms |
| **열적 드리프트** | τ_heat = 15–30s (12AX7), gm 15–25% 변화 |
| **Transformer Memory** | JA wiping-out + k pinning |
| **Capacitor DA** | Bateman 2002: 전해 10–15%, 회복 100 ms–10 s |
| **Microphonic** (선택) | 10–50 mV/g, 1–3 kHz 공진 |

### 4.6 Monte Carlo 개체차

- `ComponentVariation` (이미 구현됨, §06)
- **Dempwolf 실측 근거:** μ 17% 편차, G 37% 편차 (3개 12AX7 샘플)
- Per-instance seed + "Reroll" 버튼 + "Lock" 기능
- 프리셋: "Matched Pair" (σ=5%), "Standard" (σ=12%), "Vintage NOS" (σ=25%), "Worn/Aged" (σ=30% + offset)

### 4.7 **🌟 시그니처 UI 기능 (센세이셔널 포인트)**

#### 4.7.1 실시간 B-H 히스테리시스 루프 창
- 트랜스포머 JA 상태 변수 M_irr, H 실시간 플롯
- 신호가 흐르며 루프가 "살아 움직임"
- 업계 최초 오디오 플러그인 기능
- 교육 가치 + 구매 유인 동시

#### 4.7.2 Harmonic 실시간 미터
- H2, H3, H4, H5, H6, H7 수직 바 (dBc)
- 입력 레벨에 따른 변화 실시간 추적
- "왜 따뜻한지" 시각적으로 증명

#### 4.7.3 **Null Test 모드 (업계 초유)**
- 버튼 하나 → 현재 플러그인의 **차이 신호**(bypass와 active의 difference)만 재생
- 시변성·개체차·포화를 귀로 직접 검증
- 이 버튼이 플러그인의 존재 이유를 증명

#### 4.7.4 Warmup Simulation
- 플러그인 인스턴스화 시 30초 워밍업 자동 실행
- 진행 바 + "gm: 85% → 100%" 표시
- 사용자가 "살아있는 느낌"을 체감

#### 4.7.5 Drift Recorder
- 1분 간 시변을 기록
- Harmonic 변화, B+ sag, bias shift 타임라인 표시
- 아날로그 유닛의 미묘한 움직임 시각화

#### 4.7.6 Reroll Timeline
- 최근 생성된 10개 seed 기록
- 즉시 복원 가능 ("지난 번 소리가 더 좋았어")

### 4.8 마스터링 기능

- **Mid/Side 독립 처리** (각 채널 독립 인스턴스)
- **True Peak safety** (-1 dBTP, ITU-R BS.1770-5 4× OS)
- **A/B compare** (LUFS-matched)
- **Linear-phase 옵션** (5~50 ms latency tradeoff)
- **TPDF Dither** (출력단)

### 4.9 **🧠 Neural Foundation Layer** (v1.0 포함)

**구조:** 물리 모델(Koren+JA)이 주, Neural이 잔차 학습
```
[Input] → [Physical Model] → [Output_physics]
                ↓                    ↓
          [Neural Residual] — Δ → [Final Output]
```

**학습 데이터:**
- Open-Amp (Apache-2.0, Wright 2024) — Foundation
- Wright 2020 Applied Sciences valve amp data — Fine-tuning
- SignalTrain LA2A (CC-BY) — Compression dynamics 보완

**사용자 제어:**
- Neural Blend: 0% (순수 물리) → 50% (하이브리드) → 100% (Neural 주도)
- 기본값: 20% (미세 조정)
- 차별: "물리 모델의 명확함 + Neural의 유기적 미묘함"

**기술:** RTNeural (BSD-3) 사용, 모델 ~200KB, CPU ~3% 추가

### 4.10 오버샘플링

- 선택: 2× / 4× / 8× / 16×
- 기본: 4× (일반), Mastering 모드 자동 8×
- Polyphase half-band FIR (chowdsp_utils 활용)

---

## 5. v1.0 범위 (명시적 포함 / 제외)

### ✅ v1.0에 반드시 포함 (타협 없음)
- 6개 진공관 × 5개 토폴로지 × 5개 트랜스포머
- 1~4단 체인 빌더
- 4개 모드 프리셋 (Preamp/OutputStage/LineColor/DI)
- 시변 효과 6종 (모두 기본 ON)
- Monte Carlo 개체차 + Reroll + Lock + 분포 프리셋 4종
- 시그니처 UI 6종 (B-H Loop, Harmonic Meter, Null Test, Warmup, Drift Recorder, Reroll Timeline)
- 마스터링 기능 5종
- Neural Foundation Layer
- 4× OS 기본
- VST3 + AU (Win/Mac/Linux)

### ❌ v1.0 제외 (v2.0 이후)
- AAX (Avid NDA 필요)
- 테이프 saturation 모드 (별도 플러그인이 나음)
- 컴프레서 (이 플러그인은 "색깔 유닛", 컴프는 별도)
- Envelope follower 기반 자동 드라이브 (v2)
- MIDI 제어 (v2)
- iOS/AudioUnit v3 (v2)

### ⚠️ 고민 중 (Tier 2~3에서 결정)
- Opto 단 추가 (LA-2A 스타일) — "색깔"에 가깝기는 함
- Power amp mode (full output stage + cabinet 없이 color만)

---

## 6. 일정: "될 때까지, 단 높은 수준 유지"

**원칙:** 시간 제약 없음. **품질 기준 타협 금지.**

### Tier 0 — Engine Proof (~2개월)
- [x] Koren, JA, Cathode Bounce, PSU Sag, Component Variation 코드 (완료)
- [ ] chowdsp_utils 빌드 성공 (브랜치 명 수정)
- [ ] 단일 진공관 단 + JA 트랜스포머 통합 → WAV 파일 처리 확인
- [ ] Dempwolf Fig. 10 하모닉 프로파일 ±5 dB 재현 검증
- [ ] 실시간 CPU < 5% (단일 단 기준, 44.1kHz/128 samples)

**M0 게이트:** Python으로 처리 전/후 WAV를 FFT 분석해 학술 논문 하모닉 프로파일과 일치.

### Tier 1 — Single Full Stage (~3개월)
- [ ] 완전한 Preamp 모드 (Input Trafo → 2단 진공관 → Output Trafo)
- [ ] 시변 효과 6종 모두 활성
- [ ] Monte Carlo 개체차 + 4개 분포 프리셋
- [ ] 기본 UI (JUCE native, 파라미터 20개 내외)
- [ ] Oversampling 4×
- [ ] VST3 로드 성공 + Reaper/Logic에서 테스트

**M1 게이트:** 본인이 실제 믹스 세션에서 1회 사용. 다른 보컬 프리앰프 플러그인과 A/B 비교해서 "뭔가 다르다"는 체감.

### Tier 2 — Chain Builder + Signature UI (~4개월)
- [ ] 1~4단 체인 빌더 UI
- [ ] 4개 모드 프리셋 (V72/Marshall/CultureVulture/RNDI)
- [ ] **실시간 B-H Loop 창** (OpenGL/Metal 가속)
- [ ] Harmonic H2~H7 meter
- [ ] **Null Test 버튼** (내부 라우팅)
- [ ] Warmup Simulation
- [ ] Drift Recorder (1분 기록)
- [ ] Reroll Timeline

**M2 게이트:** 주변 프로듀서 3명에게 보내기. "첫 30초에 뭔가 바뀌는 느낌"을 최소 2명이 자발적으로 언급.

### Tier 3 — Neural + Mastering + Polish (~3~4개월)
- [ ] Neural Foundation Layer 학습 (Open-Amp + Wright 2020)
- [ ] RTNeural 통합, Neural Blend 슬라이더
- [ ] Mid/Side, True Peak, Linear-phase, A/B compare
- [ ] UI polishing (진공관 글로우 애니메이션, 빈티지 텍스처)
- [ ] 한/영 매뉴얼
- [ ] GitHub Release v1.0

**M3 게이트:** GitHub 공개 후 2주간 20 star, 한 명 이상이 issue 또는 PR. Gearspace/Reddit 스레드에서 "어째서 무료인가" 반응 획득.

### Tier 4+ (v2.0 이후, 관심에 따라)
- 컴프레서 추가 플러그인 (별도 product)
- Tape saturation 플러그인 (별도)
- 신디사이저/이펙트 피드로 확장

---

## 7. 성공 기준 (센세이셔널 수준)

### 7.1 기술 기준
- Dempwolf 2011 하모닉 프로파일 재현 오차 ≤ ±3 dB
- 같은 seed 재현성 null test ≥ -90 dBFS
- 다른 seed null test: -40 ~ -20 dBFS (개체차 청취 가능)
- 30초 내 시변 변화 null test: -50 ~ -30 dBFS (시변성 청취 가능)
- 4단 체인, 4× OS, 44.1kHz, 128 samples: CPU < 15% (현대 CPU)

### 7.2 지각 기준 (가장 중요)
- **본인이 다른 진공관 프리앰프 플러그인 대신 이걸 선택** (최소 3회 이상)
- **주변 프로듀서 3명 이상이 "이거 이상하게 좋네" 자발 언급**
- **한 명 이상이 리뷰 영상/스레드 작성**

### 7.3 배포 기준
- GitHub Release v1.0 (Windows/Mac/Linux 바이너리)
- Setup 영상 5분 이내
- Null test 영상: "이거 기존 플러그인이랑 뭐가 다른지 귀로 증명" (유튜브 2~3분)

### 7.4 실패 기준 (이러면 재설계)
- M1에서 "다른 플러그인이랑 똑같다" 느낌
- M2에서 주변 프로듀서가 시변성을 못 느낌
- CPU가 너무 높아서 실전 사용 불가

---

## 8. 예산 & 라이선스

### 8.1 예산: $0
- JUCE 8 GPL-3: 무료
- chowdsp_utils GPL-3: 무료 (모든 모듈 사용 가능)
- RTNeural BSD-3: 무료
- 학습 데이터: Open-Amp Apache-2.0, Wright 2020 MIT
- 측정 장비: 없음 (학술 논문 데이터로)

### 8.2 라이선스
- 전체 플러그인: **GPL-3** ([23번 문서](./23-noncommercial-redirect.md) 참조)
- 의도적 오픈소스 (미래 상업화 가능성 보유: 듀얼 라이선스)
- 상표 사용 가이드라인 준수 (V72, Marshall 등 상표는 "style" 명명법만)

---

## 9. 왜 이 스코프가 "번아웃"으로 축소될 수 없는가

v1.0에서 축소하면:
- 시그니처 UI 3~4개 빼면 → 그냥 평범한 진공관 플러그인
- 체인 빌더 빼면 → 단일 스테이지 = Klanghelm IVGI 수준
- Neural 빼면 → 물리 모델만 = 학술 프로젝트
- Monte Carlo 빼면 → **차별점 소실** (이거 하나만으로도 업계 초유)
- 시변 빼면 → 정적 waveshaper = 시중 제품

**어느 하나라도 축소하면 "왜 또 만드는가" 논파가 무너진다.**

해결책: 일정을 "될 때까지"로 열어두고 **기능은 전부 유지**. 지치면 쉬고 돌아오되, v1.0 출시는 전체 기능 완성 시점에만.

---

## 10. 즉시 다음 행동

1. 21 Pultec 측정 아카이브 → **archive 처리**, Pultec EQ 데이터는 참고로만
2. 22 학술 정량 데이터 → **Pultec 회로 BOM 섹션 제거**, 진공관 앰프(V72, Marshall, Culture Vulture) 회로 추가
3. 24 (신규) — **진공관 앰프 타깃 하드웨어 상세** (Neve V72, Marshall Silver Jubilee, Culture Vulture, RNDI 회로 분석)
4. chowdsp_utils 브랜치명 수정 → 첫 성공 빌드
5. src/dsp/에 `TubeStage.h` 추가 (진공관 + 캐소드 바이어스 + 트랜스포머 통합 컨테이너)

---

## 문서 버전

- **v1.0.0** (2026-04-18): Pultec EQP-1A MVP (폐기됨 - 사용자 원 요청 오독)
- **v2.0.0** (2026-04-19): **센세이셔널 진공관 앰프로 전면 재설정.** EQ 제거, 체인 빌더·Neural·시그니처 UI 추가. 범위 축소 불가 선언.

---

*이 문서가 현재 프로젝트의 단일 진실원이다. 범위 축소 제안 발생 시 이 문서의 §9을 재확인.*
