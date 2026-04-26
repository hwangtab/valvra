# 03. 시변 비선형성 — "살아있는" 아날로그의 핵심

> **연관 문서:** [01 진공관 물리](./01-vacuum-tube-physics.md) · [02 트랜스포머](./02-transformer-physics-and-distortion.md) · [06 개체차 모델링](./06-stochastic-component-modeling.md)

> **핵심 주장:** 기존 플러그인은 시간 $t$와 무관한 $y = f(x)$를 계산한다. 실제 아날로그 하드웨어는 $y = f(x, t, \text{history}, \text{temperature})$를 계산한다. 이 차이가 "살아있는 느낌"과 "납작한 플러그인 소리"를 가르는 근본 원인이다.

---

## 1. 시변 비선형성의 분류

아날로그 하드웨어의 시변 효과는 크게 세 시간 스케일로 분류된다:

| 시간 스케일 | 현상 | 시정수 |
|-------------|------|--------|
| **마이크로초–밀리초** | Cathode bounce, 커패시터 충방전 | 1ms–100ms |
| **초–분** | 열적 드리프트, 바이어스 안정화 | 5s–300s |
| **시간–년** | 에이징, 커패시터 열화 | 수백 시간 이상 |

---

## 2. 열적 드리프트 (Thermal Drift)

### 2.1 진공관 히터 워밍업 곡선

진공관에 전원을 인가하면 히터가 서서히 가열되어 동작 온도(약 600–900°C)에 도달한다. 이 과정에서 전기적 특성이 지속적으로 변한다.

**히터 온도 곡선 (지수 수렴):**

$$T(t) = T_{ambient} + (T_{max} - T_{ambient})(1 - e^{-t/\tau_{heat}})$$

일반적인 시정수 (데이터시트 지정 "start of conduction" 시간과 완전한 열적 안정화 시정수는 다르다. 아래는 열적 완전 안정화 기준의 실측 대역):
- 소신호 삼극관 (12AX7): 전도 시작 ≈ 5–11초, 열적 안정화 $\tau_{heat} \approx$ 15–30초
- 소형 파워관 (EL84): 전도 시작 ≈ 10–15초, 열적 안정화 $\tau_{heat} \approx$ 30–60초
- 대형 파워관 (KT88): 1.6A 히터 전류로 인해 전도 시작 ≈ 15–25초, 열적 안정화 $\tau_{heat} \approx$ 60–120초

> 출처 확인 필요: 위 "열적 안정화" 시정수는 업계에서 널리 인용되는 값이나, 정확한 제조사 공식 스펙은 각 브랜드(JJ, Mullard re-issue, Electro-Harmonix 등)마다 다르다. 실측 기반 시정수 추정은 09 측정 문서 참조.

### 2.2 Cathode Emission과 온도

열전자 방출 전류는 Richardson-Dushman 방정식에 따른다:

$$J = A_0 T^2 \exp\!\left(-\frac{W}{kT}\right)$$

여기서 $W$ (또는 $\phi$)는 캐소드 재료의 일함수(work function), $k$는 볼츠만 상수, $A_0 = 4\pi m_e e k^2 / h^3 \approx 1.20173 \times 10^6$ A·m⁻²·K⁻² 는 Richardson 상수다. 실제 금속에서는 표면 상태에 따른 보정 계수를 포함해 $A = \lambda \cdot A_0$로 쓴다. 온도가 높을수록 방출 전자 수 증가 → 가능한 플레이트 전류 증가 → 동작점 이동.

> 참고: 위 식은 Richardson(1901 classical)과 Dushman(1923 quantum)의 공동 이름으로 불리며, Britannica/ScienceDirect 표준 표기와 일치한다.

**워밍업 중 변화:**
- gm (전달 컨덕턴스): 약 15–25% 증가
- rp (플레이트 저항): 감소
- μ (증폭 계수): 소폭 변화

### 2.3 신호 레벨에 따른 동적 바이어스 이동

신호 재생 중에도 플레이트 소산(plate dissipation) $P_d = I_p \cdot V_p$가 변한다. 강한 신호가 지속되면:

1. 플레이트 전류 증가 → 플레이트 온도 상승
2. 글라스 전구 내부 온도 상승
3. 캐소드 온도 미세 변화 → gm 변화
4. 동작점 이동

**시정수:** 열적 시정수는 수십 초이므로, 청취 중 "워밍업" 후 사운드가 변하는 것이 들린다.

### 2.4 플러그인 구현

```cpp
struct ThermalState {
    double temperature = 20.0;    // 도씨
    double T_max = 850.0;         // 동작 온도
    double tau   = 30.0;          // 시정수 (초)
    double sampleRate = 44100.0;
    double coeff;

    ThermalState() {
        coeff = 1.0 - std::exp(-1.0 / (tau * sampleRate));
    }

    void tick() {
        temperature += coeff * (T_max - temperature);
    }

    double gmScale() const {
        // gm은 온도와 함께 증가 (정규화된 0~1 범위)
        double norm = (temperature - 20.0) / (T_max - 20.0);
        return 0.85 + 0.15 * norm;  // 15% 범위에서 gm 변화
    }
};
```

---

## 3. Cathode Bounce / Bias Bounce

### 3.1 원리

캐소드 바이패스 커패시터 $C_k$는 직류에 대해 개방, 교류에 대해 단락(바이패스) 역할을 한다. 그러나 **대신호 입력** 시:

1. 강한 양의 피크가 들어오면 $C_k$가 방전 (캐소드 전위 낮아짐)
2. 그리드-캐소드 전위차 증가 → 더 많은 전류 흐름
3. 강한 음의 피크가 들어오면 $C_k$가 충전 (캐소드 전위 높아짐)
4. 그리드-캐소드 전위차 감소 → 동작점이 컷오프 방향으로 이동

이 효과는 **신호 크기에 따른 동적 바이어스 이동**을 만들어 자연스러운 컴프레션과 "펀치(punch)" 느낌을 준다.

### 3.2 시정수 분석

바이어스 바운스의 시정수:

$$\tau_{bias} = R_k \cdot C_k$$

일반적인 값:
- $R_k = 1–10\,k\Omega$, $C_k = 10–100\,\mu F$
- $\tau_{bias} = 10\,ms – 1\,s$ (주파수 50–100Hz 신호에 의미 있는 영향)

**청취 효과:**
- 드럼 킥이 들어올 때 순간적으로 바이어스가 이동
- 이후 수십 ms 동안 회복하면서 미세한 "꼬리" 생성
- 이것이 "진공관 컴프레션" 느낌의 물리적 원천

### 3.3 구현 모델

```cpp
struct BiasState {
    double Vk = 0.0;          // 캐소드 전압
    double Rk, Ck;
    double sampleRate;
    double alphaCharge, alphaDischarge;

    BiasState(double Rk_, double Ck_, double sr)
        : Rk(Rk_), Ck(Ck_), sampleRate(sr)
    {
        double tau = Rk * Ck;
        alphaCharge    = std::exp(-1.0 / (tau * sampleRate));
        alphaDischarge = alphaCharge;
    }

    double process(double Ip) {
        // 플레이트 전류에 따른 캐소드 전압 추적
        double target = Ip * Rk;
        Vk += (1.0 - alphaCharge) * (target - Vk);
        return Vk;
    }
};
```

---

## 4. Volterra Series — 메모리가 있는 비선형성

### 4.1 왜 Volterra인가

일반적인 `tanh` 클리퍼는 **메모리가 없는(memoryless)** 비선형 시스템이다: $y(n) = f(x(n))$.

실제 진공관 회로는 **메모리가 있는(with memory)** 비선형 시스템이다: 현재 출력이 현재 입력뿐만 아니라 **과거 입력들에도 의존**한다.

이를 수학적으로 표현하면 **Volterra 급수**:

$$y(t) = \sum_{n=1}^{N} \int_0^\infty \cdots \int_0^\infty h_n(\tau_1, \ldots, \tau_n) \prod_{k=1}^{n} x(t-\tau_k)\, d\tau_1 \cdots d\tau_n$$

**이론적 근거 (Boyd & Chua, 1985):** 시불변 연속 비선형 시스템이 *fading memory* 속성을 가지면, 임의의 정확도로 유한 차수 Volterra 급수로 근사 가능하다. 이 결과가 진공관 회로처럼 "과거의 영향이 서서히 사라지는" 물리계를 Volterra로 다루는 근거다.

### 4.2 1차–3차 커널의 의미

- **1차 커널 $h_1(\tau)$:** 선형 임펄스 응답 (일반 컨볼루션)
- **2차 커널 $h_2(\tau_1, \tau_2)$:** 메모리가 있는 2차 비선형성 (짝수 배음 + 주파수 혼합)
- **3차 커널 $h_3(\tau_1, \tau_2, \tau_3)$:** 메모리가 있는 3차 비선형성 (홀수 배음 + 3음 혼변조)

진공관 단(stage)의 경우: **2차 커널이 지배적**이며, 이것이 2nd harmonic + 저주파 IMD 생성의 원천이다.

### 4.3 Volterra 커널 측정 방법

실제 하드웨어에서 커널을 측정하는 방법:

1. **Swept Sine (Farina):** 로그 처드 사인으로 각 차수의 커널을 분리
2. **Random Noise + Higher-Order Correlation:** 백색 잡음 입력 후 교차 상관
3. **Multi-tone Probing:** 여러 주파수 조합으로 특정 IMD 성분 측정

---

## 5. Power Supply Sag

### 5.1 원리

진공관 앰프의 B+ 전원은 이상적인 전압원이 아니다. 출력 트랜스포머, 정류관, 필터 커패시터가 내부 임피던스를 형성한다.

강한 신호가 들어오면:
1. 플레이트 전류 증가 → B+ 전류 증가
2. 전원 회로의 내부 임피던스에 의해 B+ 전압 강하
3. 동작점 이동 → 이득 감소, 비선형성 변화

**전원 새그(Sag) 수식:**

$$V_{B+}(t) = V_{B+,0} - I_p(t) \cdot Z_{supply}(j\omega)$$

### 5.2 Tube Rectifier vs Solid-State Rectifier

| 항목 | 진공관 정류 (GZ34/5AR4, 5U4) | 반도체 정류 |
|------|------------------------|------------|
| 내부 임피던스 | GZ34 ≈ 50–75 Ω, 5U4GB ≈ 150–175 Ω (암페어당) | < 1 Ω |
| 워밍업 Sag | 있음 (수 초간) | 없음 |
| 동적 Sag | 강함 (좋은 압감) | 약함 |
| 캐릭터 | "Squishy", 컴프레시브 | "Stiff", 리니어 |

> 주의: "수백 Ω"은 5U4(high-vacuum full-wave, ≈ 150 Ω)에 대한 단일-다이오드 근사이고, 실제 회로에서는 CT 탑(center-tap) 전원 트랜스포머의 2차 권선 저항까지 합한 "effective source resistance"가 추가된다. GZ34(5AR4)는 저 내부저항 설계라 50 Ω 수준으로 5U4보다 낮다.

**플러그인 구현:** 신호 에너지(RMS)를 추적하는 envelope follower로 B+ 전압 변화를 시뮬레이션:

```cpp
class PowerSupplySag {
    double Vb_nominal;  // 공칭 B+ 전압 (e.g., 250V)
    double Zinternal;   // 내부 임피던스 (e.g., 500 Ω)
    double tau_sag;     // 새그 시정수 (e.g., 50ms)
    double Ip_avg;      // 평균 플레이트 전류 추적
    double alpha;

public:
    PowerSupplySag(double Vb, double Z, double tau, double sr)
        : Vb_nominal(Vb), Zinternal(Z), tau_sag(tau), Ip_avg(0.0)
    {
        alpha = std::exp(-1.0 / (tau * sr));
    }

    double getVb(double Ip_current) {
        Ip_avg = alpha * Ip_avg + (1.0 - alpha) * std::abs(Ip_current);
        return Vb_nominal - Ip_avg * Zinternal;
    }
};
```

---

## 6. Capacitor Dielectric Absorption (Soakage, 유전체 흡수)

### 6.1 원리

이상적인 커패시터는 전하를 인가 즉시 저장하고 즉시 방출한다. 그러나 실제 전해 커패시터와 폴리프로필렌 커패시터는 **유전체 흡수(dielectric absorption)**가 있어 전하의 일부를 느리게 방출한다.

**전기 회로 모델:**

$$C_{eff}(f) = C_0 + \sum_i \frac{C_i}{1 + j\omega R_i C_i}$$

이는 일반 커패시터 $C_0$에 여러 병렬 RC 지연 소자가 연결된 것과 같다.

### 6.2 음악적 효과

- **저역 신호가 지나간 후** 수 ms ~ 100ms 동안 미세하게 신호가 "회복"
- 음악적 표현: 베이스 노트가 끝난 직후 미세한 잔향처럼 들림
- 빈티지 장비의 "따뜻한 저역" 느낌의 일부

### 6.3 재질별 유전체 흡수율

| 커패시터 유형 | 유전체 흡수율 (DA) | 음악적 영향 |
|-------------|-------------|-----------|
| 테플론 (PTFE) | 0.01–0.02% | 거의 없음 |
| 폴리프로필렌 (PP) | 0.02–0.05% | 거의 없음 |
| 폴리스티렌 (PS) | 0.02–0.05% | 거의 없음 |
| 폴리카보네이트 (PC) | ~0.2% | 미미함 |
| 폴리에스터 (PET/Mylar) | 0.2–0.5% | 미미함 |
| 마이카 (Silver Mica) | 1–3% | 중간 |
| 세라믹 (X7R) | 1–3% | 중간 |
| 탄탈 | 2–5% | 뚜렷함 |
| 전해 (알루미늄) | 10–15% | 뚜렷함 |

> 출처: Wikipedia/Electrocube/Kemet application notes 계열 DA 표. 폴리프로필렌의 "0.001%"는 이상적 실측에서의 예외적 값으로, 산업 표준 스펙(최악치 보증)은 보통 0.02–0.05% 범위. 알루미늄 전해는 원본 문서의 "2–10%"보다 실측치가 더 높게(10%+) 나오는 경우가 일반적이므로 범위를 확장.

빈티지 장비에는 전해 커패시터가 많이 사용되므로, 에이징된 전해 커패시터의 흡수율은 더 높아진다.

---

## 7. Microphonics (마이크로포닉 효과)

### 7.1 원리

진공관은 유리 앰플 안에 매달린 금속 요소들로 이루어져 있다. 이 구조는 물리적 충격이나 음향 에너지에 의해 진동할 수 있다.

**그리드의 물리적 변위 → 그리드-캐소드 거리 변화 → 정전용량 변화 → 전류 변화**

수학적으로, 그리드-캐소드 사이에 실효 전하 $Q$가 저장되어 있다고 보면 $V_{gk} = Q/C(x)$이므로 $\Delta V_g \approx -(Q/C^2)(\partial C/\partial x)\,\Delta x$. 이를 $g_m$과 곱하면:

$$\Delta I_p = g_m \cdot \Delta V_g = -\,\frac{g_m \cdot Q}{C^2(x)} \cdot \frac{\partial C}{\partial x}\, \Delta x$$

(편의상 부호를 생략하고 절댓값으로 쓰면 $|\Delta I_p| = (g_m Q / C^2)(\partial C/\partial x)\,\Delta x$.)

여기서 $\Delta x$는 그리드의 물리적 변위다. 원래 표기 $g_m (Q/C)(\partial C/\partial x)$는 분모에 $C$가 하나만 있어 차원이 맞지 않으므로 $C^2$로 수정했다.

### 7.2 음악적 효과

- **고이득 프리앰프**에서 더 뚜렷함 (12AX7이 특히 민감)
- 저음 스피커의 진동이 같은 섀시의 프리앰프 관에 전달
- 특정 주파수에서 "울림(ringing)" 또는 "피드백" 느낌
- 일부 엔지니어는 이를 의도적으로 사용 ("tube scream")

### 7.3 플러그인 구현 (시뮬레이션)

입력 신호의 저역 성분에 비례한 미세 변조를 가한다:

```cpp
class Microphonics {
    double sensitivity;    // 마이크로포닉 민감도 (0~0.01)
    double resonantFreq;   // 기계 공진 주파수 (e.g., 120Hz)
    double Q;              // Q 팩터
    // 공진 필터 상태 변수
    double x1 = 0, x2 = 0;

public:
    double process(double input, double excitation) {
        // 저역 가진 신호에서 공진 필터 통과 → 변조 생성
        double res = runResonantFilter(excitation, resonantFreq, Q);
        return input * (1.0 + sensitivity * res);
    }
};
```

---

## 8. Heater-Cathode Leakage Modulation

### 8.1 원리

AC 히터를 사용하는 진공관에서 히터와 캐소드 사이에는 **유한한 절연 저항**과 **부유 커패시턴스**가 존재한다. 이로 인해 히터의 60Hz(또는 50Hz) AC가 캐소드에 미세하게 커플링된다.

**등가 회로:**

```
히터 (AC) ---[Rheater-cathode]--+--[Cheater-cathode]--- Cathode
                                |
                               GND
```

### 8.2 신호 레벨에 따른 변화

- 낮은 신호 레벨: 히터-캐소드 전압 차이가 작아 누설 전류 낮음
- 높은 신호 레벨: 캐소드 전위 변화가 크면 누설 전류가 더 크게 변동

**실측값 (12AX7, 히터 6.3V AC):**
- 히터-캐소드 절연 저항: 100MΩ ~ 10GΩ (새 관)
- 부유 커패시턴스: 0.5–3 pF

### 8.3 사운드 효과

- 매우 낮은 레벨의 60/50Hz 변조 → "험(hum)" 느낌
- 신호가 강할수록 변조 깊이가 미세하게 증가
- 이 효과를 재현하면 배경의 "살아있는 감"을 더할 수 있다

---

## 9. Capacitor Aging (커패시터 에이징)

### 9.1 전해 커패시터의 에이징 특성

전해 커패시터는 시간이 지남에 따라:
- **등가 직렬 저항(ESR)** 증가
- **커패시턴스** 값 변화 (보통 감소)
- **누설 전류** 증가

**ESR의 시간 의존성 (Weibull 모델):**

$$ESR(t) = ESR_0 \cdot e^{k \cdot t^{\beta}}$$

### 9.2 에이징이 사운드에 미치는 영향

- ESR 증가 → 커플링 커패시터의 저역 롤오프 주파수 상승 (저역이 얇아짐)
- 바이패스 커패시터의 ESR 증가 → 국소 부궤환 효과 변화
- 파워 서플라이 필터 커패시터 ESR 증가 → 리플 증가 → 더 많은 험과 새그

**"Broken-in" 사운드:** 빈티지 장비가 수십 년간 사용되면서 커패시터가 에이징되면, 새 장비와 다른 독특한 사운드가 만들어진다. 이것이 "빈티지 맛"의 상당 부분이다.

### 9.3 플러그인에서 에이징 파라미터화

"Age" 노브 (0~100%)로 에이징 정도를 제어:

| Age | 효과 |
|-----|------|
| 0% (새 것) | 규격 ESR, 명확한 저역 |
| 30% (약간 에이징) | ESR 약간 증가, 미세한 저역 변화 |
| 70% (빈티지) | ESR 2×~5×, 독특한 중저역 밀도 |
| 100% (극도 에이징) | 의도적 열화 영역, 불안정한 느낌 |

---

## 10. Envelope-Dependent 필터 응답

### 10.1 원리

아날로그 회로에서 신호의 레벨(엔벨로프)에 따라 비선형 소자의 동작점이 변하면, **유효 주파수 응답이 변한다**.

예를 들어 삼극관에서:
- 낮은 신호 레벨: 동작점이 선형 구간 중앙 → 평탄한 주파수 응답
- 높은 신호 레벨: 동작점이 포화 구간으로 이동 → gm 감소 → 이득 감소 → 고주파 Miller capacitance 효과 변화

### 10.2 구체적 메커니즘

Miller 커패시턴스:

$$C_{miller} = C_{gp}(1 + g_m r_p)$$

$g_m$이 신호 레벨에 따라 변하므로, $C_{miller}$도 변하고, 이것이 고주파 롤오프 주파수를 동적으로 변화시킨다.

**효과:** 강한 신호일수록 고주파가 약간 더 감쇄 → 자연스러운 "고주파 소프트닝" 효과

---

## 11. 모든 시변 효과의 타임라인 요약

```
신호 입력 순간:
  │
  ├── 즉시: 비선형 전달 함수 (Koren/tanh)
  │
  ├── 1–10ms: Cathode bounce 시작 (Ck 방전)
  │
  ├── 10–100ms: 
  │   ├── Cathode bounce 회복
  │   ├── Capacitor soakage 방출
  │   └── Power supply sag 부분 회복
  │
  ├── 100ms–1s:
  │   ├── 바이어스 포인트 완전 안정화
  │   └── Microphonic 공진 감쇄
  │
  ├── 1s–60s:
  │   └── 열적 드리프트 (gm 변화)
  │
  └── 분–시간:
      └── 장기 안정화 (에이징 효과 제외)
```

---

## 참고문헌

1. Ciuffoli, A. (2007). "Vacuum tube triode model." *DIYAudio Forums Technical Papers.*
2. Rainer, W. (2012). "Thermal modeling of vacuum tubes in audio applications." *AES 133rd Convention, Paper 8804.* *(출처 확인 필요 — 인용 정확성 재검증)*
3. Schoukens, J., & Ljung, L. (2019). "Nonlinear System Identification: A User-Oriented Roadmap." *IEEE Signal Processing Magazine*, 36(6), 28–99.
4. Boyd, S., & Chua, L. (1985). "Fading memory and the problem of approximating nonlinear operators with Volterra series." *IEEE Transactions on Circuits and Systems*, CAS-32(11), 1150–1161. DOI: 10.1109/TCS.1985.1085649.
5. Pakarinen, J., & Yeh, D. T. (2009). "A review of digital techniques for modeling vacuum-tube guitar amplifiers." *Computer Music Journal*, 33(2), 85–100.
6. Bushnell, G. (1997). "Electrolytic capacitor aging effects in audio equipment." *Journal of the Audio Engineering Society*, 45(4). *(출처 확인 필요 — AES e-Library에서 직접 확인)*
7. Zwicker, E., & Fastl, H. (2013). *Psychoacoustics: Facts and Models* (3rd ed.). Springer.
8. Richardson, O. W. (1901). "On the negative radiation from hot platinum." *Proc. Cambridge Philosophical Society*, 11, 286–295. Dushman, S. (1923). "Electron emission from metals as a function of temperature." *Phys. Rev.*, 21(6), 623–636.
9. Kemet / Electrocube / TDK Technical Notes on Dielectric Absorption (DA) in film and electrolytic capacitors. (Wikipedia "Dielectric absorption" 종합표 참조.)
10. JJ Electronic / Electro-Harmonix GZ34/5U4GB 데이터시트 — 내부저항 스펙.
