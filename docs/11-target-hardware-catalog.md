# 11. 타깃 하드웨어 카탈로그 (Target Hardware Catalog)

> **문서 목적**: 본 플러그인이 모델링 대상으로 삼는 레전더리 아날로그 하드웨어 유닛들의 회로 토폴로지, 측정된 하모닉 프로파일, 믹싱 엔지니어 관점의 사운드 캐릭터, 그리고 본 플러그인에서의 재현 접근 전략을 정리한다. "우리가 구체적으로 어떤 하드웨어를 재현할 것인가"에 대한 명시적 기준점을 제공하는 것이 목표이다.

## 관련 문서

- [`00-README.md`](./00-README.md) — 전체 문서 네비게이션
- [`01-vacuum-tube-physics.md`](./01-vacuum-tube-physics.md) — 진공관 물리 모델(Koren, JA)
- [`02-transformer-physics-and-distortion.md`](./02-transformer-physics-and-distortion.md) — 트랜스포머 히스테리시스 및 포화
- [`03-time-varying-nonlinearities.md`](./03-time-varying-nonlinearities.md) — 시변 비선형성 (캐소드 바운스, PSU sag, bias drift)
- [`04-circuit-interactions-and-impedance.md`](./04-circuit-interactions-and-impedance.md) — 회로 간 임피던스 상호작용
- [`05-harmonic-spectrum-and-psychoacoustics.md`](./05-harmonic-spectrum-and-psychoacoustics.md) — 하모닉 스펙트럼과 심리음향
- [`08-competitive-analysis.md`](./08-competitive-analysis.md) — 경쟁 플러그인 분석
- [`17-legal-and-licensing.md`](./17-legal-and-licensing.md) — 법적/라이선스 이슈 (*예정*)

---

## 목차

| # | 유닛 | 카테고리 | 주요 캐릭터 |
|---|------|----------|-------------|
| 11.1 | Neve 1073 | Mic Preamp + EQ | Transformer punch, mid warmth |
| 11.2 | Pultec EQP-1A | Passive EQ + Tube Makeup | "Boost & cut" magic, silky top |
| 11.3 | Fairchild 670 | Variable-Mu Compressor | Glue, thickness, low-mid saturation |
| 11.4 | Teletronix LA-2A | Optical Compressor | Slow, musical, program-dependent |
| 11.5 | Urei 1176 | FET Compressor | Fast, aggressive punch (참고) |
| 11.6 | Manley Vari-Mu | Variable-Mu Compressor | Open, airy, modern Vari-Mu |
| 11.7 | Thermionic Culture Vulture | Multi-Stage Distortion | Extreme tube color, pentode/triode |
| 11.8 | Altec 436C | Variable-Mu Compressor (vintage) | Motown-era broadcast compression |
| 11.9 | RCA BA-6A | Variable-Mu Compressor (vintage) | Radio/mastering legendary warmth |
| 11.10 | EMI TG12345 | Transformer-coupled console | Abbey Road 70s colour |

추가 섹션:
- **11.11** 교차 비교 매트릭스 (Cross-Comparison Matrix)
- **11.12** MVP 타깃 선정 가이드
- **11.13** 법적/상표 주의

---

## 11.1 Neve 1073 (Mic Preamp + EQ)

### 11.1.1 개요

- **출시 연도**: 1970년 (Rupert Neve 설계, Neve Electronics)
- **원설계자**: Rupert Neve
- **시대적 맥락**: Neve 80 시리즈 콘솔(8014, 8048, 8068 등)의 채널 모듈로 탄생. 1970년대 영국 록/팝 황금기의 사운드를 정의.
- **대표 사용 음반/엔지니어**:
  - Led Zeppelin 후기 작업(Olympic Studios 8048)
  - Nirvana *Nevermind* (Sound City 8028)
  - Dave Grohl, Steve Albini, Chris Lord-Alge 등이 "go-to" preamp으로 언급
  - 2010년대 이후 "Neve sound"는 사실상 업계 기준으로 자리잡음

### 11.1.2 회로 토폴로지

- **진공관 구성**: **없음** (Class A **discrete BJT** design — BA283 채널 앰프 보드 기반).
  - 실제 능동 소자는 **BC184** (소신호 NPN) 계열과 **출력 파워 BJT**. AMS Neve 공식 사사("1073 History", ams-neve.com/consoles/history-of-1073/)는 Rupert Neve가 "BC182/BC184"를 주요 트랜지스터로 선택했다고 기술함. 후기 리비전/클론에서는 BC184L/BC214L suffix 변형이 사용된다.
  - 엄밀히 말해 1073은 "진공관이 아닌" 디스크리트 트랜지스터 프리앰프이지만, **트랜스포머-커플드 Class A 설계**로 인해 진공관과 유사한 하모닉 프로파일을 생성함. **12AX7 등 진공관은 사용되지 않는다** — 상업 플러그인 2차 문헌에서 종종 "튜브-같은 소리"라는 표현이 "내부에 진공관이 있다"로 잘못 번역되는 경우가 있으나, 실제 BA283은 순수 discrete solid-state이다.
- **주요 트랜스포머** (AMS-Neve 공식 문서 기준):
  - **Mic Input Transformer**: Marinair **10468** (TF10003), 1967-68년 Rupert Neve 지휘 하 David Rees가 Marinair의 Peter Hurst와 공동 개발
  - **Line Input Transformer**: Marinair **31267** (같은 1967-68년 협업)
  - **Output Transformer**: Marinair **LO1166** (gapped-core, 1964년 Rupert Neve가 Phillips Records용 트랜지스터 콘솔을 위해 자체 설계, 1966년 Marinair에 양산 이관)
  - Marinair 트랜스포머가 "Neve sound"의 80% 이상을 결정한다는 것이 업계 consensus (단, 1차 출처 없는 정성적 주장)
  - *(주의: 과거 본 문서가 입력 LO1166 / 출력 LO1167으로 표기했으나, AMS-Neve 공식 사사에 따르면 LO1166은 출력 트랜스포머이고 입력단은 Marinair 10468/31267임. 수정됨.)*
- **신호 체인 순서**:
  1. Input Marinair transformer (첫 번째 채색 단계)
  2. Class A discrete amp stage (2× 2N3055 또는 BC184)
  3. Passive inductor-based EQ (Carnhill 또는 Marinair inductor)
  4. Makeup gain stage (Class A)
  5. Output Marinair transformer (두 번째 채색 단계)
- **특이한 회로 요소**:
  - **Passive inductor EQ**: 3-band (HF shelving, MF peaking, LF shelving) with air gap inductor
  - **Class A biasing**: 항상 최대 전력 소모, 크로스오버 왜곡 없음
  - **+48V phantom on Marinair**: 프리앰프 입력단 impedance가 소스 임피던스에 따라 3~10 kΩ 범위로 변동

### 11.1.3 하모닉 프로파일 (측정 기반)

1kHz sine, +4 dBu 입력, 26 dB gain, unity output 기준:

| Harmonic | Level (dBc) | 비고 |
|----------|-------------|------|
| H2 | −55 ~ −60 dBc | 중-저 출력에서 약 0.1~0.2% THD |
| H3 | −70 ~ −75 dBc | |
| H4 | −85 dBc 이하 | 거의 측정 noise floor |
| H5 | −95 dBc 이하 | |

**고출력 시(+18 dBu 이상)**:
- H2 레벨이 −40 dBc까지 상승 (약 1% THD)
- 트랜스포머 포화에 의한 **소프트 클립** 발생
- H3/H2 비율은 여전히 낮게 유지 (~1:3)

**주파수별 THD 특성**:
- **저역 (20~200 Hz)**: Marinair 코어 포화로 인해 H3 성분이 상대적으로 증가. 50 Hz에서 약 +6 dB 더 많은 H3.
- **중역 (200~3000 Hz)**: 최저 THD 영역. "투명한" 느낌의 소스.
- **고역 (3~20 kHz)**: Transformer leakage inductance 때문에 고역 롤오프 시작 (~15 kHz −1 dB), 이것이 "silky top" 캐릭터의 원인.

**IMD (SMPTE 60/7000 Hz, 4:1)**:
- +4 dBu 출력에서 0.02% 이하
- +18 dBu 출력에서 0.3~0.5%

**출처**:
- Julian Krause YouTube 측정 (2021, "Neve 1073 vs clones")
- AES Convention Paper 8901 (Hughes, 2013) "Measuring the Classic Console"
- Gearspace "Neve 1073 shootout" thread (2018)

### 11.1.4 사운드 캐릭터 (엔지니어 관점)

- **"두꺼움 (thickness)"**: 저역에서의 H2+H3 증가 → 60~250 Hz 대역이 실제 EQ boost 없이도 "살집" 있게 들림.
- **"펀치 (punch)"**: Transformer saturation이 transient 압축 역할 → 드럼, 베이스에서 peak energy가 soft-knee로 깎임.
- **"에어 (air)"**: 10 kHz 근방의 약간의 phase shift + 15 kHz의 gentle roll-off가 역설적으로 "부드럽고 공기감 있는" 고역을 만듦.
- **잘 맞는 소스**:
  - 록/팝 보컬 (중역 warmth)
  - 스네어, 킥 (transformer punch)
  - 일렉기타 DI (하모닉 enrichment)
  - 마스터 버스 (subtle glue)
- **피해야 할 소스**:
  - 이미 과포화된 디지털 소스 (중복 coloration)
  - 하이엔드 투명성이 중요한 클래식/재즈 (특정 장르)
  - 매우 빠른 transient가 필요한 타악기 일부 (snap이 둔해질 수 있음)

### 11.1.5 본 플러그인에서의 재현 접근

- **필요한 물리 모델**:
  - **주**: Transformer 히스테리시스 모델 (Jiles-Atherton, 문서 02 참조)
  - **부**: Class A BJT saturation (Ebers-Moll 간략화)
  - **진공관 모델 불필요** (1073은 solid-state)
- **핵심 시변 효과**:
  - **Transformer core memory**: 이전 신호 히스토리가 현재 B-H 루프 위치에 영향
  - **Low-frequency saturation onset**: 주파수 의존적 포화 threshold (λ(f) = λ_nom × (f/f_nom)^−0.5)
  - **PSU sag는 약함** (Class A는 정전류 소모)
- **구현 우선순위**:
  - **MVP**: JA 트랜스포머 모델 (입력 + 출력) + static BJT nonlinearity (Chebyshev fit)
  - **확장판**: Inductor EQ를 정확한 회로 네트워크로 (state-space), impedance interaction 추가

---

## 11.2 Pultec EQP-1A (Passive EQ + Tube Makeup Amp)

### 11.2.1 개요

- **출시 연도**: Pulse Techniques, Incorporated 공식 설립은 **1953년 2월 1일**(공식 사사 pulsetechniques.com/history). 원형 **EQP-1은 1955~1956년경** MGM Studios 테스트 이후 정식 출시(Pulse Techniques 공식 사사 및 Wikipedia 상호 교차). **EQP-1A는 1961년** 발매, 20 Hz boost/attenuation, 16 kHz boost, 5/10/20 kHz attenuation이 추가됨.
- **원설계자**: **Eugene Richard Shenk** & **W. Oliver "Ollie" Summerlin** (Pulse Techniques 공식 사사 기준 풀네임. 많은 2차 자료에 "Summerland"로 잘못 표기되어 있음).
- **시대적 맥락**: 미국 방송/마스터링 업계의 표준. Frank Sinatra 시대부터 현대 EDM 마스터링까지 연속적으로 사용.
- **대표 사용 음반/엔지니어**:
  - Bob Ludwig, Chris Lord-Alge, Andrew Scheps의 보컬 및 마스터 체인
  - Michael Jackson *Thriller* (Bruce Swedien)
  - 모든 현대 힙합/팝 마스터링 스튜디오의 필수품
  - "Pultec trick" (저역 동시 boost + cut)은 사실상 모든 마스터링 엔지니어가 사용

### 11.2.2 회로 토폴로지

- **진공관 구성**: **12AX7 (ECC83) x1** (입력 게인 단), **12AU7 (ECC82) x1** (출력 트랜스 드라이버), **6X4 rectifier x1**
  - EQP-1A3(후기 모델)은 12AX7 대신 12AY7 사용
- **주요 트랜스포머** (vintage Pultec 공식/Wikipedia 교차 확인):
  - **Input & Interstage Transformer**: **Triad** 사 제조 (original specification)
  - **Output Transformer**: **Peerless** 사 커스텀 (S-217D 계열)
  - *(주의: 구판 본 문서가 입출력 모두 Peerless로 표기했으나 오리지널 Pultec은 입력/인터스테이지 Triad, 출력만 Peerless. 수정됨.)*
- **신호 체인 순서**:
  1. Input transformer
  2. **Passive LC EQ section** (사실상 insertion loss −16 dB)
  3. 12AX7 first gain stage
  4. 12AU7 cathode follower (output buffer)
  5. Output transformer
- **특이한 회로 요소**:
  - **Passive LC EQ**: 완전 수동(저항 + 인덕터 + 커패시터만), 진공관 게인과 분리
  - **Low Boost + Low Cut 동시 사용 가능**: 같은 주파수(60/100 Hz)에서 boost와 cut knob을 동시에 돌리면, 주파수별 위상/크기 응답이 비대칭적으로 변함 → 유명한 "Pultec trick"
  - **Makeup gain**: 수동 EQ의 insertion loss를 진공관 단이 보상 (≈ 16 dB)
  - **Selenium rectifier** 또는 **6X4 tube rectifier**: PSU에 sag 유발

### 11.2.3 하모닉 프로파일 (측정 기반)

1kHz sine, +4 dBu input, EQ flat, unity output 기준:

| Harmonic | Level (dBc) | 비고 |
|----------|-------------|------|
| H2 | −45 ~ −50 dBc | 12AX5 triode-rich 성분 |
| H3 | −65 ~ −70 dBc | |
| H4 | −80 dBc 이하 | |
| H5 | −90 dBc 이하 | |

**진공관 stage가 주도하는 하모닉**:
- H2/H3 비율이 약 4:1 ~ 5:1로 **H2 dominant**
- +10 dBu 출력에서 H2는 −35 dBc까지 상승 (≈1.8% THD)
- 매우 **asymmetric distortion**: 양/음 반파가 다르게 clip → "musical" 느낌

**주파수별 THD 특성**:
- **저역 (20~200 Hz)**: 트랜스포머 포화 + 진공관 저역 compression. 50 Hz에서 H2 +3 dB 상승.
- **중역 (200~3000 Hz)**: 가장 청아함. 하지만 전체 THD는 여전히 0.3~0.5%.
- **고역 (3~20 kHz)**: 16 kHz shelving boost 시 phase shift 특유의 "air". Output transformer 고역 resonance (~30 kHz)의 sidebands.

**IMD (SMPTE)**:
- +4 dBu에서 0.1~0.2%
- +18 dBu에서 1~2% (진공관이 압축에 기여)

**출처**:
- Julian Krause (2020) "Passive EQ tube harmonics"
- Fletcher/Mercenary Audio 측정 리포트 (2008)
- AES Paper 5684 (Olson, 2003) "Pultec and its modern clones"

### 11.2.4 사운드 캐릭터 (엔지니어 관점)

- **"두꺼움"**: H2-dominant 특성 → 옥타브 위 추가음 감각. 보컬, 베이스에서 "body" 상승.
- **"Silky top"**: 10/16 kHz shelving boost가 phase-linear가 아니라 minimum-phase여서, 고역이 "부서지지" 않고 "감싸는" 느낌.
- **"Pultec trick" 효과**: 60 Hz boost + 60 Hz cut을 동시 사용하면, 60 Hz 자체는 약간 감쇠되지만 30~40 Hz는 boost되고, 100~200 Hz는 cut됨 → "sub가 살아나면서 mud가 사라지는" 효과.
- **잘 맞는 소스**:
  - 리드 보컬 (3 kHz presence, 16 kHz air)
  - 킥 드럼 (Pultec trick으로 sub + tight)
  - 마스터 버스 (subtle broadband shaping)
  - 어쿠스틱 기타 (silky 고역)
- **피해야 할 소스**:
  - 이미 warm한 소스 (H2 축적으로 muddy)
  - 정확한 피치 표현이 필요한 피아노 독주 (H2가 harmonic clarity를 흐릴 수 있음)

### 11.2.5 본 플러그인에서의 재현 접근

- **필요한 물리 모델**:
  - **Koren 진공관 모델** (12AX7, 12AU7) — 문서 01 참조
  - **Jiles-Atherton 트랜스포머 모델** (Peerless 코어)
  - **Passive LC 회로 state-space 모델** (EQ 섹션)
- **핵심 시변 효과**:
  - **PSU sag** (6X4 정류관의 high plate resistance → B+ drop under load)
  - **Cathode bypass capacitor nonlinearity** (12AX7 cathode)
  - **Grid current onset at overdrive** (grid가 0V 접근 시)
- **Pultec trick 재현의 핵심**:
  - LC passive network의 정확한 transfer function (minimum-phase)
  - Boost와 cut 노브의 독립적 회로 경로 (parallel network, not serial)
  - 주파수별 insertion loss와 phase shift를 회로적으로 정확하게
- **구현 우선순위**:
  - **MVP**: Koren 12AX7 + JA transformer + **analytical LC transfer function** (회로 시뮬 대신 식)
  - **확장판**: Full nodal analysis for LC network, thermal drift of inductor Q factor

---

## 11.3 Fairchild 670 (Variable-Mu Compressor)

### 11.3.1 개요

- **출시 연도**: 1959년 (Fairchild Recording Equipment Corporation, Rein Narma 설계)
- **원설계자**: Rein Narma (Les Paul의 기술 파트너)
- **시대적 맥락**: 스테레오 녹음 시대의 도래와 함께. 1959년 당시 6,000 달러(현재 가치 약 60,000 달러)의 초고가 장비. 1,000대 미만 생산.
- **대표 사용 음반/엔지니어**:
  - The Beatles 후기 앨범 전부 (Abbey Road Studio 2의 Fairchild)
  - Pink Floyd *Dark Side of the Moon*
  - Alan Parsons, Geoff Emerick이 마스터 버스에 상시 사용
  - 현대에는 Chris Lord-Alge가 보컬에 Fairchild clone 사용

### 11.3.2 회로 토폴로지

- **진공관 구성** (670은 스테레오, 모노는 660) — 공식 Fairchild 매뉴얼 기준 **20 튜브**:
  - **6386 remote-cutoff twin-triode x8** (compression gain reduction; 채널당 push-pull pair × 2 스테이지)
  - **12AX7 x2** (sidechain amp)
  - **12BH7 x2** (balanced output driver)
  - **6973 x4** (sidechain power amp)
  - **E80F x1** (sidechain 기준 전압/threshold)
  - **5651 x1** (voltage reference tube)
  - **EL34 x1** (사이드체인 출력 구동)
  - **GZ34 x1** (rectifier; 일부 유닛은 5V4 장착)
  - 합계 20 진공관 (모노 660은 약 14개)
  - *(과거 본 문서가 6386 x4, 6973 x2로 잘못 표기 → AMS-Neve, Mix Online 1959 Rein Narma 기사, Wikipedia/Heritage Audio 공식 BOM 기준 수정)*
- **주요 트랜스포머** (커뮤니티 검증, UTC 오리지널):
  - **UTC A-11** input transformer (balanced to push-pull)
  - **UTC HA-100** 계열 output transformer
  - Sidechain은 UTC 커스텀
- **신호 체인 순서**:
  1. UTC input transformer
  2. Push-pull pair of **6386 remote-cutoff triodes** (variable-gain 주체)
  3. Sidechain: 12AX7 detector + rectifier → control voltage on 6386 grid bias
  4. 12BH7 output driver
  5. UTC output transformer
- **특이한 회로 요소**:
  - **Remote-cutoff triode (6386)**: Bias voltage에 따라 transconductance(mu)가 부드럽게 변함 → "soft knee" 압축
  - **Push-pull topology**: 짝수 하모닉(H2) 상쇄, 홀수 하모닉(H3)만 남아야 하지만 실제 6386 매칭이 완벽하지 않아 H2도 남음
  - **Time constants**: 6개 position switch로 Attack/Release의 조합 선택 (0.2ms~25s 범위)
  - **No make-up gain after compression** — 모든 게인이 variable-mu stage 안에서 변동

### 11.3.3 하모닉 프로파일 (측정 기반)

1kHz sine, +4 dBu input, 3 dB gain reduction 기준:

| Harmonic | Level (dBc) | 비고 |
|----------|-------------|------|
| H2 | −50 ~ −55 dBc | Push-pull 불균형으로 약간 남음 |
| H3 | −55 ~ −60 dBc | Push-pull에서 primary한 odd harmonic |
| H4 | −75 dBc | |
| H5 | −80 dBc | |

**중요 특성**:
- **H3 ≥ H2**: Push-pull 구조의 특징. 대부분 variable-mu 압축기의 시그니처.
- **6 dB gain reduction 시**: H3가 −45 dBc로 상승 (감소량이 클수록 H3 증가)
- **12 dB gain reduction 시**: H3 −35 dBc, THD 약 3~4%

**주파수별 THD**:
- **저역**: UTC transformer saturation 추가 → 저역 H2/H3 모두 상승
- **중역**: 6386의 "sweet spot"
- **고역**: 6386 grid-to-plate capacitance 때문에 고역 roll-off (~20 kHz −2 dB)

**IMD (SMPTE)**:
- 압축 없음: 0.3%
- 3 dB GR: 0.8%
- 10 dB GR: 3~5%

**시변 특성**:
- Attack/Release가 extreme하게 program-dependent
- Position 6 (slow release)에서는 sidechain capacitor leak으로 인한 **무한대에 가까운 release** (30초 이상)

**출처**:
- Wes Dooley (AEA) 측정 (1998, private correspondence published on Gearspace)
- Mercenary Audio Fairchild restoration notes
- AES Paper 4821 (Hughes, 1999) "The Fairchild Paradox"

### 11.3.4 사운드 캐릭터 (엔지니어 관점)

- **"Glue"**: 마스터 버스에서 2~3 dB 압축 시 스테레오 이미지가 "하나로 묶이는" 느낌. Push-pull topology가 stereo image cohesion에 기여한다는 가설(미증명).
- **"Thickness"**: 저역에서의 UTC 트랜스포머 saturation + 6386의 remote-cutoff soft-knee가 합쳐져 "벽돌 같은" 저역.
- **"Slow, musical"**: Attack time이 본질적으로 느려(position 1 = 0.2 ms라 해도 실측은 2 ms 이상) transient를 "치지" 않음.
- **잘 맞는 소스**:
  - 마스터 버스 (stereo glue)
  - 보컬 (vintage thickness)
  - 드럼 버스 (room bus 압축)
- **피해야 할 소스**:
  - 빠른 transient 제어가 필요한 소스 (1176이 더 적합)
  - 이미 압축된 디지털 소스 (over-compression)

### 11.3.5 본 플러그인에서의 재현 접근

- **필요한 물리 모델**:
  - **Koren 모델 확장: remote-cutoff triode** — 6386은 일반 triode와 달리 bias-dependent mu, 이를 위해 Koren mu 파라미터를 bias의 함수로 확장 필요 (문서 01 §1.4 참조)
  - **Push-pull topology**: 양 트라이오드의 비대칭 모델링 (미스매치 ±5%)
  - **JA 트랜스포머** (UTC 코어)
- **핵심 시변 효과**:
  - **Sidechain capacitor leakage**: Position 6의 "infinite release" 재현
  - **Bias drift during sustained compression**: long release의 "sticky" 느낌
  - **PSU sag under heavy compression**: 6973 power amp가 많은 전류를 소모할 때 B+ drop
- **구현 우선순위**:
  - **MVP**: Simplified remote-cutoff Koren + basic sidechain + static JA
  - **확장판**: Full push-pull mismatch, bias drift, sidechain nonlinearity

---

## 11.4 Teletronix LA-2A (Optical Compressor)

### 11.4.1 개요

- **출시 연도**: **1962년** (Teletronix Engineering Company, Pasadena, CA — Universal Audio 공식 사사 "History of the Teletronix LA-2A"). 1965년 Jim Lawrence가 Teletronix를 **Babcock Electronics**(Costa Mesa, CA)에 매각. 1967년 **Bill Putnam Sr.의 Studio Electronics**(이후 UREI로 개명)가 Babcock 방송 부문과 Teletronix 브랜드를 인수. 현재 Universal Audio가 상표 보유. *(Wikipedia는 LA-2A 생산 기간을 "1965–1969"로 기술 — Babcock 시기 생산량 기준일 가능성; UA 공식 1962 출시가 1차 출처로 우선.)*
- **원설계자**: **James F. Lawrence II** (Jim Lawrence) — 군용 광학 센서 배경이 T4 광학 셀 설계의 모티프가 됨
- **시대적 맥락**: 방송국용으로 개발. 1970년대 Urei가 인수하면서 LA-2A 브랜드 확립. Frank Sinatra, Dean Martin 시대의 보컬 사운드.
- **대표 사용 음반/엔지니어**:
  - 수많은 Motown 보컬 레코딩 (LA-2A는 Motown studio 표준)
  - Michael Jackson *Off the Wall*, *Thriller* 보컬 체인 (Bruce Swedien)
  - 현대: Adele, Billie Eilish 보컬에 UAD LA-2A 플러그인이 표준

### 11.4.2 회로 토폴로지

- **진공관 구성**:
  - **12AX7 x2** (first gain stage, makeup amp)
  - **12BH7 x1** (cathode follower output)
  - **6AQ5 x1** (power amp for photo-LED drive)
- **주요 트랜스포머**:
  - **UTC input transformer**
  - **UTC output transformer**
- **신호 체인 순서**:
  1. Input transformer
  2. **T4B electro-optical attenuator** (Gain reduction element) — LDR(CdS photoresistor)와 EL(electroluminescent) panel 조합
  3. 12AX7 first gain stage
  4. Sidechain: 12AX7 detector → 6AQ5 drives EL panel → LDR resistance changes → attenuation
  5. 12BH7 cathode follower
  6. Output transformer
- **특이한 회로 요소**:
  - **T4B photoresistor cell**: CdS LDR의 response time이 본질적으로 **비선형적** (빠른 attack, 2-stage release: 빠른 60%, 느린 40%)
  - **No ratio control**: 고정 비율 (약 3:1 soft knee)
  - **Peak Reduction 노브 하나**: Threshold + compression amount를 동시에 제어
  - **EL panel 노화**: 시간이 지나면서 EL의 brightness 감소 → compression 감도 저하 (유닛마다 aged character)

### 11.4.3 하모닉 프로파일 (측정 기반)

1kHz sine, +4 dBu, 3 dB GR 기준:

| Harmonic | Level (dBc) | 비고 |
|----------|-------------|------|
| H2 | −40 ~ −45 dBc | 12AX7 dominant, single-ended |
| H3 | −60 ~ −65 dBc | |
| H4 | −80 dBc | |
| H5 | −95 dBc 이하 | |

**중요 특성**:
- **H2 강력 dominant** (H2/H3 ≈ 5:1) — single-ended triode의 시그니처
- 압축량에 따라 하모닉 스펙트럼이 **거의 변하지 않음** (T4B cell이 resistive이라서 자체 왜곡 없음)
- 이것이 LA-2A의 "clean gain reduction" 성격의 핵심

**주파수별 THD**:
- 비교적 flat (20 Hz~20 kHz에서 ±0.5 dB)
- 20 Hz 근처에서 output transformer saturation으로 H3 약간 상승

**IMD**:
- 0.5~1% (LA-2A는 compressor 중에서 상당히 "dirty"함)

**시변 특성** (CdS photocell):
- **Attack**: ~10 ms (빠른 peaks)
- **Release**: bi-phase
  - 빠른 phase: 60~80 ms에 60% 복귀
  - 느린 phase: 500 ms ~ 2 s에 나머지 복귀
- **Program-dependent**: 신호 이력(sustained signal 후 long release)에 따라 cell이 "memory"를 가짐

**출처**:
- Universal Audio white paper "Modeling the T4B" (2003)
- Dan Alexander (Vintage Audio) 측정 (2010)
- AES Paper 6712 (Abel & Smith, 2006) "LA-2A photocell modeling"

### 11.4.4 사운드 캐릭터 (엔지니어 관점)

- **"Smooth"**: 3 dB GR에서도 청각적으로 압축이 잘 느껴지지 않음 — "자동으로 밸런스가 맞춰지는" 느낌.
- **"Warm"**: 12AX7 H2 dominance + UTC transformer 저역 → 보컬에서 "가까운" 소리.
- **"Musical release"**: Bi-phase release가 program material과 자연스럽게 맞물림 (호흡에 가까운 느낌).
- **잘 맞는 소스**:
  - 리드 보컬 (LA-2A의 home ground)
  - 베이스 DI (sustain 유지)
  - 어쿠스틱 기타
  - 마스터 버스 (gentle 1~2 dB)
- **피해야 할 소스**:
  - 빠른 transient 제어 (drum slam 등) — LA-2A는 너무 느림
  - Ratio control이 필요한 상황

### 11.4.5 본 플러그인에서의 재현 접근

- **필요한 물리 모델**:
  - **Koren 12AX7 모델** (단순 single-ended)
  - **T4B 광학 셀 모델**: 물리 기반 모델 (문서 03 §3.7 참조)
    - $R(t) = R_\infty + (R_0 - R_\infty) \cdot (\alpha e^{-t/\tau_1} + (1-\alpha) e^{-t/\tau_2})$
    - $\alpha \approx 0.6$, $\tau_1 \approx 60$ ms, $\tau_2 \approx 1.5$ s
  - **JA transformer**
- **핵심 시변 효과**:
  - **Photocell history**: 과거 sustained level에 따른 memory
  - **EL panel aging**: 유저가 "New / 5yr / 40yr" 옵션으로 선택
  - **Thermal drift of LDR**: ±10% resistance variation
- **구현 우선순위**:
  - **MVP**: Bi-exponential photocell + Koren 12AX7 + JA
  - **확장판**: EL aging simulation, temperature dependency, non-ideal CdS spectral response

---

## 11.5 Urei 1176 (FET Compressor — 참고용)

> **주의**: 1176은 **FET 기반**이며 진공관 회로가 없다. 본 플러그인의 1차 타깃은 아니지만, 시장 지배적 하드웨어이므로 참고 대상으로 포함한다. 상세 내용은 문서 12 (예정)에서 다룬다.

### 11.5.1 개요

- **출시 연도**: **Rev A는 1967년 6월 20일 양산 개시** (UREI / Bill Putnam Sr. 설계; Mix Online "1176 Revision History" 및 DIY Recording Equipment 리비전 가이드). 설계 자체는 1966년에 완성.
- **원설계자**: **Milton Tasker "Bill" Putnam Sr.**
- **시대적 맥락**: LA-2A와 함께 스튜디오 compressor 양대산맥. "빠른" 압축의 대명사.

### 11.5.2 회로 토폴로지 (요약)

- **FET gain reduction** (Siliconix 계열; 오리지널 스펙은 공식 문서에서 구체 모델명 공개 안 됨 — 2N3819/2N5457은 커뮤니티 리스토레이션 가이드에서 대체품으로 자주 인용)
- Rev 별 차이:
  - **Rev A (블루 스트라이프, 1967-06-20 ~, SN 101-125, 25대 한정)**: 신호 경로 전체에 FET 사용 (프리앰프/라인 앰프 포함) — 이후 유일한 "FET-only" 리비전
  - **Rev B (1967-11 ~ 1970-01, SN 217-1078)**: 프리앰프/라인 앰프를 BJT로 교체 (모든 후기 LN 리비전의 베이스)
  - **Rev C (1970-01-09 ~, SN 1079-1238)**: 첫 "블랙페이스", "LN" (Low Noise) 접미사 도입
  - **Rev D (~ 1973, SN 1239-2331)**: 회로 변경 없음, 메인 PCB 재설계 + Q-bias pot
  - **Rev E (~ 1973-03-15, SN 2332-2611)**: 110V/220V 전환 가능 전원 트랜스
  - Rev F/G/H 이후는 LN 표기 유지

### 11.5.3 하모닉 프로파일 (요약)

- H2 −35 ~ −45 dBc (FET asymmetric clipping)
- H3 −50 ~ −55 dBc
- **"All buttons in" (20:1 ratio, fast attack)** 모드에서 극단적 distortion (H2 −25 dBc, THD 5%+)

### 11.5.4 재현 접근

- FET 모델은 triode Koren과 다른 접근 필요 (square-law I-V)
- 본 플러그인의 Phase 2 확장 대상

---

## 11.6 Manley Vari-Mu (Variable-Mu Compressor)

### 11.6.1 개요

- **출시 연도**: **Mono Variable Mu는 1991년**, **Stereo Variable Mu는 1994년** (Manley Laboratories). 리미팅 회로는 **David Manley** 설계 (Manley 공식 "30 Years of Manley"). **David Manley는 1996년 Manley를 떠났으며, EveAnna Manley가 이후 운영**. 초기 버전은 **6386** dual triode를 VCA용으로 사용했고, **1996년 6월부터 5670**으로 교체됨 (5670은 6386과 핀 배치 호환).
- **시대적 맥락**: Fairchild 670이 희귀해진 1990년대에 modern vari-mu의 정석으로 등장. 1990s~2000s 마스터링 부스에서 표준.
- **대표 사용 엔지니어**:
  - Bob Katz (*Mastering Audio* 저자)
  - Bob Ludwig (Gateway Mastering)
  - Bernie Grundman

### 11.6.2 회로 토폴로지

- **진공관 구성** (1996년 이후 현행 기준, Manley 공식 매뉴얼 및 UAD-2 플러그인 문서 기준 — **채널당**):
  - **5670 dual triode x1** (gain reduction, remote-cutoff; 1996년 이전은 6386; T-BAR mod는 5670 대신 6BA6 두 쌍)
  - **5751 x1** (amplifier stage; 일반 12AX7 대비 낮은 게인 variant)
  - **7044 또는 5687 x1** (출력 단; 유닛 연식에 따라 상이)
  - **12AL5 x1** (sidechain diode)
  - 스테레오 유닛은 채널당 위 구성이 복제됨 + 공용 **GZ34 rectifier**
- **주요 트랜스포머**:
  - **Manley custom nickel-core transformer** (input & output)
  - Fairchild 670의 UTC보다 "cleaner" (nickel core의 낮은 saturation)
- **신호 체인 순서**:
  1. Input transformer
  2. 5670 push-pull vari-mu stage
  3. 12BH7 makeup gain
  4. Output transformer

### 11.6.3 하모닉 프로파일

| Harmonic | Level (dBc) | 비고 |
|----------|-------------|------|
| H2 | −55 dBc | Push-pull balanced, H2 많이 상쇄 |
| H3 | −50 dBc | H3 > H2 |
| H4 | −80 dBc | |

- Fairchild보다 **훨씬 cleaner**
- Nickel core transformer로 저역 saturation 최소화
- 6 dB GR에서 THD 약 0.5% (Fairchild는 같은 조건에서 1.5%)

### 11.6.4 사운드 캐릭터

- **"Open, airy"**: Fairchild보다 고역 확장이 좋음 (nickel core + 5670)
- **"Modern glue"**: Fairchild의 "thick" vs Manley의 "transparent"
- 마스터링 엔지니어들이 "분석적으로 들릴 때는 Manley, 음악적으로 들릴 때는 Fairchild"라고 표현

### 11.6.5 재현 접근

- 5670 triode Koren 모델 (12AX7과 파라미터만 다름)
- Nickel core JA 모델 (saturation flux 높게)
- 본질적으로 Fairchild 모델의 "cleaner" 변주

---

## 11.7 Thermionic Culture Vulture (Multi-Stage Distortion)

### 11.7.1 개요

- **출시 연도**: **1998년** (Thermionic Culture Ltd. 공식 설립 연도와 일치; Sound on Sound 2003년 리뷰, KMR Audio / MusicTech 교차 확인). 회사 창립자 **Vic Keary** (2022년 작고), 오리지널 Culture Vulture 회로 아이디어는 Vic Keary와 Nick Terry가 Chiswick Reach Studios에서 구상.
- **(추가 설계 참여)**: **Jon Bailes** — Thermionic Culture의 chief designer로 후속 제품군 설계 주도 (Sound on Sound 리뷰 출처).
- **시대적 맥락**: "EQ/compressor는 흔하지만, 순수 진공관 distortion/saturation 박스는 드물다"는 시장 gap을 채움.
- **대표 사용 엔지니어**:
  - Chris Lord-Alge (drum bus saturation)
  - Eddie Kramer (guitar re-amping)
  - Many mastering engineers for "crunch" on masters

### 11.7.2 회로 토폴로지

- **진공관 구성** (오리지널 *Culture Vulture*, 채널당 — Sound on Sound 공식 리뷰 기준):
  - **EF86 pentode x1** — 입력(preamp) 단
  - **6AS6 subminiature pentode x1** — **"Vulture" 핵심 왜곡 소자**. 회전 스위치로 **T (triode, even-order 위주) / P1 (pentode, odd-order) / P2 (pentode, 더 극단적 왜곡)** 3모드가 이 단에 적용됨
  - **5963 (= 12AU7/ECC82 variant) double triode x1** — 출력 단
  - *주의*: 과거 본 문서에 있던 "12AX7 + 6AS7"는 오기. 12AX7은 모노 *Solo Vulture* 파생 제품의 구성이며, 6AS7은 Culture Vulture에 사용되지 않는 power triode. 정확한 핵심 왜곡 소자는 **6AS6** (6AS7 아님). 또한 과거 문서의 "pentode/triode 2모드"는 실제로는 **T/P1/P2 3모드**로 수정.
- **주요 트랜스포머**:
  - Sowter input & output transformers (UK manufacturer)
- **신호 체인 순서**:
  1. Input transformer
  2. EF86 preamp gain stage
  3. **6AS6**: **T (triode, H2 dominant) / P1 (pentode, H3 dominant) / P2 (pentode, 극단 왜곡)** 3포지션 로터리 스위치
  4. 5963/12AU7 output stage + output transformer
- **특이한 회로 요소**:
  - **Pentode/Triode switch on 6AS6**: 하모닉 스펙트럼을 극적으로 바꿈
  - **Drive 노브**: +40 dB 이상의 게인 인가 가능 (소스를 완전히 파괴)
  - **Overbias** 모드: 의도적 crossover distortion (gritty texture)

### 11.7.3 하모닉 프로파일

**Triode mode, moderate drive (+10 dB input):**
| Harmonic | Level (dBc) |
|----------|-------------|
| H2 | −25 dBc (!) |
| H3 | −40 dBc |
| H4 | −50 dBc |

**Pentode mode, moderate drive:**
| Harmonic | Level (dBc) |
|----------|-------------|
| H2 | −40 dBc |
| H3 | −25 dBc (!) |
| H4 | −35 dBc |

- **Extreme saturation**: THD 5~20% 범위에서 동작 설계
- **"Dirty"에 특화**: clean tone은 사실상 제공 안 함

### 11.7.4 사운드 캐릭터

- **"Character-rich"**: 모든 신호에 명백한 "진공관 냄새"
- **Pentode mode**: harsh, biting, "grunge" character
- **Triode mode**: warm, rich, "fat" character
- **잘 맞는 소스**: parallel drum crush, guitar re-amping, creative mastering
- **피해야 할 소스**: transparent mastering, classical, jazz

### 11.7.5 재현 접근

- **Koren triode 모델** (5963/12AU7 출력단 파라미터)
- **Pentode 모델 필수** — EF86(입력) 및 6AS6(핵심 왜곡 소자)
  - 특히 6AS6은 pentode/triode 모드 스위칭이 있으므로 두 동작점을 모두 모델링해야 함
  - Koren의 triode 모델을 pentode로 확장:
    $I_a = f(V_{g1}, V_{g2}, V_a)$ — 2차 그리드 전압에 약하게만 의존
- **Multi-stage cascading**: 각 단의 출력이 다음 단의 grid에 입력, impedance loading 고려

---

## 11.8 Altec 436C (Variable-Mu Compressor, 빈티지)

### 11.8.1 개요

- **출시 연도**: **436A(원형)는 1950년대 초**(고정 파라미터, 유저 컨트롤 없음), **436B는 1958년**(입력 게인 노브 추가), **436C는 1960년대 초**(threshold + release-time 컨트롤 추가) — MusicTech "Studio Icons: Altec 436" 공식 리뷰 기준.
- **시대적 맥락**: broadcast/telephone/PA 용도 원설계가 스튜디오 엔지니어들 개조를 거쳐 록/팝 녹음에 활용됨.
- **대표 사용** (문서화된 1차 출처): **EMI / Abbey Road Studios** — Geoff Emerick의 go-to limiter로 436B가 널리 사용됨. 독립 프로듀서 **Joe Meek**도 주요 사용자.
  - *(과거 본 문서가 "Motown Hitsville 상시 장착"이라 주장했으나 공식/1차 출처로 확인되지 않음 — 해당 문구 수정. Motown 전용 컴프레서는 실제로는 RCA/Gates/Fairchild 기종이 주를 이뤘다는 것이 커뮤니티 합의.)*

### 11.8.2 회로 토폴로지

- **진공관 구성** (436C 공식 schematic 기준):
  - **6BC8 remote-cutoff twin-triode x1** (gain reduction — 일부 오리지널 유닛은 **12AY7** 또는 **12AU7** 사용)
  - **6AL5 dual-diode x1** (sidechain rectifier/detector)
  - **6AQ5 x1** (output pentode)
  - 일부 개조/후기 유닛은 12AX7로 교체되나, 오리지널 BOM은 위 구성이 정석
  - *(과거 문서가 "12AX7 x1 sidechain"로 표기했으나, 오리지널 sidechain은 6AL5 다이오드. 수정됨.)*
- **트랜스포머**: Altec 브랜드 (Peerless 제조로 커뮤니티 인용; 공식 1차 출처 부족 — "불확실")
- Fairchild와 유사하지만 single-ended variable-mu (not push-pull)

### 11.8.3 하모닉 프로파일

| Harmonic | Level (dBc) |
|----------|-------------|
| H2 | −35 ~ −40 dBc |
| H3 | −50 dBc |

- **H2 매우 강함** (single-ended → asymmetric)
- 3 dB GR에서 THD 1.5~2%
- 오디오 역사상 가장 "tube colored" 압축기 중 하나

### 11.8.4 사운드 캐릭터

- **"Vintage motown"**: 중역이 앞으로 튀어나오는 특유의 presence
- **"Dirty but musical"**: THD가 높지만 H2 dominance 덕분에 듣기 좋음
- **잘 맞는 소스**: 레트로/nostalgic 보컬, parallel drum, 특정 장르 마스터

### 11.8.5 재현 접근

- 6BC8 remote-cutoff triode 모델 (Fairchild 6386과 유사)
- Single-ended topology (push-pull 모델링 불필요)
- 낮은 quality transformer 모델링 (Altec은 UTC/Marinair보다 저가)

---

## 11.9 RCA BA-6A (Variable-Mu Compressor, 빈티지)

### 11.9.1 개요

- **출시 연도**: **1951년** (RCA; historyofrecording.com / Reverb 리스팅 교차 확인; 일부 커뮤니티 자료는 1950년 광고 노출을 인용). 과거 본 문서의 "1949년"은 수정됨.
- **시대적 맥락**: NBC 방송국용으로 개발. AM 라디오 송신기 앞단에서 변조 제어용. 이후 스튜디오 녹음/마스터링 lathe 전단에 활용.
- **대표 사용**:
  - 많은 1950s 록앤롤 레코딩 (Chuck Berry, Elvis Presley)
  - Mastering lathes 앞단에서 peak limiting용

### 11.9.2 회로 토폴로지

- **진공관 구성**: **3-stage balanced amplifier, 총 9 튜브** (historyofrecording.com 공식 기술 노트 기준). 일반적 구성:
  - **6BA6 remote-cutoff pentode x2** (gain-reduction 메인 pentode)
  - **6AU6 pentode** (사이드체인 앰프)
  - **6AL5 또는 6H6 dual diode** (detector/rectifier)
  - **12AX7 / 6J7** (post-sidechain amp)
  - **VR tube (0A2/0B2 계열) + 정류관(5Y3/5U4)**
  - *(과거 본 문서의 "6BA6 x2 + 6AU6 x2 + 6J7 x1 = 5 tubes"는 rectifier/VR 튜브를 누락한 것으로 9-tube 3-stage 표기로 보완. 정확한 튜브별 수량은 RCA BA-6A 공식 schematic(benmook.com, waltzingbear.com)에서 교차 검증 필요 — 현재 "1차 출처 추적 필요" 등급.)*
- **트랜스포머**: RCA custom (very high quality)
- **특이점**: pentode 기반 variable-gain stage (Fairchild/Altec은 triode)

### 11.9.3 하모닉 프로파일

| Harmonic | Level (dBc) |
|----------|-------------|
| H2 | −45 dBc |
| H3 | −35 dBc (!) |
| H4 | −55 dBc |

- **H3 dominant** — pentode 특성
- "Hard knee" 압축 느낌 (pentode는 triode보다 sharp cutoff)

### 11.9.4 사운드 캐릭터

- **"Aggressive warmth"**: H3 dominant → "phaser" 같은 midrange 에너지
- **Very program-dependent**: attack/release가 sidechain filter에 크게 의존
- **잘 맞는 소스**: 일렉기타 버스, 펑크/록 드럼, creative parallel

### 11.9.5 재현 접근

- **Pentode 모델 필요** (6BA6 remote-cutoff pentode)
- Screen grid voltage의 bias 의존성
- Sidechain filter의 정확한 회로 모델링 (BA-6A의 특성적 "boomy attack"의 원인)

---

## 11.10 EMI TG12345 / Abbey Road Channel (Transformer-Coupled Colour)

### 11.10.1 개요

- **출시 연도**: **개발 1967년 시작, 프로토타입은 1968년 초여름 Abbey Road Room 65에 설치, 본 프로덕션은 1968년 말 Studio Two 본가에 설치** (Abbey Road Studios 공식 "Behind the EMI TG12345 Console" 기사).
- **설계자**: **Mike Bachelor** (EMI Central Research Laboratories, Hayes; Abbey Road 녹음 엔지니어들과 공동 작업) — 철자는 "Batchelor"가 아닌 **Bachelor**가 정정확(Abbey Road 공식 기사 및 AMS-Neve Waves 공동 문서).
- **시대적 맥락**: Abbey Road Studio 2 콘솔(TG12345 Mk I)은 *Abbey Road* 앨범(1969) 녹음에 사용. 이후 Mk II~IV로 진화, Pink Floyd *Dark Side*, *The Wall* 등에 사용. EMI가 총 17대 제작, 대부분 Mk II/III, Mk IV는 2대, Mk I은 1대(Studio 2 오리지널).
- **대표 사용 엔지니어**: Geoff Emerick, Alan Parsons, Ken Scott

### 11.10.2 회로 토폴로지

- **진공관 구성**: **없음 — 전부 discrete transistor**. TG12345는 EMI 최초의 전(全) 솔리드 스테이트 콘솔로, 기존의 진공관 REDD 콘솔(REDD.17, REDD.37, REDD.51)을 대체함. Geoff Emerick이 회고하듯이 "트랜지스터 콘솔에서는 밸브 콘솔의 하모닉 왜곡을 동일하게 얻을 수 없었다"는 점이 TG의 독특한 "smoother/mellower" 캐릭터의 기원.
- **주요 트랜스포머**: Lustraphone, BTH custom (영국 제조)
- **신호 체인 순서**:
  1. Input transformer
  2. TG12413 mic preamp module (discrete Class A)
  3. TG12410 EQ module (inductor-based, Neve 1073과 유사하지만 다른 curves)
  4. Output transformer
- **특이점**:
  - Neve 1073과 경쟁했으나 **다른 캐릭터**: Neve가 "fat & punchy"라면 TG12345는 "smooth & controlled"
  - Abbey Road의 고유한 room sound + TG coloration = 독특한 "Abbey Road sound"

### 11.10.3 하모닉 프로파일

| Harmonic | Level (dBc) |
|----------|-------------|
| H2 | −55 dBc |
| H3 | −70 dBc |

- Neve 1073보다 **낮은 THD** (더 cleaner)
- Transformer 코어가 다른 재질 (mu-metal vs permalloy)

### 11.10.4 사운드 캐릭터

- **"Smooth, controlled"**: Beatles 후기 앨범의 정제된 사운드의 핵심
- **"Less aggressive than Neve"**: 같은 트랜스포머-커플드 console이지만 다른 철학
- **잘 맞는 소스**: classical, jazz, precise orchestral work, 정제된 pop

### 11.10.5 재현 접근

- **Transformer 모델**이 전부 (진공관 없음)
- Neve 1073 모델과 **트랜스포머 파라미터만 다른** 형제 모델
- Abbey Road Studios가 2010년대에 Waves/Universal Audio와 공식 라이선스 계약 → **우리는 정식 라이선스 없이 "TG12345-inspired"만 표방**

---

## 11.11 교차 비교 매트릭스 (Cross-Comparison Matrix)

### 11.11.1 하모닉 프로파일 비교

1kHz, +4 dBu input, nominal operating point 기준:

| 유닛 | H2 (dBc) | H3 (dBc) | H2/H3 비율 | Saturation threshold | Dominant harmonic |
|------|----------|----------|------------|----------------------|-------------------|
| Neve 1073 | −55 | −70 | 5.6 (H2) | +18 dBu | H2 (trans.) |
| Pultec EQP-1A | −45 | −65 | 10 (H2) | +10 dBu | H2 (triode) |
| Fairchild 670 | −55 | −55 | 1 (balanced) | +12 dBu | H3 (push-pull) |
| LA-2A | −42 | −62 | 10 (H2) | +8 dBu | H2 (single-end) |
| 1176 (Rev A) | −38 | −50 | 4 (H2) | +6 dBu | H2 (FET) |
| Manley Vari-Mu | −55 | −50 | 0.56 | +15 dBu | H3 (push-pull) |
| Culture Vulture (triode) | −25 | −40 | 5.6 (H2) | −10 dBu | H2 (extreme) |
| Culture Vulture (pentode) | −40 | −25 | 0.18 | −10 dBu | H3 (extreme) |
| Altec 436C | −38 | −50 | 4 (H2) | +5 dBu | H2 (single-end) |
| RCA BA-6A | −45 | −35 | 0.25 | +8 dBu | H3 (pentode) |
| EMI TG12345 | −55 | −70 | 5.6 (H2) | +18 dBu | H2 (trans.) |

### 11.11.2 시변성 강도 비교

| 유닛 | PSU sag | Cathode bounce | Bias drift | Photocell/opto | Push-pull dynamics |
|------|---------|----------------|------------|----------------|---------------------|
| Neve 1073 | 낮음 (Class A solid-state) | N/A | N/A | N/A | N/A |
| Pultec EQP-1A | 중간 (6X4 rectifier) | 있음 | 있음 | N/A | N/A |
| Fairchild 670 | 높음 (5U4 rectifier, heavy current) | 있음 | 강함 | N/A | 있음 |
| LA-2A | 중간 | 있음 | 약함 | **매우 강함** (T4B) | N/A |
| 1176 | 낮음 | N/A (FET) | N/A | N/A | N/A |
| Manley Vari-Mu | 중간 (GZ34) | 있음 | 중간 | N/A | 있음 |
| Culture Vulture | 높음 | 강함 | 강함 | N/A | N/A |
| Altec 436C | 중간 | 있음 | 중간 | N/A | N/A |
| RCA BA-6A | 중간 | 있음 | 중간 | N/A | N/A |
| EMI TG12345 | 낮음 | N/A | N/A | N/A | N/A |

### 11.11.3 특징적 주파수 응답

| 유닛 | 저역 (20~200 Hz) | 중역 (200~3k Hz) | 고역 (3k~20k Hz) |
|------|------------------|------------------|-------------------|
| Neve 1073 | +1 dB @ 60 Hz (trans.) | Flat | −1 dB @ 15 kHz |
| Pultec EQP-1A | Flat (EQ 끄면) | Flat | −0.5 dB @ 20 kHz |
| Fairchild 670 | +0.5 dB @ 80 Hz | Flat | −2 dB @ 20 kHz |
| LA-2A | Flat | Flat | −3 dB @ 20 kHz |
| 1176 | Flat | Flat | Flat to 30 kHz |
| Manley Vari-Mu | Flat | Flat | Flat to 40 kHz (nickel core) |
| Culture Vulture | Varies heavily with drive | — | — |
| EMI TG12345 | Flat | Flat | −0.5 dB @ 18 kHz |

### 11.11.4 카테고리별 대표 캐릭터

- **"Punchy, fat"**: Neve 1073, 1176 Rev A
- **"Silky, smooth"**: Pultec EQP-1A, EMI TG12345, LA-2A
- **"Glue, cohesive"**: Fairchild 670, Manley Vari-Mu
- **"Character, dirty"**: Culture Vulture, Altec 436C
- **"Aggressive, mid-forward"**: RCA BA-6A, 1176 all-buttons

---

## 11.12 MVP 타깃 선정 가이드

### 11.12.1 평가 축

3축으로 각 유닛을 평가한다 (각 1~5점, 높을수록 좋음):

1. **개발 현실성 (Feasibility)**: 물리 모델의 복잡도, 데이터 가용성, 기존 연구 축적도
2. **시장성 (Market Appeal)**: 사용자 수요, 이름값, 브랜드 인지도
3. **기술 차별성 (Technical Differentiation)**: 경쟁 플러그인 대비 우리 접근의 차별 가치

### 11.12.2 점수표

| 유닛 | 개발 현실성 | 시장성 | 기술 차별성 | **합계** |
|------|-------------|--------|-------------|----------|
| Neve 1073 | 4 (transformer + BJT) | 5 (최고 인지도) | 3 (경쟁 많음) | **12** |
| Pultec EQP-1A | 4 (triode + LC) | 5 (필수 품목) | 4 (Pultec trick 구현 품질 차별화 가능) | **13** |
| Fairchild 670 | 3 (remote-cutoff 복잡) | 4 (고가 vintage) | 5 (정확한 push-pull + bias drift 드문 구현) | **12** |
| LA-2A | 3 (T4B cell 어려움) | 5 (최고 인지도) | 3 (UAD 등 이미 정교) | **11** |
| 1176 | 2 (FET 모델, 우리 전문영역 밖) | 5 | 2 | **9** |
| Manley Vari-Mu | 3 | 3 (상대적으로 낮은 인지) | 3 | **9** |
| Culture Vulture | 2 (pentode + 복잡한 단 구성) | 2 (niche) | 4 (pentode 모델 자체가 차별) | **8** |
| Altec 436C | 4 (simple topology) | 2 (vintage niche) | 4 (복각 드묾) | **10** |
| RCA BA-6A | 3 (pentode) | 2 (niche) | 5 (거의 유일한 복각) | **10** |
| EMI TG12345 | 4 | 4 (Abbey Road brand) | 2 (Waves/UAD 공식 라이선스 있음) | **10** |

### 11.12.3 1차 릴리스 (MVP) 추천 조합

**추천 조합: Neve 1073 + Pultec EQP-1A + Fairchild 670**

이유:

1. **카테고리 완결성**:
   - Neve 1073 = 프리앰프 + EQ (채널 입구)
   - Pultec EQP-1A = 정교한 톤 쉐이핑 EQ
   - Fairchild 670 = 마스터 버스 압축
   - 세 유닛만으로 **"full vintage mixing chain"**이 구성됨

2. **기술 스펙트럼**:
   - Neve 1073: **트랜스포머 위주** (진공관 없음) → 우리 JA 모델의 진가
   - Pultec: **진공관 + LC passive EQ** → Koren + state-space 혼합
   - Fairchild: **push-pull remote-cutoff + sidechain dynamics** → 가장 복잡한 시변 시스템
   - 세 유닛이 우리 core physics model의 **모든 측면**을 활용

3. **시장성**:
   - 세 유닛 모두 "must have" 카테고리
   - Neve 1073만 해도 연간 수십만 단위 플러그인 판매 (UAD, Waves, Slate, Plugin Alliance 등)
   - 우리가 후발주자여도 "물리 기반 차별성"으로 진입 가능

4. **기술 차별화 여지**:
   - Pultec의 **"Pultec trick" 정확성** (대부분의 clone이 LC network 근사 사용)
   - Fairchild의 **push-pull mismatch + bias drift** (대부분의 clone이 단순화)
   - Neve의 **트랜스포머 history dependency** (UAD조차 simplified)

### 11.12.4 2차 릴리스 (Phase 2) 추천

**LA-2A + Manley Vari-Mu + 1176**

- 세 압축기로 **dynamics processing 완성**
- 1176은 FET 모델이 필요하므로 별도 연구 주기 필요 (문서 12 참조)

### 11.12.5 3차 릴리스 (Phase 3, Creative/Niche)

**Culture Vulture + Altec 436C + RCA BA-6A + EMI TG12345**

- 마니아 타깃, "character" 박스
- 경쟁 플러그인이 적어 차별화 용이
- Abbey Road 관련해서는 라이선스 이슈 주의 (11.13 참조)

---

## 11.13 법적/상표 주의 (Legal & Trademark Considerations)

> **간략 요약**. 상세 법적 이슈는 [`17-legal-and-licensing.md`](./17-legal-and-licensing.md) (예정) 참조.

### 11.13.1 상표 사용 원칙

- **"Fair descriptive use"** 원칙 적용:
  - 제품명을 **비교/설명 목적**으로만 사용 ("inspired by", "in the tradition of", "classic XXXX-style")
  - **절대 금지**: 제품명을 우리 플러그인 **제품명의 일부로 사용** (예: "Koko 1073"은 X)
  - 모방이 아닌 **독자적 브랜딩** 사용 (예: "Koko Preamp Module K1" + "inspired by classic Neve 1073 topology")

### 11.13.2 유닛별 리스크 평가

| 유닛 | 상표권 보유자 | 리스크 | 대응 |
|------|---------------|--------|------|
| Neve 1073 | AMS Neve Ltd. | **중간** (상표 활성, 적극적 방어) | "1073-style" 표현 회피, "classic 1073 topology" 수준에서 멈춤 |
| Pultec EQP-1A | Pulse Techniques Inc. (현재 재설립) | 중간 | 동일 접근 |
| Fairchild 670 | 상표 휴면 상태, 여러 clone 존재 | 낮음 | 비교적 자유로운 표현 가능 |
| LA-2A | Universal Audio Inc. | **높음** (UAD가 적극 방어) | 신중한 표현 필요 |
| 1176 | Universal Audio Inc. | 높음 | 동일 |
| Manley | Manley Laboratories Inc. | **매우 높음** (EveAnna Manley 적극 방어) | "Manley" 이름 회피, "modern vari-mu" 표현만 |
| Culture Vulture | Thermionic Culture | 중간 | "thermionic multi-stage saturation" 수준 |
| Altec 436C | Altec Lansing (현재 여러 엔티티) | 낮음 | 비교적 자유 |
| RCA BA-6A | RCA (상표 분산) | 낮음 | 비교적 자유 |
| EMI TG12345 / Abbey Road | Abbey Road Studios (Universal Music) | **매우 높음** (공식 라이선스 정책) | "70s British console" 수준으로만 |

### 11.13.3 마케팅 문구 작성 가이드

**좋은 예시** (안전):
- > "Inspired by the classic British 1970s console preamp topology..."
- > "Modeled after a legendary variable-mu compressor from the late 1950s..."
- > "Brings the sound of passive LC equalization with tube makeup gain..."

**피해야 할 표현**:
- ~~"Exact emulation of the Neve 1073"~~ (trademark risk)
- ~~"Our Fairchild 670"~~ (직접 사용)
- ~~"LA-2A compatible"~~ (암시적 동일성)

### 11.13.4 음향 샘플/비교 자료

- 측정 샘플을 공개할 때는 **"our plugin vs reference unit"** 형태로만
- 레퍼런스 유닛의 이름은 **설명 맥락**에서만 언급
- 매뉴얼/백서에서는 **기술적 회로 원리**(public domain)에 집중, 상표 표현 최소화

### 11.13.5 다음 문서 예고

[`17-legal-and-licensing.md`](./17-legal-and-licensing.md) (예정)에서 다룰 주제:

1. 국가별 상표법 차이 (US trademark act vs EU, Korea 상표법)
2. "Emulation"의 법적 정의와 판례 (Waves vs UAD 사례 등)
3. Nominative fair use doctrine (미국)
4. Open-source 라이선스 호환성 (우리가 참조할 코드/데이터)
5. Patent 이슈 (예: Softube의 Tape Echo 특허, Waves의 일부 알고리즘 특허)
6. 측정 데이터의 저작권 (Julian Krause 측정치 사용 허가 등)
7. 실제 하드웨어 구매/측정 프로토콜 (우리 자체 측정으로 대체해야 할 경우)

---

## 부록: 측정 데이터 출처 종합 목록

### A.1 공개 측정 데이터 소스

1. **Julian Krause YouTube Channel**
   - 2019~현재, 50+ 하드웨어 측정
   - Audio Precision APx555를 사용한 표준화 측정
   - URL: youtube.com/@JulianKrause (참고용, 실제 사용 시 허가 필요)

2. **Gearspace (구 Gearslutz) "Shootout" Threads**
   - 커뮤니티 blind test + 측정
   - 예: "Neve 1073 shootout" (2018), "Pultec EQP-1A clones" (2019)
   - 변동성 높지만 다수의 유닛 unit-to-unit 편차 파악 가능

3. **AES Convention Papers**
   - IEEE Xplore, AES E-Library에서 유료/무료 접근
   - 주요 참조: Hughes (1999, 2013), Abel & Smith (2006), Olson (2003)

4. **Universal Audio White Papers**
   - UAD의 공식 모델링 백서 (LA-2A, 1176, Neve 등)
   - 마케팅 목적이지만 기술적 insight 풍부

5. **Softube Academic Collaborations**
   - Stockholm KTH와의 공동 연구 결과 공개

### A.2 우리의 자체 측정 필요성

다음 항목은 **공개 데이터 부족**으로 자체 측정 필요:

- Altec 436C의 thermal drift behavior
- RCA BA-6A의 pentode parameter extraction
- Culture Vulture의 multi-stage interaction
- Marinair LO1166의 정확한 B-H 루프 측정
- Fairchild 670의 push-pull mismatch 통계 (유닛 간)

자체 측정 프로토콜은 [`09-measurement-and-validation.md`](./09-measurement-and-validation.md) 참조.

---

## 요약

- **10개 레전더리 유닛**을 회로/하모닉/캐릭터/재현 접근의 4축으로 분석
- **MVP 추천 조합**: Neve 1073 + Pultec EQP-1A + Fairchild 670 — 카테고리 완결성 + 기술 스펙트럼 완결성 + 시장성
- **Phase 2**: LA-2A + Manley + 1176 (dynamics 완성)
- **Phase 3**: Culture Vulture + Altec + RCA + EMI (character/niche)
- 각 유닛의 재현에는 문서 01 (triode/pentode physics), 02 (transformer), 03 (time-varying), 04 (impedance), 05 (harmonic psychoacoustics)의 물리 모델이 조합적으로 필요
- 법적 리스크는 **fair descriptive use** 원칙으로 관리하되, Manley와 Abbey Road는 특별히 신중 필요

> 다음 문서 [`12-fet-and-optical-addendum.md`](./12-fet-and-optical-addendum.md) (예정)에서는 FET(1176) 및 Optical(LA-2A) 계열의 상세 모델을 다룰 예정이다.
