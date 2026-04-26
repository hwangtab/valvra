# 13. 마스터링 엔지니어용 기능 명세 (Mastering-Grade Feature Specification)

> 연관 문서: [07. 구현 전략](./07-implementation-strategies.md), [09. 측정 및 검증](./09-measurement-and-validation.md), [08. 경쟁 분석](./08-competitive-analysis.md)

---

## 13.1 서론 (Introduction)

### 13.1.1 왜 이 문서가 필요한가

본 플러그인의 핵심 설계 목표는 "진공관-트랜스포머 기반 아날로그 색(analog color)을 물리적으로 정확히 재현"하는 것이다. 그러나 감사(audit) 결과, **마스터링 엔지니어(mastering engineer)가 현업에서 필수로 요구하는 일련의 기능들**이 기존 문서(01–12)에 전혀 언급되지 않았음을 발견했다:

1. **Mid/Side 독립 처리** — 중앙(mid)과 측면(side) 채널에 서로 다른 saturation 양 적용
2. **True Peak 제어** — ITU-R BS.1770-5 (2023) 규격을 준수하는 intersample peak 한계
3. **Linear-phase 옵션** — 위상 왜곡 없이 순수 색깔만 추가
4. **A/B 비교 + LUFS 매칭** — 편향(bias) 없는 청취 비교
5. **Safety clipper / brick-wall limiter** — 출력 단 안전장치
6. **PDC (Plugin Delay Compensation)** — DAW가 정확한 latency를 알도록 보고

이 문서는 위 기능들의 **수학적 근거, UI 설계, 구현 의사코드, 검증 방법**을 체계적으로 정리한다.

### 13.1.2 마스터링 vs 믹싱 단계의 요구사항 차이

| 항목 | 믹싱 (Mixing) | 마스터링 (Mastering) |
|------|--------------|---------------------|
| 소스 | 개별 트랙 (vocals, drums, bass, …) | 2-mix 스테레오 파일 |
| 드라이브 양 | 중간~강함 (creative color) | 가벼움 (transparent glue) |
| 위상 허용도 | 자연스러운 minimum-phase OK | 가급적 linear-phase |
| True Peak | 채널별 헤드룸 여유 | 배급 플랫폼 규격 준수 필수 |
| Latency | < 10ms 선호 (monitoring) | 50ms까지 허용 (bounce) |
| A/B 비교 | 솔로/뮤트로 대체 | 플러그인 내부 필수 |
| 대상 라우드니스 | 자유 | 플랫폼별 고정 타깃 (LUFS) |
| Mono 호환 | 트랙별 차이 | 엄격히 검증 필요 |

### 13.1.3 플러그인 포지셔닝: "Mix Bus + Mastering Dual-Purpose"

본 플러그인은 **믹스 버스(mix bus)와 마스터 버스(master bus)에서 모두 사용 가능한 듀얼 목적(dual-purpose) 프로세서**로 설계된다. 이를 위해 다음의 원칙을 채택한다:

- 기본(default) 상태는 믹싱 친화적: stereo linked, minimum-phase, moderate drive
- "Mastering 모드" 스위치로 전환 시: M/S 독립, linear-phase 옵션, safety limiter 활성
- 한 플러그인 내에서 두 워크플로우를 지원하되, UX가 과도하게 복잡해지지 않도록 **고급 기능은 접혀 있는(collapsed) 패널**에 배치

---

## 13.2 Mid/Side 독립 처리 (Mid/Side Independent Processing)

### 13.2.1 필요성 (Motivation)

스테레오 버스에서 중앙과 측면에 동일한 saturation을 걸면 공간감이 둔해지거나 중앙이 과하게 뭉개지는 경우가 많다. 마스터링 엔지니어들은 다음과 같은 전략을 즐겨 쓴다:

- **Mid 강한 tube drive**: 보컬·킥·스네어 등 중앙 정위된 요소에 진공관 색을 강조
- **Side 투명 (minimal drive)**: 리버브 테일, 광폭 신스 등 측면 정보는 공간감을 유지

반대의 사용례도 있다:
- **Side만 tape saturation**: 스테레오 이미지를 더 넓게 느껴지도록 (아날로그 테이프의 edge warming)
- **Mid만 transformer harmonic**: 중앙 베이스에 두께 추가

### 13.2.2 수학적 정의 (Mathematical Definition)

본 플러그인은 **에너지 보존형(energy-preserving)** M/S 변환을 채택한다:

**Encode (L/R → M/S):**
$$M = \frac{L + R}{\sqrt{2}}, \quad S = \frac{L - R}{\sqrt{2}}$$

**Decode (M/S → L/R):**
$$L = \frac{M + S}{\sqrt{2}}, \quad R = \frac{M - S}{\sqrt{2}}$$

**Unity-gain 보존 증명:**

$$L' = \frac{1}{\sqrt{2}} \left( \frac{L+R}{\sqrt{2}} + \frac{L-R}{\sqrt{2}} \right) = \frac{1}{\sqrt{2}} \cdot \frac{2L}{\sqrt{2}} = L$$

$$R' = \frac{1}{\sqrt{2}} \left( \frac{L+R}{\sqrt{2}} - \frac{L-R}{\sqrt{2}} \right) = R$$

즉, encode → (processing 없이) decode → 원본과 완전 동일.

### 13.2.3 왜 $\sqrt{2}$ 정규화인가 (1/2 vs 1/√2)

일부 구현에서는 다음과 같이 쓴다:
$$M = \frac{L+R}{2}, \quad S = \frac{L-R}{2}$$

이 경우 decode 시 **2배를 곱해야** 원본이 복원되는데, 처리 과정에서 M과 S를 독립적으로 증폭시키면 **에너지 계산이 꼬인다**. $\sqrt{2}$ 정규화는 다음 의미를 가진다:

- **Parseval 정리 준수**: $|L|^2 + |R|^2 = |M|^2 + |S|^2$
- M/S 도메인에서의 RMS 측정이 L/R 도메인과 일치
- Saturation 비선형 처리 후에도 에너지 관계 유지

### 13.2.4 UI 설계 (User Interface)

```
┌────────────────────────────────────────────┐
│  Stereo Mode                                │
│  ( ) Stereo (L/R independent)               │
│  (•) Mid/Side                               │
│  ( ) Mono-Linked (L=R forced)               │
│                                             │
│  ┌──── MID ────┐     ┌──── SIDE ────┐       │
│  │ Drive  [ ●] │     │ Drive  [●  ] │       │
│  │ Char   [ ●] │     │ Char   [●  ] │       │
│  │ Age    [●●] │     │ Age    [● ] │        │
│  │ Mix    [●●] │     │ Mix    [●●] │       │
│  └─────────────┘     └─────────────┘        │
│                                             │
│  [Link MID↔SIDE]  [Copy MID→SIDE]           │
└────────────────────────────────────────────┘
```

**모드별 동작:**
- **Stereo**: 기존 L/R 독립 처리 (기본)
- **Mid/Side**: 내부에서 encode → M/S 각각 독립 processing → decode
- **Mono-Linked**: $M$만 처리, $S = 0$으로 강제 (모노 호환 극대화용)

### 13.2.5 구현 고려사항 (Implementation Considerations)

#### 위상 보존 (Phase Preservation)

M과 S 채널의 **처리 체인 latency가 샘플 단위로 완벽히 일치**해야 한다. 불일치 시:
- Mono 다운믹스에서 cancellation 발생 ($L - R$의 일부가 $M$을 뒤흔듦)
- 스테레오 이미지의 "smearing" (흐려짐)

구현에서 동일한 오버샘플러, 동일한 SIMD 경로를 사용하여 **determinism**을 보장해야 한다.

#### 모노 호환성 검증

M/S 처리 후 `L + R`을 모노 다운믹스해서 다음을 측정:

1. Side 채널의 일부가 Mid로 누설(bleed)되지 않는가?
2. Side의 비선형 왜곡(harmonic)이 모노에서 어떻게 들리는가?

권장 테스트: 중앙 정위 kick drum + 광폭 reverb → mono 축소 시 **kick의 trnasient가 smear되면 문제**.

#### 의사코드

```cpp
void processMidSide(float* L, float* R, int numSamples) {
    const float invSqrt2 = 0.7071067811865475f;

    for (int n = 0; n < numSamples; ++n) {
        // Encode
        float M = (L[n] + R[n]) * invSqrt2;
        float S = (L[n] - R[n]) * invSqrt2;

        // Independent processing
        M = midChain.process(M);   // 독립 파라미터
        S = sideChain.process(S);

        // Decode
        L[n] = (M + S) * invSqrt2;
        R[n] = (M - S) * invSqrt2;
    }
}
```

---

## 13.3 True Peak 제어 (True Peak Control)

### 13.3.1 ITU-R BS.1770-5 기준 True Peak 재확인

**샘플 피크(sample peak)** 는 이산 샘플 값의 절댓값 최대이다:
$$P_\text{sample} = \max_n |x[n]|$$

**True Peak** 는 복원된 연속 신호(continuous waveform)의 최대 절댓값이다:
$$P_\text{true} = \max_t |x(t)|$$

$P_\text{true} \geq P_\text{sample}$이며, 둘 사이 차이를 **intersample peak (ISP)** 라 부른다. **ITU-R BS.1770-5 (2023-11 개정판, 현재 최신)** 는 **최소 4배 오버샘플링(4× FIR upsampling)** 으로 true peak를 추정하도록 규정한다. BS.1770-4(2015) 대비 -5는 객체 기반 오디오(object-based audio) 측정 알고리즘 추가가 주요 변경이며, 스테레오 측정용 K-가중·2채널 게이팅 로직은 -4와 실질적으로 동일하다. 즉 **기존 -4 기반 구현은 그대로 유효**하나, 본 문서의 공식 참조는 **-5(2023)** 로 갱신한다.

**K-weighting 필터 (48 kHz 기준):**
- Pre-filter: high-shelf (+4 dB @ ~1681 Hz, 2nd-order)
- RLB filter: high-pass (~38 Hz, 2nd-order)
- 다른 샘플레이트는 동일한 주파수 응답이 재현되도록 계수 재계산 필요 (단순히 -4 계수를 옮기면 오류)

**채널 가중:**
- L, R, C: 1.0 (0 dB)
- Ls, Rs (서라운드): **1.41 (≈ +1.5 dB)**
- LFE: 측정 제외

**단위:**
- dBFS: 샘플 피크 기준, full-scale digital = 0 dBFS
- **dBTP**: true peak 기준, full-scale digital continuous = 0 dBTP

배급 플랫폼(Spotify, Apple Music 등)은 샘플 피크가 아닌 **dBTP 한계**를 요구한다.

### 13.3.2 Oversampled Saturation의 Intersample Peak 문제

진공관·트랜스포머의 비선형 처리는 **새 주파수 성분(harmonic)을 생성**한다. 이는 intersample peak을 악화시키는 주요 원인이다:

1. 입력 신호의 true peak이 -0.3 dBTP
2. 강한 drive 시 3차 고조파가 추가됨
3. 추가된 고조파가 기존 ISP와 위상 정합되면서 **새로운 ISP가 +1.5 dBTP까지 상승**

특히 주의할 경우:
- **0 dBFS 근처 입력** + heavy saturation
- **저주파 풍부 소스** (kick, bass): harmonic이 가청 대역에 집중
- **asymmetric saturation** (even-order harmonic): DC offset과 결합 시 peak 상승

### 13.3.3 Safety Clipper / Limiter

본 플러그인은 출력 단에 **선택 가능한 3가지 안전장치**를 제공한다:

#### 모드 1: None (Off)

- 아날로그 충실도 우선. 외부 리미터 사용 가정.
- CPU 최소, latency 추가 없음.

#### 모드 2: Soft Clipper (tanh-based)

2× 오버샘플에서 $\tanh$ 기반 soft clipping:

$$y = \tanh\left(\frac{x}{T}\right) \cdot T$$

여기서 $T$는 threshold linear 값 (예: $T = 10^{-1/20} \approx 0.891$ for -1 dBTP).

- **장점**: 연속 미분 가능 → aliasing-free at 2× OS
- **단점**: Soft knee라 약간의 compression 발생 (harmonic도 추가)

#### 모드 3: Brick-Wall Limiter (True Peak aware)

- 4× 오버샘플링으로 true peak 감지
- **Look-ahead 5ms** (지연 보상)
- Envelope smoothing: attack 0.1ms, release 50ms
- 출력이 절대로 타깃 dBTP를 넘지 않도록 보장

### 13.3.4 사용자 설정 범위

- **Target True Peak**: -3.0 ~ -0.1 dBTP (0.1 dB step), 기본값 -1.0 dBTP
- **Mode**: None / Soft / Brick-wall
- **Look-ahead**: Brick-wall일 때만 활성 (1–10ms)
- **Oversampling for ISP detection**: 2× / 4× / 8× (CPU 트레이드오프)

### 13.3.5 구현 의사코드

```cpp
class TruePeakSafety {
public:
    enum class Mode { None, Soft, BrickWall };

    void setTargetDBTP(float dbtp) {
        targetLin_ = std::pow(10.0f, dbtp / 20.0f);
    }

    void setMode(Mode m) { mode_ = m; }

    float process(float in) {
        switch (mode_) {
            case Mode::None:
                return in;

            case Mode::Soft:
                return softClip(in);

            case Mode::BrickWall:
                return brickWallLimit(in);
        }
        return in;
    }

private:
    float targetLin_ = 0.891f;  // -1 dBTP
    Mode mode_ = Mode::None;
    Oversampler4x os_;
    LookAheadBuffer lookAhead_{480};  // 5ms @ 96kHz

    float softClip(float in) {
        // 2x OS로 upsample
        float up[2];
        os2x_.upsample(in, up);
        for (int i = 0; i < 2; ++i) {
            up[i] = std::tanh(up[i] / targetLin_) * targetLin_;
        }
        return os2x_.downsample(up);
    }

    float brickWallLimit(float in) {
        // 1) Look-ahead 버퍼에 push
        float delayed = lookAhead_.push(in);

        // 2) 현재 샘플의 4x OS peak 측정
        float up[4];
        os_.upsample(in, up);
        float peak = 0;
        for (int i = 0; i < 4; ++i) peak = std::max(peak, std::abs(up[i]));

        // 3) Gain 계산 (peak > target → attenuate)
        float targetGain = (peak > targetLin_) ? (targetLin_ / peak) : 1.0f;

        // 4) Envelope smoothing (attack/release)
        envGain_ = smoothGain(envGain_, targetGain);

        // 5) 지연된 샘플에 적용
        return delayed * envGain_;
    }
};
```

### 13.3.6 Aliasing 고려사항

- Soft clip은 2× OS에서도 안전하지만, **tanh의 3차 고조파**가 fs/4 근처에서 문제가 될 수 있음 → 4× 권장
- Brick-wall의 gain modulation도 aliasing 원천 → smoothed envelope 필수

---

## 13.4 Linear-Phase vs Minimum-Phase 모드

### 13.4.1 왜 선택권이 필요한가

**Minimum-phase (기본값):**
- 아날로그 회로의 실제 위상 응답을 충실히 재현
- 전통적으로 "음악적(musical)" 하다고 평가됨
- 낮은 latency (< 1ms, 오버샘플링 overhead만)
- **단점**: 주파수별 위상 이동 → transient가 살짝 smear될 수 있음

**Linear-phase:**
- 모든 주파수가 동일 지연 → 위상 왜곡 0
- 마스터링에서 "색깔만 깔끔하게 추가"하고 싶을 때 선호
- **단점**:
  - 높은 latency (수십 ms 수준)
  - **Pre-ringing** (원인 전에 리플 발생): 짧은 transient(kick, snare)에 부자연스러움

### 13.4.2 Linear-phase 구현 방법

본 플러그인의 비선형 부분(saturation)은 본질적으로 위상을 보존하지 않는다. Linear-phase 모드는 **주파수 응답(EQ 특성)만 zero-phase로 재구성**하는 전략을 사용한다:

#### 전략 A: Offline 임펄스 응답 측정 + FIR 변환

1. Minimum-phase 체인에 임펄스 주입 → IR 측정
2. IR의 magnitude 응답만 추출: $|H(f)|$
3. Zero-phase FIR 재구성: $h_\text{lin}[n]$ 설계 (역 FFT → symmetric windowing)
4. Zero-phase convolution으로 런타임 적용

#### 전략 B: 분리형 접근 (권장)

- 비선형 saturation은 그대로 minimum-phase로 처리
- **Pre-EQ / Post-EQ만 linear-phase FIR**로 구현 (transformer의 frequency shaping 부분)
- 이 방식은 "색의 본질"은 유지하면서 EQ 커브의 위상만 linear 화

```
입력 → [Linear-phase Pre-EQ] → [Saturation (min-phase)] → [Linear-phase Post-EQ] → 출력
```

### 13.4.3 구현 디테일 (FIR 설계)

- **Tap 수**: 타깃 주파수 해상도에 따라 결정
  - 최저 유효 주파수 $f_\text{low} = 20$ Hz
  - 최소 tap 수: $N \geq \frac{2 f_s}{f_\text{low}}$
  - 44.1 kHz에서 $N \approx 4410$ taps (약 50ms latency의 절반)
- **Windowing**: Kaiser window ($\beta = 8$) 또는 Blackman-Harris
- **Zero-phase**: 대칭(symmetric) FIR → group delay = $(N-1)/2$ 샘플 상수

### 13.4.4 모드별 비교

| 항목 | Minimum-phase | Linear-phase |
|------|--------------|-------------|
| Latency | < 1ms (오버샘플 only) | 5–50ms |
| 위상 왜곡 | 자연스러운 아날로그 | 없음 (constant group delay) |
| Pre-ringing | 없음 | 있음 (짧은 transient에 문제) |
| CPU | 낮음 | 중간~높음 (FIR convolution) |
| 메모리 | 낮음 | 높음 (FIR tap buffer) |
| 추천 용도 | 믹싱, creative color, 실시간 모니터링 | 마스터링 purity, offline bounce |

### 13.4.5 UI

```
┌──────────────────────────┐
│  Phase Mode              │
│  (•) Minimum (Analog)    │
│  ( ) Linear (Mastering)  │
│                          │
│  Latency: 1 sample       │
└──────────────────────────┘
```

Linear 선택 시 latency 값이 실시간으로 갱신되어 DAW에 보고됨.

---

## 13.5 A/B 비교 기능 (A/B Comparison)

### 13.5.1 필수 기능 (Requirements)

1. **내부 A/B 스냅샷**: 현재 파라미터 세트를 A 또는 B 슬롯에 저장
2. **즉시 전환**: 클릭·팝 노이즈 없음 (10ms crossfade)
3. **Volume-compensated**: A와 B의 라우드니스 차이로 인한 편향(loudness bias) 제거
4. **Null-test mode**: Bypass와 Active의 차이(difference signal)만 재생
5. **Copy A→B / B→A / Reset** 버튼

### 13.5.2 UI

```
┌───────────────────────────────┐
│  [ A ]  [ B ]  [Copy A→B]     │
│   ●                            │
│  LUFS Match:  [ ON ]           │
│  Null Test:   [ OFF ]          │
│                                │
│  A: -14.2 LUFS                 │
│  B: -13.9 LUFS (+0.3 offset)   │
└───────────────────────────────┘
```

### 13.5.3 LUFS 자동 매칭

인간의 청각은 **라우드 = 좋다**고 착각하기 쉽다. 따라서 A/B 비교 시 라우드니스 매칭이 필수이다.

#### 알고리즘

1. Active 상태와 Bypass 상태를 **병렬로 동시 처리** (true-parallel)
2. 두 경로의 **integrated LUFS (30초 window)** 실시간 계산 (ITU-R BS.1770-5)
3. 차이(offset)를 계산: $\Delta = \text{LUFS}_\text{active} - \text{LUFS}_\text{bypass}$
4. Bypass 출력에 $+\Delta$ gain 적용 → 청각적 크기 동일화

#### 구현 의사코드

```cpp
class ABComparator {
    Snapshot stateA_, stateB_;
    bool current_ = false;  // false = A, true = B

    LUFSMeter lufsActive_, lufsBypass_;  // ITU-R BS.1770-5 기반
    float matchGain_ = 1.0f;
    bool lufsMatch_ = true;
    bool nullTest_ = false;

    // 10ms crossfade (at 48kHz → 480 samples)
    CrossfadeEnvelope xfade_{480};

public:
    void switchTo(bool toB) {
        if (current_ != toB) {
            loadState(toB ? stateB_ : stateA_);
            xfade_.trigger();
            current_ = toB;
        }
    }

    float process(float in) {
        float active = chain_.process(in);           // 실제 처리
        float bypass = in;                            // 원본

        lufsActive_.update(active);
        lufsBypass_.update(bypass);

        if (lufsMatch_) {
            float delta = lufsActive_.getLUFS()
                        - lufsBypass_.getLUFS();
            matchGain_ = std::pow(10.0f, delta / 20.0f);
        }

        float bypassCompensated = bypass * matchGain_;

        if (nullTest_) {
            return active - bypassCompensated;  // difference
        }

        return xfade_.isActive()
            ? xfade_.mix(active, bypassCompensated)
            : active;
    }
};
```

### 13.5.4 Null-Test Mode

두 신호의 차이를 재생하여 **플러그인이 실제로 추가한 색깔만** 들어볼 수 있게 한다:

$$y_\text{null}(t) = y_\text{active}(t) - y_\text{bypass}(t) \cdot 10^{\Delta/20}$$

- 고요하면 → 처리 영향이 거의 없다
- 큰 소리가 들리면 → 많이 처리됨 (좋은지 나쁜지는 귀 판단)

### 13.5.5 구현 고려사항

- **True-parallel**: A/B 전환 시마다 재계산하지 말고 **항상 두 경로를 병렬 처리**하여 즉시 전환 가능하게 한다 (CPU는 2배지만 UX 가치가 압도적)
- **Snapshot**: 모든 파라미터 + 내부 DSP 상태(envelope, filter coefficient)까지 포함
- **Deterministic**: 동일 입력 + 동일 스냅샷 → 동일 출력 (random seed 고정)

---

## 13.6 Loudness-Compliant Processing

### 13.6.1 타깃 플랫폼별 규격 (재정리)

| 플랫폼 | Integrated LUFS | True Peak | 기타 |
|--------|-----------------|-----------|------|
| Spotify | −14 LUFS | −1 dBTP (limiter는 -1 dB sample peak 기준) | Loud: −11, Normal: −14, Quiet: −23 |
| Apple Music | −16 LUFS | −1 dBTP | "Sound Check" — 큰 트랙만 낮춤 (소리 올리지 않음). AES TD1008 기반 |
| YouTube / YouTube Music | −14 LUFS | −1 dBTP | 큰 트랙만 낮춤 (quieter 트랙은 그대로) |
| Tidal | −14 LUFS | −1 dBTP | |
| Amazon Music | −14 LUFS | −2 dBTP | 더 엄격한 true-peak ceiling (코덱 헤드룸) |
| SoundCloud | −14 LUFS (추정) | −1 dBTP | 공식 수치 비공개, 업계 관측치 |
| **AES Streaming (TD1008, 2021)** | **−16 to −20 LUFS 범위 권고** | −1 dBTP | **AES TD1004(2015)는 TD1008(2021)로 대체됨.** 단일 타깃이 아닌 범위 |
| EBU R128 Broadcast | −23 LUFS (±0.5 LU) | −1 dBTP | 유럽 방송 |
| ATSC A/85 (US Broadcast) | −24 LKFS (±2 LU) | −2 dBTP | |
| Netflix | −27 LKFS (dialog-gated, ±2 LU) | −2 dBTP | Dialog 게이팅 기반 (대사 구간만 측정); 대사 부족 시 −24 LKFS integrated로 폴백 |

### 13.6.2 플러그인 내 Loudness 모드

```
┌──────────────────────────────────┐
│  Target Loudness                  │
│  [Spotify/YouTube (-14 LUFS) ▼]   │
│                                   │
│  Current: -13.8 LUFS integrated   │
│  Makeup: +0.2 dB auto             │
│                                   │
│  Window: (•) 30s recent  ( ) All  │
│  [ Reset Integrated ]             │
└──────────────────────────────────┘
```

### 13.6.3 자동 Makeup Gain

1. 사용자가 타깃 LUFS 선택 (예: -14)
2. Integrated LUFS 측정 (30초 or 전체)
3. 차이 계산: $\Delta = \text{LUFS}_\text{target} - \text{LUFS}_\text{current}$
4. 출력 gain에 $+\Delta$ dB 적용 (천천히 smoothing, 1초 단위)
5. True peak ceiling과 충돌 시 브릭월 리미터가 개입

### 13.6.4 측정 Window

- **전체 (Integrated)**: 트랙 처음부터 누적 (마스터링 final check용)
- **최근 30초**: 최근 30초 slide window (실시간 튜닝용)
- **Short-term (3초)**: loudness 변화 모니터링

---

## 13.7 Output Dithering (선택)

### 13.7.1 왜 필요한가

내부 처리는 **64-bit double precision float**이지만, 출력은 DAW에 따라 16-bit / 24-bit / 32-bit float이다. 16-bit / 24-bit로 내보낼 때 **quantization error**가 발생한다:

- 16-bit 다이내믹 레인지: 96.33 dB
- 24-bit: 144.49 dB
- 양자화 잡음이 신호와 **상관(correlated)** 되면 왜곡처럼 들림

**Dither**는 양자화 전 미세한 노이즈를 추가하여 이 상관을 끊는 기술이다.

### 13.7.2 TPDF Dither

**Triangular Probability Density Function** — 두 개의 독립 균등(uniform) 난수 합:

$$d = u_1 + u_2, \quad u_i \sim U\left[-\tfrac{1}{2}\,\text{LSB},\ +\tfrac{1}{2}\,\text{LSB}\right]$$

- Range: $d \in [-1, +1]$ LSB
- PDF: 삼각형 (0에서 peak, $\pm 1$ LSB에서 0)
- 평균 $E[d] = 0$, 분산 $\mathrm{Var}[d] = \tfrac{1}{6}\,\text{LSB}^2$ (two-sample sum of uniforms)
- **기대 noise power**: $\sigma_d^2 = 2 \cdot \tfrac{1}{12}\,\text{LSB}^2 = \tfrac{1}{6}\,\text{LSB}^2$ → RMS ≈ 0.408 LSB

**효과:**
- Quantization noise의 mean이 0
- Variance가 입력 신호와 독립
- 가청 왜곡이 **white noise floor**로 전환 (약 -90 dBFS for 16-bit)

### 13.7.3 Noise Shaping (선택)

TPDF 잡음을 spectrum상 **고주파 쪽으로 밀어넣기**:
- 가청 대역(20Hz–20kHz)의 잡음 레벨을 낮추고
- 20kHz 이상 대역에 집중 (청각 민감도 낮은 영역)

구현: 2차 또는 9차 IIR feedback shaping filter.

### 13.7.4 UI

```
┌──────────────────────────┐
│  Output Dither            │
│  ( ) Off                  │
│  (•) TPDF 16-bit          │
│  ( ) TPDF 24-bit          │
│  ( ) Shaped (9th order)   │
└──────────────────────────┘
```

### 13.7.5 구현 의사코드

```cpp
class TPDFDither {
    XorshiftRNG rng_;
    float lsb_;  // = 2^(-(bits-1))

public:
    void setBitDepth(int bits) {
        lsb_ = std::pow(2.0f, -(bits - 1));
    }

    float process(float in) {
        float u1 = rng_.nextFloat() - 0.5f;  // [-0.5, 0.5]
        float u2 = rng_.nextFloat() - 0.5f;
        float d = (u1 + u2) * lsb_;           // TPDF in [-1, +1] LSB
        return in + d;
    }
};
```

---

## 13.8 PDC (Plugin Delay Compensation)

### 13.8.1 왜 중요한가

DAW는 멀티트랙 재생 시 모든 트랙을 샘플 단위로 동기화한다. 플러그인이 latency를 유발하면 DAW가 **다른 트랙을 그만큼 미리 재생**해서 맞춰야 한다. 플러그인이 정확한 latency를 보고하지 않으면 **sync가 어긋남** → phasing·timing artifact.

### 13.8.2 Latency 보고 API

**VST3**:
```cpp
tresult PLUGIN_API getLatencySamples() override {
    return latencySamples_;
}
```

**AU**:
```cpp
virtual NSTimeInterval getLatency() const {
    return (double)latencySamples_ / sampleRate_;
}
```

**설정이 바뀔 때:**
```cpp
void onModeChange() {
    recomputeLatency();
    host_->setLatencyChanged();  // DAW에 알림
}
```

### 13.8.3 모드별 Latency 표

| 모드 조합 | Latency (at 48 kHz) |
|-----------|--------------------|
| Minimum-phase, 2× OS, no limiter | ~8 samples (0.17 ms) |
| Minimum-phase, 4× OS, no limiter | ~16 samples (0.33 ms) |
| Minimum-phase, 8× OS, no limiter | ~32 samples (0.67 ms) |
| Linear-phase EQ, 4× OS | ~2048 samples (42.7 ms) |
| Linear-phase EQ, 8× OS | ~4096 samples (85.3 ms) |
| + Brick-wall limiter (5ms lookahead) | +240 samples |

### 13.8.4 Latency 소스 상세

- **Oversampling FIR filter delay**: 각 polyphase branch의 group delay
- **Linear-phase EQ**: FIR tap 수의 절반 $(N-1)/2$
- **Look-ahead limiter**: 설정값 그대로
- **M/S encode/decode**: 0 샘플 (in-place)

### 13.8.5 검증

- DAW에 플러그인 로드 → 동일 트랙을 2개 복제 → 한쪽에만 플러그인 적용 → phase-invert 후 합산
- 정확히 PDC되었다면 **완벽한 null** (잡음 포함 제로)
- 1 샘플이라도 어긋나면 comb filtering 발생

---

## 13.9 High Sample Rate 지원

### 13.9.1 지원 샘플레이트

44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz

### 13.9.2 내부 오버샘플 비율 조정

입력 샘플레이트에 따라 **자동 조정**:

| 입력 SR | 내부 오버샘플 | 실질 처리 SR |
|---------|--------------|-------------|
| 44.1 kHz | 8× | 352.8 kHz |
| 48 kHz | 8× | 384 kHz |
| 88.2 kHz | 4× | 352.8 kHz |
| 96 kHz | 4× | 384 kHz |
| 176.4 kHz | 2× | 352.8 kHz |
| 192 kHz | 2× | 384 kHz |

목표는 **실질 처리 SR을 350 kHz 이상**으로 유지하여 aliasing을 충분히 밀어내는 것.

### 13.9.3 CPU 최적화

- 88.2 / 96 kHz 이상 입력 시 2× OS만으로도 충분 → **CPU 50% 감소**
- 사용자가 "High Quality" 옵션 켜면 한 단계 더 올릴 수 있음 (192→4×)

### 13.9.4 FIR 설계 요구사항

- Anti-aliasing filter cutoff: $0.45 \cdot f_\text{target}$
- Stopband attenuation: ≥ 90 dB (24-bit 다이내믹 지원)
- Passband ripple: ≤ 0.01 dB
- **Kaiser window 권장 β** (Kaiser-Bessel 공식):
  - 90 dB stopband 목표: $\beta = 0.1102 \cdot (A - 8.7) \approx 8.96$ (A=90 dB 대입)
  - Transition bandwidth $\Delta f \approx \frac{(A - 8)}{2.285 \cdot \pi \cdot N}$ → 90 dB, N=64 taps 기준 $\Delta f / f_s \approx 0.2$
  - 4× OS + 90 dB + fs=48 kHz → 실용적으로 tap 수 N = 64–128 사이 반복 피팅
- 대안: equiripple(Parks-McClellan) 설계 — 동일 사양에 더 적은 tap 수 가능하나 passband ripple 균일분포

---

## 13.10 Stereo Linking / Unlinking

### 13.10.1 옵션

1. **Fully Linked (Mono-Linked)**: 같은 processing을 L, R에 동일 적용 (vintage hardware 에뮬)
2. **Stereo (L/R Independent)**: L과 R을 독립된 tube/transformer 인스턴스로 처리 (진공관 한 쌍 모델링)
3. **M/S**: §13.2 참조

### 13.10.2 Component Variation과의 상호작용

본 플러그인은 [문서 06](./06-stochastic-component-modeling.md)에 따라 **부품 값의 개체차(stochastic variation)** 를 모델링한다. L/R 독립 처리 시 seed 전략이 중요하다:

**옵션 A: 다른 seed (기본)**
- L과 R이 서로 약간씩 다른 tube/transformer 특성
- 자연스러운 "개체차" 재현 (실제 아날로그 하드웨어는 L/R이 완벽히 동일하지 않음)
- **장점**: 스테레오 이미지가 풍부해짐
- **단점**: 모노 호환성 미세 저하 (< 0.1 dB)

**옵션 B: 동일 seed ("Mono-Compatible")**
- L=R로 완전 동일 처리
- 모노 다운믹스에서 완벽한 대칭
- 마스터링 엔지니어가 strict mono-compat를 요구할 때 선택

### 13.10.3 UI

```
┌──────────────────────────────┐
│  Stereo Handling              │
│  (•) Stereo (L/R independent) │
│  ( ) Mid/Side                 │
│  ( ) Mono-Linked              │
│                               │
│  [ Mono-Compatible Seed: ON ] │
└──────────────────────────────┘
```

---

## 13.11 실무 프리셋 (마스터링 전용)

### 13.11.1 "Mix Bus Glue"
- **용도**: 믹스 버스에 가볍게 얹어서 응집력(cohesion) 추가
- Drive: 2/10 (아주 약함)
- Character: tube-leaning
- M/S: Stereo (linked via same seed)
- Phase: Minimum
- Limiter: Off
- Target LUFS: off

### 13.11.2 "Master Color"
- **용도**: 마스터링에서 아날로그 색 추가
- Drive: 4/10 (중간)
- M/S: 독립 (Mid 강, Side 약)
- Phase: Minimum
- Limiter: Soft clip @ -1 dBTP
- Target: Spotify (-14 LUFS)

### 13.11.3 "Loudness Push"
- **용도**: 경쟁적 라우드니스 필요 시
- Drive: 7/10 (강함)
- M/S: Stereo
- Phase: Minimum
- Limiter: Brick-wall @ -1 dBTP (5ms lookahead)
- Target: -11 LUFS (Spotify loud)

### 13.11.4 "Transparent"
- **용도**: 거의 선형, 트랜스포머 색만 살짝
- Drive: 1/10
- Tube path: Off 또는 최소
- Transformer path: low-mid shaping
- Phase: Linear
- Limiter: Off

### 13.11.5 "Vintage Tape"
- **용도**: 아날로그 테이프 warming
- Drive: 5/10
- Character: tape-style asymmetry
- Age: heavy (high component variation)
- M/S: Mono-linked
- Phase: Minimum
- Limiter: Soft @ -0.5 dBTP

---

## 13.12 검증 체크리스트 (마스터링 전용)

### 13.12.1 Functional 검증

- [ ] M/S 모드에서 처리 없이 encode→decode → 원본과 sample-exact 일치
- [ ] M/S 모드에서 mono 재생 ($L+R$) 시 왜곡·smearing 없음
- [ ] L/R 독립 모드 + mono-compatible seed → mono diff < -90 dB
- [ ] A/B switch: 10ms crossfade 중 click·pop 없음
- [ ] A/B LUFS 매치: Active vs Bypass 라우드니스 오차 < 0.1 LU
- [ ] Null test: Bypass 상태에서 null 신호 < -100 dBFS

### 13.12.2 True Peak 및 Loudness

- [ ] Safety limiter on → 출력 True Peak < 타깃 dBTP (4× OS 측정)
- [ ] Safety limiter off, heavy drive → ISP 증가 확인 + 경고 표시
- [ ] Target LUFS 설정 → 측정 LUFS가 ±0.2 LU 오차 내 수렴
- [ ] Integrated LUFS (30초) 실시간 갱신
- [ ] Short-term LUFS (3초) 정확성

### 13.12.3 Phase 및 Latency

- [ ] Minimum-phase 모드: phase response는 회로 이론값과 일치
- [ ] Linear-phase 모드: group delay = constant (표준편차 < 0.01 샘플)
- [ ] Linear-phase 모드: 주파수별 위상 shift = 0° (magnitude 경계 제외)
- [ ] PDC 보고값 = 실측 latency (DAW null test로 확인)
- [ ] 모드 전환 시 `setLatencyChanged()` 호출 확인

### 13.12.4 Sample Rate 및 Aliasing

- [ ] 44.1, 48, 88.2, 96, 176.4, 192 kHz 모두 동작
- [ ] 각 SR에서 aliasing < -90 dBc (fs/2 근처 impulse response 측정)
- [ ] SR 전환 시 clicks·pops 없음 (internal state 재초기화)

### 13.12.5 Dither 및 Output

- [ ] TPDF dither on/off 시 quantization SNR 측정 → dither 적용 시 향상 확인
- [ ] Noise shaped dither: 가청 대역 noise floor 감소 + 20kHz 이상 증가 확인
- [ ] 16-bit / 24-bit 출력 모두 spec 준수

### 13.12.6 Stress 및 Edge Cases

- [ ] 0 dBFS 입력 + max drive → output은 target dBTP 이하
- [ ] DC 입력 → 출력 DC offset < 0.001 linear (-60 dB)
- [ ] Silence 입력 → 출력 silence (floating point denormal 없음)
- [ ] 긴 세션 (8시간) 재생 → integrated LUFS 메모리 누수·drift 없음
- [ ] Automation 격변 (파라미터 급변) → 스무스 전환, zipper noise 없음

---

## 13.13 관련 문서 및 참고문헌

### 연관 문서
- [07. 구현 전략](./07-implementation-strategies.md) — DSP 구현 디테일
- [09. 측정 및 검증](./09-measurement-and-validation.md) — 기본 측정 방법론
- [08. 경쟁 분석](./08-competitive-analysis.md) — 타 제품 대비 포지셔닝

### 표준 및 규격
- **ITU-R BS.1770-5** (2023-11) — Loudness 및 True Peak 측정 (현재 최신판; BS.1770-4(2015)의 후속)
  - https://www.itu.int/rec/R-REC-BS.1770-5-202311-I/en
- **EBU R128** (최신 2020 개정) — 유럽 방송 라우드니스 규격
- **AES TD1008.1.21-9** (2021) — Audio Streaming Loudness 권장 (**TD1004(2015)를 대체**)
  - https://www.aes.org/technical/documentDownloads.cfm?docID=731
- **ATSC A/85** (CALM Act 준거) — US 방송 라우드니스 규격

### 참고 도서 및 논문
- Bob Katz, *Mastering Audio: The Art and the Science* (3rd ed., 2014)
- Ian Shepherd, *Mastering for iTunes, Spotify, and Beyond* (2019)
- Earl Vickers, "The Loudness War: Background, Speculation, and Recommendations", AES Convention Paper (2010)
- Thomas Lund, "Stop Counting Samples", AES 121st Convention Paper (2006) — intersample peak

### 오픈소스 도구
- **libebur128** — BS.1770 loudness 측정 (MIT License)
- **JUCE dsp::Oversampling** — 4× / 8× 오버샘플링 구현 reference
- **FFTW / KFR** — Linear-phase FIR convolution

---

*작성: 2026-04-17 · 본 문서는 감사(audit) 결과 누락된 마스터링 필수 기능 6종을 체계적으로 보강한다.*
