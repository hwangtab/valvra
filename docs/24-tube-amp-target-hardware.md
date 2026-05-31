# 24. 진공관 앰프 타깃 하드웨어 상세

> **작성일:** 2026-04-19 · **갱신:** 2026-04-27 (use-case 정합 정정)
> **목적:** [20 MVP v2.0](./20-mvp-scope-decision.md)의 5개 모드(V72 Preamp / Console Output / Culture Vulture / RNDI / HiFi 300B SE)의 기술적 근거가 되는 실물 하드웨어를 분석. 각 하드웨어의 회로 토폴로지, 진공관 동작점, 트랜스포머 스펙을 학술 논문·엔지니어링 레퍼런스에서 수집.
> **연관 문서:** [01 진공관](./01-vacuum-tube-physics.md) · [14 회로 토폴로지](./14-missing-circuit-topologies.md) · [22 학술 정량 데이터](./22-academic-quantitative-data.md)

> **사용 정체:** 본 플러그인은 **믹싱 / 마스터링용 진공관 색깔 프로세서**다. 이 문서가 Marshall JCM800 / Fender Twin / Vox AC30 / Western Electric 91A 같은 하드웨어를 인용하는 것은 운영점 / 트랜스포머 / 전원부 calibration의 *물리적 출처*이며, 본 플러그인이 기타앰프 시뮬레이션이라는 뜻이 아니다. 예컨대 §B "Output Stage Mode"의 회로 분석은 그대로 살아 있지만, 실제 Valvra의 **Console Output** 모드는 같은 회로를 **class-A1 mid-rail bias**로 옮겨 mix-bus / 마스터버스 워밍에 적합하게 voicing 한다 (사용자가 Drive를 ≥ 2.5로 푸시하면 class-AB cutoff 영역으로 들어가 본문에 기술된 기타-앰프 크런치 캐릭터도 사용 가능).

> **상표 주의:** 상표는 교육·설명 목적으로만 사용. 본 프로젝트는 Nominative Fair Use 범위 내에서 회로 **원리**를 참고하며, 제품 상표를 마케팅에 사용하지 않음 ([17번 법률](./17-legal-and-licensing.md) Option A). 사용자-가시 모드 라벨은 "V72 Preamp / Console Output / Culture Vulture / RNDI DI / HiFi 300B SE" 로, 회사명/제품명 직접 노출을 피한다.

---

## A. Preamp Mode — Telefunken V76 / Neumann V72 계열

### A.1 역사와 의의

- **V72** (Neumann, 1950년대 중반): 독일 방송국 표준 마이크 프리앰프, 40 dB gain
- **V76** (Telefunken, 1960년대): V72 후속 개량형, 선택 가능 35/45/55 dB gain
- 현대 "Telefunken sound"의 기원. ABBA, Beatles 후기 녹음, 수많은 전설적 레코드에 사용
- Chandler Limited TG1 (EMI 기반)과 함께 "진공관 프리앰프의 고전"

### A.2 회로 토폴로지 (V76 기준)

```
[Mic Input]
     ↓
[Input Trafo] (Haufe 또는 custom, 1:5~1:10 step-up)
     ↓
[V1: EF804S pentode] — 고이득 입력단 (common-cathode)
     ↓
[Interstage Cap] (0.47 μF)
     ↓
[V2: EF804S pentode 또는 6072 triode] — 드라이버
     ↓
[Global Negative Feedback Loop] ← 35/45/55 dB 선택
     ↓
[Output Trafo] (Haufe custom, 600Ω line out)
     ↓
[Line Output]
```

### A.3 핵심 기술 파라미터

| 항목 | 값 | 출처 |
|-----|-----|------|
| 진공관 | EF804S × 2 (pentode), 또는 EF804S + 6072 | Telefunken V76 service manual |
| B+ 전압 | ~260 V DC | 동일 |
| 입력 트랜스포머 | Haufe M8, 1:10 turns ratio (mic position) | Mischer 1965 ref |
| 출력 트랜스포머 | Haufe M131, 10k:600 Ω | 동일 |
| Gain | 35 / 45 / 55 dB 3단 | V76 front panel |
| THD @ +4 dBu out, 1 kHz | < 0.5% (60 dB input gain) | Telefunken spec |
| 주파수 응답 | 30 Hz–15 kHz ±1 dB | 동일 |
| Input impedance | 200 Ω (mic) | 동일 |

### A.4 EF804S 진공관 특성

- **EF804S**: Telefunken 자체 설계 저노이즈 audio pentode
- μ_eff ≈ 500 (triode-strapped), gm ≈ 3.5 mA/V
- Triode-strapped 모드 주로 사용 → **2nd harmonic 지배** (warm)
- 현대 대체: EF86 (일반적, 저렴) or EF806S (selected low-noise)
- **MVP 구현:** EF86 pentode Koren-확장 모델 사용 → triode-strap 토글

### A.5 Preamp Mode 구현 사양 (Valvra)

```cpp
// Preamp Mode default chain
ChainConfig V72_Preamp = {
    .inputTransformer = TransformerType::HaufeCustomM8,  // Ni-Permalloy
    .stages = {
        {
            .tube = TubeType::EF86_TriodeStrap,
            .topology = Topology::CommonCathode,
            .Rk = 1500.0,    // 1.5 kΩ
            .Ck = 47e-6,     // 47 μF (full bypass)
            .Rp = 220000.0,  // 220 kΩ
            .Vp_nominal = 250.0
        },
        {
            .tube = TubeType::TUBE_6072,  // = 12AY7
            .topology = Topology::CommonCathode,
            .Rk = 820.0,
            .Ck = 22e-6,
            .Rp = 100000.0,
            .Vp_nominal = 260.0
        }
    },
    .outputTransformer = TransformerType::HaufeCustomM131,
    .globalFeedback = 45.0,  // dB reduction (선택적)
    .rectifier = RectifierType::GZ34  // slight sag
};
```

---

## B. Console Output Mode — Marshall JCM800 / Fender Twin 회로 참조

> **사용자-가시 라벨:** "Console Output". 본 섹션의 JCM800 / Twin 회로는 **물리적 참조**로 인용 — DSP가 빌리는 토폴로지·트랜스포머·전원부 calibration의 출처다. 운영점 자체는 mix/master 친화로 옮겼다 (§B.5 참조).

### B.1 역사와 의의 (회로 출처)

- **Marshall JCM800 2203** (1981~): 록 기타 톤의 상징. EL34 기반 100W 전력앰프 — LTP phase splitter + push-pull EL34 + UTC-style OPT 의 정전형(canonical) 회로 출처.
- **Fender Twin Reverb** (1965~): 6L6GC 기반 클린 파워, 미국 Clean 사운드의 표준.
- **본 플러그인에서의 활용:** 이 회로의 **물리** (LTP 차동 입력, push-pull 짝수배음 상쇄, OPT 히스테리시스, B+ rail dynamics)는 그대로 시뮬레이션. 단 운영점은 mix/master 정합으로 옮김 (Vg_bias −36 V class-AB1 → −25 V class-A1, driveScale 32 → 15). 디폴트 Drive에서는 짝수배음 dominant gentle saturation (drum bus / master glue), Drive ≥ 2.5에서 회로 본연의 class-AB cutoff crunch 도달 — guitar coloration 도 사용자가 원할 때 사용 가능.

### B.2 Marshall JCM800 Power Section

```
[Phase Inverter: 12AX7 LTP]
     ↓ (반위상 2 outputs)
[EL34 × 4, Class AB Push-Pull]
     ↓ (UL 43% tap)
[Marshall Output Transformer] (Drake or Dagnall, 3.4k plate-plate : 4/8/16 Ω)
     ↓
[Speaker Output]
```

### B.3 Fender Twin Reverb Power Section

```
[Phase Inverter: 12AT7 LTP]
     ↓
[6L6GC × 4, Class AB Push-Pull]
     ↓ (cathode biased)
[Output Transformer] (Schumacher, 6.6k:4/8 Ω)
     ↓
[Speaker Output]
```

### B.4 핵심 기술 파라미터

| 항목 | Marshall JCM800 | Fender Twin |
|-----|----------------|-------------|
| 파워관 | EL34 × 4 | 6L6GC × 4 |
| 동작 클래스 | AB1 push-pull | AB1 push-pull |
| 바이어스 | Fixed (-40~-45V grid) | Cathode (~250 mA 총) |
| UL tap | 43% | 50% (또는 full pentode) |
| B+ | ~470 V | ~465 V |
| 출력 임피던스 | 8 Ω primary 3.4 kΩ | 8 Ω primary 6.6 kΩ |
| THD @ 50% output | ~1% | ~0.5% |
| THD @ clipping | 10~30% (soft saturation) | 10~25% |

### B.5 Console Output Mode 구현 (Valvra)

**중요:** 스피커·cabinet 시뮬레이션은 **포함하지 않음**. 우리는 "파워관 + OPT의 색깔"만 취한다 (amp sim이 아니라 mix/master color processor).

본 플러그인의 실제 구현 (`src/dsp/TubeAmpChain.h::MarshallMode`)은 위 회로 참조의 **물리 구조**를 그대로 가져오되, 운영점만 mix/master 정합으로 이동 — bias가 class-AB1 cutoff knee 대신 class-A1 mid-rail에 앉아 디폴트 Drive에서 짝수배음 워밍이 되고, 사용자가 Drive를 푸시할수록 위 §B.4 표의 class-AB 시그니처로 자연스럽게 진입한다.

```cpp
// 실제 코드의 의도 요약 (src/dsp/TubeAmpChain.h::MarshallMode 참고)
ChainConfig ConsoleOutput = {
    .inputTransformer = TransformerType::Off,           // line level 직입
    .preampStages = {
        { .tube = TUBE_12AX7, .Vg_bias = -1.6 },         // warm bias (was −1.2 cold)
        { .tube = TUBE_12AX7, .Vg_bias = -1.8 }          // linear driver
    },
    .pushPullPower = {                                   // ideal phase split + PP pair
        .tube = EL34_TriodeStrap,
        .Vg_bias = -25.0,                                // class-A1 mid-rail (was −36 class-AB1)
        .driveScale_VPerUnit = 15.0,                     // gentler default (was 32)
        .Vp_nominal = 450.0,
        .Rk_tail = 470.0,
        .tubeAsymmetry = 0.03                            // matched-pair "even-harmonic breath"
    },
    .outputTransformer = UTC_A12_Style,                  // Si-steel
    .rectifier = SolidState,
    .sagBehavior = SolidStateStiff                       // weak but non-zero sag
};
```

→ 사용자가 Drive=1.0에서 듣는 톤은 "console power-stage warmth" (drum bus / mix bus / master glue), Drive ≥ 2.5에서 듣는 톤은 §B.4의 class-AB JCM800 character. 한 회로, 두 use case.

---

## C. Line Color Mode — Thermionic Culture Vulture

### C.1 역사와 의의

- **Thermionic Culture Vulture** (1998, Vic Keary 설계)
- "극단 드라이브"를 의도적으로 추구한 스튜디오 유닛
- 3단 모드: **Triode** (짝수 하모닉), **Pentode 1** (홀수 강조), **Pentode 2** (극단 왜곡)
- 드럼 버스, 보컬 더블, 베이스에 "파괴적 색깔" 주는 용도
- Soundtoys Decapitator의 "T"/"P" 모드가 이를 참조

### C.2 회로 토폴로지 (2차 검증됨, [11 §11.7](./11-target-hardware-catalog.md))

```
[Input Trafo] (custom)
     ↓
[V1: EF86 pentode] — Input stage (low noise pentode)
     ↓
[Overbias 전환] ← 사용자 Bias 노브
     ↓
[V2: 6AS6] — Distortion core (pentode, can bias across full curve)
     ↓
[Mode switch: T / P1 / P2]
     ↓
[V3: 5963 (= 12AU7 variant)] — Output triode (cathode follower)
     ↓
[Output Trafo]
```

**핵심:** 6AS6가 "Culture Vulture 사운드"의 심장. 이 관은 rare하지만 **gain control grid(G3)를 이용한 variable-μ 동작**이 핵심.

### C.3 기술 파라미터

| 항목 | 값 |
|-----|-----|
| 진공관 | EF86 + 6AS6 + 5963 (12AU7 계열) |
| Drive range | 0~100% (클린에서 완전 파괴까지) |
| Bias 노브 | Fixed bias voltage 가변 (drive 레벨 변화) |
| 모드 T (Triode) | 6AS6 triode-strap, H2 지배, "warm saturation" |
| 모드 P1 (Pentode low) | 6AS6 normal pentode, H3 강조 |
| 모드 P2 (Pentode high) | 6AS6 extreme bias, 극단 왜곡 |
| THD T at full drive | 10~20% (musically pleasing) |
| THD P2 at full drive | 30~50% (completely destroyed) |

### C.4 Line Color Mode 구현 (Valvra)

```cpp
ChainConfig CultureVulture_Mode = {
    .inputTransformer = TransformerType::CinemagStyle,  // 중간 사이즈 input
    .stages = {
        {
            .tube = TubeType::EF86_Pentode,  // Full pentode (not strapped)
            .topology = Topology::CommonCathode,
            .Rk = 820.0,
            .Ck = 1.0e-6  // 낮은 값 → partial bypass (intentional)
        },
        {
            .tube = TubeType::TUBE_6AS6,  // variable-mu pentode
            .topology = Topology::CommonCathode,
            .driveBias = true,  // 사용자 bias 제어
            .modeSwitch = CultureMode::T_P1_P2  // 사용자 선택
        },
        {
            .tube = TubeType::TUBE_12AU7,
            .topology = Topology::CathodeFollower,  // Buffer
            .Rk = 10000.0
        }
    },
    .outputTransformer = TransformerType::VintageSmall,
    .driveRange = {0.0, 1.0}  // 0%~100% 극단까지
};
```

### C.5 왜 이 모드가 꼭 있어야 하는가

- Culture Vulture는 **하드웨어 $2,500~$3,500** → 접근 불가
- Soundtoys Decapitator의 T/P 모드가 이를 근사하지만 **정적 waveshaper**
- Valvra의 Line Color는 **실제 6AS6 회로 + 시변 + 개체차**로 "진짜 Culture Vulture 같은 살아있는 파괴"를 구현

---

## D. DI / Instrument Mode — Rupert Neve Designs RNDI 계열

### D.1 역사와 의의

- **Rupert Neve Designs RNDI** (2013~): 모던 "Class A active DI"
- Rupert Neve 특유의 **Jensen 트랜스포머 기반 saturation** 활용
- 베이스 DI, 기타 DI, 신디 DI에서 "아날로그 펀치" 추가
- 드럼 DI 루트에서도 활용 (킥, 스네어 트리거)

### D.2 회로 토폴로지

```
[Hi-Z Input (1 MΩ)]
     ↓
[JFET buffer] ← 진공관 아님! (but Rupert의 철학: "진공관 색깔을 JFET+Transformer로")
     ↓
[Class A amplification stage]
     ↓
[Jensen JT-DB-E 트랜스포머] ← **핵심 색깔**
     ↓
[XLR Balanced Out (150 Ω)]
```

### D.3 Valvra DI Mode의 재해석

Rupert의 JFET 대신 **실제 진공관**으로 해석:

```cpp
ChainConfig RNDI_Mode = {
    .inputTransformer = TransformerType::Off,  // Hi-Z 직입
    .stages = {
        {
            .tube = TubeType::TUBE_12AX7,
            .topology = Topology::CathodeFollower,  // Hi-Z to Low-Z buffer
            .Rk = 4700.0  // 100% bypassed
        },
        {
            .tube = TubeType::TUBE_12AU7,
            .topology = Topology::CommonCathode,
            .drive = 0.3  // light
        }
    },
    .outputTransformer = TransformerType::JensenJT_DB_E,  // 80% Ni, very clean
    .rectifier = RectifierType::None,
    .colorEmphasis = ColorProfile::TransformerDominant  // Trafo sat > tube sat
};
```

### D.4 특징

- **Low drive, high headroom** — 진공관은 살짝, 트랜스포머가 주
- Jensen JT-DB-E의 **낮은 THD, 넓은 BW**가 "투명하지만 색깔 있는" 특성
- DI 용이므로 임피던스 매칭 모델링 (1 MΩ 입력 → 150 Ω 출력)

---

## E. 시변 효과의 실제 하드웨어 근거

### E.1 각 모드별 시변 특성

| 모드 | Cathode Bounce | PSU Sag | 열 드리프트 | Trafo Memory |
|-----|---------------|---------|-----------|-------------|
| **Preamp (V72)** | 약 (10~30 mV) | 매우 약 (SS rect) | 강 (15~30s) | 중간 |
| **Output Stage (Marshall)** | 없음 (fixed bias) | 약 (SS rect) | 중간 | **강** (OPT 히스테리시스) |
| **Output Stage (Fender)** | 중간 (cathode bias) | **강** (GZ34) | 중간 | 강 |
| **Line Color (Vulture)** | **강** (extreme bias swing) | 중간 | 약 (이미 상승 상태) | 약 |
| **DI (RNDI)** | 없음 | 없음 | 약 | 중간 |

### E.2 모드별 "살아있는 느낌" 차별

- **Preamp:** 30초 워밍업 이후 "다른 악기"처럼 변하는 체감
- **Output Stage (Fender):** 드럼 킥 뒤 B+ sag로 부드러운 압감
- **Line Color:** Drive 극단에서 바이어스가 "숨쉬는" 듯한 변조
- **DI:** 가장 미묘. 트랜스포머 메모리만 작동

---

## F. 경쟁 제품 세밀 비교 (Output Stage 모드 기준)

| 플러그인 | 실제 기술 | Valvra와의 차이 |
|---------|---------|--------------|
| **UAD Marshall Plexi Classic** | 회로 시뮬 기반, 우수 | 정적. 캐소드 바운스·개체차 없음 |
| **Softube Marshall Plexi Super Lead** | 컴포넌트 시뮬 | 정적. 미세한 시변만 |
| **IK Multimedia AmpliTube** | 멀티 앰프 + cabinet | Amp sim (우리 목적 아님) |
| **Neural DSP Fortin Cali** | LSTM 기반 | 물리 모델 없음. 시변은 학습 데이터 의존 |
| **Valvra Output Stage** | **물리+Neural 하이브리드, 완전 시변** | 전 항목 앞섬 |

---

## G. 학술 논문 / 레퍼런스 정리

### G.1 V72 / Preamp 관련
- Blumlein, A. (1938). "Telephone-microphone amplifier with output transformer" — 초기 트랜스포머 결합 진공관 프리앰프 특허
- Telefunken V76 Service Manual (1965), Telefunken AG
- Berlant, A. (2010). "Vintage Tube Microphone Preamplifiers" *Recording Magazine*

### G.2 파워앰프 출력단
- Hafler, D., Keroes, H. (1951). "An Ultra-Linear Amplifier" *Audio Engineering* — UL 토폴로지 원전
- Jones, M. (2011). *Valve Amplifiers* 4th ed., Elsevier — Push-pull, LTP, OPT 상세
- Pakarinen & Yeh (2009) CMJ 33(2) — 디지털 시뮬레이션 리뷰

### G.3 Culture Vulture / Line Color
- Thermionic Culture Vulture *Operator's Manual* (Vic Keary)
- *Sound on Sound* "Thermionic Culture Vulture Review" (2001)
- 6AS6 datasheet, Raytheon (1960)

### G.4 DI / Transformer-dominated
- Whitlock, B. (2008). "Audio Transformers" in *Handbook for Sound Engineers*
- Jensen JT-DB-E 데이터시트, Jensen Transformers
- Rupert Neve Designs RNDI 공식 문서

### G.5 Neural Hybrid 데이터
- Wright, A. et al. (2020). "Real-Time Guitar Amplifier Emulation with Deep Learning" *Applied Sciences* 10(3):766
- Wright, A. et al. (2024). "Open-Amp" ICASSP 2025 preprint
- Hawley, S. et al. (2019). "SignalTrain" AES 147 Paper 10260

---

## H. 구현 우선순위 (v1.0 Tier 매핑)

| 모드 | Tier | 이유 |
|-----|------|-----|
| **Preamp (V72)** | Tier 1 | 가장 일반적 사용 (보컬·어쿠스틱), 회로 단순, 구현 기반 |
| **Output Stage (Marshall/Fender)** | Tier 2 | Push-pull LTP 토폴로지 추가 필요, 복잡 |
| **Line Color (Vulture)** | Tier 2 | 6AS6 variable-mu 특별 모델링 필요 |
| **DI (RNDI)** | Tier 3 | 비교적 단순하지만 Jensen JA 파라미터 정확도 요구 |

---

## I. 데이터 공백 & 해결 전략

| 공백 | 대응 |
|-----|------|
| V72 Haufe 트랜스포머 JA 실측 | Ni-Permalloy 재료 JA 파라미터 ([§22 C.2](./22-academic-quantitative-data.md)) 사용, 청취 기반 조정 |
| EF86 Dempwolf 급 실측 없음 | Koren EF86 표준 파라미터 + Dempwolf 12AX7 grid current 식 재활용 |
| 6AS6 variable-mu 정확 모델 | 6AS6 datasheet + Cohen-Hélie 2010 grid current 모델 |
| Culture Vulture 회로도 | Sound on Sound 2001 리뷰 + 커뮤니티 리버스 엔지니어링 |

---

## J. 문서 버전

- **v1.0.0** (2026-04-19): 진공관 앰프 4개 모드 (Preamp/Output Stage/Line Color/DI) 근거 하드웨어 분석. 20번 v2.0 MVP 스코프의 기술적 기초 제공.

---

*이 문서는 Valvra 플러그인의 4개 모드 프리셋이 단순 감각이 아닌 **실제 하드웨어 회로 토폴로지 + 학술 문헌**에 근거함을 증명한다.*
