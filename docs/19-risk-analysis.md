# 19. 프로젝트 리스크 분석 (Project Risk Analysis)

> **문서 목적:** 본 프로젝트("살아있는 아날로그" 진공관/트랜스포머 에뮬레이션 플러그인)가 기획 → 개발 → 출시 → 성장 각 단계에서 직면할 수 있는 **기술·시장·법적·운영·외부 리스크**를 식별하고, 확률(Probability)과 영향(Impact)에 기반해 우선순위를 매기며, 각 리스크에 대한 **Mitigation(완화)** 및 **Contingency(대체 계획)** 을 정의한다.
>
> **냉정한 현실 인식:** 이 문서는 프로젝트에 대한 마케팅 낙관론이 아니라 **사실에 입각한 위험 관리 도구**다. 리스크를 직시해야 실제 실행 단계에서 허를 찔리지 않는다.

**관련 문서:**
- `08-competitive-analysis.md` — 경쟁 환경, UAD/Waves/Acustica 포지셔닝
- `11-target-hardware-catalog.md` — 측정 대상 하드웨어, 개체차 모델링 난이도
- `17-legal-and-compliance.md` — 상표·라이선스·특허 상세 전략
- `18-business-strategy.md` — 가격·GTM·매출 목표

---

## 19.1 서론 (Introduction)

### 19.1.1 리스크 관리의 필요성

오디오 플러그인 개발, 특히 **본 프로젝트처럼 깊은 물리 모델링(WDF+JA+Monte Carlo)을 상업화**하는 경우 일반 소프트웨어 대비 리스크가 다중 축으로 쌓인다.

1. **기술 난이도:** 학계·제조사에서도 실시간으로 완성하지 못한 비선형 상태 방정식을 상용 품질로 수렴시켜야 함.
2. **시장 포화:** 진공관/트랜스포머 플러그인 시장은 UAD·Waves·Universal Audio·Acustica·Plugin Alliance 등 25년 이상 이어진 기업이 지배.
3. **법적 지뢰:** "Neve", "Pultec", "1176" 등 상표·트레이드드레스 이슈.
4. **단독 개발의 취약성:** DSP·GUI·QA·마케팅·세무를 한 사람이 맡으면 병목이 곧 프로젝트 생존으로 직결됨.

리스크 관리 없이 "훌륭한 기술을 만들면 팔릴 것"이라 가정하는 것은 **오디오 플러그인 시장의 실패 통계와 정면으로 충돌**한다. 대부분의 1인 플러그인 프로젝트는 기술 완성 후 마케팅·유통 실패로 빛을 보지 못한다.

**업계 규모 참고 (2024–2026 시장 조사):**
- **전체 오디오 플러그인 시장 규모:** 2026년 ~$360M → 2035년 ~$740M (7.2% CAGR, Business Research Insights 추정). 본 프로젝트 대상(analog emulation 세그먼트)은 전체의 30–40% 추산.
- **주요 경쟁사 점유:** Waves Audio ~20% (스튜디오 사용 기준), Universal Audio ~11%, Plugin Alliance·iZotope·FabFilter·Soundtoys·Acustica·Arturia가 나머지 상위권 분할.
- **Kickstarter 오디오 플러그인 사례:** 음악 카테고리는 Kickstarter 2024 보고서에서 "최소 노출" 수준 — 오디오 소프트웨어 단일 카테고리는 성공 사례가 많지 않다(예외: Lightpad Block류 하드웨어-소프트웨어 복합). 순수 플러그인 스타트업 Kickstarter 성공 사례는 제한적이며, 본 프로젝트는 **Kickstarter보다 pre-sale·landing page 전환 모델**이 현실적.
- **1인 플러그인 스타트업 실패 패턴 (관측 기반, 공식 통계 없음):** 기술 완성 이후 유통 채널 미확보·마케팅 부재로 18–24개월 내 사실상 개점휴업 상태로 진입하는 사례가 Gearspace/KVR 관찰상 다수.

### 19.1.2 이 문서의 사용법

- **프로젝트 kickoff 시:** Section 19.9의 Risk Register 전체를 읽고 P1/P2 리스크에 대한 이해 공유.
- **주간 단위:** 새로 발견된 리스크 추가, P1 항목 진행 상황 업데이트.
- **월간 단위:** Risk Register 전체 재평가, Probability/Impact 변경 여부 확인.
- **분기 단위:** Section 19.10 Action Plan 실행 결과 리뷰, 필요 시 mitigation 수정.
- **반기 단위:** Pivot 시나리오(§19.12) 검토, 전체 전략 재조정 여부 결정.

**문서는 살아있는(living) 문서다.** 한 번 쓰고 서랍에 넣는 순간 가치는 0이 된다.

---

## 19.2 리스크 분류 프레임워크 (Risk Taxonomy)

본 프로젝트의 리스크는 다음 다섯 카테고리로 분류한다.

| 카테고리 | 설명 | 예시 |
|---------|------|------|
| **기술적 리스크 (Technical)** | 구현 난이도, 수치 수렴 실패, 성능 목표 미달, 수치 안정성, 플랫폼 호환성 | WDF+JA Newton-Raphson 발산, CPU 오버헤드 |
| **시장 리스크 (Market)** | 시장 수요 검증 부족, 경쟁사 대응, 가격 수용도, 판매 채널 | UAD 유사 제품 출시, 잠재 고객의 지불 의사 부족 |
| **법적 리스크 (Legal)** | 상표, 라이선스(GPL 감염), 특허, 개인정보·GDPR | "Neve" 상표 cease & desist, chowdsp GPL 의존 |
| **운영 리스크 (Operational)** | 팀(1인), 일정, 예산, 베타 테스터 확보 | 단독 개발자 burnout, 예산 초과 |
| **외부 리스크 (External)** | 플랫폼 정책 (Apple, Microsoft, Avid), 경제·환율 | AAX 승인 지연, Apple notarization 정책 변경 |

각 리스크에는 **고유 식별자**를 부여한다:
- `R-T#` (Technical)
- `R-M#` (Market)
- `R-L#` (Legal)
- `R-O#` (Operational)
- `R-E#` (External)

이를 통해 이슈 트래커, 회의록, 코드 주석에서 리스크를 명확히 참조할 수 있다.

---

## 19.3 Risk Matrix (위험 평가 매트릭스)

### 19.3.1 평가 기준

각 리스크를 **Probability × Impact** 로 평가한다.

**Probability (발생 확률):**
| 등급 | 정의 | 대략적 수치 |
|------|------|------------|
| Low | 예외적 상황에서만 발생 | < 20% |
| Medium | 적절한 관리 없이는 발생 가능 | 20–60% |
| High | 관리해도 발생 가능성 높음 | > 60% |

**Impact (영향도):**
| 등급 | 정의 | 예시 |
|------|------|------|
| Low | 일정 1–2주 지연, 부분 기능 포기 | 특정 DAW 후속 지원 |
| Medium | 일정 1–3개월 지연, 주요 기능 축소 | 베타 테스터 재모집 |
| High | 핵심 기능 재설계, 분기 이상 지연 | DSP 엔진 교체 |
| Critical | 프로젝트 중단·피벗 또는 사업 실패 | 상표권 소송, 법률상 상용 불가 |

### 19.3.2 우선순위 매트릭스

|             | Impact: Low | Impact: Medium | Impact: High | Impact: Critical |
|-------------|-------------|----------------|--------------|------------------|
| **Prob: High**   | P4          | P2             | P2           | **P1**           |
| **Prob: Medium** | P4          | P3             | P2           | **P1**           |
| **Prob: Low**    | P5          | P4             | P3           | P2               |

- **P1:** 즉시 대응. 매주 리뷰. mitigation을 **선제적**으로 집행.
- **P2:** 적극 대응. 월간 리뷰. mitigation을 플랜에 명시 반영.
- **P3:** 상시 모니터링. 분기별 리뷰.
- **P4:** 인지. 필요 시 대응.
- **P5:** 기록만 유지.

---

## 19.4 기술적 리스크 (Technical Risks)

### R-T1: WDF + JA + Newton-Raphson 수렴 실패

- **Probability:** Medium
- **Impact:** High
- **Priority:** **P2**
- **설명:** Wave Digital Filter(WDF)에 Jiles-Atherton(JA) 자성 비선형과 진공관 비선형을 동시에 내장하면, Newton-Raphson(NR) 반복이 **강한 비선형 영역**(overdrive, 트랜스포머 포화 인접)에서 발산하거나 수렴이 느려질 수 있다. 실시간 제약(버퍼당 수 μs)으로 반복 횟수에 상한을 두면 수렴 전 종료 → 부정확한 해 → 오디오 아티팩트(클릭, 팝) 또는 크래시 위험.
- **징후 (Early Warning Signals):**
  - 유닛 테스트에서 high-gain 신호에 대해 NR 반복 10회 초과 비율 상승
  - 플러그인 CPU 스파이크 (특정 샘플에서만)
  - 스펙트럼 이상(존재하지 않아야 할 고주파 피크)
  - 사용자 리포트: "특정 소스에서 크래시"
- **Mitigation:**
  - **Robust fallback:** NR 반복이 tolerance 내에 수렴 못 하면 이전 샘플 해를 재사용(hold last good state) + 로그 기록
  - **Homotopy continuation:** 입력이 급변하는 경우 작은 step으로 분할하여 연속 풀이
  - **Pre-solved LUT:** hot path(자주 쓰이는 입력 범위)에 대해 미리 푼 해를 테이블화, NR의 초기값으로 사용
  - **Oversampling(4x–8x):** 샘플당 상태 변화를 작게 만들어 수렴성 향상
  - **Double precision:** 핵심 NR 루프는 `double`로 유지 (float32 캐스팅은 최종 출력 시만)
- **Contingency:**
  - MVP에서 WDF 대신 **Koren direct model + LUT**로 폴백. 물리 정확도는 낮아지지만 수렴 보장.
  - "Physics" 모드와 "Fast" 모드를 분리 제공.
- **참조:** `07-implementation-strategies.md §7.3 (수치 안정성), §7.5 (denormal 처리)`

---

### R-T2: CPU 성능 목표 미달

- **Probability:** Medium
- **Impact:** Medium
- **Priority:** **P3**
- **설명:** 목표: 128 샘플 버퍼, 44.1 kHz, 단일 인스턴스 CPU < 20% (M1, 2020년대 중급 CPU 기준). JA + WDF + Monte Carlo 개체차 + 시변 상태까지 얹으면 이 목표를 초과할 위험.
- **Mitigation:**
  - **Quality mode switch:** Low/Medium/High/Ultra — 사용자가 선택. Low 모드는 LUT 기반, Ultra는 full physics.
  - **SIMD 최적화:** AVX2 (x64), NEON (Apple Silicon) 활용. 병렬 샘플 처리.
  - **Multi-rate processing:** 시변 상태(thermal, bias drift)는 낮은 SR(100 Hz–1 kHz)로 업데이트. 오디오 SR는 그대로.
  - **Profile-guided optimization:** Release 빌드에 PGO 적용.
  - **Aggressive profiling:** Xcode Instruments / Intel VTune으로 hot path 분기점 매주 측정.
- **Contingency:**
  - MVP에서 Monte Carlo 개체차 샘플 수를 축소 (예: 64 → 16), High/Ultra에서만 full activation.
  - Time-varying 고급 기능은 V2로 연기.

---

### R-T3: 측정 데이터 부족 → 모델 파라미터 부정확

- **Probability:** High
- **Impact:** Medium
- **Priority:** **P2**
- **설명:** **개체차 모델링이 본 프로젝트의 핵심 차별점**인데, Neve 1073 같은 유닛을 10대 이상 측정하는 것은 현실적으로 매우 어렵다. 한 대당 수천 달러, 대여도 쉽지 않음. 제조사 NDA 하에 측정 접근이 가능하지만 협상이 필요. 결과적으로 Monte Carlo 분포가 실제 개체 분포와 다를 수 있음.
- **Mitigation:**
  - **다층 데이터 소스:** (a) 문헌 측정 데이터 + (b) 자체 1~3대 측정 + (c) 회로 시뮬레이션 기반 합리적 추론 결합
  - **커뮤니티 측정 풀링:** GitHub/Gearspace 사용자에게 THD, freq response, impedance curve 업로드 요청 (익명화).
  - **제조사 파트너십 시도:** Neve, Chandler Limited 등에 "공식 에뮬레이션" 제안 (UAD 모델). 현실적으로 이미 UAD와 계약 독점적이지만 2nd-tier 제조사는 가능성.
  - **측정 장비 확보:** 최소한 Audio Precision APx500 시리즈 또는 대체 솔루션 (Cosmos ADC + REW) 확보.
- **Contingency:**
  - 특정 유닛 에뮬레이션이 아닌, **"General Vintage Character" / "British Console" / "American Tube"** 처럼 모호한 캐릭터 프리셋으로 포지셔닝 변경. 상표·측정 정확성 양쪽 리스크 동시 완화.
- **참조:** `09-measurement-and-validation.md`, `11-target-hardware-catalog.md`

---

### R-T4: Neural 모델 학습 실패 또는 오버핏

- **Probability:** Medium
- **Impact:** Low (optional 기능으로 포지션 시)
- **Priority:** **P4**
- **설명:** Grey-box hybrid (Physics + 잔차 neural network) 접근에서 NN이 학습 데이터에만 핏되고 일반화 실패. 또는 physics와 NN의 역할 분리가 모호해져 수렴 안 됨.
- **Mitigation:**
  - **Pure physics 모델을 기본으로:** Neural은 선택적 레이어. 기본 체인은 physics-only.
  - **Diverse training data:** 합성 + 실측 + 다양한 입력 신호 (sine sweep, pink noise, 실제 음악)
  - **Physics-constrained NN:** 출력 범위·에너지 보존 제약 부여
  - **Ablation study:** NN on/off 비교로 실제 기여 검증
- **Contingency:** Neural layer 제거, physics-only로 출시. 로드맵에서 제외.

---

### R-T5: 플랫폼별 버그 (VST3 vs AU vs AAX)

- **Probability:** High
- **Impact:** Medium
- **Priority:** **P2**
- **설명:** 각 플러그인 포맷 간 미묘한 차이(파라미터 자동화 이벤트, bypass 동작, state save/load, buffer size 변화, sample rate 변화) 때문에 **한 포맷에서 동작하는 것이 다른 포맷에서 실패**하는 버그가 필연적으로 발생.
- **Mitigation:**
  - **JUCE 최신 안정 버전 사용** (현재 8.x) — 플랫폼 추상화 계층의 신뢰성
  - **주요 DAW 테스트 매트릭스:**
    - macOS: Logic Pro, Pro Tools, Ableton Live, Cubase, Studio One
    - Windows: Pro Tools, Cubase, FL Studio, Reaper, Ableton Live, Studio One
  - **CI/CD:** GitHub Actions로 자동 빌드 + 유닛 테스트 + pluginval 자동화
  - **Pluginval, AudioPluginHost** 등 표준 테스트 도구를 pipeline에 통합
- **Contingency:** 문제 포맷 임시 제외하고 후속 업데이트로 제공. 사용자에게 투명하게 공지.

---

### R-T6: 수치 정밀도 문제 (denormal, overflow, NaN 전파)

- **Probability:** Medium
- **Impact:** Medium
- **Priority:** **P3**
- **설명:** 비선형 반복 시스템에서 denormal 숫자가 누적되면 CPU 스파이크 10–100배. NaN이 한 번 발생하면 상태 변수 전반으로 전파되어 세션 재시작까지 복구 안 됨.
- **Mitigation:**
  - **Denormal flushing:** `_mm_setcsr` FTZ/DAZ 활성화 (process block 시작 시)
  - **Double precision:** 핵심 루프 `double`
  - **상태 변수 클리핑:** 물리적으로 불가능한 값은 saturate
  - **NaN guard:** block 끝에서 NaN 감지 → 상태 리셋
  - **Stress test:** 극한 입력 (full-scale impulse, DC offset) 시나리오 자동화
- **Contingency:** Crash log 수집 → hot-fix 패치
- **참조:** `07-implementation-strategies.md §7.5`

---

## 19.5 시장 리스크 (Market Risks)

### R-M1: 시장 수요 검증 부족

- **Probability:** High
- **Impact:** Critical
- **Priority:** **P1**
- **설명:** "살아있는 아날로그(개체차 + 시변)"라는 컨셉이 실제 사용자에게 **지불 의사가 있는 가치**인지 **출시 전까지 검증되지 않았다**. 많은 프로 엔지니어는 기존 Waves/UAD로 "충분히 만족"하고, 개체차 같은 미묘한 차이에 추가 비용을 지불하지 않을 가능성이 높다.
- **Mitigation:**
  - **출시 전 프로토타입 인터뷰:** 마스터링 엔지니어, 믹스 엔지니어 20–30명과 1:1 인터뷰. "이 기능에 얼마까지 지불하겠는가?"를 직접 질문.
  - **Alpha 단계 free testing:** NDA 하에 proof-of-concept 제공, 피드백 수집.
  - **커뮤니티 모니터링:** Gearspace "Plugins" 섹션, r/audioengineering, KVR Audio에서 유사 제품 반응, 경쟁 토론 정기 관찰.
  - **Landing page 사전 test:** 출시 6개월 전 landing page 공개, 이메일 가입 수치로 수요 측정.
- **Contingency:**
  - 포지셔닝 수정: 일반 플러그인 → "마스터링 전문" 또는 "특정 장르 sound design"으로 좁힘.
  - 가격 하향 ($299 → $149)
  - 최악의 경우 오픈소스화 + 서비스/컨설팅 수익 모델 피벗.
- **참조:** `18-business-strategy.md` GTM/positioning 섹션

---

### R-M2: UAD / Waves / Acustica / Plugin Alliance 빠른 대응

- **Probability:** Medium
- **Impact:** High
- **Priority:** **P2**
- **설명:** 본 플러그인의 차별점 (시변, 개체차 Monte Carlo, 물리 기반 WDF+JA)이 주목받으면, **경쟁사는 수 분기 내에 유사 기능을 추가**할 수 있다. UAD는 이미 "Unison" 기술로 하드웨어 측정에 능숙, Acustica는 "sampling convolution"으로 유사 캐릭터 제공.
- **Mitigation:**
  - **출시 타이밍 가속:** 초기 18개월 → 12개월 계획 압축. MVP 최소화.
  - **특허 확보:** Monte Carlo 기반 인스턴스 개체차 샘플링, 시변 열·바이어스 drift의 오디오 플러그인 적용 등 구체적 알고리즘을 특허 출원 검토.
  - **브랜드·마케팅 구축:** "살아있는 아날로그"라는 포지셔닝 언어를 선점. 마케팅 톤을 기술적+철학적으로 만들어 경쟁사의 단순 copy가 약해 보이게.
  - **지속 업데이트:** 분기별 개선으로 "2세대 기술" 유지. 경쟁사 follow up까지 2–3년 유지 목표.
  - **커뮤니티 lock-in:** 사용자 포럼, 프리셋 공유, 아티스트 엔도스먼트.
- **Contingency:** 틈새 시장 집중 — 마스터링 특화, 클래식 녹음 특화, 아시아 시장 특화 등 큰 회사가 놓칠 세그먼트 타겟팅.
- **참조:** `08-competitive-analysis.md`

---

### R-M3: 가격 책정 실패

- **Probability:** Medium
- **Impact:** Medium
- **Priority:** **P3**
- **설명:** $299로 출시했는데 시장 상단은 $199. 또는 반대로 $149로 출시 후 "너무 저렴해서 의심"으로 안 팔림 (오디오 프로 시장은 가격과 품질 인식의 상관이 강함).
- **Mitigation:**
  - **Tier 가격:** Basic $99 (core physics만), Standard $199 (시변 추가), Pro $299 (Monte Carlo + 측정 유닛 라이브러리 full)
  - **Early-bird 할인:** 출시 후 3개월간 50% 할인으로 초기 사용자 확보 + 리뷰 축적
  - **가격 A/B:** landing page에서 두 가격 제시 후 전환률 측정
  - **Bundle 판매:** 다른 플러그인 제조사와 cross-bundle
- **Contingency:** 출시 후 3–6개월 내 가격 조정. Black Friday, NAMM 시즌 할인.
- **참조:** `18-business-strategy.md` Pricing 섹션

---

### R-M4: 판매 채널 부족

- **Probability:** Low
- **Impact:** Medium
- **Priority:** **P4**
- **설명:** 자체 웹사이트만으로는 트래픽 확보 어려움. Plugin Boutique, Pluginfox 등 distributor 등록은 리뷰 심사가 있음.
- **Mitigation:**
  - **자체 웹사이트** + **Plugin Boutique** + **Pluginfox** + **ADSR Sounds** 등 최소 3개 채널
  - **Bundle 파트너십:** Splice, Output 등 구독 서비스 협상
  - **Distributor fee 감안:** 일반적으로 25–40% 수수료
- **Contingency:** 자체 채널 마케팅 투자 확대, 영향력 있는 유튜버/엔지니어 스폰서십

---

## 19.6 법적 리스크 (Legal Risks)

### R-L1: 상표권 cease & desist

- **Probability:** Low (Option A 전략 시) / High (Option C 전략 시)
- **Impact:** High
- **Priority:** **P2**
- **설명:** "Neve", "Pultec", "1176", "LA-2A" 등 상표를 마케팅에 직접 사용하면 상표권자(Universal Audio, AMS Neve, Warm Audio 라이선스 등)로부터 경고서(C&D) 수령 가능. 심할 경우 소송.
- **Mitigation:** **Option A 전략** (상표 사용 회피 + 기술·소리 특성으로 간접 묘사) 채택. 마케팅 문구, 프리셋 이름, 매뉴얼, 웹사이트 전수 검수. 예: "Neve 1073" ❌ → "British 1970s console preamp" ✅ (단, 트레이드드레스 소지도 주의).
- **Contingency:** C&D 수령 시 48시간 내 문제 표현 제거, 법률 자문 후 공식 답변. 소송 전환 시 외부 IP 로펌 의뢰.
- **참조:** `17-legal-and-compliance.md §17.2` 상표 전략

---

### R-L2: GPL 의존성 감염 (Copyleft Contamination)

- **Probability:** Medium (관리 소홀 시 High)
- **Impact:** Critical
- **Priority:** **P1**
- **설명:** `chowdsp_utils` 같은 GPL-3 라이브러리를 본 상용 플러그인에 링크하면 **전체 플러그인이 GPL 공개 의무 발생** → 상업 판매 사실상 불가. 실제 오디오 업계에서 반복적으로 발생한 사고.
- **Mitigation:**
  - **의존성 감사:** `cmake`·`conan`·`vcpkg` manifest + 자동 SPDX license scanner (예: `licensecheck`, `fossa`)
  - **Pre-commit hook:** 새 의존성 추가 시 라이선스 자동 체크, GPL/AGPL/LGPL(정적링크 시) 차단
  - **Clean-room 재구현:** WDF 트리, JA 솔버, Newton-Raphson 등 GPL 코드에서 아이디어만 참고하여 독자 구현. 구현자는 GPL 원본을 보지 않고 논문·공개 문헌 기반으로 작성.
  - **JUCE Pro 라이선스:** JUCE AGPL-3 모드 대신 상용 라이선스 구매. **2026-04 2차 검증** (juce.com/get-juce + juce.com/legal/juce-8-licence/): Starter $0 (연 매출 ≤$20K), Indie **$40/월 또는 $800 영구/개발자** (연 매출 ≤$300K), Pro **$175/월 또는 $3,500 영구/개발자** (**매출 한도 없음**), Educational 무료(상용 불가). 이전 문서 초안의 "연 $800–1500" 표기는 오래된 수치이므로 교체. 연간 구독 플랜은 존재하지 않으며, 영구 ↔ 구독 전환 불가.
  - **chowdsp_utils 주의 (2026-04 2차 검증):** 모노리식 GPL이 아니라 **모듈별로 BSD 또는 GPL-3 혼재**. DSP 계열(`chowdsp_filters`, `chowdsp_compressor`, `chowdsp_dsp_utils`, `chowdsp_eq`, `chowdsp_reverb`, `chowdsp_sources`, `chowdsp_waveshapers`, `chowdsp_modal_dsp`)과 GUI/plugin_base 계열(`chowdsp_gui`, `chowdsp_visualizers`, `chowdsp_foleys`, `chowdsp_plugin_base`, `chowdsp_plugin_utils`)은 **모두 GPL-3 → 상용 감염**. Common/Infra(`chowdsp_core`, `chowdsp_data_structures`, `chowdsp_json`, `chowdsp_simd`, `chowdsp_math`, `chowdsp_buffers`, `chowdsp_logging`, `chowdsp_serialization` 등)와 State/Preset(`chowdsp_parameters`, `chowdsp_plugin_state`, `chowdsp_presets_v2`)은 BSD로 링크 가능. 모듈별 LICENSE 파일 확인 필수.
  - **서드파티 리스트:** `THIRD_PARTY_LICENSES.md`를 코드와 동기화.
- **Contingency:** GPL 감염이 확인되면 해당 코드 제거 + 재구현까지 출시 지연. 이미 출시 후 발견 시 오픈소스 전환 또는 리콜.
- **참조:** `17-legal-and-compliance.md §17.3` 라이선스 전략

---

### R-L3: 특허 침해

- **Probability:** Low
- **Impact:** High
- **Priority:** **P3**
- **설명:** 오디오 DSP 관련 특허 (특히 2015년 이후 neural audio 관련)를 침해할 가능성. 예: waveform transform 특허, specific anti-aliasing 기법, DSP hybrid 특허.
- **Mitigation:**
  - **USPTO / EPO / KIPO 검색:** 관련 분야 (audio plugin, nonlinear DSP, neural audio) prior art 조사.
  - **Freedom-to-Operate (FTO) 분석:** 출시 3개월 전 IP 변호사에게 의뢰.
  - **회피 설계:** 유사 특허 발견 시 알고리즘 우회 또는 독자 기술 유도.
- **Contingency:** 침해 주장 수령 시 법률 자문 + 라이선스 협상 또는 회피 코드 업데이트.

---

## 19.7 운영 리스크 (Operational Risks)

### R-O1: 단독 개발자 burnout

- **Probability:** High
- **Impact:** Critical
- **Priority:** **P1**
- **설명:** DSP 알고리즘 + C++ 구현 + JUCE GUI + 플랫폼 포팅 + 베타 관리 + 마케팅 + 고객 지원까지 1인 수행은 **장기적으로 지속 불가능**. 6–12개월차에 번아웃으로 프로젝트 정체 확률 높음.
- **Mitigation:**
  - **일정 여유:** 공격적 12개월 일정 대신 **현실적 18개월**
  - **주간 작업 시간 상한:** 50시간 이하 유지. 주말 최소 1일 완전 휴식.
  - **외주화 가능 영역 식별:** GUI 디자인(Figma), 마케팅 카피, 랜딩 페이지, 매뉴얼 번역 등은 초기부터 외주.
  - **커뮤니티 기여:** 일부 DSP 모듈을 (비핵심 영역) 오픈소스화하여 외부 기여 유치.
  - **MVP 엄격:** "있으면 좋은" 기능은 V2로. MVP는 핵심 5–6개 유닛만.
- **Contingency:** 일시 중단 선언 (예: 2–4주), 스코프 재조정. 최악의 경우 파트너(공동 개발자/사업자) 영입.

---

### R-O2: 개발 일정 지연

- **Probability:** High (복잡 프로젝트 특성상)
- **Impact:** Medium
- **Priority:** **P2**
- **설명:** DSP 연구 프로젝트는 "이만큼 걸릴 것 같다"의 **2–3배**가 걸리는 것이 업계 정설.
- **Mitigation:**
  - **Agile + 분기 milestone**
  - **Scope 가변화:** 기능을 "Must / Should / Could / Won't" (MoSCoW)로 분류. 일정 위기 시 Could/Won't부터 제거.
  - **월간 speed 리포트:** 실제 소요 vs 예상 vs 남은 기능 비율 — velocity 감지
- **Contingency:** MVP 범위 축소, V1.1·V1.2로 분할 출시.

---

### R-O3: 베타 테스터 부족

- **Probability:** Medium
- **Impact:** Medium
- **Priority:** **P3**
- **설명:** 프로 엔지니어는 바쁘다. NDA·피드백 요청을 무시하거나 "좋던데요" 한 줄 답변만 주면 실질 QA 가치 없음.
- **Mitigation:**
  - **Alpha 단계부터 10명 이상 확보:** 이미 관계 있는 엔지니어 중심
  - **NDA + 명확한 기대치:** "주 1회 30분 테스트, 구체적 리포트 1건"
  - **인센티브:** 영구 라이선스 free, 엔드 크레딧, 홍보 영상 출연
  - **다양한 워크플로우 커버:** 마스터링, 믹싱, 레코딩, 사운드 디자인 각 1–2명
- **Contingency:** 유료 베타 QA 서비스 (예: BetaFamily, UserTesting 오디오 전문)

---

### R-O4: 예산 초과

- **Probability:** Medium
- **Impact:** High
- **Priority:** **P2**
- **설명:** 측정 장비, 법률 자문, 상표 등록, 코드 사이닝 인증서, 개발자 계정, 마케팅, 디자인 외주 등 예상 초과 쉬움.
- **Mitigation:**
  - **단계별 예산:**
    - Phase 1 (R&D, 0–6개월): $5,000 — JUCE 라이선스, 소규모 측정 장비, 툴
    - Phase 2 (개발, 6–12개월): $15,000 — 법률 자문, 디자인 외주, 측정 추가
    - Phase 3 (출시 준비, 12–18개월): $30,000 — 마케팅, distributor 초기 비용, 상표 등록, 1차 캠페인
  - **Bootstrap 원칙:** 필수가 아니면 지출 회피. 무료 대안(Reaper eval, Figma free, GitHub free tier) 적극 활용.
  - **월 단위 burn rate 추적**
- **Contingency:** Crowdfunding (Kickstarter, Indiegogo), Patreon, 초기 라이선스 pre-sale, sponsorship

---

## 19.8 외부 리스크 (External Risks)

### R-E1: Apple Notarization / Gatekeeper 정책 변경

- **Probability:** Low
- **Impact:** Medium
- **Priority:** **P4**
- **설명:** Apple이 macOS용 플러그인 서명·공증 정책을 강화하거나 Apple Silicon 전용 요구 확대.
- **Mitigation:** **Apple Developer Program** 연 **$99 USD** (Individual/Organization), Enterprise는 별도 $299/년 — 일반 배포에는 Individual/Organization $99로 충분. Developer ID 인증서 발급, WWDC 발표 모니터링, Universal Binary(arm64 + x86_64) 기본 빌드, notarytool 자동화.
- **Contingency:** macOS 지원 일시 보류 및 업데이트 패치.

---

### R-E2: Windows Code Signing EV Cert 이슈

- **Probability:** Low
- **Impact:** Low
- **Priority:** **P5**
- **설명:** EV 코드 서명 인증서 비용 (2026-04 재확인: **Sectigo EV ~$279–296/년** 다수 리셀러, 사용처에 따라 $453 수준까지; **DigiCert EV ~$525–581/년**, 리셀러별 편차 큼) 또는 SmartScreen 평판 축적 문제.
- **2026 정책 변경 (2차 검증):** CA/Browser Forum Ballot CSC-31에 따라 코드 서명 인증서 최대 유효기간이 **39개월 → 약 460일(≈15개월)** 로 단축. 적용 시점은 **2026-03-01** 발급분부터(DigiCert·Sectigo 모두 2026-02-24 이후 발급분에 적용). 기존 3년 인증서는 만료까지 유효하나 **갱신 시부터는 15개월 한도**. 일부 언론·리셀러의 "1년 제한" 표기는 부정확한 단순화이며, 실제 상한은 **459일**임에 주의. 또한 **Microsoft는 EV 코드 서명이 있어도 SmartScreen 경고를 자동 제거하지 않음**(2024-03 이후 평판 기반 완화만 적용). 즉 EV 프리미엄의 실효성이 과거보다 낮다.
- **Mitigation:** DigiCert / Sectigo EV 인증서 사용(대부분의 경우 Sectigo 충분), **Azure Trusted Signing**(클라우드 HSM, 월 $9.99 수준) 같은 서명 서비스 검토, 자동 서명 pipeline. 15개월 갱신 주기에 맞춘 캘린더 알람 필수.
- **Contingency:** Standard OV cert + 초기 SmartScreen 경고 수용 후 평판 축적 (일반적으로 수천 설치 후 해제). Azure Trusted Signing 전환 시 HSM 보관·관리 부담 경감.

---

### R-E3: Avid AAX 승인 지연

- **Probability:** Medium
- **Impact:** Medium
- **Priority:** **P3**
- **설명:** Pro Tools 지원을 위해 AAX 포맷 필요. Avid Developer Program 승인, AAX SDK 접근, Qualified 검증은 수 주 ~ 수 개월 소요. Pro Tools 생태계는 프로 마켓의 큰 부분이므로 무시 못 함.
- **Mitigation:**
  - **초기 출시는 VST3/AU만**으로 진행, AAX는 V1.1에서 추가
  - **Avid Developer Program 조기 신청**
- **Contingency:** AAX가 3–6개월 지연되어도 VST3/AU 매출로 유지.

---

### R-E4: 환율 / 경제 변동

- **Probability:** Medium
- **Impact:** Low
- **Priority:** **P5**
- **설명:** 글로벌 판매 시 USD/EUR/JPY 환율 변동, 경제 침체 시 플러그인 같은 "non-essential" 구매 감소.
- **Mitigation:** 다통화 결제 (Stripe, FastSpring), 지역별 가격 조정, 구독·렌탈 모델 고려.
- **Contingency:** 할인 프로모션, 구독 전환.

---

## 19.9 Risk Register 종합 (Priority 순)

| ID | 리스크 | Probability | Impact | Priority | Owner |
|----|-------|-------------|--------|----------|-------|
| **R-M1** | 시장 수요 검증 부족 | High | Critical | **P1** | 사업/마케팅 |
| **R-L2** | GPL 의존성 감염 | Medium | Critical | **P1** | 개발/법무 |
| **R-O1** | 단독 개발자 burnout | High | Critical | **P1** | 개인 |
| **R-T1** | WDF+JA 수렴 실패 | Medium | High | P2 | DSP |
| **R-T3** | 측정 데이터 부족 | High | Medium | P2 | DSP/측정 |
| **R-T5** | 플랫폼별 버그 | High | Medium | P2 | QA/CI |
| **R-M2** | 경쟁사 빠른 대응 | Medium | High | P2 | 마케팅/PM |
| **R-L1** | 상표권 이슈 | Medium (Option A), High (Option C) | High | P2 | 법무 |
| **R-O2** | 일정 지연 | High | Medium | P2 | PM |
| **R-O4** | 예산 초과 | Medium | High | P2 | PM/재무 |
| R-T2 | CPU 성능 미달 | Medium | Medium | P3 | DSP |
| R-T6 | 수치 정밀도 | Medium | Medium | P3 | DSP |
| R-M3 | 가격 실패 | Medium | Medium | P3 | 마케팅 |
| R-L3 | 특허 침해 | Low | High | P3 | 법무 |
| R-O3 | 베타 테스터 부족 | Medium | Medium | P3 | PM |
| R-E3 | AAX 승인 지연 | Medium | Medium | P3 | 엔지니어링/사업 |
| R-T4 | Neural 학습 실패 | Medium | Low | P4 | DSP (R&D) |
| R-M4 | 판매 채널 부족 | Low | Medium | P4 | 마케팅 |
| R-E1 | Apple 정책 변경 | Low | Medium | P4 | 엔지니어링 |
| R-E2 | Windows EV Cert | Low | Low | P5 | 엔지니어링 |
| R-E4 | 환율/경제 변동 | Medium | Low | P5 | 재무 |

> **1인 개발 전제 시 "Owner"는 모두 본인이지만, 명시적 role 라벨링은 의사결정의 맥락을 구분하는 데 여전히 유용하다.**

---

## 19.10 Action Plan (P1·P2 리스크 우선 대응)

### 즉시 (Week 1–2)

- [ ] **R-M1:** 잠재 사용자 10명(프로 엔지니어) 인터뷰 예약. 질문지 초안 작성.
- [ ] **R-L2:** JUCE 라이선스 결정(GPL vs Commercial). `chowdsp_utils` 사용 여부 최종 판단. 의존성 감사 자동화 pipeline 구축.
- [ ] **R-O1:** 프로젝트 스코프 명시적 문서화. MVP 범위 확정 (핵심 유닛 5–6개, 모드 3개).
- [ ] **R-T1:** WDF+JA 수렴성 프로토타입 spike. 간단한 tube stage로 convergence rate 벤치.

### 단기 (Month 1–3)

- [ ] **R-M1:** 인터뷰 완료, 결과 정리 → MVP/pricing 조정.
- [ ] **R-L2:** Clean-room WDF/JA 구현 초안 완료 또는 JUCE Pro 계약.
- [ ] **R-T1:** WDF+JA+NR 공식 구현, stress test 통과 (극한 입력에서 수렴).
- [ ] **R-T3:** 기존 공개 측정 데이터(KVR, AES 논문, 문헌) 수집 · 정리.
- [ ] **R-L1:** 제품명·브랜드 상표 사전 검색 (USPTO TESS, KIPRIS). 법률 자문 1회 예약.
- [ ] **R-O4:** Phase 1 예산 상세화, 월간 burn rate 추적 spreadsheet.

### 중기 (Month 3–6)

- [ ] **R-T5:** Mac/Windows 3개 DAW (Pro Tools, Logic, Ableton)에서 초기 플러그인 로드 테스트.
- [ ] **R-M2:** 경쟁사 제품 업데이트 모니터링 RSS/뉴스레터 시스템 구축. 분기 1회 review.
- [ ] **R-O2:** 첫 milestone 리뷰. velocity 실측 → 일정 재조정.
- [ ] **R-T3:** 자체 측정 세션 1회 (최소 하드웨어 1–3대).
- [ ] **R-O1:** 1차 burnout 리스크 셀프 평가. 스코프 축소 필요 여부 결정.

### 장기 (Month 6–12)

- [ ] **R-O4:** Phase 2 예산 집행 검토. Crowdfunding/pre-sale 검토.
- [ ] **R-T3:** 하드웨어 측정 세션 확장 (최소 3대 이상, 가능 시 5대+).
- [ ] **R-L1:** 상표 출원 (주요 시장 — US, EU, KR, JP).
- [ ] **R-M1:** Closed beta 출시, 20–50명 사용자 확보, 피드백 기반 최종 조정.
- [ ] **R-E3:** Avid Developer 신청 및 AAX qualification 진행.

---

## 19.11 리스크 리뷰 주기

| 주기 | 활동 | 산출물 |
|------|------|--------|
| **주간** | 새 이슈 탐지, P1 진행 상황 체크 | 주간 status 노트 |
| **월간** | Risk Register 업데이트 (P/I 재평가, 신규 리스크 추가) | 업데이트된 Register |
| **분기** | Stakeholder(있다면) 리뷰, 전략 조정 | 분기 report |
| **반기** | Milestone vs 예산 평가, 범위 재평가, Pivot 검토 | 반기 strategy doc |

---

## 19.12 Pivot Scenarios (피벗 시나리오)

리스크가 현실화되어 원래 계획대로 진행이 불가능한 경우에 대비한 **사전 설계된 피벗 경로**.

### 시나리오 A: 시장 반응 약함 (R-M1 현실화)

- **신호:** 베타 테스터 NPS < 20, landing page 전환률 < 1%, 인터뷰에서 "지불 의사 없음" 다수.
- **피벗:**
  - 포지셔닝을 **일반 플러그인 → 특수 용도**로 재조정 (예: "마스터링 전용", "클래식 녹음용", "영화 사운드 디자인용")
  - 가격 하향 ($299 → $99–149)
  - 타깃 재정의: "프로 믹스 엔지니어 전체" → "마스터링 엔지니어 + 오디오필" 같은 좁은 세그먼트
  - 마케팅 메시지 재작성

### 시나리오 B: 개발 난이도 > 예상 (R-T1/R-T2 현실화)

- **신호:** 12개월 시점에 WDF+JA 수렴 불안정 지속, CPU 목표 30% 초과.
- **피벗:**
  - MVP 범위 대폭 축소: **Physical tube model + 정적(static) transformer** 만. Monte Carlo 시변 기능은 V2로 전면 이동.
  - V1.0 출시 후 점진적 기능 추가 (1년에 1–2개 메이저 업데이트).
  - "V1 = 기본 physics, V2 = 살아있는 아날로그 완성"으로 스토리텔링.

### 시나리오 C: 경쟁사 유사 제품 출시 (R-M2 현실화)

- **신호:** UAD/Waves가 "개체차 기반" 또는 "시변 파라미터" 유사 기능을 정식 발표/출시.
- **피벗:**
  - **차별화 강화:** 특정 하드웨어 특화 (측정 접근 가능한 희귀 유닛), 특정 장르 특화 (클래식, 재즈, K-pop 등)
  - **B2B 라이선싱:** 하드웨어 제조사에 자체 브랜드 플러그인 제공 (white label)
  - **기술 컨설팅·라이선싱:** DSP 엔진 자체를 다른 플러그인 제조사에 라이선스

### 시나리오 D: 오픈소스 피벗 (R-L2 심각화 또는 비즈니스 실패 시)

- **신호:** GPL 감염 해결 불가, 또는 상업화 실패로 매출 불투명.
- **피벗:**
  - 코어 엔진 GPL-3 또는 MIT로 공개
  - 수익 모델: 상용 플러그인(프리미엄 GUI, 프리셋 라이브러리, 측정 데이터 pack) 유료, 또는 컨설팅·교육·오디오 제조사 통합
  - 커뮤니티 성장 → 장기 브랜드 가치

---

## 19.13 성공 기준 (Success Criteria) / 실패 기준 (Failure Criteria)

리스크 관리의 실효성은 결국 "성공·실패를 어떻게 정의하는가"에 달려있다.

### 1년 내 성공 기준

- MVP 출시 완료 (VST3 + AU 최소)
- 1,000 유저 (무료 베타 포함)
- 주요 DAW 3종 이상 공식 지원 (Logic, Pro Tools, Ableton)
- 최소 10개 공개 리뷰 (사용자 포럼, 유튜버 영상)

### 2년 내 성공 기준

- 유료 유저 500–2,000명
- 누적 매출 $100K–500K
- 주요 출판 매체 리뷰 1회 이상 (Sound on Sound, Mix Online, Tape Op, ComputerMusic)
- AAX 지원 완료 → Pro Tools 생태계 진입

### 5년 내 성공 기준

- 업계 인지도: "UAD, Waves 다음 세대 신흥 브랜드"로 언급
- 제품 포트폴리오 3–5개
- 지속 가능한 매출 (개발자 1인 full-time 유지 가능)

### 실패 기준 (이 조건 충족 시 Pivot Scenario 집행)

- 2년 내 유료 유저 < 200명
- 출시 후 6개월 이내 리콜 수준의 기술 이슈 (대량 크래시, 대규모 부정확 출력)
- 법적 분쟁 패소 (상표, 특허, 라이선스)
- 1년 이상 burn rate 지속 + 매출 < 비용

---

## 19.14 Stakeholder 분석 (해당 시)

1인 프로젝트에서도 이해관계자는 존재하며, 각각의 영향력·관심도를 이해하면 의사결정 속도가 올라간다.

| Stakeholder | Interest (관심) | Power (영향력) | 관리 전략 |
|-------------|----------------|----------------|----------|
| **개발자 자신** | 높음 | 높음 | 자기 관리 (burnout prevention, 지속가능한 페이스) |
| **베타 테스터** | 중간 | 낮음–중간 | Monthly 업데이트 공유, 의견 반영 투명성 |
| **초기 사용자** | 높음 | 중간 | 빠른 support, 커뮤니티 구축 |
| **법률 자문** | 낮음 (외부) | 높음 (리스크 축소) | 정기 자문 계약, 명확한 질문 pre-defined |
| **플랫폼 (Apple, Avid, Microsoft)** | 낮음 | 높음 | 정책 compliance, developer program 유지 |
| **Distributor (Plugin Boutique 등)** | 중간 | 중간 | 주기적 소통, 프로모션 협력 |
| **(해당 시) 투자자/파트너** | 높음 | 높음 | 분기 report, milestone 기반 커뮤니케이션 |
| **경쟁사** | 중간 | 중간 | 모니터링 대상, 직접 소통은 지양 |
| **커뮤니티 (Gearspace, KVR, r/audioengineering)** | 중간 | 중간 (평판) | 건전한 engagement, 과한 marketing 자제 |

---

## 19.15 결론

> **좋은 기술은 필요조건이지 충분조건이 아니다.**

본 프로젝트는 DSP 기술 측면(문서 01–07)에서 높은 완성도를 지향한다. 그러나 기술 완성도만으로는 **상용 플러그인 시장에서 살아남지 못한다**. 시장 수요(R-M1), 경쟁사 대응(R-M2), 법적 안정성(R-L1, R-L2), 그리고 무엇보다 **개발자의 지속가능성(R-O1)** 이 기술 완성도만큼 중요하다.

본 문서는 "두려움을 조장"하려는 문서가 아니라, **두려워할 지점을 정확히 알고 그에 대응하는 도구**다. 분기별로 이 문서를 다시 열어 Risk Register를 업데이트하고, Action Plan의 진행 상황을 점검하는 것이 프로젝트의 장기 생존을 좌우한다.

리스크는 제거할 수 없다. 관리할 수 있을 뿐이다.

---

**연관 문서:**
- [08. Competitive Analysis](./08-competitive-analysis.md)
- [11. Target Hardware Catalog](./11-target-hardware-catalog.md)
- [17. Legal & Compliance](./17-legal-and-compliance.md)
- [18. Business Strategy](./18-business-strategy.md)
