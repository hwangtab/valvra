# 02. 트랜스포머 왜곡의 물리 — 가장 과소평가된 색깔의 원천

> **연관 문서:** [01 진공관 물리](./01-vacuum-tube-physics.md) · [03 시변 비선형성](./03-time-varying-nonlinearities.md) · [04 임피던스 상호작용](./04-circuit-interactions-and-impedance.md)

> **핵심 주장:** 대부분의 아날로그 에뮬레이션 플러그인이 트랜스포머를 단순한 저역/고역 통과 필터로 처리한다. 그러나 트랜스포머는 포화(saturation), 히스테리시스(hysteresis), 에디 전류(eddy current), 공진(resonance)이 복잡하게 얽힌 **독립적인 비선형 시스템**이며, 아날로그 사운드의 색깔을 결정하는 가장 중요한 요소 중 하나다.

---

## 1. 자기 회로 기초

### 1.1 B-H 곡선과 자화 (Magnetization)

트랜스포머 코어는 자성 재료로 이루어져 있다. 자기화 강도(H, A/m)와 자속 밀도(B, T)의 관계가 비선형이다:

$$B = \mu_0 \mu_r H$$

여기서 $\mu_r$은 **비투자율(relative permeability)**로, 선형 시스템에서는 상수이지만 실제 자성 재료에서는 H의 함수다.

**B-H 곡선의 특징:**
- 초기 선형 구간: 낮은 H에서 B가 선형 증가
- 무릎(knee) 구간: B의 증가율이 감소 시작
- 포화 구간: H를 아무리 늘려도 B가 거의 증가하지 않음 ($B_{sat}$)

재질별 포화 자속 밀도:

| 재질 | $B_{sat}$ (T) | 초기 $\mu_r$ | 특성 |
|------|--------------|-------------|------|
| Silicon Steel (3% Si, GOSS) | 1.8–2.0 | 500–7000 | 저가형, 파워 트랜스포머 |
| Permalloy 80 (≈80% Ni, 4.5% Mo) | 0.6–0.8 | 20,000–100,000 | 고급 오디오 입력·마이크 트랜스포머 |
| Mu-Metal (≈77–80% Ni, 4.5% Mo) | 0.7–0.8 | 100,000–400,000 | 차폐용, 매우 낮은 포화 |
| Amorphous (비정질 Fe-based/Co-based) | 1.2–1.5 | 10,000–30,000 | 저손실, 고급 스튜디오 장비 |
| Nanocrystalline (FINEMET 등) | 1.2 | 100,000–150,000 | 초저손실 |

> 값은 대표치이며 제조사·열처리·권선 형태에 따라 ±20 % 편차. 출처: [Permalloy (Wikipedia)](https://en.wikipedia.org/wiki/Permalloy), [Mu-metal (Wikipedia)](https://en.wikipedia.org/wiki/Mu-metal), [Magnetic Shield Corp. Mumetal TDS](https://www.magnetic-shield.com/mumetal-technical-data/) (Bsat ≈ 0.76 T, µmax 350k–500k).

### 1.2 히스테리시스 (Hysteresis) — 메모리를 가진 비선형성

자성 재료는 이력(history)을 가진다. H를 증가시키는 경로와 감소시키는 경로에서 B가 다른 값을 가진다. 이 면적이 **히스테리시스 손실**이며, 매 사이클마다 열로 변환된다.

히스테리시스는 트랜스포머의 가장 독특한 비선형성의 원천이다:
- 입력 신호의 파형 진폭·히스토리에 따라 현재 응답이 달라진다
- 정상 상태에 도달하기 전 과도 응답이 수 사이클간 지속된다
- 비대칭 포화를 유발해 짝수 배음 생성 가능

---

## 2. Jiles-Atherton 히스테리시스 모델

Jiles와 Atherton이 1986년 제안한 모델은 물리적으로 의미 있는 파라미터로 히스테리시스 B-H 곡선을 기술한다.

### 2.1 모델 방정식

**무히스테리시스 자화 (Anhysteretic Magnetization):**

$$M_{an}(H_e) = M_s \left[ \coth\!\left(\frac{H_e}{a}\right) - \frac{a}{H_e} \right]$$

이는 Langevin 함수로, 자기 모멘트들이 완전히 정렬된 이상적 상태를 나타낸다. $|H_e|$가 매우 작을 때($|H_e| \ll a$)는 직접 계산 시 0/0 형태의 수치 불안정이 발생하므로, Taylor 전개로 대체한다:

$$M_{an}(H_e) \approx \frac{M_s}{3a} H_e - \frac{M_s}{45\,a^3} H_e^3 + \mathcal{O}(H_e^5)$$

도함수도 마찬가지로

$$\frac{dM_{an}}{dH_e} \approx \frac{M_s}{3a} - \frac{M_s}{15\,a^3} H_e^2 + \mathcal{O}(H_e^4)$$

로 계산한다 (일반적으로 $|H_e/a| < 10^{-4}$일 때 분기).

**유효 자기장:**

$$H_e = H + \alpha M$$

여기서 $\alpha$는 분자 내 상호작용을 나타내는 결합 계수다.

**가역/비가역 성분 분리:**

전체 자화는 가역(reversible) 성분과 비가역(irreversible) 성분의 합이다:

$$M = M_{irr} + M_{rev}, \qquad M_{rev} = c\,(M_{an} - M_{irr})$$

따라서 $M = M_{irr} + c(M_{an} - M_{irr}) = (1-c)M_{irr} + c\,M_{an}$이며, 여기서 $c \in [0, 1]$는 가역성 비율이다.

**비가역 자화 미분 방정식:**

$$\frac{dM_{irr}}{dH} = \frac{M_{an} - M_{irr}}{k\,\delta - \alpha(M_{an} - M_{irr})}$$

여기서 $\delta = \text{sign}(dH/dt)$는 자화 방향이다. 분모의 $k\delta$ 항이 이력 손실, $\alpha(M_{an}-M_{irr})$ 항이 유효장 보정이다.

**전체 자화율:**

가역/비가역 결합을 전개하면

$$\frac{dM}{dH} = (1-c)\,\frac{dM_{irr}}{dH} + c\,\frac{dM_{an}}{dH_e} \cdot \frac{dH_e}{dH}$$

이 성립한다. $H_e = H + \alpha M$이므로 $dH_e/dH = 1 + \alpha\,dM/dH$이며, 이를 대입해 정리하면 음함수 형태를 풀어 다음을 얻는다:

$$\boxed{\;\frac{dM}{dH} = \frac{(1-c)\dfrac{M_{an} - M_{irr}}{k\delta - \alpha(M_{an} - M_{irr})} + c\,\dfrac{dM_{an}}{dH_e}}{1 - \alpha c\,\dfrac{dM_{an}}{dH_e}}\;}$$

문헌(Jiles-Atherton 1986, Holters-Zölzer DAFx-16 2016)에서는 이 식을 $M$ 자체에 대한 ODE로 풀거나, $M_{irr}$을 상태 변수로 두고 $M = (1-c)M_{irr} + c\,M_{an}(H_e)$로 관측하는 두 가지 형태가 모두 쓰인다. 아래 구현은 후자(상태 변수 $M_{irr}$) 방식을 따른다.

### 2.2 5개 파라미터의 물리적 의미

| 파라미터 | 단위 | 물리적 의미 | 영향 |
|----------|------|------------|------|
| $M_s$ | A/m | 포화 자화 (saturation magnetization) | 포화 임계값 결정 |
| $a$ | A/m | 무히스테리시스 자화의 형태 | B-H 곡선 "무릎" 위치 |
| $\alpha$ | — | 분자 내 자기 결합 | 이력 폭과 초기 경사 |
| $k$ | A/m | 비가역 과정의 에너지 장벽 | 이력 면적 (손실량) |
| $c$ | — | 가역성 계수 (0~1) | 가역 자화 비율 |

### 2.3 주요 오디오 트랜스포머의 추정 JA 파라미터

| 트랜스포머 유형 | Ms (kA/m) | a (A/m) | α | k (A/m) | c |
|----------------|-----------|---------|---|---------|---|
| Marinair T3 (Neve) | 450 | 18 | 0.0009 | 25 | 0.12 |
| Jensen JT-11P-1 | 380 | 12 | 0.0007 | 18 | 0.08 |
| UTC A-12 (빈티지) | 520 | 30 | 0.0012 | 40 | 0.15 |
| Cinemag CM-DBX | 400 | 20 | 0.0010 | 28 | 0.10 |

> **출처 및 한계에 관한 주의:** 위 값은 단일 권위 있는 측정이 아니라 다음의 혼합 자료에서 역산·피팅한 **추정치**다 —
> (1) Jensen Transformers의 공개 데이터시트(JT-11P-1의 1차 인덕턴스·$B_{sat}$·저왜율 곡선),
> (2) Lundahl Transformers가 아모퍼스/니켈 코어 제품에 대해 공개한 B-H 피팅 곡선,
> (3) Holters & Zölzer (2016, DAFx-16) 논문 Table 1의 일반 오디오 코어 JA 파라미터,
> (4) 빈티지 UTC·Cinemag 기기에 대한 애호가·리페어 커뮤니티의 측정 보고.
>
> 동일 모델이어도 생산 연도·코어 로트·에이징 상태에 따라 $k, \alpha, c$는 수십 % 편차를 보이며, 특히 빈티지 UTC A-12 수치는 원본 설계 스펙이 아니라 측정 샘플의 평균에 가깝다. 정확한 에뮬레이션을 위해서는 **대상 개체(individual unit)**의 B-H 루프를 직접 측정해 5-파라미터 피팅(예: Levenberg-Marquardt)을 수행하는 것이 권장된다.

### 2.4 수치 해법 (Runge-Kutta 4)

실시간 구현을 위해 $M_{irr}$을 상태 변수로 두고 RK4로 적분한다. 관찰되는 자화는 $M = (1-c)M_{irr} + c\,M_{an}(H_e)$로 매 스텝 재구성한다.

```cpp
struct JAState {
    double Mirr    = 0.0;   // 비가역 자화 (상태 변수)
    double H       = 0.0;   // 이전 자기장
    double delta   = +1.0;  // 마지막으로 유효했던 자화 방향 sign(dH/dt)
    bool   primed  = false; // 첫 호출 여부 (delta 초기화용)
};

// Langevin 함수 L(x) = coth(x) - 1/x 및 그 도함수
// |x| 가 작을 때는 Taylor 전개로 대체하여 수치 안정성 확보
inline double langevin(double x)
{
    if (std::abs(x) < 1e-4)
        return x / 3.0 - (x * x * x) / 45.0;   // Taylor O(x^5)
    return 1.0 / std::tanh(x) - 1.0 / x;
}

inline double langevinDeriv(double x)
{
    if (std::abs(x) < 1e-4)
        return 1.0 / 3.0 - (x * x) / 15.0;     // Taylor O(x^4)
    double sh = std::sinh(x);
    return 1.0 / (x * x) - 1.0 / (sh * sh);
}

// dMirr/dH 계산. delta 는 호출자가 현재 step 기준으로 결정해서 전달.
double dMirr_dH(double H, double Mirr, double delta, const JAParams& p)
{
    double M  = (1.0 - p.c) * Mirr + p.c * p.Ms * langevin((H + p.alpha * Mirr) / p.a);
    // 위 M 은 Mirr 기반 근사; RK4 내부 단계에서는 근사적으로 Mirr + c*(Man-Mirr) 사용
    double He  = H + p.alpha * M;
    double Man = p.Ms * langevin(He / p.a);

    double diff  = Man - Mirr;
    double denom = p.k * delta - p.alpha * diff;

    // 수치 안정화: denom 이 0 에 접근하면 이력 전환점(루프 반전) 부근이다.
    // 부호를 유지하면서 작은 하한으로 클램프한다.
    constexpr double kEps = 1e-12;
    if (std::abs(denom) < kEps)
        denom = (denom < 0.0 ? -kEps : kEps);

    // wiping-out 조건: (Man - Mirr) 와 delta 가 반대 부호이면
    // 비가역 자화는 변하지 않는다 (minor loop 안쪽).
    if (diff * delta < 0.0)
        return 0.0;

    return diff / denom;
}

double rk4Step(double H_new, JAState& s, const JAParams& p)
{
    double dH = H_new - s.H;

    // delta 결정: dH=0 이면 직전 값을 유지해 sign 발산을 막는다.
    if (dH > 0.0)      s.delta = +1.0;
    else if (dH < 0.0) s.delta = -1.0;
    else if (!s.primed) s.delta = +1.0;   // 첫 호출이고 dH=0 일 때
    s.primed = true;

    double k1 = dMirr_dH(s.H,            s.Mirr,                  s.delta, p);
    double k2 = dMirr_dH(s.H + 0.5*dH,   s.Mirr + 0.5*dH*k1,      s.delta, p);
    double k3 = dMirr_dH(s.H + 0.5*dH,   s.Mirr + 0.5*dH*k2,      s.delta, p);
    double k4 = dMirr_dH(s.H + dH,       s.Mirr +     dH*k3,      s.delta, p);

    s.Mirr += (dH / 6.0) * (k1 + 2.0*k2 + 2.0*k3 + k4);
    s.H     = H_new;

    // 관측 자화 M 재구성
    double He  = H_new + p.alpha * s.Mirr;    // 1차 근사; 필요시 fixed-point 반복
    double Man = p.Ms * langevin(He / p.a);
    return (1.0 - p.c) * s.Mirr + p.c * Man;
}
```

주요 수정 포인트:

- `lastH` 전역을 제거하고, 이전 $H$와 마지막 유효 `delta`를 `JAState`에 캡슐화했다.
- `dH = 0` (DC 구간, 정지 샘플)에서 `delta`가 부호 없이 발산하던 버그를 직전 값 유지로 해결. 첫 호출에는 $+1$로 초기화한다.
- Langevin 및 그 도함수에서 $|H_e/a|$가 작을 때 Taylor 전개를 반환해 `coth(0)`의 발산을 피한다.
- 분모 $k\delta - \alpha(M_{an} - M_{irr})$이 0에 가까워질 때(이력 루프 반전점 부근) 부호 보존 클램프를 적용.
- Wiping-out 조건: 방향($\delta$)과 $M_{an}-M_{irr}$의 부호가 반대이면 비가역 성분은 변하지 않는다는 JA 모델의 물리적 제약을 명시 처리했다.

샘플 레이트가 낮아 $|dH|$가 큰 경우, RK4 한 스텝 안에서 delta 반전이 섞일 수 있으므로 오버샘플링(전형적으로 $4\times$–$8\times$)을 병행한다.

---

## 3. Preisach 모델 — 완전한 이력 메모리

JA 모델이 미분방정식으로 히스테리시스를 근사하는 데 비해, **Preisach 모델**은 모든 가능한 이력 경로를 이중 적분으로 기술한다:

$$M(t) = \iint_{\alpha \geq \beta} \mu(\alpha, \beta)\, \gamma_{\alpha\beta}[H(t)]\, d\alpha\, d\beta$$

여기서 $\gamma_{\alpha\beta}$는 상태가 +1/-1인 기초 히스테레시스 연산자(relay)이며, $\mu(\alpha, \beta)$는 Preisach 가중 함수다.

**장점:**
- 마이너 루프를 포함한 완전한 이력 묘사
- 실제 측정 데이터로 $\mu$ 함수 피팅 가능

**단점:**
- 실시간 구현 비용 O(N²) (그리드 크기 N)
- 전형적으로 오프라인 렌더링이나 고급 모드에 적합

---

## 4. 코어 손실 — 에디 전류와 히스테리시스 손실

### 4.1 Steinmetz 경험식

코어 손실은 주파수와 자속 밀도의 함수다:

$$P_{core} = k_h\, f\, B_{max}^{\,n} + k_e\, f^{2}\, B_{max}^{\,2}$$

- $k_h f B_{max}^{\,n}$: 히스테리시스 손실 (주파수에 1승 비례, Steinmetz 지수 $n \approx 1.6{-}2.2$, 재질 의존)
- $k_e f^2 B_{max}^2$: 고전적 에디 전류 손실 (주파수 제곱 비례, $B^{\,2}$가 이상적 사인파 가정)

> 보다 정밀한 non-sinusoidal 여자는 iGSE/Natural-Steinmetz 등 변형식을 사용한다 ([Steinmetz's equation (Wikipedia)](https://en.wikipedia.org/wiki/Steinmetz%27s_equation)).

결과: **고주파에서는 에디 전류 손실이 지배**하며, 이것이 트랜스포머의 자연스러운 고주파 롤오프의 원인 중 하나다.

### 4.2 Skin Effect (표피 효과)

고주파에서 와전류가 코어 단면 전체를 통과하지 못하고 표면만 사용한다. 침투 깊이(skin depth):

$$\delta_s = \sqrt{\frac{2\rho}{\omega\mu}}$$

여기서 $\rho$는 비저항, $\omega = 2\pi f$이다. 이로 인해 유효 단면적이 감소하고 고주파 특성이 더욱 저하된다.

---

## 5. 등가 회로 모델 (Equivalent Circuit)

트랜스포머의 전체 주파수 응답을 설명하는 등가 회로:

```
         Rp    Lleak_p            Lleak_s   Rs
o---[Rp]--[Lleak_p]---+---[Lleak_s]---[Rs]---o
                      |
                    [Lm]  [Rc]   (이상 변압기)
                      |
o-------------------GND-------------------o
```

- **Rp, Rs**: 1차·2차 권선 저항 (직류 저항, DCR)
- **Lleak_p, Lleak_s**: 누설 인덕턴스 (Leakage Inductance) — 고주파 롤오프
- **Lm**: 자화 인덕턴스 (Magnetizing Inductance) — 저주파 롤오프
- **Rc**: 코어 손실 저항

**WDF(Wave Digital Filter) 구현 주해:**

위 회로를 WDF로 실시간 이산화할 때는 **사다리꼴(trapezoidal/bilinear) 적분**을 가정한 표준 선형 원소 모델을 쓴다:

- **인덕터 $L$**: 포트 저항 $R_L = 2L/T_s$의 반사 계수 기반 원소. 상태 업데이트는 $b[n] = -a[n-1]$ (bilinear 한정).
- **커패시터 $C$**: 포트 저항 $R_C = T_s/(2C)$. 상태 업데이트는 $b[n] = +a[n-1]$.
- **저항 $R$**: 포트 저항 $R_R = R$, 반사 $b = 0$.

여기서 $T_s = 1/f_s$는 샘플 주기다. 사다리꼴 규칙은 2차 정확도·A-안정성을 가지나, 나이키스트 근처에서 주파수 워핑이 발생하므로 공진 피크를 가진 단(5.2)을 구현할 때는 **사전 워핑(pre-warping)** 또는 오버샘플링을 병행한다. 오일러/후방 오일러를 쓰면 포트 저항 공식이 달라져 위 값과 맞지 않으므로, 본 문서의 수식은 **사다리꼴 가정** 하에서 유효함을 명시한다.

### 5.1 저주파 롤오프

저주파에서 $L_m$의 임피던스 $j\omega L_m$이 감소하여 신호가 분로된다. -3dB 주파수:

$$f_{low} = \frac{R_{source} + R_p}{2\pi L_m}$$

일반적인 오디오 트랜스포머의 $L_m$은 수 H ~ 수십 H이므로 $f_{low}$은 10–50Hz 범위다.

### 5.2 고주파 롤오프와 공진

누설 인덕턴스($L_{leak}$)와 권선 간 부유 커패시턴스($C_{stray}$)가 **공진 회로**를 형성한다:

$$f_{res} = \frac{1}{2\pi\sqrt{L_{leak} C_{stray}}}$$

이 공진은 특정 고주파에서 **피킹(peaking)**을 만들고 이후 급격히 롤오프된다. 이것이 Neve 1073의 특유의 "에어(air)" 느낌의 원인이다.

---

## 6. DC 오프셋에 의한 비대칭 포화

단방향 자화(DC bias)는 히스테리시스 루프를 중심에서 벗어나게 하여 **비대칭 포화**를 유발한다.

### 6.1 발생 원인
- 입력 신호에 DC 오프셋이 있을 때
- Push-pull 증폭기에서 두 관의 전류 불균형
- 직결(DC-coupled) 설계에서 전단의 DC 드리프트

### 6.2 효과

```
대칭 포화 (DC=0):          비대칭 포화 (DC≠0):
     B                           B
     |    /--\                   |         /--\
     |   /    \                  |        /    \
  ---|--/------\--H           ---|-------/------\--H
     | /        \                |      /        \
      /          \               |     /          \
```

비대칭 포화는 짝수 배음, 특히 2nd harmonic을 강화한다. 이것이 일부 빈티지 콘솔 채널이 독특한 "펀치"를 가지는 이유다.

---

## 7. Inter-Winding Capacitance와 CMRR

권선 간 부유 커패시턴스($C_{IW}$)는 1차와 2차 권선 사이에 AC 경로를 제공한다.

**영향:**
- 공통 모드 신호(common mode noise)가 2차에 커플링 → CMRR 감소
- 고주파에서 1차의 신호가 직접 2차에 누설
- 차폐(shield) 권선이 있으면 이 효과를 줄일 수 있다

오디오 엔지니어링 관점에서: **인터리빙(interleaving)** 기법으로 누설 인덕턴스를 줄이면 고주파 응답이 향상되지만 $C_{IW}$가 증가하는 트레이드오프가 있다.

---

## 8. Magnetostriction — 물리적 형태 변화

자성 재료는 자화 시 물리적으로 수축·팽창한다. 이 현상을 **자기변형(magnetostriction)**이라 한다.

$$\lambda = \frac{\Delta l}{l} \propto B^2$$

**오디오에서의 영향:**
- 트랜스포머 코어가 신호 주파수에서 진동
- 기계적 공진이 특정 주파수에서 증폭됨
- "코어 울림(core ringing)" — 비선형 분수 배음 생성
- 빈티지 트랜스포머에서 저역 "임팩트" 느낌의 일부

---

## 9. 유명 트랜스포머 분석

### 9.1 Marinair (Neve 1073 출력 트랜스 LO1166 등)

- 코어: 니켈-철 합금 (초기 문헌·유저 리포트상 50/50 Ni–Fe 수준이며, 인풋 트랜스(LO1167 계열)에서 고니켈 변종 사용). 원본 스펙시트는 유실. 참고: [AMS Neve — About Marinair](https://www.ams-neve.com/about-marinair/)
- 권선: 정밀 인터리빙으로 낮은 누설 인덕턴스
- 특성: 저주파 -3 dB ≈ 20–40 Hz 수준, 고역 공진 피크는 개체차 있음
- 사운드: "Neve 색깔"의 원천 — 중역대 밀도, 부드러운 고역 포화
- 주의: "Marinair T3"는 특정 모델명이라기보다 Neve/Marinair 계열 트랜스 일반을 가리키는 관례적 표현이다. Neve 1073의 공식 출력 트랜스 부품명은 **LO1166**(마이크 입력은 **LO1167**)이다.

### 9.2 Jensen JT-11P-1

- 코어: 니켈-철 계 합금 (고순도, MuMETAL 자기차폐 캔 내장). [JT-11P-1 datasheet](https://www.jensen-transformers.com/wp-content/uploads/2014/08/jt-11p-1.pdf)
- 용도: 10k:10k (1:1) 라인 입력 트랜스
- 측정치: THD 0.025 % @ 20 Hz, +20 dBu; 대역폭 –3 dB: 0.25 Hz – 95 kHz
- 사운드: 투명하지만 살짝 "아날로그" 느낌

### 9.3 UTC A-12 (빈티지, 1950–1970년대)

- 코어: 초기 실리콘 스틸, 포화 임계값 낮음
- THD at +24dBu: 수% (의도적 saturation 영역)
- 사운드: 두꺼운 저역, 독특한 mid-range density
- "빈티지 맛"의 주원인은 낮은 포화 임계값

### 9.4 Lundahl LL1538 (모던 하이엔드 마이크 입력)

- 코어: **고투자율 mu-metal** (Lundahl 사양서 기준; 아모퍼스 코어는 LL1679AM 등 별도 AM 계열 모델). 참조: [Lundahl LL1538](https://www.orso-audio.com/products/ll1538), 3-섹션 권선 구조.
- 용도: 마이크·저레벨 입력 트랜스 (1:5 또는 1:10 승압)
- 대역폭: 10 Hz – 100 kHz (±0.3 dB, 200 Ω 소스)
- THD: 극히 낮음
- 주로 투명도 목적, 의도적 saturation 없음

> 5 Hz–150 kHz, 아모퍼스 코어는 Lundahl LL1679AM/LL1623AM 등 **아모퍼스 시리즈 출력 트랜스포머**의 대표적 스펙으로, LL1538과 혼동하지 말 것.

---

## 10. 구현 함의

### 10.1 실시간 Jiles-Atherton 구현 전략

JA 모델의 핵심 연산은 수치 적분이므로 샘플 단위 처리에서 비용이 크다.

**최적화 방법:**
1. **Look-up table (LUT):** $M_{an}(H_e)$를 사전 계산, 보간
2. **1차 오일러 적분:** 정확도 낮지만 가장 빠름
3. **RK4 + 적분 단계 제어:** 오버샘플링과 함께 사용

```cpp
// 최적화된 Langevin 근사 (LUT 기반)
double langevinApprox(double x) {
    // Padé 근사: 1/tanh(x) - 1/x ≈ x/(3 + x²/(15 + ...))
    if (std::abs(x) < 1e-6) return 0.0;
    double x2 = x * x;
    return x * (1.0 / (3.0 + x2 / (5.0 + x2/7.0)));
}
```

### 10.2 Transformer 모델 신호 체인

```
입력 신호
    │
    ├─ [1차 저주파 롤오프] ← Lm, Rp
    │
    ├─ [Jiles-Atherton Hysteresis] ← B-H 비선형
    │
    ├─ [에디 전류 손실 필터] ← 주파수 종속 손실
    │
    ├─ [Leakage inductance 롤오프]
    │
    └─ [공진 피킹 + 고주파 롤오프] ← Lleak + Cstray
         │
         출력 신호
```

### 10.3 DC 오프셋 시뮬레이션

플러그인에서 "Drive" 노브가 높아질 때 미세한 DC 오프셋을 신호에 가해 비대칭 포화를 강화하면 "vintage" 느낌을 낼 수 있다.

---

## 참고문헌

1. Jiles, D. C., & Atherton, D. L. (1986). "Theory of ferromagnetic hysteresis." *Journal of Magnetism and Magnetic Materials*, 61, 48–60. DOI: [10.1016/0304-8853(86)90066-1](https://doi.org/10.1016/0304-8853(86)90066-1).
2. Holters, M., & Zölzer, U. (2016). "Circuit simulation with inductors and transformers based on the Jiles-Atherton model of magnetization." *Proc. 19th Int. Conf. Digital Audio Effects (DAFx-16)*, Brno. (주의: 초기 인용에서 DAFx-13 표기는 오류 — 10 문서 교차검증 참조.)
3. Preisach, F. (1935). "Über die magnetische Nachwirkung." *Zeitschrift für Physik*, 94, 277–302.
4. Snelling, E. C. (1988). *Soft Ferrites: Properties and Applications* (2nd ed.). Butterworths.
5. Blencowe, M. (2012). "How to design transformers for guitar amps." *Valve Wizard Tech Notes*.
6. Whitlock, B. (2008). "Audio transformers." In *Handbook for Sound Engineers* (4th ed.). Focal Press.
7. Bogason, O., & Werner, K. J. (2022). "Modeling the Harmonic Character of Audio Transformers Using a Neural Network Framework." *Proc. 25th Int. Conf. Digital Audio Effects (DAFx-22)*.
8. Wikipedia. "Jiles–Atherton model." <https://en.wikipedia.org/wiki/Jiles%E2%80%93Atherton_model>
9. Wikipedia. "Steinmetz's equation." <https://en.wikipedia.org/wiki/Steinmetz%27s_equation>
10. AMS Neve. "About Marinair." <https://www.ams-neve.com/about-marinair/>
11. Jensen Transformers. *JT-11P-1 Datasheet.* <https://www.jensen-transformers.com/wp-content/uploads/2014/08/jt-11p-1.pdf>
12. Magnetic Shield Corporation. *MuMETAL Technical Data Sheet.* <https://www.magnetic-shield.com/mumetal-technical-data/>
13. Lundahl Transformers. LL1538 data/application notes (vendor 페이지들 — 예: <https://www.orso-audio.com/products/ll1538>).
