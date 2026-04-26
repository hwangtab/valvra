# 16. UI / UX 설계 스펙

> TubeAmp 플러그인의 사용자 인터페이스 및 사용자 경험(UX) 설계 스펙.
> 10개의 주요 파라미터(Drive, Character, Transformer, Dynamics, Age, Instance Seed, Output 등)를
> 엔지니어가 실제 작업 환경에서 어떻게 조작할지, 시각적 피드백을 어떻게 제공할지를 정의한다.

---

## 16.1 설계 철학 (Design Philosophy)

### 핵심 원칙

1. **아날로그 하드웨어의 "물리적 노브(physical knob)" 느낌**
   - 고가의 아웃보드 장비(Neve 1073, Pultec EQP-1A, Fairchild 670)를 만질 때의
     질감과 반응성을 화면에서 재현한다.
   - 노브 회전은 감쇄 곡선(momentum curve)을 가지며, 드래그 중단 시 미세한 관성 감쇠가 있다.
   - 클릭음, 스텝 저항감은 선택적으로 햅틱 피드백(지원 컨트롤러에 한함)으로 제공한다.

2. **디지털만의 강점도 활용**
   - 파라미터 자동화(automation), A/B 비교, 프리셋 스냅샷, 실시간 시각화는
     아날로그가 할 수 없는 영역이며 적극 활용한다.
   - 실시간 하모닉 분석(FFT)은 교육적 가치 + 실용적 피드백을 동시에 제공한다.

3. **3단계 모드 (Easy / Standard / Expert)**
   - 초보자에서 연구자까지 하나의 플러그인으로 커버한다.
   - 모드 간 전환은 부드럽게(애니메이션 300ms fade), 파라미터 상태는 유지된다.

### 설계 기조 요약 표

| 축 | 방향 | 근거 |
|----|------|------|
| 복잡도 | 모드별 분리 | 초보자 혼란 방지 |
| 정보 밀도 | 중간 ~ 높음 | 엔지니어는 정보를 많이 보길 원함 |
| 시각 피드백 | 실시간 | 소리 없이 눈으로도 대략 파악 가능해야 함 |
| 리얼리즘 | 스큐어모픽 + 플랫 하이브리드 | 완전 포토리얼리즘은 피로도 유발 |

---

## 16.2 Easy Mode

### 대상 사용자
- 믹싱/마스터링을 시작한 지 1년 이내
- 시간이 부족해 "한 방에 결과를 내고 싶은" 엔지니어
- 사운드 디자이너, 작곡가

### 파라미터 (3개만)

| 파라미터 | 역할 | 범위 |
|----------|------|------|
| **Character** | 타깃 하드웨어 프리셋 선택 | Neve / Pultec / Fairchild / Custom |
| **Amount** | 통합 drive control | 0–100% |
| **Output** | 자동 makeup gain | −12 dB ~ +12 dB |

### 설계 포인트

- **Amount**는 내부적으로 drive, transformer saturation, dynamics(PSU sag)를
  비례 커브로 연동한다. (엔지니어가 3개 파라미터를 수동으로 매칭할 필요 없음)
- Character 선택 시 어울리는 디폴트 세팅이 자동 로드된다.
- Output은 기본값 "Auto"이며, 입력/출력 RMS를 ±0.5 dB 이내로 맞춘다.

### 레이아웃 특징

- 큰 노브 3개만 화면 중앙에 배치
- 하단에 작은 스펙트럼 애널라이저 (하모닉만 표시)
- "Advanced" 버튼 하나로 Standard 모드 전환

---

## 16.3 Standard Mode (기본, 대부분의 사용자)

### 대상 사용자
- 중급 ~ 상급 믹싱/마스터링 엔지니어
- 사운드의 세부 톤 조정을 원하는 프로듀서

### 주요 파라미터 (7개)

| # | 파라미터 | 범위 | 기본값 | 설명 |
|---|---------|------|-------|------|
| 1 | **Drive** | 0–1 | 0.3 | 진공관 그리드 입력 레벨 |
| 2 | **Character (Tube type)** | 12AX7 / 12AU7 / EL34 / 6L6GC / 300B | 12AX7 | 튜브 모델 선택 |
| 3 | **Transformer** | Neve / Pultec / UTC / Jensen / Off | Neve | 트랜스포머 코어 모델 |
| 4 | **Dynamics** | 0–1 | 0.4 | Cathode bounce + PSU sag 결합 강도 |
| 5 | **Age** | 0–100% | 20% | 부품 노후화 시뮬레이션 정도 |
| 6 | **Instance Variation** | 0–65535 (seed) | auto | 개별 하드웨어 편차 (reroll 버튼 포함) |
| 7 | **Output Level** | −24 ~ +12 dB | 0 dB | 마무리 게인 |

### 부속 토글 (Secondary Controls)

- **Oversample:** 2x / 4x / 8x / 16x
  - 기본 4x. Ultra 품질에서는 8x 권장
- **Quality:** Low / Medium / High / Ultra
  - Koren 모델 복잡도와 Runge-Kutta 차수 결정
- **Stereo mode:** Stereo / Mid / Side / M-S link
  - 마스터링 시 사이드만 처리하거나, M-S 링크로 이미지 보존

### 레이아웃 특징

- 상단 1/3: 실시간 하모닉 디스플레이 + I/O 미터
- 중앙 1/2: 7개 노브 (2행 배치, 중요도 순)
- 하단 1/6: 프리셋 메뉴, A/B 버튼, 부속 토글

---

## 16.4 Expert Mode

### 대상 사용자
- 엔지니어, DSP 연구자, 커스텀 사운드 튜너
- 특정 하드웨어를 "내 귀"로 재현하려는 매니아

### 추가 파라미터 (Standard + 고급)

#### Koren 튜브 모델 (개별 조정)
- **μ (mu):** 전압 이득 (amplification factor)
- **Ex:** 지수 (exponent, 보통 1.4)
- **Kg:** 그리드 계수 (plate resistance 영향)
- **Kp:** 비선형성 계수
- **Kvb:** 바이어스 전압 계수

#### Jiles-Atherton (JA) 트랜스포머 모델
- **Ms:** 포화 자화 (saturation magnetization)
- **a:** 도메인 벽 밀도
- **α:** 도메인 상호작용
- **k:** 핀 고정 계수 (hysteresis loss)
- **c:** 가역/비가역 비율

#### 시변 파라미터
- **Thermal drift speed:** 히터 워밍업 시간 상수 (초 단위)
- **Cathode bypass time constant:** 캐소드 바이패스 RC 시정수
- **PSU internal impedance:** 전원부 내부 임피던스 (Ω)
- **HT voltage:** B+ 전압 (V, 보통 250–400)

### 추가 뷰

- **회로 다이어그램** (schematic view):
  - 현재 파라미터가 회로의 어느 부분에 대응하는지 강조
  - 클릭 시 해당 부분의 전압/전류 실시간 표시
- **Internal signal probes:**
  - Plate voltage, cathode current, grid voltage를 오실로스코프 스타일로 표시

### 레이아웃 특징

- 탭 구조: [Koren] [JA] [Thermal] [PSU] [Circuit]
- 각 탭 내 슬라이더 + 수치 입력 박스
- "Reset to preset" 버튼으로 Standard 모드 값으로 복귀

---

## 16.5 시각화 패널 (Visualization Panels)

### 16.5.1 Input/Output Meters

- **Peak / RMS / LUFS 동시 표시** (3중 바)
- **True Peak (4x oversample)** 측정치 별도 표시
- **Clipping indicator** (0 dBFS 초과 시 빨강 LED 점멸, 3초 홀드)
- **Gain Reduction bar** (다이나믹스 영역에서 신호 압축량 표시)
- **Correlation meter** (스테레오 시 +1 / 0 / −1)

### 16.5.2 Harmonics Display

- 실시간 스펙트럼 분석 (FFT 4096점, Hann 윈도우, 75% overlap)
- **H2, H3, H4, H5 레벨 바** (dBc, dB relative to carrier 단위)
- **Input vs Output 오버레이**
  - 회색 = input
  - 컬러(주황) = output
  - 차이만큼이 "added harmonics"
- 레벨에 따른 하모닉 변화 실시간 관찰
- 주파수 축: 20 Hz ~ 20 kHz, 로그 스케일

```
    dBc
      0 ┤
     -20┤           H2
     -30┤    ▓▓    ▓▓
     -40┤    ▓▓    ▓▓    H3     H4
     -50┤    ▓▓    ▓▓    ▓▓    ▓▓
     -60┤    ▓▓    ▓▓    ▓▓    ▓▓    H5
     -70┤    ▓▓    ▓▓    ▓▓    ▓▓    ▓▓
     -80┤_____________________________________
            DC    2f    3f    4f    5f    (freq)
```

### 16.5.3 Saturation Meter

- 현재 신호가 비선형 영역 어디에 있는지 시각화
- 4구간: **clean / mild / medium / heavy**
- 각 샘플의 비선형성 크기를 GR-style meter로 표시
- "How much are you coloring?"을 직관적으로 전달

### 16.5.4 Hysteresis Loop (B-H 곡선)

- JA 모델의 현재 상태를 실시간 B-H 곡선으로 표시
- 가로축: H (자기장 세기, A/m)
- 세로축: B (자속 밀도, T)
- **교육적 가치:** 트랜스포머 포화가 실제로 어떻게 나타나는지 보여줌
- **실용적 가치:** 루프가 "납작"하면 클린, "풍성하면" 포화가 많음

### 16.5.5 Transfer Function

- 현재 설정의 **정적 전달 함수** 그래프 (Vin → Vout)
- 가로축: 입력 전압 (−1 ~ +1 정규화)
- 세로축: 출력 전압 (−1 ~ +1 정규화)
- 입력 신호가 실제로 그 곡선을 따라 매핑되는 **애니메이션 도트**
- Symmetric/Asymmetric clipping 구분 가능

### 16.5.6 Tube "Glow" Indicator

- 가상 진공관 이미지 (플레이트, 히터 필라멘트)
- 히터 워밍업 상태 시각화 (0→100%까지 2~5초)
- 신호 레벨에 따른 미세한 밝기 변화 (신호 ↑ → glow ↑)
- **재미 + 정보:** 시변 효과를 체감하게 함
- 마스터 볼륨에도 연동해 전체 "살아있는 느낌" 부여

---

## 16.6 파라미터 매핑 & Range

| 파라미터 | Min | Max | 단위 | 기본값 | Curve | 비고 |
|----------|-----|-----|------|--------|-------|------|
| Drive | 0 | 1 | % | 0.3 | Logarithmic | 0→0.5는 완만, 0.5→1은 급격 |
| Character (Tube) | 0 | 4 | enum | 0 (12AX7) | Discrete | 5종 |
| Transformer | 0 | 4 | enum | 1 (Neve) | Discrete | 5종 (Off 포함) |
| Dynamics | 0 | 1 | % | 0.4 | Linear | 시변 효과 결합 강도 |
| Age | 0 | 1 | % | 0.2 | Linear | 노후화 정도 |
| Instance Seed | 0 | 65535 | int | random | Discrete | reroll 버튼 |
| Output | −24 | +12 | dB | 0 | Linear | makeup gain |
| Oversample | 1 | 4 | enum | 1 (4x) | Discrete | 2/4/8/16x |
| Quality | 0 | 3 | enum | 2 (High) | Discrete | Low/Med/High/Ultra |
| Stereo Mode | 0 | 3 | enum | 0 (Stereo) | Discrete | Stereo/M/S/M-S link |

### Curve 상세

- **Logarithmic (Drive):** `drive_internal = drive_ui^2.2`
  - 낮은 구간에서 미세 조정, 높은 구간에서 빠른 변화
- **Linear (Dynamics, Age, Output):** `val = min + (max-min) * ui`
- **Discrete (enum):** 스냅(snap) 동작, 중간값 없음

---

## 16.7 프리셋 설계 철학

### 카테고리

| 카테고리 | 프리셋 예시 | 용도 |
|---------|-------------|------|
| **Mix Bus** | Neve Bus Glue, SSL Sheen, API Punch | 믹스 버스 전체 컬러링 |
| **Vocals** | Lead Vocal Warmth, BGV Air, Rap Aggression | 보컬 트랙 |
| **Drums** | Kick Thump, Snare Crack, OH Shimmer, Room Ambience, Drum Bus | 드럼 파트별 |
| **Mastering** | Master Glue, Master Color, Loudness Push | 마스터링 전용 |
| **Creative** | Vintage Crush, Tape Sim, 70s Gold, Broken Radio | 아트 효과 |

### 프리셋 저장 구조 (XML)

```xml
<Preset name="Neve Bus Glue" category="Mix Bus" version="1.0">
  <Parameters
    drive="0.2"
    character="12AX7"
    transformer="Neve"
    dynamics="0.3"
    age="0.15"
    instance_seed="8472"
    output="0.0"
    oversample="4x"
    quality="High"
    stereo_mode="Stereo"
  />
  <Notes>Subtle saturation for entire mix bus. Best placed post-compressor.
    Provides warmth and low-mid glue without obvious distortion.</Notes>
  <Author>TubeAmp Factory</Author>
  <Tags>mix-bus, subtle, neve, warmth</Tags>
</Preset>
```

### 프리셋 브라우저 UX

- 카테고리 트리 (좌측)
- 프리셋 리스트 (중앙, 썸네일 + 제목)
- 미리보기 (우측, 파라미터 요약 + Notes)
- "Like" 버튼으로 즐겨찾기
- 검색 (제목/태그)
- 최근 사용 히스토리

---

## 16.8 Tooltip & Help System

### 파라미터 툴팁
- 각 파라미터 호버 시 다음이 순차 표시된다:
  1. 이름 + 단위 + 범위 (1초 후)
  2. 한 줄 설명 (2초 후)
  3. "Learn more →" 링크 (문서 섹션으로 이동)

### Physics 토글 (고급 모드)

"Show physics" 토글을 켜면 각 파라미터가 물리적 의미와 함께 표시된다:

- **Drive** → "Vgrid 입력 레벨 (플레이트 전류 구동)"
- **Dynamics** → "PSU 내부 임피던스로 인한 B+ 드롭 시뮬레이션"
- **Age** → "캐패시터 ESR 증가 + 튜브 이미션 감소"

### 초보자 튜토리얼 모드

- 최초 실행 시 5단계 온보딩:
  1. Drive로 warmth 추가하기
  2. Character로 튜브 종류 바꾸기
  3. Transformer 추가로 low-end 보강
  4. Dynamics로 생동감
  5. Output으로 makeup
- 각 단계에서 A/B 버튼으로 직접 비교

---

## 16.9 A/B 비교 UI

### 기본 동작
- **A 슬롯 / B 슬롯** 버튼
- **Copy A→B** / **B→A** 버튼 (한쪽 설정을 다른 쪽에 복사)
- **A/B 토글**: 현재 재생 중인 설정을 전환

### 고급 기능
- **Null test 토글:** A와 B 처리 결과의 차이 신호만 재생 (차이가 0이면 무음)
- **LUFS-matched 자동 모드:** A와 B의 LUFS를 ±0.3 LU 이내로 자동 매칭
  - 라우드니스 차이로 인한 "더 크면 좋게 들리는" 오해 방지
- **Blind test 모드:** 사용자가 모르는 상태에서 A/B 랜덤 제공 → 귀로만 판단

### 히스토리
- 파라미터 변경 히스토리 32단계 저장 (Undo/Redo)
- "Snapshot"으로 현재 설정을 Slot C, D, E 등에 저장 가능

---

## 16.10 자동화 (Automation) Lane 최적화

### 자주 자동화될 파라미터 (우선순위)
1. **Drive** (리드 보컬 chorus에서 집중적으로 saturation 증가 등)
2. **Output** (section별 라우드니스 조정)
3. **Dynamics** (build-up에서 PSU sag 증가로 "가라앉는" 느낌)

### 비자동화 파라미터 (정적 권장)
- **Character (Tube type):** 중간에 바꾸면 소리가 튀므로 정적 유지
- **Transformer:** 동일
- **Instance Seed:** 정적 (변경 시 전체 리렌더링 필요)

### Zipper noise 방지
- 자동화 변경 시 **스무싱 필터** 적용 (10 ms 시정수)
- Drive, Dynamics, Output은 샘플 단위 보간
- 스텝성 파라미터(Character, Transformer)는 변경 순간 **crossfade 50 ms**

### DAW별 호환성
- VST3 parameter IDs는 고정 (DAW 프로젝트 호환성 유지)
- Reaper, Logic, Cubase, Pro Tools, FL Studio, Ableton Live 테스트 완료

---

## 16.11 키보드 & 마우스 숏컷

| 입력 | 동작 |
|------|------|
| Shift + drag | fine adjustment (1/10 속도) |
| Cmd/Ctrl + click | reset to default |
| Double-click | 숫자 입력 모드 |
| Alt + drag | 연결된 그룹 동시 조정 (link mode) |
| Scroll on knob | 1 unit씩 증감 |
| Shift + scroll | 0.1 unit씩 증감 |
| Right-click | 컨텍스트 메뉴 (automation 할당, MIDI learn 등) |
| Space | Bypass 토글 |
| A / B 키 | A/B 슬롯 전환 |
| Cmd/Ctrl + Z | Undo |
| Cmd/Ctrl + Shift + Z | Redo |

---

## 16.12 레이아웃 가이드 (ASCII 목업)

### Easy Mode

```
┌─────────────────────────────────────────────────────────┐
│ TubeAmp                                     [Easy ▼][?] │
├─────────────────────────────────────────────────────────┤
│                                                         │
│           ╭──────╮   ╭──────╮   ╭──────╮               │
│           │Char  │   │Amount│   │Output│               │
│           │Neve ▼│   │  30% │   │ Auto │               │
│           ╰──────╯   ╰──────╯   ╰──────╯               │
│                                                         │
│    ╭─────────────────────────────────────────╮         │
│    │  Harmonics:  H2 ▮▮▮  H3 ▮▮  H4 ▮       │         │
│    ╰─────────────────────────────────────────╯         │
│                                                         │
│  Input: -18 dB       Output: -17 dB        [Advanced]  │
└─────────────────────────────────────────────────────────┘
```

### Standard Mode

```
┌─────────────────────────────────────────────────────────┐
│ TubeAmp                                 [Standard ▼][?] │
├─────────────────────────────────────────────────────────┤
│ ╭──────────╮  ╭──────╮ ╭──────╮ ╭──────╮ ╭──────╮      │
│ │ Harmonics│  │Drive │ │Char │ │Trans │ │Dyn   │      │
│ │ H2 ▮▮▮▮  │  │ 30%  │ │12AX7│ │ Neve │ │ 40%  │      │
│ │ H3 ▮▮    │  ╰──────╯ ╰──────╯ ╰──────╯ ╰──────╯      │
│ │ H4 ▮     │                                          │
│ ╰──────────╯  ╭──────╮ ╭──────╮ ╭──────╮              │
│               │ Age  │ │Instance│ │Output│             │
│ Input: -18dB  │ 20%  │ │ #8472 │ │ 0 dB │             │
│ Output:-17dB  ╰──────╯ │[Reroll]│ ╰──────╯             │
│ TP:    -1 dB          ╰────────╯                       │
│                                                         │
│ [Hysteresis Loop]  [Transfer Function]  [Glow]         │
│                                                         │
│ Presets ▼  A ▮▯ B  [Compare] [Null] [Bypass]           │
└─────────────────────────────────────────────────────────┘
```

### Expert Mode

```
┌─────────────────────────────────────────────────────────┐
│ TubeAmp                                   [Expert ▼][?] │
├─────────────────────────────────────────────────────────┤
│ [Standard] [Koren] [JA] [Thermal] [PSU] [Circuit]       │
├─────────────────────────────────────────────────────────┤
│ Koren Model Parameters (12AX7)                          │
│   μ     : [100.0]  ████████░░                          │
│   Ex    : [1.40 ]  █████░░░░░                          │
│   Kg    : [1060 ]  ██████░░░░                          │
│   Kp    : [600  ]  ███████░░░                          │
│   Kvb   : [300  ]  █████░░░░░                          │
│                                                         │
│ ╭────────────────────────╮  ╭──────────────────────╮   │
│ │ Transfer Function      │  │ Plate Characteristic │   │
│ │        ____            │  │   Ia vs Va curves    │   │
│ │     __/                │  │                      │   │
│ │  __/                   │  │                      │   │
│ ╰────────────────────────╯  ╰──────────────────────╯   │
│                                                         │
│  [Reset to Preset]  [Save Custom]  [Export Model]      │
└─────────────────────────────────────────────────────────┘
```

---

## 16.13 폰트, 색상, 스타일 (Visual Style Guide)

### 색상 팔레트

| 역할 | 색상 | HEX | 비고 |
|------|------|-----|------|
| Background | 딥 다크 블루그레이 | `#1A1D23` | 다크 모드 기본 |
| Surface (panel) | 차콜 | `#24282F` | |
| Accent (glow) | 주황 | `#FF8C1A` | 진공관 히터 연상 |
| Secondary accent | 엠버 | `#FFB84D` | 강조 요소 |
| Success / clean | 청록 | `#4DD0E1` | 좋은 상태 |
| Warning | 노랑 | `#FFD54F` | 주의 (clipping 근접) |
| Error / clipping | 선명 빨강 | `#E53935` | |
| Text primary | 흰색 | `#F5F5F5` | |
| Text secondary | 옅은 회색 | `#9E9E9E` | 라벨 |

### 폰트

- **숫자 (파라미터 값):** JetBrains Mono, 모노스페이스
- **레이블 (Drive, Output 등):** Inter, sans-serif
- **본문 (툴팁, 설명):** Inter, sans-serif
- 최소 폰트 크기: 11px (접근성 고려)

### 스타일 규칙

- **Retina / HiDPI** 완전 지원 (2x, 3x 에셋 제공)
- 노브는 벡터 렌더링 (SVG → GPU 비트맵 캐시)
- 그림자와 글로우는 최소화 (CPU 부담 줄임)
- 애니메이션은 300ms 이내, ease-out 권장

### 다크/라이트 모드

- 기본: 다크
- 라이트 모드: 사용자 선택 (스튜디오 조명이 밝은 경우)

---

## 16.14 접근성 (Accessibility)

### 키보드 조작
- 모든 노브, 버튼, 메뉴 키보드로 조작 가능
- Tab 키로 순차 이동, 화살표로 값 조정
- Focus indicator 명확히 표시 (outline 2px)

### 스크린 리더 지원
- 모든 UI 요소에 `aria-label` 또는 동등한 VST3 accessibility 속성 부여
- 파라미터 변경 시 값을 음성으로 읽어주는 모드 (옵션)

### 색맹 친화
- 주황/빨강 외에도 **모양**과 **위치**로 상태 전달
- Clipping indicator: 빨강 + 깜빡임 + "CLIP" 텍스트
- Deuteranopia/Protanopia 대안 컬러 팔레트 제공 (설정에서 전환)

### 저시력 지원
- UI 스케일 100% / 125% / 150% / 200% 옵션
- 고대비 모드 (Accent 강도 증가)

---

## 16.15 설계 검증 체크리스트

- [ ] 3개 모드(Easy / Standard / Expert) 간 자연스러운 전환 (300ms fade)
- [ ] 모든 자동화 가능 파라미터 DAW에서 제어 가능
- [ ] CPU usage 실시간 표시 (상태바 우측)
- [ ] 프리셋 저장/불러오기 동작 (XML + 썸네일)
- [ ] MIDI learn 지원 (모든 knob)
- [ ] VST3 / AU / AAX / CLAP 모든 포맷에서 동일 동작
- [ ] 4K / 8K 디스플레이에서 선명 표시
- [ ] 모바일 터치(iPad Logic Remote 등)에서 조작 가능
- [ ] 색맹 대안 팔레트 확인
- [ ] 스크린 리더로 모든 파라미터 접근 가능
- [ ] Undo/Redo 32 단계 정상 동작
- [ ] A/B 토글 시 지터(glitch) 없음
- [ ] Null test 정확도 검증 (동일 설정이면 -∞ dB)
- [ ] Automation zipper noise 없음 (10 ms 스무싱)

---

## 16.16 프로토타입 로드맵

| 단계 | 작업 | 기간 | 성과물 |
|------|------|------|-------|
| 1 | Mock-up (Figma) | 2주 | 3개 모드 전체 플로우, 인터랙션 프로토타입 |
| 2 | JUCE 기반 GUI 뼈대 | 4주 | 정적 레이아웃, 파라미터 연결 |
| 3 | 시각화 컴포넌트 (Hysteresis / Transfer / Glow) | 3주 | OpenGL/Metal 커스텀 렌더링 |
| 4 | 테마 / 폴리싱 | 2주 | 색상, 애니메이션, 접근성 |
| 5 | 베타 피드백 반영 | 4주 | 10명 이상 실사용 테스트 |

**총 예상 기간: 약 15주 (약 3.5개월)**

### 베타 테스터 프로파일
- 프로 엔지니어 3명 (Mix / Master)
- 중급 프로듀서 3명
- 초보자 2명
- DSP 전공 연구자 2명

---

## 16.17 향후 확장 고려사항

### 추가 논의 필요 항목
- **튜토리얼 비디오** 임베드 여부 (플러그인 내부 vs 외부 링크)
- **클라우드 프리셋 공유** 기능 (사용자 프리셋 업로드/다운로드)
- **AI 기반 자동 매칭** ("이 보컬 트랙에 맞는 설정 추천")
- **MIDI 컨트롤러 프리셋** (APC40, Maschine 등 매핑)
- **iPad / Surface 터치 최적화 UI** (큰 터치 영역, 제스처)

### 차기 버전 후보
- **Sidechain input** (ducking용 compressor behavior)
- **Multi-band** 모드 (저역/중역/고역 별도 saturation)
- **Convolution hybrid** (IR과 모델링 결합)

---

**문서 버전:** 1.0
**최종 수정일:** 2026-04-17
**작성자:** TubeAmp Design Team
