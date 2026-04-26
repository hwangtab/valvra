# 09. 측정과 검증 방법론

> **연관 문서:** [05 심리음향](./05-harmonic-spectrum-and-psychoacoustics.md) · [08 경쟁 분석](./08-competitive-analysis.md)

> **핵심 원칙:** "들린다"는 주관적 판단 이전에, 측정 가능한 물리적 차이가 있어야 한다. 이 문서는 본 플러그인이 경쟁사와 기술적으로 다름을 측정으로 증명하는 방법을 정의한다.

---

## 1. Null Test — 최강의 비교 도구

### 1.1 Null Test 원리

두 처리 결과의 위상을 반전하여 합산하면, 동일한 처리는 완전히 상쇄(null)된다:

$$\text{residual}(t) = y_1(t) - y_2(t)$$

두 처리가 동일하면 residual = 0 (−∞ dBFS). 다르면 residual이 들린다.

**이 도구로 확인 가능한 것:**
- 동일 플러그인의 두 인스턴스가 같은가 (개체차 확인)
- 시간에 따라 출력이 변하는가 (시변성 확인)
- 레벨에 따라 특성이 변하는가 (프로그램 의존성 확인)

### 1.2 Null Test 절차 (DAW 기반)

```
1. 원본 트랙을 두 번 복사
2. 양쪽에 플러그인 적용 (동일 설정)
3. 한 쪽을 폴라리티 반전 (180° flip)
4. 두 트랙 합산
5. 결과 레벨 측정
```

**해석:**
- < −60 dBFS: 사실상 동일
- −60 ~ −40 dBFS: 미세한 차이 (청취 불가)
- −40 ~ −20 dBFS: 뚜렷한 차이 (들릴 수 있음)
- > −20 dBFS: 매우 큰 차이

### 1.3 본 플러그인 기대 Null Test 결과

| 비교 대상 | 기대 null 깊이 | 이유 |
|----------|-------------|------|
| 같은 인스턴스, 같은 seed, 시작부터 | < −80 dBFS | 동일 처리 |
| 다른 seed, 같은 input | −40 ~ −20 dBFS | 개체차 |
| 같은 seed, 1분 후 replay | −60 ~ −40 dBFS | 시변성 (열적 드리프트) |
| 같은 seed, 강한 input 후 | −50 ~ −30 dBFS | 캐소드 바운스 잔류 |

---

## 2. THD+N 스펙트럼 측정

### 2.1 측정 설정

**표준 조건:**
- 입력: 1kHz 사인파 (THD+N 측정의 기준)
- 레벨 스윕: −30 dBFS ~ −3 dBFS (1dB 스텝)
- 주파수 스윕: 20Hz ~ 20kHz (1/3 옥타브 또는 1/6 옥타브)

**소프트웨어:**
- Room EQ Wizard (REW) — 무료, 정밀
- ARTA — 전문가용
- Audio Precision APx555 (하드웨어) — 업계 최고 정밀도

### 2.2 2D THD 히트맵 생성

주파수 × 레벨의 2D 공간에서 THD를 색상으로 표현:

```
레벨 (dBFS)
  0 │ ████████████████████ 높은 왜곡 영역
 -6 │ ██████████████░░░░░░
-12 │ ████████░░░░░░░░░░░░
-18 │ ████░░░░░░░░░░░░░░░░
-24 │ ██░░░░░░░░░░░░░░░░░░
-30 │ ░░░░░░░░░░░░░░░░░░░░
    └────────────────────
     20  100  1k   5k  20kHz

색상: ░ < 0.01% │ ▒ 0.01-0.1% │ ▓ 0.1-1% │ █ > 1%
```

이 히트맵이 "중역에서 saturation이 가장 강한" 물리적 특성을 시각적으로 보여준다.

### 2.3 개별 하모닉 레벨 측정

단순 THD가 아닌 H2, H3, H4, H5 각각을 주파수별로 측정:

```python
import numpy as np
from scipy.fft import rfft, rfftfreq

def measure_harmonics(signal, fs, fundamental, num_harmonics=7):
    N = len(signal)
    freqs = rfftfreq(N, 1/fs)
    spectrum = np.abs(rfft(signal)) / N * 2

    fundamental_level = spectrum[np.argmin(np.abs(freqs - fundamental))]
    harmonics = {}
    for h in range(2, num_harmonics + 1):
        target_freq = fundamental * h
        if target_freq < fs / 2:
            idx = np.argmin(np.abs(freqs - target_freq))
            harmonics[f'H{h}'] = 20 * np.log10(spectrum[idx] / fundamental_level)
    return harmonics
```

---

## 3. IMD 측정

### 3.1 SMPTE IMD 절차

```
입력 신호 생성:
  60Hz (레벨 A) + 7000Hz (레벨 A/4)
  총 레벨: 원하는 측정 레벨

측정:
  출력에서 7000 ± n×60 Hz 성분 추출 (n = 1, 2, 3, ...)

SMPTE IMD = sqrt(sum of sideband^2) / 7000Hz_level × 100%
```

**Python 구현:**

```python
def measure_smpte_imd(signal, fs, f_low=60, f_high=7000, num_sidebands=5):
    N = len(signal)
    freqs = rfftfreq(N, 1/fs)
    spectrum = np.abs(rfft(signal)) / N * 2

    carrier_idx = np.argmin(np.abs(freqs - f_high))
    carrier_level = spectrum[carrier_idx]

    sideband_power = 0
    for n in range(1, num_sidebands + 1):
        for sign in [1, -1]:
            sb_freq = f_high + sign * n * f_low
            if 0 < sb_freq < fs / 2:
                idx = np.argmin(np.abs(freqs - sb_freq))
                sideband_power += spectrum[idx] ** 2

    return 100 * np.sqrt(sideband_power) / carrier_level
```

### 3.2 CCIF IMD (DFD)

```
입력: 19000Hz + 20000Hz (동일 레벨)
측정: 1000Hz (차음) 레벨

CCIF_IMD = A_1kHz / A_19kHz × 100%
```

### 3.3 Multi-Tone IMD

여러 주파수 동시 입력으로 복잡한 음악 신호에서의 IMD를 측정:

```python
def generate_multitone(frequencies, fs, duration, amplitude_dbfs=-18):
    t = np.linspace(0, duration, int(fs * duration))
    signal = np.zeros_like(t)
    amplitude = 10 ** (amplitude_dbfs / 20)
    for f in frequencies:
        phase = np.random.uniform(0, 2 * np.pi)  # 랜덤 위상
        signal += amplitude / len(frequencies) * np.sin(2 * np.pi * f * t + phase)
    return signal

# 음악적 주파수 선택 (도, 미, 솔, 도)
freqs = [261.6, 329.6, 392.0, 523.2]  # C4, E4, G4, C5
```

---

## 4. Phase Response와 Group Delay 측정

### 4.1 위상 응답의 중요성

진폭 응답(magnitude response)만으로는 아날로그 장비의 캐릭터를 완전히 설명할 수 없다. 동일한 magnitude를 가진 두 필터도 위상 응답이 다르면 과도 응답(transient response)과 스테레오 이미지가 달라진다. 본 플러그인은 트랜스포머와 진공관 단간 커플링의 위상 특성을 사실적으로 모델링하므로, 이를 측정으로 검증해야 한다.

### 4.2 이론 배경

주파수 응답 $H(j\omega) = |H(j\omega)| \cdot e^{j\phi(\omega)}$에서 위상 $\phi(\omega)$는 일반적으로 $\pm\pi$ 범위에서 wrap되어 나타난다. 의미 있는 분석을 위해서는 **unwrapped phase**가 필요하다.

**Group delay**는 위상의 주파수 미분에 음수를 취한 값으로, "각 주파수 성분이 시스템을 통과하는 데 걸리는 시간"을 의미한다:

$$\tau_g(\omega) = -\frac{d\phi(\omega)}{d\omega}$$

- **Minimum-phase 시스템:** magnitude와 phase가 Hilbert 변환으로 연결됨. 대부분의 아날로그 장비는 minimum-phase에 가깝다.
- **Linear-phase 시스템:** group delay가 주파수에 관계없이 일정. 주로 디지털 FIR 필터.
- **아날로그 실기 특성:** 트랜스포머, 진공관 커플링 커패시터, 출력 트랜스의 누설 인덕턴스로 인해 **고주파에서 group delay가 증가**하는 경향이 있다 (수 μs ~ 수십 μs 수준).

### 4.3 측정 절차

**신호:** 20Hz ~ 20kHz logarithmic sweep (ESS — Exponential Sine Sweep). 로그 스윕은 주파수당 에너지가 균등하지 않지만 SNR이 높고 Farina 기법(Section 6)과 결합 시 하모닉 분리가 가능하다.

**Python 구현 (scipy.signal.csd 기반 전달함수 추정):**

```python
import numpy as np
from scipy.signal import csd, welch

def measure_phase_response(x, y, fs, nperseg=8192):
    """
    x: 입력 신호 (sweep)
    y: 플러그인 출력
    fs: 샘플링 주파수
    returns: freqs, magnitude_dB, phase_unwrapped_rad, group_delay_s
    """
    # Cross-spectral density 및 Auto-spectral density
    f, Pxy = csd(x, y, fs=fs, nperseg=nperseg)
    _, Pxx = welch(x, fs=fs, nperseg=nperseg)

    # 전달함수 H(f) = Pxy / Pxx (H1 estimator)
    H = Pxy / (Pxx + 1e-20)

    magnitude_dB = 20 * np.log10(np.abs(H) + 1e-20)
    phase_wrapped = np.angle(H)
    phase_unwrapped = np.unwrap(phase_wrapped)

    # Group delay: τ = -dφ/dω, ω = 2πf
    group_delay = -np.gradient(phase_unwrapped, 2 * np.pi * f)

    return f, magnitude_dB, phase_unwrapped, group_delay
```

### 4.4 본 플러그인에서의 기대 특성

| 영역 | 기대 group delay | 의미 |
|------|-----------------|------|
| 100Hz ~ 1kHz | < 20 μs, 거의 평탄 | 중역은 위상 왜곡 최소 |
| 1kHz ~ 8kHz | 20 ~ 50 μs, 완만한 증가 | 커플링 캡의 영향 |
| 8kHz ~ 20kHz | 50 ~ 200 μs, 급격한 증가 | 트랜스포머 누설 인덕턴스 |

**검증 포인트:** 기존 플러그인(정적 waveshaper)은 group delay가 샘플 레이턴시 외에는 완전 평탄하다. 본 플러그인에서 위와 같은 주파수별 변화가 측정되면 물리적 네트워크 모델링이 작동함을 증명한다.

---

## 5. Harmonic Phase Relationship

### 5.1 위상이 음색에 미치는 영향

하모닉의 **크기**만이 음색을 결정하는 것이 아니라, 기본파(fundamental)에 대한 각 하모닉의 **상대 위상**도 중요하다. 같은 H2/H3 비율이라도 위상 관계가 다르면 과도 파형(transient shape)이 달라지며, 특히 드럼·타악기의 어택 성질에 크게 영향을 준다.

**심리음향적 관찰:**
- 하모닉이 fundamental과 **동위상**(0°)으로 정렬되면 파형이 비대칭(asymmetric)이 되어 "단방향 클리핑"처럼 들림 → 2차 하모닉의 전형적 패턴
- **반대 위상**(180°)이면 대칭적(symmetric) 왜곡 → 3차 하모닉의 전형적 패턴
- 진공관의 single-ended 클래스 A 동작은 H2가 주로 0° 또는 180°에 고정되지만, 캐소드 피드백·트랜스포머 커플링으로 인해 수십 도의 위상 편차가 발생한다.

### 5.2 진공관 vs 반도체의 하모닉 위상 패턴 (요약)

| 회로 타입 | H2 위상 특성 | H3 위상 특성 |
|-----------|-------------|-------------|
| 진공관 single-ended (12AX7) | ~0° 고정, bias에 따라 ±30° 변동 | ~180°, level-dependent |
| 진공관 push-pull | H2 상쇄 경향, 위상 불안정 | ~180°, 명확 |
| BJT 반도체 클리핑 | 가파른 위상 전환 (스위칭) | 급격한 180° 점프 |
| FET soft-knee | 진공관과 유사, 그러나 주파수 의존성 적음 | 진공관 대비 평탄 |

### 5.3 측정 절차

FFT로 기본파 bin과 각 하모닉 bin의 복소 스펙트럼 값을 추출하고, 위상차를 계산한다.

```python
import numpy as np
from scipy.fft import rfft, rfftfreq

def measure_harmonic_phases(signal, fs, fundamental, num_harmonics=7):
    """
    각 하모닉의 기본파 대비 상대 위상을 측정.
    fundamental 대비 h번째 하모닉의 상대 위상 = angle(Y[h*f0]) - h * angle(Y[f0])
    """
    N = len(signal)
    # 윈도우 적용 (Hann) — spectral leakage 최소화
    window = np.hanning(N)
    spectrum = rfft(signal * window)
    freqs = rfftfreq(N, 1 / fs)

    fund_idx = np.argmin(np.abs(freqs - fundamental))
    fund_phase = np.angle(spectrum[fund_idx])

    harmonic_phases = {}
    for h in range(2, num_harmonics + 1):
        target = fundamental * h
        if target < fs / 2:
            idx = np.argmin(np.abs(freqs - target))
            # 하모닉의 "절대" 위상에서 기본파 위상의 h배를 뺀 것이
            # 시간 원점에 독립적인 상대 위상
            relative_phase = np.angle(spectrum[idx]) - h * fund_phase
            # -π ~ π로 wrap
            relative_phase = np.mod(relative_phase + np.pi, 2 * np.pi) - np.pi
            harmonic_phases[f'H{h}'] = np.degrees(relative_phase)
    return harmonic_phases
```

### 5.4 본 플러그인의 검증

**기대 결과 (−12 dBFS 1kHz 기준):**
- H2 상대 위상: 0° ± 20° (드라이브 세팅에 따라 변동)
- H3 상대 위상: 180° ± 30°
- 드라이브가 증가하면 H2 위상이 bias 이동과 함께 수 도~수십 도 이동 → 시변성의 또 다른 증거

**기존 플러그인 한계:** 정적 waveshaper는 하모닉 위상이 완전히 고정되어 있고 level에 따라 불변. 본 플러그인이 drive/level에 따라 H2 위상이 드리프트한다면 물리 모델의 비선형성이 제대로 구현되었음을 증명한다.

---

## 6. Farina Exponential Swept-Sine

### 6.1 배경

Angelo Farina가 AES 108th Convention (2000)에서 제안한 방법으로, **한 번의 로그 스윕 측정**으로 선형 impulse response와 모든 차수의 하모닉 IR을 동시에 분리하여 얻을 수 있다. 현재 룸 어쿠스틱 측정과 비선형 시스템 식별의 표준이 되었다.

### 6.2 수학적 표현

**입력 스윕** (exponential sine sweep, ESS):

$$x(t) = \sin\left( \frac{2\pi f_1 T}{\ln(f_2 / f_1)} \left[ \exp\left(\frac{t}{T} \ln\frac{f_2}{f_1}\right) - 1 \right] \right), \quad 0 \le t \le T$$

여기서 $f_1, f_2$는 시작·종료 주파수, $T$는 스윕 길이.

**Inverse filter** $x^{-1}(t)$는 $x(t)$를 시간 역방향으로 재생하고 진폭에 $e^{-t/T \cdot \ln(f_2/f_1)}$을 곱한 것이다:

$$x^{-1}(t) = x(T - t) \cdot \exp\left(-\frac{t}{T} \ln\frac{f_2}{f_1}\right)$$

**핵심 성질:** $x(t) * x^{-1}(t) = \delta(t)$ (Dirac delta). 즉 시스템 출력 $y(t)$를 $x^{-1}(t)$와 컨볼루션하면 선형 IR이 얻어지고, 비선형 성분(하모닉)은 **시간적으로 음의 시간으로 분리**되어 나타난다.

### 6.3 하모닉 IR 분리

$k$차 하모닉의 임펄스 응답 $h_k(t)$는 선형 IR보다 $\Delta t_k$만큼 **앞서서** 나타난다:

$$\Delta t_k = T \cdot \frac{\ln k}{\ln(f_2 / f_1)}$$

예: $f_1=20\text{Hz}, f_2=20\text{kHz}, T=10\text{s}$일 때
- $\Delta t_2 \approx 1.00\text{s}$ (2차 하모닉 IR)
- $\Delta t_3 \approx 1.59\text{s}$
- $\Delta t_4 \approx 2.00\text{s}$
- $\Delta t_5 \approx 2.32\text{s}$

따라서 deconvolution 결과 시간축에서 적절한 구간을 잘라내면 각 차수 하모닉의 IR을 독립적으로 얻을 수 있다.

### 6.4 Python 구현

```python
import numpy as np
from scipy.signal import fftconvolve

def generate_ess(f1, f2, T, fs):
    """Exponential Sine Sweep + inverse filter 생성."""
    t = np.arange(0, T, 1 / fs)
    K = T * 2 * np.pi * f1 / np.log(f2 / f1)
    L = T / np.log(f2 / f1)
    x = np.sin(K * (np.exp(t / L) - 1))

    # Inverse filter: 시간 역방향 + 진폭 엔벨로프
    inv_envelope = np.exp(-t / L)
    x_inv = x[::-1] * inv_envelope
    # Magnitude 보정 (Farina 2000, eq. 14)
    x_inv *= (f1 / f2)  # 대략적인 정규화

    return x, x_inv, t

def deconvolve_ir(y, x_inv, fs, f1, f2, T):
    """선형 IR + 하모닉 IR 추출."""
    ir_full = fftconvolve(y, x_inv, mode='full')

    # 선형 IR은 전체 응답의 "끝" 부근에 위치
    linear_start = len(x_inv) - 1  # 합성 지점
    linear_ir = ir_full[linear_start:linear_start + int(0.5 * fs)]

    # k차 하모닉 IR의 시작점 (선형 IR 앞쪽)
    harmonic_irs = {}
    for k in range(2, 6):
        dt_k = T * np.log(k) / np.log(f2 / f1)
        offset = int(dt_k * fs)
        start = linear_start - offset
        length = int(0.1 * fs)  # 100ms 추출
        harmonic_irs[f'H{k}'] = ir_full[start - length:start]
    return linear_ir, harmonic_irs

# 사용 예
fs = 96000
x, x_inv, _ = generate_ess(20, 20000, 10, fs)
y = plugin.process(x)  # 플러그인 출력
lin_ir, harm_irs = deconvolve_ir(y, x_inv, fs, 20, 20000, 10)
```

### 6.5 장점 및 본 플러그인 활용

- **단일 측정**으로 선형 IR(주파수 응답 + 위상 + group delay) + 2~5차 하모닉 IR 추출
- 각 하모닉 IR을 FFT하면 하모닉의 **주파수 의존성**(어느 대역에서 어느 하모닉이 우세한지) 파악
- 본 플러그인의 "중역 saturation 강조" 특성이 H2/H3 IR의 주파수 분포에서 직접 확인 가능
- 서로 다른 drive 세팅의 ESS 측정을 비교하면 비선형 특성의 level-dependent 거동을 한눈에 시각화

---

## 7. True Peak / Inter-Sample Peak Detection

### 7.1 문제 정의

디지털 샘플값이 0 dBFS를 넘지 않아도, **샘플 사이**에 위치한 연속 신호(analog reconstructed waveform)가 0 dBFS를 초과하는 현상이 발생한다. 이를 **inter-sample peak** 또는 **true peak**라 한다. 표본화 정리(Shannon-Nyquist)에 따르면 샘플값은 대역 제한된 연속 신호의 점 값일 뿐이며, 그 사이에는 sinc 보간으로 복원되는 peak가 있다.

**실무적 문제:**
- DAC나 다운스트림 아날로그 단에서 클리핑 발생
- 손실 인코딩(MP3, AAC)으로 변환 시 true peak가 추가로 증가 → 스트리밍 플랫폼의 Loudness Normalization 후 클리핑 유발
- 마스터링에서 사용되는 플러그인은 true peak를 측정·표시해야 함

### 7.2 ITU-R BS.1770 권고 (True-Peak Annex 2)

ITU-R BS.1770-4/-5 Annex 2는 **최소 4배 오버샘플링** 후 peak를 측정하도록 규정한다(상위 샘플레이트: 48 kHz → 192 kHz). 실무적으로는 4× polyphase FIR 또는 IIR upsampler로 구현한다. 표준은 먼저 12.04 dB(2-bit shift) 감쇠를 적용한 뒤 정수 산술 헤드룸을 확보하고 나서 FIR로 4× 업샘플하도록 규정한다.

**알고리즘:**
```
1. 입력 샘플 스트림 x[n] (fs)
2. 4× 업샘플 (polyphase FIR, 예: 48 tap, Kaiser window)
3. 업샘플된 스트림 y[m] (4·fs)에서 max(|y[m]|) 추출
4. dBTP = 20 · log10(peak_amplitude)
```

### 7.3 C++ 의사코드

```cpp
class TruePeakMeter {
    PolyphaseFIR upsampler_4x;   // 4× oversampling FIR
    float peak_linear = 0.0f;

public:
    void process(const float* input, int num_samples) {
        std::array<float, 4> upsampled;
        for (int n = 0; n < num_samples; ++n) {
            upsampler_4x.process(input[n], upsampled.data());
            for (float s : upsampled) {
                float abs_s = std::fabs(s);
                if (abs_s > peak_linear) peak_linear = abs_s;
            }
        }
    }

    float getTruePeakDBTP() const {
        if (peak_linear < 1e-20f) return -144.0f;
        return 20.0f * std::log10(peak_linear);
    }

    void reset() { peak_linear = 0.0f; }
};
```

**권장 FIR:** Kaiser window, β=8.6, 48~64 tap. 저역 통과 차단주파수 ≈ 0.45·fs. 프리링잉(pre-ringing)을 줄이기 위해 minimum-phase FIR로 설계하는 것도 고려할 수 있다.

### 7.4 권장 한계 및 본 플러그인 적용

| 용도 | 권장 상한 | 비고 |
|------|---------|------|
| 스트리밍 마스터 (Spotify/Apple Music) | −1.0 dBTP | 인코딩 헤드룸 확보 |
| 보수적 마스터 (손실 코덱 안전) | −2.0 dBTP | 다단 처리 체인 대응 |
| 방송 (EBU R128) | −1.0 dBTP | 법적 규정 |

**본 플러그인에서의 측정:**
- 플러그인 내부에 output 단 바로 전에 True Peak 모듈을 상시 동작
- GUI에 현재 dBTP와 피크 홀드 표시
- drive 증가로 인한 하모닉 추가가 true peak를 얼마나 증가시키는지 측정 → saturation이 "volume"으로만 들리지 않고 "headroom"을 소비함을 사용자에게 가시화

---

## 8. LUFS 라우드니스 측정 (ITU-R BS.1770)

> 2026년 현재 최신 버전은 **ITU-R BS.1770-5 (2023-11)**. BS.1770-4 (2015)와 측정 원리 (K-weighting, 400 ms 블록 gating, True-peak 4× oversample)는 동일하며, -5는 True-peak 필터 계수의 명시적 표현과 surround 채널 구성(Rs/Ls/Lss/Rss 등 immersive audio)을 확장했다. 본 문서의 식·계수는 BS.1770-5와 BS.1770-4 모두에 호환된다.

### 8.1 측정 체인

ITU-R BS.1770은 인간 청각 가중된 라우드니스를 정의한다. 측정 체인은:

```
입력 → K-weighting 필터 → 각 채널 제곱 → 채널 가중합 → 시간 평균 → LUFS
```

### 8.2 K-weighting 필터

두 단의 IIR 필터로 구성:

**Stage 1 — Pre-filter (high shelf):** 고역 강조, ~1.5kHz 이상 +4 dB
**Stage 2 — RLB (Revised Low-frequency B-weighting):** high-pass, 60Hz 이하 감쇠

ITU-R BS.1770-4 Annex 1은 48kHz 기준 biquad 계수를 직접 제시한다. 다른 샘플레이트에서는 bilinear transform으로 재계산.

```python
# 48kHz 기준 K-weighting 계수 (ITU-R BS.1770-4)
# Pre-filter (high shelf)
b_pre = [1.53512485958697, -2.69169618940638, 1.19839281085285]
a_pre = [1.0,              -1.69065929318241, 0.73248077421585]

# RLB filter (high-pass)
b_rlb = [1.0, -2.0, 1.0]
a_rlb = [1.0, -1.99004745483398, 0.99007225036621]
```

### 8.3 채널 가중치

| 채널 | 가중치 $G_i$ |
|------|-------------|
| L, R (front L/R) | 1.0 (0 dB) |
| C (center) | 1.0 (0 dB) |
| LFE | 0 (측정 제외) |
| Ls, Rs (surround) | 1.41 (정확히 $10^{1.5/10}$ ≈ +1.5 dB) |

> BS.1770-5의 immersive 확장에서는 상단 채널(top-front/back, Lss/Rss 등)도 1.41 또는 지정된 값을 사용. 스테레오·모노 경우 C·Ls·Rs는 존재하지 않아 L+R만 합산.

라우드니스 (per block):

$$L = -0.691 + 10 \log_{10} \sum_i G_i \cdot \frac{1}{T} \int_0^T y_i^2(t) \, dt \quad [\text{LUFS}]$$

### 8.4 시간 적분 타입

| 측정 | 윈도우 | 용도 |
|------|-------|------|
| **Momentary (M)** | 400 ms, overlap 75% | 순간 라우드니스 미터 |
| **Short-term (S)** | 3 s, overlap 75% | 평균 청취 느낌 |
| **Integrated (I)** | 전체 프로그램, gated | 마스터링 타깃 측정 |

**Integrated gating:**
1. **Absolute gate:** 블록 라우드니스가 $-70$ LUFS 미만인 블록 제외
2. **Relative gate:** ungated 평균보다 10 LU 낮은 블록 제외
3. 남은 블록들의 평균으로 Integrated LUFS 결정

**Loudness Range (LRA):** Short-term LUFS의 10 ~ 95 퍼센타일 차이 (LU 단위). 동적 범위의 심리음향적 지표.

### 8.5 Python 스켈레톤

```python
from scipy.signal import lfilter
import numpy as np

def k_weight(x, fs):
    # 48kHz 계수 — 다른 fs는 bilinear transform으로 재계산 필요
    b_pre = [1.53512485958697, -2.69169618940638, 1.19839281085285]
    a_pre = [1.0, -1.69065929318241, 0.73248077421585]
    b_rlb = [1.0, -2.0, 1.0]
    a_rlb = [1.0, -1.99004745483398, 0.99007225036621]
    y = lfilter(b_pre, a_pre, x)
    y = lfilter(b_rlb, a_rlb, y)
    return y

def integrated_lufs(channels, fs, block_ms=400, overlap=0.75):
    """channels: list of 1D arrays per channel."""
    weights = {'L': 1.0, 'R': 1.0, 'C': 1.0, 'Ls': 1.41, 'Rs': 1.41}
    block = int(fs * block_ms / 1000)
    hop = int(block * (1 - overlap))

    # 채널별 K-weighting 및 프레임별 MS 계산
    msq = []  # per-block mean-square sum (가중치 적용)
    filtered = [k_weight(ch, fs) for ch in channels]
    n_frames = (len(filtered[0]) - block) // hop + 1
    for i in range(n_frames):
        s = 0.0
        for g, ch in zip(weights.values(), filtered):
            seg = ch[i * hop:i * hop + block]
            s += g * np.mean(seg ** 2)
        msq.append(s)
    msq = np.array(msq)

    # Absolute gate (-70 LUFS)
    block_loudness = -0.691 + 10 * np.log10(msq + 1e-20)
    passed = msq[block_loudness > -70]
    if len(passed) == 0:
        return -np.inf

    # Relative gate
    ungated_mean = 10 * np.log10(np.mean(passed)) - 0.691
    threshold = ungated_mean - 10
    mask = -0.691 + 10 * np.log10(passed) > threshold
    gated = passed[mask]
    if len(gated) == 0:
        return -np.inf

    return -0.691 + 10 * np.log10(np.mean(gated))
```

### 8.6 표준 타깃 라우드니스 (2025 기준)

| 플랫폼 / 규격 | Integrated LUFS (playback target) | True Peak 권장 상한 | 정규화 방향 | 비고 |
|--------------|-----------------------------------|---------------------|-------------|------|
| Spotify (기본) | −14 LUFS | −1 dBTP (loud master는 −2) | 양방향 (loud↓, quiet↑ with limiter) | "Loud" 프로파일 −11, "Quiet" −19 |
| Apple Music (Sound Check on) | −16 LUFS | −1 dBTP | **아래로만** (quiet은 그대로) | iOS 기본 ON |
| YouTube | ≈ −14 LUFS | −1 dBTP (여러 단계 트랜스코드 대비 −1.5 권장) | 아래로만 | |
| Tidal | −14 LUFS | −1 dBTP | 아래로만 | |
| Amazon Music | −14 LUFS | −1 dBTP | 아래로만 | |
| AES TD1008 (streaming 권장) | **−16 LUFS** (popular music, track) / album 내 최대 −14 | −1 dBTP | — | 2021 AES Technical Document |
| EBU R128 (broadcast) | **−23 LUFS** (±0.5) | −1 dBTP | — | European broadcast standard |
| ATSC A/85 RP (US broadcast) | **−24 LKFS** (±2) | −2 dBTP | — | 미국 CALM Act 준거 |
| Netflix (stream delivery spec) | −27 LKFS Dialog-gated (Dialnorm) | −2 dBTP | — | Dolby dialog-anchored |

> LUFS와 LKFS는 같은 양의 다른 명칭이다(LKFS: ITU, LUFS: EBU). 플랫폼 정책은 빈번히 업데이트되므로 최종 체크는 각사 developer/engineering 문서를 참조. 본 표는 2024–2025 공개 자료 기준.

### 8.7 본 플러그인 적용

- **Makeup gain 검증:** 플러그인 bypass와 enable 상태의 Integrated LUFS가 ±0.5 LU 이내면 auto-gain 매치가 제대로 작동
- **Loudness war 방지:** drive가 LUFS에만 기여하고 음악성에 기여하지 않는 구간을 Integrated / Short-term 비교로 탐지
- **LRA 보존:** 정적 컴프레서·리미터와 달리, 본 플러그인은 LRA를 과도하게 축소시키지 않음을 짧은 샘플(예: 30s) 처리 전후 LRA 비교로 증명
- **A/B 비교 필수 지표:** 경쟁사 플러그인 대비 동일 Integrated LUFS에서의 청감적 임팩트 비교 (ABX Section 12와 결합)

---

## 9. 시변성 측정 — 차별점 증명의 핵심

### 9.1 열적 드리프트 측정

**절차:**
1. 플러그인을 "cold" 상태 (새로 인스턴스화)에서 1kHz 사인파 재생 시작
2. 1분간 연속 녹음
3. 각 5초 구간의 THD+N을 개별 측정
4. 시간에 따른 THD+N 그래프 작성

**예상 결과 (본 플러그인):**
```
THD (dBc)
-55 │                    .....____
-60 │             .....--
-65 │         ....
-70 │ ........
-75 │
    └──────────────────────────
     0   10   20   30   60 초
```

초기(cold): 낮은 왜곡, 시간에 따라 왜곡 증가 (열적 드리프트로 바이어스 이동)

**기존 플러그인 (Waves, Slate 등):**
```
THD (dBc)
-65 │ ─────────────────────────
    └──────────────────────────
     0   10   20   30   60 초
```
완전 평탄 → 정적 처리 증명

### 9.2 Cathode Bounce 측정

**절차:**
1. 큰 amplitude 버스트 (1초, −3 dBFS)를 인가
2. 버스트 직후 −30 dBFS의 1kHz 사인파로 전환
3. 수 100ms 동안 THD+N 추적

**예상 결과:**
```
THD (%)
  0.8 │*
  0.6 │ *
  0.4 │  * *
  0.2 │     * * *
  0.1 │           * * * ____
    0 ├──────────────────────
      0   50  100  200  500ms
```

버스트 직후 일시적인 THD 증가, 이후 수십 ms에 걸쳐 회복 → 캐소드 바운스 시그니처

### 9.3 PSU Sag 측정

**절차:**
1. 정현파 스윕을 다양한 레벨에서 재생하면서
2. 이득(gain) 변화를 실시간 측정
3. 레벨에 따른 동적 이득 그래프

---

## 10. 인스턴스 변이 통계 측정

### 10.1 100 seed 샘플링

```python
# 100개 다른 seed에서 플러그인 렌더
results = []
for seed in range(100):
    plugin.set_seed(seed)
    output = plugin.process(test_signal)
    harmonics = measure_harmonics(output, fs, 1000)
    results.append(harmonics)

# 통계 분석
import pandas as pd
df = pd.DataFrame(results)
print(df.describe())  # 평균, 표준편차, min, max

# H2 분포 히스토그램
import matplotlib.pyplot as plt
plt.hist(df['H2'], bins=20)
plt.xlabel('H2 level (dBc)')
plt.ylabel('Count')
plt.title('Instance-to-Instance H2 Variation (100 seeds)')
```

**기대 결과:**
- H2 표준편차: ±2–4 dBc (개체차 반영)
- 기존 플러그인: H2 표준편차 = 0 (완전 동일)

### 10.2 두 인스턴스의 스펙트럼 비교

```python
# 두 인스턴스의 하모닉 스펙트럼 오버레이
plt.figure(figsize=(12, 5))
for seed in [0, 1, 2, 3, 4]:
    plugin.set_seed(seed)
    out = plugin.process(test_signal)
    spectrum = compute_spectrum(out, fs)
    plt.semilogx(freqs, spectrum, alpha=0.6, label=f'Instance {seed}')
plt.xlabel('Frequency (Hz)')
plt.ylabel('Level (dBFS)')
plt.legend()
```

---

## 11. ABX 청취 테스트

### 11.1 설계 원칙

ABX 테스트는 청취자가 A(레퍼런스)와 B(비교 대상)를 들은 후 X(무작위로 A 또는 B)가 어느 쪽인지 판별하는 방법이다.

**통계적 요건:**
- 최소 20 트라이얼
- 통계적 유의성: p < 0.05
- 정확도 기준: 75% 이상 = 청취 가능한 차이 존재

### 11.2 테스트 시나리오

| 테스트 | A | B | 기대 결과 |
|--------|---|---|----------|
| 시변성 | 처음 1초 | 1분 후 1초 | 75%+ 구별 가능 |
| 개체차 | seed 0 | seed 1 | 60%+ 구별 가능 |
| vs 경쟁사 | 본 플러그인 | Slate VMR | 80%+ 구별 가능 |
| 실제 vs 에뮬 | 실제 Neve 1073 | 본 플러그인 | 50%에 가까울수록 성공 |

### 11.3 테스트 조건 표준화

- 청취 레벨: 79 dBSPL (AES 기준 믹싱 레벨)
- 모니터: 평탄 응답 레퍼런스 스피커
- 룸: RT60 < 0.3s
- 청취자: 전문 믹싱 엔지니어 최소 5명

---

## 12. 측정 레퍼런스 시스템

### 12.1 소프트웨어 도구 스택

| 도구 | 용도 | 비용 |
|------|------|------|
| REW (Room EQ Wizard) | THD+N, IR 측정 | 무료 |
| ARTA | 정밀 왜곡 분석 | €99 |
| Reaper | DAW 기반 자동화 측정 | $60 |
| Python (scipy, numpy) | 데이터 처리 및 시각화 | 무료 |
| pandas + matplotlib | 통계 분석 및 그래프 | 무료 |

### 12.2 참고 측정 방법론

**Julian Krause (YouTube)** — 플러그인 정밀 측정의 현재 표준:
- 오실로스코프 + DAW 조합 측정
- THD, IMD, 시변성 측정 프로토콜 확립
- Null test 기반 플러그인 비교

**Audio Precision APx555:**
- 업계 최고 정밀도 ($20,000+)
- THD+N 잔류 노이즈 < −120 dBFS
- 최종 검증 단계에서 사용 권장

---

## 13. 검증 체크리스트

개발 완료 후 최소 다음 측정을 통과해야 한다:

### 기능적 검증

- [ ] 모든 drive 설정에서 Nyquist 주파수 이상 앨리어싱 < −80 dBc
- [ ] 동일 seed, 동일 설정의 두 렌더가 null > −90 dBFS (재현성)
- [ ] 다른 seed의 두 인스턴스 H2 차이 > 2 dBc (개체차 존재)

### 시변성 검증

- [ ] "Cold" 시작 후 30초 이상 구간에서 THD 변화 > 1 dBc (열적 드리프트 확인)
- [ ] 강한 버스트 직후 수십 ms 이내 THD 변화 > 2 dBc (캐소드 바운스 확인)

### 물리적 일치성

- [ ] H2/H3 비율이 12AX7 실측값과 ±3 dBc 내 (타겟 모드에서)
- [ ] 저주파 THD가 중역 THD보다 높음 (트랜스포머 특성)
- [ ] CCIF IMD가 SMPTE IMD보다 낮음 (진공관 특성)
- [ ] 고주파(8kHz↑)에서 group delay 증가 관찰 (트랜스포머/커플링 캡 특성)
- [ ] H2 상대 위상이 drive 증가와 함께 수 도 이상 이동 (level-dependent 위상)
- [ ] Farina ESS로 추출한 H2/H3 IR이 주파수 의존성을 보임 (중역에서 최대)

### 라우드니스 / 피크 검증

- [ ] Bypass와 enable의 Integrated LUFS 차이 < 0.5 LU (auto-gain 매치)
- [ ] 0 dBFS 샘플 피크 신호에서 true peak 측정이 ≥ 0.0 dBTP를 정상 포착
- [ ] 마스터링 프리셋에서 출력 True Peak ≤ −1.0 dBTP 보장 모드 동작
- [ ] 30s 샘플 처리 전후 LRA 차이 < 1.5 LU (동적 범위 보존)

### 성능 검증

- [ ] 44.1kHz, 128샘플 버퍼, Medium 품질에서 CPU < 20% (단일 코어)
- [ ] 레이턴시 < 4ms (128샘플 버퍼 기준)
- [ ] 상태 저장/복원 후 측정값 동일 (재현성)

---

## 참고문헌

1. ITU-R BS.1770-5 (2023-11). *Algorithms to measure audio programme loudness and true-peak audio level.* International Telecommunication Union. https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf
2. EBU R 128 (2020). *Loudness normalisation and permitted maximum level of audio signals.* European Broadcasting Union.
3. ATSC A/85 (2013). *Techniques for Establishing and Maintaining Audio Loudness for Digital Television.* Advanced Television Systems Committee.
4. AES TD1008.1.21-9 (2021). *Recommendations for Loudness of Internet Audio Streaming and On-Demand Distribution.* Audio Engineering Society. https://www.aes.org/technical/documentDownloads.cfm?docID=731
5. IEC 60268-1. *Sound system equipment — Part 1: General.* International Electrotechnical Commission.
6. IEC 60268-3:2018. *Sound system equipment — Part 3: Amplifiers.* (DIM IMD test per §14.12.3: 3.15 kHz square + 15 kHz sine, 4:1 amplitude)
7. SMPTE RP 120-1994. *Measurement of Intermodulation Distortion in Film Sound Systems.* Society of Motion Picture and Television Engineers. (60 Hz + 7 kHz, 4:1)
8. AES17-2020. *AES standard method for digital audio engineering — Measurement of digital audio equipment.* Audio Engineering Society.
9. Temme, S. (1992). "Application Note: Audio Distortion Measurements." *Brüel & Kjær.*
10. Klippel, W. (2006). "Distortion Analyzer." *AES 120th Convention.*
11. Krause, J. (2022). "Plugin vs Hardware Null Testing." YouTube Channel: *Julian Krause Audio.*
12. Farina, A. (2000). "Simultaneous Measurement of Impulse Response and Distortion With a Swept-Sine Technique." *AES 108th Convention*, Paris, preprint 5093.
13. Farina, A. (2007). "Advancements in Impulse Response Measurements by Sine Sweeps." *AES 122nd Convention*, Vienna. (개선된 역필터 정규화)
14. Zwicker, E., & Fastl, H. (2013). *Psychoacoustics: Facts and Models* (3rd ed.). Springer.
