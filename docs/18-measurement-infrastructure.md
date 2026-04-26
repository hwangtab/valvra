# 18. 하드웨어 측정 인프라 및 프로토콜

> **연관 문서:** [06 확률 컴포넌트 모델링](./06-stochastic-component-modeling.md) · [09 측정과 검증](./09-measurement-and-validation.md) · [11 타겟 하드웨어 카탈로그](./11-target-hardware-catalog.md) · [19 리스크 분석](./19-risk-analysis.md)

> **핵심 원칙:** 이론 모델만으로는 실제 하드웨어의 색(character)을 재현할 수 없다. Monte Carlo 개체차 모델링(06 문서)과 경쟁사 대비 차별화(08 문서) 모두 **실측 데이터**가 기반이 되어야 한다. 본 문서는 "그래서 Neve 1073을 어떻게 측정할 것인가"의 실무 답이다.

---

## 18.1 서론

### 18.1.1 왜 실제 측정이 필요한가

본 프로젝트의 차별화는 다음 세 축을 기반으로 한다:

1. **시변 비선형성(Time-Varying Nonlinearity)** — 진공관의 열적 드리프트, 캐소드 바운스, 동적 바이어스 변동
2. **개체차(Unit-to-Unit Variation)** — 같은 모델이라도 유닛별로 다른 THD·주파수 응답
3. **회로 상호작용(Circuit Interaction)** — 진공관 ↔ 트랜스포머 ↔ 전원부 ↔ 입출력 임피던스 결합

이 세 가지는 **이론 식만으로는 절대 수치를 못 박을 수 없다.** 예를 들어:

- Koren 수식의 파라미터(μ, Ex, Kg 등)는 **측정된 플레이트 특성 곡선**에 피팅해야 한다.
- Jiles-Atherton 히스테리시스 파라미터(Ms, a, α, k, c)는 **측정된 B-H 루프**가 필요하다.
- 개체차 분포는 **같은 모델 N대의 측정값**에서만 추출 가능하다.

### 18.1.2 Monte Carlo 개체차 모델링의 데이터 요구

06 문서에서 제시한 확률 모델:

$$\theta_i = \mu_\theta + \sigma_\theta \cdot \varepsilon_i, \quad \varepsilon_i \sim \mathcal{N}(0,1)$$

여기서 $\mu_\theta, \sigma_\theta$는 **실측 분포**에서 나와야 한다. 문헌만으로는 얻을 수 없고, 추정치도 근거가 약하다.

### 18.1.3 본 문서의 범위

본 문서는 다음을 다룬다:

- 측정 장비 BOM과 예산 시나리오 (§18.2)
- 하드웨어 확보 전략 (§18.3) — 구매, 대여, 협력, 데이터 구매
- 단일 유닛 측정 프로토콜 (§18.4)
- 다중 유닛 측정 프로토콜과 통계 추출 (§18.5)
- 오차 분석과 불확실성 버젯 (§18.6)
- 파라미터 피팅 파이프라인 (§18.7)
- 자동화와 데이터 관리 (§18.8–18.11)

**냉정한 관점:** 본 문서는 "이상적 프로토콜"과 "현실적으로 가능한 것" 모두를 솔직하게 적는다. 개인 개발자가 APx555를 $40K에 사는 것은 비현실적이다. Phase 1($3,500)로 시작하는 길을 제시한다.

---

## 18.2 측정 장비 (Bill of Materials)

### 18.2.1 필수 장비

| 장비 | 용도 | 가격 (USD, 2026-04 업계 시세) | 대체품 | 비고 |
|------|------|-----------|--------|------|
| **Audio Precision APx555B** | 최고 정밀 THD/IMD/FR | **~$40,000 (base)**; Advanced Digital I/O 옵션 포함 시 ~$42,000. 공식 판매가 비공개, AxiomTest·Transcat 리스팅 기준 | APx525 (~$12–15K, 여전히 판매 중) / APx517B (acoustic test system) | Ref-tier. [ap.com/analyzers-accessories/apx555](https://www.ap.com/analyzers-accessories/apx555) |
| Neutrik Minilyzer ML1 | 휴대용 간이 측정 | $1,200 | — | 현장 스팟 체크 |
| **RME ADI-2 Pro FS R BE** | 고품질 ADC/DAC | ~$2,000 (EU: €1,688 incl. VAT); **단종 아님** (2026-04 Legacy 리스트 미포함) | RME Babyface Pro FS ($900) | −117 dB noise floor. 재고 변동 주의 |
| **RME Fireface UFX III** | 참고 비교용 32-ch 인터페이스 | **~$2,700 (EU €2,686 incl. VAT)** | UFX II (~$2,100) | Phase 3 예산에서 업그레이드 선택지 |
| Oscilloscope (Keysight DSOX1102G) | 파형 관찰 | $800 (2026-04 Keysight 리스트) | Rigol DS1054Z ($400, 4-ch hackable) / Rigol MSO5074 (~$1,500) | 4-ch 권장 |
| Multimeter (Fluke 87V) | DC/AC 측정 | $500 (2026-04 북미 리테일) | UT61E ($60) | 진공관 바이어스 측정용 |
| Function Generator (Keysight 33500B) | 테스트 신호 | $2,500 | JDS6600 ($80) | 저왜곡 필요 시 고가 권장 |
| 측정 마이크 (필요 시) | SPL/룸 측정 | $1,500 | Dayton UMM-6 ($80) | 스피커 로드 측정 시 |

> **가격 주의 (2026-04 재확인):** 위 가격은 2026-04 시점 공급업체 리스팅 기준. APx555B·Fireface UFX III·Fluke 87V 등은 제조사 공식 판매가가 비공개이거나 환율·관세·총판 마진에 따라 상당한 편차(±15%)가 있다. 실제 구매 시 AxiomTest, Transcat, Mouser, Sweetwater, Thomann, 리코더맨 등 복수 견적 필수. 측정 장비 가격은 분기 단위 재확인 권장.

### 18.2.2 소프트웨어 도구

| 소프트웨어 | 라이선스 | 용도 | 비고 |
|-----------|---------|------|------|
| Room EQ Wizard (REW) | Freeware | 일반 측정 (FR, THD, IR) | 필수 기본 도구 |
| ARTA | €99 | 왜곡 분석, 스텝 응답 | REW 보완 |
| Audio Precision APx500 | $0 (장비 포함) | APx 장비 전용 | 자동화 API 제공 |
| Reaper | $60 | 자동화 측정 세션 | ReaScript로 매크로 |
| Python (scipy, numpy, matplotlib) | Free | 커스텀 분석 | 본 프로젝트 주력 도구 |
| MATLAB / Octave | $ / Free | 학술 분석, DSP 검증 | 논문급 산출 |
| LTspice / QSPICE | Free | 회로 시뮬레이션 대조 | 측정 예측 |

### 18.2.3 예산 시나리오

**Budget Minimum (약 $3,500) — 프로토타입용**
- RME ADI-2 Pro FS ($2,000) 또는 Babyface Pro ($800)
- Rigol DS1054Z ($400)
- UT61E + 중급 멀티미터 ($150)
- JDS6600 또는 Behringer UCA222 LOopback ($80)
- REW (free) + ARTA (€99) + Reaper ($60)
- **한계:** noise floor ~ −110 dB, 수동 조작 많음, 고정밀 THD(<0.01%) 측정 어려움

**Professional ($10,000–15,000) — 논문/사업용**
- APx525 ($12,000)
- RME ADI-2 Pro FS ($2,000) — 백업/비교용
- Rigol MSO5074 ($1,500)
- Fluke 87V ($500)
- **한계:** 최고급 수준은 아니지만 논문 게재·사업 성과 제시로 충분. 일부 ultra-low THD(−130 dB) 측정 제약.

**Reference ($40,000+) — 연구소급**
- APx555 ($40,000)
- RME Fireface UFX III ($2,500)
- Keysight DSOX3054T ($8,000)
- Fluke 8845A benchtop DMM ($1,500)
- 정밀 voltage regulator, isolation transformer ($1,500)
- **한계:** 거의 없음. 업계 레퍼런스급. 개인 개발자에게는 과잉.

### 18.2.4 현실적 권장

**개인 개발자 시작점:** Phase 1($3,500) → 프로토타입 완성 후 Phase 2 업그레이드 검토.
**스타트업/연구팀:** Phase 2($12,000)로 시작, 데이터 구매·협력으로 보완.
**대학·기업 R&D:** Phase 3($50K+) 접근 가능.

---

## 18.3 측정 대상 하드웨어 확보 전략

### 18.3.1 소유 vs 대여 vs 협력 비교

| 전략 | 비용 | 접근 난이도 | 다중 유닛 가능? | 장점 | 단점 |
|------|------|------------|---------------|------|------|
| 자체 구매 (한 대) | 고가 ($3K–5K 빈티지) | 낮음 | 불가 | 시간 무제한 | 1대 → 통계 X |
| 스튜디오 대여 | 시간당 $50–200 | 중간 | 제한적 | 실제 환경 | 시간 압박 |
| 제조사 협력 | 무료 또는 할인 | 높음 | 가능 | 새 유닛 여러 대 | 협력 어려움 |
| 리뷰어/측정자 데이터 구매 | $0–500 | 낮음 | 다량 가능 | 빠름, 저렴 | 통제 불가 |
| 커뮤니티 측정 풀링 | 자율 | 낮음 | 매우 다량 | 스케일 | 품질 편차 |

### 18.3.2 추천 접근 (3단계)

**1단계 — 기존 데이터 수집 및 구매 (1–3개월)**

- YouTube 측정자 자료 체계적 수집:
  - Julian Krause 채널(프리앰프·컨버터 측정) — 대부분 APx555 기반
  - Audio Science Review (ASR) — Amir의 측정 데이터베이스
  - ProductionExpert, PureMix 등
- Audio Precision 공식 측정 리포트 접근 (일부 유료, ap.com/technical-library/datasheets)
- Gearspace (구 Gearslutz) 포럼 측정 보고서 아카이빙
- 학술 논문의 진공관·트랜스포머 측정 데이터 (JAES 등)

**Julian Krause YouTube 방법론 신뢰도 (검증 2024-2026):** Krause는 APx555 기반 측정을 공개하며, 오디오 인터페이스·프리앰프 리뷰에서 실제 −120 ~ −130 dB 노이즈 플로어를 제시한다. 측정 환경은 Windows+ASIO 기반, 측정 스프레드시트가 공개되어 커뮤니티 레퍼런스로 사용된다. Audient iD24·iD44 공식 리뷰 파트너십도 보유. 본 프로젝트의 1차 reference 데이터 소스로 활용 가능.

**AES/IEC/ITU 표준 문서 접근:**
- ITU-R BS.1770-5 (2023-11) — **무료 PDF** ([itu.int/rec/R-REC-BS.1770-5](https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en))
- AES **TD1008.1.21-9** (2021) / TD1004.1.15-10 (2015) — **무료 PDF** (aes.org/technical) — 스트리밍 배포 라우드니스 권고
- EBU R128 — **무료**
- IEC 60268 시리즈 (오디오 장비 측정) — **유료** (CHF 200~400/건, webstore.iec.ch)
- 따라서 현업 참고 표준은 무료로 확보 가능하며, IEC 계열만 일부 유료 구매 필요

**2단계 — 자체 한 대 확보 (3–6개월)**

대표 유닛 1–2대 확보. 타겟 우선순위:
- Neve 1073 (또는 1073DPA 클론) — 트랜스포머 + 진공관 성격 대표
- Pultec EQP-1A (또는 Warm Audio EQP-WA 클론) — 패시브 EQ + 튜브
- SSL G-Series bus compressor 클론 — VCA 기반 컴프 대표
- LA-2A (또는 Warm Audio WA-2A) — 옵토 컴프레서

빈티지 원본은 비싸고 상태 편차 크므로 **고급 클론**이 현실적. Warm Audio, Heritage Audio, BAE 등.

**3단계 — 스튜디오 협력으로 Multi-Unit 측정 (6–12개월)**

- 한국: 엠넷, SBS 등 방송국 스튜디오, 대형 레코딩 스튜디오
- 해외: Abbey Road Institute, Mix With The Masters 제휴 스튜디오

협력 조건:
- 무료 측정 제공 → 스튜디오에는 측정 리포트 제공
- 측정 결과의 연구/상업적 이용 동의서
- 장비를 옮기지 않고 현장에서 측정

### 18.3.3 대여 가능한 스튜디오 (2025–2026 기준 참고)

**서울 지역:**
- Louder Studio — Neve 1073, API 1608 등 다수
- Ingrid Studio — Vintage Neve, Pultec
- Sound Mirror — SSL, Neve
- Mastering Lab (주요 마스터링 스튜디오) — 마스터링 장비 풀

**해외:**
- Abbey Road Studios (UK) — 측정 협력 가능, 요금 높음
- Pachyderm Recording (US, Minnesota) — 빈티지 장비 풍부
- Capitol Studios (US) — API, Neve 다수
- Electric Lady Studios (US, NY) — Neve 8078 원본

**협력 접근 시 주의:**
- 보험(장비 파손 보상) 필수
- 운영시간 외 대여(야간)가 비용 절감
- NDA 필요 시 협의

### 18.3.4 대안: 측정 데이터 구매

같은 하드웨어를 여러 번 측정한 데이터를 가진 업체·개인에게 구매:

- Julian Krause 개인 컨설팅 ($500–2,000)
- Audio Precision 레퍼런스 측정 세트 (일부 공개)
- 학술 연구실 협력 (데이터 공유)

이 경우 **원시 데이터(raw WAV, 매개변수 기록)** 요구. 가공된 그래프만으로는 파라미터 피팅 불가.

---

## 18.4 측정 프로토콜 — Single Unit

### 18.4.1 환경 통제

| 항목 | 요구 사양 | 비고 |
|------|-----------|------|
| 실내 온도 | 22°C ± 2°C | 계측기 데이터시트 기준 |
| 상대 습도 | 40–60% | 트랜스포머·커패시터 영향 |
| 전원 전압 | 220V ± 1% (KR), 120V ± 1% (US) | Voltage regulator 권장 |
| 접지 | 단일 접지(Star ground) | Ground loop 방지 필수 |
| 워밍업 | 진공관 장비 30분, TR 15분 | 열평형 도달 |
| 전자기 환경 | 60Hz 허용, RF 최소 | 형광등 OFF, WiFi 멀리 |

**측정 세션 시작 체크리스트:**

- [ ] 온도/습도 기록
- [ ] 라인 전압 측정 후 기록
- [ ] 모든 장비 접지 확인
- [ ] 장비 워밍업 30분 이상
- [ ] 자체 loopback으로 noise floor 확인
- [ ] 케이블 쇼트/오픈 확인

### 18.4.2 기본 측정 세트 (7종)

#### 1) 주파수 응답 (Frequency Response)

- **입력:** 로그 스윕 20Hz–20kHz, −20 dBu
- **측정:** 출력 FFT, 스윕 deconvolution (Farina ESS)
- **기록:**
  - −3 dB 저/고 주파수
  - 중역(1kHz) 대비 평탄성 (편차 dB)
  - 공진 피크/딥 유무

#### 2) THD vs 주파수

- **입력:** 사인파 50/100/500/1k/5k/10k Hz, −20 dBu
- **측정:** 각 주파수의 THD+N, 고조파 H2–H7 개별 레벨
- **기록:**
  - THD+N (%)
  - H2, H3, H4, H5, H6, H7 각각 dBc (대비 기본파)
  - 저역 고조파 증가 여부 (트랜스포머 포화)

#### 3) THD vs 레벨 (Distortion Sweep)

- **입력:** 1kHz 사인파, −30 ~ +20 dBu (2 dB 스텝, 26 포인트)
- **측정:** 각 레벨의 THD+N과 고조파 분포
- **기록:**
  - 레벨 vs THD 곡선
  - Soft clip / hard clip 전환점
  - H2/H3 비율 변화 (진공관 특성 지표)

#### 4) IMD (SMPTE + CCIF)

- **SMPTE:** 60Hz + 7kHz (4:1 amplitude ratio), −10 dBu 합
- **CCIF:** 19kHz + 20kHz (1:1), −10 dBu 합
- **기록:**
  - SMPTE IMD (%)
  - CCIF: 1kHz 차주파 성분 레벨
  - 고차 IMD 산포

#### 5) 시변성 (Time-Variance)

- **절차:**
  1. 냉시작 (전원 OFF 1시간 이상)
  2. 전원 ON 후 즉시 1kHz, −10 dBu 연속 재생
  3. 60분 동안 재생 지속
  4. 1분 간격으로 THD 측정 자동 기록
- **기록:**
  - 시간에 따른 THD 변화 곡선
  - 열평형 도달 시점
  - 드리프트 총량 (ΔTHD, ΔFR)

#### 6) Burst Response (캐소드 바운스 등 메모리 효과)

- **입력 시퀀스:**
  - 30초 −40 dBu (조용한 구간)
  - 1초 +10 dBu burst (큰 신호)
  - 30초 −20 dBu (복구 관찰)
- **측정:** burst 후 5초간 THD를 100ms 단위로 기록
- **기록:**
  - 바이어스 복귀 시정수 (τ)
  - 복귀 후 잔류 비대칭성

#### 7) 위상 응답 (Phase & Group Delay)

- **입력:** Farina ESS 로그 스윕
- **추출:** complex FR → 위상 → group delay
- **기록:**
  - 위상 응답 곡선 (°)
  - Group delay (ms) 주파수 별
  - 트랜스포머 저역 위상 회전 특성

### 18.4.3 측정 데이터 기록 템플릿

```yaml
# 측정 세션 메타데이터
Unit_ID: "Neve_1073_SN_12345"
Measurement_Date: "2026-03-15"
Operator: "K. Hwang"
Location: "Louder Studio, Seoul"

# 환경
Temperature_C: 22.5
Humidity_pct: 45
Line_Voltage_V: 220.3
Warmup_min: 45

# 설정
Gain_dB: 40
Input_Impedance_Ohm: 10000
Output_Load_Ohm: 600
Pad: 0

# 계측기
Analyzer: "Audio Precision APx525"
Analyzer_SN: "APX525-XXXX"
Loopback_NoiseFloor_dB: -117.3

# 결과 1: Frequency Response
Frequency_Response:
  - { freq_Hz: 20,    level_dB: -1.2 }
  - { freq_Hz: 50,    level_dB: -0.3 }
  - { freq_Hz: 100,   level_dB:  0.0 }
  - { freq_Hz: 1000,  level_dB:  0.0 }
  - { freq_Hz: 10000, level_dB:  0.1 }
  - { freq_Hz: 20000, level_dB: -0.8 }

# 결과 2: THD vs Level
THD_vs_Level_1kHz:
  - { level_dBu: -20, THD_pct: 0.008, H2_dBc: -70, H3_dBc: -88, H4_dBc: -95, H5_dBc: -100 }
  - { level_dBu:   0, THD_pct: 0.08,  H2_dBc: -50, H3_dBc: -75, H4_dBc: -82, H5_dBc:  -88 }
  - { level_dBu: +10, THD_pct: 0.35,  H2_dBc: -38, H3_dBc: -58, H4_dBc: -68, H5_dBc:  -75 }

# 결과 3: Time-Variance
Time_Variance:
  - { time_min:  0, THD_1kHz_pct: 0.015 }
  - { time_min:  5, THD_1kHz_pct: 0.022 }
  - { time_min: 15, THD_1kHz_pct: 0.030 }
  - { time_min: 30, THD_1kHz_pct: 0.035 }
  - { time_min: 60, THD_1kHz_pct: 0.038 }

# 원시 데이터 링크
Raw_Files:
  - path: "./raw/sweep_20_20k.wav"
    sha256: "abc123..."
  - path: "./raw/thd_1khz_burst.wav"
    sha256: "def456..."
```

---

## 18.5 측정 프로토콜 — Multi-Unit (통계 분포 추출)

### 18.5.1 목표

- 같은 모델 **10–50대**를 동일 프로토콜로 측정
- 각 파라미터의 **평균·표준편차·분포형**을 추출
- Component variation model(06 문서)의 실제 데이터 기반 제공

### 18.5.2 프로토콜

- **동일 환경:** 한 스튜디오의 한 세션에서 일괄 측정 (장소·시간 분산 시 편차 혼재)
- **동일 순서:** 각 유닛에 대해 §18.4.2의 7가지 측정을 동일한 순서로 실행
- **식별:** 고유 ID 부여 — 모델명 + S/N + 제조년월 + (보정 이력)

| 유닛 ID | S/N | 제조년월 | THD@1kHz+4dBu (%) | f_low (−3dB) | H2/H3 비율 |
|--------|-----|---------|-------------------|-------------|-----------|
| N1073_001 | 12345 | 2018-03 | 0.085 | 18 Hz | 3.1 |
| N1073_002 | 12789 | 2019-11 | 0.093 | 22 Hz | 2.8 |
| N1073_003 | 13102 | 2020-05 | 0.078 | 17 Hz | 3.3 |
| ... | ... | ... | ... | ... | ... |

### 18.5.3 통계 분석

**분포 추정:**

- 각 파라미터의 평균 μ, 표준편차 σ
- 정규성 검정 (Shapiro-Wilk)
- 비정규일 경우 log-normal 등 대안 분포 피팅

**파라미터 간 상관:**

- PCA로 주요 변동 축 추출
- 상관행렬로 coupling 식별 (예: "f_low와 H3는 트랜스포머 품질에서 같이 움직임")

**이상치 처리:**

- 고장 유닛(bias drift, 부품 불량) 제거
- Cook's distance, Z-score 기준

```python
import numpy as np
import pandas as pd
from scipy import stats

df = pd.read_csv("multi_unit_measurements.csv")

# 기본 통계
print(df.describe())

# 정규성 검정
for col in ['THD_1kHz', 'f_low', 'H2_H3_ratio']:
    stat, p = stats.shapiro(df[col])
    print(f"{col}: Shapiro p = {p:.4f}")

# 상관행렬
print(df.corr())

# PCA
from sklearn.decomposition import PCA
pca = PCA(n_components=3)
pca.fit(df[['THD_1kHz', 'f_low', 'H2_H3_ratio']].values)
print("Explained variance:", pca.explained_variance_ratio_)
```

### 18.5.4 현실적 제약

**솔직하게 말하자면:**

- 개인 개발자가 같은 모델 10대를 측정하는 것은 거의 불가능
- 방송국·대형 스튜디오 협력 아니면 **제조사 새 유닛 협력**이 유일한 길
- 빈티지 원본은 상태 편차가 너무 커서 통계로 잡기 어려움 (각 유닛이 "다른 악기")
- 현실적 타협: **3–5대 직접 측정 + 문헌 데이터 보간 + 시뮬레이션 외삽**

**현실적 로드맵:**

1. 3대 직접 측정 (본인이 가능한 범위)
2. 공개 측정 데이터 10건 이상 수집 (Julian Krause, ASR, 포럼)
3. 합쳐서 σ 추정 (과소 추정 우려 → 보수적으로 1.2배)
4. 장기적으로 제조사/스튜디오 협력 확대

---

## 18.6 불확실성 / 오차 분석

### 18.6.1 측정 오차 요인

| 오차 원인 | 크기 (대략) | 완화 방법 |
|----------|-----------|----------|
| 계측기 noise floor | −110 ~ −120 dB | 저노이즈 장비, 평균화 |
| 케이블/커넥터 손실 | < 0.1 dB @ 10 kHz | 고품질 케이블, 짧게 |
| 전원 변동 | ± 0.5% (규제 전원) | Voltage regulator |
| 온도 변동 | ΔTHD ~ 10% per 5°C | 항온 실 또는 기록 |
| Ground loop | 60 Hz 혐 +20 dB | Star ground, iso trans |
| 커넥터 접촉저항 | 0.01–0.1 Ω | 금도금, 정기 청소 |

### 18.6.2 오차 버젯 (Error Budget)

**APx555 기준 사양:**

- THD 측정 정확도: ±0.5% at −60 dBc, < 1% at −80 dBc
- Residual THD+N: < −110 dB (A-weighted)
- FR 정확도: ±0.05 dB (20 Hz – 20 kHz)

**측정 대상의 THD가 −60 dBc(0.1%)라면:**

- 계측기 자체 오차 ±0.5%
- 케이블/로드 ±0.2%
- 환경(온도/전원) ±1%
- **총 불확실성: ±1.5% (상대) 또는 ±2 dB (하모닉 레벨)**

**중요한 규칙:** 측정 장비의 residual은 측정 대상보다 **20 dB 이상 낮아야** 신뢰 가능. APx555는 < −110 dB이므로 −90 dBc 하모닉까지 측정 가능.

### 18.6.3 반복성 (Repeatability / Reproducibility)

- 같은 유닛, 같은 세션 5회 측정 → 표준편차
- 이상적: H2/H3 레벨 σ < 0.5 dB
- 실제: σ < 1 dB이면 수용
- σ > 2 dB이면 측정 setup 재검토 (접촉불량, 온도드리프트 등)

---

## 18.7 파라미터 피팅 (측정 → 모델 파라미터)

### 18.7.1 Koren 파라미터 피팅 (진공관)

**배경:** Koren 진공관 모델은 5개 파라미터(μ, Ex, Kg, Kp, Kvb)로 플레이트 전류를 기술:

$$I_p = \frac{1}{Kg} \cdot \left[\frac{Vp}{Kp} \ln\left(1 + e^{Kp\left(\frac{1}{\mu} + \frac{Vg}{\sqrt{Kvb + Vp^2}}\right)}\right)\right]^{Ex}$$

**피팅 절차:**

1. 플레이트 특성 측정 (곡선 추적기 또는 자체 제작 jig):
   - Vp: 0–400 V, 50 V 스텝
   - Vg: 0, −1, −2, −4, −8 V
   - Ip 측정
2. Levenberg-Marquardt로 Koren 식 피팅
3. RMS 잔차 평가

```python
import numpy as np
from scipy.optimize import least_squares

def koren_current(Vp, Vg, mu, Ex, Kg, Kp, Kvb):
    E1 = (Vp / Kp) * np.log(1 + np.exp(Kp * (1.0/mu + Vg / np.sqrt(Kvb + Vp**2))))
    return (E1 ** Ex) / Kg

def koren_residual(params, Vp_data, Vg_data, Ip_measured):
    mu, Ex, Kg, Kp, Kvb = params
    Ip_model = np.array([
        koren_current(Vp, Vg, mu, Ex, Kg, Kp, Kvb)
        for Vp, Vg in zip(Vp_data, Vg_data)
    ])
    return Ip_measured - Ip_model

# 12AX7 초기 추정값 (문헌)
x0 = [100, 1.4, 1060, 600, 300]

result = least_squares(
    koren_residual, x0=x0,
    args=(Vp_data, Vg_data, Ip_measured),
    method='lm'
)

print("Fitted params:", result.x)
print("RMS residual:", np.sqrt(np.mean(result.fun**2)))
```

### 18.7.2 Jiles-Atherton 파라미터 피팅 (트랜스포머 코어)

**측정 절요:**

- 트랜스포머 1차에 삼각파/사인파 전류 인가
- 2차 오픈 상태에서 1차 전압 측정 (← dΦ/dt)
- Φ 적분 → B 도출
- 측정된 B-H 궤적에 JA 식 피팅

**파라미터:** Ms (포화자화), a (anhysteretic), α (도메인 결합), k (손실), c (역가역)

초기 추정값(코어 재질별):
- GOES (Grain-Oriented Electrical Steel): Ms ~ 1.7 MA/m, k ~ 50 A/m
- Nickel (Hi-Mu): Ms ~ 0.5 MA/m, k ~ 5 A/m
- Permalloy: Ms ~ 0.8 MA/m, k ~ 10 A/m

### 18.7.3 신경망 보조 피팅 (Grey-Box)

**잔차 학습:** 물리 모델(Koren+JA)로 1차 예측 → 측정과의 잔차를 소형 MLP가 학습:

$$y_\text{true} = y_\text{physics}(x; \theta_\text{fitted}) + f_\text{NN}(x; \phi)$$

- 순수 black-box보다 해석 가능
- 순수 white-box보다 정확
- 06 문서의 개체차 파라미터 θ는 물리 모델, NN 가중치 φ는 "잔여 비선형성"으로 해석

---

## 18.8 자동화 측정 세션

### 18.8.1 자동화 도구

| 도구 | 언어/API | 용도 |
|------|---------|------|
| REW API | Java/CLI | FR/THD/IR 자동 실행 |
| APx500 APX API | .NET (C#/VB) | Audio Precision 전용 |
| MATLAB Instrument Control Toolbox | MATLAB | GPIB/USB 계측기 제어 |
| Python + pyvisa | Python | 오실로스코프·DMM 제어 |
| Reaper ReaScript | Lua/Python | DAW 자동 세션 |

### 18.8.2 워크플로

```
[1] 하드웨어 연결 확인 (loopback 체크)
      ↓
[2] 자동 레벨 캘리브레이션 (0 dBu → 0 dBFS)
      ↓
[3] 측정 매크로 실행
      - FR sweep
      - THD sweep (50/100/500/1k/5k/10k)
      - Level sweep (−30 ~ +20 dBu)
      - IMD (SMPTE, CCIF)
      - Time variance (60 min)
      - Burst response
      ↓
[4] 결과 자동 저장 (YAML + raw WAV)
      ↓
[5] 자동 보고서 생성 (Python + matplotlib → PDF)
      ↓
[6] DB에 업로드 (SQLite 또는 PostgreSQL)
```

### 18.8.3 예상 소요 시간

| 모드 | 유닛당 시간 | Multi-Unit 10대 | 비고 |
|------|------------|----------------|------|
| Manual | 4–8 시간 | 불가능 | 지침 변동 위험 |
| Semi-Auto (REW 매크로) | 2–3 시간 | 2–3일 | 현실적 |
| Full-Auto (APx + Python) | 1–2 시간 | 1–2일 | 장비 요구 |

---

## 18.9 예산 계획 (현실적 로드맵)

### Phase 1 — Budget Start ($3,500)

| 항목 | 가격 |
|------|------|
| RME ADI-2 Pro FS | $2,000 |
| Rigol DS1054Z | $400 |
| UT61E + Fluke 101 | $150 |
| JDS6600 function gen | $80 |
| REW + ARTA + Reaper | $170 |
| 케이블·커넥터·어댑터 | $300 |
| voltage regulator | $200 |
| 예비 | $200 |
| **합계** | **~$3,500** |

→ **Neve 1073 한 대 + 기본 측정 가능**. 논문급은 아니지만 프로토타입 개발 충분.

### Phase 2 — Professional ($15,000)

Phase 1 + 아래:

| 항목 | 가격 |
|------|------|
| APx525 (중고 또는 신품) | $12,000 |
| 추가 측정 마이크 | $500 |
| 고급 케이블·적재 | $500 |
| **추가 합계** | **~$13,000** |

→ **학술급 측정, 논문급 데이터**, 상업 제품 측정 인증 가능.

### Phase 3 — Reference ($50,000+)

| 항목 | 가격 |
|------|------|
| APx555 | $40,000 |
| Keysight DSOX3054T | $8,000 |
| Fluke 8845A | $1,500 |
| Precision voltage source | $2,000 |
| **추가 합계** | **~$51,500** |

→ **업계 최고 수준**. 대학/기업 R&D급.

### 권장 시작

**개인 개발자/스타트업:** Phase 1로 시작 → 프로토타입 검증 → 자금 확보 후 Phase 2 업그레이드.
**대학/기업:** Phase 2 직행, 필요시 Phase 3.

---

## 18.10 측정 → 모델 파라미터 → 플러그인 프리셋 파이프라인

```
[실제 하드웨어]
     │
     │ §18.4 측정
     ▼
[YAML/JSON Raw Data + WAV 원시 파일]
     │
     │ Python 파싱 (§18.11.1 형식)
     ▼
[Python 분석 스크립트]
     │
     │ §18.7 피팅 (Levenberg-Marquardt + NN)
     ▼
[Koren / JA 파라미터 세트]
     │
     │ Monte Carlo 분포 주입 (§06)
     ▼
[Unit 파라미터 샘플러 (μ, σ)]
     │
     │ 플러그인 빌드에 내장
     ▼
[Factory Preset (XML/JUCE ValueTree)]
     │
     │ 플러그인 런타임 초기화
     ▼
[사용자에게 노출되는 "Neve 1073 Unit #3" 등]
```

**핵심:** 이 파이프라인의 입력이 없으면 출력도 없다. 측정 인프라는 "이론 → 제품" 단절을 메우는 유일한 다리이다.

---

## 18.11 데이터 관리

### 18.11.1 저장 형식

| 형식 | 용도 | 예시 |
|------|------|------|
| WAV (24bit / 96kHz) | 원시 측정 신호 | `sweep_20_20k_N1073_001.wav` |
| YAML | 측정 메타데이터 | `session_20260315.yaml` |
| JSON | 가공된 파라미터 세트 | `koren_params_N1073_001.json` |
| Parquet | 대량 다중 유닛 DB | `multi_unit_db.parquet` |
| PDF | 자동 보고서 | `report_N1073_001.pdf` |

### 18.11.2 버전 관리

- **Git** — YAML/JSON/코드만 (소형 파일)
- **Git LFS** — WAV 파일 (대용량)
- 측정 세션 단위 commit, 태그 부여:
  - `session/2026-03-15-Neve1073-3units`

### 18.11.3 백업 전략

- **로컬 NAS** (Synology 등) — 주요 저장소
- **클라우드** (Google Drive, Backblaze B2) — 2차 백업
- **오프사이트** — 물리 HDD 분리 보관 (1년 1회)
- **3-2-1 룰 준수:** 3 사본, 2 매체, 1 오프사이트

### 18.11.4 메타데이터 표준

최소 필드 (YAML):
```yaml
unit_id: str
model: str
manufacture_date: date
measurement_date: datetime
operator: str
location: str
environment:
  temperature_C: float
  humidity_pct: float
  line_voltage_V: float
instruments:
  analyzer: str
  analyzer_sn: str
  loopback_noise_dB: float
settings:
  gain_dB: float
  impedance_ohm: float
results:
  frequency_response: [...]
  thd_vs_level: [...]
  time_variance: [...]
raw_files:
  - path: str
    sha256: str
```

---

## 18.12 검증 루프 (세션 체크리스트)

### 매 측정 세션 시작

- [ ] 장비 워밍업 30분 이상
- [ ] 환경 기록 (T, RH, line voltage)
- [ ] Loopback 테스트 → noise floor 확인
- [ ] 측정 대상 DUT(Device Under Test) 연결 검증
- [ ] 케이블 커넥터 상태 확인

### 측정 실행

- [ ] Protocol §18.4.2 순서대로 실행
- [ ] 각 결과 초기 sanity check (이상치 탐지)
- [ ] 의심 결과는 즉시 재측정

### 측정 종료

- [ ] 원시 데이터 백업 (로컬 + NAS)
- [ ] YAML 메타데이터 완성
- [ ] 분석 스크립트 자동 실행
- [ ] PDF 보고서 생성 및 검토
- [ ] Git commit + LFS push

### 주기적 검증

- [ ] **월간:** 계측기 self-test 및 교정 상태 확인
- [ ] **분기:** 레퍼런스 DUT (변하지 않는 기준 회로) 재측정
- [ ] **연간:** APx 장비 공장 교정 (권장)

---

## 18.13 참고자료

### 학술·기술 문서
- Audio Precision. *APx500 User Manual* (최신판)
- Horowitz, P. & Hill, W. *The Art of Electronics* (3rd ed.) — 측정 이론
- Metzler, B. *Audio Measurement Handbook* (Audio Precision, free PDF)
- Whitlock, B. *Measuring Audio Transformers* (Jensen Application Note AN-002)
- Cordell, B. *Designing Audio Power Amplifiers* — 측정 챕터

### 진공관 모델링
- Koren, N. "Improved Vacuum-Tube Models for SPICE Simulations" (Glass Audio, 1996)
- Pakarinen, J. & Karjalainen, M. "Enhanced Wave Digital Triode Model for Real-Time Tube Amplifier Emulation" (IEEE Trans. ASLP, 2010)

### 트랜스포머 모델링
- Jiles, D.C. & Atherton, D.L. "Theory of ferromagnetic hysteresis" (J. Magn. Magn. Mater., 1986)
- Sarker, P.S. et al. "Recent advances on the Jiles-Atherton hysteresis model" (IET Electric Power Applications, 2020)

### 측정 실무 (온라인)
- Julian Krause YouTube — 프리앰프·컨버터 측정 시리즈
- Audio Science Review (audiosciencereview.com) — Amir의 측정 DB
- Gearspace Forum — Measurement subforum

### 소프트웨어
- Room EQ Wizard: roomeqwizard.com
- ARTA: artalabs.hr
- Python scipy.optimize 문서 — Levenberg-Marquardt

---

**작성자 주:** 본 문서의 비용·대여처 정보는 2025–2026년 기준이며, 시간이 지나면 갱신이 필요하다. 특히 스튜디오 대여 조건, 제조사 협력 조건은 개별 접촉으로 확인할 것.
