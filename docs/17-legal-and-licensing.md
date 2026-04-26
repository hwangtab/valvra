# 17. 법적 및 라이선스 이슈 (Legal & Licensing)

> **연관 문서:** [11. 타깃 하드웨어 카탈로그](./11-target-hardware-catalog.md) · [19. 리스크 분석](./19-risk-analysis.md)

---

## 17.1 면책 사항 (Disclaimer) — IMPORTANT

본 문서는 **법률 자문(legal advice)이 아니며**, 단지 오디오 플러그인 프로젝트를 기획·개발·배포하는 과정에서 개발자가 사전에 인지해야 할 법적·라이선스 쟁점을 정리한 참고 자료다. 실제 프로젝트 진행 시 다음 원칙을 반드시 따를 것:

- **지적재산권(IP) 전문 변호사의 공식 검토를 거친 뒤에만 상용 배포를 진행한다.**
- 국제 배포 시 **국가별 법률 차이**를 반드시 고려한다. 미국·EU·한국·일본의 상표법, 저작권법, 개인정보법은 상당히 다르며 일부 조항은 상충한다.
- 본 문서의 모든 조언은 **2026년 4월 기준**이며, 판례·법령·라이선스 조건은 수시로 변경된다.
- **무료 상담(pro bono)이나 커뮤니티 의견**에 의존해 중요한 의사결정을 하지 말 것. KVR, GearSpace, Reddit의 의견은 참고용일 뿐 법적 보호 수단이 되지 않는다.

> **핵심 원칙:** "안 걸렸으니까 괜찮다"는 관점은 일관되게 틀렸다. 상용 플러그인 시장은 좁고, 상표·라이선스 감시자들은 조용히 지켜보다가 매출이 발생한 시점에 한 번에 공격한다. 초기 설계부터 방어적(defensive)으로 구성해야 한다.

---

## 17.2 Trademark (상표권) 이슈

### 17.2.1 보호받는 주요 상표

오디오 업계에서 제품 이름·브랜드 이름으로 흔히 언급되는 것 중 **현재 보호되는 상표**는 다음과 같다. 아래 목록은 완전하지 않으며, 배포 전 **USPTO (미국), KIPRIS (한국), EUIPO (유럽)** 검색이 필수다.

| 브랜드 | 현 소유자 | 상표 상태 | 비고 |
|-------|----------|----------|------|
| **Neve®** | AMS Neve Ltd. | 활성 | 콘솔·프리앰프 전체 |
| **Pultec®** | Pulse Techniques LLC | 활성 | "Pultec EQP-1A" 등 포함 |
| **UREI® / Urei®** | Universal Audio | 활성 | "1176" 숫자 자체는 상표 아님 |
| **Teletronix™** | Universal Audio | 활성 | LA-2A 포함 |
| **Fairchild®** | 현재 Fairchild Semiconductor 아님 — 오디오 관련 상표 확인 필요 | 불분명 | 역사적 Fairchild Recording Equipment Corp. 상표 상태 복잡 |
| **API®** | Automated Processes Inc. | 활성 | "API 2500", "API 550" 등 |
| **SSL®** | Solid State Logic | 활성 | 강력한 보호 |
| **Manley®** | Manley Laboratories | 활성 | |
| **Chandler Limited®** | Chandler Limited | 활성 | |
| **Trident®** | Trident Audio Developments | 활성 | |
| **Helios®** | Helios Electronics Ltd. | 활성 | |
| **Studer®** | Studer / Harman | 활성 | 테이프 레코더 |
| **Ampex®** | Ampex Data Systems (현재는 데이터 기록 중심) | 활성 | 오디오 분야 사용 시 주의 |
| **Marshall®** | Marshall Amplification plc | 활성 (매우 강력) | 기타 앰프 |
| **Fender®** | Fender Musical Instruments | 활성 (매우 강력) | |
| **Vox™** | Korg / VOX Amplification | 활성 | "AC30" 자체는 기술적 서술 논쟁 여지 |
| **Mesa/Boogie®** | Gibson | 활성 | Kemper 소송 관련 이력 |

### 17.2.2 Nominative Fair Use — 이름 언급만은 일반적으로 허용

미국과 EU 상표법은 "**nominative fair use**" 법리를 인정한다. 이는 **상표가 지칭하는 실제 대상을 묘사하기 위해 최소한으로 사용**하는 것을 말한다.

**허용되는 예:**
- "이 플러그인은 Neve 1073 스타일의 색감(coloration)을 제공합니다."
- "Inspired by classic British console preamps."
- "동작 원리는 Pultec EQP-1A의 passive EQ 구조를 참조합니다."

**위험한 예:**
- 제품명에 직접 사용: "TubeAmp **Neve** Edition" → 거의 확실히 침해
- 로고 유사 디자인: Neve 특유의 red/black 배색과 font 모방
- 메타태그·광고 키워드 stuffing: "Neve plugin", "Neve emulation" 집중 SEO → 회색지대
- 사용자 혼동(consumer confusion) 야기: UI가 실제 Neve 콘솔과 구별 불가

### 17.2.3 업계 사례 — 실제 해결 방식

| 회사 | 전략 | 결과 |
|------|------|------|
| **UAD (Universal Audio)** | 대부분 브랜드와 **공식 라이선스** 계약 | 매출의 일정 % 로열티 추정 |
| **Waves** | API, SSL 등과 라이선스 | SSL G/E 채널 플러그인 정식 |
| **Plugin Alliance** | Neve, bx, Lindell 등과 라이선스 | 고가의 수익 배분 |
| **Slate Digital** | VMR에서 "Neve"→"British N", "SSL"→"British E/G" 등 우회 | 저렴·대체 |
| **Soundtoys** | 자체 브랜드 중심, 하드웨어 이름 회피 | "Decapitator", "Devil-Loc" 독자 네이밍 |
| **Acustica Audio** | 일부 공식 협력 + 일부는 "Cream", "Lime", "Pink", "Amber" 등 **색상 코드명** 사용 | 모호한 회색지대 |
| **Black Rooster Audio** | "VLA-2A", "VLA-3A" 등 **숫자·기술 용어**만 사용 | 정식 브랜드 이름 회피 |
| **IK Multimedia** | T-RackS에서 "White 2A" (Teletronix LA-2A), "Black 76" (1176) 등 색상 네이밍 | 법적 안전 |

### 17.2.4 권장 전략 — 본 프로젝트

- **Option A (Low Risk):** 제품 마케팅에서 브랜드 이름 **완전 회피**. "Classic British Preamp Color", "American Discrete Class-A Tone", "Vintage Opto Compressor" 등 **기능적 서술**만 사용.
- **Option B (Medium Risk):** "Inspired by...", "Tribute to...", "In the tradition of..." 등 간접 문구. 제품명에는 절대 브랜드 이름을 포함하지 않음.
- **Option C (High Risk, High Cost):** 명시적 **상표 라이선스 협상**. 6–24개월의 법무·협상 비용, 매출 로열티(통상 10–25%), 품질 심사 조건 등.

> **본 프로젝트 권장:** **Option A로 시작**한다. 시장 진입(launch) 및 최소 2년간의 안정적 매출이 확보된 뒤 Option B로 확대하고, Option C는 반드시 **판매 실적과 협상 레버리지**가 있을 때만 접근한다. 초기 스타트업이 Option C를 시도하는 것은 거의 항상 시간·자금의 낭비다.

### 17.2.5 프로젝트 이름 자체의 상표 검색

**TubeAmp**이라는 이름은 범용어에 가까워 상표 등록 자체는 어려울 수 있으나, 기존 등록자와의 충돌 가능성은 반드시 검색해야 한다.

```
USPTO TESS search:
  - "TubeAmp"
  - "Tube Amp"
  - "TubeAmps"
  - 발음 유사어 (Toob-Amp, Tube Amp Pro 등)

KIPRIS:
  - 한글 "튜브앰프"
  - "TubeAmp" 영문

EUIPO eSearch plus:
  - 유럽 등록 여부
```

회색지대라면 **초기 내부명(codename)**을 유지하고, 공식 런칭 직전에 최종 상표 검색·등록 비용을 지출하는 것이 합리적이다.

---

## 17.3 회로 Reverse Engineering 법적 쟁점

### 17.3.1 허용되는 것 (Generally Safe)

- **하드웨어 입출력 측정** — 스위핑 톤(sweep tone), 임펄스 응답(IR), 스텝 응답, THD, IMD 측정
- **공개 회로도(schematic) 참조** — 서비스 매뉴얼, 공개된 학술 논문, Jakob Vogel 등의 리버스 엔지니어링 공개 자료
- **물리적 동작 원리 학습 및 자체 구현** — Koren 모델, Spice 시뮬레이션, Jiles-Atherton 이론은 모두 공공 영역(public domain)의 과학 지식이다.
- **파라미터 피팅(parameter fitting)** — 측정 데이터로부터 모델 파라미터를 수치 최적화로 도출하는 것

### 17.3.2 위험한 것 (Legally Risky)

- **특허받은 특정 회로 구조 구현** — 대부분의 빈티지 회로 특허는 만료되었으나(20년), 최근 30년 내의 일부 특허는 여전히 유효. 예: 특정 variable-mu 변종 회로, digital modeling 관련 특허(Line 6, Fractal 등 일부).
- **Trade secret 노하우 유출** — 해당 회사의 퇴사 엔지니어에게서 **문서화되지 않은** 기술을 습득했다면 영업비밀 침해 가능성.
- **ROM/firmware 덤프** — 디지털 장비(Eventide H3000, Lexicon 224 등)의 경우 firmware 저작권 침해가 된다. 절대 금지.
- **회로도 자체의 저작권** — 학술 논문에 그려진 schematic 이미지를 그대로 복사하는 것은 저작권 침해. 기술 자체는 자유.

### 17.3.3 실제 사례

- **Kemper vs. Fender/Mesa Boogie (2016–2018):** 앰프 프로파일링(profiling) 기술과 특정 앰프 사운드 배포를 둘러싼 분쟁. 합의 종결. Kemper는 "Rigs" 배포 시 사용자 책임 조항 강화.
- **Neural DSP:** 현존 제품의 명시적 에뮬레이션(Nolly, Plini, Abasi, Soldano SLO-100 등)을 **브랜드와 공식 라이선스 계약** 하에 진행. Fortin Nameless Suite도 동일.
- **Line 6:** 초기 POD은 "Inspired by" 명시. 이후 자체 HX 기술로 전환해 법적 회피.
- **Atomic AmpliFIRE, Headrush:** "Americana 2x12", "British Blues" 등 기능적 네이밍으로 우회.
- **IK Multimedia AmpliTube:** 초기에는 이름 회피, 이후 Fender·Mesa·Orange 등과 **공식 라이선스** 확보 (버전 3 이후).

### 17.3.4 안전한 접근 — 본 프로젝트 원칙

1. **물리 기반 모델링을 우선**하며, Koren(진공관 모델링, 1996, 공개), Jiles-Atherton(자성체 이력, 1984–, 공개), Wright-Kelly(트랜스포머) 등 학술 공개 모델만 사용.
2. 측정 데이터는 **내부 피팅 참조용**이며, 배포 제품에는 raw 측정 데이터를 포함하지 않는다.
3. 마케팅에서 "**똑같이 들린다(identical to)**"는 표현은 절대 사용하지 않는다. "reminiscent of", "inspired by" 선에서 멈춘다.
4. 특정 제품 브랜드 이름과 1:1 매칭 UI (예: Neve 1073 외관 모방)를 피한다.

---

## 17.4 Hardware Sampling (하드웨어 샘플링) 이슈

### 17.4.1 샘플링 기반 플러그인의 법적 쟁점

**Acustica Audio**로 대표되는 sampling 방식(convolution + nonlinear sampling kernel)은 특히 쟁점이 많다:

- 실제 하드웨어의 **출력(output) 자체를 녹음**하는 것은, 녹음물에 해당 하드웨어의 "performance"가 저작물로 포함되는가 논쟁.
- 하드웨어 소유자의 **사용 허가(permission)** 필요 여부에 대해 국가별 판례 미성숙.
- 빌려온 하드웨어로 측정·샘플링한 경우 소유자와의 계약 필수.
- 샘플된 데이터의 **재배포·재판매 제약** — 타인 제품에 내장해 판매 시 저작권 주장 가능.

### 17.4.2 본 프로젝트 스탠스

- **Pure physical modeling** 접근 — 어떤 형태의 샘플링도 사용하지 않는다.
- 측정 데이터(sweep, IR 등)는 **파라미터 피팅 전용**이며, 피팅 완료 후 데이터는 제품 바이너리에 포함되지 않는다.
- 개발 과정에서 수집한 모든 raw 측정 데이터는 **내부 리포지토리에만 보관**하고, 외부 배포·오픈소스 공개를 하지 않는다.
- 향후 커뮤니티 기여(impulse response 공유 등)를 받는 경우, 기여자로부터 **명시적 라이선스 서명** 필수.

---

## 17.5 오픈소스 라이선스 호환성 — CRITICAL

### 17.5.1 주요 의존성 라이선스 분석

| 라이브러리 | 라이선스 | 상용 배포 호환? | 비고 |
|----------|---------|----------------|------|
| **JUCE** | **AGPL-3 / Commercial (dual)** | Commercial 구매 **필수** | JUCE 8부터 GPL-3 → **AGPL-3**으로 전환 (2024) |
| **chowdsp_utils** | **모듈별 상이 — BSD 또는 GPL-3** | BSD 모듈만 사용 시 가능 | 모노리식 GPL 아님. 모듈별 LICENSE 개별 확인 필수 |
| **RTNeural** | BSD-3-Clause | ✓ | 자유 사용 (10 문서와 일치) |
| **Eigen** | MPL-2.0 | ✓ (조건부) | 수정 시 해당 파일만 소스 공개 |
| **xsimd** | BSD-3-Clause | ✓ | 매우 관대 |
| **Dear ImGui** | MIT | ✓ | GUI 서브시스템으로 활용 가능 |
| **FFTW** | GPL-2 or Commercial | GPL 또는 MIT 구매 | **KISS FFT, PFFFT가 대체재** |
| **libsamplerate** | BSD-2-Clause (v0.1.9+) | ✓ | 이전 GPL 버전 주의 |
| **fmt** | MIT | ✓ | |
| **nlohmann/json** | MIT | ✓ | |
| **Catch2** | BSL-1.0 | ✓ (테스트 전용) | 배포 바이너리 비포함 |

### 17.5.2 JUCE 라이선스 옵션 상세 (2026년 4월 기준 — juce.com/get-juce 및 juce.com/legal/juce-8-licence/ 2차 검증 완료)

JUCE는 Raw Material Software(현재 Sound Ship 계열)가 dual license로 제공한다. **JUCE 8부터 오픈소스 trunk는 GPL-3에서 AGPL-3으로 변경**되었다 (공식 EULA: https://juce.com/legal/juce-8-licence/). 이는 네트워크 서비스 형태 배포 시에도 copyleft가 적용됨을 의미한다.

| 티어 | 가격 (2026-04 공식 EULA 확인) | 연 매출·펀딩 한도 | 적용 |
|------|--------------------|------|------|
| **JUCE Starter** | 무료 | **Up to $20,000** | 완전 개인 개발/취미 단계 |
| **JUCE Indie** | **$40/월/사용자** 또는 **$800 영구 (1회)** | **Up to $300,000**, 월 최소 1개월 | **초기 런칭 단계 권장** |
| **JUCE Pro** | **$175/월/사용자** 또는 **$3,500 영구 (1회)** | **매출 한도 없음(No limit)** | 성공 시 이전 |
| **JUCE Educational** | 무료 | 교육기관·학생 한정(상용 금지) | 해당 시 |
| **AGPL-3** | 무료 | 결과물도 AGPL-3 배포 필수 (네트워크 배포 포함) | **상용 제한적** |

> **2차 검증(2026-04):** Starter $20K, Indie $300K, Pro 무제한, Educational(상용 불가) — JUCE 8 EULA Section 1.2 기준. 월간 구독($40 / $175)과 영구 라이선스($800 / $3,500) 병행 체계이며, **연간 구독은 제공되지 않음**. 영구 ↔ 구독 간 전환 불가. 계약 전 반드시 [juce.com/get-juce](https://juce.com/get-juce/) 및 [JUCE 8 EULA](https://juce.com/legal/juce-8-licence/) 직접 확인.
>
> **JUCE 7 보유자**: JUCE 8 영구 라이선스 업그레이드 시 할인 적용(상세는 공식 안내 참조).

**AGPL-3 조건 핵심 (GPL-3과 차이 포함):**
- 배포 시 **소스 전체 공개**
- 사용자에게 **AGPL-3 또는 호환 라이선스**로 재배포 권리 부여
- 정적 링크(static link)한 모든 코드가 AGPL-3 적용 받음
- **네트워크 서비스로 제공 시에도 소스 공개 의무** (GPL-3과 결정적 차이)
- **Copyleft 전염**: 자체 작성 DSP 코드까지 AGPL-3으로 강제 공개해야 함

즉, 상용(closed-source) 배포 또는 SaaS 배포 의도라면 **JUCE Commercial (Starter/Indie/Pro) 구매 외 선택지 없음**.

### 17.5.3 chowdsp_utils 해결 전략 — 모듈별 라이선스 혼재 주의

Jatin Chowdhury의 `chowdsp_utils`는 WDF, nonlinear solver, DSP utilities를 풍부하게 포함한다. **2차 검증 (2026-04):** 이 라이브러리는 **모노리식 GPL-3이 아니라 모듈별로 라이선스가 상이**하다. 공식 README에도 "Each module in this repository has its own unique license. If you would like to use code from one of the modules, please check the license of that particular module."라고 명시돼 있다. 모듈 구분은 다음과 같다 (README + 각 모듈 디렉토리 LICENSE 파일 2차 재확인):

| 계열 | 모듈 (BSD) | 모듈 (GPL-3) |
|-----|-----------|--------------|
| **Common / Infra** | `chowdsp_core`, `chowdsp_data_structures`, `chowdsp_json`, `chowdsp_listeners`, `chowdsp_logging`, `chowdsp_reflection`, `chowdsp_serialization`, `chowdsp_buffers`, `chowdsp_math`, `chowdsp_simd`, `chowdsp_rhythm`, `chowdsp_version`, `chowdsp_fuzzy_search`, `chowdsp_clap_extensions` | — |
| **Parameters / State / Preset** | `chowdsp_parameters`, `chowdsp_plugin_state`, `chowdsp_presets`, `chowdsp_presets_v2` | — |
| **DSP 핵심** | — | `chowdsp_filters`, `chowdsp_compressor`, `chowdsp_dsp_utils`, `chowdsp_dsp_data_structures`, `chowdsp_eq`, `chowdsp_modal_dsp`, `chowdsp_reverb`, `chowdsp_sources`, `chowdsp_waveshapers` |
| **GUI / Plugin base** | — | `chowdsp_gui`, `chowdsp_visualizers`, `chowdsp_foleys`, `chowdsp_plugin_base`, `chowdsp_plugin_utils` |
| **기타 (examples / tests / benchmarks)** | — | GPL-3 (비모듈 코드 포함) |

따라서 **Common/Infra/State 계열은 상용 링크 가능(BSD)**이나, WDF·비선형 솔버·컴프레서·리버브·EQ·필터·shaper 등 **본 프로젝트가 실제로 필요한 DSP 모듈은 전부 GPL-3**이다. 결과적으로 **DSP 핵심 기능 직접 사용은 불가에 가깝다**. Chowdhury 측은 "proprietary developers interested in GPL modules should contact the authors for non-GPL licensing options"라고 공지하고 있어 **협상의 여지는 열려 있다**.

출처: `github.com/Chowdhury-DSP/chowdsp_utils` README + 각 모듈 디렉토리 내 `LICENSE` 파일 (2026-04 재확인).

**해결 옵션:**

| 옵션 | 설명 | 비용 | 위험 | 권장도 |
|-----|------|------|------|-------|
| **A. GPL 배포 수용** | 플러그인 전체를 GPL-3으로 공개, 수익은 기부·Patreon·유료 지원으로 | 저 | 비즈니스 모델 제한 | 일반 상용에 부적합 |
| **B. Clean-room 재구현** | 필요한 WDF, JA 등을 자체 구현 | 중 (엔지니어 시간) | 저 | **권장** |
| **C. Commercial licensing 협상** | Jatin Chowdhury와 직접 협상 (공식 제안 창구 열림) | 불확실 (가격 비공개, 협상 필요) | 중 | **가능성 있음** |
| **D. BSD 모듈만 선택적 사용** | `chowdsp_core` 등 BSD 모듈만 링크, GPL 모듈은 피함 | 저 | 저 | 보조적으로 유효 |
| **E. 대체 오픈소스** | 다른 MIT/BSD 라이선스 WDF 라이브러리 탐색 | 중 | 기능 부족 | 보조적 |

**Option B 구체 전략:**

1. `chowdsp_utils`의 **구조와 인터페이스만 참고**하고, 실제 구현은 학술 논문(Kurt Werner PhD thesis, Olafur Bogason, David Yeh 등)을 근거로 **처음부터 작성**한다.
2. **Clean-room 원칙**을 엄격히 지킨다:
   - Engineer A가 chowdsp 코드를 읽고 **사양(spec)만 문서화**
   - Engineer B는 chowdsp 코드를 **절대 보지 않고** 사양만 보고 구현
   - 모든 단계를 git history·리뷰로 기록
3. 결과물은 **MIT 또는 Apache-2.0**으로 라이선스 (본 프로젝트에선 내부 private)
4. JA 히스테리시스 구현도 동일 접근: Jiles-Atherton 원 논문 + Chowdhury 2019 JAES 논문 참조, chowdsp 코드 미참조.

> **본 프로젝트 권장:** **Option B**를 채택. 핵심 WDF 엔진, Jiles-Atherton 솔버, Koren 진공관 모델은 모두 자체 개발한다. 이는 라이선스 자유도뿐 아니라 **성능 튜닝·디버깅 자유도**도 확보한다.

### 17.5.4 권장 의존성 스택 (호환성 확인 완료)

```
상용 배포 전제:
  JUCE Pro/Indie (라이선스 구매)
  ├── 자체 WDF 라이브러리 (clean-room)
  ├── 자체 JA/Koren 구현
  ├── RTNeural (BSD-3-Clause) — 옵션, neural hybrid 사용 시
  ├── Eigen (MPL-2) — 선형대수, 헤더만 사용
  ├── xsimd (BSD-3) — SIMD 추상화
  ├── Dear ImGui (MIT) — 디버그 UI 또는 서브시스템
  ├── fmt (MIT) — 포맷팅
  ├── nlohmann/json (MIT) — 프리셋·설정
  └── PFFFT (BSD-like) — FFT (FFTW 회피)

테스트 (배포 미포함):
  └── Catch2 (BSL-1.0) 또는 doctest (MIT)
```

---

## 17.6 플러그인 포맷 별 법적 제약

### 17.6.1 VST3 (Steinberg)

- **라이선스 (2026-04 재확인):** GPL-3 + proprietary dual license. 상용 closed-source 배포 시 Steinberg 개발자 등록(무료, 온라인 양식 서명) 후 Proprietary 라이선스 자동 적용. GPL-3 경로 선택 시 플러그인 소스도 GPL-3 배포 의무.
- **상용 사용:** Steinberg 3rd Party Developer 등록 필요 (무료, 서명만)
- **배포 제약:** Steinberg 심사 없음, 자유 배포
- **상표 사용:** "VST" 로고 사용 시 Steinberg 가이드라인 준수
- **주의:** VST2는 더 이상 신규 라이선스 발급 안 됨 (2018년 종료)

### 17.6.2 AU (Audio Unit, Apple)

- **라이선스:** Apple SDK — Xcode + Apple Developer 계정 필요
- **Notarization:** macOS 10.15+ 필수 (Apple 공증 — `notarytool` CLI)
- **Signing:** Developer ID 인증서 — **Apple Developer Program 연 $99 USD** (Individual/Organization 공통, 2026-04 기준 변동 없음). Enterprise는 $299/년으로 별도(일반 배포에는 불필요).
- **App Store 배포:** sandboxing 제약, 별도 심사, 일부 DSP 기능 제한
- **주의:** 일반 배포(DMG, installer)는 App Store 대비 자유로우나 notarization은 필수

### 17.6.3 AAX (Avid Pro Tools)

- **라이선스:** NDA 서명 필수
- **개발자 등록:** Avid Developer Program 가입
- **심사:** Avid QA 및 승인 과정 있음 (수 주–수 개월)
- **PACE 서명:** 모든 AAX 바이너리는 PACE iLok에 의해 서명 필수
- **AAX DSP:** HDX 하드웨어용 별도 제약, 매우 엄격
- **주의:** NDA로 인해 기술 세부사항 공개 제한

### 17.6.4 CLAP (CLever Audio Plug-in)

- **라이선스:** **MIT** (공식 repo `github.com/free-audio/clap` LICENSE 2026-04 재확인)
- **배포:** 완전 자유
- **생태계:** 2022년 발표, 2024–2026년 급속 확산 (Bitwig, Reaper, FL Studio, Studio One 6.5+ 지원)
- **주의:** 아직 Pro Tools, Logic 미지원 → AU/VST3 병행 필수

### 17.6.5 권장 배포 조합

| 단계 | 포맷 | 이유 |
|------|------|------|
| **Alpha/Beta** | VST3 + CLAP | 가장 단순, 빠른 이터레이션 |
| **공식 런칭 v1** | VST3 + AU + CLAP | macOS/Windows 주요 DAW 커버 |
| **v1.5+** | +AAX | Pro Tools 사용자 확보 시 |

---

## 17.7 국가·지역별 고려사항

### 17.7.1 United States

- **DMCA (Digital Millennium Copyright Act):**
  - §1201 안티회피(anti-circumvention) 조항 준수. DRM 크랙 도구 배포 금지.
  - Safe harbor 조항(§512)으로 사용자 포럼 운영 가능.
- **제품 책임:** 소프트웨어는 전기 제품 안전 규정 미적용. 단, **warranty disclaimer** EULA에 명시.
- **Tax:** Sales tax nexus — 주별로 다름. Shopify, Paddle, FastSpring 등 MoR(merchant of record) 서비스가 자동 처리.
- **GDPR 영향:** 미국 거주자라도 EU 고객 대상 서비스 시 GDPR 적용.

### 17.7.2 European Union

- **GDPR (General Data Protection Regulation):**
  - 사용자 데이터 수집 시 명시적 동의, 데이터 목적·보관 기간 명시.
  - EU 내 사용자 대상 서비스 시 위치 무관 적용.
  - 위반 시 매출 4% 또는 €20M 중 큰 금액.
- **VAT / MOSS (One-Stop-Shop):**
  - 디지털 상품은 구매자 거주 국가 VAT 부과 (B2C).
  - EU 외 판매자도 OSS 등록 가능.
  - Paddle, FastSpring 등 MoR 사용 시 자동 처리.
- **CE 마킹:** 소프트웨어 플러그인에는 일반적으로 불필요.
- **Cookie Law (ePrivacy):** 웹사이트 운영 시 쿠키 배너 필수.
- **Geoblocking Regulation:** EU 내 국가별 차별 금지.

### 17.7.3 한국

- **전자상거래법:** 직접 판매 시 **통신판매업 신고**(지자체) 필요. 온라인 쇼핑몰 형태면 필수.
- **개인정보보호법 (PIPA):**
  - GDPR과 유사하나 일부 더 엄격 (개인정보 국외이전 제약).
  - 수집 시 동의, 처리방침 공개, 개인정보보호책임자 지정.
- **소프트웨어 저작권:** 저작권법 보호, 등록은 선택 사항.
- **표시광고법:** 허위·과장 광고 금지 — "세계 최고의 tube emulation" 같은 표현 주의.
- **부가세(VAT):** 연매출 일정 기준 초과 시 등록. 해외 사업자도 간이과세 제도 고려.

### 17.7.4 일본 / 기타 아시아

- **일본:** 특정상거래법(特定商取引法) 준수, 이용약관 일본어 번역 권장.
- **중국:** 소프트웨어 판매 규제 복잡. China 진출 시 별도 법무 검토.
- **기타:** 라이선스 ASCII/영문 조항의 법적 유효성 국가별 상이. 번역본 제공이 안전.

---

## 17.8 개인정보 처리 (Privacy)

### 17.8.1 플러그인이 수집할 수 있는 데이터

| 데이터 유형 | 목적 | 법적 근거 | 권장 |
|-----------|------|----------|------|
| 라이선스 활성화 정보 | DRM | 계약 이행 | 최소 수집 |
| 하드웨어 ID (machine fingerprint) | 활성화 한계 | 정당한 이익 | 해시 처리 |
| 이메일 주소 | 활성화·업데이트 고지 | 동의 | 옵트아웃 제공 |
| 버전 정보 (자동 업데이트) | 기능 제공 | 계약 이행 | 투명하게 고지 |
| 사용 통계 (텔레메트리) | 제품 개선 | **동의 필수** | **기본 비활성(opt-in)** |
| 크래시 리포트 | 품질 개선 | 동의 | opt-in, 익명화 |

### 17.8.2 필수 정책 문서

1. **Privacy Policy** — 웹사이트와 플러그인 설치 중 명시. GDPR Art. 13·14 요구사항 충족.
2. **Cookie Policy** (웹사이트 운영 시)
3. **DPA (Data Processing Agreement)** — B2B 고객이 요구할 수 있음.
4. **Data Retention Policy** — 해지·환불 후 몇 년간 보관하는지 명시.

> **실용 조언:** 초기에는 **텔레메트리·크래시 리포트를 아예 구현하지 않는 것**이 가장 간단한 GDPR 준수책이다. 수집하지 않으면 위반할 것도 없다. 제품 안정화 후 필요 시 opt-in 기반으로 도입.

---

## 17.9 이용 약관 (EULA) 핵심 조항

### 17.9.1 필수 포함 조항

1. **License Grant** — 사용자에게 허용되는 범위
   - Personal vs Commercial 구분
   - 동시 설치 기기 수 (통상 2–3대)
   - 양도 가능 여부
2. **Restrictions**
   - 리버스 엔지니어링, 수정, 재배포 금지
   - 임대·공유 금지
   - DRM 우회 금지
3. **Ownership** — 저작권·IP 모든 권리 개발사 귀속 명시
4. **Warranty Disclaimer** — "AS IS" 제공, 묵시적 보증 배제
5. **Limitation of Liability** — 손해 배상 한계 (통상 구매 금액)
6. **Termination** — 위반 시 라이선스 자동 해지
7. **Governing Law & Jurisdiction** — 분쟁 관할 법원 지정 (통상 개발사 소재지)
8. **Updates & Support** — 업데이트 의무 범위
9. **Third-Party Notices** — 오픈소스 의존성 고지 (MIT, BSD, MPL 등 의무 사항)

### 17.9.2 템플릿 및 참고 자료

- **KVR Audio Developer Forum** — 커뮤니티 공유 샘플 EULA (참고용)
- **Plugin Boutique / Pluginboutique.com** — 표준 유통사 EULA 구조
- **Audio Software Law Specialists:**
  - Ari Goldberg (뉴욕, 오디오 기술 특화)
  - 한국: 지재권 전문 로펌 (법무법인 태평양, 광장 등 IT 전문팀)
- **절대 피할 것:** 생성형 AI로 EULA 초안 생성 후 변호사 검토 없이 사용. 관할·준거법 조항은 특히 커스텀 필요.

### 17.9.3 권장 프로세스

1. 유사 플러그인 회사 EULA 2–3개 수집 (공개 자료)
2. 구조 참조해 1차 초안 작성
3. 한국 지재권 변호사 1차 검토 ($500–1500)
4. 주요 시장(미국) 변호사 2차 검토 ($1000–3000)
5. 중요 업데이트 시 재검토

---

## 17.10 해적판(Piracy) 대응

### 17.10.1 DRM 전략 비교

| 방식 | 대표 | 사용자 편의성 | 크랙 저항 | 비용 | 권장도 |
|------|------|-------------|---------|------|-------|
| **iLok (PACE)** | UAD, Waves, Slate | 낮음 (동글) | 높음 | 통합 비용 + per-seat | 대형 스튜디오 대상 |
| **PACE** | AAX 등 | 중 | 높음 | 라이선스 비용 | 대형사 |
| **자체 DRM** | 자체 구현 | 중 | 중 (공격 대상) | 개발 시간 | 소규모 |
| **Machine Fingerprint** | 다수 | 높음 | 낮음 | 낮음 | 초기 단계 |
| **Honor System (No DRM)** | Valhalla DSP, TAL, u-he (일부) | 매우 높음 | 매우 낮음 | 거의 없음 | 지지자 기반 확보 시 |

### 17.10.2 업계 흐름 (2020–2026)

- **iLok 의존도 감소** — 사용자 불만 누적, 특히 인디 스튜디오
- **Honor system 성공 사례 증가** — Valhalla, Soundtoys 일부, u-he 기본
- **온라인 활성화 + 소프트 한도** — 동시 3대, 온라인 인증 30일 주기 (TokyoDawn, SIR 등)

### 17.10.3 본 프로젝트 권장

- **v1.0 초기:** 소프트 DRM (라이선스 키 + 1회 온라인 활성화 + 오프라인 fallback)
- **크랙 발견 시 대응:**
  - 추격·고발은 비용 대비 효과 낮음
  - **대신 제품 가치·업데이트·커뮤니티에 집중**
  - Valhalla 식 철학: "합당한 가격이면 정당 구매자가 절대다수"
- **iLok 도입은 B2B 대형 고객이 요구할 때만** 검토

> **현실적 인식:** 모든 DRM은 결국 깨진다. DRM에 투입하는 엔지니어링 시간보다 **제품 개선에 투입하는 시간**이 장기 수익에 훨씬 긍정적이다. 과도한 DRM은 정당 사용자에게 부담을 주어 오히려 역효과다.

---

## 17.11 특허 이슈

### 17.11.1 일반 오디오 DSP 특허 현황

- 대부분의 **고전 아날로그 회로 특허는 만료** (특허 존속기간 20년, 1950–1980년대 회로 대부분 만료).
- 측정·계측 관련 특허(Audio Precision, ITU, EBU 등)는 대부분 공공 영역 또는 표준.
- **최근 30년 내 특허는 주의:**
  - Neural network 기반 모델링 (2015–2026): 신규 특허 급증
  - ML 기반 음원 분리(demucs, spleeter): 특허 논쟁 진행 중
  - Real-time convolution 최적화 일부 알고리즘

### 17.11.2 본 프로젝트에서 구체 조사 필요 영역

| 영역 | 특허 리스크 | 권장 조치 |
|------|-----------|----------|
| Variable-mu 압축 (1940s Fairchild 원리) | 매우 낮음 (만료) | 자유 사용 |
| Ultra-Linear 출력 (Hafler/Keroes 1951) | 만료 | 자유 사용 |
| Koren triode/pentode model (1996) | 학술 공개 | 자유 사용 |
| Jiles-Atherton (1984) | 학술 공개 | 자유 사용 |
| Wave Digital Filter (Fettweis 1986) | 학술 공개 | 자유 사용 |
| Neural Amp Modeler 아키텍처 | **조사 필요** | Steven Atkinson 공개 이후 가능성 검토 |
| Kemper Profiling | 독점 특허 보유 | 근본 원리 회피 (본 프로젝트 비해당) |
| Fractal Audio AxeFX 기술 | 일부 특허 | 구체 클론 회피 |

### 17.11.3 특허 검색 프로세스

1. **Google Patents** — 무료, 키워드 검색
2. **USPTO PATFT** — 공식 미국 특허 DB
3. **EPO Espacenet** — 유럽 특허
4. **KIPRIS** — 한국 특허
5. 핵심 기술 구현 전 **특허성 검토(freedom to operate) 변호사 자문** ($2000–5000)

### 17.11.4 본 프로젝트 리스크 평가

**전반적 리스크 낮음** — 이유:

- 공개된 학술 모델(Koren, JA, WDF) 기반
- 상용 특허 영역(neural profiling, Kemper 방식 등) 회피
- 물리 기반 모델링은 1차 원리(first principles)에 가까움

다만 neural hybrid 기능을 추가할 경우 **RTNeural 구조 사용이 적절한지, NAM 특허 상태가 어떻게 변했는지** 재검토 필요.

---

## 17.12 실제 조치 계획 (Actionable Checklist)

### 17.12.1 프로젝트 진행 전 (Pre-Development)

- [ ] 지재권 변호사 1차 자문 — 예산 $500–2000 (한국) 또는 $1500–5000 (미국)
- [ ] JUCE 라이선스 결정: Pro/Indie 구매 vs GPL-3 수용
- [ ] chowdsp_utils 사용 여부 결정 (**권장: Clean-room 재구현**)
- [ ] 프로젝트 이름 상표 검색 — USPTO, KIPRIS, EUIPO
- [ ] 도메인 확보 (.com, .io, .co.kr)
- [ ] 회사/개인사업자 설립 (책임 분리)

### 17.12.2 개발 중 (Development Phase)

- [ ] 모든 오픈소스 의존성의 라이선스를 `LICENSES/` 디렉토리에 원문 보관
- [ ] 각 의존성의 NOTICE 파일 작성 (배포 바이너리와 함께 포함)
- [ ] 코드 리뷰에 "라이선스 호환성" 체크 항목 추가
- [ ] 외부 기여(PR) 받을 시 CLA(Contributor License Agreement) 요구 검토

### 17.12.3 베타 전 (Pre-Beta)

- [ ] EULA 초안 작성 + 변호사 검토
- [ ] Privacy Policy 작성 (웹사이트 + 플러그인 동의 화면)
- [ ] 베타 테스터 NDA 준비 (선택)
- [ ] DMCA takedown 창구 마련 (정식 agent 등록은 선택)

### 17.12.4 출시 전 (Pre-Launch)

- [ ] 국가별 배포 규정 확인 (미국, EU, 한국 최소)
- [ ] MoR 서비스 선정 (Paddle, FastSpring, Lemon Squeezy 등) — 세금 자동 처리
- [ ] iLok/자체 DRM 인프라 구축 및 테스트
- [ ] Notarization (macOS) 및 Code Signing (Windows) 인프라
- [ ] 유료 고객 데이터 보관 정책 수립
- [ ] 환불 정책(Refund Policy) 문서화

### 17.12.5 출시 후 (Post-Launch)

- [ ] 매 버전 릴리즈 시 라이선스 고지 갱신
- [ ] 분기 1회 상표권자 동향 모니터링
- [ ] 연 1회 EULA·Privacy Policy 재검토
- [ ] 법령 변경 (GDPR 업데이트, 한국 PIPA 개정 등) 추적

---

## 17.13 위기 대응 계획 (Incident Response)

### 17.13.1 상표권자로부터 Cease & Desist 수신 시

**행동 강령:**

1. **즉각 마케팅 수정 (24시간 내)** — 문제 표현을 임시 제거. 이는 "고의성 없음(no willful infringement)" 입증에 도움.
2. **법률 자문 (48시간 내)** — 지재권 변호사에게 원본 C&D 송부. 독자 응답 금지.
3. **협상 또는 수정 배포 (2주 내):**
   - 표현만 문제인 경우: 사과·수정으로 종결
   - 제품명 문제: 리브랜딩 계획 수립
   - 심각한 침해 주장: 라이선스 협상 또는 소송 방어 준비
4. **문서화** — 모든 통신·변경 이력 기록

### 17.13.2 GPL 위반 발각 시

GPL-3 라이선스 위반은 **Software Freedom Conservancy**와 같은 단체가 실제 추적·제소한다. 방치 시 전체 소스 강제 공개까지 갈 수 있다.

1. **즉각 소스 공개** 또는 **위반 의존성 제거** 중 선택
2. 대체재로 **clean-room 재구현** — 제거된 기능 복구
3. 기존 배포 바이너리 **리콜 검토** (라이선스 조건에 따라)
4. 위반 원인 분석 — CI 파이프라인에 라이선스 스캔 도구(FOSSA, ScanCode, Black Duck) 도입

### 17.13.3 특허 침해 소송 시

특허 소송은 극히 비용이 크다 (미국 평균 방어 비용 $3M+).

1. **전문 변호사 즉각 선임** — 특허 소송 경험자
2. **방어 전략:**
   - Prior art 제시 (해당 기술의 기존 사례)
   - Non-infringement 주장 (claim construction으로 청구범위 밖 주장)
   - 무효 심판(IPR, inter partes review) 청구
3. **보험:** Patent infringement insurance 사전 가입 검토 — 연 $5–20K, 소송 시 수십만 달러 커버

### 17.13.4 데이터 유출(GDPR) 시

1. 72시간 내 관할 감독기구에 통지 (GDPR Art. 33)
2. 영향받은 사용자에게 고지 (위험 고려)
3. 원인 분석 및 재발 방지 조치
4. 기록 보관 및 변호사 협의

---

## 17.14 참고 자료

### 17.14.1 도서·논문

- Robert P. Merges, *The Laws of Intellectual Property* (Aspen Publishing)
- Lawrence Lessig, *Free Culture* (자유 라이선싱 철학)
- Ashley Packard, *Digital Media Law* (미국 미디어법 개관)

### 17.14.2 온라인 자료

- **KVR Audio Developer Forum** — 법률·라이선스 논의 아카이브 (참고용, 법적 근거 아님)
- **GPL-3 원문**: https://www.gnu.org/licenses/gpl-3.0.html
- **JUCE 공식 라이선스**: https://juce.com/get-juce/
- **Steinberg 3rd Party Developer**: https://www.steinberg.net/en/company/developers.html
- **Apple Developer Program**: https://developer.apple.com/programs/
- **Avid Developer Program**: https://www.avid.com/alliance-partner-program

### 17.14.3 검색 도구

- **상표 검색:**
  - USPTO TESS: https://tmsearch.uspto.gov/
  - KIPRIS: http://www.kipris.or.kr/
  - EUIPO eSearch plus: https://euipo.europa.eu/eSearch/
  - WIPO Global Brand Database: https://www3.wipo.int/branddb/
- **특허 검색:**
  - Google Patents: https://patents.google.com/
  - USPTO PATFT
  - EPO Espacenet
- **라이선스 분석:**
  - SPDX License List: https://spdx.org/licenses/
  - TLDRLegal: https://www.tldrlegal.com/
  - ChooseALicense: https://choosealicense.com/

### 17.14.4 법무 자문 찾기

- **한국:** 한국지식재산보호원 무료 상담, 대한변호사협회 IP 특별위원회
- **미국:** American Bar Association IP Section, 음악·오디오 기술 전문 변호사 (Ari Goldberg 등)
- **EU:** European Patent Attorneys (epi-informa list)
- **커뮤니티:** KVR Developer, Audio Programmer Discord에서 변호사 추천 문의 가능 (법률 자문 자체는 받지 말 것)

---

## 17.15 요약 및 본 프로젝트 권장 결정사항

| 쟁점 | 본 프로젝트 결정 | 근거 |
|------|---------------|------|
| 상표 전략 | **Option A (회피)** — "Classic British", "American Tube" 등 기능적 네이밍 | 초기 리스크 최소화 |
| 회로 RE | **공개 자료·측정 기반, 샘플링 없음** | 특허·영업비밀 회피 |
| Hardware sampling | **사용하지 않음** | pure physical modeling |
| JUCE 라이선스 | **Indie 구매 → 성공 시 Pro** | 상용 배포 필수 |
| chowdsp_utils | **Clean-room 재구현** | GPL-3 감염 회피 |
| DRM | **소프트 DRM (키 + 온라인 활성화)** | 사용자 편의 vs 보호 균형 |
| 텔레메트리 | **v1.0 미도입** | GDPR 단순화 |
| EULA | **변호사 검토 완료 후 공개** | 리스크 관리 |
| 배포 국가 | **v1.0: 미국·EU·한국·일본** | 주요 시장, 규정 파악 완료 지역 |
| MoR | **Paddle/FastSpring/Lemon Squeezy 중 선택** | VAT·Sales Tax 자동 |
| 특허 방어 | **공개 학술 모델 중심, neural 영역만 모니터링** | 리스크 낮음 |

> **최종 원칙:** 법적 리스크는 **돈이 벌리기 시작하면 등장**한다. 매출 0일 때는 아무도 신경 쓰지 않지만, 월 $10K를 넘는 순간 경쟁사·상표권자·라이선스 감시자가 조용히 확인한다. 따라서 **출시 전 준비(Pre-Launch checklist)를 완료**한 뒤 런칭하는 것이 장기적으로 가장 저렴하다. 사후 수습 비용은 사전 준비 비용의 10–100배다.

---

> **연관 문서 재안내:**
> - [11. Target Hardware Catalog](./11-target-hardware-catalog.md) — 어떤 하드웨어를 참조할지와 해당 브랜드의 상표 상태가 연결됨
> - [19. Business & Distribution](./19-business-and-distribution.md) *(예정)* — 가격, 유통 채널, 결제 인프라 (MoR 등) 상세
