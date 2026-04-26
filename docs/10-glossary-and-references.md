# 10. 용어집 및 참고문헌

> **전체 문서의 공통 레퍼런스 허브. 한·영 기술 용어 대조, 핵심 학술 논문 목록, 관련 오픈소스 프로젝트.**

---

## 1. 한·영 기술 용어 대조표

### A–C

| 한국어 | 영어 | 관련 문서 |
|--------|------|----------|
| 가역 자화 | Reversible magnetization | 02 |
| 개체차 | Unit-to-unit variation / Instance variation | 06 |
| 고조파 왜곡 | Harmonic Distortion (HD) | 05 |
| 공간 전하 | Space charge | 01 |
| 공진 | Resonance | 02 |
| 교류 결합 | AC coupling | 04 |
| 그리드 누설 저항 | Grid leak resistor | 04 |
| 기생 커패시턴스 | Stray / Parasitic capacitance | 02, 04 |
| 내부 임피던스 | Internal impedance | 01 |
| 누설 인덕턴스 | Leakage inductance | 02 |

### D–H

| 한국어 | 영어 | 관련 문서 |
|--------|------|----------|
| 다이나믹 바이어스 | Dynamic bias | 03 |
| 단계 간 임피던스 | Interstage impedance | 04 |
| 동작점 | Operating point / Q-point | 01 |
| 등가 직렬 저항 | Equivalent Series Resistance (ESR) | 03 |
| 메모리 효과 | Memory effect | 03 |
| 무히스테리시스 자화 | Anhysteretic magnetization | 02 |
| 밀러 커패시턴스 | Miller capacitance | 04 |
| 바이어스 | Bias | 01 |
| 반전류 | Heater-cathode leakage current | 03 |
| 부하선 | Load line | 01 |
| 비가역 자화 | Irreversible magnetization | 02 |
| 비선형 | Nonlinear | 01 |
| 사극관 | Tetrode | 01 |
| 삼극관 | Triode | 01 |
| 상호 인덕턴스 | Mutual inductance | 02 |
| 서스펜더 커패시터 | Cathode bypass capacitor (Ck) | 01, 03 |
| 소신호 | Small signal | 01 |
| 스크린 그리드 | Screen grid | 01 |
| 시변 | Time-varying | 03 |

### I–N

| 한국어 | 영어 | 관련 문서 |
|--------|------|----------|
| 이력(히스테리시스) | Hysteresis | 02 |
| 자기 포화 | Magnetic saturation | 02 |
| 자기변형 | Magnetostriction | 02 |
| 자속 밀도 | Magnetic flux density (B) | 02 |
| 자화 강도 | Magnetic field intensity (H) | 02 |
| 전달 컨덕턴스 | Transconductance (gm) | 01 |
| 전류 포화 | Current saturation | 01 |
| 전원 새그 | Power supply sag | 03 |
| 전자 방출 | Thermionic emission | 01 |
| 정류관 | Rectifier tube | 03 |
| 증폭 계수 | Amplification factor (μ) | 01 |
| 짝수 배음 | Even harmonics | 01, 05 |
| 캐소드 | Cathode | 01 |
| 캐소드 바운스 | Cathode bounce | 03 |
| 컷오프 | Cutoff | 01 |
| 크로스오버 왜곡 | Crossover distortion | 05 |

### O–Z

| 한국어 | 영어 | 관련 문서 |
|--------|------|----------|
| 투자율 | Permeability (μ) | 02 |
| 트랜스포머 | Transformer | 02 |
| 파동 디지털 필터 | Wave Digital Filter (WDF) | 07 |
| 플레이트 | Plate / Anode | 01 |
| 플레이트 저항 | Plate resistance (rp) | 01 |
| 포화 | Saturation | 01, 02 |
| 포화 자속 밀도 | Saturation flux density (Bsat) | 02 |
| 홀수 배음 | Odd harmonics | 01, 05 |
| 확률적 모델링 | Stochastic modeling | 06 |
| 허용오차 | Tolerance | 06 |
| 헌 | Hum | 01, 03 |
| 혼변조 왜곡 | Intermodulation Distortion (IMD) | 05 |

---

## 2. 핵심 학술 논문 목록

### 진공관 물리 모델링

1. **Koren, N. (1996).** "Improved vacuum-tube models for SPICE simulations." *Glass Audio*, 8(5), 18–27.
   - **의의:** Koren 모델의 원본 논문. 삼극관 SPICE 모델의 사실상 표준.
   - **원문:** https://www.normankoren.com/Audio/Tubemodspice_article.html (저자 본인 게재, Glass Audio 발행)

2. **Dempwolf, K., Holters, M., & Zölzer, U. (2011).** "A Physically-Motivated Triode Model for Circuit Simulations." *Proc. 14th Int. Conf. Digital Audio Effects (DAFx-11)*, Paris, France, Sept 19–23.
   - **의의:** 12AX7 등 실제 삼극관의 그리드 전류까지 포함하는 물리 기반 연속 미분가능 모델.
   - **PDF:** https://dafx.de/paper-archive/2011/Papers/76_e.pdf

3. **Leach, W. M. (1995).** "SPICE models for vacuum-tube amplifiers." *Journal of the Audio Engineering Society*, 43(3), 117–126.
   - **의의:** 다양한 진공관의 SPICE 파라미터 측정값 제공.
   - **AES E-Library:** https://www.aes.org/e-lib/browse.cfm?elib=7958

4. **Pakarinen, J., & Yeh, D. T. (2009).** "A review of digital techniques for modeling vacuum-tube guitar amplifiers." *Computer Music Journal*, 33(2), 85–100. DOI: 10.1162/comj.2009.33.2.85
   - **의의:** 진공관 앰프 디지털 모델링의 종합 리뷰.

### 트랜스포머 & 히스테리시스

5. **Jiles, D. C., & Atherton, D. L. (1986).** "Theory of ferromagnetic hysteresis." *Journal of Magnetism and Magnetic Materials*, 61(1-2), 48–60. DOI: 10.1016/0304-8853(86)90066-1
   - **의의:** Jiles-Atherton 모델의 원본 논문. 트랜스포머 히스테리시스 모델링의 기반.

6. **Holters, M., & Zölzer, U. (2016).** "Circuit Simulation with Inductors and Transformers Based on the Jiles-Atherton Model of Magnetization." *Proc. 19th Int. Conf. Digital Audio Effects (DAFx-16)*, Brno, Czech Republic, Sept 5–9.
   - **의의:** JA 모델의 오디오 회로 시뮬레이션 적용 (원래 표기된 DAFx-13은 오류, 실제로는 DAFx-16).
   - **PDF:** https://www.hsu-hh.de/ant/wp-content/uploads/sites/699/2017/10/Holters_jamodel_DAFx16.pdf

7. **Preisach, F. (1935).** "Über die magnetische Nachwirkung." *Zeitschrift für Physik*, 94(5-6), 277–302. DOI: 10.1007/BF01349418
   - **의의:** 히스테리시스의 Preisach 모델 원본 논문. 릴레이 히스테론의 병렬 합으로 일반 히스테리시스를 표현.

8. **Bogason, Ó., & Werner, K. J. (2018).** "Modeling Time-Varying Reactances using Wave Digital Filters." *Proc. 21st Int. Conf. Digital Audio Effects (DAFx-18)*, Aveiro, Portugal. (Best Paper Award)
   - **의의:** WDF 프레임워크에서의 시변 리액턴스 모델링 — 트랜스포머/인덕터의 시변 특성에 응용 가능.
   - **비고:** 원문에 표기된 "Bogason & Werner 2022 DAFx-22 트랜스포머 하모닉 논문"은 DAFx20in22 프로시딩에서 확인되지 않아 DAFx-18 논문으로 교체.

### Wave Digital Filters

9. **Fettweis, A. (1986).** "Wave digital filters: Theory and practice." *Proceedings of the IEEE*, 74(2), 270–327. DOI: 10.1109/PROC.1986.13458
   - **의의:** WDF 이론의 원본 논문 (Alfred Fettweis).

10. **Werner, K. J., Nangia, V., Bernardini, A., Smith, J. O., III, & Sarti, A. (2015).** "An Improved and Generalized Diode Clipper Model for Wave Digital Filters." *AES 139th Convention*, New York, Oct 29 – Nov 1, Paper **9360**.
    - **의의:** 비선형 소자의 WDF 구현 개선.
    - **AES E-Library:** https://aes.org/publications/elibrary-page/?id=17918 (paper # 재검증 결과 9387 → 9360)

11. **[검증 불가]** Rodríguez-Serrano, Á., Mannall, A., & Reiss, J. D. (2021). "A Comparison of Wave Digital Filter Topologies for Simulation of the Korg MS-20 VCF." (DAFx-21 프로시딩에서 동일 제목·저자 조합 미확인)
    - **의의:** 실제 신디사이저 회로의 WDF 구현 사례 (원 출처).
    - **경고:** 2026-04 재검증 시 DAFx20in21 공식 프로시딩 목차에서 해당 저자 3인의 논문 미확인. 인용 전 반드시 저자의 개인 publication list 재확인 필요. 대안 레퍼런스: Bogason & Werner 2018(항목 8), Werner et al. 2015(항목 10).

### Modified Nodal Analysis / DK Method

12. **Yeh, D. T., Abel, J., & Smith, J. O. (2007).** "Simplified, Physically-Informed Models of Distortion and Overdrive Guitar Effects Pedals." *Proc. Int. Conf. Digital Audio Effects (DAFx-07)*, Bordeaux.
    - **의의:** DK Method의 오디오 적용 첫 사례.

13. **Yeh, D. T., & Smith, J. O. (2006).** "Discretization of the '59 Fender Bassman Tone Stack." *Proc. 9th Int. Conf. Digital Audio Effects (DAFx-06)*, Montreal, Canada, Sept 18–20.
    - **의의:** 실제 빈티지 앰프 회로의 MNA 구현.
    - **PDF:** https://dafx.de/paper-archive/2006/papers/p_001.pdf (재검증 시 원본 DAFx-06 확인됨, 기존 DAFx-08 표기는 오류)

### 측정 및 오디오 계측

14. **Farina, A. (2000).** "Simultaneous Measurement of Impulse Response and Distortion with a Swept-Sine Technique." *AES 108th Convention*, Paris, Feb 18–22, Paper 5093.
    - **의의:** Exponential sine sweep (ESS) 방식 — 비선형 시스템의 임펄스 응답과 하모닉 왜곡을 동시 측정.
    - **AES E-Library:** https://www.aes.org/e-lib/browse.cfm?elib=10211

### 신경망 기반 오디오 처리

15. **Hawley, S. H., Colburn, B., & Mimilakis, S. I. (2019).** "SignalTrain: Profiling Audio Compressors with Deep Neural Networks." *arXiv:1905.11928* (AES 147th Convention, NY, Paper 10260).
    - **의의:** 오디오 장비의 신경망 프로파일링.
    - **주의:** 기존 "Menegola, S." 표기는 오류 — 실제 제3 저자는 Stylianos I. Mimilakis.

16. **Wright, A., Damskägg, E-P., Juvela, L., & Välimäki, V. (2020).** "Real-Time Guitar Amplifier Emulation with Deep Learning." *Applied Sciences*, 10(3), 766. DOI: 10.3390/app10030766
    - **의의:** LSTM 기반 실시간 앰프 에뮬레이션.

17. **Comunità, M., Steinmetz, C. J., Phan, H., & Reiss, J. D. (2023).** "Modelling Black-Box Audio Effects with Time-Varying Feature Modulation." *Proc. IEEE ICASSP 2023* (arXiv:2211.00497).
    - **의의:** 시변 특성을 가진 오디오 이펙트의 신경망 모델링 (TCN + FiLM).
    - **주의:** 기존 제목 "…using Convolutional Neural Networks and Feature Modulation"은 미제출 버전의 표기. 확정 제목은 위 ICASSP 2023 수록 버전.

### 심리음향

18. **Plomp, R., & Levelt, W. J. M. (1965).** "Tonal consonance and critical bandwidth." *Journal of the Acoustical Society of America*, 38(4), 548–560. DOI: 10.1121/1.1909741
    - **의의:** 협화음 이론 — 짝수 배음이 더 협화적인 이유.

19. **Hamm, R. O. (1973).** "Tubes versus transistors: Is there an audible difference?" *Journal of the Audio Engineering Society*, 21(4), 267–273.
    - **의의:** 진공관 vs 반도체 청취 비교 초기 연구.
    - **AES E-Library:** https://www.aes.org/e-lib/browse.cfm?elib=1980

20. **Zwicker, E., & Fastl, H. (2007).** *Psychoacoustics: Facts and Models* (3rd ed., Springer Series in Information Sciences, Vol. 22). Springer. ISBN-13: 978-3-540-23159-2 (hardcover) / 978-3-540-68888-4 (eBook).
    - **의의:** 심리음향학의 핵심 교재.
    - **주의:** 기존 표기의 ISBN 978-3-662-09562-1은 2nd ed. (1999) eBook 재인쇄 식별자 — 3rd ed.와 불일치. 3rd ed. 출판연도는 2013이 아닌 2007.

---

## 3. 관련 오픈소스 프로젝트

| 프로젝트 | URL | 관련 기능 | 라이선스 (2026-04 기준) |
|---------|-----|---------|---------|
| **Neural Amp Modeler (NAM)** | github.com/sdatkinson/NeuralAmpModelerCore | LSTM 기반 앰프 캡처 | MIT |
| **RTNeural** | github.com/jatinchowdhury18/RTNeural | 실시간 신경망 추론 C++ | BSD-3-Clause |
| **chowdsp_utils** | github.com/Chowdhury-DSP/chowdsp_utils | WDF, 필터, 포화 | 모듈별 혼합 (core/data/json 등 BSD, dsp/gui/plugin-base는 GPLv3) |
| **GuitarML** | guitarml.com | ML 기반 앰프 에뮬 | MIT (프로젝트별 상이) |
| **JUCE** | github.com/juce-framework/JUCE | 플러그인 프레임워크 | AGPLv3 / 상용 (듀얼 라이선스, JUCE 8 기준) |
| **wdf-library** | github.com/jatinchowdhury18/wdf-library | Wave Digital Filter 구현 | MIT |
| **Eigen** | libeigen.gitlab.io (구 eigen.tuxfamily.org) | 선형대수 (WDF 행렬 연산) | MPL-2 (일부 파일은 BSD/LGPL) |
| **xsimd** | github.com/xtensor-stack/xsimd | SIMD 추상화 | BSD-3-Clause |

> **라이선스 변경 주의 사항:**
> - **JUCE**는 JUCE 7까지 GPLv3/상용 듀얼이었으나 JUCE 8부터 **AGPLv3**/상용으로 변경됨. AGPL은 네트워크 이용 시에도 소스 공개 의무가 발생하므로 SaaS·웹 서비스 통합 시 주의.
> - **chowdsp_utils**는 단일 라이선스가 아닌 모듈별 라이선스 체계. DSP/GUI/Plugin 모듈은 여전히 GPLv3이며, 상용 이용 시 chowdsp@gmail.com 으로 별도 협의 필요.
> - **RTNeural**은 원본 설명과 달리 **BSD-3-Clause** (MIT 아님). 상용 이용에 더 유리.
> - **Eigen**은 GitLab 이전. 구 tuxfamily URL은 리다이렉트되나 공식 주소는 libeigen.gitlab.io.

---

## 4. AES / IEC 표준 문서

| 표준 번호 | 제목 | 관련 내용 |
|----------|------|---------|
| **AES17-2020** | AES standard method for digital audio engineering — Measurement of digital audio equipment | 기본 측정 방법론 (2015 → 2020 개정, 0 dBFS 정의 명확화) |
| **ITU-R BS.1770-5** (2023-11) | Algorithms to measure audio programme loudness and true-peak audio level | LUFS 라우드니스 / true-peak 측정 (BS.1770-4 → -5로 개정) |
| IEC 60268-1 | Sound system equipment — General | 시스템 측정 일반 요구사항 |
| IEC 60268-3 | Sound system equipment — Part 3: Amplifiers | 앰프 측정 방법 |
| IEC 60384-1 | Fixed capacitors for use in electronic equipment — Part 1: Generic specification | 커패시터 일반 규격 |
| MIL-STD-883 | Test Method Standard — Microcircuits | 컴포넌트 신뢰성/환경 시험 (미국 방위표준) |

---

## 5. 추천 서적

1. **Jones, M. (2003).** *Valve Amplifiers* (3rd ed.). Newnes. ISBN-13: 978-0750656948 / ISBN-10: 0750656948. 640 pp.
   - 진공관 앰프 설계의 종합 교재. 이론과 실제.
   - **참고:** 현재 최신판은 4th ed. (Newnes, 2012, ISBN 978-0080966403).

2. **Blencowe, M. (2009).** *Designing Tube Preamps for Guitar and Bass* (1st ed.). Wem Publishing. ISBN-13: 978-0956154507.
   - 프리앰프 설계의 실용적 접근. 캐소드 바운스, 바이어스 설계 상세.
   - **참고:** 2nd ed. (2012, ISBN 978-0956154521) 존재.

3. **Self, D. (2010).** *Audio Power Amplifier Design Handbook* (5th ed.). Focal Press. ISBN-13: 978-0240521626.
   - 파워앰프 설계. 크로스오버 왜곡, 클래스 분석.
   - **참고:** 최신판은 6th ed. *Audio Power Amplifier Design* (Focal Press/Routledge, ISBN 978-0240526133).

4. **Whitlock, B. (2008).** "Audio transformers." In *Handbook for Sound Engineers* (4th ed., G. Ballou, Ed.). Focal Press.
   - 트랜스포머의 모든 것. Jensen 트랜스포머 설계자의 저술.
   - **전문 PDF:** https://www.jensen-transformers.com/wp-content/uploads/2014/09/Audio-Transformers-Chapter.pdf

5. **Snelling, E. C. (1988).** *Soft Ferrites: Properties and Applications* (2nd ed.). Butterworths.
   - 자성 재료의 물리. 코어 재질별 B-H 특성.

6. **Zölzer, U. (Ed.) (2011).** *DAFX: Digital Audio Effects* (2nd ed.). Wiley. ISBN-13: 978-0470665992.
   - 오디오 이펙트 DSP의 교과서. 관련 장 예시:
     - Ch.4 *Nonlinear Processing* (Arfib, Keiler, Zölzer): 소프트/하드 클리핑, 웨이브셰이퍼 이론.
     - Ch.12 *Virtual Analog Effects* (Pakarinen, Yeh): 진공관/회로 에뮬레이션.
     - WDF, DK-method, 스펙트럴 효과 포함.
   - **공식 사이트:** https://www.dafx.de/DAFX_Book_Page_2nd_edition/

7. **Smith, J. O. (2010).** *Physical Audio Signal Processing.* W3K Publishing.
   - 물리 기반 오디오 신호 처리. 온라인 무료 제공 (https://ccrma.stanford.edu/~jos/pasp/).

8. **Katz, B. (2014).** *Mastering Audio: The Art and the Science* (3rd ed.). Focal Press. ISBN-13: 978-0240818962.
   - 마스터링 이론과 실제. 라우드니스 (LUFS) 측정, PLR 평가, 다이나믹스 관리 — ITU-R BS.1770 계측과 연계된 실무 관점.

---

## 6. 유용한 온라인 리소스

| 리소스 | URL | 내용 |
|--------|-----|------|
| CCRMA Julius Smith | ccrma.stanford.edu/~jos | 물리 오디오 신호 처리 무료 교재 |
| KVR Audio Forums | kvraudio.com/forum | 플러그인 개발 커뮤니티 |
| DIYAudio | diyaudio.com | 진공관 회로 설계 커뮤니티 |
| Julian Krause YouTube | youtube.com/@JulianKrause | 플러그인 측정 영상 |
| DAFx Proceedings | dafx.de | 오디오 이펙트 학술 논문 아카이브 |
| AES E-Library | aes.org/e-lib | AES 논문 전체 아카이브 |
| Tube Data Sheet Locator | tubedata.hellom.net | 진공관 데이터시트 모음 |

---

## 7. 검증 날짜 및 방법

**최종 검증일:** 2026-04-17

**검증 방법:**

1. **학술 논문 (Section 2):**
   - DOI 직접 조회: `https://doi.org/<DOI>` → 출판사 원문/메타데이터 확인.
   - 주요 DB 교차 확인: ScienceDirect, IEEE Xplore, AIP (JASA), AES E-Library, DAFx 아카이브 (dafx.de/paper-archive), Semantic Scholar, NASA ADS.
   - DAFx 논문은 연도별 프로시딩 PDF로 존재 유무 및 수록 페이지 확인.

2. **서적 ISBN (Section 5):**
   - Amazon, AbeBooks, Routledge/Elsevier/Wiley 공식 페이지, Google Books, Internet Archive 교차 확인.
   - 초판/개정판 구분 — 원문에 명시된 판본의 ISBN 직접 확인 + 최신판 병기.

3. **오픈소스 라이선스 (Section 3):**
   - 각 GitHub 저장소의 LICENSE / LICENSE.md / README 직접 방문.
   - JUCE: LICENSE.md 현재 AGPLv3+상용 듀얼 (JUCE 8 기준) 확인.
   - chowdsp_utils: README의 "모듈별 라이선스" 정책 확인 — BSD(core) + GPLv3(dsp/gui).
   - RTNeural: README의 "BSD 3-clause" 명시 확인 (기존 MIT 표기 오류 수정).
   - Eigen: GitLab 이전 반영 (libeigen.gitlab.io).

4. **표준 문서 (Section 4):**
   - AES: aes.org/publications/standards — AES17-2015 → AES17-2020 개정 확인.
   - ITU: itu.int — BS.1770-5 (2023-11) 최신판 확인.

**주요 변경사항 요약:**

- **수정:** Holters & Zölzer JA 모델 논문은 DAFx-13(2013, Maynooth) → **DAFx-16(2016, Brno)**가 실제 발행.
- **수정:** RTNeural 라이선스 MIT → **BSD-3-Clause**.
- **수정:** JUCE 라이선스 GPL/Commercial → **AGPLv3/Commercial** (JUCE 8부터).
- **수정:** chowdsp_utils 라이선스 GPL-3 → **모듈별 혼합 (BSD + GPLv3)**.
- **교체:** "Bogason & Werner 2022 DAFx-22 트랜스포머 하모닉 논문"은 DAFx20in22 프로시딩에서 미확인 → 실재하는 **Bogason & Werner 2018 DAFx-18 시변 리액턴스 (Best Paper)**로 교체.
- **보강:** Farina 2000 ESS 측정 논문, Preisach 1935 DOI, Pakarinen-Yeh DOI, Fettweis DOI, Plomp-Levelt DOI, Zölzer DAFX 책 구체 장 번호(Ch.4, Ch.12), Bob Katz *Mastering Audio* 3rd ed. 추가.
- **보강:** AES17-2020, ITU-R BS.1770-5 최신판 반영.

**2026-04 재검증(2차) 수정사항:**

- **수정:** Yeh & Smith 톤스택 논문 **DAFx-08 → DAFx-06 (2006, Montreal)**. 원 출처(ccrma.stanford.edu/~dtyeh) 및 DAFx 아카이브 직접 확인.
- **수정:** Werner et al. 2015 AES 139th **Paper 9387 → 9360** (AES E-Library id=17918 직접 확인).
- **수정:** Hawley et al. 2019 SignalTrain 제3 저자 **"Menegola, S." → "Mimilakis, S. I."** (arXiv metadata 및 NASA ADS 확인) — 이는 치명적 오류.
- **수정:** Comunità & Reiss 2023 → **Comunità, Steinmetz, Phan, Reiss 2023**로 저자 확장, 제목도 "…with Time-Varying Feature Modulation" (arXiv 2211.00497 v1-v3 공식 제목)으로 교정.
- **수정:** Zwicker & Fastl 3rd ed. **2013 → 2007**. ISBN 978-3-662-09562-1은 2nd ed. eBook 식별자 — 3rd ed. 하드커버 ISBN은 978-3-540-23159-2.
- **검증 불가:** Rodríguez-Serrano, Mannall, Reiss (2021) DAFx-21 Korg MS-20 VCF 논문 — DAFx20in21 공식 프로시랑 검색에서 미확인. 항목에 경고 주석 삽입.

**검증 불가 / 주의 항목:**

- Koren (1996) Glass Audio 8(5): 잡지 자체가 2005년 폐간되어 AES DB·학술 DB에 미색인. 저자 홈페이지(normankoren.com)에 동일 내용이 게재되어 있어 1차 출처로 대체.
- "GuitarML" 은 단일 레포가 아닌 우산 프로젝트 — 하위 프로젝트별 라이선스 상이.
- MIL-STD-883은 미국 DoD 정기 개정 (현재 Rev. K, 2019 계열) — 정확 Revision은 사용처마다 지정 필요.
