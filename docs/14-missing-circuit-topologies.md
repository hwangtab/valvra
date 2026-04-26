# 14. 회로 토폴로지 보완 — 빈티지 하드웨어의 핵심 구조

> **연관 문서:** [01 진공관 물리](01-vacuum-tube-physics.md) · [04 회로 상호작용과 임피던스](04-circuit-interactions-and-impedance.md) · [07 구현 전략](07-implementation-strategies.md) · [11](11-*) (별도 항목이 있을 경우)

---

## 14.1 서론

### 왜 이 토폴로지들이 중요한가

01 문서(진공관 물리)와 04 문서(회로 상호작용)는 단일 진공관 단(stage)의 동작과 그에 연결되는 주변 임피던스의 영향을 다뤘다. 그러나 실제 빈티지 오디오 하드웨어 — Fairchild 670, Manley Vari-Mu, Pultec EQP-1A, Neve 1073, Marshall JCM800, Fender Bassman, McIntosh MC275 등 — 은 **단일 common-cathode 증폭 단의 단순한 적층(cascade)이 아니다.** 이들의 특징적 사운드는 상당 부분 **회로 토폴로지(circuit topology)**, 즉 두 개 이상의 진공관이 협력하는 방식에서 비롯된다.

예컨대 Fairchild 670의 그 유명한 "부드러운" 압축감은 variable-mu 진공관 자체의 특성이기도 하지만, 차동 증폭(differential amplification)을 통한 2nd harmonic 상쇄의 결과이기도 하다. Marshall JCM800의 고역 크런치(crunch)는 EL34 자체의 왜곡만으로는 설명되지 않으며, 43% Ultra-Linear 접속의 부하선(load line) 기울기가 핵심이다. 이런 **토폴로지 수준의 세부**는 진공관 한 개만을 모델링해서는 재현되지 않는다.

### 01·04 문서에 포함되지 않은 이유

01 문서의 범위는 "단일 진공관 내부의 물리" — $I_p = k (V_g + V_p/\mu)^{3/2}$ 와 같은 Child-Langmuir 관계, grid current, space charge, heater hum — 에 국한된다. 04 문서는 그 한 진공관이 "소스 임피던스"와 "부하 임피던스" 사이에 놓였을 때 무엇이 달라지는가 — Miller effect, cathode bypass 커패시터, 결합 커패시터의 고역/저역 롤오프, 트랜스포머 부하의 비선형 AC 부하선 — 을 다룬다.

즉, 두 문서 모두 **"진공관 한 개 + 주변 수동 소자"**의 관점에 머무른다. 두 개 이상의 진공관이 **능동적으로 상호작용**하는 구조(차동, bootstrap, push-pull, cascode 등)는 별도의 수학적 틀이 필요하며, 이것이 본 14 문서의 존재 이유다.

### 이 문서의 역할

본 문서는 **실제 타깃 하드웨어 모델링**에 필요한 핵심 토폴로지를 망라한다. 각 토폴로지마다:

1. **구조(ASCII 회로도 포함)**
2. **전달 특성과 임피던스 수식**
3. **대표 하드웨어 제품**
4. **본 플러그인에서의 구현 우선순위와 WDF/MNA 구현 힌트**

를 제공한다. 가장 중요한 점은, 이 토폴로지들이 **고유의 배음 생성 패턴**을 만든다는 것이다. Long-tailed pair는 2nd harmonic을 상쇄하고, SRPP는 odd harmonic을 억제하며, Cathodyne은 플레이트/캐소드 간 위상 불일치로 고역에서 특유의 smearing을 만든다. 이 차이를 포착하지 못하면 "튜브 사운드"는 일반적인 soft-clipper와 구별되지 않는다.

---

## 14.2 Common-Cathode 증폭 단 (복습 + 심화)

### 기본 구조

가장 일반적이고 널리 쓰이는 진공관 증폭 단. 12AX7 한 쪽, 6SN7 한 쪽 등이 다음 구성으로 쓰인다.

```
                +B (e.g., +250V)
                 |
                 R_L  (e.g., 100k)
                 |
   Input ──Cc──[G]
                [ V1 ]── Plate ──Cc──► Output
                [K]
                 |
                 R_k  (e.g., 1.5k)
                 |       ║
                 ├───────╫── C_k  (bypass, e.g., 22 µF)
                 |       ║
                GND
```

### 전달 특성

**소신호 이득 (cathode 완전 bypass 가정):**
$$A_v = -\frac{\mu R_L}{r_p + R_L}$$

**Cathode 비바이패스 시:**
$$A_v = -\frac{\mu R_L}{r_p + R_L + (1+\mu) R_k}$$

여기서 $R_k$가 **local negative feedback** 역할을 하여 이득이 떨어지는 대신 선형성이 향상된다. 12AX7 기준 ($\mu=100$, $r_p=62.5$k, $R_L=100$k, $R_k=1.5$k) 계산 값:

| 모드 | 이득 $|A_v|$ |
|------|-------------|
| 완전 bypass | ≈ 61 |
| 비바이패스 | ≈ 37 |

### 전달 함수 비대칭 → H2 지배

Common-cathode 단의 핵심 특성. $V_g$가 음으로 크게 흔들리면 **cut-off** (전류 포화 불가, 하한선 고정), 양으로 흔들리면 **grid current**가 흐르기 시작하며 **soft clipping**.

이 비대칭은 Taylor 급수에서 짝수 항의 계수가 0이 아님을 의미:

$$I_p \approx I_0 + g_m v_g + \frac{1}{2} g_m' v_g^2 + \frac{1}{6} g_m'' v_g^3 + \ldots$$

$g_m' \neq 0$ 때문에 $v_g^2$ 항이 살아 있고, 이것이 2nd harmonic의 지배적 기원이 된다. 결과적으로 common-cathode 단의 특징은:

- **짝수 배음 (특히 H2) 지배**
- **홀수 배음은 상대적으로 약함**
- **압축(compression) 특성은 비대칭** — 양측 엔벨로프가 서로 다른 속도로 clip

### 변형

#### Cathode 바이패스 있음 vs 없음

- **바이패스 있음 ($C_k$ 존재):** 오디오 대역에서 $R_k$가 효과적으로 단락 → 최대 이득, H2 최대
- **바이패스 없음 ($C_k$ 제거):** 이득 감소, 왜곡 감소, 주파수 응답 평탄화
- **Classic rock/blues 기타 앰프**는 바이패스 있음으로 H2 살림
- **Hi-Fi 프리**는 종종 바이패스 제거로 선형성 우선

#### Partial bypass (부분 바이패스)

$C_k$를 작게 써서 **특정 주파수 이상에서만 bypass**가 되게 한다. 코너 주파수:

$$f_c = \frac{1}{2\pi R_k' C_k}, \quad R_k' = R_k \| \frac{r_p + R_L}{1 + \mu}$$

효과: 저역은 바이패스 없이(저 이득), 고역은 바이패스 있음(고 이득). **Fender tone stack의 "bright cap"**과 비슷한 효과로 고역 강조.

Vox AC30의 Top Boost 채널 등에서 전형적으로 쓰이는 트릭. 본 플러그인에서는 `cathode_bypass_corner` 파라미터로 노출 예정.

#### 고정 바이어스 vs 자기 바이어스

- **자기 바이어스 (self-bias, cathode bias):** $R_k$로 바이어스 — 온도 보상 자동, 간단, 안정적. 소신호 단의 표준.
- **고정 바이어스 (fixed bias):** 별도 음전원으로 grid에 직접 공급 — 효율 ↑, 출력 ↑, 온도 drift 있음. 파워 앰프 출력단의 표준.

본 문서의 나머지에서 특별한 언급이 없으면 self-bias를 가정한다.

---

## 14.3 Cathode Follower (심화)

### 기본 구조

플레이트는 +B에 직결(또는 낮은 값의 저항을 통해), 출력은 캐소드에서 취한다. 고유의 불침투성 낮은 출력 임피던스를 가진 버퍼.

```
            +B
             |
            [P]
   In ──Cc──[G]
            [ V1 ]
            [K]──────────► Output
             |
             R_k  (e.g., 22k)
             |
            GND
```

### 전달 특성

04 문서에서 정정된 바와 같이:

**출력 임피던스:**
$$Z_{out} = \frac{r_p}{\mu+1} \| R_k \approx \frac{r_p}{\mu+1}$$

**전압 이득:**
$$A_v = \frac{\mu R_k}{r_p + (\mu+1) R_k} \approx \frac{\mu}{\mu+1}$$

12AX7 기준 $\mu/(\mu+1) = 100/101 \approx 0.99$. 실제로는 $R_k$가 무한이 아니므로 0.95 ~ 0.97 정도.

### 특징

- **출력 임피던스 극저:** $r_p/(\mu+1)$. 12AX7이면 ≈ 620 Ω. 6SN7이면 ≈ 800 Ω.
- **입력 임피던스 고:** grid resistor 값 그대로 (보통 1 MΩ). Bootstrap을 걸면 더 높아진다 (§14.3.3).
- **이득 < 1:** 전압 이득은 없으나 전류 이득(버퍼) 제공.
- **왜곡 극저:** 100% cathode feedback으로 $g_m'$ 계수 억제. H2가 common-cathode 단보다 10~20 dB 낮다.

### White Cathode Follower

두 진공관을 쓴 대전류 구동 버전. 상단 관이 정전류 부하처럼 동작하고, 하단 관이 구동.

```
            +B
             |
            [P]
            [ V2 ]
            [K]─────────┬───► Output
             |          |
             R_k1       |
             |          |
             ├──────────┤
             |          |
            [P]         |
     In──Cc─[G]          
            [ V1 ]      R_load
            [K]         |
             |          |
             R_k2       |
             |          |
            GND ────────┘
```

V2의 grid가 V1의 plate swing에 연결되어, V1이 "밀 때" V2가 "당긴다". 효과:

- **Push-pull 캐소드 팔로워** — Class AB 동작 가능
- **출력 임피던스 더 낮음** — 단일 팔로워의 1/2 ~ 1/4
- **대전류 구동 능력** — 헤드폰, 저임피던스 라인 드라이브

빈티지 고급 하이파이 프리앰프, 일부 방송국 장비에서 사용.

### Bootstrapped Cathode Follower

Cathode의 AC 신호를 grid 레그(leg)의 중간 탭에 되먹임. Grid resistor가 "자기 자신이 흔들고 있는 신호에 올라타기" 때문에 AC 임피던스가 극도로 높아진다.

```
            +B
             |
            [P]
   In ──Cc──┬─[G]
            |  [ V1 ]
          R_g1 [K]───┬──► Output
            |        |
           ─┴─       |
         C_boot ─────┤
           ─┬─       |
            |        |
          R_g2       |
            |        |
           GND      R_k
                    |
                   GND
```

부트스트랩 후 유효 입력 임피던스:

$$Z_{in,boot} \approx \frac{R_{g1} + R_{g2}}{1 - A_v} \approx \frac{R_g}{1 - 0.97} \approx 30 \cdot R_g$$

1 MΩ grid resistor가 30 MΩ처럼 보이게 된다.

**Neve 1073 입력 단 구조와 유사** — Neve는 솔리드-스테이트지만 같은 원리의 bootstrapping을 쓴다. 마이크 트랜스포머 2차측에 매우 가벼운 로드를 제공하여 트랜스포머의 자연스러운 저역을 보존.

---

## 14.4 Long-Tailed Pair (LTP) — 차동 증폭기

### 구조

두 진공관의 캐소드를 공통 **tail resistor** $R_{tail}$을 통해 (또는 이상적 정전류 소스를 통해) GND 혹은 음전원으로 연결. 두 그리드가 차동 입력, 두 플레이트가 차동 출력.

```
         +B                      +B
          |                       |
         R_L                     R_L
          |                       |
  ┌──── Plate1 ──► Out−     Out+ ◄── Plate2 ────┐
  |       |                       |              |
  |      [G]                     [G]             |
In+──Cc──[V1]                   [V2]──Cc──In−   
  |      [K]                     [K]             |
  |       └───────┬───────────────┘              |
  |               |                              |
  |              R_tail  (크거나 CCS)             |
  |               |                              |
  |              V−  (음전원 or GND − offset)    |
  └──────────────────────────────────────────────┘
```

### 수식

**차동 이득 (differential gain):**

$V_{in1} - V_{in2} = V_d$, $V_{in1} + V_{in2} = 2 V_{cm}$이라 할 때, **한 쪽 플레이트** 출력의 차동 응답(싱글 엔드 출력)은 다음과 같다. 순수 차동 신호 가진에서 공통 캐소드 노드는 AC 접지처럼 동작(두 관 전류의 AC 성분이 $R_{tail}$에서 상쇄)하므로:

$$A_{d,\,single-ended} = \frac{\mu R_L}{r_p + R_L}\quad (\text{effectively independent of } R_{tail})$$

**플레이트 간 차동(balanced) 출력**으로 취하면 부호가 반대인 두 출력의 차이이므로 위 값의 **2배**:

$$A_{d,\,balanced} = \frac{2\,\mu R_L}{r_p + R_L}$$

원본 표기의 "$A_d = \mu R_L / (r_p + R_L + (1+\mu)\cdot 2 R_{tail})$"는 한 관만 구동되고 다른 관의 그리드가 AC 접지된 상황(싱글 엔드 구동, 유효 "cathode-bypassed 공통캐소드 단 + $2R_{tail}$ 캐소드 저항")에 해당하며, 순수 차동 가진에는 적용되지 않는다. 즉, **이상적 $R_{tail}\to\infty$에서도 "차동 모드"에서는 $R_{tail}$ 항이 사라진다** — 이는 본 문서의 "즉 차동 신호에 대해서는 일반 common-cathode 단과 같은 이득" 결론과 정합.

참고: 12AX7 LTP에서 유효 cathode 저항 $\approx 2/g_m$로 해석하면 실용 개략 이득은 "플레이트 저항 / (2/g_m)"로 쉽게 추정 가능 (aikenamps.com Long-Tailed Pair 유도).

**공통 모드 이득 (common-mode gain):**

두 그리드에 같은 신호가 걸리면 두 캐소드가 같이 흔들리며 $R_{tail}$을 통해 완전히 노출:

$$A_{cm} \approx \frac{-R_L}{2 R_{tail} + 2 r_p / (\mu+1)}$$

큰 $R_{tail}$에서:
$$A_{cm} \approx \frac{-R_L}{2 R_{tail}}$$

**CMRR (common-mode rejection ratio):**
$$\text{CMRR} = \left| \frac{A_d}{A_{cm}} \right| \approx \frac{\mu R_{tail}}{r_p + R_L}$$

실용적인 $R_{tail} = 47$ kΩ, 12AX7 기준 CMRR ≈ 30 dB. CCS(정전류 소스)로 tail을 대체하면 60+ dB 가능.

### 실제 하드웨어 사용

- **Fairchild 670 variable-mu 스테이지:** 6386 두 개로 LTP 구성. 차동 구동으로 2nd harmonic 상쇄, 특유의 "투명한" 압축. 자세한 모델은 §14.14 참조.
- **Manley Vari-Mu:** 5670 LTP. Fairchild의 현대적 재해석.
- **고급 콘솔 phase splitter:** Neve, Trident, API의 드라이버 단.
- **고전적 기타 앰프 phase splitter:** Marshall JTM45, Fender Twin 후기형, Ampeg SVT (ECC83/12AX7 LTP).

### Push-pull 구동 원리 — 배음 상쇄의 수학

두 입력 $v_g$와 $-v_g$에 대한 전류:

$$i_{p1} = g_m v_g + \frac{1}{2} g_m' v_g^2 + \frac{1}{6} g_m'' v_g^3$$
$$i_{p2} = g_m (-v_g) + \frac{1}{2} g_m' v_g^2 - \frac{1}{6} g_m'' v_g^3$$

차동 출력 $i_{p1} - i_{p2}$:

$$i_{d} = 2 g_m v_g + \frac{1}{3} g_m'' v_g^3 + \ldots$$

**짝수 항은 완전히 상쇄, 홀수 항만 살아남는다.** 이론적으로 H2, H4, H6 → 0. 실제로는 두 관의 완전한 매칭이 불가능하므로 잔존 H2가 −40 dB 수준에 머문다.

**결과적 사운드 특성:**
- **클린한 헤드룸** — 짝수 배음의 "따뜻함"은 줄지만 출력 헤드룸 ↑
- **밸런스드 드라이브** — 홀수 배음(H3 위주)이 남아 "angular", "aggressive"한 특성
- **클래식 마스터링 컴프의 투명감** — Fairchild, Manley 계열의 시그니처

**흥미로운 점:** 일부 빈티지 하드웨어는 **일부러 매칭을 흐트러뜨려** H2가 살아 있게 만든다. Fairchild 670의 6386 매칭 오차가 사운드의 일부로 간주되는 이유. 본 플러그인에서는 `tube_matching_tolerance` 파라미터(06 문서의 stochastic modeling과 연계)로 노출 예정.

---

## 14.5 SRPP (Shunt-Regulated Push-Pull)

### 구조

두 진공관이 세로(series)로 적층. **위쪽 진공관의 캐소드**가 **아래쪽 진공관의 플레이트**에 직결. 위쪽 관의 그리드는 아래쪽 관의 플레이트-캐소드 접점(즉, 출력점)에서 적절한 저항으로 바이어스.

```
            +B
             |
            [P]
            [ V2 ]  ← 위쪽 (active load)
            [G]────┐
            [K]────┤  ← 여기가 "shared node"
             ├─────┤
             |     └─► Output
            [P]   
     In──Cc─[G]         
            [ V1 ]  ← 아래쪽 (증폭)
            [K]
             |
             R_k
             |
            GND
```

두 관이 **서로의 동작 점을 regulate**하는 구조라서 "shunt-regulated" push-pull이라 불린다.

### 수식

**전압 이득:**
$$A_v \approx \frac{\mu R_{k,upper}}{r_p + R_{k,upper} + \ldots} \approx \mu \cdot \frac{1}{1 + \delta}$$

실용적으로 common-cathode 단과 유사한 $\mu R_L / (r_p + R_L)$이지만, 위쪽 관이 **active, signal-tracking load**로 작용하여 **부하선이 동적으로 구부러짐**. 결과적으로:

- **이론적 이득:** 상단 관이 "완전한 정전류 부하" 극한에서 $A_v \to \mu$. 실제 SRPP는 상단 관의 $r_{p,upper}$가 유한하므로 $A_v \approx \mu \cdot r_{p,upper} / (r_{p,lower} + r_{p,upper}/(1+\mu_{upper}))$, 12AX7 기준 대략 $0.7\mu \sim 0.9\mu$. 원 문서의 "$A_v \approx \mu$"는 이상적 극한에서만 성립하는 근사임에 유의 (Vogel, *How to Gain Gain*, Springer 2008/2013 참조).
- **출력 임피던스:**
$$Z_{out} \approx \frac{r_p}{2\mu} \cdot (1 + \text{matching error})$$

매우 낮은 값 (12AX7 SRPP면 ≈ 300 Ω).

### 특징

1. **Class A 강제** — 위쪽 관이 항상 전류를 "끌어당기고" 아래쪽 관이 "밀어낸다". Plate current가 0이 될 수 없음.
2. **높은 이득 + 낮은 출력 임피던스 동시 달성** — common-cathode 단의 장점(이득)과 cathode follower의 장점(낮은 Zout)을 조합.
3. **전원 잡음 제거(PSRR) 우수** — 위쪽 관이 전원 변동을 차단.
4. **왜곡 특성 독특** — Class A push-pull과 유사하게 H2 부분 상쇄, H3 잔존.

### 주의점

- **균형의 취약성** — 두 관의 $\mu$, $r_p$가 정확히 매칭되어야 최적 동작. Mismatch 시 Class A 구간이 줄어 음질 악화.
- **수렴 어려움** — MNA/WDF 시뮬레이션 시 Newton-Raphson 반복이 잘 수렴하지 않는 경우가 있다. §14.15 참조.

### 사용처

- **EAR/Yoshino 제품** — Tim de Paravicini 설계. 834P, 834L 프리앰프.
- **일부 빈티지 마이크 프리앰프** — Beyer, AKG의 1950년대 제품 일부.
- **고급 Hi-Fi 프리앰프** — Counterpoint, Audio Research 일부 모델.

---

## 14.6 Mu-Follower

### 구조

SRPP와 시각적으로 매우 유사하지만 **위쪽 관이 순수한 active load로만 기능** — 자체적인 signal path로는 관여하지 않는다. 위쪽 관은 cascode 형태 또는 constant current source (CCS)로 동작.

```
            +B
             |
            [P]
            [ V2 ]   ← 상단 (CCS 또는 cascode)
            [G]── V_bias (fixed, DC)
            [K]────┐
             ├─────┴─► Output
            [P]
     In──Cc─[G]
            [ V1 ]   ← 하단 (증폭)
            [K]
             |
             R_k
             |
            GND
```

SRPP와 결정적 차이: V2의 그리드가 **신호 경로와 분리되어 DC 바이어스로 고정**. 따라서 V2는 "어떤 신호가 와도 일정한 전류"를 유지한다.

### 특징

- **이득:** $A_v \approx \mu$ (이상적 정전류 부하 → $R_L \to \infty$의 극한)
- **출력 임피던스:** 극저, SRPP보다 더 낮음.
- **왜곡 특성:** SRPP보다 **더 직선적** — 홀수 배음 우세, 짝수 배음 억제.
- **수식:**
$$A_v = \frac{\mu r_{p,upper}}{r_{p,lower} + r_{p,upper}/(1+\mu_{upper})} \approx \mu_{lower}$$

### 사용처

고성능 하이파이 프리앰프. 자가 제작자(DIY) 커뮤니티에서 SRPP의 진화형으로 선호. 본 플러그인 MVP에는 우선순위가 낮음 (v3).

---

## 14.7 Ultra-Linear (UL) Pentode 접속

### 기본 원리

Pentode 출력관 (EL34, 6L6, KT88 등)을 사용할 때, 출력 트랜스포머 **1차 권선에 중간 탭(tap)**을 내어 그 탭을 **스크린 그리드 (G2)**에 연결. 결과적으로 G2가 플레이트 전압 변동의 일부를 추적(track)하게 된다.

```
     +B ──┬──────────────────────┐
          |                       |
          ├── Primary of OPT ────┤
          |                       |
          ├── Tap (κ = V_G2/V_P) ─┐
          |                       |
          └── Plate of V1         |
                     [P]          |
             In──────[G1]         |
                     [G2]─────────┘  (from tap)
                     [G3] (suppressor)
                     [K]
                      |
                     Cathode (bias)
```

### Tap 비율 $\kappa$

- $\kappa = 0\%$: G2가 +B에 직결 → 순수 **Pentode**
- $\kappa = 100\%$: G2가 Plate에 직결 → 순수 **Triode-strapped**
- $\kappa = 20\text{-}50\%$: **Ultra-Linear** 영역

동적 관계:
$$V_{G2}(t) = V_{G2,DC} + \kappa \cdot v_P(t)$$

여기서 $v_P(t)$는 플레이트 전압의 AC 성분.

### 효과

Pentode의 "공장 특성곡선"은 $V_P$가 큰 값일 때 거의 수평(포화) → 왜곡 많음. Triode의 특성곡선은 대각선(더 선형) → 왜곡 적음. UL은 이 중간:

- **Triode의 선형성 근접** — 이득은 약간 감소
- **Pentode의 출력 전력 대부분 유지** — Triode 대비 70~90% 유지
- **H3 감소, H2 약간 증가** — 사운드가 "부드러워지면서도 파워 유지"

### 대표 제품별 κ

| 하드웨어 | κ | 관 |
|---------|---|---|
| Marshall JCM800 | ~43% | EL34 |
| Fender 60s Bassman | ~40% | 6L6 (간혹 UL) |
| McIntosh MC275 | unique (bifilar, "Unity Coupling") | KT88 |
| Dynaco ST-70 | 43% (Hafler-Keroes) | EL34 |
| Leak Stereo 20 | 43% | EL84 |

### Hafler-Keroes Patent (1951 article, US 2,710,312 issued 1955)

David Hafler와 Herbert Keroes는 1951년 *Audio Engineering* 誌에 "ultra-linear" 개념을 발표하고, 1952년 5월 20일 미국 특허 **US 2,710,312 "Ultra Linear Amplifiers"**를 출원(1955년 6월 7일 등록). 43%는 KT88 기준의 최적 탭 비율로 널리 인용되며, EL34 등 다른 출력관도 최적 κ가 40–43% 부근에 몰려 있어 Dynaco ST-70 등에서 **43% 표준**이 자리잡았다.

이보다 앞서 Alan Blumlein이 1937년 영국 특허 **GB 496,883** "distributed loading"으로 유사 개념을 특허화했으며(US 2,218,902 1938), "ultra-linear"는 Hafler-Keroes가 도입한 상업적 명칭이다.

UL 접속의 효과:

1. $g_m$의 실효 변화율 감소 → 선형화
2. $r_p$가 triode처럼 낮아짐 → damping factor 증가
3. Output 트랜스포머 1차 임피던스 요구 완화 (3-4 kΩ 수준)

### 본 플러그인에서의 모델링

Pentode 모델의 $V_{G2}$ 입력에 $\kappa \cdot V_P$를 피드백하는 간단한 구조로 구현 가능. 핵심은 특성곡선 **두 세트**(Pentode, Triode) 사이의 **보간(interpolation)**:

$$I_p^{UL}(V_g, V_p) = (1-\kappa) \cdot I_p^{Pentode}(V_g, V_p) + \kappa \cdot I_p^{Triode}(V_g, V_p)$$

이는 1차 근사이며, 실제로는 $V_{G2}$의 공간전하(space charge) 영향을 별도로 고려해야 한다 (01 문서 pentode 섹션 참조).

---

## 14.8 Triode-Strapped Pentode

### 구조

Pentode의 G2를 **플레이트에 직결** (또는 100~1000 Ω의 작은 저항으로). 결과: pentode가 3극관처럼 동작한다.

```
            +B ── R_L ──┐
                         |
                        [P]
                        [G2]──┐ (직결)
               In ─────[G1]   |
                        [G3]──┘  (G3는 보통 K에 연결)
                        [K]
                         |
                         R_k
                         |
                        GND
```

### 특징

Pentode → Triode 변환 시 (EL34, 6L6 등 대표 5극관 기준 일반 경향):

| 파라미터 | Pentode | Triode-strapped |
|---------|---------|------------------|
| 출력 전력 | 100% (기준) | 30~50% (대개 1/3 수준) |
| $\mu$ | 거의 무한($\mu_{g1-g2}$ 기준 큼) | 낮음 (EL34: ≈ 10, 6L6: ≈ 8) |
| $r_p$ | 매우 높음 (EL34 pentode 모드 ≈ 15–30 kΩ) | 낮음 (EL34 triode ≈ 1–2 kΩ) |
| $g_m$ | 기준 | 약간 더 높음 |
| H3 | 많음 | 적음 |
| H2 | 약간 | 많음 |
| 사운드 | "powerful, aggressive" | "warm, smooth" |

> 주의: Pentode의 "rp > 50 kΩ"은 매우 높은 스크린 전압에서 관측되는 극한치로, 실측 EL34 pentode $r_p$는 일반적으로 15–30 kΩ 범위다. Triode-strap 결과는 audiodesignguide.com "Pentodes connected as Triodes" (Schlangen) 및 tubes.nekhbet.com 표에서 교차 검증됨.

### 대표 제품

- **Fender Champion 600** (5F1 회로): 6V6 triode-strap은 일부 커스텀 버전에서.
- **KT88 Triode-strap 하이파이:** Audio Note, Jadis 일부 모델.
- **EL34 Triode-strap:** Vox AC30의 일부 옵션, 일부 모던 부티크 앰프의 "plex" 모드.
- **Mesa/Boogie의 "half-drop" 스위치:** 일부 모델에서 triode/pentode 전환 제공.

### 토글 가능 파라미터

본 플러그인에서는 **"Triode/Ultra-Linear/Pentode" 3단 토글**로 노출 예정. κ 값:
- Triode: 100%
- UL: 43% (디폴트) or 40%
- Pentode: 0%

연속 슬라이더로 exposure해도 직관적.

---

## 14.9 Cascode

### 구조

두 진공관 직렬. **하단**은 common-cathode (입력 그리드), **상단**은 common-grid (입력 = 하단의 플레이트, 출력 = 상단의 플레이트).

```
             +B
              |
              R_L
              |
       ┌───── Plate 2 ──► Out
       |     [ V2 ]
       |     [G]──┬── V_bias (fixed)
       |     [K]──┘        (AC grounded via C)
       |      |
       └──── Plate 1
              |
             [ V1 ]
     In ──Cc─[G]
             [K]
              |
              R_k
              |
             GND
```

### 특징

- **극도로 높은 이득 × 대역폭 곱(GBW)**: 유효 부하가 상단 common-grid 단의 출력 임피던스이므로 매우 큰 값 (≈ $r_{p2}(1 + \mu_2) + R_L$), 따라서 $|A_v|$가 단일 common-cathode 단보다 훨씬 크다.
- **Miller effect 거의 없음**: 하단 V1의 입장에서 부하(=V2의 캐소드)는 **낮은 임피던스 $\approx 1/g_{m,2}$**로 보여서 V1의 플레이트 전압이 거의 움직이지 않는다. 따라서 V1 입력에서 본 Miller 확장 계수 $(1 + |A_{v,stage1}|) \approx (1 + g_{m1}/g_{m2}) \approx 2$ 정도에 머문다. 원 문서의 "V2가 constant voltage처럼 보임"은 "V1의 플레이트 노드에서 본 임피던스가 작다"로 고쳐 읽는 것이 정확하다. 결론적으로 $C_{gp,effective}$가 플레이트 swing에 따라 부풀지 않는다.
- **매우 넓은 대역폭**: 수 MHz까지 평탄.
- **출력 임피던스는 상당히 높음**: $Z_{out} \approx r_{p2} \cdot (1 + \mu_2 R_{k1}/r_{p1}) \| R_L$. 단점이라고 할 수 있으나 측정 장비에서는 문제가 아니다.
- **왜곡 특성**: 두 관의 비선형성이 곱해지지 않고 더해진다. H3가 common-cathode 단보다 낮을 수 있음.

### 사용처

- **고감도 측정 장비**: Tektronix 오실로스코프 입력단, 일부 스펙트럼 분석기.
- **RF 튜너 입력단**: FM 튜너의 RF 증폭.
- **소수의 마이크 프리앰프**: Thermionic Culture Vulture의 일부 모드.

오디오 파워 앰프나 프리앰프에서는 드물게 쓰인다. 본 플러그인에서는 **구현 우선순위 낮음** — 오디오 신호에 특별히 기여하는 "색채"가 적기 때문.

---

## 14.10 Phase Splitter (위상 분할 회로)

Push-pull 출력단을 구동하기 위해서는 **위상이 180° 반대**인 두 신호가 필요하다. 이를 만드는 회로가 phase splitter. 역사적으로 네 가지 주요 형태가 있다.

### Cathodyne (Split-Load)

한 진공관의 플레이트와 캐소드에서 각각 출력을 취한다. $R_L$과 $R_k$를 같은 값으로 설정.

```
         +B
          |
          R_L
          |
          ├──► Out+ (Plate)
         [P]
    In ──[G]
         [V1]
         [K]
          ├──► Out− (Cathode)
          |
          R_k (= R_L)
          |
         GND
```

**이득:** 각 출력은 $\approx \mu R / (r_p + 2R + \mu R) \approx 0.95$ — 즉 **버퍼 수준의 이득**.

**장점:**
- 단일 관으로 구현, 간단, 저렴
- 위상 정확도 ≈ 180° (저주파에서)

**단점:**
- **플레이트와 캐소드의 출력 임피던스가 다름** — Plate: $R_L \| r_p$, Cathode: $r_p/(\mu+1)$. 고주파에서 두 출력이 서로 다른 속도로 롤오프.
- 드라이브 능력 낮음 — 파워 관 grid의 전류 요구를 감당 못함.
- "Concertina"라고도 불림 — §14.12 참조.

**사용처:** Marshall 18W, Fender Champ, Vox AC4 등 저출력 클래식 기타 앰프.

### Long-Tailed Pair (LTP)

§14.4 참조. **고성능 앰프의 표준**. Fender Twin, Marshall JCM800, Ampeg SVT.

**장점:**
- 두 출력의 임피던스가 같음 → 고주파 밸런스 우수
- 높은 이득
- 공통모드 제거 → 전원 잡음 억제

**단점:**
- 진공관 2개 필요
- $R_{tail}$에 큰 DC 전압 강하 → 높은 +B 필요

### Paraphase

두 진공관. 1번 관은 정상 증폭, **2번 관의 그리드는 1번 관의 플레이트에서 일부를 받아** 반전된 복사본을 만든다. 구식 설계.

```
   In ─── V1 ───── Plate1 ──► Out+
                    |
                   분압기
                    |
             V2 ── Grid2
                   Plate2 ──► Out−
```

**단점:** V1과 V2의 이득이 정확히 맞아야 balance가 유지됨. 매칭 어렵고 $\mu$ drift에 취약. 1930~40년대 앰프에서 쓰임, 현대에는 거의 사라짐.

### Floating Paraphase

Paraphase의 진화형. 두 관의 캐소드가 공통 $R_k$에 연결되어 **자가 균형(self-balancing)** 특성을 가짐. 불균형 신호가 생기면 공통 캐소드가 반대로 흔들려 보정.

Fender Bassman 초기형(5B6, 5D6), Tweed Champ 등에서 발견.

### 위상 정확도 분석

저주파(< 1 kHz)에서는 모두 ≈ 180°. 고주파에서 차이 발생:

| 토폴로지 | 위상 오차 @ 10 kHz | 진폭 오차 @ 10 kHz |
|---------|---------------------|---------------------|
| LTP | < 1° | < 0.5 dB |
| Cathodyne | 3~5° | 1~2 dB |
| Paraphase | 5~10° | 2~5 dB |
| Floating Paraphase | 2~4° | 1~2 dB |

Cathodyne의 비대칭 롤오프가 만드는 **고역 smearing**은 저출력 기타 앰프의 특유의 "brash"한 고음을 만드는 기여 요소 중 하나.

---

## 14.11 Tube Parallel (병렬 접속)

### 원리

동일 진공관 여러 개의 동일 전극을 병렬 연결. Grid → Grid, Plate → Plate, Cathode → Cathode.

### 집계 파라미터

$N$개 병렬 시:

| 파라미터 | 단일 | 병렬 $N$ |
|---------|------|---------|
| $g_m$ | $g_m$ | $N \cdot g_m$ |
| $r_p$ | $r_p$ | $r_p / N$ |
| $\mu$ | $\mu$ | $\mu$ (변화 없음) |
| $C_{gp}$ | $C_{gp}$ | $N \cdot C_{gp}$ |
| $C_{gk}$ | $C_{gk}$ | $N \cdot C_{gk}$ |

즉 **transconductance는 $N$배, plate resistance는 $1/N$배** — cathode follower나 드라이버 단에서 구동 능력 향상.

### SNR 고찰

노이즈 전력은 진공관마다 독립이므로 **$\sqrt{N}$ 배 증가**. 신호는 $N$배. 따라서:

$$\text{SNR}_{gain} = \frac{N}{\sqrt{N}} = \sqrt{N}$$

2개 병렬 → 3 dB SNR 향상. 4개 → 6 dB. 저노이즈 마이크 프리앰프 입력단에서 유용.

### 사용처

- **고전력 드라이버:** 2×12AX7 병렬로 파워 관 grid 구동 (일부 Marshall plexi 변형).
- **저노이즈 입력:** Neumann U47의 M7 캡슐 프리앰프는 EF14 병렬 구조.
- **Fender Bassman 5F6-A:** 초단 V1이 12AX7 두 섹션을 **캐스케이드(cascade, 병렬 아님)**로 쓰지만, 그 다음 단은 일부 변형에서 병렬.

본 플러그인에서는 **실제 병렬 구현 대신 $g_m$과 $r_p$의 파라미터화**로 대체 예정 (구현 복잡도 대비 사운드 기여 미미).

---

## 14.12 Concertina (Cathodyne의 동의어)

### 재차 분석

"Concertina"는 주로 영국(Marshall, Vox, Orange)에서 쓰이는 용어, "Cathodyne"은 미국(Fender, Ampeg)에서 쓰는 용어. 회로는 동일.

### 문제점 복기

1. **출력 임피던스 비대칭:**
   - Plate 측: $Z_P \approx R_L \| r_p$. 12AX7 + 100 kΩ면 ≈ 38 kΩ.
   - Cathode 측: $Z_K \approx r_p/(\mu+1) \| R_k$. 12AX7면 ≈ 620 Ω.
   - **비율 60:1!**

2. **고주파 롤오프 불일치:**
   다음 단(파워 관 grid)의 Miller 용량이 $C_M \approx (1+|A_v|) C_{gp}$로 상당히 큼 (EL34면 100+ pF). 두 출력이 이 용량을 공통으로 구동하지만, 플레이트 측의 $Z_P$가 높아서 훨씬 빨리 롤오프. 결과:
   $$f_{-3dB, plate} \approx \frac{1}{2\pi \cdot 38k \cdot 100p} = 42 \text{ kHz}$$
   $$f_{-3dB, cathode} \approx \frac{1}{2\pi \cdot 620 \cdot 100p} = 2.6 \text{ MHz}$$

   10 kHz에서 플레이트 측이 0.5 dB 낮음, 위상 5° 지연. 20 kHz에서 2 dB 낮음, 위상 15°.

3. **드라이브 능력:**
   캐소드 측은 Class AB 구동이 가능하지만 플레이트 측은 안 됨 (전류가 $R_L$로 흐르지 않고 플레이트로 "빨려야" 하는데 pentode-like 포화 발생).

### 특유의 사운드 기여

위의 고주파 불일치와 비대칭 드라이브가 **"Marshall 18W 특유의 고음 피로감"** 혹은 **"Champ의 질감"**에 기여한다는 주장. 본 플러그인에서는 phase splitter 선택 시 LTP와 Cathodyne을 스위치로 제공하여 이 차이를 사용자가 비교할 수 있게 할 것.

---

## 14.13 Interstage Transformer Coupled Stages

### 구조

두 진공관 단 사이를 **커패시터 대신 트랜스포머**로 연결.

```
         V1 Plate
           |
           Primary
           |
          GND (또는 +B return)
           
           Secondary
           |
          V2 Grid
           |
          V2 Cathode, etc.
```

### 특징

1. **DC 블로킹:** 커플링 커패시터 역할과 동일.
2. **임피던스 매칭:** 턴 비 $n$에 따라 전압/전류 변환. Low-$r_p$ 구동단 → high-$Z_{in}$ 파워단 매칭에 유리.
3. **반전/비반전 선택:** 2차 권선 극성으로 쉽게 전환 가능.
4. **Center-tap 활용 → 내장 phase splitter:** 2차에 중간 탭을 내면 push-pull 출력단을 직접 구동 가능 (별도 phase splitter 불필요).

### 주파수 응답 특성

- **저주파:** 1차 인덕턴스 $L_p$가 결정. $f_{LF} = Z_{source} / (2\pi L_p)$. 대형 인터스테이지 트랜스포머의 $L_p$가 100 H 이상이면 저역이 10 Hz 이하까지 평탄.
- **고주파:** Leakage inductance와 winding capacitance의 공진. 보통 50~100 kHz에서 $+3$ dB 봉우리 → 급격히 롤오프. 이 공진이 "프레즌스"와 "톤"에 영향.

### 트랜스포머 히스테리시스

02 문서의 출력 트랜스포머와 마찬가지로, 인터스테이지 트랜스포머도 **Jiles-Atherton 모델**로 비선형 거동을 가진다. 다만:

- 일반적으로 저전력 (< 1W)이므로 saturation까지는 잘 안 감.
- 그러나 **저주파 고레벨**에서는 코어 포화 발생 가능.
- 히스테리시스로 인한 **미세한 H3 + 트랜스포머 독특의 주파수 의존 왜곡**이 색을 더함.

### 사용처

- **McIntosh 파워 앰프:** bifilar wound interstage + UL OPT 조합.
- **일부 하이엔드 마이크 프리앰프:** Sowter, Lundahl 트랜스포머 사용. 예: Chandler Germanium, Manley Voxbox 일부 구간.
- **Western Electric 빈티지 방송 앰프:** 거의 모든 단이 트랜스포머 커플링.

### 본 플러그인에서의 우선순위

**MVP v1에 포함 (핵심).** 이유: 인터스테이지 트랜스포머는 많은 빈티지 프리앰프의 시그니처이며, 02 문서의 JA 모델을 재활용할 수 있어 추가 개발 비용이 낮다. 구현은 각 단 사이에 `InterstageTransformer` 블록을 삽입하고, 1차 인덕턴스, leakage, winding cap, JA 파라미터를 노출.

---

## 14.14 본 플러그인에서의 구현 우선순위

### 종합표

| 토폴로지 | 구현 복잡도 | 사운드 기여 | MVP 포함? | 구현 단계 |
|---------|-----------|-----------|---------|----------|
| Common-cathode | ★ | 기본 (모든 단의 기본) | ✓ 필수 | v1 |
| Cathode follower | ★★ | 중간 (버퍼·구동) | ✓ 필수 | v1 |
| Cathode bypass tuning | ★★ | 높음 (톤 형성) | ✓ 필수 | v1 |
| Long-tailed pair | ★★★ | 높음 (Fairchild 에뮬) | ✓ MVP v1.1 | v1.1 |
| Interstage transformer | ★★★★ | 높음 (핵심 색) | ✓ MVP v1 (핵심) | v1 |
| SRPP | ★★★★ | 중간 | v2 | v2 |
| Ultra-Linear | ★★★ | 중간 (파워 앰프 모드) | v2 | v2 |
| Triode-strap | ★ | 낮음 (UL과 병합 가능) | v2 (토글) | v2 |
| Mu-follower | ★★★★ | 낮음 | v3 | v3 (옵션) |
| Cascode | ★★★ | 낮음 | 스킵 | - |
| Cathodyne (phase splitter) | ★★ | 중간 | v2 | v2 |
| Parallel tubes | ★ | 낮음 ($g_m$ 스케일) | 스킵 ($\mu/g_m$ 파라미터로 대체) | - |
| Bootstrapped CF | ★★★ | 중간 (Neve 풍) | v2 | v2 |
| White CF | ★★★★ | 낮음 | v3 | v3 |

### 타깃 하드웨어 ↔ 필요 토폴로지 매핑

각 타깃 하드웨어를 모델링할 때 필요한 토폴로지 리스트:

#### Fairchild 670 (variable-mu 컴프)
- **LTP** (6386 × 2, 차동 증폭) — §14.4
- **Cathode follower** (출력 버퍼) — §14.3
- **Interstage transformer** (입력) — §14.13
- **Common-cathode** (측정 앰프) — §14.2

#### Manley Vari-Mu
- **LTP** (5670) — §14.4
- **Cathode follower** — §14.3

#### Pultec EQP-1A
- **Common-cathode** (전단 × 2) — §14.2
- **Cathode follower** (버퍼) — §14.3
- **Interstage transformer** (입출력) — §14.13
- **Passive EQ network** (별도 문서)

#### Neve 1073 (솔리드-스테이트지만 원리 참고)
- **Bootstrapped cathode follower** (진공관 버전 재현 시) — §14.3.3

#### Marshall JCM800 / Plexi
- **Common-cathode** (12AX7 다단) — §14.2
- **Partial cathode bypass** (bright cap) — §14.2
- **Cathodyne phase splitter** — §14.12
- **Ultra-Linear output** (EL34, 43%) — §14.7

#### Fender Twin / Bassman
- **Common-cathode** — §14.2
- **LTP phase splitter** (후기형) or **Floating paraphase** (초기형) — §14.10
- **Fixed bias pentode** output (6L6, 가끔 UL) — §14.7

#### McIntosh MC275
- **Common-cathode** driver
- **Interstage transformer** (bifilar) — §14.13
- **Unity coupling output** (McIntosh 고유, UL 변형) — §14.7

---

## 14.15 WDF/MNA 구현 가이드

### MNA Matrix 구조 힌트

각 토폴로지의 노드 리스트와 KCL 방정식은 다음과 같이 구성된다 (상세는 07 구현 전략 문서의 MNA 섹션 참조).

#### Common-cathode (3 노드: G, P, K)

```
nodes = [V_G, V_P, V_K]
KCL at P: (V_P - V_B)/R_L + I_p(V_G-V_K, V_P-V_K) = 0
KCL at K: -I_p + V_K/R_k = 0
V_G = input (fixed in MNA update)
```

#### LTP (4 노드: G1, G2, P1, P2, K_common)

```
nodes = [V_P1, V_P2, V_K]  (V_G1, V_G2는 입력)
KCL at P1: (V_P1 - V_B)/R_L + I_p1(V_G1-V_K, V_P1-V_K) = 0
KCL at P2: (V_P2 - V_B)/R_L + I_p2(V_G2-V_K, V_P2-V_K) = 0
KCL at K:  -I_p1 - I_p2 + (V_K - V_tail_src)/R_tail = 0
```

Jacobian 계산에 공통 $V_K$의 편미분이 중요. 두 관의 electron current가 같은 노드로 합쳐지므로 tightly coupled.

#### SRPP (3 노드: P1_out, K1=P2_bottom, K2)

```
nodes = [V_P1, V_K1, V_K2]  (V_P1이 shared node = output)
KCL at shared node (V_K1 of upper = V_P1 of lower):
    I_p,lower(V_G,in - V_gnd, V_P1 - V_gnd) = I_p,upper(V_bias - V_K1, V_B - V_K1)
```

**수렴 주의:** SRPP는 두 관의 동작이 강하게 coupled되어 있어 Newton-Raphson에서 초기 추정이 나쁘면 발산한다. 실전 팁:
- Damping 계수 0.5 ~ 0.8 사용 (표준 1.0 대신)
- 초기 추정은 DC 동작점(operating point) 근처에서 시작
- 연속적 시간 샘플에서는 직전 샘플의 해를 warm start로 활용
- Broyden's method 고려 (Jacobian 재계산 빈도 감소)

### WDF Adaptor 배치

WDF 구현 시:

- **Common-cathode:** Plate 비선형 포트 1개, 나머지는 linear adaptor tree.
- **Cathode follower:** Cathode가 비선형 포트. $R_k$와 병렬.
- **LTP:** 두 비선형 포트(두 plate)가 **공통 $R_{tail}$**을 통해 coupling. 표준 tree WDF로는 표현 어려움 → **MNA hybrid** 또는 **multi-port nonlinear element** 접근.
- **SRPP:** LTP보다 더 강하게 coupled. MNA hybrid 권장.
- **Interstage transformer:** 02 문서의 JA 모델을 2-port WDF element로 래핑.

### 실전 권장 아키텍처

**하이브리드 접근:**
1. 대부분의 신호 경로는 WDF tree로 구현 (선형 부분 + 단일 비선형 노드의 common-cathode 단)
2. LTP, SRPP, interstage transformer처럼 **강하게 coupled된 블록**은 로컬 MNA 서브시스템으로 풀고, 그 결과를 WDF tree에 feed
3. 전체 샘플 루프는 WDF 기반, MNA 블록은 사이사이에 삽입

이 방식이 순수 MNA (매트릭스 크기 증가로 느림) 나 순수 WDF (다중 비선형성 표현 어려움)의 중간에서 실시간 성능과 정확도를 모두 잡는다. 상세는 07 문서 §구현 아키텍처 참조.

---

## 14.16 맺음말

본 문서에서 다룬 토폴로지들은 단일 진공관 수준의 모델링으로는 포착되지 않는 **빈티지 하드웨어의 시그니처 사운드**의 상당 부분을 결정한다. 특히:

- **Long-tailed pair**의 배음 상쇄 → Fairchild/Manley의 "투명한 압축"
- **Ultra-Linear** 접속의 부하선 특성 → 빈티지 파워 앰프의 "파워풀한 선형성"
- **Interstage transformer**의 복합 전달 특성 → 1950-60년대 고급 프리앰프의 "밀도"
- **Cathodyne**의 위상 비대칭 → 저출력 기타 앰프의 "brash" 고음
- **Partial cathode bypass**의 주파수 의존 이득 → Vox의 "chime"

본 플러그인의 MVP v1에서는 common-cathode, cathode follower, interstage transformer를 필수 구현하고, v1.1에서 LTP를 추가하여 Fairchild/Manley 스타일 variable-mu 압축기를 구현한다. UL, SRPP, Cathodyne phase splitter 등은 v2의 모델 확장에서 다룬다.

각 토폴로지의 수학적 모델은 본 문서의 수식을 바탕으로 하되, 실전 튜닝에서는 09 측정·검증 문서의 방법론에 따라 실기 측정과 비교하여 파라미터를 조정해야 한다. 특히 매칭 오차 (06 stochastic modeling) 같은 "실제 하드웨어의 imperfection"이 사운드의 일부임을 잊지 말 것.

---

## 관련 문서

- [01. 진공관 물리](01-vacuum-tube-physics.md) — 단일 진공관 내부 동작
- [02. 트랜스포머 물리](02-transformer-physics-and-distortion.md) — JA 모델, 인터스테이지·출력 트랜스포머 공통
- [04. 회로 상호작용과 임피던스](04-circuit-interactions-and-impedance.md) — 단일 단 + 주변 임피던스
- [05. 배음 스펙트럼](05-harmonic-spectrum-and-psychoacoustics.md) — 토폴로지별 배음 분포의 지각적 의미
- [06. 확률적 컴포넌트 모델링](06-stochastic-component-modeling.md) — 매칭 오차의 음향적 기여
- [07. 구현 전략](07-implementation-strategies.md) — WDF/MNA 상세
- [09. 측정과 검증](09-measurement-and-validation.md) — 토폴로지별 특성 측정법

---

## 참고문헌

1. Hafler, D., & Keroes, H. I. "An Ultra-Linear Amplifier." *Audio Engineering*, November 1951. US Patent **2,710,312** (filed 1952-05-20, issued 1955-06-07).
2. Blumlein, A. D. British Patent **496,883** (1937) — distributed loading 원리.
3. Vogel, B. (2008/2013). *How to Gain Gain: A Reference Book on Triodes in Audio Pre-Amps*. Springer. — SRPP, Mu-follower, White CF 수학적 분석.
4. Blencowe, M. (2009). *Designing Tube Preamps for Guitar and Bass*. — common-cathode, cathode follower, phase splitter 유도.
5. Aiken, R. "The Long-Tail Pair" & "What is Miller Capacitance?" — aikenamps.com 기술 노트.
6. The Valve Wizard. "Cathode Follower", "Grid Stoppers and Miller Capacitance" — valvewizard.co.uk.
7. Schlangen, T. "Pentodes connected as Triodes." ETF06 (2006). — EL34, KT88 등 Triode-strap 실측 특성곡선.
8. AMS-Neve. "About Marinair" & "1073 History" — ams-neve.com. Marinair 트랜스포머 및 BA283 topology.
9. "Ultra-linear." Wikipedia / HandWiki "Engineering:Ultra-linear" — Hafler-Keroes / Blumlein 선후관계 검증.
10. Wikipedia "Cascode" — common-cathode + common-grid 구조 및 Miller effect 감소 원리.
