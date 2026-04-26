# 06. 확률적 개체차 모델링 (Monte Carlo)

> **연관 문서:** [01 진공관 물리](./01-vacuum-tube-physics.md) · [03 시변 비선형성](./03-time-varying-nonlinearities.md) · [09 측정 방법론](./09-measurement-and-validation.md)

> **핵심 주장:** 스튜디오의 랙에 꽂힌 10개의 Neve 1073은 서로 미세하게 다른 소리를 낸다. 이것이 "아날로그의 개성"이다. 현재 어떤 플러그인도 이 개성을 진지하게 모델링하지 않는다. Monte Carlo 컴포넌트 허용오차 시뮬레이션으로 이를 구현한다.

---

## 1. 왜 개체차가 중요한가

### 1.1 아날로그 "집합적 개성"의 효과

여러 인스턴스의 플러그인이 **모두 동일하게** 동작하면:
- 멀티트랙의 여러 채널에 같은 플러그인을 걸면 위상 정렬이 완벽 → 부자연스러운 이미지
- 반복 재생 시 매번 동일 → 기계적인 느낌

반면 각 인스턴스가 **미세하게 다르다면:**
- 여러 채널에 걸 때 각 채널이 살짝 다른 색깔 → 자연스러운 앙상블 느낌
- "글루" 효과 증가
- 실제 아날로그 장비 여러 대를 사용하는 것과 유사

### 1.2 실제 아날로그 장비의 변이 범위

전문 측정 기관의 데이터:
- 같은 제품 라인 내 THD 변이: ±2–5 dB (−60 dBc 기준에서)
- 저주파 롤오프 주파수 변이: ±0.5–2 Hz
- 이득 변이: ±0.1–0.5 dB
- 하모닉 위상 변이: ±10–30°

---

## 2. 컴포넌트 허용오차 분석

### 2.1 저항 허용오차

E-series(E3/E6/E12/E24/E48/E96/E192)는 IEC 60063에 정의된 "preferred values"로, 각 시리즈의 뒤 숫자는 한 decade당 표준값 개수를 뜻하며 허용오차는 40%/20%/10%/5%/2%/1%/0.5%로 대응된다.

| 등급 | 허용오차 | 분포 (실측) | 주요 용도 |
|------|---------|------|---------|
| 0.1% (E192) | ±0.1% | Gaussian (σ≈0.03%), 센터링 양호 | 정밀 계측 |
| 1% (E96) | ±1% | Gaussian (σ≈0.3%), 센터링 양호 | 고급 오디오 |
| 5% (E24) | ±5% | Gaussian 또는 절단된 Uniform (선별 후 남은 분포) | 일반 회로 |
| 10% (E12) | ±10% | 절단된 Uniform 또는 이봉(bimodal) — 5% 선별 후 남은 품 | 빈티지/저가 회로 |

**중요:** 5%·10% 품의 분포가 항상 Gaussian인 것은 아니다. 제조사가 측정해서 정확한 것은 1%·0.5% 품으로 먼저 출고하고 **남은 것**을 5%·10%로 등급 분류하기 때문에, 실제 분포는 중앙이 비어 있는 이봉 형태이거나 절단된 uniform에 가까울 수 있다. Monte Carlo 시뮬레이션에서 "5% Gaussian"으로 가정하면 실제 하드웨어보다 저왜곡 쪽으로 치우친 결과가 나올 수 있다.

빈티지 장비 (1960–1980년대)는 주로 5–10% 저항을 사용했다.

**회로 내 영향:**
- 바이어스 저항의 편차 → 동작점 이동 → 하모닉 비율 변화
- 음량 분배 저항의 편차 → 이득 변화
- 피드백 저항의 편차 → 전체 이득 + 왜곡 변화

### 2.2 커패시터 허용오차

| 유형 | 허용오차 | 온도 계수 | 에이징 특성 |
|------|---------|----------|-----------|
| 폴리프로필렌 | ±2–5% | 매우 낮음 (±50 ppm/°C) | 안정적 |
| 폴리에스터 | ±10–20% | 중간 | 안정적 |
| 전해 (알루미늄) | −20%/+80% | 높음 | 불안정 (ESR 증가) |
| 세라믹 (X7R) | ±10–20% | 높음 (−3000~+3000 ppm/°C) | DC 바이어스 의존성 큼 |

**전해 커패시터의 비대칭 허용오차 분포:**

전해 커패시터는 제조 특성상 음방향보다 양방향으로 더 큰 허용오차를 가진다. 이 비대칭 분포가 회로의 저주파 응답에 불규칙한 변이를 만든다.

### 2.3 진공관 파라미터 분포

같은 로트(lot)의 12AX7 진공관들의 파라미터 분포:

| 파라미터 | 평균 | 표준편차 | 분포 형태 |
|----------|------|---------|---------|
| gm (전달 컨덕턴스) | 1.6 mA/V | ±0.2 mA/V (≈12.5%) | 대략 Gaussian |
| μ (증폭 계수) | 100 | ±10 (≈10%) | Gaussian |
| rp (플레이트 저항) | 62 kΩ | ±8 kΩ (≈13%) | Gaussian |

> 주의: 위 수치는 공개된 NOS·현대관 다수 측정치와 튜브테스터 데이터를 종합한 **합리적 추정치**다. 특정 로트·제조사를 대표하는 공식 통계가 아니므로, 정확한 분포는 선별 등급·제조년도에 따라 ±30% 이상 차이날 수 있다. 데이터시트의 min/max 범위(JJ/EH/Sovtek 등)와 Mullard/Telefunken NOS 측정 자료를 참고로 사용한다.

**제조사 등급별 선별:**
- "선별관(matched/selected)": σ ≈ 5%
- "표준(standard)": σ ≈ 10–15%
- "저가/미선별": σ ≈ 20–25%

---

## 3. Matched Pair와 Unmatched의 소리 차이

### 3.1 Dual Triode (12AX7) 내부 두 섹션

12AX7 한 관 안에는 두 개의 독립적인 삼극관이 있다. 이론적으로 동일해야 하지만 실제로는:
- gm 차이: 보통 1–5%, 나쁜 경우 10% 이상
- 섹션 간 내부 커플링: 공유되는 히터, 진공 공간

**두 섹션의 gm 차이가 만드는 효과:**
- 스테레오 채널 분리용으로 쓸 경우: 이득 불균형
- 푸시풀 드라이버로 쓸 경우: 짝수 배음 상쇄가 불완전 → **2nd harmonic 강화**

### 3.2 Matched vs Unmatched Push-Pull Power Tubes

스테레오 파워앰프의 출력관 (EL34 4개 등):

| 매칭 상태 | H2 (@rated output) | H3 (@rated output) | 특성 |
|----------|-------------------|-------------------|------|
| 완벽 매칭 | −65 dBc 이하 | −55 dBc | 클리어, 중립 |
| 5% 미스매치 | −52 dBc | −56 dBc | 약간 따뜻함 |
| 10% 미스매치 | −46 dBc | −57 dBc | 뚜렷한 따뜻함 |

**포인트:** 의도적 미스매치가 더 "좋은" 소리를 내는 경우가 있다. 이것이 일부 엔지니어가 "unmatched tubes sound better"라고 하는 이유다.

---

## 4. Aging Distribution — 에이징 확률 모델

### 4.1 Weibull 분포 기반 진공관 수명 모델

진공관의 파라미터 저하는 Weibull 분포로 모델링할 수 있다:

$$f(t) = \frac{\beta}{\eta}\left(\frac{t}{\eta}\right)^{\beta-1} e^{-(t/\eta)^{\beta}}$$

여기서:
- $t$: 사용 시간 (시간)
- $\eta$: 특성 수명 (12AX7: ~10,000–30,000 시간)
- $\beta$: 형상 파라미터 (초기 고장: $\beta < 1$, 마모 고장: $\beta > 1$)

### 4.2 파라미터 시간 의존성

| 사용 시간 | gm 변화 | 노이즈 레벨 | 특성 |
|----------|---------|-----------|------|
| 새 관 (0h) | 100% (기준) | 낮음 | 선명, 밝음 |
| 100h | 98–99% | 약간 증가 | 안정적 |
| 500h | 94–97% | 약간 증가 | "break-in" 완료 |
| 2000h | 88–93% | 중간 | 빈티지 시작 |
| 5000h | 78–87% | 뚜렷한 증가 | 뚜렷한 빈티지 느낌 |
| 10000h+ | 60–75% | 높음 | 교체 권장 수준 |

gm 감소는 이득 감소뿐만 아니라 왜곡 특성도 변화시킨다.

---

## 5. Per-Instance Seed System

### 5.1 설계 목표

1. **재현성:** 동일 프로젝트를 다른 날 열어도 동일 소리
2. **다양성:** 새 인스턴스마다 다른 개성
3. **제어성:** "Reroll" 기능으로 원하는 개성 탐색
4. **투명성:** 어떤 파라미터가 얼마나 변화했는지 시각화 가능

### 5.2 구현 구조

```cpp
class ComponentVariation {
public:
    uint64_t seed;          // 재현성을 위한 고정 시드
    float gm_scale;         // 진공관 gm 스케일 (0.85–1.15)
    float mu_offset;        // μ 오프셋 (−10 ~ +10)
    float Rk_scale;         // 캐소드 저항 스케일 (0.9–1.1)
    float Ck_scale;         // 캐소드 커패시터 스케일 (0.8–1.2)
    float transformer_sat;  // 트랜스포머 포화 임계값 스케일 (0.9–1.1)
    float lf_rolloff_hz;    // 저주파 롤오프 주파수 (7 ± 2 Hz)
    float hf_peak_hz;       // 고주파 공진 피크 (16000 ± 1500 Hz)

    void generate(uint64_t instanceSeed) {
        seed = instanceSeed;
        std::mt19937_64 rng(seed);
        std::normal_distribution<float> norm(0.0f, 1.0f);

        gm_scale          = 1.0f + 0.08f * norm(rng);  // σ=8%
        mu_offset         = 8.0f * norm(rng);           // σ=8
        Rk_scale          = 1.0f + 0.05f * norm(rng);  // σ=5%
        Ck_scale          = 1.0f + 0.10f * norm(rng);  // σ=10% (전해)
        transformer_sat   = 1.0f + 0.06f * norm(rng);  // σ=6%
        lf_rolloff_hz     = 7.0f + 2.0f * norm(rng);
        hf_peak_hz        = 16000.0f + 1200.0f * norm(rng);
    }

    void reroll() {
        // 새로운 랜덤 시드 생성
        std::random_device rd;
        generate(rd());
    }
};
```

### 5.3 DAW 상태 저장

플러그인의 상태를 저장할 때 `seed` 값만 저장하면 된다. 로드 시 동일 seed로 모든 파라미터를 재생성 → 완전한 재현성.

```xml
<!-- VST3 state 예시 -->
<PluginState>
  <InstanceSeed>8247392847362</InstanceSeed>
  <TubeAge>0.35</TubeAge>  <!-- 0=새것, 1=극도 에이징 -->
  <Drive>0.5</Drive>
</PluginState>
```

---

## 6. Population Statistics

### 6.1 실제 하드웨어 측정 기반 분포 추출

이상적인 경우: 동일 제품 50대를 측정하여 파라미터 분포를 추출한다.

**측정 파라미터:**
- 1kHz에서 H2, H3 레벨 (dBc)
- SMPTE IMD
- 저주파 −3dB 주파수
- 고주파 −3dB 주파수
- 이득 (+/−)

**분석 방법:**
1. 각 파라미터의 평균, 표준편차, 분포 형태(Gaussian/Log-normal/Uniform) 추정
2. 파라미터 간 상관관계(correlation matrix) 분석
3. PCA로 주요 변이 방향 추출

### 6.2 파라미터 상관관계

중요: 컴포넌트 편차들은 **독립적이지 않다**. 예:
- gm이 높은 관은 rp가 낮은 경향 → 부(negative) 상관
- Lm이 높은 트랜스포머는 저주파 롤오프가 낮음 → 양(positive) 상관

이 상관관계를 반영한 다변량 정규분포로 파라미터를 생성:

```cpp
// Cholesky decomposition으로 상관 있는 변수 생성
Eigen::MatrixXf L = covMatrix.llt().matrixL();
Eigen::VectorXf uncorrelated = sampleGaussian(rng, N);
Eigen::VectorXf correlated = L * uncorrelated;
```

---

## 7. Vintage vs New Production 분포 차이

### 7.1 빈티지 진공관 (NOS: New Old Stock, 1950–1980년대)

- 제조사: Telefunken, Mullard, Amperex, GE, RCA
- 특징: 당시 엄격한 군/방송용 규격, 내구성 우선
- 파라미터 분포: gm 표준편차 **σ ≈ 5%** (현대 생산보다 타이트)
- 노이즈: 플리커 노이즈 낮음
- 에이징: 이미 수십 년이 지나 파라미터가 안정화됨

### 7.2 현대 생산 진공관 (2000년대~)

- 제조사: JJ, Electro-Harmonix, Sovtek, Psvane
- 특징: 수익성 중시, 일부 제조사는 품질 변동 큼
- 파라미터 분포: gm σ ≈ 10–20% (빈티지보다 넓음)
- 노이즈: 일부 현대관에서 높은 플리커 노이즈
- 에이징: 초기 변화 더 큼

**플러그인에서 "Vintage" vs "Modern" 토글:**
두 분포 프리셋을 선택할 수 있도록 설계한다.

---

## 8. Monte Carlo 렌더링 모드

### 8.1 개념

오프라인 렌더링 시 동일 트랙을 **서로 다른 seed(인스턴스 변이)**로 여러 번 렌더하여 "녹음 여러 테이크"를 시뮬레이션한다.

**사용 시나리오:**
- 드럼: 각 테이크마다 미세하게 다른 포화 → 현실적인 변이
- 합창: 각 성부를 약간 다른 "마이크/채널" 특성으로 처리
- 앙상블: 여러 악기를 동일 버스에 거치되 각 "채널"이 다름

### 8.2 구현

```cpp
class MonteCarloRenderer {
    int numTakes;
    std::vector<ComponentVariation> variations;

public:
    MonteCarloRenderer(int takes, uint64_t baseSeed) : numTakes(takes) {
        std::mt19937_64 rng(baseSeed);
        for (int i = 0; i < takes; ++i) {
            variations.emplace_back();
            variations.back().generate(rng());
        }
    }

    AudioBuffer renderAndBlend(const AudioBuffer& input,
                                TubeProcessor& proc,
                                BlendMode mode)
    {
        std::vector<AudioBuffer> takes(numTakes);
        for (int i = 0; i < numTakes; ++i) {
            proc.setVariation(variations[i]);
            takes[i] = proc.process(input);
        }
        return blend(takes, mode);  // 평균 또는 다이버시티 합성
    }
};
```

---

## 참고문헌

1. Dempwolf, K., Zölzer, U. (2016). "Analysis and Simulation of the Harmonic Spectrum of Vacuum Tube Amplifiers." *Proc. Int. Conf. Digital Audio Effects.*
2. MIL-STD-883, *Test Methods and Procedures for Microelectronics.* US Department of Defense.
3. IEC 60384-1:2021 (Edition 6.0), *Fixed capacitors for use in electronic equipment — Part 1: Generic specification.* International Electrotechnical Commission. (전해관의 Z 코드: +80%/−20% 허용오차 표기)
4. IEC 60063:2015, *Preferred number series for resistors and capacitors.* 전해를 포함한 E3/E6/E12/E24/E48/E96/E192 시리즈의 공식 정의.
5. Erickson, R. W. (1997). *Fundamentals of Power Electronics.* Springer. (Chapter on component tolerances)
6. Lawton, R. A. (1979). "Variability of electronic components in audio equipment." *Journal of the Audio Engineering Society*, 27(11).
7. Middleton, D. (1960). *An Introduction to Statistical Communication Theory.* McGraw-Hill. (on parameter distribution)
8. Kuijk, M. (2012). "Monte Carlo simulation for audio circuit design." *Linear Audio*, 3.
9. Weibull, W. (1951). "A statistical distribution function of wide applicability." *Journal of Applied Mechanics*, 18(3), 293–297. (Weibull 분포 원전. 진공관 수명을 포함한 전자부품 신뢰성 분석의 표준 모델.)
