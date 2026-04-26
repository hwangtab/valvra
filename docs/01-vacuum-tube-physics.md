# 01. 진공관의 물리적 동작 원리

> **연관 문서:** [02 트랜스포머](./02-transformer-physics-and-distortion.md) · [03 시변 비선형성](./03-time-varying-nonlinearities.md) · [07 구현 전략](./07-implementation-strategies.md)

---

## 1. 진공관의 종류와 동작 원리

### 1.1 삼극관 (Triode)

삼극관은 **캐소드(Cathode)**, **그리드(Control Grid)**, **플레이트(Plate/Anode)** 세 전극으로 구성된다. 히터(Heater)가 캐소드를 가열하면 열전자 방출(thermionic emission)이 일어나 전자가 공간으로 방출된다. 플레이트에 양전압을 인가하면 전자가 플레이트로 끌려가며 전류가 흐른다. 그리드 전압 $V_g$는 이 전류를 제어한다.

**제어 관계:**

$$I_p = f(V_p, V_g)$$

작은 $\Delta V_g$가 큰 $\Delta I_p$를 제어하는 것이 증폭의 원리다.

### 1.2 사극관 (Tetrode)과 오극관 (Pentode)

삼극관에서 플레이트–그리드 간 밀러 커패시턴스(Miller capacitance)로 인한 고주파 불안정을 해결하기 위해 스크린 그리드(Screen Grid, G2)를 추가한 것이 **사극관**이다. 추가로 억제 그리드(Suppressor Grid, G3)를 넣어 2차 전자 방출을 억제한 것이 **오극관**이다.

| 특성 | 삼극관 | 오극관 |
|------|--------|--------|
| rp (내부 임피던스) | 낮음 (1–50kΩ) | 높음 (100kΩ–수 MΩ) |
| μ (증폭 계수) | 낮음~중간 (10–100) | 높음 (100~1000) |
| 주요 배음 성분 | 2nd 지배적 | 홀수 배음 증가 |
| 출력 임피던스 | 낮음 (트랜스포머 매칭 유리) | 높음 |
| 주요 용도 | 프리앰프, 드라이버 단 | 파워앰프 |

### 1.3 빔 사극관 (Beam Tetrode) — 6L6GC, KT88, 6V6, EL84(변종)

**빔 사극관(beam tetrode, 또는 beam power tube)**은 pentode와 같은 문제(2차 전자 방출로 인한 tetrode "kink")를 해결하되, **G3(suppressor grid)를 물리적 그리드 대신 공간 전하(space charge)로 대체**한 설계다. 원 아이디어는 1933–34년 영국 EMI의 Isaac Shoenberg·Cabot Bull·Sidney Rodda가 특허화(Marconi N40, 1935년 발표)하였고, 이후 RCA의 O. H. Schade가 beam-forming plate 구조를 정제해 1936년 4월 **RCA 6L6**으로 상업화했다 ([Wikipedia: Beam tetrode](https://en.wikipedia.org/wiki/Beam_tetrode); [Schade 1938, RCA ST-59](https://www.worldradiohistory.com/BOOKSHELF-ARH/Technology/RCA-Books/RCA-Beam-Power-Tubes-O.H.-Schade-1938-48-pages.pdf)).

**빔 형성 구조(beam-forming construction):**

1. **Beam-forming plate(빔 성형 전극):** 캐소드와 같은 전위로 접지된 금속판을 G2 바깥, 플레이트 앞에 배치한다. 이 전극이 전자 흐름을 두 개의 "시트(sheet)" 형태로 초점 집속(focus)한다.
2. **그리드 정렬(aligned grids):** G1과 G2의 와이어가 캐소드 축을 따라 **정확히 동일 각도로 겹쳐지도록 감겨(wound in register)** 있다. 이로 인해 전자들이 G2 와이어를 피해 "사이 공간"으로만 통과하며, G2에 부딪히는 전자가 줄어든다 → G2 전류 감소, 효율 증가.
3. **넓은 G2–플레이트 간격:** pentode보다 G2–플레이트 거리가 크다. 이 공간에서 전자 밀도가 높아져 **공간 전하 구름(virtual cathode)**이 형성된다.

**공간 전하가 G3 역할을 대체하는 원리:**

일반 pentode에서는 플레이트에서 튕겨 나온 2차 전자(secondary emission electrons)가 전위가 더 높은 G2로 끌려가 "tetrode kink"(저 $V_p$ 영역에서 $I_p$가 음의 기울기를 보이는 현상)를 만든다. Pentode는 G3를 캐소드 전위로 두어 2차 전자를 다시 플레이트로 되돌려 보낸다.

빔관에서는 G3 대신, **G2–플레이트 사이의 공간 전하 자체가 음전위 포텐셜 우물(potential well)**을 형성한다:

$$V(x) \approx V_{g2} - \frac{\rho(x)}{\varepsilon_0} \cdot \frac{d^2}{2}$$

전자 밀도 $\rho(x)$가 충분히 높을 때 $V(x)$의 최솟값이 캐소드 전위에 가깝게 내려가며, 이것이 2차 전자를 되돌려 보내는 가상의 G3 역할을 한다. 즉 **"virtual suppressor grid"**다.

**Pentode와 beam tetrode의 비교:**

| 항목 | True Pentode (EL34, EF86) | Beam Tetrode (6L6GC, KT88, 6V6) |
|------|---------------------------|--------------------------------|
| 2차 전자 억제 | 물리적 G3 grid | 공간 전하 potential well |
| G2 전류 | 중간 | **낮음** (beam-forming + aligned grids) |
| 플레이트 곡선 knee | 더 둥글고 점진적 | **더 예리(sharper knee)** |
| 저 $V_p$ 영역 | 부드러운 포화 | 급격한 clipping |
| 3rd harmonic | 상대적으로 많음 | 상대적으로 적음 |
| 고조파 구조 | H3, H5 풍부 | H2, H3 균형 (더 "fat") |
| 대표 사운드 | "sharper, cutting" (British) | "thicker, warm" (American) |
| 대표 제품 | Marshall (EL34), Vox (EL84) | Fender (6V6, 6L6GC), Ampeg (6L6GC/KT88) |

**SPICE 모델링 관점:** Koren의 5-파라미터 pentode 확장식은 빔관·true pentode 모두를 같은 수식 구조(plate·screen 전류 분리)로 다룰 수 있으나, **knee sharpness**를 결정하는 $K_p$ 값이 빔관에서 더 크게(예리하게) 피팅된다. 빔 형성에 의한 전류 집속은 별도 파라미터로 모델링하지 않고 $K_g$에 흡수시킨다.

---

## 2. Child-Langmuir 3/2 법칙 (공간 전하 제한 전류)

히터에서 방출된 전자들이 공간 전하(space charge)를 형성하면, 이 공간 전하가 추가적인 전자 방출을 억제한다. 이 상태에서 플레이트 전류는 플레이트 전압의 3/2 승에 비례한다:

$$I_p \propto V_p^{3/2}$$

이것이 진공관 전달 특성의 근본적 비선형성의 기원이다. $V^{3/2}$ 비선형성은 테일러 전개하면:

$$I_p = a_0 + a_1 V + a_2 V^2 + a_3 V^3 + \cdots$$

여기서 **짝수 차수 항($a_2 V^2$)이 홀수 차수 항보다 크게 나타나는 것**이 진공관 하모닉의 특징이다.

---

## 3. Koren 모델 — 정밀 진공관 SPICE 모델

Norman Koren이 1996년 *Glass Audio* 논문 "Improved vacuum-tube models for SPICE simulations"에서 발표한 모델은 SPICE 시뮬레이션의 사실상 표준(de-facto standard)으로 자리잡았다. 실제 플레이트 곡선을 낮은 $V_p$ 영역(knee)부터 컷오프(cutoff)까지 하나의 연속 함수로 정확히 피팅할 수 있다.

### 3.1 Koren 모델 방정식 (원 논문 표기)

중간 변수 $E_1$은 그리드-플레이트의 실효 전압(effective control voltage)을 나타내며, 다음과 같이 정의된다:

$$E_1 = \frac{V_p}{K_p} \cdot \ln\!\left(1 + \exp\!\left(K_p \left(\frac{1}{\mu} + \frac{V_g}{\sqrt{K_{vb} + V_p^{2}}}\right)\right)\right)$$

플레이트 전류 $I_p$는 이 $E_1$의 $E_x$ 거듭제곱으로 표현된다:

$$I_p = \frac{E_1^{\,E_x}}{K_g} \cdot \bigl(1 + \operatorname{sgn}(E_1)\bigr)$$

$(1 + \operatorname{sgn}(E_1))$ 인수는 $E_1 > 0$일 때 $2$, $E_1 < 0$일 때 $0$이 되어 컷오프 이하에서 전류를 0으로 강제한다(반도체 모델의 `MAX(0, ·)`과 동일한 효과).

**파라미터 설명:**

| 파라미터 | 의미 | 단위 | 비고 |
|----------|------|------|------|
| $\mu$ | 증폭 계수 (amplification factor) | — | 데이터시트에서 바로 읽음 |
| $E_x$ | 전류 지수 (exponent). 이상적 Child-Langmuir 모델은 $3/2$지만 실제 관은 $1.3$–$1.5$ 범위 | — | Koren의 핵심 피팅 파라미터 |
| $K_g$ | 전류 스케일 상수 (larger $K_g$ = 작은 $I_p$) | — | 데이터시트 곡선과 피팅 |
| $K_p$ | 컷오프 근방의 sharpness 제어 (예리한 knee vs. 부드러운 전이) | — | 클수록 tanh에 가까워짐 |
| $K_{vb}$ | knee 전압 스케일. 저 $V_p$ 영역의 곡률 결정 | V² | $\sqrt{K_{vb} + V_p^2}$ 형태로 사용 |

> **표기 주의:** 일부 온라인 자료는 $E_x$ 대신 $K_x$, $K_g$ 대신 $G$ 등의 기호를 사용하기도 하나, 본 문서는 원 논문(Koren 1996)과 LTspice `.subckt` 라이브러리 표기를 따른다.

### 3.2 주요 진공관별 Koren 파라미터 (실측/SPICE 라이브러리값)

아래 값들은 Koren의 원 논문, Duncan Munro의 진공관 SPICE 라이브러리, 그리고 Ayumi Nakabayashi의 피팅 데이터에서 수집한 대표값이다. 제조사·로트별로 ±10% 편차가 있다.

Koren 파라미터는 **(a) 피팅 샘플**과 **(b) 라이브러리 버전**에 따라 수 % ~ 수십 % 편차가 있다. 아래는 두 계열의 값을 병기한다:
- 열 "A" — Koren 1996 원 논문 및 데이터시트 공칭값 기준의 단순화/반올림 값 (설명용)
- 열 "B" — 공개 LTspice `Koren_Tubes.INC` 라이브러리(비선형 최소자승 피팅) 값 ([tiny-terror-simulation/Koren_Tubes.INC](https://github.com/IHorvalds/tiny-terror-simulation/blob/master/Koren_Tubes.INC))

| 진공관 | $\mu$ (A / B) | $E_x$ (A / B) | $K_g$ (A / B) | $K_p$ (A / B) | $K_{vb}$ (A / B) | 성격 |
|--------|---------------|---------------|---------------|---------------|------------------|------|
| 12AX7 (ECC83) | 100 / 100 | 1.4 / 1.4 | 1060 / 1060 | 600 / 600 | 300 / 300 | 고이득 프리앰프, H2 지배 |
| 12AU7 (ECC82) | 17 / 20.21 | 1.3 / 1.23 | 1180 / 1108.7 | 300 / 84.96 | 300 / 551.3 | 중이득, 낮은 rp |
| 12AT7 (ECC81) | 60 / 67.49 | 1.35 / 1.234 | 580 / 419.1 | 300 / 213.96 | 300 / 300 | 중간 특성 |
| 6SN7 | 20 / 21.07 | 1.4 / 1.341 | 1000 / 1446.2 | 160 / 157.81 | 300 / 179.4 | 낮은 왜곡, 부드러움 |
| 300B | 3.85 / 3.92 | 1.4 / 1.504 | 1550 / 2140.3 | 160 / 64.28 | 300 / 300 | SE 파워 삼극관, 매우 낮은 왜곡 |
| EL34 (pentode, triode-strap) | 11 / 12.02 | 1.35 / 1.169 | 650 / 353.9 | 60 / 61.11 | 24 / 29.9 | 파워 앰프, 중간 배음 |
| 6L6GC (beam, triode-strap) | 8.7 / 9.88 | 1.35 / 1.442 | 1460 / 1686.6 | 48 / 30.98 | 12 / 19.4 | 클리어, 높은 출력 |
| KT88 (beam, triode-strap) | 8.8 / 12.38 | 1.35 / 1.246 | 1500 / 340.4 | 60 / 26.48 | 16 / 36.5 | 강력한 저역 |

> 열 B 값이 일반적으로 실측 곡선과 더 잘 일치하므로 실제 시뮬레이션에서는 B 값을 우선 권장한다. 데이터시트 공칭 $\mu$(예: 12AU7=17, 12AT7=60, 6L6GC=8)는 특정 동작점의 스펙이며, 전체 $V_p$ 영역에 걸친 평균적 피팅값과 10–30 % 차이가 날 수 있다.
>
> 펜토드/빔관의 pentode-mode 모델은 $V_{g2}$(screen) 의존성과 knee curvature를 다루기 위해 Koren의 5-파라미터 pentode 확장식을 사용한다. 위 표의 EL34/6L6GC/KT88 값은 triode-strapped 등가로 환산한 값이다.

### 3.3 Koren 모델 C++ 구현

아래 구현은 §3.1 본문 수식을 1:1로 옮긴 것이다. `log1p(exp(x))`는 $x$가 커지면 $\exp$가 오버플로하므로 **LogSumExp 패턴**으로 $x > 20$ 구간을 $\ln(1+e^{x}) \approx x$로 근사한다(오차 $< 2 \times 10^{-9}$):

```cpp
#include <cmath>

// Numerically stable softplus: log(1 + exp(x))
// For x > 20, exp(x) > 4.85e8 and exp(-x) < 2.06e-9, so log(1+exp(x)) == x
// to within double precision. Avoids overflow of exp(x) for x >> 700.
inline double softplus(double x) noexcept
{
    if (x > 20.0)   return x;           // log(1 + e^x) ≈ x
    if (x < -20.0)  return std::exp(x); // log(1 + e^x) ≈ e^x  (avoid log1p of tiny)
    return std::log1p(std::exp(x));
}

// Koren triode model (Koren 1996, Glass Audio 8(5)).
//   Vp  : plate-to-cathode voltage  [V]
//   Vg  : grid-to-cathode voltage   [V]   (typically negative in class-A bias)
//   mu  : amplification factor
//   Ex  : current exponent (≈ 1.3–1.5, not the ideal 3/2)
//   Kg  : current scaling constant
//   Kp  : cutoff sharpness
//   Kvb : knee scaling (units V^2; used as sqrt(Kvb + Vp^2))
// Returns: plate current Ip [A], or 0 in the cutoff region (E1 ≤ 0).
double triodeCurrentKoren(double Vp, double Vg,
                          double mu, double Ex, double Kg,
                          double Kp, double Kvb) noexcept
{
    const double sqrtTerm = std::sqrt(Kvb + Vp * Vp);
    const double inner    = Kp * (1.0 / mu + Vg / sqrtTerm);
    const double E1       = (Vp / Kp) * softplus(inner);

    if (E1 <= 0.0) return 0.0;                        // (1 + sgn(E1))/2 == 0

    return (std::pow(E1, Ex) / Kg) * 2.0;             // (1 + sgn(E1)) == 2 here
}
```

**구현 메모:**

- `pow(E1, Ex)`는 $E_x$가 런타임 상수이므로 `std::exp(Ex * std::log(E1))`와 동일 비용이다. Hot path에서는 tube-type별 `Ex`를 캐싱해 `fastpow`를 쓰는 것도 가능하다.
- `sqrt(Kvb + Vp*Vp)` 형태는 $V_p < 0$에서도 안전하다. 원 논문의 `sqrt(Kvb/Vp + 1) * Vp` 형태는 $V_p = 0$ 특이점이 있으므로 피한다.
- 그리드 전류 영역($V_g > 0$)은 이 모델로 커버되지 않는다. §6을 참조.

---

## 4. 플레이트 특성 곡선과 부하선 (Load Line)

### 4.1 플레이트 특성 곡선

$V_g$를 고정하고 $V_p$를 변화시키며 $I_p$를 측정하면 **플레이트 특성 곡선(plate characteristic curve)**을 얻는다. 12AX7의 경우:

```
Ip (mA)
  12 |       Vg = 0V
     |      /
  10 |     /   Vg = -0.5V
     |    /  /
   8 |   /  /   Vg = -1V
     |  /  /  /
   6 | /  /  /  Vg = -2V
     |/  /  /  /
   4 |  /  /  /  Vg = -4V
     | /  /  /  /
   2 |  /  /  /  Vg = -6V
     |___________/________
   0  100 200 300 400   Vp (V)
```

### 4.2 부하선 분석

플레이트 저항 $R_L$이 있을 때:

$$V_p = V_{B+} - I_p \cdot R_L$$

이를 플레이트 특성 곡선 위에 그으면 **부하선(load line)**이 된다. 동작점(Q-point)은 부하선과 해당 $V_g$ 곡선의 교점이다.

**바이어스 포인트 선택이 하모닉 특성에 미치는 영향:**

- 동작점이 곡선의 **선형 중심부**에 있으면 → 낮은 왜곡
- 동작점이 **컷오프(cutoff) 쪽**으로 치우치면 → 홀수 배음 증가
- 동작점이 **포화(saturation) 쪽**으로 치우치면 → 짝수 배음 증가

---

## 5. 왜 진공관은 짝수 배음이 지배적인가

### 5.1 비대칭 전달 함수에서의 배음 분석

테일러 급수로 전달 함수를 전개한다. 대칭 함수 $f(-x) = -f(x)$는 홀수 항만 가지고, 비대칭 함수는 짝수 항을 포함한다.

단일 삼극관의 전달 함수는 **비대칭**이다:
- 양의 피크 (플레이트 전압 하강): $V_p$ 감소 → $I_p$ 증가, 그러나 공간 전하 포화로 증가 폭 제한
- 음의 피크 (플레이트 전압 상승): $V_p$ 증가 → $I_p$ 감소, 그러나 컷오프에 가까워지며 감소 폭 비대칭

결과적으로:

$$y(t) = a_1 x + a_2 x^2 + a_3 x^3 + \cdots$$

여기서 $a_2 \gg a_3$이므로 **2nd harmonic이 지배**한다.

### 5.2 2차 배음 생성의 수학

입력 $x = A\cos(\omega t)$에 대해:

$$a_2 x^2 = a_2 A^2 \cos^2(\omega t) = \frac{a_2 A^2}{2}(1 + \cos(2\omega t))$$

DC 오프셋 $\frac{a_2 A^2}{2}$와 **2배 주파수 성분** $\frac{a_2 A^2}{2}\cos(2\omega t)$가 생성된다.

**두 음 $\omega_1, \omega_2$ 입력 시 IMD 생성:**

$$a_2 x^2 \supset 2a_2 A_1 A_2 \cos((\omega_1 - \omega_2)t) + \cdots$$

$(\omega_1 - \omega_2)$ 주파수의 차이 성분이 생성된다. 이것이 아날로그 "두께"와 "온기"의 물리적 원천이다.

### 5.3 Push-Pull vs Single-Ended 구성

**Push-Pull 증폭기:** 두 진공관이 역위상으로 동작하며 짝수 배음이 이론적으로 상쇄된다. 전원 리플도 상쇄. 결과: 낮은 H2, 홀수 배음 상대적 증가.

**Single-Ended 증폭기:** 짝수 배음이 상쇄되지 않아 H2 지배. "더 따뜻한" 음색이 나온다. 그러나 트랜스포머 코어에 DC 전류가 흘러 자기적 포화 문제 발생.

---

## 6. Grid Current Conduction과 Positive Grid Drive

$V_g > 0$이 되면 그리드가 양전위를 가지므로 전자가 그리드에 흡수되어 **그리드 전류**가 흐른다. 이 영역은 입력 임피던스가 급격히 감소하고 전달 특성이 매우 비선형이 된다.

플러그인 구현에서 이 영역은 특히 중요하다:
- 강한 드라이브 시 그리드 전류 도통이 시작되는 "crunch" 느낌
- 그리드 전류로 인한 이전 단(source)의 부하 변화
- 그리드 누설 저항($R_k$)과의 상호작용으로 DC 오프셋 발생

---

## 7. 캐소드 자기 바이어스 (Cathode Self-Bias)

고정 바이어스(Fixed Bias) 대신 캐소드 저항($R_k$)으로 바이어스를 형성하면, $I_p \cdot R_k$에 의한 전압이 캐소드 전위를 올려 자동으로 그리드가 캐소드 대비 음전위가 된다.

**캐소드 바이패스 커패시터 ($C_k$)의 영향:**

- $C_k$가 있을 때: $R_k$가 AC적으로 바이패스 → 이득 증가, 왜곡 감소
- $C_k$가 없을 때: AC 부귀환(local negative feedback) → 이득 감소, 왜곡 감소, 출력 임피던스 감소

**시간 의존성:** $C_k$는 대신호 입력 시 충방전되며 바이어스 포인트를 이동시킨다. 이것이 [캐소드 바운스(Cathode Bounce)](./03-time-varying-nonlinearities.md#cathode-bounce)의 원인이다.

---

## 8. DC 히터 vs AC 히터

| 항목 | DC 히터 | AC 히터 |
|------|---------|---------|
| 헌 (Hum) | 없음 | 50/60Hz + 배음 발생 가능 |
| 히터–캐소드 누설 | 낮음 | 높음 (AC 신호 커플링) |
| 비용 | 정류/레귤레이션 회로 필요 | 단순 |
| 주요 용도 | 고급 장비, 마이크 프리앰프 | 일반 앰프 |

플러그인 구현에서 AC 히터 에뮬레이션은 미세한 50/60Hz 변조를 추가해 "살아있는" 느낌을 제공한다 ([03 시변 비선형성](./03-time-varying-nonlinearities.md) 참조).

---

## 9. 주요 진공관 캐릭터 비교

### 9.1 12AX7 (ECC83) — 고이득 프리앰프 삼극관
- μ = 100, rp ≈ 62.5kΩ, gm ≈ 1.6mA/V (Vp=250V, Vg=-2V, [ECC83 datasheet](https://frank.pocnet.net/sheets/084/e/ECC83.pdf))
- 2nd harmonic 지배, 따뜻하고 "크리미(creamy)"한 saturation
- Neve 1073의 입력단은 BC184 트랜지스터이고 12AX7은 아니다. 12AX7은 Fender/Marshall/Vox 프리앰프와 Telefunken V72 등 튜브 콘솔에 사용됨을 유의.
- 강한 드라이브 시 부드럽게 클리핑

### 9.2 12AU7 (ECC82) — 중이득 드라이버 삼극관
- μ = 17, rp ≈ 7.7kΩ, gm ≈ 2.2mA/V (Vp=250V, Vg=-8.5V, [ECC82 datasheet](https://frank.pocnet.net/sheets/084/e/ECC82.pdf))
- 낮은 rp 덕에 낮은 출력 임피던스
- 클리어하고 빠른 응답
- 버퍼, 드라이버 단에 적합

### 9.3 EL34 — 클래식 파워 오극관
- gm ≈ 11mA/V, 최대 출력 25W (푸시풀 스테레오 시 50W)
- Marshall JTM45, Hiwatt DR103 등에 사용
- 중역대 강조, "브리티시" 사운드
- 삼극관 접속(triode-strapped) 시 2nd harmonic 강화

### 9.4 300B — 고급 단일 삼극관 파워관
- μ = 3.85, rp ≈ 700Ω, gm ≈ 5.5mA/V (5500 µmhos, Vp=300V, [Western Electric 300B datasheet](http://www.tubebooks.org/tubedata/we300a_b.pdf))
- 매우 낮은 THD (< 1% at rated output)
- 그러나 있는 왜곡은 거의 순수 2nd harmonic
- 하이파이 SE 앰프의 대명사

---

## 10. 구현 함의 (Implementation Implications)

### 진공관 모델 선택

| 정확도 수준 | 방법 | CPU 비용 |
|-------------|------|---------|
| 낮음 | tanh soft clipper | O(1) |
| 중간 | Polynomial approximation (차수 5–7) | O(N) |
| 높음 | Koren model (direct computation) | O(1), but complex math |
| 최고 | WDF + Newton-Raphson iteration | O(iterations) |

### 바이어스 포인트 파라미터화

Koren 파라미터를 DAW에서 접근 가능한 파라미터로 매핑:
- "Character" 노브 → 동작점 이동 (μ, Kvb 변화)
- "Drive" 노브 → 입력 레벨 스케일
- "Tube Type" 선택 → Koren 파라미터 프리셋 전환

### 과샘플링 (Oversampling) 요구사항

Koren 모델의 $\ln(1 + e^x)$ 항은 고주파 앨리어싱을 유발한다. 최소 **4× 오버샘플링**, 권장 **8× 이상**. 자세한 내용은 [07 구현 전략](./07-implementation-strategies.md) 참조.

---

## 참고문헌

1. Koren, N. (1996). "Improved vacuum-tube models for SPICE simulations." *Glass Audio*, 8(5). 온라인: <https://www.normankoren.com/Audio/Tubemodspice_article.html>
2. Dempwolf, K., Holters, M., & Zölzer, U. (2011). "Physically-motivated triode model for circuit simulations." *Proc. 14th Int. Conf. Digital Audio Effects (DAFx-11)*.
3. Leach, W. M. (1995). "SPICE models for vacuum-tube amplifiers." *Journal of the Audio Engineering Society*, 43(3), 117–126.
4. Blencowe, M. (2009). *Designing Tube Preamps for Guitar and Bass*. Merlin Blencowe.
5. Jones, M. (2003). *Valve Amplifiers* (3rd ed.). Newnes.
6. AES Standard AESXXX: *Characterization of Nonlinear Audio Devices.*
7. Schade, O. H. (1938). *Beam Power Tubes* (Publication No. ST-59). RCA Tube Division. <https://www.worldradiohistory.com/BOOKSHELF-ARH/Technology/RCA-Books/RCA-Beam-Power-Tubes-O.H.-Schade-1938-48-pages.pdf>
8. Wikipedia. "Beam tetrode." <https://en.wikipedia.org/wiki/Beam_tetrode> (EMI 1933–34 특허 → RCA 6L6 1936 상용화).
9. Koren, N. "Finding SPICE tube model parameters." <https://www.normankoren.com/Audio/Tube_params.html>
10. Duncan Munro. *Spice Models of Vacuum Tubes.* <http://tdsl.duncanamps.com/dcigna/tubes/spice/>
11. 데이터시트: ECC82/12AU7, ECC83/12AX7, ECC81/12AT7, 6SN7GT, EL34, 6L6GC (Frank Pocnet Tube Data, Western Electric 300B 자료). 각 §9의 각주 링크 참조.
