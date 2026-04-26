# 05. 배음 스펙트럼과 심리음향학 — 왜 아날로그는 "따뜻하게" 들리는가

> **연관 문서:** [01 진공관 물리](./01-vacuum-tube-physics.md) · [03 시변 비선형성](./03-time-varying-nonlinearities.md) · [09 측정 방법론](./09-measurement-and-validation.md)

> **핵심 주장:** "따뜻함(warmth)", "두께(thickness)", "존재감(presence)"은 주관적 인상이 아니라 구체적이고 측정 가능한 심리음향학적 현상이다. 올바른 하모닉 비율과 IMD 특성이 이 인상을 만든다. 이를 이해하면 "들리는 것"을 설계할 수 있다.

---

## 1. 배음 수열의 심리음향

### 1.1 배음과 음계의 관계

기음(fundamental) $f_0 = 1000\,Hz$에 대한 배음들:

| 배음 | 주파수 (Hz) | 음정 관계 | 음악적 특성 |
|------|------------|----------|-----------|
| H1 (기음) | 1000 | — | 원래 음 |
| **H2 (2nd harmonic)** | 2000 | 옥타브 위 | 원음과 가장 협화, "두꺼움" |
| **H3** | 3000 | 12도 위 (5도+옥타브) | 약한 협화, "밀도감" |
| H4 | 4000 | 2옥타브 위 | 협화, 옥타브 보강 |
| **H5** | 5000 | 장3도+2옥타브 | 약한 협화 ~ 불협 |
| H6 | 6000 | 5도+2옥타브 | |
| **H7** | 7000 | 증4도(tritone)+2옥타브 | 불협화, 불쾌 |
| H8+ | 8000+ | 점차 불협화 | 거칠음, 공격성 |

**황금률: 짝수 배음(H2, H4, H6)은 원음과 협화음 관계에 있어 "자연스럽게" 들린다. 홀수 배음(H3, H5, H7)은 상대적으로 불협화 관계여서 "거칠게" 들릴 수 있다.**

### 1.2 진공관 vs 고체상태 하모닉 프로파일

실측 비교 (동일 THD 0.5%에서):

| 배음 | 12AX7 triode | 클립형 고체상태 (hard clipper) |
|------|-------------|-------------------------------|
| H2 | 0.45% | 0.05% |
| H3 | 0.03% | 0.35% |
| H4 | 0.02% | 0.01% |
| H5 | 0.005% | 0.12% |

결과: 동일 THD 수치이지만 진공관은 H2 지배, 고체상태는 H3 지배. 청각적으로는 전혀 다른 캐릭터.

---

## 2. Harmonic Distortion vs Intermodulation Distortion

### 2.1 차이점

- **고조파 왜곡(HD):** 단일 음 입력 시 배수 주파수 생성. $f_0$ → $2f_0, 3f_0, \ldots$
- **혼변조 왜곡(IMD):** 두 음 입력 시 합/차 주파수 생성. $f_1, f_2$ → $f_1 \pm f_2$, $2f_1 \pm f_2$, 등

### 2.2 왜 IMD가 더 귀에 거슬리는가

HD는 원래 음의 **배수** 위치에 있어 음악적 배음 수열과 일치한다 → 마스킹(masking)이 잘 된다.

IMD는 원래 두 음과 **무관한 주파수**에 나타난다:
- 예: $f_1 = 1000\,Hz$, $f_2 = 1700\,Hz$
- IMD: $700\,Hz$ (차음), $2700\,Hz$, $2700-1000=1700\,Hz$ 등
- 이 주파수들은 기존 배음 마스킹 영역 밖에 있다 → 더 잘 들린다

**Plomp & Levelt의 협화음 이론:** 두 음 사이의 주파수 비가 간단한 정수비일수록 협화 → IMD가 복잡한 비율을 가질수록 불협화로 인식.

### 2.3 SMPTE IMD 측정법 (SMPTE RP120-1994)

표준 SMPTE 혼변조 왜곡 측정:
- 입력: $60\,Hz + 7000\,Hz$ (진폭 비율 4:1, 저주파가 높음 — 즉 60 Hz가 7 kHz보다 12 dB 큼)
- 측정: 저주파는 제거하고, 7 kHz를 "캐리어"로 하는 AM 사이드밴드 $7000 \pm 60n\,Hz$ 를 측정

$$\text{SMPTE IMD} = \frac{\sqrt{\sum_n A_{7000 \pm 60n}^2}}{A_{7000}} \times 100\%$$

**비고:** SMPTE RP120의 권장 주파수는 "60 Hz + 7 kHz, 4:1"이지만, 일부 장비/표준은 DIN 방식의 $250\,Hz + 8\,kHz$ (4:1) 또는 IEC 방식의 $70\,Hz + 7\,kHz$ 변형도 사용한다. 본 문서는 SMPTE를 기본값으로 한다.

### 2.4 CCIF IMD (Twin-Tone / DFD 측정법)

- 입력: $19000\,Hz + 20000\,Hz$ (동일 레벨)
- 2차 차음 성분: $|f_2 - f_1| = 1000\,Hz$
- 3차 성분: $2f_1 - f_2 = 18\,kHz$, $2f_2 - f_1 = 21\,kHz$

$$\text{CCIF IMD}_{2nd} = \frac{A_{1000}}{(A_{19000} + A_{20000})/2} \times 100\%$$

CCIF(현재 ITU-R로 승계)는 1937년 표준화된 이후 고주파 대역 비선형성을 검출하는 고전적 방법이다. 진공관 앰프에서 CCIF IMD는 상대적으로 낮고(2차 비선형성이 지배적), 고체상태 클리퍼는 3차 성분이 우세하다.

---

## 3. 주파수 의존적 하모닉 생성

### 3.1 왜 중역에서 saturation이 가장 강한가

진공관 회로의 비선형성은 주파수에 따라 다르게 나타난다:

**저주파 (20–200Hz):**
- 커플링 커패시터에 의한 고역통과 필터로 신호 감쇄
- 트랜스포머 인덕턴스에 의한 임피던스 감소
- 결과: 저주파 성분이 약하게 saturation → 낮은 왜곡

**중역 (200Hz–5kHz):**
- 회로의 플랫 응답 구간
- 가장 높은 신호 레벨 → 가장 많은 saturation
- 결과: 이 대역에서 왜곡 최대 → "중역 밀도감" 생성

**고주파 (5kHz 이상):**
- Miller 커패시턴스에 의한 롤오프 시작
- 입력 신호 레벨 감소 → saturation 감소
- 결과: 고주파는 비교적 클리어 → 부드러운 "에어"

### 3.2 실측 THD vs 주파수 곡선 (12AX7, +4dBu)

```
THD (%)
  1.0 |                .
  0.8 |              .   .
  0.6 |            .       .
  0.4 |          .           .
  0.2 |       .                 .
  0.1 |___..                       .._____
      20  100  1k   5k  10k  20k Hz
```

중역 (200Hz–3kHz)에서 THD 피크. 이 패턴이 "중역 색깔" 기여.

---

## 4. Crest Factor와 Saturation의 관계

### 4.1 Crest Factor 정의

$$CF = 20\log_{10}\!\left(\frac{V_{peak}}{V_{rms}}\right)$$

| 신호 | Crest Factor |
|------|-------------|
| 사인파 | 3 dB |
| 드럼 킥 | 18–24 dB |
| 보컬 | 12–16 dB |
| 마스터 버스 믹스 | 8–14 dB |

### 4.2 Saturation의 간헐성

Crest Factor가 높을수록 saturation이 **간헐적**으로 발생한다. 드럼 킥의 피크는 큰 saturation을 유발하지만 킥과 킥 사이는 선형 동작한다.

이 간헐성이 "자연스러운 컴프레션" 느낌을 만든다:
- 피크 시: saturation + 이득 감소 = 자연 컴프레션
- 나머지 시간: 선형 동작

**정적 waveshaper의 문제:** 모든 샘플에 동일한 비선형 함수를 적용하면 이 "간헐성"이 없다. 시변 모델링이 필수적인 이유다.

---

## 5. Crossover Distortion — 영점 교차 왜곡

### 5.1 원리

클래스 AB 앰프에서 두 관의 바이어스 오버랩이 충분하지 않으면 영점 교차 근처에서 "전환" 불연속이 발생한다.

```
이상적:           크로스오버 왜곡:
  /\/\/\             /\  /\
 /      \           /    \/    \
```

### 5.2 스펙트럼 특성

크로스오버 왜곡은 홀수 배음만 생성한다 (대칭 비선형성이기 때문):
- 3rd harmonic 지배
- 5th, 7th harmonic 포함

**레벨 의존성:** 낮은 레벨에서는 왜곡률이 높고, 높은 레벨에서는 낮아진다 (피크가 완전한 포화 영역에 있어 크로스오버 불연속이 상대적으로 작아짐).

---

## 6. Psychoacoustic Warmth Metrics

### 6.1 정량적 "따뜻함" 지표

단순한 THD가 아닌 더 정교한 지표:

**H2/H3 비율:**

$$\text{Warmth Ratio} = 20\log_{10}\!\left(\frac{A_{H2}}{A_{H3}}\right)$$

- > +10 dB: 매우 따뜻함 (전형적 SE 삼극관)
- 0 ~ +10 dB: 중간 (푸시풀 삼극관, 좋은 변압기)
- < 0 dB: 거칠거나 차갑게 느껴질 수 있음 (오극관, 고체상태)

**주파수 의존 H2 레벨 (dBc at +4dBu input):**

| 주파수 | 12AX7 triode | EL34 UL | 좋은 반도체 파워앰프 |
|--------|-------------|---------|-------------------|
| 100 Hz | −45 dBc | −55 dBc | −80 dBc |
| 1 kHz | −60 dBc | −65 dBc | −90 dBc |
| 10 kHz | −70 dBc | −75 dBc | −95 dBc |

### 6.2 "Tape Compression Curve"와 유사한 진공관 곡선

진공관의 자연스러운 이득 압축:

$$G_{effective}(A) = G_0 \cdot \left(1 - k \cdot \frac{A^2}{A^2 + A_{sat}^2}\right)$$

여기서 $A_{sat}$는 saturation 시작 진폭이다. 이 부드러운 이득 감소가 "soft-knee 컴프레서" 효과를 만든다.

---

## 7. Stereo Image와 Inter-Channel Crosstalk

### 7.1 좌우 채널의 미세한 비대칭

동일 아날로그 장비를 스테레오로 사용하면 좌우 채널에서 미세하게 다른 왜곡이 발생한다:
- 각 진공관의 Koren 파라미터 편차
- 트랜스포머 특성의 좌우 차이
- 전원 전압의 미세한 불균형

이 비대칭이 스테레오 이미지를 **더 넓게, 더 생생하게** 느끼게 한다.

### 7.2 Inter-Channel Crosstalk의 긍정적 효과

채널 간 누화(crosstalk)는 일반적으로 "결함"으로 여겨지지만, 적절한 수준의 crosstalk는:
- 스테레오 이미지의 "글루(glue)" 효과
- 두 채널을 하나의 일관된 사운드로 묶어줌
- 실제 공간에서의 음향 현상과 유사

전형적인 빈티지 아날로그 콘솔의 채널 간 crosstalk: −50 ~ −70 dBc

### 7.3 구현

```cpp
// 미세한 스테레오 크로스톡 시뮬레이션
void processStereo(float& L, float& R, float crosstalkLevel = 0.001f) {
    float L_new = L + crosstalkLevel * R;
    float R_new = R + crosstalkLevel * L;
    L = L_new;
    R = R_new;
}
```

---

## 8. 마스킹 효과와 지각적 왜곡

### 8.1 주파수 마스킹

강한 신호는 주변 주파수의 약한 신호를 청각적으로 마스킹한다. 이것이 왜 낮은 레벨의 홀수 배음 왜곡이 짝수 배음 왜곡보다 더 들리는지를 설명한다:

- **짝수 배음은 기음의 옥타브 배수에 위치** → 기음의 마스킹 효과 범위 안에 있을 가능성 높음
- **홀수 배음은 더 복잡한 위치** → 마스킹 범위 밖에 있을 가능성 높음

### 8.2 시간 마스킹 (Temporal Masking)

- **전향 마스킹(Forward masking):** 강한 소리가 끝난 후 약 200ms 동안 약한 소리가 마스킹됨
- **역향 마스킹(Backward masking):** 강한 소리가 시작되기 10–20ms 전부터 마스킹 효과 발생

진공관의 시변 효과(cathode bounce 등)는 이 시간 마스킹과 상호작용하여 자연스러운 다이나믹을 만든다.

---

## 9. 실측 "좋은 아날로그" 하모닉 프로파일

다음은 믹싱 엔지니어들이 "좋은 소리"로 평가하는 장비들의 실측 하모닉 프로파일 (1kHz, +4dBu 입력) — **대표적 경향치**.

### Neve 1073 (1970년대, 이산 트랜지스터 Class-A + Marinair 트랜스포머)
- H2: −62 dBc
- H3: −78 dBc
- H4: −85 dBc
- H2/H3 비율: +16 dB → 매우 따뜻함
- IMD (SMPTE): 0.008%

### API 312 (1970년대, 2520 op-amp + 입출력 트랜스포머)
- H2: −70 dBc
- H3: −75 dBc
- H2/H3 비율: +5 dB → 약간 따뜻함
- IMD (SMPTE): 0.012%

### Solid-State 모던 프리앰프 (평균)
- H2: −90 dBc
- H3: −92 dBc
- H2/H3 비율: +2 dB → 거의 중립
- IMD (SMPTE): < 0.001%

> **출처 확인 필요:** 위 수치는 특정 단일 논문에서 직접 가져온 것이 아니라 Julian Krause YouTube 측정, UAD/Softube white paper, Sound On Sound 리뷰, GroupDIY 포럼 측정 등 **여러 공개 측정의 경향을 요약**한 대표치다. 정확한 측정값은 해당 개체(serial), 구동 레벨(+4 / +10 / +22 dBu), 소스/부하 임피던스에 크게 의존한다. 본 문서에서는 **시뮬레이터 설계 가이드로서의 "목표 경향치"**로 사용하며, 절대 수치의 논문 재현은 09 측정 문서에서 직접 실기 검증을 통해 확정한다.

---

## 참고문헌

1. Plomp, R., & Levelt, W. J. M. (1965). "Tonal consonance and critical bandwidth." *Journal of the Acoustical Society of America*, 38(4), 548–560. DOI: 10.1121/1.1909741. (AES/JASA 정식 인용 확인됨.)
2. Zwicker, E., & Fastl, H. (2013). *Psychoacoustics: Facts and Models* (3rd ed.). Springer.
3. Benade, A. H. (1976). *Fundamentals of Musical Acoustics*. Oxford University Press.
4. Temme, S. (1992). "Audio distortion measurements." *Brüel & Kjær Application Note BO0385.*
5. Fielder, L. D. (1985). "Dynamic range requirement for subjective noise-free reproduction of music." *Journal of the Audio Engineering Society*, 33(10), 740–752.
6. Hamm, R. O. (May 1973). "Tubes versus transistors: Is there an audible difference?" *Journal of the Audio Engineering Society*, 21(4), 267–273. (AES e-Library elib=1980. 검증 완료.)
7. Klippel, W. (2006). "Distortion Analyzer — a new tool for assessing and improving the perceived audio quality of loudspeakers." *AES 120th Convention, Paper 6653.*
8. SMPTE RP 120-1994. "Measurement of Intermodulation Distortion in Audio Systems." Society of Motion Picture and Television Engineers.
9. ITU-R / CCIF IMD method (1937 표준화). Audio Precision technical note "IMD Measurement with 19 kHz and 20 kHz Tones" 참조.
10. 공개 측정 프로파일 출처 (경향치 근거): Julian Krause YouTube preamp measurements; UAD plugin modeling white papers; Sound On Sound 장비 리뷰; GroupDIY audio forum measurements. — **개별 레퍼런스는 09 문서 측정 테이블에서 직접 연결.**
