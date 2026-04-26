# 07. 물리에서 코드로 — 구현 전략

> **연관 문서:** [01 진공관 물리](./01-vacuum-tube-physics.md) · [02 트랜스포머](./02-transformer-physics-and-distortion.md) · [04 임피던스 상호작용](./04-circuit-interactions-and-impedance.md)

> **이 문서는 앞의 물리 이론을 실시간 오디오 플러그인으로 구현하는 방법을 다룬다. 각 방법의 정확도, CPU 비용, 복잡도를 비교하여 최적의 하이브리드 접근을 제시한다.**

---

## 1. 구현 방법론 비교

| 방법 | 정확도 | CPU 비용 | 개발 복잡도 | 시변 지원 |
|------|--------|---------|-----------|----------|
| Static Waveshaper (tanh) | 낮음 | O(1) | 낮음 | 없음 |
| Polynomial Approximation | 중간 | O(N) | 낮음 | 없음 |
| Koren Direct | 높음 | O(1) | 중간 | 파라미터 변경으로 가능 |
| Wave Digital Filters | 매우 높음 | O(NR) | 높음 | 자연스러움 |
| Modified Nodal Analysis (DK) | 매우 높음 | O(NR²) | 매우 높음 | 자연스러움 |
| Neural (LSTM/TCN) | 측정 의존 | O(layers) | 중간~높음 | 학습 데이터 의존 |
| Grey-Box Hybrid | 높음 | 중간~높음 | 높음 | 구조적으로 가능 |

---

## 2. Wave Digital Filters (WDF)

### 2.1 이론적 배경

WDF는 Alfred Fettweis(1971)가 제안한 디지털 필터 이론으로, 아날로그 회로의 Kirchhoff 전류·전압 방정식을 **파동 변수(wave variables)**로 변환하여 디지털로 구현한다.

**Kirchhoff → Wave 변수 변환:**

포트 전압 $V$와 전류 $I$에서:

$$a = V + R_0 I \quad \text{(incident wave)}$$
$$b = V - R_0 I \quad \text{(reflected wave)}$$

여기서 $R_0$은 포트 임피던스(reference impedance)다.

이 변환의 장점:
- 임피던스 적응(impedance adaptation) 자동 처리
- 비선형 소자를 스칼라 방정식으로 처리 가능
- 단방향(one-path) 연산으로 지연-없는 루프(delay-free loop) 회피

### 2.2 기본 1포트 소자의 WDF

**저항 (Resistance R):**

$$b_R = -a_R \quad (R_0 = R)$$

**커패시터 (Capacitance C) — trapezoidal (bilinear) 이산화 가정:**

$$b_C[n] = a_C[n-1] \quad (R_0 = 1/(2Cf_s))$$

**인덕터 (Inductance L) — trapezoidal (bilinear) 이산화 가정:**

$$b_L[n] = -a_L[n-1] \quad (R_0 = 2Lf_s)$$

**전압원 (Voltage Source $V_s$):**

$$b_s = 2V_s - a_s$$

> **중요:** 위 커패시터/인덕터의 $R_0$ 값은 **연속시간 미분을 trapezoidal rule(= bilinear transform $s \leftrightarrow 2f_s\frac{z-1}{z+1}$)로 이산화했을 때** 얻어지는 포트 임피던스다. 이산화 방법이 바뀌면 $R_0$와 반사식이 모두 달라진다.

#### 이산화 방법별 비교

| 소자 | Trapezoidal (Bilinear) $R_0$ | Backward Euler $R_0$ | 반사 관계 |
|------|------------------------------|----------------------|-----------|
| 커패시터 $C$ | $\dfrac{T}{2C} = \dfrac{1}{2Cf_s}$ | $\dfrac{T}{C} = \dfrac{1}{Cf_s}$ | trapezoidal: $b[n]=a[n-1]$; BE: $b[n]=0$ (적응된 경우) |
| 인덕터 $L$ | $\dfrac{2L}{T} = 2Lf_s$ | $\dfrac{L}{T} = Lf_s$ | trapezoidal: $b[n]=-a[n-1]$; BE: $b[n]=0$ |

> 표기법 주의: 문헌에 따라 sampling period $T = 1/f_s$ 또는 $T_s$를 쓴다. 본 문서는 $f_s$ 표기를 우선하고 등가식으로 $T$ 표기를 병기한다. Fettweis(1986) §II.C, Yeh(2009) thesis Ch.3, 그리고 Smith의 "Physical Audio Signal Processing" online book의 WDF 장이 1차 참조다.

#### Trade-off

- **Trapezoidal / Bilinear:** 2차 정확도, **에너지 보존**(수동 회로의 passivity 유지), 하지만 **주파수 워핑(frequency warping)** — $\omega_d = \frac{2}{T}\tan(\omega_a T/2)$ 관계로 Nyquist에 가까워질수록 주파수가 압축된다. 오디오 범위에서 고주파 레조넌스 주파수가 아래로 밀린다.
- **Backward Euler:** 1차 정확도, **무조건 안정(A-stable 이상, L-stable)** — 강한 비선형/강직(stiff) 시스템에서 폭주하지 않음. 그러나 **수치 감쇠(numerical damping)** 가 들어가 고주파 성분이 원본보다 더 빨리 감쇠한다. 레조넌스의 Q가 실제보다 낮아진다.

**실무 권고:** 선형/약비선형 섹션은 trapezoidal(정확도 + passivity), 강한 비선형(다이오드, 포화 트랜스포머)에서 수렴 문제가 생기면 해당 서브회로만 Backward Euler로 전환하거나 TR-BDF2(trapezoidal + BDF2 혼합)를 사용한다.

### 2.3 Series/Parallel Adaptor

두 포트를 직렬 연결할 때 Series Adaptor:

$$b_1 = a_2 - \frac{R_{01}}{R_{01}+R_{02}}(a_1 + a_2)$$
$$b_2 = a_1 - \frac{R_{02}}{R_{01}+R_{02}}(a_1 + a_2)$$

병렬 연결 시 Parallel Adaptor는 위와 듀얼(dual) 관계다.

### 2.4 비선형 소자: 다이오드 / 진공관

비선형 소자는 독립 포트로 처리한다. 다이오드의 경우:

$$I_D = I_S(e^{V_D/V_T} - 1)$$

이를 파동 변수로 변환하면:

$$b = a - 2R_0 I_D(V_D)$$

여기서 $V_D = (a + b)/2$이므로 Newton-Raphson으로 $b$를 수치 해석한다.

### 2.5 R-type Adaptor (다중 비선형 소자)

여러 비선형 소자가 있는 회로에서는 일반 어댑터가 동작하지 않는다. Müller의 R-type adaptor가 해결책이다:

$$\vec{b} = S \vec{a}$$

여기서 $S$는 회로 topology에서 유도되는 산란 행렬(scattering matrix)이다.

### 2.6 SPQR Tree Decomposition

복잡한 회로를 WDF로 구현하려면 회로 graph를 **SPQR tree**로 분해한다:
- S: 직렬 접속
- P: 병렬 접속
- Q: 단일 소자
- R: R-type adaptor 필요 (비선형 서브그래프)

```
Neve 1073 입력단 SPQR Tree 예:
        [R-type: 비선형 진공관 서브회로]
       /                               \
[P: 트랜스포머 등가회로]          [S: 출력 단]
  /         \                    /           \
[Lm]  [R-type: 히스테리시스]  [진공관 단]  [부하]
```

---

## 3. Modified Nodal Analysis (MNA) — DK Method

### 3.1 원리

Yeh와 Smith(2008, DAFx-08)는 MNA 상태방정식 위에서 **K-method**(Borin et al. 2000의 state-space 비선형 해법)와 wave digital 정식화를 비교하여 가이터 디스토션 회로의 실시간 시뮬레이션 프로토콜을 정립했다. 한국 문헌에서 흔히 쓰이는 "DK(Direct Method with Kirchhoff)"라는 명명은 실제 논문 본문의 용어가 아니라 **MNA + K-method**(또는 후속 Yeh 2010 TASLP의 자동화된 state-space)의 약칭에 해당한다. 원전은 Yeh의 Bassman tone stack 논문(DAFx-06)으로 거슬러 올라간다.

**회로 상태 방정식:**

$$C \dot{x}(t) = A x(t) + B u(t) + D f(x)$$

여기서:
- $x$: 상태 벡터 (커패시터 전압, 인덕터 전류)
- $f(x)$: 비선형 함수 벡터 (진공관 전류 등)
- $A, B, C, D$: 회로 topology에서 유도되는 행렬

**디스크리트화 (Bilinear Transform):**

$$x[n+1] = \left(I - \frac{\Delta t}{2C^{-1}A}\right)^{-1}\left(\left(I + \frac{\Delta t}{2C^{-1}A}\right)x[n] + \cdots\right)$$

### 3.2 Newton-Raphson 비선형 해법

매 샘플마다 비선형 방정식을 푸는 Newton-Raphson 반복:

```cpp
// 매 샘플마다 수렴까지 반복
Vec f_vec = computeResidual(x_guess, state);
while (f_vec.norm() > tolerance) {
    Mat J = computeJacobian(x_guess, state);
    Vec delta = J.ldlt().solve(-f_vec);
    x_guess += delta;
    f_vec = computeResidual(x_guess, state);
}
```

**수렴 전략:**
- 일반적으로 3–5회 반복으로 수렴
- 전 샘플의 해를 초기값으로 사용 (warm start)
- 수렴 실패 시 이전 샘플의 해로 폴백

---

## 4. State-Space Nonlinear Modeling

### 4.1 Port-Hamiltonian Systems

에너지 보존을 보장하는 Port-Hamiltonian 프레임워크:

$$\dot{x} = (J - R)\nabla H(x) + B u$$
$$y = B^T \nabla H(x)$$

여기서:
- $H(x)$: 시스템 에너지 (Hamiltonian)
- $J$: 반대칭 행렬 (에너지 보존)
- $R$: 양정치 행렬 (에너지 소산)
- $B$: 입력/출력 포트

**장점:** 에너지 보존이 구조적으로 보장되어 수치 불안정 없음.

### 4.2 Symplectic Integrators

에너지를 보존하는 수치 적분 기법. 표준 Euler나 RK4보다 에너지 드리프트가 적다:

- **Störmer-Verlet:** 2차 정확도, 에너지 보존
- **Leapfrog:** Verlet과 동일하지만 속도 업데이트를 반 스텝 앞에서

---

## 5. Oversampling

### 5.1 왜 오버샘플링이 필수인가

비선형 처리는 원래 신호보다 고주파 성분(하모닉, IMD)을 생성한다. 이 고주파 성분이 Nyquist 주파수를 초과하면 **앨리어싱(aliasing)**이 발생한다.

예: 1kHz 입력, 10× hard clip
- 생성되는 10th harmonic: 10kHz
- 44.1kHz에서 Nyquist = 22.05kHz → OK
- 그러나 20kHz 입력의 5th harmonic: 100kHz → Nyquist 초과 → 앨리어싱

**해결책:** 오버샘플링 후 비선형 처리, 다운샘플링.

### 5.2 Polyphase Half-Band Filter

오버샘플링/다운샘플링의 핵심은 효율적인 안티앨리어싱 필터다. Half-band filter는 계수의 절반이 0이어서 계산이 효율적이다.

**4× 오버샘플링 구현:**

```cpp
class Oversampler4x {
    std::array<float, 16> delay_up, delay_down;
    static const float halfband_coeffs[8];  // 폴리페이즈 계수

public:
    void upsample(float in, float out[4]) {
        // 폴리페이즈 구조로 4개 샘플 보간
    }

    float downsample(const float in[4]) {
        // 4개 → 1개 데시메이션 + 앨리어싱 제거
    }
};

// 사용 예:
float out[4];
oversampler.upsample(input, out);
for (int i = 0; i < 4; ++i)
    out[i] = nonlinearProcess(out[i]);
float result = oversampler.downsample(out);
```

### 5.3 오버샘플 비율별 권장 사용

| 오버샘플 | 최고 앨리어스-프리 하모닉 | CPU 비용 | 권장 용도 |
|---------|------------------------|---------|---------|
| 2× | 11th harmonic at 1kHz | 2× | 최소 요건 |
| 4× | 22nd harmonic at 1kHz | 4× | 일반 saturation |
| 8× | 44th harmonic | 8× | 강한 drive, 고급 모드 |
| 16× | 88th harmonic | 16× | 마스터링 최고 품질 |

---

## 6. Neural Grey-Box Hybrid

### 6.1 개념

물리 모델의 **구조**를 유지하면서, 정확한 파라미터 값을 **실제 하드웨어 측정 데이터로 학습**한다.

```
물리 모델 구조:
  [Input] → [Pre-filter] → [Tube Model] → [Transformer] → [Post-filter] → [Output]
                             ↑                  ↑
                    학습 가능 파라미터     JA 파라미터
                    (Koren μ, Kg 등)    (Ms, a, α, k, c)
```

**학습 목표:** 실제 하드웨어 측정 데이터와의 잔차(residual)를 최소화

$$\mathcal{L} = \|y_{measured} - y_{model}(\theta)\|_2^2 + \lambda \|\theta - \theta_{prior}\|_2^2$$

### 6.2 LSTM/GRU 메모리 모델

진공관의 메모리 효과(Volterra series)를 순환 신경망으로 모델링한다.

```python
import torch
import torch.nn as nn

class TubeLSTM(nn.Module):
    def __init__(self, hidden_size=32):
        super().__init__()
        self.lstm = nn.LSTM(input_size=1, hidden_size=hidden_size,
                             num_layers=2, batch_first=True)
        self.output = nn.Linear(hidden_size, 1)

    def forward(self, x, hidden=None):
        # x: (batch, time, 1)
        lstm_out, hidden = self.lstm(x, hidden)
        y = self.output(lstm_out)
        return y, hidden
```

**학습 데이터 수집:**
- 실제 하드웨어에 다양한 입력 신호 (사인파, 노이즈, 음악) 인가
- 입출력 쌍을 기록
- PyTorch로 학습

### 6.3 Temporal Convolutional Network (TCN)

LSTM보다 병렬 처리에 유리한 TCN:

```python
class DilatedConvBlock(nn.Module):
    def __init__(self, channels, dilation):
        super().__init__()
        self.conv = nn.Conv1d(channels, channels, 3,
                              padding=dilation, dilation=dilation)
        self.norm = nn.BatchNorm1d(channels)

    def forward(self, x):
        return x + torch.tanh(self.norm(self.conv(x)))

class TCN(nn.Module):
    def __init__(self, num_layers=8, channels=16):
        super().__init__()
        self.input_conv = nn.Conv1d(1, channels, 1)
        self.blocks = nn.Sequential(*[
            DilatedConvBlock(channels, 2**i) for i in range(num_layers)
        ])
        self.output_conv = nn.Conv1d(channels, 1, 1)
```

### 6.4 RTNeural — 실시간 추론 라이브러리

학습된 PyTorch/TensorFlow 모델을 실시간 C++ 플러그인에서 추론한다. Jatin Chowdhury가 유지 보수하는 **헤더-only C++17 라이브러리**로, github.com/jatinchowdhury18/RTNeural에서 배포되며 **BSD-3-Clause** 라이선스로 상용 플러그인에 사용 가능하다. Dense/Conv1D/GRU/LSTM 등 주요 레이어가 컴파일타임(`ModelT`)·런타임(`Model`) 두 경로 모두 제공되어, 실시간 오디오 스레드에서 동적 할당 없이 추론할 수 있다.

```cpp
#include <RTNeural/RTNeural.h>

// 모델 로드
RTNeural::ModelT<float, 1, 1, RTNeural::LSTMLayerT<float, 1, 32>,
                              RTNeural::LSTMLayerT<float, 32, 32>,
                              RTNeural::DenseT<float, 32, 1>> model;
model.load("tube_model.json");

// 실시간 추론
float output = model.forward(input_sample);
```

### 6.5 Neural Amp Modeler (NAM)

Steven Atkinson의 **NAM**(github.com/sdatkinson/neural-amp-modeler 학습 프레임워크, github.com/sdatkinson/NeuralAmpModelerCore 실시간 DSP 코어)은 기타/베이스 앰프의 블랙박스 캡처 표준으로 빠르게 자리잡았다. Core 라이브러리와 플러그인 모두 **MIT 라이선스**로 배포되어 상용·오픈소스 제품 모두에 재사용 가능하다. 본 플러그인이 순수 물리모델 노선을 택하더라도, **NAM 포맷의 모델을 로드하는 대체 프런트엔드**를 제공하면 사용자 자가 캡처와의 호환성을 확보할 수 있다.

---

## 7. 실시간 제약과 최적화

### 7.1 CPU 복잡도 분석

128샘플 버퍼, 44.1kHz, 8× 오버샘플링 시:
- 처리해야 할 샘플 수: 128 × 8 = 1024개
- 이용 가능 시간: 128 / 44100 ≈ 2.9ms
- 샘플당 이용 시간: 2.9ms / 1024 ≈ 2.8μs

따라서 샘플당 처리 시간이 2.8μs 미만이어야 실시간 가능.

### 7.2 SIMD 최적화

AVX2 명령어로 8개 float을 동시 처리:

```cpp
#include <immintrin.h>

void processBlock_AVX2(float* output, const float* input,
                        int numSamples, float drive)
{
    __m256 vDrive = _mm256_set1_ps(drive);
    for (int i = 0; i < numSamples; i += 8) {
        __m256 x = _mm256_loadu_ps(input + i);
        x = _mm256_mul_ps(x, vDrive);
        // tanh AVX 근사 (Padé approximation)
        x = avx_tanh(x);
        _mm256_storeu_ps(output + i, x);
    }
}
```

### 7.3 블록 처리 vs 샘플 처리

| 방식 | 지연(latency) | 상태 추적 | CPU 효율 |
|------|-------------|---------|---------|
| 샘플 단위 | 최소 | 쉬움 | 낮음 (캐시 miss) |
| 블록 처리 | 블록 크기 | 복잡 | 높음 (SIMD 친화적) |
| 청크 처리 (4–16샘플) | 4–16샘플 | 중간 | 중간 |

**권장:** 비선형 처리는 샘플 단위(상태 유지), 선형 처리는 블록 처리.

### 7.4 근사 수학 함수

```cpp
// Fast tanh (Padé approximant, 최대 오차 < 0.001)
inline float fast_tanh(float x) {
    float x2 = x * x;
    float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return a / b;
}

// Fast exp (비선형 처리에서 자주 사용)
inline float fast_exp(float x) {
    union { float f; int i; } v;
    v.i = (int)(12102203.0f * x) + 1065353216;
    return v.f;
}
```

---

## 7.5 수치적 안정성 (Numerical Stability)

물리 기반 DSP(JA 히스테리시스, WDF, MNA)는 **상태 변수가 샘플마다 누적**되고, **Newton-Raphson 같은 비선형 해법을 반복**하기 때문에 부동소수점 수치 이슈가 일반 waveshaper보다 훨씬 심각하게 나타난다. 출시 가능한 플러그인을 만들려면 아래 다섯 가지를 반드시 다뤄야 한다.

### 7.5.1 Denormal 처리

**문제:** IEEE 754 denormal(subnormal) 값은 프로세서가 소프트웨어 경로로 처리하는 경우가 많아 **50–100배 느려진다**. 한 채널에서 denormal이 발생하면 해당 버퍼에서 CPU 사용률이 순간적으로 치솟고 드롭아웃이 난다.

**JA/WDF에서 특히 문제인 이유:**
- JA 히스테리시스의 누수 저항이 큰 IIR 필터는 상태 변수($M$, $H_{eff}$, 앵커리지 변수)가 입력이 0이 된 후 수 초에 걸쳐 서서히 감쇠한다. 이 꼬리가 $10^{-38}$ 근처로 들어가면 모든 샘플이 denormal이 된다.
- WDF 포트에서 반사파 $b$가 미소하게 남은 상태로 지연 라인을 타고 돌면 마찬가지로 denormal 상태에 빠진다.
- 캐소드 바이어스 바운스의 커패시터 전압도 동일.

**해결: x86 (SSE)**

```cpp
#include <xmmintrin.h>
#include <pmmintrin.h>

// DAW process 콜백 진입 시
_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
```

**해결: ARM NEON (Apple Silicon, iOS, Android)**

```cpp
// FPCR의 FZ(Flush-to-Zero) 비트 설정. AArch64에서:
uint64_t fpcr;
asm volatile("mrs %0, fpcr" : "=r"(fpcr));
fpcr |= (1ULL << 24);  // FZ bit
asm volatile("msr fpcr, %0" : : "r"(fpcr));
```

**RAII 가드:** process 블록 진입/퇴출마다 플래그를 안전하게 설정·복원한다. 다른 플러그인이 FZ를 기대하지 않을 수도 있으므로 **복원은 필수**다.

```cpp
class DenormalGuard {
public:
    DenormalGuard() {
    #if defined(__SSE2__)
        prevFZ = _MM_GET_FLUSH_ZERO_MODE();
        prevDAZ = _MM_GET_DENORMALS_ZERO_MODE();
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    #elif defined(__aarch64__)
        asm volatile("mrs %0, fpcr" : "=r"(prevFPCR));
        uint64_t f = prevFPCR | (1ULL << 24);
        asm volatile("msr fpcr, %0" : : "r"(f));
    #endif
    }
    ~DenormalGuard() {
    #if defined(__SSE2__)
        _MM_SET_FLUSH_ZERO_MODE(prevFZ);
        _MM_SET_DENORMALS_ZERO_MODE(prevDAZ);
    #elif defined(__aarch64__)
        asm volatile("msr fpcr, %0" : : "r"(prevFPCR));
    #endif
    }
private:
#if defined(__SSE2__)
    unsigned int prevFZ, prevDAZ;
#elif defined(__aarch64__)
    uint64_t prevFPCR;
#endif
};

void TubeAmpPlugin::processBlock(AudioBuffer& buffer) {
    DenormalGuard guard;   // 블록 내부에서만 FZ/DAZ ON
    // ... 실제 DSP 처리 ...
}
```

### 7.5.2 Double vs Float 선택 기준

| 모듈 | 권장 정밀도 | 이유 |
|------|-----------|------|
| JA 수치 적분 ($M$, $H_{eff}$ 누적) | **double (f64)** | 상태 변수가 장시간 누적, 오차가 DC 드리프트로 드러남 |
| MNA/WDF 상태 벡터 $x[n]$ | **double** | 행렬 분해의 조건수(condition number)가 클 수 있음 |
| Newton-Raphson 잔차 계산 | **double** | 수렴 판정에 $10^{-8}$ 이하 허용 오차 사용 |
| Waveshaper (정적 tanh, Koren 직접) | float | 입출력만 있고 상태 없음 |
| 오버샘플링 FIR/IIR 계수 | float | 계수가 $O(1)$, 상태 누적이 짧음 |
| 파라미터 스무딩 | float | 수십 ms 시정수, 오차 누적 미미 |

**SIMD 고려:**
- AVX/AVX2: float 8개 또는 **double 4개** 동시 처리 가능 (256-bit)
- AVX-512: double 8개 (고급 데스크톱 CPU에서만 사용 가능)
- NEON: float 4개 또는 **double 2개** (ARMv8)

JA를 AVX double 4-way로 처리하면 채널 4개(스테레오 2페어) 또는 4-voice를 한 번에 돌릴 수 있다.

### 7.5.3 상태 변수 클리핑 (Saturating Guard)

Newton-Raphson이 발산하면 해가 $\pm\infty$나 NaN으로 튀고, 이후 모든 샘플이 오염된다. **하드 리미트**를 둬서 폭주를 차단한다.

```cpp
constexpr double kStateClipLimit = 1.0e6;  // V, A, Wb 단위

template<class T>
inline bool isSane(T x) {
    return std::isfinite(x) && std::abs(x) < kStateClipLimit;
}

// Newton-Raphson 루프 내
x_guess += delta;
if (!isSane(x_guess)) {
    x_guess = x_prev_sample;   // 이전 샘플 해로 폴백
    nrFailCounter++;
    break;
}
```

폴백이 너무 자주 발생하면(예: 100 샘플 내 10회 이상) 스텝 사이즈를 줄이거나 서브스텝으로 재진입한다.

### 7.5.4 샘플레이트 변경 시 재초기화

DAW가 세션 샘플레이트를 바꾸거나 오프라인 렌더로 전환하면 `prepareToPlay()`가 호출된다. 이때 **모든 내부 상태를 완전히 리셋**해야 한다.

```cpp
void TubeAmpPlugin::prepareToPlay(double sampleRate, int blockSize) {
    fs = sampleRate;

    // 1. WDF/MNA 포트 임피던스 재계산 (R_0 = 1/(2Cfs) 등)
    wdf.updateSampleRate(fs);
    mna.rebuildMatrices(fs);

    // 2. 필터 계수 재계산
    oversampler.designFilters(fs);
    millerHPF.setCutoff(fc, fs);

    // 3. 상태 변수 전부 DC 평형점으로
    ja.M = 0.0;
    ja.Hprev = 0.0;
    cathode.Vk = Vk_bias;      // 바이어스 DC
    plate.Ip_avg = Ip_quiescent;
    for (auto& d : wdf.delays) d = 0.0;
    newton.xPrev = xEquilibrium;

    // 4. 램프 카운터 리셋(7.5.5 참고)
    rampSamples = static_cast<int>(0.020 * fs);  // 20 ms
    rampCounter = rampSamples;
}
```

### 7.5.5 Reset 시 클릭 방지

상태를 갑자기 0으로 리셋하면 $\pm 50\,V$ DC 오프셋이 $0\,V$로 튀어 **강한 "틱" 소리**가 난다. 출력에 10–50 ms 램프를 걸어 점진적으로 수렴시킨다.

```cpp
// processBlock 내부
for (int i = 0; i < numSamples; ++i) {
    float y = processSample(input[i]);

    if (rampCounter > 0) {
        float t = 1.0f - (float)rampCounter / (float)rampSamples;  // 0 → 1
        float g = 0.5f * (1.0f - std::cos(M_PI * t));              // 라이즈드 코사인
        y *= g;
        --rampCounter;
    }
    output[i] = y;
}
```

라이즈드 코사인 윈도우는 선형 램프보다 시작/끝에서 도함수가 0이라 고주파 틱이 거의 없다. 10 ms는 대부분의 경우 충분하며, 50 ms는 보수적 선택.

---

## 7.6 LogSumExp 및 수치 함수 안정화

비선형 회로 방정식(다이오드, Child-Langmuir, Koren 진공관, JA Langevin)은 $\exp$, $\log$, $\tanh$, $\coth$ 같은 초월 함수를 반복적으로 평가한다. 입력 범위가 넓어지면 **오버플로우·언더플로우·"0/0" 불확정 형태**가 발생해 NaN을 생산한다. 다음 기법으로 이를 회피한다.

### 7.6.1 softplus / log(1 + exp(x))

스무스 다이오드·softclip 모델에서 자주 나온다. 순진하게 구현하면 $x > 88$(float)일 때 $e^x$가 $\infty$가 되어 결과가 $\infty$로 튄다. 반대로 $x < -88$이면 $1 + e^x$가 정확히 $1$이 되어 도함수 정보가 사라진다.

**분기 근사:**

$$\text{softplus}(x) = \log(1 + e^x) \approx \begin{cases} x & x > 20 \\ \log(1 + e^x) & -20 \le x \le 20 \\ e^x & x < -20 \end{cases}$$

근사 오차는 양쪽 끝에서 $e^{-20} \approx 2 \times 10^{-9}$ 미만으로, float 정밀도에서 구분되지 않는다.

```cpp
inline float softplus(float x) {
    if (x > 20.0f)  return x;
    if (x < -20.0f) return std::exp(x);
    return std::log1p(std::exp(x));   // log1p는 x≈0에서 더 정확
}
```

**LogSumExp 일반형** $\text{LSE}(x_1,\dots,x_n) = \log\sum_i e^{x_i}$는 최대값을 빼서 안정화한다:

$$\text{LSE}(\vec{x}) = x_{\max} + \log \sum_i e^{x_i - x_{\max}}$$

### 7.6.2 Langevin 함수 $\mathcal{L}(x) = \coth(x) - 1/x$

JA 모델의 무이력 자화 곡선 $M_{an} = M_s\,\mathcal{L}(H_e/a)$에 나온다. $x \to 0$에서 $\coth(x) \to \infty$, $1/x \to \infty$로 둘 다 발산하지만 차이는 유한 — 단순히 계산하면 **치명적 상쇄(catastrophic cancellation)**.

**원점 근방 Taylor 전개:**

$$\mathcal{L}(x) = \frac{x}{3} - \frac{x^3}{45} + \frac{2x^5}{945} - \frac{x^7}{4725} + O(x^9)$$

**큰 $|x|$에서:**

$$\mathcal{L}(x) \approx \text{sgn}(x) - \frac{1}{x} \quad (|x| \gtrsim 10)$$

```cpp
inline double langevin(double x) {
    double ax = std::abs(x);
    if (ax < 1e-4) {
        // Taylor (6차): 상대 오차 < 1e-16
        double x2 = x * x;
        return x * (1.0/3.0 - x2 * (1.0/45.0 - x2 * (2.0/945.0)));
    }
    if (ax > 10.0) {
        return (x > 0 ? 1.0 : -1.0) - 1.0/x;
    }
    return 1.0/std::tanh(x) - 1.0/x;
}
```

도함수 $\mathcal{L}'(x) = 1/x^2 - 1/\sinh^2(x)$도 동일한 방식으로 분기 처리 — JA Jacobian 계산에 필수.

### 7.6.3 Fast-exp / Fast-tanh Padé 근사와 오차 경계

| 함수 | 근사 방법 | 최대 절대 오차 | 영역 | CPU 비용 |
|------|-----------|---------------|------|----------|
| `std::tanh` | libm | — | $\mathbb{R}$ | 기준 (1×) |
| Fast-tanh Padé(7,6) | 위 `fast_tanh` | $< 1 \times 10^{-3}$ | $\|x\| < 4$ | ~0.15× |
| Fast-tanh Padé(9,8) | 9차/8차 유리함수 | $< 3 \times 10^{-5}$ | $\|x\| < 5$ | ~0.20× |
| `std::exp` | libm | — | $\mathbb{R}$ | 기준 |
| Fast-exp (bit hack) | 위 `fast_exp` | 상대 오차 ~3% | $\|x\| < 80$ | ~0.05× |
| Fast-exp (Remez+scale) | range reduction + 5차 다항 | $< 1 \times 10^{-6}$ | $\|x\| < 80$ | ~0.25× |

**권고:**
- **오버샘플링된 경로의 최종 waveshaper**: Fast-tanh Padé(7,6)로 충분. 16-bit 양자화 잡음($\approx 10^{-5}$)보다 작은 오차.
- **JA Newton-Raphson의 Jacobian 내부**: 고정밀 `std::exp` 또는 Remez 근사 사용. 근사 오차가 Jacobian에 들어가면 수렴이 느려지거나 발산한다.
- **비트 해킹 fast_exp**는 "무섭게 빠르지만" 3% 오차 → 아날로그 플러그인 최종 단에서는 가청 왜곡이 될 수 있다. 테스트하고 결정.

```cpp
// Range reduction 기반 fast_exp (정확도 > 1e-6)
inline float fast_exp_accurate(float x) {
    // e^x = 2^(x/ln2), x = k*ln2 + r  (|r| < ln2/2)
    constexpr float INV_LN2 = 1.4426950408889634f;
    constexpr float LN2 = 0.6931471805599453f;
    float k = std::round(x * INV_LN2);
    float r = x - k * LN2;
    // 5차 다항 for e^r, r ∈ [-0.347, 0.347]
    float r2 = r * r;
    float p = 1.0f + r + 0.5f*r2 + (1.0f/6.0f)*r*r2
            + (1.0f/24.0f)*r2*r2 + (1.0f/120.0f)*r*r2*r2;
    // 2^k: exponent bits 조작
    union { float f; int32_t i; } u;
    u.i = (static_cast<int32_t>(k) + 127) << 23;
    return p * u.f;
}
```

이러한 안정화를 모두 적용하면 수십 분 연속 재생에서도 drift, denormal spike, click, NaN 전염이 발생하지 않는 프로덕션급 플러그인을 얻을 수 있다.

---

## 8. 전체 신호 체인 아키텍처

### 8.1 모듈 구조

```
┌─────────────────────────────────────────────────┐
│                 TubeAmp Plugin                   │
│                                                 │
│  [Input Stage]                                  │
│    ├── Gain Staging                             │
│    ├── Input Impedance Emulation                │
│    └── ComponentVariation apply                 │
│                                                 │
│  [4× ~ 8× Oversampling]                        │
│                                                 │
│  [Tube Model] (per-sample)                      │
│    ├── Thermal state update (slow)              │
│    ├── Koren / WDF computation                  │
│    ├── Cathode Bounce (Ck state)                │
│    └── Power Supply Sag                         │
│                                                 │
│  [Transformer Model] (per-sample)               │
│    ├── Jiles-Atherton hysteresis                │
│    ├── Lm LPF, Lleak HPF                       │
│    └── Resonance filter                         │
│                                                 │
│  [Downsampling + Anti-alias]                    │
│                                                 │
│  [Output Stage]                                 │
│    ├── Output Impedance                         │
│    ├── Noise floor (optional)                   │
│    └── Output Level                             │
└─────────────────────────────────────────────────┘
```

### 8.2 파라미터 구조

```
User Parameters → Internal DSP Parameters
─────────────────────────────────────────
Drive (0–1)          → gm scale, input level
Character (0–1)      → Koren μ, Kvb
Transformer (0–1)    → JA Ms, saturation threshold
Dynamics (0–1)       → Ck, Rk (bias bounce depth)
Age (0–1)            → 에이징 파라미터
Instance seed        → ComponentVariation
Output               → makeup gain
```

---

## 참고문헌

1. Fettweis, A. (1986). "Wave digital filters: Theory and practice." *Proceedings of the IEEE*, 74(2), 270–327. DOI: 10.1109/PROC.1986.13458.
2. Yeh, D. T., & Smith, J. O. (2006). "Discretization of the '59 Fender Bassman Tone Stack." *Proc. DAFx-06*, Montréal. https://dafx.de/paper-archive/2006/papers/p_001.pdf
3. Yeh, D. T., Abel, J., & Smith, J. O. (2007). "Simplified, Physically-Informed Models of Distortion and Overdrive Guitar Effects Pedals." *Proc. DAFx-07.*
4. Yeh, D. T., Abel, J., & Smith, J. O. (2008). "Simulating guitar distortion circuits using wave digital and nonlinear state-space formulations." *Proc. DAFx-08*, Espoo. (WDF vs K-method 비교 및 "DK"의 근간)
5. Yeh, D. T. (2009). *Digital Implementation of Musical Distortion Circuits by Analysis and Simulation.* PhD thesis, CCRMA, Stanford. (MNA + K-method 상세)
6. Yeh, D. T. (2010). "Automated Physical Modeling of Nonlinear Audio Circuits For Real-Time Audio Effects — Part I: Theoretical Development." *IEEE Trans. Audio, Speech, Lang. Process.*, 18(4).
7. Werner, K. J., et al. (2015). "The Stable Scattering Transform for Audio Effects." *AES 139th Convention.*
8. Parker, J., Zavalishin, V., & Le Beux, E. (2016). "Reducing the Aliasing of Nonlinear Waveshaping Using Continuous-Time Convolution." *Proc. DAFx-16.*
9. Chowdhury, J. (2021). *RTNeural: Real-Time Neural Network Inference Library.* GitHub: jatinchowdhury18/RTNeural. License: BSD-3-Clause.
10. Atkinson, S. (2022–현재). *Neural Amp Modeler / NeuralAmpModelerCore.* GitHub: sdatkinson/neural-amp-modeler, sdatkinson/NeuralAmpModelerCore. License: MIT.
11. Hawley, S. H., et al. (2019). "SignalTrain: Profiling Audio Compressors with Deep Neural Networks." *arXiv:1905.11928.*
