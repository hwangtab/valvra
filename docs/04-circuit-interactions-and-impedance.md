# 04. 회로 단(Stage) 간 임피던스 상호작용

> **연관 문서:** [01 진공관 물리](./01-vacuum-tube-physics.md) · [03 시변 비선형성](./03-time-varying-nonlinearities.md) · [07 구현 전략](./07-implementation-strategies.md)

> **핵심 주장:** 일반 플러그인은 진공관 → 트랜스포머 → EQ를 독립적인 블록으로 직렬 연결한다. 실제 아날로그 회로에서는 각 단의 출력 임피던스와 다음 단의 입력 임피던스가 서로 "대화"하며, 신호 레벨에 따라 이 대화의 내용이 실시간으로 변한다.

---

## 1. 임피던스 분배 원리

두 회로가 연결될 때 신호는 소스 임피던스($Z_s$)와 부하 임피던스($Z_L$)에 의해 분배된다:

$$V_{out} = V_{in} \cdot \frac{Z_L}{Z_s + Z_L}$$

이상적인 경우 $Z_s \ll Z_L$이어야 하지만, 실제 진공관 회로에서:
- $Z_s$는 신호 레벨과 주파수에 따라 변한다
- $Z_L$도 다음 단의 동작 상태에 따라 변한다

결과: **신호 전달 비율이 동적으로 변한다** → 주파수 응답이 프로그램 종속적으로 변한다.

---

## 2. 삼극관의 출력 임피던스

### 2.1 플레이트 저항 $r_p$ — 소신호 모델

소신호 모델에서 삼극관의 출력 임피던스:

$$Z_{out} = r_p \| R_L$$

여기서 $r_p = \partial V_p / \partial I_p |_{V_g = const}$는 플레이트 저항이다.

**문제:** $r_p$는 동작점에 따라 변한다.
- 선형 영역 (낮은 신호): $r_p = 40–100\,k\Omega$ (12AX7)
- 포화 영역 (강한 신호): $r_p$ 감소
- 컷오프 영역: $r_p$ 무한대 (전류가 거의 0)

결과: **강한 신호일수록 출력 임피던스가 낮아지고, 다음 단의 부하에 의한 영향을 덜 받는다.**

### 2.2 Cathode Follower — 낮은 출력 임피던스

캐소드 팔로워 구성에서 출력은 캐소드에서 취한다. 소신호 모델을 꼼꼼히 풀면(캐소드 저항 $R_k$가 100% 직렬 부귀환으로 작용) **정확한 출력 임피던스**는 다음과 같이 유도된다.

소신호 등가회로(플레이트는 AC적으로 접지)에서 출력 노드(캐소드)에 시험 전류 $i_{test}$를 주입하고 입력 그리드는 접지시키면, 플레이트 저항 $r_p$를 통한 전류와 의존전원 $\mu v_{gk} = -\mu v_k$ 의 합이 $i_{test}$와 $R_k$로 흐르는 전류의 합과 같아야 한다. 풀면:

$$Z_{out,CF}\;=\;\frac{r_p}{\mu + 1}\;\Big\|\;R_k$$

즉, "$r_p/(\mu+1)$과 $R_k$의 병렬"이 정확한 결과다. 이는 The Valve Wizard, Ampbooks, ScienceDirect 등에서 일관되게 제시되는 표준 유도 결과와 일치한다. $R_k$가 충분히 크면(일반적인 설계) 다음과 같이 단순화된다:

$$Z_{out,CF} \approx \frac{r_p}{\mu + 1} = \frac{1}{g_m} \cdot \frac{\mu}{\mu+1}\Big|_{\mu \gg 1} \approx \frac{1}{g_m}$$

**흔한 근사 $Z_{out} \approx 1/g_m$은 $\mu \gg 1$이고 $R_k \gg r_p/(\mu+1)$인 극한에서만 성립한다.** 12AX7($\mu \approx 100$, $r_p \approx 62.5\,k\Omega$, $g_m \approx 1.6\,mA/V$)의 경우 $r_p/(\mu+1) \approx 619\,\Omega$로 $1/g_m \approx 625\,\Omega$와 거의 같지만, $\mu$가 작은 관(예: 6N6P, $\mu \approx 20$)이나 $R_k$가 작은 경우에는 두 식이 크게 달라진다.

**신호 의존성:** $r_p$, $g_m$, $\mu$가 모두 동작점에 따라 변하므로 $Z_{out,CF}$도 실시간으로 변한다. 구현 시에는 병렬식을 직접 사용하는 편이 안전하다.

---

## 3. Miller Capacitance와 동적 고주파 롤오프

### 3.1 기본 원리

삼극관의 그리드-플레이트 간 기생 커패시턴스 $C_{gp}$ (약 1.5–3 pF, 12AX7 데이터시트상 $C_{gp} \approx 1.7\,pF$)는 밀러 효과로 인해 증폭된다. 입력 노드에서 본 총 커패시턴스:

$$C_{in} = C_{gk} + C_{gp}(1 + |A_v|)$$

여기서 $|A_v|$는 단의 전압 이득(절댓값). 12AX7 common-cathode 단의 경우(데이터시트 + 스트레이 캐패시턴스 포함):
- $C_{gk} \approx 1.6\,pF + 0.7\,pF \text{ (stray)} = 2.3\,pF$
- $C_{gp} \approx 1.7\,pF + 0.7\,pF \text{ (stray)} = 2.4\,pF$
- $|A_v| \approx 60$ → $C_{in} \approx 2.3 + 2.4 \times 61 \approx 149\,pF$

실용적으로는 "12AX7 입력 커패시턴스 ≈ 100–150 pF" 수준. 이 커패시턴스가 **소스 임피던스(=이전 단의 $Z_{out}$)**와 함께 저역통과 필터를 형성해 고주파를 롤오프시킨다:

$$f_{-3dB} = \frac{1}{2\pi \cdot Z_{source} \cdot C_{in}}$$

> 수정: 원 문서의 "고역통과 필터"는 "저역통과(고주파 롤오프)"의 오기. 또한 롤오프를 결정하는 것은 다음 단의 그리드-리크 저항 $R_{grid}$가 아니라 전(前) 단의 출력 임피던스 $Z_{source}$(두 저항의 병렬이 되는 경우도 있음).

### 3.2 동적 변화

신호 레벨이 높아져 포화 영역에 가까워지면:
1. $g_m$ 감소 → $A_v$ 감소 → $C_{miller}$ 감소
2. $f_{-3dB}$ 상승 → 고주파 롤오프 완화

반대로 아주 낮은 레벨에서는 $C_{miller}$가 더 크고 고주파가 더 감쇄된다.

**청취 효과:** 신호 레벨에 따라 자연스럽게 고음의 밝기가 변한다. 강한 신호는 조금 더 선명하게 들릴 수 있다.

---

## 4. Interstage Coupling Capacitor 동작

### 4.1 기본 역할

두 증폭 단 사이에 삽입된 커플링 커패시터 $C_c$는 DC를 차단하고 AC 신호만 통과시킨다. 다음 단의 그리드 누설 저항 $R_g$와 함께 고역통과 필터를 구성한다:

$$f_{low} = \frac{1}{2\pi R_g C_c}$$

일반적인 값: $C_c = 22\,nF$, $R_g = 1\,M\Omega$ → $f_{low} \approx 7\,Hz$ (저역 응답 충분)

### 4.2 Grid Conduction DC Shift — 비선형 효과

강한 신호가 들어오면 그리드가 순간적으로 양전위가 되어 그리드 전류가 흐른다. 이 전류가 $C_c$를 충전하여 **DC 오프셋이 축적**된다.

```
신호   ___/‾‾‾\_____
Vg_dc  __________/‾  ← DC 오프셋 천천히 증가
```

**효과:**
- 동작점이 컷오프 방향으로 이동
- 이득 감소, 비선형성 증가
- 회복 시정수: $\tau = R_g \cdot C_c$ (음악적으로 의미 있는 수십 ms~초)

**이 효과가 "진공관 컴프레션"의 또 다른 원천이다.**

---

## 5. Feedback Loop Dynamics — 부귀환의 역동성

### 5.1 전역 부귀환 (Global Negative Feedback)

많은 진공관 파워 앰프는 출력에서 초단 입력으로 신호를 돌려 이득을 낮추고 왜곡을 감소시킨다.

$$A_{closed} = \frac{A_{open}}{1 + \beta A_{open}}$$

여기서 $\beta$는 귀환 계수. 부귀환이 크면 왜곡이 줄지만...

### 5.2 귀환 루프 지연과 위상 왜곡

귀환 신호는 여러 단을 통과하면서 **지연(delay)**과 **위상 회전**이 발생한다. 이 위상 회전이 특정 주파수에서 귀환이 오히려 **정귀환**이 될 수 있다.

**Nyquist/Barkhausen 발진 조건:** 루프 이득의 크기가 1 (0 dB)이고, 동시에 루프 위상이 원래 **음의 귀환(−180°)** 에서 추가로 180° 회전하여 총 **−360° (= 0°, 즉 동상)**가 되는 주파수가 존재하면 발진한다. 부귀환 회로는 출발점이 이미 −180° 반전이므로, 실무적으로는 "루프 내부 추가 위상 지연이 −180°에 도달하기 전에 이득이 1 미만으로 떨어져야 한다"는 **위상 마진(phase margin)** 로 관리한다.

실제 파워앰프는 발진 없이 안정적이지만, 고주파에서 위상 마진(phase margin)이 적다면 **과도 응답에서 미세한 링잉(ringing)**이 발생한다.

**청취 효과:** 고주파 과도 신호에서 수 마이크로초의 링잉이 "에어(air)"와 "존재감(presence)"을 더한다.

### 5.3 지연 없는 귀환 vs 실제 귀환

플러그인에서 전역 부귀환을 단순히 이득 감소로 처리하면 위상 응답이 틀린다. 실제 귀환 루프를 모델링하려면 루프 지연(수 μs~수십 μs)을 반드시 포함해야 한다.

---

## 6. Transformer-Tube 상호작용

### 6.1 출력 트랜스포머가 플레이트 부하를 변화시킨다

출력 트랜스포머의 1차 인덕턴스 $L_p$는 주파수에 의존하는 부하 임피던스를 플레이트에 제공한다:

$$Z_L(j\omega) = j\omega L_p + R_{primary}$$

**저주파에서:** $|Z_L| = \omega L_p \rightarrow 0$ → 부하 임피던스 감소 → 부하선 기울기 변화 → **비선형성 증가**

따라서 진공관 파워앰프는 **저주파에서 왜곡이 더 크다**. 이것이 저역의 "두툼함"과 "밀도감"의 원천이다.

### 6.2 Load Line 기울기의 주파수 의존성

고주파: $Z_L \approx R_{primary}$이 지배 → 직선에 가까운 부하선
저주파: $Z_L$이 주파수에 따라 변 → 기울기가 주파수마다 다른 부하선

**구현:** 주파수 의존 부하 임피던스를 Koren 모델에 통합하면 저주파 왜곡 특성을 정확히 모델링할 수 있다.

---

## 7. Class-AB Push-Pull 임피던스 불연속

### 7.1 Crossover 영역

클래스 AB 파워 앰프에서 두 진공관은 각각 양의 반주기와 음의 반주기를 담당한다. 두 관이 동시에 모두 통전하는 **오버랩 영역** 밖에서는 한 관만 동작한다.

**영점 교차(zero crossing) 근처:**
- 동작 중인 관: 출력 임피던스 낮음 ($r_p \| R_L$)
- 쉬고 있는 관: 출력 임피던스 매우 높음 (거의 단락되지 않음)
- 결과: 교차 지점에서 **출력 임피던스 불연속** → 크로스오버 왜곡

### 7.2 크로스오버 왜곡의 스펙트럼

크로스오버 왜곡은 주로 **홀수 배음(3rd, 5th, 7th)**을 생성한다. 특히 낮은 레벨에서 더 두드러진다 (높은 레벨에서는 포화 왜곡에 묻힌다).

**음악적 영향:**
- 클래스 AB 앰프의 낮은 레벨에서 약간의 "거침" 또는 "그런지(grit)"
- 클래스 A 동작 (높은 바이어스)에서는 이 효과가 없음

---

## 8. 다단 증폭기에서의 누적 비선형성

### 8.1 단 간 비선형 상호작용

1단에서 생성된 하모닉이 2단에 입력되면:

$$y_2 = f_2(y_1) = f_2(f_1(x_1 + h_1 \text{ harmonics}))$$

새로운 하모닉이 이미 존재하는 하모닉과 혼합되어 **조합음 (combination tones)**을 생성한다.

예: 1단에서 2nd harmonic $2f_0$ 생성 → 2단에서 $2f_0$와 원래 $f_0$가 혼합 → IMD 성분 $3f_0$, $f_0$ 등이 새로 생성

### 8.2 3단 구성의 Neve 1073 예시

**정정:** Neve 1073는 1970년부터 **Class-A 이산(discrete) 트랜지스터** 설계이며, 진공관을 사용하지 않는다. 본 문서는 "다단 간 비선형성 누적"의 원리를 설명하기 위한 예시로만 1073을 참조한다.

Neve 1073의 신호 경로 (BA283 기반):
1. Marinair 입력 트랜스포머 + 마이크 이득 단 (실리콘 트랜지스터, 2N3055 등 포함하는 이산 Class-A 증폭)
2. 라인 이득 단 (BA283의 두 번째 증폭 섹션)
3. EQ 회로 + 출력 단 + Marinair 출력 트랜스포머

각 단에서 발생하는 비선형성 + 두 트랜스포머의 Jiles-Atherton 히스테리시스가 상호작용하여 최종적인 복잡한 하모닉 구조를 만든다. 이것이 단순 saturation 플러그인이 1073의 색깔을 재현하지 못하는 이유다. 진공관 아날로그 다단 설계(예: Pultec EQP-1A, Fairchild 670)도 같은 "단 누적" 원리를 공유한다 — 자세한 토폴로지 구분은 14 문서 참조.

---

## 9. 신호 의존 임피던스 — 실시간 구현 전략

### 9.1 계층화된 임피던스 모델

실시간으로 단 간 임피던스 상호작용을 구현하는 전략:

```
[입력 신호]
     │
     ▼
[1단: 진공관 모델]
  - Koren / tanh 비선형
  - 동적 rp 계산
  - Ck 바이어스 바운스
     │
     ├─ Z_out(t) 계산
     ▼
[임피던스 분배기]
  - Z_out(1단) + Z_in(2단) → 전달 계수 계산
     │
     ▼
[2단: 트랜스포머]
  - JA hysteresis
  - Lm, Lleak 필터링
     │
     ├─ Z_out(t) 계산
     ▼
...
```

### 9.2 근사 방법: 신호 의존 필터 계수

완전한 임피던스 상호작용은 매우 비용이 크다. 실용적인 근사:

1. 신호의 RMS 또는 피크 레벨을 추적
2. 레벨에 따라 필터 계수를 보간
3. 이를 통해 "프로그램 종속 주파수 응답"을 근사

```cpp
// 신호 레벨에 따른 고주파 롤오프 계수 보간
float millerCutoff(float signalRms, float gm0, float Cgp, float Rg) {
    float gmEffective = gm0 * (1.0f - 0.3f * std::min(signalRms, 1.0f));
    float Cmiller = Cgp * (1.0f + gmEffective * 30000.0f); // 30kΩ 부하
    return 1.0f / (2.0f * M_PI * Rg * Cmiller);
}
```

---

## 참고문헌

1. Blencowe, M. (2009). *Designing Tube Preamps for Guitar and Bass*. Merlin Blencowe.
2. Millman, J., & Halkias, C. (1972). *Integrated Electronics*. McGraw-Hill.
3. Self, D. (2010). *Audio Power Amplifier Design Handbook* (5th ed.). Focal Press.
4. Yeh, D. T., & Smith, J. O. (2006). "Discretization of the '59 Fender Bassman tone stack." *Proc. DAFx-06*, Montréal. (주의: 이전 DAFx-08 표기는 오류 — 10 문서 수정 이력 참조.)
5. Werner, K. J., Nangia, V., Bernardini, A., Smith, J. O., & Sarti, A. (2016). "An Improved and Generalized Diode Clipper Model for Wave Digital Filters." *AES 139th Convention, Paper 9387.*
6. Holters, M., & Zölzer, U. (2011). "Physical Modelling of a Wah-Wah Guitar Effects Pedal as a Case Study for Application of the Nodal DK Method to Circuits with Variable Parts." *Proc. 14th Int. Conf. Digital Audio Effects (DAFx-11).*
7. Aiken, R. (2006). "What is Miller Capacitance?" *Aiken Amplification Technical Notes.* — 12AX7 Miller 커패시턴스 계산 예시.
8. The Valve Wizard. "Cathode Follower" & "Grid Stoppers and Miller Capacitance" — 캐소드 팔로워 $Z_{out}$ 및 Miller 효과 유도.
9. AMS-Neve. "1073 History" (ams-neve.com) / JLM Audio. "Neve transformer info." — BA283 및 Marinair 트랜스포머 소스.
10. Nyquist, H. (1932). "Regeneration theory." *Bell System Technical Journal*, 11(1), 126–147.
