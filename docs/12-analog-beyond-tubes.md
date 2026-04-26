# 12. 진공관 너머의 아날로그 비선형성

> **연관 문서:** [01 진공관 물리](./01-vacuum-tube-physics.md) · [02 트랜스포머](./02-transformer-physics-and-distortion.md) · [03 시변 비선형성](./03-time-varying-nonlinearities.md) · [07 구현 전략](./07-implementation-strategies.md) · [11 타깃 하드웨어](./11-target-hardware-catalog.md) · [13 마스터링 기능](./13-mastering-features.md)

> **이 문서는 "아날로그 warmth"를 만드는 진공관 외의 비선형 메커니즘들을 물리·수학적으로 정리하고, 본 플러그인이 어떤 것을 MVP에 포함하고 어떤 것을 후순위로 미룰지 그 근거를 제시한다.**

---

## 12.1 서론

### 왜 이 문서가 필요한가

본 연구 문서 01–10은 진공관(tube)과 트랜스포머(transformer)의 물리에 집중해 왔다. 그러나 실제 아날로그 오디오 장비의 "따뜻함(warmth)"과 "캐릭터(character)"를 만드는 메커니즘은 진공관 하나에 그치지 않는다. 상업 플러그인 시장에서 "analog emulation"이라는 카테고리로 분류되는 제품 중 절반 이상은 사실 진공관이 아닌 다른 비선형 소자의 모델링을 중심으로 하며, 다음과 같은 대표적 범주가 존재한다:

- **Optical compressor**(LA-2A): 전광(electroluminescent)-광저항(CdS) 결합
- **FET saturation**(1176): 전압 제어 저항으로서의 FET
- **Tape saturation**(Studer, Ampex, Revox): 자기 히스테리시스 + 이동 매질
- **Diode clipping**(Tube Screamer, Boss OD): Shockley 방정식
- **Op-amp saturation**(NE5534, SSL bus comp): slew rate + 출력 clipping
- **Germanium transistor**(Fuzz Face): 열 민감 BJT
- **Bucket-brigade delay**(MN3007, Boss CE-1): switched-capacitor + companding

이 모든 메커니즘은 "아날로그 warmth"의 합법적인 원천이지만, **각자 물리가 완전히 다르며 동일한 구현 프레임워크로 재사용하기 어려운 경우가 많다.** 본 문서는 이 문제를 감사(audit) 관점에서 정리한다.

### 본 플러그인의 스코프 결정을 위한 근거 자료

이 문서의 목적은 두 가지다:

1. **지식 감사(knowledge audit):** "tube + transformer"에만 집중한 우리가 놓칠 수 있는 아날로그 현상을 체계적으로 나열.
2. **스코프 의사결정(scope decision):** 각 현상을 MVP에 포함할지, 유료 확장으로 미룰지, 별도 제품으로 분리할지, 스킵할지를 판단할 수 있는 재료 제공.

최종 결정 매트릭스는 12.9절에, MVP 스코프 확정은 12.10절에 있다.

---

## 12.2 Optical Compression (LA-2A 스타일)

### 12.2.1 회로 원리

Teletronix LA-2A(1962)는 광학 감쇠기(opto-attenuator)를 사용한 가장 유명한 컴프레서다. 핵심 소자는 **T4 photocell**이라 불리는 특수 부품으로, 다음 두 요소가 **광학적으로만** 결합된 밀폐된 모듈이다:

- **Electroluminescent panel (EL):** 인가된 AC 전압에 비례하여 발광하는 형광 패널
- **CdS photoresistor:** 빛을 받으면 저항이 감소하는 황화카드뮴(cadmium sulfide) 광저항

신호 흐름:

$$
\text{입력 신호} \;\to\; \text{사이드체인 정류} \;\to\; \text{EL 구동} \;\to\; \text{빛} \;\to\; \text{CdS 저항 변화} \;\to\; \text{감쇠 제어}
$$

오디오 신호 자체는 CdS의 저항을 통해 그라운드로 흘러나가는 분배기(voltage divider) 형태로 처리되며, 사이드체인 신호의 강도에 따라 이 "다운로드 비율"이 변한다.

### 12.2.2 물리·수학 모델

#### EL 발광 시정수

EL 패널은 인가 전압에 대해 즉각적이지 않고, 발광 강도가 지수적으로 증가한다:

$$L(t) = L_\infty\left(1 - e^{-t/\tau_\text{EL}}\right), \quad \tau_\text{EL} \approx 10\,\text{ms}$$

이것이 LA-2A의 "느린 attack"의 한 원천이다. 어떤 프리셋으로도 1ms attack은 물리적으로 불가능하다.

#### CdS 광감응 시정수 (2단계 release)

CdS는 LA-2A 특유의 "릴리즈 곡선"을 만든다. 빛이 사라진 후의 저항 회복은 단일 지수가 아닌 **2단계(biphasic)** 지수 합으로 잘 맞는다:

$$R(t) = R_\text{atk} + A_1\left(1 - e^{-t/\tau_1}\right) + A_2\left(1 - e^{-t/\tau_2}\right)$$

- 빠른 단계: $\tau_1 \approx 40\text{–}80\,\text{ms}$ (UA 공식 스펙: 50% 릴리즈에 약 60 ms)
- 느린 단계: $\tau_2 \approx 1\text{–}15\,\text{s}$ (UA 공식 스펙: 나머지 50%의 완전 회복에 1–15 s, 개체차에 따라 폭 넓음)
- 진폭 비율 $A_1/A_2 \approx 0.4\text{–}0.7$ (개체차 큼)

사용자가 "LA-2A는 빠르게 풀려서 펌핑이 자연스럽고, 그 다음에 오래 붙잡고 있다"고 묘사하는 것의 물리적 근원이다.

#### 입력 레벨에 대한 저항 비선형 관계

CdS의 저항 $R_\text{CdS}$과 조사 광량(illumination $E$) 사이의 관계는 멱법칙(power law)이다:

$$R_\text{CdS}(E) = R_0 \cdot E^{-\gamma}, \quad \gamma \approx 0.7\text{–}1.0$$

이것이 신호에 의존하는 **비선형 게인 감소 곡선**을 만들며, 곡선의 knee가 soft하게 자연 발생하는 이유다.

#### Frequency-dependent compression

LA-2A 사이드체인은 고역 롤오프가 있는 전대역 통과 회로이지만, **EL 자체가 저주파에서 더 강한 발광**을 하는 비선형 특성이 있다. 결과적으로 저역 신호가 고역보다 더 강하게 압축된다 — 음악적으로 베이스가 과하게 눌리지 않게 제한하는 효과(레벨이 낮으면 덜 누르고, 베이스만 있으면 EL 발광이 세서 더 누름)가 있다. 이는 모던 디지털 컴프레서에서는 잘 재현되지 않는다.

### 12.2.3 구현 접근

**1. 비대칭 2단계 envelope follower**

```text
env(t) = one_pole_attack(input_abs, tau_EL)
gr(t)  = biphasic_release(env, tau_1, tau_2, A1, A2)
```

**2. 저항 vs 입력 LUT (또는 수식)**

광학-저항 변환은 멱법칙 $R = R_0 E^{-\gamma}$로 파라미터화하거나, 실제 T4 측정 데이터에서 추출한 lookup table로 처리한다.

**3. 히스테리시스 모델 (광학 메모리 효과)**

T4 셀에는 **"memory effect"**로 알려진 현상이 있다: 긴 과거에 강하게 구동된 후에는 동일 입력에서도 저항 회복이 느려진다. 이는 CdS 결정 내 트랩 준위(trap levels)의 장시간 상수 때문이며, 현실적으로는 3차 시정수($\tau_3 \approx 10\text{–}60\,\text{s}$)를 추가하거나 Preisach 유사 히스테리시스로 모델링할 수 있다.

**참고문헌:** Kemper & GuitarML opto 연구(2020), Universal Audio LA-2A 리버스 엔지니어링 자료, Stanford CCRMA Smith 노트.

---

## 12.3 FET Saturation (1176 스타일)

### 12.3.1 회로 원리

Urei 1176(1967)은 **Junction FET**(original revs A–E는 매칭 페어로 사용한 **2N5457** JFET가 표준)를 **전압 제어 저항(voltage-controlled resistor, VCR)**으로 사용한 최초의 상용 컴프레서다. 입력단은 **Class A FET 증폭기 + input transformer**이고, 게인 감소는 FET의 $V_{GS}$로 $R_{DS}$를 바꾸어 분압기를 형성하는 방식이다.

### 12.3.2 물리·수학 모델

#### FET drain-source resistance

Triode region(저$V_{DS}$)에서 JFET의 drain-source 저항은:

$$R_{DS}(V_{GS}) = \frac{R_0}{1 - V_{GS}/V_P}$$

여기서 $V_P$는 pinch-off voltage, $R_0$는 $V_{GS}=0$일 때 저항. $V_{GS}$가 $V_P$에 가까워질수록 $R_{DS}$는 발산하며, 이것이 1176의 극단적 GR에서 "폭발적 찌그러짐"의 원천이다.

#### Transfer characteristic 비대칭

FET의 채널 전하 수송은 전압 극성에 따라 비대칭이며, positive half와 negative half의 클리핑 knee가 다르다:

$$
V_\text{out}(V_\text{in}) \approx 
\begin{cases}
V_\text{in} - \alpha_+ V_\text{in}^2 + \cdots & V_\text{in} > 0 \\
V_\text{in} + \alpha_- V_\text{in}^2 + \cdots & V_\text{in} < 0
\end{cases}
$$

$\alpha_+ \neq \alpha_-$로 인해 2차 배음이 발생하지만, 진공관과 달리 **3차 배음(H3)이 지배적**이다. 이는 FET가 본질적으로 대칭 소자(squared-law)에 가깝기 때문이다.

#### Attack times: 20μs–800μs

1176의 사이드체인은 거의 저항-커패시터 회로만으로 구성되어 있어 시정수가 매우 빠르다. 최속 세팅에서는 **20μs**까지 내려간다 — 이는 LA-2A보다 500배 빠르다. "all buttons in" mode(4 ratios simultaneously)에서의 공격적인 캐릭터도 이 빠른 attack + 강한 피드백 불안정성에서 기인한다.

### 12.3.3 구현 접근

FET 단의 비선형은 복잡한 ODE가 아닌 **간단한 waveshaper + 비대칭 clipper**로 충분히 모델링 가능하다:

$$y = \tanh(x) + \beta \tanh^2(x)$$

또는 좀 더 정확히:

$$y = \frac{x + \alpha x^2}{1 + \gamma |x|}$$

단 사이드체인은 실제 회로처럼 피크 검출(diode rectifier) + 매우 빠른 RC로 모델링해야 "1176 feel"이 나온다. FET 자체의 비선형성은 놀랄만큼 **정적(static)**이며 시변 요소가 적다 — 이것이 "neutral하고 punchy하지만 덜 vintage한" 1176의 성격을 만든다.

**참고문헌:** Giannoulis et al. "Digital Dynamic Range Compressor Design" (AES 2012), Urei 1176 service manual.

---

## 12.4 Tape Saturation

### 12.4.1 물리 원리

테이프 포화는 자성체의 B-H 히스테리시스(진공관 플러그인에서 이미 다룬 [02 트랜스포머](./02-transformer-physics-and-distortion.md)와 **동일한 물리적 원리**)이지만, 두 가지 차이가 있다:

1. **매질이 이동한다** — 테이프는 헤드를 지나쳐 흘러간다(7.5, 15, 30 IPS)
2. **Bias가 있다** — 120–200kHz 고주파 신호가 오디오에 중첩되어 동작점을 H-B 곡선의 선형 영역으로 이동시킴

#### Bias signal의 역할

고주파 bias는 히스테리시스 루프를 국소적으로 "평탄화"시킨다. Bias 없이는 자성체의 B-H가 심하게 S자형이라 작은 신호도 크게 왜곡되지만, bias와 함께라면 신호가 각 순간마다 루프의 선형 구간을 지나가게 되어 **저레벨에서 거의 선형**이 된다. 반면 고레벨에서는 bias조차 자성체를 포화 영역으로 밀어넣지 못하고 왜곡이 시작된다 — 이것이 테이프의 **soft knee** 특성의 물리적 원천이다.

#### Tape speed의 영향

- **7.5 IPS**: 더 강한 포화, 저역 강조, 고역 롤오프 뚜렷, noise 큼
- **15 IPS**: 프로 스튜디오 표준, 균형
- **30 IPS**: 가장 선형, 저역 롤오프(!) 발생, 고역 확장

30 IPS에서 저역이 오히려 얇아지는 것은 반직관적이지만, 짧은 파장 기록이 헤드 gap에서 간섭을 일으키기 때문이다("head bump" 현상).

### 12.4.2 수학 모델 — Bertram / Chowdhury

> *주의*: 구버전 본 문서는 "Bogdanowicz"를 표준 레퍼런스로 인용하였으나, 해당 저자의 단독 논문은 주요 오디오 DSP DB(IEEE Xplore, AES E-Library, Google Scholar)에서 즉시 검증되지 않는다. 현재 공개 검증이 가능하고 오픈 소스 구현이 있는 기준 레퍼런스는 Bertram(1994)과 Chowdhury(DAFx 2019, CHOWtape)이다.

테이프를 전체 시스템으로 모델링하려면 다음 4개의 서브시스템이 필요하다:

**1. Preisach hysteresis with motion**

Jiles-Atherton보다 테이프에 더 적합한 것은 **Preisach 모델**이다. 테이프는 다수의 독립 쌍안정 자화 요소(hysteron)의 합이며, 각 hysteron은 고유한 $(\alpha, \beta)$ switching threshold를 갖는다:

$$M(t) = \iint_{\alpha \geq \beta} \mu(\alpha, \beta) \, \hat{\gamma}_{\alpha\beta}[H(t)] \, d\alpha \, d\beta$$

여기서 $\hat{\gamma}_{\alpha\beta}$는 hysteron 연산자, $\mu(\alpha, \beta)$는 Preisach 분포 함수(재료 특성)이다.

**2. Gap loss filter**

Record/playback head의 gap 크기 $g$는 파장이 $g$에 접근하면 신호를 감쇠시킨다:

$$H_\text{gap}(f) = \frac{\sin(\pi f g / v)}{\pi f g / v} = \text{sinc}(\pi f g / v)$$

$v$는 테이프 속도, $g$는 gap width. 15 IPS + 2μm gap → 첫 null이 약 190kHz, 20kHz에서는 약 -1dB. 하지만 7.5 IPS에서는 첫 null이 95kHz로 내려와 20kHz에서 약 -4dB가 발생 → 고역 롤오프.

**3. Modulation noise (tape granularity)**

자성 입자(magnetic particle)의 불균일한 분포로 인해 신호 레벨에 비례하는 잡음이 추가된다:

$$n_\text{mod}(t) = n_0(t) \cdot (1 + \kappa |s(t)|)$$

이는 화이트 노이즈가 아닌 **신호 의존 노이즈**로, 브라운 노이즈 스펙트럼에 가깝다.

**4. Wow & flutter**

기계적 속도 변동으로 발생하는 시간 축 변조:

$$y(t) = x(t - \delta(t)), \quad \delta(t) = A_w \sin(2\pi f_w t) + A_f \sin(2\pi f_f t) + n_m(t)$$

- Wow: $f_w \approx 0.5\text{–}6\,\text{Hz}$ (capstan/reel rotation)
- Flutter: $f_f \approx 10\text{–}200\,\text{Hz}$ (motor/bearing)
- 현대 고급 기기: < 0.02% RMS

### 12.4.3 왜 Tape는 진공관과 다른가

| 특성 | 진공관 | 테이프 |
|------|--------|--------|
| 비선형 종류 | 순간적(instantaneous) | 히스테리시스(memory) |
| 배음 | 짝수(H2) 지배 | 복잡, 홀수+짝수 혼합 |
| Time dynamics | 매우 빠름 (μs) | 느림 + 매질 이동 |
| 노이즈 | 열잡음, 플리커 | 신호 의존 granularity |
| 캐릭터 | "warmth" | "glue", "smear" |

Tape는 진공관과 **완전히 독립적인 미감**을 제공하며, 서로 대체 관계가 아니다. 믹싱 엔지니어가 "tape on master bus"를 쓰는 이유는 진공관이 줄 수 없는 **시간적 smear + soft dynamic glue** 때문이다.

### 12.4.4 본 플러그인과의 관계

- **별도 제품이 타당한가?** 테이프는 물리 모델 복잡도가 진공관에 버금가거나 그 이상이며, UI도 완전히 다르다(테이프 종류, bias, speed, wow/flutter 파라미터). **별도 제품으로 스핀오프하는 것이 합리적**.
- **재사용 가능성:** 트랜스포머 JA 코드에서 Preisach로 전환할 수 있지만, motion + bias는 새로 작성해야 함. 전체 코드의 40–50% 정도만 재사용 가능.

---

## 12.5 Diode Clipping

### 12.5.1 회로 유형

Diode clipper는 기타 이펙터 역사에서 가장 널리 쓰인 비선형 회로다.

- **Symmetric pair** (2 diodes back-to-back): Boss OD-1 초기 버전, DS-1, MXR Distortion+
- **Asymmetric** (예: 1개 vs 2개): Tube Screamer TS808/TS9 — 한쪽은 diode 1개, 다른 쪽은 diode 2개 직렬 → 비대칭 clipping
- **Diode 종류:**
  - Silicon (1N914, 1N4148): 가장 hard, clipping threshold ~0.6V
  - Germanium (1N34A): soft, threshold ~0.3V, 더 낮은 게인에서도 잘 clip
  - LED: threshold 1.5–3V, headroom 큼, 유지되는 "open" 감
  - Zener, Schottky: 각기 고유한 knee

### 12.5.2 수학 모델

**Shockley 방정식:**

$$I_D = I_S\left(e^{V_D/nV_T} - 1\right)$$

- $I_S$: saturation current (~$10^{-12}\text{A}$ for silicon)
- $n$: ideality factor (1.0–2.0)
- $V_T = kT/q \approx 25.85\,\text{mV}$ at 300K

Op-amp feedback에 diode가 있는 회로(Tube Screamer의 핵심)는 implicit equation이 되어 해석적으로 풀리지 않는다:

$$V_\text{out} = V_\text{in} + R_f \cdot I_S\left(e^{V_\text{out}/nV_T} - 1\right)$$

이를 풀려면 **Lambert W 함수**를 사용한다:

$$V_\text{out} = n V_T \cdot W\left(\frac{R_f I_S}{n V_T} e^{(V_\text{in} + R_f I_S)/nV_T}\right) - R_f I_S$$

Lambert W는 표준 라이브러리로 제공되지 않지만, Newton-Raphson 2–3 iteration으로 충분히 빠르게 수렴한다. **Yeh & Smith (2008) "Simulation of the Diode Limiter in Guitar Distortion Circuits by Numerical Solution of Ordinary Differential Equations"**이 이 접근의 표준 레퍼런스다.

### 12.5.3 진공관과의 차이

- **Hard knee:** diode의 exponential I-V는 $V_T$ 근처에서 급격히 꺾여 진공관보다 훨씬 날카로운 clipping을 만든다.
- **홀수 배음 지배:** 대칭 diode pair는 H3, H5, H7을 강하게 만들어 "buzzy"한 소리를 낸다. 비대칭(TS808)은 H2도 약간 있음.
- **정적 특성:** diode는 온도에 약한 의존성($V_T$)만 있을 뿐 시변 요소가 거의 없다. 진공관의 "살아있음"이 없다.

Tube amp의 디스토션에는 **출력단 진공관의 소프트 클리핑 + 출력 트랜스포머 + 스피커**의 복합 상호작용이 있어 diode clipper와 근본적으로 다르다. 그러나 본 플러그인의 "overdrive" 옵션 모드로 **diode stage를 추가**하는 것은 비용 대비 효과가 큰 차별화 포인트가 될 수 있다.

---

## 12.6 Op-Amp 특성

### 12.6.1 Slew Rate Limiting

실제 op-amp의 출력은 어떤 유한한 속도(slew rate, V/μs)를 초과해서 변할 수 없다. 고주파 + 대신호 조건에서 이 제한이 활성화되면, 사인파가 **삼각파**에 가까운 모양으로 왜곡된다.

| Op-amp | Slew rate | 특성 |
|--------|-----------|------|
| LM324 | 0.5 V/μs | 매우 느림, "저가 믹서 특유의 고역 거침" |
| TL072 | 13 V/μs | 기타/베이스 이펙터 표준 |
| NE5534 | 13 V/μs | 스튜디오 표준, 저노이즈 |
| OPA1612 | 27 V/μs | 현대 하이엔드 |
| LME49710 | 20 V/μs | 하이엔드, 오디오 지향 |

20kHz 2Vpp 신호에 필요한 최대 slew rate: $2\pi f V_\text{max} = 2\pi \cdot 20000 \cdot 1 = 0.126\,\text{V/μs}$. 따라서 LM324조차 계산상으론 통과 가능하지만, 이는 수학적 최대이며 **과도(transient) 신호에서는 훨씬 더 빠른 slew가 필요**하다. 그래서 저가 LM324를 쓴 오래된 디지털 믹서 프리앰프가 고주파 트랜지언트를 "거칠게" 재생하는 것이다.

**구현:**

$$\left|\frac{dy}{dt}\right| \leq S$$

디지털에서는 각 샘플마다:

```python
delta_max = S / fs  # max change per sample
y[n] = y[n-1] + clamp(target[n] - y[n-1], -delta_max, +delta_max)
```

### 12.6.2 Output Swing Clipping

Op-amp의 출력은 **supply rail**에 도달하면 hard clipping된다.

- **구형 op-amp (741, 5534):** 출력이 rail보다 2–3V 안쪽에서 그침 (예: ±15V supply → ±12V swing)
- **Rail-to-rail op-amp:** 수 mV 이내까지 근접 — 다만 rail 근처에서 비선형성 급증

비대칭 supply가 흔하다 (예: +15V, -12V) → **비대칭 clipping** → 2차 배음 생성.

### 12.6.3 구현

Op-amp를 "그 자체로" 시뮬레이션하는 것은 본 플러그인의 범위 밖이지만, 빈티지 console / EQ / preamp emulation 모듈에서는 slew rate limiter를 추가하는 것이 의외로 큰 차이를 만든다. 계산 비용은 거의 0에 가깝다 (per-sample 비교 + clamp).

---

## 12.7 Germanium Transistor 비선형성

### 12.7.1 회로 유형

- **Fuzz Face** (Dallas Arbiter, 1966): 2-트랜지스터 positive feedback fuzz
- **Tone Bender** (Sola Sound, 1965): 3-트랜지스터, Jimmy Page
- **사용 트랜지스터:** AC128, OC75 (PNP germanium), 2N404, NKT275

### 12.7.2 특성

**1. 온도 의존성이 극단적**

Germanium의 bandgap은 0.67 eV로 silicon(1.12 eV)의 절반 수준이다. 결과적으로:

- Saturation current $I_S$가 온도당 2배로 증가 (silicon은 ~1.5배)
- $V_{BE}$가 온도에 따라 약 -2 mV/°C 변동 (silicon과 유사하나 더 민감)
- **실제 현장에서 자주 관찰되는 현상:** 추운 공연장에서 Fuzz Face가 "죽어"있다가, 연주 중 따뜻해지면서 gain이 올라온다.

**2. Base leakage current**

Germanium BJT는 고주파 / 고온에서 베이스 누설이 크다. 이로 인해:

- Bias point가 시간에 따라 drift
- 매 유닛마다 편차가 매우 큼 (20–50%의 $h_{FE}$ 편차)

**3. 비대칭 clipping**

진공관과 유사하게 PNP germanium은 비대칭 transfer curve를 갖는다. Fuzz의 특유한 "쫀득함"은 이 비대칭 + 피드백 루프의 결합이다.

### 12.7.3 본 플러그인과의 관계

Germanium transistor는 물리적으로 흥미롭지만, **시장 차별성이 낮다** — 이미 수많은 fuzz 플러그인이 있고, 오디오 엔지니어가 믹싱에 fuzz 색채를 넣는 일은 흔치 않다. 기타리스트 대상 제품이라면 고려할 가치 있으나, 본 플러그인(믹싱/마스터링 지향)의 우선순위에서는 낮다. **스킵 권장**.

---

## 12.8 Bucket Brigade Delay (BBD) 비선형성

### 12.8.1 회로 원리

BBD는 1969년 Sangamo-Weston이 개발한 discrete-time analog delay line이다. 대표 IC: **MN3007**(1024-stage), **MN3005**(4096-stage), **SAD-512**.

핵심은 **switched capacitor ladder**: N개의 커패시터가 직렬로 있고, 2상 clock이 매 클록마다 charge를 다음 셀로 이동시킨다. 클록 속도가 delay time을 결정한다 (typical 10–100 kHz clock → 10–100 ms delay).

**핵심 부가 회로:**

1. **Anti-aliasing low-pass filter** (입력): Clock 절반 이하로 대역 제한
2. **Compander** (NE570/571): 입력 압축 → BBD → 출력 확장. 신호 대 잡음비를 ~20dB 개선. 저레벨에서 노이즈가 "들어왔다 나갔다" 하는 breathing 효과의 원천.
3. **Reconstruction filter** (출력): Sample-and-hold 계단 노이즈 제거

### 12.8.2 사운드 특성

**1. Clock noise 누설:** 아무리 필터링해도 BBD의 클록 주파수가 출력에 약간 새어나온다. 이것이 "아날로그 코러스의 고역 쉬익"을 만듦.

**2. Companding artifacts:** 신호 dynamics에 따라 noise floor가 "호흡"한다. 특히 release 구간에서 noise가 들어왔다 나갔다 하는 현상.

**3. Aliasing:** BBD는 이산 시간 시스템이므로 입력 대역 제한이 불충분하면 aliasing 발생. 디지털보다 **의도적으로 "더러운"** 소리를 내는 이유.

**4. 시변 LFO 변조:** 아날로그 코러스/플랜저는 LFO로 클록 주파수를 변조하는데, 이때 **pitch도 같이 변함**(Doppler-like) → 이것이 BBD 코러스의 생명.

### 12.8.3 본 플러그인과의 관계

BBD는 delay effect이지 saturation effect가 아니다. 완전히 **다른 제품군**으로, 본 플러그인과 코드 공유가 거의 없다. **별도 제품 / 별도 스핀오프**가 적절.

**참고문헌:** Raffel & Smith "Practical Modeling of Bucket-Brigade Device Circuits" (DAFx 2010).

---

## 12.9 통합 vs 분리 결정 매트릭스

각 메커니즘에 대해 다음 4개 축으로 평가:

- **물리 복잡도:** 모델 구현의 과학적 난이도
- **구현 재사용성:** 기존 tube/transformer 코드베이스와 얼마나 공유 가능한가
- **시장 차별성:** 믹싱 엔지니어에게 기존 제품 대비 얼마나 새로운가
- **본 프로젝트 결정:** MVP 포함 / v1.1 / v2 / 별도 제품 / 스킵

| 메커니즘 | 물리 복잡도 | 구현 재사용성 | 시장 차별성 | 본 프로젝트 결정 |
|---------|-----------|-----------|-----------|--------------|
| **Tube** | 높음 (Koren + 시변) | (기본) | 높음 (시변 + 개체차로 차별화) | ✓ MVP 포함 |
| **Transformer** | 높음 (JA 히스테리시스) | Tube와 직접 결합 | 높음 (대부분 플러그인이 생략) | ✓ MVP 포함 |
| **Opto (LA-2A)** | 중간 (2단 RC + 광학 메모리) | 독립적 (별도 사이드체인) | 중간 (경쟁 많음) | v1.1 확장 모듈 |
| **FET sat (1176)** | 낮음 (waveshaper 수준) | 독립적 | 낮음 (포화 시장) | v2 확장 또는 스킵 |
| **Tape** | 높음 (Preisach + motion) | JA 부분 재사용 가능 | 중간 (경쟁 많지만 수요 견고) | **별도 제품 스핀오프** |
| **Diode clipping** | 낮음 (Lambert W 정도) | WDF 내 처리 가능 | 낮음 (무수히 많음) | ✓ MVP 옵션 모드 |
| **Op-amp slew rate** | 중간 | 독립적 (1-line limiter) | 낮음 | v2 확장 |
| **Germanium transistor** | 낮음 | 독립적 | 낮음 | 스킵 |
| **BBD delay** | 높음 (companding + aliasing) | 거의 0% 재사용 | 중간 | **별도 제품 스핀오프** |

### 각 결정의 근거

**MVP 포함:**
- Tube, Transformer: 본 프로젝트의 정체성. 없으면 프로젝트가 존재할 이유가 없음.
- Diode clipping (옵션): 구현 비용이 거의 공짜(Lambert W 1회 호출)인데, "overdrive mode"로 제품 범위를 확장할 수 있어 가성비 최상.

**v1.1 확장 (MVP 후 첫 update):**
- Opto compressor: 별도 사이드체인 모듈로 추가. 기존 코드와 독립적이므로 MVP 이후 점진 추가 가능. 시장 차별화는 "진공관 색채 + optical dynamics" 조합으로.

**v2 확장 또는 스킵:**
- FET saturation: 차별성이 낮다. v2에서 "console emulation 모드"의 일부로 묶어 출시.
- Op-amp slew rate: 마스터링 엔지니어가 "구형 console 느낌"을 원할 때 옵션으로.

**별도 제품 스핀오프:**
- **Tape saturation:** 물리 복잡도가 tube에 맞먹고 UI가 완전히 다름. 코드 재사용 40–50%. 별도 제품군으로 스핀오프하면 시장 확장성이 좋음.
- **BBD delay:** 완전히 다른 카테고리(delay/modulation). 공유 코드 거의 없음.

**스킵:**
- Germanium transistor: 시장이 기타리스트 중심이며 믹싱 엔지니어의 실용성 낮음. 우선순위 밖.

---

## 12.10 결론 — 본 플러그인의 MVP 스코프 확정

### 확정된 MVP 스코프

```
MVP (v1.0)
├── Tube stage           (Koren + 시변 + 확률적 개체차)  ← 필수
├── Transformer stage    (Jiles-Atherton 히스테리시스)   ← 필수
└── Diode clip (옵션)    (Lambert W, on/off toggle)      ← 저비용 고효용

v1.1 Extension
└── Opto compressor      (T4 cell emulation, 2-stage release)

v2.0 Extension
├── FET stage            (1176-style, "console emulation" mode의 일부)
└── Op-amp slew limiter  (same mode)

Separate Products (Future)
├── Tape Saturation      (Preisach + gap loss + wow/flutter)
└── BBD Chorus/Delay     (MN3007-style modulation delay)

Skipped
└── Germanium fuzz       (target market mismatch)
```

### 의사결정 원칙 정리

1. **"살아있음(livingness)"이 본 플러그인의 축이다.** Tube와 transformer는 가장 풍부한 시변 특성을 가진다. Opto도 그 축에 속한다. FET, diode, op-amp는 정적(static) 비선형으로 상대적 후순위.

2. **코드 재사용성이 높고 물리 원리가 가까운 것부터 확장한다.** Tape는 물리가 가깝지만 엔지니어링 비용이 제품을 분리할 만큼 크다.

3. **각 제품은 하나의 스토리를 한다.** "Tube + Transformer + Opto"는 "Vintage Analog Mastering"이라는 일관된 내러티브가 있다. 여기에 fuzz나 BBD를 섞으면 스토리가 흐려진다.

4. **차별화가 가능한 곳에 집중한다.** Diode clipping은 널린 기술이지만 tube와 결합하면 독특한 "overdriven tube" 사운드를 만들 수 있다. 이는 MVP에 포함시킬 만한 ROI가 있다.

### 향후 재검토 시점

- **MVP 출시 6개월 후:** Opto 모듈 수요 실측 → v1.1 확정
- **v1.1 출시 6개월 후:** FET / console emulation 모드 수요 조사
- **유저 피드백에서 "tape-like" 요구가 20% 이상 나올 경우:** Tape 제품 파일럿 시작

---

## 참고문헌 (12장 보완)

**Opto:**
- Kemper & GuitarML opto research collection (2020–2023). https://github.com/GuitarML
- Bonhoeffer, P. "Modelling the T4B cell in the LA-2A compressor." AES 135th Convention, 2013.

**FET / 1176:**
- Giannoulis, D., Massberg, M., Reiss, J. D. "Digital Dynamic Range Compressor Design — A Tutorial and Analysis." J. Audio Eng. Soc., 2012.

**Tape:**
- *참조명 확인 필요*: "Bogdanowicz" 단독 저자의 테이프 디지털 시뮬레이션 논문은 공개 DB(IEEE Xplore, Google Scholar)에서 즉시 검증되지 않음 — 인용 시 재확인 필요. 테이프 디지털 모델링의 표준 레퍼런스로는 아래를 신뢰 출처로 권장:
  - Bertram, H. N. *Theory of Magnetic Recording.* Cambridge University Press, 1994.
  - Kemp, M. J. "Analysis and Simulation of Non-Linear Audio Processes using Finite Impulse Response and Dynamic Convolution." AES 106th Convention, 1999.
  - Holters, M., Zölzer, U. "Circuit simulation with inductors and transformers based on the Jiles-Atherton model of magnetization." DAFx 2016.
  - Chowdhury, J. "Real-time Physical Modelling for Analog Tape Machines." DAFx 2019 — 현재 가장 포괄적인 오픈 소스 레퍼런스(CHOWtape 구현 포함).

**Diode:**
- Yeh, D. T., Smith, J. O. "Simulation of the Diode Limiter in Guitar Distortion Circuits by Numerical Solution of Ordinary Differential Equations." DAFx 2008.
- Macak, J., Schimmel, J. "Nonlinear Circuit Simulation using Time-Variant Filter." DAFx 2009.

**BBD:**
- Raffel, C., Smith, J. O. "Practical Modeling of Bucket-Brigade Device Circuits." DAFx 2010.
- Pekonen, J., Välimäki, V. "BBD delay line chorus/flanger modeling." J. Audio Eng. Soc. Brief, 2008.

**Op-amp slew rate:**
- Solomon, J. E. "The Monolithic Op Amp: A Tutorial Study." IEEE J. Solid-State Circuits, 1974.
- Self, D. "Small Signal Audio Design." 3rd ed., Focal Press, 2020.

---

> **다음 문서:** [13 MVP 스코프 결정](./13-mvp-scope-decision.md) — 본 장의 결정 매트릭스를 기반으로 개발 우선순위, 릴리즈 전략, 리소스 배분을 최종 정리한다.
