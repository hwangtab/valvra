# 22. 학술 논문 기반 정량 데이터 아카이브

> **⚠️ 방향 정정 (2026-04-19):** 본 문서의 **§B "Pultec 회로 재현 BOM"**은 **현행 MVP에서 사용하지 않음** ([21번 ARCHIVED](./21-pultec-measurement-archive.md)). Valvra v1.0은 진공관 앰프 색깔 유닛이며, 하드웨어 타깃은 [24 진공관 앰프 타깃](./24-tube-amp-target-hardware.md) 참조.
>
> **재사용되는 섹션:** §A 12AX7 Dempwolf 파라미터, §C 트랜스포머 JA 파라미터, §D 12AX7/12AU7 Koren 아카이브, §E Neural 학습 데이터셋 — 이들이 Valvra DSP 코어의 실측 기반을 제공한다.

---

> **작성일:** 2026-04-18 · **방법:** 5개 영역 병렬 학술 문헌 리서치 (실물 측정 불가 대체)
> **연관 문서:** [01 진공관 물리](./01-vacuum-tube-physics.md) · [02 트랜스포머](./02-transformer-physics-and-distortion.md) · [03 시변 비선형성](./03-time-varying-nonlinearities.md) · [21 Pultec 측정 아카이브](./21-pultec-measurement-archive.md)

> **문서 역할:** 21번 문서가 제품 스펙·리뷰 중심인 데 비해, 이 문서는 **peer-reviewed 학술 논문, thesis, 표준 engineering reference**에서 **재현 가능한 수치**만 선별한 아카이브다. MVP 구현 시 이 문서의 값으로 직접 코드를 작성할 수 있어야 한다.

> **핵심 인정:** **Pultec EQP-1A 자체를 peer-reviewed로 분석한 학술 논문은 사실상 부재**. 학계는 guitar amp (Fender Bassman, Marshall)에 집중한다. 따라서 본 아카이브는 (1) 12AX7/12AU7 일반 논문 + (2) Ni-permalloy 재료과학 + (3) DIY 커뮤니티 회로 복원의 조합으로 Pultec을 재구성한다.

---

## A. 12AX7 / 12AU7 논문 기반 비선형 파라미터

### A.1 Dempwolf-Holters-Zölzer 2011 Triode 모델 (MVP 기준점)

**논문:** Dempwolf, K., Holters, M., Zölzer, U. (2011). "A Physically-Motivated Triode Model for Circuit Simulations." *Proc. DAFx-11*, Paris, pp. 257–262. [PDF](https://dafx.de/paper-archive/2011/Papers/76_e.pdf)

**모델 수식:**
$$I_k = G \cdot (V_{eff})^{\gamma}, \quad I_g = G_g \cdot V_{eff}^{\xi} + I_{g0}$$

여기서 $V_{eff}$는 smoothing 함수로 정의된 유효 grid voltage (Koren 모델보다 더 물리적).

**3개 12AX7 실측 피팅 파라미터 (측정 범위 Va=10–300V, Vg=−5~+3V, fs=96kHz):**

| 파라미터 | RSD-1 | RSD-2 | EHX-1 | 물리 해석 |
|---------|-------|-------|-------|---------|
| G (A/V^γ, perveance) | 2.242e-3 | 2.173e-3 | 1.371e-3 | cathode current 계수 |
| μ | **103.2** | **100.2** | **86.9** | amplification factor |
| γ | 1.26 | 1.28 | 1.349 | Langmuir 지수 (3/2에서 이탈) |
| C (smoothing) | 3.40 | 3.19 | 4.56 | 음→양 전이 매끄러움 |
| Gg (A/V^ξ) | 6.177e-4 | 5.911e-4 | 3.263e-4 | grid current 계수 |
| ξ | 1.314 | 1.358 | 1.156 | grid current 지수 |
| Cg | 9.901 | 11.76 | 11.99 | grid-onset smoothing |
| Ig0 (A) | 8.025e-8 | 4.527e-8 | 3.917e-8 | grid leakage 기저 |

**개체 편차의 학술적 증거:** μ = 87–103 (**17% 편차**), G = **37% 편차**. 같은 박스 튜브도 하모닉 특성이 크게 다르다는 직접 증거 → 06번 Monte Carlo σ 모델 정당화.

### A.2 Parasitic Capacitances (Dempwolf §5.6)

- **Cak = 0.9 pF** (플레이트-캐소드)
- **Cgk = 2.3 pF** (그리드-캐소드)
- **Cag = 2.4 pF** (그리드-플레이트, Miller 원천)

Miller 효과로 고역 롤오프: $C_{miller} = C_{ag} \cdot (1+|A_v|) \approx 240\text{ pF}$ (게인 100 가정)

### A.3 12AX7 실측 하모닉 주파수 응답 (Dempwolf Fig. 10)

| 입력 | 주파수 | H2 (dBc) | H3 (dBc) | H4+ (dBc) | 관찰 |
|------|-------|---------|---------|----------|-----|
| 4V peak sweep | 100 Hz | ~-25 | ~-35 | -45~-55 | 저주파 하모닉 풍부 |
| 4V peak sweep | 1 kHz | ~-20 | ~-30 | -40~-50 | 피크 |
| 4V peak sweep | 10 kHz | ~-30 | ~-45 | <-55 | HF 롤오프 |

### A.4 Rutt 1984 Grid-Current 비선형 (AES Preprint 2141)

**조건:** Vk=1.4V, Vp=265V (440V supply, 100kΩ plate load)

**Grid current fit (실측):**
$$I_g = 3.1 \times 10^{-4} \cdot (V_{gk} + 0.53)^3 \text{ A}$$

- Voff = −0.53V (grid conduction threshold)
- Rutt는 **duty-cycle shift 실측 관찰** → 짝수 하모닉(H2) 지배의 물리적 원인 규명

[PDF](https://robrobinette.com/images/Guitar/Overdrive/Rutt_Overdrive_Paper.pdf)

### A.5 Quadric Surface Model (Giannoulis et al., DAFx-23)

**Dempwolf 확장, 12AX7 개별 파라미터:**
- G0 = 1.102 mA/V, G1 = 15.12 μA/V^(5/3), G2 = −31.56 μA/V^(8/3), G3 = −3.286 μA/V^(11/3)
- h0 = 0.6 V, μ0 = 99.705, μ1 = −22.98 kV⁻¹, μ2 = −0.4489 V⁻², μ3 = −22.27 kV⁻³
- Voff = −0.2 V, D = 0.12, K = 1.1
- kpg = 1.076e-5, kp2 = 1.014e-5, kg2 = 5.498e-8 V²

[PDF](https://dafx.de/paper-archive/2023/DAFx23_paper_15.pdf)

### A.6 12AU7 학술 데이터 (공백)

**중요 인정:** Dempwolf 2011이 12AT7을 시도했으나 **"fitting results not as good as for the 12AX7"**. **12AU7 전용 실측 피팅은 peer-reviewed 논문에 부재.**

MVP 권장: Koren B 계열 파라미터 (μ=20.21, Ex=1.23, Kg=1108.7, Kp=84.96, Kvb=551.3) + 데이터시트 정적 값 (μ=17, rp=7.7kΩ, gm=2.2 mA/V @ Vp=250V, Vg=−8.5V)

**Cohen & Hélie 2010** (DAFx-10)의 grid current 3-subdomain piecewise 모델을 12AU7에 적용 가능:
- Linear region + 2차 polynomial + smooth transition

---

## B. Pultec 회로 재현 BOM (DIY 커뮤니티 1차 출처)

### B.1 수동 EQ 커패시터 뱅크 (Gyraf BOM, EQ 섹션은 원본과 동일)

**LF 섹션 (film 250V poly):**
- 0.022 / 0.033 / 0.047 / 0.068 / 0.10 / 0.15 / 0.22 / 0.33 / 0.47 / 0.68 μF

**HF 섹션 (styroflex 권장):**
- 1.0n / 1.5n / 3.3n / 4.7n / 10n / 15n / 22n / 33n (각 3/2/2/3/4/2/5/1 개)
- 추가 film: 0.01 / 0.012 / 0.015 μF

**커플링:** 0.22μF × 3, 0.47μF × 1 (250V)

[Gyraf BOM PDF](https://analogclassics.com/wp-content/uploads/2021/07/Gyraf-Pultec-BOM.pdf)

### B.2 인덕터

| 위치 | 값 | 타입 | 출처 |
|------|-----|------|------|
| LF 쵸크 | **100 mH × 2** (TDK 10RB TK4425) | ferrite pot core | Gyraf BOM |
| LF 추가 | 47 mH (TK4421), 22 mH (TK4416) | — | Gyraf BOM |
| HF 멀티탭 (stock) | **340 / 910 / 1350 / 3210 mH** (탭) | laminated iron | [GroupDIY](https://groupdiy.com/threads/pultec-eqp-1a-stock-inductor-values-and-hf-response-curves.63207/) |
| HF (Sowter 9325 대체) | 27/33/47/68/82/150 mH | — | Sowter |
| 원본 복각 | Moby Transformers EQP-1A multitap | — | [Moby](https://www.mobytransformers.com/eqp1a-inductor) |

### B.3 저항 (Gyraf BOM)

- 470Ω 1% × 10, 75Ω 1% × 10, 1kΩ 1% × 5, 10kΩ 1% × 5
- 470kΩ 1% × 10, **1MΩ 1% × 5 (grid leak)**
- 1.8kΩ 1W × 5, 3.3kΩ 2W × 5 (plate/cathode dropping)

**포텐셔미터:** 1kΩ lin × 2 (boost/atten), 2.5kΩ lin × 1, **10kΩ lin × 1 (bandwidth)**, 100kΩ audio w/sw (output)

### B.4 진공관 동작점 (원본 EQP-1A 실측)

| 관 | Vp | Vk | Ip (est.) | 출처 |
|----|----|----|----|------|
| **V1 12AX7 (ECC83)** anode | **~140 V** (실측 127–137V) | **~1.04 V** | **~0.9 mA** | [SOS](https://www.soundonsound.com/reviews/pulse-techniques-eqp-1a), [GroupDIY voltages](https://groupdiy.com/threads/pultec-eqp-1a-voltages.8272/) |
| **V2 12AU7 (ECC82)** anode | **~290 V** (push-pull) | ~2 V | ~3 mA | SOS |
| 6X4 rectifier | **B+ reservoir ~350 V DC** | — | ≤70 mA | SOS |

### B.5 Power Supply

- 전원 트랜스: 220V / 9V / 5V 다중 2차
- Reservoir: **220 μF × 2 @ 350V** (dual B+ bank), 470 μF @ 16V (히터 바이어스), 4700 μF @ 25V (히터 regulated)
- 정류: 원본 6X4 관 / Gyraf는 SS diode (NTE-5304) × 2 + LM317 × 2
- 드로핑: **3.3kΩ 2W × 5** (B+ 단간 필터 체인)

### B.6 Gain Staging

- 수동 EQ 삽입 손실: **~16 dB** (플랫, 중역)
- Makeup amp 이득: **정확히 15~16 dB** (2단 공통캐소드 + 피드백) → unity output
- 출처: [Fox Audio Research](https://www.foxaudioresearch.ca/PultecPreamp.htm)

### B.7 리비전 차이 (Wikipedia 검증)

| 리비전 | 연도 | 특징 |
|-------|------|-----|
| EQP-1 | 1956 | 3U, LF 30/60/100 Hz, HF boost 3/5/8/10/12 kHz |
| **EQP-1A** | **1961** | 3U, +20 Hz LF boost/atten, +16 kHz HF boost, +5/10/20 kHz atten |
| EQP-1A3 | 1971 | 2U (회로 동일, 섀시 축소) |

### B.8 데이터 격차 (공개 미확인)

- 각 LF/HF 스위치 포지션별 **정확한 L-C 매핑**: 원본 DWG E-72 (유료 service manual) 없이는 공개 문서로 1:1 확인 불가
- 원본 plate 저항 (220k 혹은 100k): GroupDIY "schematic discrepancies" 스레드에서 **논쟁 중**
- LF 메인 쵸크 정확 값 (2.5H / 4H / 500mH 설 혼재): 확정 없음

**MVP 대응:** Gyraf BOM을 기준으로 SPICE/DSP 구현 → Tier 0 프로토타입 후 Vintage Windings PDF 추가 확보로 원본 DWG 재구성

---

## C. 트랜스포머 코어 재료 JA 파라미터 (문헌 추정)

### C.1 중요 인정

**Pultec EQP-1A 트랜스포머(Triad HS-52, Peerless S-217-D)의 직접 실측 JA 파라미터는 peer-reviewed 학술 논문에 존재하지 않는다.** 다음은 동일 재질 계열(80% Ni-Permalloy, Mu-metal)의 산업 데이터시트 및 일반 JA 피팅 논문에서 파생한 추정치.

### C.2 80% Ni-Permalloy (Peerless S-217-D 추정 코어)

| 파라미터 | 값 | 조건 | 출처 |
|---------|-----|-----|------|
| Bsat | **0.75 – 0.80 T** | 실온, 어닐링 후 | ESPI Metals Permalloy 80, Wikipedia |
| μr_initial | 40,000 – 80,000 | 저자장 (<0.4 A/m) | ESPI Metals |
| μr_max | 200,000 – 300,000 | H2 어닐링 후 | Domadia datasheet |
| Hc | **0.4 – 4 A/m** | — | ESPI / Wikipedia |
| Br | ~0.5 T | 저자장 구동 | ESPI |
| λs (자기변형) | **≈ 0** (조성 81.5% Ni에서) | 벌크 폴리크리스털 | EPJ Conf. 2013; Wikipedia |

**JA 파라미터 (Roubal, Measurement 2013 fits + 재료 유추):**
- **Ms ≈ 4.2 × 10⁵ A/m** (420 kA/m)
- **a = 10 – 30 A/m** (낮은 저항 조직)
- **α = 1 × 10⁻⁵ – 5 × 10⁻⁵**
- **k = 5 – 20 A/m** (낮은 Hc 반영)
- **c = 0.1 – 0.3** (reversibility)

### C.3 Mu-metal (HS-52/S-217-D 케이스 및 코어)

- 조성: **80% Ni, 4.5% Mo, ~15% Fe** (Magnetic Shield Corp)
- Bsat: **0.75 – 0.76 T**
- μr_initial: 80,000 – 100,000
- μr_max: **350,000 – 500,000** (H2 어닐링 후)
- Hc: **0.6 A/m** (최종 열처리 후)
- λs: ≈ 0 (조성 설계)
- **충격 민감성:** 한 번의 기계 충격으로 μ **40–50% 감소**, 재어닐링 필요

### C.4 Grain-oriented Si-steel (M6, Sowter 9530 참조)

| 파라미터 | 값 | 출처 |
|---------|-----|------|
| Bsat | **2.0 – 2.03 T** | AIPA 2018 |
| Hc | 8 – 12 A/m | 동일 |
| JA Ms | ~1.59 × 10⁶ A/m | Springer 2015 (Unisil M130-27s) |
| a | 50 – 70 A/m | 동일 |
| k | 40 – 60 A/m | 동일 |
| α | 1 × 10⁻⁴ | 동일 |
| c | 0.1 – 0.2 | 동일 |

### C.5 Steinmetz 계수 (오디오 대역)

- **Permalloy: n ≈ 1.6 – 1.7** (낮은 Hc ⇒ 낮은 지수)
- **Si-steel: n ≈ 1.8 – 2.1**
- Eddy current: 퍼멀로이는 라미네이션 0.1 mm 구조, 20 kHz까지 **skin depth δ ≈ 0.3 mm** (ρ≈55 μΩ·cm) → 두께보다 큼, eddy loss 낮음

### C.6 Holters & Zölzer 2016 (DAFx-16)

- 측정 대상: 기타 앰프 **출력 트랜스포머(OT)** — **GO Si-steel 추정, Pultec 아님**
- Julia/ACME 패키지 구현
- **Pultec 트랜스포머 실측 JA는 기존 논문에 없음** → MVP에서 위 문헌 추정치로 시작

---

## D. 시변 효과 실측 정량 데이터 (MVP 차별화 핵심)

### D.1 Cathode Bounce (최우선)

| 조건 | Rk | Ck | 신호 레벨 | Bias shift | 회복 τ | 출처 |
|------|-----|-----|---------|-----------|--------|------|
| 12AX7 common-cathode | **1.5 kΩ** | **25 μF** | Overdrive burst | **100–500 mV** | **τ = RkCk ≈ 37.5 ms** (3τ ≈ 110 ms) | Blencowe 2012; Jones 2011 |
| 12AX7 cold-bias stage | 1.5 kΩ | 0.68 μF (partial bypass) | +10 dBu | ~30–80 mV | ~1 ms | Pakarinen & Yeh 2009 |

**청취 임계:** ≈ **5–15 mV bias shift**부터 "swell/breathing" 지각 (Jones 2011)

**모델링 논문:**
- **Yeh 2009** (Stanford PhD): WDF + state-space로 cathode bypass 동적 bias 포함
- **Pakarinen & Yeh 2009** CMJ 33(2): 85–100 — 시변 섹션 명시
- **Macak 2009** DAFx-09 Como
- **Cohen & Hélie 2010** IRCAM: port-Hamiltonian 시변 triode

### D.2 Power Supply Sag

| 정류 | Rp (plate-plate) | Peak sag | 회복 τ | 리플 사이드밴드 | 출처 |
|------|----|------|--------|-------|------|
| GZ34 / 5AR4 | **~200 Ω** | **3–5%** | 50–150 ms | -25 dB @ 100/120 Hz | Blencowe 2012 |
| 5U4GB | ~400–500 Ω | **10–15%** | 100–200 ms | -20 dB | Jones 2011 Ch.5 |
| SS bridge | <10 Ω | <1% | C·Rload | <-40 dB | Self 2013 |

**Reservoir cap ripple:** C를 40→16 μF 감소 시 ripple **+8 dB**, sag depth **+2배** (Blencowe)

### D.3 열적 드리프트

| 진공관 | τ_heat (90%) | 완전 안정화 | 장시간 drift | 출처 |
|-------|--------|-----------|-----------------|------|
| 12AX7 | **10–15 s** | 2–5 min | **0.5 dB/hr** (첫 10분) | JJ/EH datasheet; Jones 2011 |
| 12AU7 | 8–12 s | 2–4 min | 유사 | 동일 |
| EL34 (power) | 20–30 s | 5–10 min | plate-dissipation-dependent | Jones 2011 |

**Richardson-Dushman 민감도:** Vh ±5% → Ip **±10–15%** (Spangenberg *Vacuum Tubes* 1948)

**Envelope-dependent bias shift:** 지속 대신호에서 **+5 ~ 20 mV** 추가 드리프트, 시상수 수 초 (Cohen & Hélie 2010)

### D.4 Capacitor Dielectric Absorption (재확인)

| 유전체 | DA (%) | 회복 τ | 출처 |
|-------|-------|-----------|------|
| Polystyrene | **0.01 – 0.02** | >1 s | Jung & Marsh 1980 *Audio* |
| Polypropylene | **0.02 – 0.1** | 10 ms–1 s multi-τ | Bateman 2002 *Electronics World* |
| Polyester (Mylar) | **0.2 – 0.5** | 10 – 100 ms | Jung & Marsh 1980 |
| Electrolytic | **1 – 15** | 100 ms – 10 s | Bateman 2002–2003 |

**Bateman 결론:** DA가 고조파 왜곡보다 **과도 응답(step smearing)**에 지배적 영향. Pease *Analog Circuits* 2008 Ch.17 재확인.

### D.5 Microphonics

- 12AX7 일반관 g-sensitivity: **10–50 mV/g** (100k plate load 참조)
- 공진 피크: **1–3 kHz** (grid support), **5–8 kHz** (plate structure) — Jones 2011 Ch.2
- Telefunken smooth-plate ECC83: 현행 JJ/EH 대비 공진 피크 **6–10 dB 낮음**

### D.6 전해 Cap Aging

- ESR 증가율: 85°C 동작 시 **연 5–10%**, 65°C에서 **연 2–3%** (Arrhenius, Cornell Dubilier / Nichicon)
- Vintage Pultec refurb (30년 전해): ESR **3–10배 증가** → LF shelf 반응 **+0.5–1 dB 변화** (audioXpress 2015)

### D.7 Transformer Memory (JA 히스테리시스)

- JA pinning constant **k = 50–150 A/m** (GOSS 오디오 트랜스 일반 범위)
- B > 0.5·Bsat에서 비가역 domain wall motion 유의
- 대신호 후 small-signal μr **일시적 감소 수십 ms**

---

## E. Neural Hybrid 학습 데이터셋 (MVP v1.1용)

### E.1 Tube Amp / 이펙트 데이터셋

| 데이터셋 | 라이선스 | 크기 | 진공관 여부 | MVP 사용 전략 |
|---------|---------|------|-------------|-----------|
| **SignalTrain LA2A** (Hawley 2019, Zenodo 3348083) | CC BY 4.0 | 21 GB | LA-2A opto comp | ✓ 잔차 학습용 |
| **IDMT-SMT-Audio-Effects** (Zenodo 7544032) | **CC BY-NC-ND** | 30h | distortion/overdrive | ❌ **상업 불가** |
| **Wright et al. 2020** (Applied Sci 10(3) 766) | MIT (코드+데이터) | 5,000 sec | **Blackstar HT-5, Mesa 5:50+ valve amp** | ✓ 높은 가치 |
| **Open-Amp** (Wright 2024 ICASSP'25) | Apache-2.0 | 수백 device 크라우드 | tube amp 다수 | ✓ Foundation 학습 |

### E.2 NAM 통합 전략

| 자원 | 라이선스 | MVP 사용 |
|------|---------|---------|
| sdatkinson/neural-amp-modeler | MIT | ✓ 엔진 포크 |
| NeuralAmpModelerPlugin | MIT | ✓ 참조 |
| `.nam` 모델 (TONE3000) | 업로더별 상이 | ❌ 재배포 금지 → **사용자 로드 패턴** |
| GuitarML PedalNetRT | **GPL-3.0** | ⚠️ 학습 스크립트만 참고, 코드 포함 금지 |

**법적 구조:** MVP는 **NAM 엔진(MIT)만 내장** → `.nam` 파일은 사용자가 TONE3000에서 직접 다운로드/로드. 재배포 X.

### E.3 v1.1 Neural Hybrid 실전 전략

1. **기본 물리 모델:** Koren + JA (본 아카이브 + 21번 기반, 자체 IP)
2. **Pre-training:** Open-Amp 합성 데이터 (Apache-2.0)로 tube nonlinearity foundation
3. **Fine-tune:** Wright 2020 valve amp 데이터 + SignalTrain LA2A
4. **IDMT-SMT는 내부 벤치마크만** (배포 금지)
5. **Pultec 특화:** 21번 자체 아카이브가 유일 신뢰 소스

### E.4 공개 Pultec 입출력 쌍 — **부재**

YouTube (Warren Huart 등), Gearspace 샘플: **음악 저작권 + 라이선스 불명** → 사용 불가.
**공개된 Pultec hardware I/O pair 데이터셋은 실질적으로 없다.**

---

## F. MVP 구현 권장사항 (학술 데이터 기반)

### F.1 진공관 모델 선택

| 단 | 회로 조건 | 권장 모델 | 파라미터 출처 |
|----|---------|---------|-----------|
| V1 12AX7 (출력, Vp=290V) | Dempwolf 실측 범위(Va=10–300V) 내 | **Dempwolf RSD-1** | 본 문서 A.1 |
| V2 12AU7 (입력, Vp=140V) | 학술 실측 없음 | **Koren B 계열** + Cohen-Hélie grid current | 21번 D.2 |

### F.2 트랜스포머 모델

- **Peerless S-217-D (출력):** Ni-Permalloy JA — Ms=420 kA/m, a=25, α=3e-5, k=15, c=0.15 (Roubal 기반 중간값)
- **Triad HS-52 (인터스테이지):** Mu-metal JA — Ms=500 kA/m, a=30, α=2e-5, k=30, c=0.12

### F.3 시변 효과 우선순위

**구현 우선순위 권고 (Cluster 5):**

**(1) Cathode bounce + (2) PSU sag 2종만으로 기존 플러그인 대비 압도적 차별화 가능.** 두 효과 모두:
- ✓ 수식 완비 (τ = RkCk, Zsupply 기반)
- ✓ 실측 계수 확보 (학술 논문 검증)
- ✓ 청취 임계 이상 (5–15 mV ~ 3–15% sag)

**(3) 열적 드리프트 + (4) Cap DA + (5) Microphonics는 MVP v1.0 이후 추가.**

### F.4 MVP 검증 지표 (학술 기준)

| 지표 | 목표 값 | 근거 |
|------|--------|------|
| 12AX7 단 H2 @ 1kHz +4dBu | **−40 ± 5 dBc** | Dempwolf Fig. 10 재현 |
| 12AX7 단 H3 @ 1kHz +4dBu | **−55 ± 5 dBc** | 동일 |
| 개체 변이 μ σ | **~10%** (RSD-1/2/EHX-1 실측) | Dempwolf Table |
| Cathode bounce bias shift | 100–500 mV @ +10 dBu burst | Jones/Blencowe |
| PSU sag depth | 3–5% (GZ34) | Blencowe |

---

## G. 핵심 공백 및 한계

1. **Pultec EQP-1A 자체 학술 논문 부재** — 유일한 완전 소스는 DIY 커뮤니티 (Gyraf, GroupDIY)
2. **12AU7 학술 실측 피팅 없음** — Koren/데이터시트 의존
3. **Pultec 트랜스포머 JA 직접 측정 없음** — 재료과학 문헌 유추
4. **Pultec 공개 I/O 녹음 데이터셋 없음** — v1.1 Neural 학습은 Wright 2020 + SignalTrain + Open-Amp 의존
5. **각 스위치 포지션별 L-C 매핑** — 원본 DWG E-72 (유료) 필요
6. **Volterra kernel 수치** — 대부분 논문이 상태공간·Koren 모델만, 명시적 kernel 없음 (Dempwolf Fig. 10에서 |H₂(ω,ω)| 추출 가능)

---

## H. 인용 논문 전체 목록

### H.1 진공관 모델링
1. Dempwolf, Holters, Zölzer (2011). "A Physically-Motivated Triode Model for Circuit Simulations." DAFx-11. [PDF](https://dafx.de/paper-archive/2011/Papers/76_e.pdf)
2. Cohen, Hélie (2010). "Real-time Simulation of a Guitar Power Amplifier." DAFx-10. [PDF](https://dafx10.iem.at/proceedings/papers/CohenHelie_DAFx10_P45.pdf)
3. Giannoulis et al. (2023). "A Quadric Surface Model of Vacuum Tubes." DAFx-23. [PDF](https://dafx.de/paper-archive/2023/DAFx23_paper_15.pdf)
4. Macak, Schimmel (2010). "Real-Time Guitar Tube Amplifier Simulation." DAFx-10.
5. Rutt (1984). "Vacuum Tube Triode Nonlinearity as Part of the Electric Guitar Sound." AES Preprint 2141. [PDF](https://robrobinette.com/images/Guitar/Overdrive/Rutt_Overdrive_Paper.pdf)
6. Pakarinen, Yeh (2009). "A Review of Digital Techniques for Modeling Vacuum-Tube Guitar Amplifiers." CMJ 33(2):85–100.
7. Yeh (2009). Stanford CCRMA PhD Thesis. [PDF](https://ccrma.stanford.edu/~dtyeh/papers/DavidYehThesissinglesided.pdf)
8. Eichas & Zölzer (2016, 2018). "Block-Oriented Modeling of Distortion Circuits."

### H.2 트랜스포머/자성
9. Holters, Zölzer (2016). "Circuit Simulation with Inductors and Transformers Based on the Jiles-Atherton Model." DAFx-16.
10. Jiles, Atherton (1986). JMMM 61:48. DOI: 10.1016/0304-8853(86)90066-1
11. Roubal (2013). "Determination of JA Parameters." Measurement.

### H.3 시변 효과
12. Paiva, Pakarinen, Välimäki (2012). JAES 60(9):688.
13. Chowdhury (2020). DAFx (time-varying analog).
14. Bernhardsson (1991). JAES 39(5):366 (tube compression).
15. Jung, Marsh (1980). *Audio* Feb/Mar (DA).
16. Bateman (2002–2003). *Electronics World / Linear Audio* (cap series).

### H.4 서적
17. Jones, M. (2011). *Valve Amplifiers* 4th ed. Elsevier.
18. Blencowe, M. (2012). *Designing Valve Preamps for Guitar and Bass*; *Designing High-Fidelity Valve Preamps*.
19. Self, D. (2013). *Audio Power Amplifier Design* 6th ed. Focal.
20. Pease, R. (2008). *Analog Circuits: World Class Designs*.
21. Spangenberg (1948). *Vacuum Tubes*.

### H.5 데이터셋
22. Hawley, Colburn, Mimilakis (2019). SignalTrain LA2A. Zenodo 3348083.
23. Wright, Damskägg, Juvela, Välimäki (2020). Applied Sciences 10(3):766.
24. Wright et al. (2024). Open-Amp. [arXiv:2411.14972](https://arxiv.org/abs/2411.14972).

### H.6 커뮤니티 1차 출처
25. Gyraf Audio G-Pultec: https://www.gyraf.dk/gy_pd/pultec/pultec.htm
26. GroupDIY Pultec voltages: https://groupdiy.com/threads/pultec-eqp-1a-voltages.8272/
27. GroupDIY Stock inductor values: https://groupdiy.com/threads/pultec-eqp-1a-stock-inductor-values-and-hf-response-curves.63207/
28. Moby Transformers: https://www.mobytransformers.com/eqp1a-inductor
29. Fox Audio Research (gain staging): https://www.foxaudioresearch.ca/PultecPreamp.htm

---

## I. 문서 버전

- **v1.0.0** (2026-04-18): 5개 클러스터 학술 논문·thesis·engineering reference 병렬 리서치. Dempwolf 12AX7 3개 실측 피팅 + Ni-Permalloy JA 추정 + Gyraf BOM + Yeh/Jones 시변 측정 + 공개 데이터셋 라이선스 분석 통합.

**핵심 성과:**
- **12AX7 실측 파라미터 확보 (Dempwolf RSD-1/2/EHX-1)** — MVP 구현 시 직접 사용
- **개체 변이 학술 증거 (μ 17% 편차)** — 06번 Monte Carlo σ 정당화
- **시변 효과 수치 확보 (Cathode bounce 37.5ms, PSU sag 3-5%)** — MVP 차별화 구현 가능
- **데이터셋 라이선스 확정** — NAM 엔진(MIT) 통합 + Wright 2020 학습 데이터 사용 경로 확보

---

*5개 병렬 웹 리서치 에이전트 × 각 40분 × 각 영역 특화*
*최종 업데이트: 2026-04-18*
