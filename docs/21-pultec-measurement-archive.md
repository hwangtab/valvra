# 21. Pultec EQP-1A — 측정 데이터 아카이브 📦 **ARCHIVED (참고용)**

> **⚠️ 상태 (2026-04-19):** **현행 MVP 타깃 아님.** 사용자 원 요청 재확인 결과 "진공관 앰프 색깔 유닛"이 맞는 방향이며 Pultec(이퀄라이저)은 v1.0 범위에서 제외됨. [20 MVP v2.0](./20-mvp-scope-decision.md) · [24 진공관 앰프 타깃](./24-tube-amp-target-hardware.md) 참조.
> 
> 본 문서는 **참고 자료**로 보존한다. Pultec 공식 스펙, Triad/Peerless 트랜스포머 데이터, 경쟁 Pultec 플러그인 벤치마크는 미래 v2.0+ 또는 별도 EQ 플러그인 프로젝트에서 재사용 가능.
>
> **Valvra v1.0에서 재사용되는 부분:** §C.1 Triad 트랜스포머 JA 파라미터, §C.3 Ni-Permalloy Bsat, §D 12AX7/12AU7 Koren 파라미터. **재사용되지 않는 부분:** §A 주파수 응답(EQ 커브), §B THD vs level (EQ 컨텍스트), §E 경쟁 Pultec 플러그인 비교.

---

> **작성일:** 2026-04-18 · **방법:** 웹 리서치 (실물 측정 불가로 공개 2차 자료 수집)
> **연관 문서:** [09 측정 방법론](./09-measurement-and-validation.md) · [11 타깃 하드웨어](./11-target-hardware-catalog.md) · [18 측정 인프라](./18-measurement-infrastructure.md) · [20 MVP 스코프](./20-mvp-scope-decision.md)

> **주의:** 이 문서의 모든 수치는 공개된 2차 자료(제조사 스펙, 리뷰, 포럼 측정, 클론 제품 데이터시트)에서 수집한 것이다. 대부분 원본 Pultec EQP-1A의 Audio Precision 수준 정량 측정은 공개되지 않는다. MVP 구현 시 **최소 1대의 실물 또는 고품질 클론(Warm Audio EQP-WA, Pulse Techniques re-issue)을 직접 측정**하여 이 아카이브를 보강할 것을 강력히 권장한다.

> **신뢰도 등급:**
> - **A+**: 제조사 공식 스펙 (Pulse Techniques, Sowter, Triad)
> - **A**: 정식 측정 보고 (Sound on Sound, Julian Krause 직접 측정)
> - **B**: 리뷰어 정성 평가 + 클론 제품 데이터시트
> - **C**: 커뮤니티 DIY 측정, 포럼 공유
> - **D**: 추정치 / 문헌 기반 유추

---

## A. 주파수 응답 및 EQ 커브

### A.1 Low Frequency Boost (20/30/60/100 Hz)

| 선택 주파수 | Boost Max (dB) | 커브 형태 | 신뢰도 |
|-----------|---------------|---------|------|
| 20 Hz | **+13.5 dB** | Wide shelf, 센터보다 아래에서 시작 | A (Pulse Tech 공식) |
| 30 Hz | +13.5 dB | Soft broad shelf | A |
| 60 Hz | +13.5 dB | Shelf | A |
| 100 Hz | +13.5 dB | Shelf | A |

-3dB/-6dB 정확 포인트: **공개 데이터 없음 → 자체 측정 필요**

### A.2 Low Frequency Attenuation

- Max: **−17.5 dB** @ 20/30/60/100 Hz 선택 [A]
- 커브: Corner 주파수가 Boost 대비 **약 반 옥타브 더 높게 시작** (의도적 비대칭) [A: SOS]
- 선택 주파수보다 **수 옥타브 위에서** shelf rolloff 시작 [B]

### A.3 "Pultec Trick" (Boost + Atten 동시 적용)

- Boost 2.5 + Atten 2.5 동시 → **+3 dB low shelf + −2 dB mid-band dip** [A: SOS]
- Dip 중심 주파수: 선택 주파수에 따라 **~250 Hz – 2 kHz 범위**에서 이동 [A]
- 30 Hz 동시 적용 시: ~80 Hz peak + ~200 Hz dip 생성 [B: Sweetwater]

### A.4 High Frequency Boost

- 선택 가능 주파수: **3, 4, 5, 8, 10, 12, 16 kHz** [A]
- Max: **+18 dB** [A]
- Bandwidth 효과: **Narrow가 Broad보다 최대 게인 +9 dB 더 높음** [A: SOS]
  - Narrow = 정확한 peak
  - Broad = 완만한 shelf-like 곡선

### A.5 High Frequency Attenuation

- 선택 가능 주파수: **5, 10, 20 kHz** [A]
- Max: **−16 dB** (shelf 형태) [A]

### A.6 Pulse Techniques 공식 스펙 (신뢰도 A+)

| 항목 | 값 |
|------|-----|
| Flat amp response | **20 Hz – 20 kHz, ±0.5 dB** |
| Input Impedance | **~600 Ω** (40 / 150 / 250 Ω 구성 가능) |
| Output Impedance | **~50 Ω** |
| Noise floor | **−92 dB below +10 dBm** |
| Distortion | **≤ 0.15% @ +10 dBm / 600 Ω** |
| Tubes | 12AU7 + 12AX7 + 6X4 (정류) |
| Flat 상태 저역 loss | ~1 dB (실측, 설계상 의도) [A: SOS] |
| B+ 전압 | **미공개 → 2차 회로도 ~325V DC** [C: SOS] |
| Max output level | **미공개** |

**출처:** pulsetechniques.com, SOS Pulse Techniques EQP-1A 리뷰

---

## B. 왜곡 및 하모닉 프로파일

### B.1 THD+N vs Frequency / Level

| 조건 | THD(+N) | 출처 | 신뢰도 |
|------|---------|------|------|
| +10 dBm / 600Ω, 1 kHz (공식) | **≤ 0.15%** | Pulse Techniques | A+ |
| +4 dBu, 1 kHz (AudioScape 클론) | < 0.1% | highontechnology.tech | B |
| +1.23V sine (Gyraf G-Pultec DIY) | **0.25% (H2≈H3)** | GroupDIY 측정 | C |
| Warm Audio EQP-WA, 20–100 Hz | 0.01% | Warm Audio 매뉴얼 | A (클론 스펙) |
| Warm Audio EQP-WA, 100 Hz–20 kHz | < 0.004% | 동일 | A |

**주파수별 스윕 (20/50/500/5k/10k/20k Hz), 레벨 스윕(−20/−10/0/+10/+20 dBu): 공개 데이터 없음 → 자체 측정 필요**

### B.2 하모닉 프로파일 (1 kHz)

| 하모닉 | 특성 | 신뢰도 |
|--------|------|------|
| H2 | **입력 레벨에 따라 progressive rise, 지배적** | A (SOS) |
| H3 | 상대적으로 고정 경향; Gyraf DIY 빌드는 H2와 동률 | B–C |
| H4~H7 | "remain pretty much constant" — dBc 미확인 | B |
| H2/H3 비율 | **수치화 미공개** — 실물 FFT 공개 없음 | — |

### B.3 Drive Curve (포화 영역)

- +20 dBu, +22 dBu 포화 특성 — **공개 데이터 없음**
- 클리핑 knee, soft-knee vs hard-knee — **공개 데이터 없음**
- **→ MVP 구현 시 우선순위 측정 항목**

### B.4 IMD

- SMPTE (60Hz + 7kHz, 4:1) — **미공개**
- CCIF (19kHz + 20kHz) — **미공개**
- **→ 자체 측정 필요**

### B.5 노이즈 플로어

| 장비 | 수치 | 신뢰도 |
|------|------|------|
| Pulse Techniques 공식 | **92 dB below +10 dBm** | A+ |
| Warm Audio EQP-WA | < −75 dB self-noise | A |
| A-weighted 20–20k residual | 미공개 | — |

### B.6 12AX7 / 12AU7 고유 하모닉 (회로 컨텍스트)

**EQP-1A 회로 상세:**
- Tubes: 1× **12AX7** + 1× **12AU7** + 1× 6X4 (정류) [Pulse Tech 공식]
- HT: **≈325 V DC** [SOS]
- 출력단 12AX7 anode: **≈290 V**
- 입력단 12AU7 anode: **≈140 V** [SOS]

**12AX7 동작 특성:**
- Anode load 68 kΩ → H2 증가 (100 kΩ 대비)
- Vp < 100V에서 H3 급증 (transistor 유사 거동)
- Pultec 출력단 Vp 290V → H2 지배 영역 [tdpri, diyAudio]

**12AU7 동작 특성:**
- Plate current ≈ 10 mA (12AX7의 ~10배)
- 낮은 μ (17), 낮은 왜곡
- Pultec 입력단 Vp 140V는 저전압 영역 → **H3 기여 증가 가능성** [ampgarage]

### B.7 핵심 공백

대부분 정성적 언급만 공개. 다음은 **MVP 개발 전 반드시 실물 측정으로 보강**해야 한다:
1. 주파수별 개별 하모닉(H2~H7) dBc 스위프
2. 레벨별 하모닉 발달 패턴
3. SMPTE/CCIF IMD
4. 클리핑 knee 형태
5. 12AX7/12AU7 각 단의 분리 측정

---

## C. 트랜스포머 측정 데이터

### C.1 입력/인터스테이지 Triad 트랜스포머

| 항목 | 값 | 신뢰도 |
|------|-----|------|
| 인터스테이지 모델 | **Triad HS-52** (허메틱 시일) | A (Triad Magnetics 공식) |
| 입력 모델 | **Triad A-67J** (150/600 → 600Ω, 논-허메틱 동등) | A |
| 턴 비율 (HS-52) | **20kΩ/5kΩ : 600/250/150/62.5Ω** (≈5.77:1 @ 20k→600) | A |
| 1차 인덕턴스 (HS-52, W2+W3) | **≈210 H @ 100Hz** | C (DIY 측정) |
| 누설 인덕턴스 | 공식 미공개; ≈20–40 mH 추정 | D |
| DC 저항 (HS-52) | W2+W3 = **311Ω**, W1/W4 = 13–18Ω | A |
| 주파수 응답 | 20 Hz – 20 kHz (±1 dB 추정) | B |

### C.2 출력 Peerless 트랜스포머

| 항목 | 값 | 신뢰도 |
|------|-----|------|
| 모델 | **Peerless S-217-D** (Altec 산하) | A (Peerless 1960 카탈로그) |
| 턴 비율 | **15kΩ : 600Ω** (≈5:1) + tertiary NFB winding 25:1 | A |
| 권선 구성 | 1575T 1차 × 2 bifilar, 352T 2차, 71T tertiary | A |
| 1차 인덕턴스 | **≈220 H @ 30 Hz** (피크) | B |
| 주파수 응답 | **5 Hz – 60 kHz (flat)**, resonance peaks 367/512 kHz | A |
| 코어/케이스 | **Mu-metal can**, 5×5 lamination stack, copper shield | A |
| Max level (Sowter 9530 클론) | +23 dBu @ 50 Hz | B |

### C.3 코어 재질 및 Bsat 추정

| 트랜스포머 | 코어 재질 | Bsat 추정 | 신뢰도 |
|----------|----------|----------|------|
| Peerless S-217-D | **Nickel-Permalloy / Mu-metal** | ≈ 0.75 T | D |
| Triad HS-52 | Mu-metal 케이스 + Ni-Fe 라미네이션 | ≈ 0.7–0.8 T | D |
| Sowter 9530 (현대 클론) | Mu-metal + M6 grain-oriented 혼합 | M6 ≈ 2.0 T | B |

### C.4 Jiles-Atherton 파라미터 (문헌 기반 추정, 실측 피팅 없음)

| 트랜스포머 | Ms (kA/m) | a (A/m) | α | k (A/m) | c | 신뢰도 |
|-----------|-----------|---------|---|---------|---|------|
| Peerless S-217-D (Ni-Fe) | 400 | 25 | 1e-5 | 20 | 0.15 | D (유추) |
| Triad HS-52 (mu-metal) | 500 | 30 | 2e-5 | 30 | 0.12 | D |
| Sowter 9530 (M6/mu 혼합) | 1500 | 80 | 5e-5 | 60 | 0.10 | B (M6 문헌) |

**실제 Pultec 트랜스포머의 JA 피팅은 공개된 적 없음.** MVP 개발 초기에는 위 값으로 시작, 나중에 실물 B-H 루프 측정으로 보정.

### C.5 현대 대체 트랜스포머 (클론/DIY용)

| 제조사 | 모델 | 턴비 | 코어 | BW (-3dB) | 특성 |
|---|---|---|---|---|---|
| **Sowter** | 9530/9530e (출력) | 5CT:1 + 25:1 NFB | mumetal+M6 | 10 Hz–120 kHz | **Pulse Tech 2025 현재 사용** |
| **Sowter** | 3603 (입력) | — | — | — | HS-52 대체 |
| **CineMag** | CM-S217D | 15k:600 + NFB | Ni-Fe | ~20 Hz–30 kHz | S-217-D 직접 클론 |
| **Jensen** | JT-11P-1 (입력) / JT-11-DMCF (인터) | 1:1 | 80% Ni | 0.7 Hz–100 kHz | DR 우수, saturation ↓ |
| **Lundahl** | LL1538/LL1582 | 가변 | amorphous/Ni | 10 Hz–80 kHz | DIY 보편 |

**주요 차이:** 현대품은 BW·DR 우수, saturation/저주파 compression 하모닉 덜함 → **"원본 Pultec 톤 재현 부족"** 지적 (Gearspace 중평). 이것이 본 MVP가 **정확한 JA 히스테리시스 모델**로 재현해야 할 타깃.

### C.6 빈티지 에이징 영향

- **Lp 드리프트**: 수십 년 후 **+10–20%** (재료 어닐링)
- **DCR 변화**: 구리권선 산화 +5% 이내
- **주요 고장 모드**:
  1. 층간 바니시 크랙 → shorted turns → Lp 급감·saturation ↑
  2. Mu-metal shock sensitivity (충격 후 μ 50% 감소, 디가우스 필요)
  3. Tertiary NFB winding 단선 → 하모닉 급증 (수리 빈발)

---

## D. 12AX7 / 12AU7 Koren 파라미터 아카이브

### D.1 12AX7 (ECC83) — 소스별 파라미터

| Source | μ | Ex | Kg | Kp | Kvb | 신뢰도 |
|--------|---|----|----|----|-----|------|
| **Koren 1996 원 논문** | 100 | 1.4 | 1060 | 600 | 300 | A+ |
| Duncan Munro SPICE lib | 100 | 1.4 | 1060 | 600 | 300 | A |
| AyumiLab B (tiny-terror) | 100 | 1.4 | 1060 | 600 | 300 | A |
| LTspice `Koren_Tubes.INC` | 100 | 1.4 | 1060 | 600 | 300 | A |
| Effectrode BSPICE | 100 | 1.4 | 1060 | 600 | 300 | A |
| RCA NOS 데이터시트 (static) | 100 | — | — | — | — | A |

**결론:** Koren 5-파라미터는 모든 소스에서 **동일 값으로 수렴**. 파라미터 논쟁 없음. 편차는 gm/rp의 제조 tolerance 수준.

### D.2 12AU7 (ECC82) — 소스별 파라미터

| Source | μ | Ex | Kg | Kp | Kvb | 신뢰도 |
|--------|---|----|----|----|-----|------|
| Koren / 데이터시트 (A 계열) | 17 | 1.3 | 1180 | 300 | 300 | A |
| **LTspice `Koren_Tubes.INC` (B 계열)** | **20.21** | **1.230** | **1108.7** | **84.96** | **551.3** | A (피팅) |
| chanmix51 gist (generic) | 17.09 | 1.4 | — | — | — | B |

**중요:** 데이터시트 공칭 μ=17과 피팅 μ=20.2 간 ~19% 차이. Pultec 입력단 12AU7의 동작 영역(Vp ≈ 140V, 중간 영역)에는 **LTspice B 계열이 더 정확**. → **MVP 기본값: B 계열 채택.**

### D.3 데이터시트 Static 값 및 편차

| 파라미터 | Typical | Range | 빈티지 σ | 현대 σ |
|---|---|---|---|---|
| 12AX7 gm | 1.6 mA/V | 1.2–2.0 | ~8% | ~15% |
| 12AX7 μ | 100 | 90–110 | ~5% | ~10% |
| 12AX7 rp | 62.5 kΩ | 45–80 | ~10% | ~15% |
| 12AU7 gm | 2.2 mA/V | 1.8–2.6 | ~7% | ~12% |
| 12AU7 μ | 17 | 15–19 | ~5% | ~10% |
| 12AU7 rp | 7.7 kΩ | 6.5–11 | ~8% | ~12% |

**출처:** Watford Valves 2012 test report + ECC83/82 데이터시트 + 커뮤니티 측정. 공식 논문 통계 부재 → 06번 Monte Carlo 문서의 σ "추정치" 표기 유지.

### D.4 MVP 권장 파라미터 (Final)

```cpp
// 12AX7 (출력단, makeup amp)
constexpr KorenParams TUBE_12AX7 = {
    .mu  = 100.0,  .Ex  = 1.4,
    .Kg  = 1060.0, .Kp  = 600.0, .Kvb = 300.0
};

// 12AU7 (입력단, driver) — LTspice B 계열
constexpr KorenParams TUBE_12AU7 = {
    .mu  = 20.21,  .Ex  = 1.230,
    .Kg  = 1108.7, .Kp  = 84.96, .Kvb = 551.3
};
```

### D.5 Pultec 원기 진공관 선정

- **원 Pultec (1951~)**: GE/RCA/Sylvania 12AX7·12AU7. 후기 일부 Amperex/Mullard
- **현대 recommended**: JJ ECC83S / JJ ECC82, EH 12AX7, Tung-Sol 12AU7
- SPICE 수준에서 브랜드별 차이는 Monte Carlo σ 내에서 표현 가능

---

## E. 경쟁 Pultec 플러그인 벤치마크

### E.1 플러그인 비교 매트릭스

| 플러그인 | 가격 | FR 정확도 | 하모닉 | 시변? | 개체차? | CPU | 본 MVP 대비 |
|---------|------|----------|--------|------|--------|-----|-----------|
| **UAD Pultec EQP-1A Passive** | ~$149 | 실물에 가장 근접 (저역) | H2/H3 레벨 의존, +7dB서 과도 | ❌ | ❌ | 중간 (UADx native) | **최소 기준 벤치마크** |
| **UAD EQP-1A Legacy** | 번들 | 필터형 (정적) | **왜곡 없음** | ❌ | ❌ | 낮음 | 차별화 여지 큼 |
| **Waves PuigTec EQP-1A** | $29.99 | 중역 "박스형", 고역 혼탁 | 천천히 증가, +0.9dB 자체 부스트 | ❌ | ❌ | 매우 낮음 | 정확도 부족 |
| **IK Multimedia EQP-1A** | €79.99 | 양호, M/S 추가 | DC 성분 적음 (Waves보다 정제) | ❌ | ❌ | 낮음 | 차별화 여지 |
| **Softube Tube-Tech PE-1C** | ~$249 | Pultec 유사 다른 HW | **홀수 하모닉만** (Pultec H2 없음) | ❌ | ❌ | 중간 | 정체성 다름 |
| **Lindell PEX-500** (PA) | $99 | "실물과 매우 근접" | Analog 스위치: 노이즈+험+트랜스 포화 | **❌ (스위치만)** | ❌ | 낮음 | 스위치는 시변 아님 |
| **Pulsar Massive** | €149 | Manley Massive Passive 모델 (Pultec 아님) | 트랜스+튜브+인덕터, 프로그램 의존 저역 | **🟡 부분** | ❌ | 중간~높음 | 유일한 경쟁 |
| **Acustica Purple 3.5/4** | €199 | 샘플링, 포착 우수 | 샘플링 한계 | ❌ (사전 샘플) | ❌ | **매우 높음** | CPU 부담 |

### E.2 본 MVP의 유일한 차별점 (8개 중 전원 없음)

1. **캐소드 바운스** — 전원 없음
2. **열적 드리프트** — 전원 없음
3. **PSU Sag** — 전원 없음
4. **Monte Carlo 런타임 개체차** — 전원 없음 (UA는 "편차 범위 내" 정적 주장만)
5. **정확한 Jiles-Atherton 트랜스포머** — Softube 홀수만, Waves 단순 파형, 나머지는 단순 필터

### E.3 최소 기준 벤치마크

- **UAD Pultec Passive**가 실물 대비 가장 정확 (Gearshoot hardware null test)
- 본 MVP의 최소 기준:
  - **주파수 응답 정확도**: UAD 수준 (±0.3~0.5 dB)
  - **하모닉 프로파일**: H2 주도 + 입력 레벨 의존 증가 재현
  - **추가 차별화**: 시변 + 개체차로 UAD 못 이루는 영역 점유

### E.4 공개 Null Test 결과

- **Gearshoot**: 실물 EQP-1A3(1971) vs UAD/Waves — UAD가 하모닉 구조에서 실물에 가장 근접, Waves는 다른 프로파일. **수치화된 null 깊이(dB) 미공개**
- **SOS**: 100Hz +5dB boost 후 FFT — Waves 서서히 증가, UAD −10dB서 저왜곡+7dB서 급증, UAD Legacy는 "왜곡 없음"
- **Julian Krause** null test: 공개 검색 미확인

### E.5 시장 기회 검증

8개 경쟁 제품 모두 **정적 LTI 필터 + 정적 waveshaper** 조합.
**→ 본 MVP의 "시변 + 개체차" 차별화는 시장 공백을 정확히 겨냥하며, 기술적으로 방어 가능하다.**

---

## F. 측정 아카이브 공백 요약 & 자체 측정 우선순위

### F.1 공개 데이터로 확인된 것 (A/A+ 등급)

- Pulse Techniques 공식 스펙 (THD, noise, impedance)
- EQ 파라미터 범위 (boost/atten max, 주파수 선택)
- "Pultec Trick" 원리
- 진공관 구성 (12AX7 + 12AU7 + 6X4)
- 트랜스포머 모델 (Triad HS-52/A-67J + Peerless S-217-D)
- 12AX7 Koren 파라미터 (모든 소스 일치)

### F.2 공개 데이터 부재 → 실물 측정 필수 항목

| 항목 | 우선순위 | 이유 |
|------|--------|------|
| 주파수별 개별 하모닉(H2~H7) dBc | **P1** | 본 MVP의 핵심 정체성 |
| 레벨별 하모닉 발달 패턴 | **P1** | 드라이브 노브 동작 검증 |
| SMPTE / CCIF IMD 스펙트럼 | **P1** | 09번 측정 프로토콜 적용 |
| 클리핑 knee 형태 (soft vs hard) | **P1** | Koren 파라미터 피팅 |
| 12AX7/12AU7 각 단 분리 측정 | **P2** | 각 단 기여 분석 |
| 트랜스포머 B-H 루프 실측 | **P2** | JA 파라미터 피팅 |
| Phase response + Group delay | **P2** | 13번 linear-phase 검증 |
| 냉시작 30분 워밍업 시간 응답 | **P3** | 시변 모델 검증 |
| 유닛 5대 이상 통계 분포 | **P3** | Monte Carlo σ 확정 |

### F.3 실물 측정 대안 — 고품질 클론 활용

실물 Pultec EQP-1A 확보가 불가한 경우 다음을 사용:
1. **Pulse Techniques re-issue** (Pulse Tech 공식, $3,500) — 원설계와 가장 근접
2. **Warm Audio EQP-WA** ($800) — 부품 수준 클론, CineMag 트랜스포머
3. **Pulse Techniques EQP-1A3** ($3,500) — 현대 re-issue

예산 제약 시 **EQP-WA**로 시작 → Phase 2에서 정품 측정 추가.

### F.4 측정 자동화 Python 스크립트 (18번 보완)

[18 측정 인프라](./18-measurement-infrastructure.md) 문서의 Python 스크립트가 다음 측정을 자동화:
- Farina ESS (주파수 응답 + 하모닉 IR 동시 분리)
- THD+N 스위프 (주파수 × 레벨 2D 매트릭스)
- SMPTE / CCIF IMD
- Phase / Group Delay

---

## G. 문서 간 크로스 레퍼런스

- 주파수 응답 측정 방법 → [09 §4 Phase Response](./09-measurement-and-validation.md)
- 하모닉 위상 측정 → [09 §5 Harmonic Phase](./09-measurement-and-validation.md)
- Farina ESS 구현 → [09 §6](./09-measurement-and-validation.md)
- Koren 파라미터 수식 → [01 §3 Koren 모델](./01-vacuum-tube-physics.md)
- Jiles-Atherton 구현 → [02 §2 JA 모델](./02-transformer-physics-and-distortion.md)
- Monte Carlo σ → [06 §2 컴포넌트 허용오차](./06-stochastic-component-modeling.md)
- 측정 장비 BOM → [18 §2 BOM](./18-measurement-infrastructure.md)

---

## H. 출처 URL 목록 (전체)

### 공식 제조사
- Pulse Techniques EQP-1A: https://pulsetechniques.com/products/tube-equalizers/eqp-1a/
- Triad Magnetics HS-52: https://www.triadmagnetics.com/item/audio-transformers/hermetically-sealed-low-level-audio-transformers/hs-52
- Triad A-67J: https://www.mouser.com/datasheet/3/236/1/A-67J-Datasheet.pdf
- Sowter 9530: https://www.sowter.co.uk/specs/9530.php
- Jensen JT-11P-1: https://www.jensen-transformers.com/wp-content/uploads/2014/08/jt-11p-1.pdf
- Peerless 1960s Catalog: https://funkwerkes.com/web/wp-content/techdocs/MixedProAudio/Peerless-1960s-Transformer-Catalog.pdf
- Warm Audio EQP-WA 매뉴얼: https://www.manualslib.com/manual/970078/Warm-Audio-Eqp-Wa.html

### 리뷰 및 측정
- Sound on Sound Pulse Techniques EQP-1A: https://www.soundonsound.com/reviews/pulse-techniques-eqp-1a
- Sound on Sound "Hidden Dangers Vintage EQ" (플러그인 비교): https://www.soundonsound.com/sound-advice/hidden-dangers-vintage-eq
- Gearshoot EQP-1A3 vs UAD/Warm/Waves: https://gearshoot.com/reviews/pultec-review-pultec-vs-warm-vs-uad-vs-waves/
- ITB Blog Waves vs UAD: https://itbblog.com/comparing-waves-puigtec-eqp-1a-and-uad-pultec-eqp-1a-which-one-reigns-supreme/
- Sweetwater Pultec Trick: https://www.sweetwater.com/insync/boost-and-attenuate-on-pultec-eqs/
- Production Expert Top 5 Pultec: https://www.production-expert.com/production-expert-1/the-5-best-pultec-plugins-in-2022

### 회로도 및 DIY 측정
- GroupDIY Peerless S-217-D 권선 측정: https://groupdiy.com/threads/peerless-s-217-d-diy.51046/
- GroupDIY G-Pultec THD: https://groupdiy.com/threads/g-pultec-thd-distortion-figures.25503/
- Wikipedia Pultec EQP-1: https://en.wikipedia.org/wiki/Pultec_EQP-1
- High on Technology AudioScape 리뷰: http://www.highontechnology.tech/2025/05/review-audioscape-eqp-vintage-style.html

### SPICE 모델
- Norman Koren Part 1: https://www.normankoren.com/Audio/Tubemodspice_article.html
- Norman Koren Part 2: https://www.normankoren.com/Audio/Tubemodspice_article_2.html
- LTspice `Koren_Tubes.INC`: https://github.com/IHorvalds/tiny-terror-simulation/blob/master/Koren_Tubes.INC
- Effectrode BSPICE Models: https://www.effectrode.com/knowledge-base/the-accurate-bspice-tube-models/
- Ayumi Lab: http://ayumi.cava.jp/audio/pctube/node47.html
- chanmix51 SPICE gist: https://gist.github.com/chanmix51/6947361

### 데이터시트
- ECC83 (pocnet): https://frank.pocnet.net/sheets/084/e/ECC83.pdf
- ECC82 (pocnet): https://frank.pocnet.net/sheets/084/e/ECC82.pdf
- Watford Valves ECC83 Test: https://www.watfordvalves.com/cgi-bin/documents/report_ECC83_12AX7.pdf
- JJ ECC83S: https://www.jj-electronic.com/en/ecc83s-12ax7-7025

### 경쟁 플러그인
- UAD Pultec Passive: https://www.uaudio.com/products/pultec-passive-eq-collection
- Waves PuigTec 매뉴얼: https://www.manualslib.com/manual/188617/Waves-Puigtec-Eqp-1a.html
- Plugin Alliance Lindell PEX-500: https://www.plugin-alliance.com/en/products/lindell_pex-500.html
- Softube Tube-Tech PE-1C: https://www.softube.com/tube-tech-pe-1c
- Acustica Purple: https://www.acustica-audio.com/store/products/purple
- IK Multimedia EQP-1A: https://www.ikmultimedia.com/products/trvintubprogeq/

---

## I. 문서 버전

- **v1.0.0** (2026-04-18): 웹 리서치 기반 초기 아카이브. 5개 클러스터 병렬 수집. 공개 데이터 공백 영역 식별.

**업데이트 예정:**
- **v2.0.0**: Phase 0–1 실물 측정 후 정량 데이터 추가 (Warm Audio EQP-WA 최소, Pulse Tech re-issue 권장)
- **v3.0.0**: 5대 이상 유닛 통계 확보 후 Monte Carlo σ 확정

---

*작성 방법: 5개 병렬 웹 리서치 에이전트 × 30분 × 각 영역 특화*
*마지막 업데이트: 2026-04-18*
