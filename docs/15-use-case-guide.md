# 15. 사용 시나리오별 가이드 (Use Case Guide)

> 문서 00–14는 본 플러그인이 왜 그렇게 만들어졌는가(physics, DSP, architecture)에 대한 문서이다.
> 이 문서는 **믹싱 엔지니어가 실제 세션에서 이 플러그인을 어디에, 어떻게, 왜 거는가**를 다룬다.
> 이론은 최소화하고, "채널 스트립을 열었을 때 뭘 해야 하는가"에 초점을 맞춘다.

---

## 15.1 서론

### 15.1.1 플러그인의 일반 사용 철학

본 플러그인(이하 "TubeAmp")은 세 가지 방식으로 쓸 수 있다.

| 사용 방식 | 무엇을 하나 | 언제 적절한가 |
|-----------|-------------|----------------|
| **Insert (채널)** | 해당 트랙 신호를 직접 통과 | 개별 악기의 색깔 부여, transient 가공 |
| **Bus (그룹)** | 여러 트랙이 합쳐진 서브믹스에 삽입 | 글루(glue), 앙상블감, 통일된 캐릭터 |
| **Parallel (병렬)** | 원본과 블렌드해서 돌아옴 | 드라이브를 강하게 걸되 원음 보존 |

**핵심 원칙 하나를 먼저 말하자면**: TubeAmp는 "저게 원래 저 소리였다"고 속이는 플러그인이 아니라, "이 소리에 뭔가 더해졌다"고 들리는 플러그인이다. 즉 **색칠용(coloring)이지 교정용(corrective)이 아니다**.

### 15.1.2 모든 시나리오의 공통 원칙

1. **입력 레벨을 먼저 맞춘다.** −18 dBFS RMS 내외가 "스위트 스폿"이다. 이보다 낮으면 효과가 들리지 않고, 높으면 Drive 노브를 돌리기도 전에 이미 포화된다.
2. **Drive는 나중에 건드린다.** 먼저 Character / Transformer / Age로 질감을 잡고, 마지막에 Drive로 양을 조정한다.
3. **항상 A/B로 들어본다.** 특히 저역. TubeAmp는 고조파를 만드는데, 이것이 다른 저역 처리(컴프레서, EQ)와 상호작용한다.
4. **여러 트랙에 같은 프리셋을 쓰지 말 것.** 개체차(seed)가 플러그인의 가장 큰 장점인데, 같은 프리셋을 일괄 적용하면 그 장점이 사라진다.
5. **마스터 버스 외에는 -1 dBTP safety를 끌 수 있다.** 내부 True-Peak 보호는 마스터에서만 주로 필요하다.

```
[일반 적용 순서]
  입력 게인 맞추기  →  Mode 결정 (Vintage/Modern)  →  Character / Transformer / Age
                                                           ↓
                          출력 트림  ←  Drive 조정  ←  Dynamics 세팅
```

---

## 15.2 Drum Processing

드럼은 TubeAmp가 가장 빛나는 장르이다. Transient 가공과 저역 펀치 강화가 동시에 가능하기 때문이다.

### 15.2.1 Drum Individual (킥, 스네어, 톰)

#### 킥 드럼 (Kick)

**목표:** 어택은 유지하되 저역에 무게감과 "방" 느낌을 더한다.

- **Drive:** 0.45–0.55 (중간). 너무 세면 클릭이 뭉개진다.
- **Character:** 0.35–0.45. 3차 고조파를 많이 만들지 않는다 (저역이 답답해진다).
- **Transformer:** 0.65–0.75. **여기가 킥의 승부처.** 트랜스포머 포화가 60–120 Hz 대역에 "몸"을 만든다.
- **Dynamics (Cathode Bounce):** 0.55–0.65. 어택 직후 바이어스가 흔들리면서 "비팅" 느낌이 산다.
- **Age:** 0.25–0.35. 너무 높으면 비팅이 둔해진다.
- **M/S:** Stereo (언링크). 킥은 대부분 모노지만 스테레오 체인에 있다면 언링크가 자연스럽다.
- **Mode:** Vintage.

```
[킥 신호 경로 전형적 예]
 Kick ──▶ Gate ──▶ EQ (HPF 30Hz, cut 300Hz) ──▶ Comp ──▶ TubeAmp ──▶ Bus
                                                         ↑
                                        여기서 저역 펀치 + 고조파 살이 붙음
```

**주의:** Kick In / Kick Out 마이크 두 개가 있다면, Out에는 TubeAmp, In은 그대로 두는 것을 권한다. In까지 색칠하면 어택이 모호해진다.

#### 스네어 (Snare)

**목표:** 스틱 어택을 보존하면서 스네어 샷의 "바디"와 "스네어와이어"의 화려함을 살린다.

- **Drive:** 0.25–0.4 (낮음–중간). 스네어는 이미 transient-rich 하므로 Drive를 올리면 금방 지저분해진다.
- **Character:** 0.5–0.6. 2차·3차 고조파 비율이 중요하다. Character를 높이면 스네어가 "레코드 같은" 느낌을 갖는다.
- **Transformer:** 0.45–0.55.
- **Dynamics:** 0.35–0.45.
- **Age:** 0.55–0.7. **Age 높게**가 스네어의 키포인트. 부품 편차가 스네어마다 다른 꼬리를 만든다.
- **Mode:** Vintage.

**활용 팁:** Top 마이크와 Bottom 마이크가 있다면 TubeAmp를 각기 다른 seed로 하나씩 걸고 약간 다른 파라미터를 쓰면 복합적인 스네어 사운드가 나온다.

#### 톰 (Tom)

**목표:** 공명(resonance)을 살리고 중역대에 "나무 몸통" 같은 느낌을 준다.

- **Drive:** 0.35–0.5.
- **Character:** 0.45–0.55.
- **Transformer:** 0.6–0.75. 트랜스포머의 저역 saturation이 톰의 몸통을 두껍게 만든다.
- **Dynamics:** 0.45.
- **Age:** 0.5. 중간 정도.
- **Mode:** Vintage.

**활용 팁:** 플로어 톰에만 Drive 0.05 정도 더 주면 록 믹스에서 "크고 무거운" 느낌이 살아난다.

### 15.2.2 Drum Bus (OH, Room, 또는 전체 drum bus)

드럼 버스에서 TubeAmp의 쓰임은 **개별 트랙 가공과 철저히 다르다**. 여기서는 포화의 양보다 "모든 드럼이 같은 방에 있다는 느낌"을 만드는 게 목적이다.

#### 전체 Drum Bus에 mild saturation

- **Drive:** 0.2–0.3. 매우 가볍게.
- **Character:** 0.35.
- **Transformer:** 0.45.
- **Dynamics:** 0.5. 여기가 중요. 바이어스 바운스가 드럼 전체의 펌핑 리듬을 살짝 강조한다.
- **Age:** 0.4.
- **M/S:** Stereo (링크). Drum bus는 링크가 자연스럽다. 언링크 하면 스냅이 좌우 다르게 튀어서 이상해진다.

#### Stereo unlink vs link 선택 기준

| 상황 | 선택 |
|------|------|
| 개별 드럼, 좌우 비대칭 panning | Unlink (자연스런 차이 유지) |
| Drum bus, 오버헤드, 룸 마이크 | Link (중심축 안정) |
| 스테레오 와이드닝 원할 때 | Unlink (채널 독립 포화) |

#### Transient 보존을 위한 파라미터 설정

드럼 버스에서 transient를 지키고 싶다면:
- Drive < 0.3
- Dynamics ≤ 0.5 (너무 높으면 어택 후 꺼지는 게 심해짐)
- Oversampling을 4× 이상으로 (aliasing이 transient를 흐린다)

---

## 15.3 Vocal Chain

보컬은 TubeAmp의 "두 번째 홈 그라운드"이다. 특히 중·저역 색감을 더하고 싶을 때 유용하다.

### 15.3.1 Lead Vocal

**목표:** 보컬에 "질감(texture)"을 더한다. 생각 없이 건드리면 보컬이 "갇힌" 느낌이 나므로 조심.

- **Drive:** 0.2–0.3. 리드 보컬에 Drive 0.5 이상 주는 경우는 거의 없다.
- **Character:** 0.5. 중립.
- **Transformer:** 0.35–0.45. 트랜스포머 강하게 걸면 저역에 "뭉침"이 생겨서 보컬이 탁해진다.
- **Dynamics:** 0.25–0.35. 낮게. 보컬의 마이크로 다이나믹이 중요하기 때문.
- **Age:** 0.3–0.5.
- **Mode:** Modern (Vintage도 좋지만 대부분 Modern이 자연스럽다).

#### De-essing 후 삽입 vs 전

**일반 추천: De-esser → TubeAmp 순서**

이유: TubeAmp의 고조파는 상대적으로 고역에도 에너지를 주는데, 디-에싱 전에 걸면 치찰음이 포화돼서 디-에서가 제어하기 더 어려워진다.

```
[일반 보컬 체인]
 Vocal ──▶ HPF ──▶ De-esser ──▶ Comp 1 (leveling) ──▶ EQ ──▶ TubeAmp ──▶ Comp 2 (char) ──▶ Bus
                                                                  ↑
                                                      여기 위치가 "색칠"에 좋음
```

#### 주파수 의존 saturation의 활용

TubeAmp의 Character 파라미터는 고조파 스펙트럼의 형태를 조절한다. 보컬에서:
- 저역(200–400 Hz) "chest" 강조 → Transformer 중간, Character 낮게
- 중역(1–3 kHz) "존재감" 강조 → Character 높게
- 고역(5–10 kHz) "air" → Drive 매우 낮게 + Character 중간 (고역 2차 고조파)

### 15.3.2 Backing Vocal

백킹 보컬은 리드보다 뒤로 밀어야 하고, 여러 트랙이 하나의 "합창"처럼 들려야 한다.

- **Drive:** 0.35–0.5. **리드 보컬보다 세게.** 이것이 자연스럽게 뒤로 밀린다.
- **Character:** 0.5.
- **Transformer:** 0.5.
- **Dynamics:** 0.35.
- **Age:** 0.5.
- **Mode:** Vintage (리드와 구분짓는다).

#### Multiple instance로 각각 다른 seed

백킹 보컬 트랙이 4개 있다면: 각 트랙마다 TubeAmp를 개별 인스턴스로 걸고, **seed를 다르게** 한다. 그러면 각 채널마다 고조파가 미묘하게 달라서 앙상블 글루가 자연스럽다.

```
[백킹 보컬 ensemble 셋업]
 BV1 ──▶ TubeAmp (seed=1234) ──┐
 BV2 ──▶ TubeAmp (seed=5678) ──┼──▶ BV Bus ──▶ TubeAmp (bus mode, light) ──▶ Main Bus
 BV3 ──▶ TubeAmp (seed=9012) ──┤
 BV4 ──▶ TubeAmp (seed=3456) ──┘
```

### 15.3.3 Rap Vocal

- **Drive:** 0.4–0.55. 리드 래퍼에는 상대적으로 강한 Drive OK.
- **Character:** 0.55.
- **Transformer:** 0.4.
- **Dynamics:** **0.2 이하**. 중요. Dynamics를 올리면 cathode bounce가 "숨소리"를 강조하는데, 랩에서는 이게 산만해 보일 수 있다.
- **Age:** 0.55.
- **Mode:** Modern.

**주의:** 랩 보컬에서 Dynamics > 0.5는 "헐떡임" 느낌을 과하게 만든다. 일반적으로 피한다.

---

## 15.4 Bass Processing

베이스는 TubeAmp가 "앰프 느낌"을 가장 확실하게 재현하는 영역이다.

### 15.4.1 Bass DI

**목표:** 드라이한 DI 신호에 "앰프를 거친 듯한" 몸통과 고조파를 더한다.

- **Drive:** 0.4–0.5.
- **Character:** 0.55–0.65. 3차 고조파가 강하면 "펀치".
- **Transformer:** 0.7–0.8. **트랜스포머가 베이스의 펀치를 만든다.** 가장 중요한 파라미터.
- **Dynamics:** 0.45.
- **Age:** 0.5.
- **M/S:** Mono (베이스는 모노가 기본).
- **Mode:** Vintage.

#### 고조파 강조로 작은 스피커에서 존재감 확보

사람의 귀는 기본주파수가 없어도 고조파만으로 피치를 재구성한다(missing fundamental). 베이스에 2차·3차 고조파를 더하면 휴대폰 스피커, 노트북, 차 오디오에서 베이스 라인이 더 또렷하게 들린다.

```
[주파수 영역 관점]
 Pure Bass:      [ 50Hz ] ─ (거의 무음) ─ (거의 무음)
                     ▲
                     작은 스피커에서는 안 들림

 Bass + TubeAmp: [ 50Hz ] [ 100Hz ] [ 150Hz ] [ 200Hz ]
                     ▲        ▲          ▲        ▲
                   fund     2nd         3rd      4th
                           ─────────────────────
                              작은 스피커도 이 대역은 들림
                              → 뇌가 50Hz 있다고 인식
```

### 15.4.2 Bass Amp 레이어링

DI + 마이크 앰프 두 트랙을 합치는 구성이면:

- DI 트랙에만 TubeAmp (앰프 느낌 부여)
- 마이크 트랙에는 TubeAmp 쓰지 않음 (이미 실제 앰프 소리)
- 또는 둘 다 걸고, 둘의 Drive / Transformer를 살짝 다르게 해서 층을 만든다

#### 위상 정렬 주의

TubeAmp의 Character 높은 값 (강한 비선형성)은 신호에 미세한 그룹 지연을 만들 수 있다 (오버샘플링 필터 때문). DI와 앰프 트랙을 레이어링할 때:
- 두 트랙에 **같은** TubeAmp 설정을 걸거나,
- DI에만 걸되 "위상 정렬" (샘플 단위 nudging) 후 최종 믹스 판단한다.

---

## 15.5 기타·스트링 악기

### 15.5.1 Acoustic Guitar

- **Drive:** 0.15–0.25. 어쿠스틱 기타에 강한 Drive는 드물다.
- **Character:** 0.4.
- **Transformer:** 0.3. 저역은 이미 풍부.
- **Dynamics:** 0.3.
- **Age:** 0.4.
- **Mode:** Modern.

**활용 팁:** Strumming이 많은 곡에서는 Drive 0.25 정도 주면 "chord smoothing"이 생겨서 여러 string 간 attack 스프레드가 붙는 느낌이 난다.

### 15.5.2 Electric Guitar (클린)

클린 톤에 TubeAmp를 거는 건 미묘한 일이다. 이미 앰프를 거친 신호니까.

- **Drive:** 0.1–0.2 (아주 낮게).
- **Character:** 0.4.
- **Transformer:** 0.25.
- **Dynamics:** 0.3.
- **Age:** 0.3.
- **Mode:** Vintage.

**이미 디스토션이 걸린 톤에는:** 파라미터 전부 conservative (< 0.3). 디스토션 + TubeAmp + 디스토션은 두 번 튀긴 감자처럼 된다 — 맛은 강하지만 원래 재료가 안 보인다.

---

## 15.6 Keys / Synths

### 디지털 신디의 "차가운" 느낌 완화

현대 VST 신디의 소리는 보통 "너무 깨끗해서" 믹스에 자리를 잡지 못한다. TubeAmp는 이 "완벽함"에 살짝 결함을 넣어 다른 아날로그 악기들과 자연스럽게 섞이게 한다.

- **Drive:** 0.2–0.35. 신디의 종류에 따라 다르다 (패드는 낮게, 리드는 중간).
- **Character:** 0.5.
- **Transformer:** 0.4.
- **Dynamics:** 0.3.
- **Age:** 0.55. **높게.** 부품 편차가 신디에 "오래된 아날로그" 느낌을 준다.
- **Mode:** Vintage.

### Saturation curve를 악기 레벨에 맞춤

신디 패치마다 RMS 레벨이 천차만별이다. 같은 프리셋을 붙여쓰지 말고:
- 조용한 패드: Drive 높게 해도 괜찮음
- 큰 리드: Drive 낮게
- **입력 RMS를 먼저 맞추면** (−18 dBFS 근처) Drive 노브의 의미가 일관된다.

---

## 15.7 Master Bus Processing

마스터 버스에서의 TubeAmp 사용은 "믹싱"과 "마스터링" 사이의 회색지대다. 이 절의 추천은 **믹싱 엔지니어가 마스터 버스에 거는 상황** 기준이다.

### 15.7.1 Light Mix Bus Glue

- **Drive:** 0.1–0.2.
- **Character:** 0.3.
- **Transformer:** 0.3.
- **Dynamics:** 0.15–0.25. 낮게.
- **Age:** 0.3.
- **M/S:** **M/S 모드, Mid만 처리**. Side(스테레오 넓이)는 건드리지 않는다.
- **Mode:** Modern.
- **True Peak safety:** **ON**, -1 dBTP.

```
[Master Bus Glue 셋업]
 Mix ──▶ EQ (subtle) ──▶ Bus Comp (1–2 dB GR) ──▶ TubeAmp (M/S, Mid only) ──▶ Limiter ──▶ Out
                                                          ↑
                                                   여기서 harmonic glue
```

### 15.7.2 Creative Master Color

"이 믹스에 캐릭터를 입히고 싶다"는 창작 의도가 있을 때.

- **Drive:** 0.2–0.3.
- **Character:** 0.45.
- **Transformer:** 0.5. Transformer 부각. "레코드 같은" 느낌을 준다.
- **Dynamics:** 0.3.
- **Age:** 0.55–0.7. Age를 높여서 "오래된 아날로그" 느낌.
- **Mode:** Vintage.

### 15.7.3 Heavy Loudness Push

**경고: 본 플러그인은 limiter가 아니다.** TubeAmp의 내부 limiter는 safety net이지 push 도구가 아니다.

그럼에도 LUFS를 짜내야 할 때:
- **Drive:** 0.35–0.45. 이 이상은 아티팩트.
- **Character:** 0.35.
- **Transformer:** 0.4.
- **Dynamics:** 0.15. 낮게 (압축 느낌이 중첩되지 않도록).
- **Age:** 0.3.
- **True Peak safety:** **반드시 ON**, -1 dBTP.
- 이후 별도 마스터링 리미터 (FabFilter Pro-L2, Oxford Inflator 등)

**유의:** Drive > 0.5 상태의 마스터 버스 처리는 마스터링 엔지니어가 "되돌릴 수 없는" 문제를 만든다. 믹싱 단계에서는 0.3 이하로 억제한다.

---

## 15.8 Submixes (Bus Groups)

### 15.8.1 Drum Bus vs All Drums Through Same Instance

**옵션 A: 각 트랙에 개별 인스턴스 (다른 seed)**

장점:
- 트랙별로 다른 고조파 → 자연스런 "드럼 kit" 앙상블감
- 개별 파라미터 튜닝 가능

단점:
- CPU 비용이 트랙 수 × 품질 모드
- 일관된 "sum" 캐릭터가 안 나올 수 있음

**옵션 B: 하나의 bus 인스턴스**

장점:
- CPU 절약
- 일관된 글루 색
- 마스터링 장비를 한 번 통과하는 느낌

단점:
- 개별 트랙 색칠 불가
- 개체차(seed) 장점을 활용 못 함

**일반적 추천:** A + B 섞어서 쓴다. 개별에는 약하게(Drive < 0.3), bus에 진짜 glue를 건다.

```
[Drum 그룹 처리 하이브리드]
 Kick  ──▶ TubeAmp (seed=A, Drive 0.45, 킥 프리셋) ──┐
 Snare ──▶ TubeAmp (seed=B, Drive 0.3,  스네어)  ──┤
 Toms  ──▶ TubeAmp (seed=C, Drive 0.4,  톰)      ──┼──▶ Drum Bus ──▶ TubeAmp (seed=D, Drive 0.2, bus glue) ──▶ Main
 OHs   ──▶ (TubeAmp 선택사항)                       ──┤
 Room  ──▶ TubeAmp (seed=E, Drive 0.5,  거친)    ──┘
```

### 15.8.2 String / Brass Bus

오케스트라 스트링, 금관 등 앙상블 섹션 버스에 TubeAmp를 거는 것은 "하나의 홀에서 녹음된 느낌"을 만들어준다.

- **Drive:** 0.15–0.25.
- **Character:** 0.4.
- **Transformer:** 0.45.
- **Dynamics:** 0.25.
- **Age:** 0.55.
- **Mode:** Vintage.

**활용 팁:** VST 스트링/금관 라이브러리는 각 articulation이 깨끗하게 샘플링돼 있다. TubeAmp의 약한 포화가 이들을 "같은 공기를 통과한" 느낌으로 묶어준다.

---

## 15.9 시나리오별 추천 파라미터 표

| 시나리오 | Drive | Character | Transformer | Dynamics | Age | M/S | Mode |
|---------|-------|-----------|-------------|----------|-----|-----|------|
| Kick | 0.50 | 0.40 | 0.70 | 0.60 | 0.30 | Stereo | Vintage |
| Snare | 0.30 | 0.55 | 0.50 | 0.40 | 0.65 | Stereo | Vintage |
| Tom | 0.40 | 0.50 | 0.65 | 0.45 | 0.50 | Stereo | Vintage |
| Drum Bus | 0.25 | 0.35 | 0.45 | 0.50 | 0.40 | Stereo (link) | Vintage |
| OH / Room | 0.20 | 0.40 | 0.40 | 0.40 | 0.50 | Stereo | Vintage |
| Lead Vocal | 0.25 | 0.50 | 0.40 | 0.30 | 0.40 | Stereo | Modern |
| Backing Vocal | 0.45 | 0.50 | 0.50 | 0.35 | 0.50 | Stereo | Vintage |
| Rap Vocal | 0.50 | 0.55 | 0.40 | 0.15 | 0.55 | Stereo | Modern |
| Bass DI | 0.45 | 0.60 | 0.75 | 0.45 | 0.50 | Mono | Vintage |
| Bass Amp | 0.20 | 0.50 | 0.50 | 0.35 | 0.45 | Mono | Vintage |
| Acoustic Guitar | 0.20 | 0.40 | 0.30 | 0.30 | 0.40 | Stereo | Modern |
| Electric Guitar (클린) | 0.15 | 0.40 | 0.25 | 0.30 | 0.30 | Stereo | Vintage |
| E-Piano / Rhodes | 0.30 | 0.55 | 0.45 | 0.35 | 0.55 | Stereo | Vintage |
| Synth Pad | 0.15 | 0.45 | 0.35 | 0.25 | 0.60 | Stereo | Vintage |
| Synth Lead | 0.30 | 0.55 | 0.40 | 0.30 | 0.50 | Stereo | Vintage |
| String Bus | 0.20 | 0.40 | 0.45 | 0.25 | 0.55 | Stereo | Vintage |
| Master (Glue) | 0.15 | 0.30 | 0.30 | 0.20 | 0.30 | M/S (Mid) | Modern |
| Master (Color) | 0.25 | 0.45 | 0.50 | 0.30 | 0.60 | M/S (Mid) | Vintage |

> 이 표는 **출발점**이다. 곡의 장르, 트랙의 녹음 상태, 주변 플러그인에 따라 0.1 내외로 조정한다.

---

## 15.10 Stacking Multiple Instances

### 원칙

- **매 인스턴스마다 다른 seed** → 자연스러운 개성 (모든 게 똑같으면 "디지털"해진다)
- **트랙 수 × 품질 모드의 CPU 영향** → 문서 07 참조
- **Layering의 "saturation 누적" 효과** → 세 인스턴스를 각 Drive 0.3 거는 것이 한 인스턴스 Drive 0.9와 다르다

### 두 가지 철학

**A. 여러 개 가볍게**

```
 Vocal ──▶ TubeAmp (D=0.2) ──▶ TubeAmp (D=0.2) ──▶ TubeAmp (D=0.2) ──▶ ...
```

장점: 각 스테이지에서 살짝의 포화 + 새로운 고조파 스펙트럼. 실제 빈티지 믹스 체인과 비슷.

단점: CPU 비용. 위상 shift 누적 가능성.

**B. 하나를 적절히**

```
 Vocal ──▶ TubeAmp (D=0.5) ──▶ ...
```

장점: 예측 가능. CPU 효율.

단점: 고조파 스펙트럼이 단일 장치의 것 → 덜 "다면적".

**실무 가이드:** 중요한 트랙 (리드 보컬, 킥, 마스터 버스) 에는 A, 나머지는 B.

---

## 15.11 Workflow 통합

### 15.11.1 Insert Position

#### Pre-EQ vs Post-EQ

| 위치 | 효과 |
|------|------|
| Pre-EQ | TubeAmp가 만든 고조파를 EQ로 깎을 수 있음 → 제어 쉬움 |
| Post-EQ | EQ로 다듬은 깨끗한 신호에 고조파 부여 → 색이 더 "예쁨" |

**일반적:** 교정 EQ는 pre, 창작 EQ는 post.

```
 Input ──▶ Corrective EQ (HPF, 노치) ──▶ TubeAmp ──▶ Creative EQ (톤 성형) ──▶ Out
```

#### Pre-compressor vs Post-compressor

- **Pre-comp:** TubeAmp가 dynamic을 만들면 컴프가 받아친다 → 안정화.
- **Post-comp:** 컴프로 잡힌 신호에 포화를 입힌다 → 결과가 "정돈된 채 컬러풀".

**일반적:** 드럼에서는 pre-comp, 보컬에서는 post-comp.

### 15.11.2 Gain Staging

#### 입력 레벨 기준 (−18 dBFS RMS)

TubeAmp는 아날로그 레벨 기준(−18 dBFS ≈ 0 VU ≈ +4 dBu)으로 튜닝돼 있다. 입력이:
- **−24 dBFS 이하**: TubeAmp 효과가 약하게 들림 → Drive를 올려야 함
- **−18 dBFS 부근**: Drive 노브가 직관적으로 동작
- **−12 dBFS 이상**: 이미 입력에서 포화 → Drive 노브를 많이 못 씀

#### Drive 노브와 입력 게인의 관계

Drive를 올리는 것과 입력 게인을 올리는 것은 비슷하지만 **같지 않다**:
- 입력 게인 → 비선형 함수에 들어가는 레벨이 증가
- Drive → 비선형 함수 자체의 모양이 바뀜 (curve가 더 공격적)

일반적으로 입력 게인으로 "작동 포인트"를 맞추고, Drive로 "비선형성의 강도"를 조정한다.

### 15.11.3 CPU 관리 (대형 세션)

40트랙 세션에서 TubeAmp를 30개 인스턴스 쓰면 CPU가 부담된다.

**전략:**

1. **품질 모드 차등 적용:**
   - 마스터, 리드 보컬, 킥, 스네어: Quality = High (Oversampling 8×)
   - 나머지: Quality = Medium (4×)
   - 스테레오 패드, 앰비언스: Quality = Low (2×)

2. **Freeze / Bounce 타이밍:**
   - 드럼 셋업 완료 후 → drum bus freeze
   - 보컬 편집 완료 후 → 보컬 트랙 freeze (comp 포함)
   - 믹스 중반 이후 → 미사용 가상악기 bounce
   - **마스터 버스 TubeAmp는 절대 freeze하지 않는다** (마지막까지 수정 가능해야 함)

```
[전형적 CPU 관리 플로우]
 Recording → Freeze 악기   → Rough Mix (CPU 여유) → Editing → ...
                 ↓                                         ↓
             CPU 10–20%                            다시 unfreeze 해서 세부 조정
```

---

## 15.12 믹싱 엔지니어의 A/B 듣기 가이드

### 본 플러그인 vs 상용 (Slate VMR / Soundtoys Decapitator)

이 절은 **"이 플러그인이 뭐가 다른가"를 귀로 확인하는 방법**이다.

#### 무엇을 들어야 하는가

1. **"똑같은 드럼을 두 번 재생했을 때 미세한 차이" (시변성)**

   같은 드럼 루프를 두 번 렌더링한다 (플러그인 freeze → 다시 렌더). Static DSP (Decapitator 등)는 두 렌더가 샘플 단위로 동일하다. TubeAmp는 잡음 플로어와 바이어스 드리프트 때문에 두 렌더가 **미세하게 다르다**. Null 테스트 시 −60 dB 내외의 차이 신호가 남는다.

2. **"여러 채널을 같은 플러그인 통과시켰을 때 다른 색" (개체차)**

   같은 모노 소스를 8번 복제하고 각각에 TubeAmp를 걸면, seed가 다르면 약간씩 다른 고조파 프로파일이 된다. 이것이 "앙상블 글루"의 물리적 근거.

3. **"강한 transient 후 이어지는 신호의 뉘앙스" (캐소드 바운스)**

   킥이 터진 직후 베이스가 연주되면, TubeAmp에서는 킥이 바이어스를 살짝 밀어서 그 순간의 베이스 고조파가 달라진다. 이것이 vintage gear의 "반응적" 느낌.

#### 5분 청취 테스트 프로토콜

1. 8마디 드럼 루프 + 베이스 + 간단한 코드 녹음
2. 마스터 버스에 A/B 비교용으로 TubeAmp ↔ Decapitator를 번갈아 건다
3. 2dB 정도 라우드니스 매치 (소리 큰 쪽이 항상 더 좋게 들림 — loudness bias 제거)
4. 각 10초씩 블라인드로 번갈아 들으며 다음을 노트한다:
   - 드럼 transient가 "살아있는가"
   - 보컬이 "갇혀있는가"
   - 베이스가 "굵은가"
   - 전체 이미지가 "넓은가"
5. 세 번 이상 반복 후 기록을 비교

**주의:** 한 번만 들으면 선호가 편향된다. 여러 차례 청취한 후의 평균 인상을 믿는다.

---

## 15.13 일반적 실수 방지

### Drive 너무 세게: 아티팩트

**증상:** 고역이 "지글지글", 저역이 뭉개짐, transient가 사라짐.

**해결:** Drive < 0.5를 일반 기준으로 삼고, 드물게 예외.

### 모든 트랙에 같은 설정: 개성 없어짐

**증상:** 믹스가 평탄하고 "모든 악기가 같은 공간에 끼어있다" (진정한 ensemble이 아님).

**해결:** 프리셋을 출발점으로만 쓰고 매 트랙마다 조정. Seed도 트랙마다 다르게.

### Master bus에 과한 처리: 마스터링 어려워짐

**증상:** 마스터링 엔지니어가 "이 믹스에 뭘 더 하기가 어렵다"고 한다.

**해결:** 마스터 버스 Drive를 0.25 이하로 유지. 마스터 tubesaturation은 "선택사항"이지 "필수"가 아니다.

### Gain staging 무시: 의도된 포화 못 얻음

**증상:** 파라미터를 다 돌려봐도 들리는 효과가 미미.

**해결:** 입력 RMS를 −18 dBFS 근처로 맞춘다. 입력 트림으로 먼저 조정.

### Oversampling 모드를 기본값으로 두기

**증상:** Drive를 올리면 고역이 "종이처럼" 부서진다 (aliasing).

**해결:** 강한 Drive가 필요한 트랙은 oversampling 8×. 가벼운 용도는 4×로도 충분.

---

## 15.14 프로젝트 레시피 (전체 믹스 예시)

### 가상 프로젝트: 인디 팝 — 8마디 예제

**악기 구성:**
- Drums: Kick, Snare, Hat, Toms (3), OH (L/R), Room
- Bass: DI + Amp
- E. Guitar: Clean rhythm (L/R), Lead (mono)
- Keys: E-Piano (stereo), Pad (stereo)
- Vocals: Lead (mono), 3-part BVs (LCR)

### TubeAmp 배치도

```
┌─────────────────────────────────────────────────────────────────────┐
│                         FINAL MIX BUS                                │
│  ↑                                                                   │
│  TubeAmp (Master Glue, Mid-only, Drive 0.15, Mode=Modern)           │
│  ↑                                                                   │
│  ┌──────┬──────┬──────┬──────┬──────┬──────┐                        │
│  │Drums │Bass  │Guitars│Keys  │LeadV │BVs   │                        │
│  └───↑──┴───↑──┴───↑───┴───↑──┴───↑──┴───↑──┘                        │
│      │      │      │       │      │      │                          │
│      │      │      │       │      │      └─ TubeAmp per BV          │
│      │      │      │       │      │         (seed different each)   │
│      │      │      │       │      │         + TubeAmp on BV bus     │
│      │      │      │       │      │                                 │
│      │      │      │       │      └─ TubeAmp (Drive 0.25)           │
│      │      │      │       │                                         │
│      │      │      │       └─ E.Piano: TubeAmp (Drive 0.3)          │
│      │      │      │          Pad: no TubeAmp                        │
│      │      │      │                                                 │
│      │      │      └─ Clean Rhythm: TubeAmp (Drive 0.15)            │
│      │      │         Lead: TubeAmp (Drive 0.2)                     │
│      │      │                                                        │
│      │      └─ DI: TubeAmp (Drive 0.45)                             │
│      │         Amp: no TubeAmp                                       │
│      │         Bass Bus: TubeAmp (Drive 0.15 glue)                  │
│      │                                                               │
│      └─ Kick: TubeAmp (Drive 0.5)                                   │
│         Snare: TubeAmp (Drive 0.3, Age 0.65)                        │
│         Toms: TubeAmp (Drive 0.4)                                   │
│         OH: TubeAmp (Drive 0.2)                                     │
│         Room: TubeAmp (Drive 0.5, aggressive)                       │
│         Drum Bus: TubeAmp (Drive 0.25 glue, link)                   │
└─────────────────────────────────────────────────────────────────────┘
```

### 각 섹션의 청취 가이드

**드럼:**
- 킥: 저역 펀치가 크고 "방에 있는 느낌"
- 스네어: 바디와 스네어와이어 화려함
- 드럼 버스: 모든 드럼이 "같은 키트에서 녹음된 느낌"

**베이스:**
- DI TubeAmp를 bypass ↔ engage: 작은 스피커에서 베이스 라인이 또렷해지는지
- Bus glue로 베이스 레이어가 한 악기처럼 움직이는지

**기타:**
- Bypass vs engage 차이가 **미묘해야 한다** — 너무 확 바뀌면 Drive가 센 것
- 믹스 안에서 기타가 "공기 속에 자리잡았는가"

**키:**
- E-Piano의 "나무상자" 느낌이 살아나는가
- 패드는 건드리지 않았으므로 vs 다른 악기와의 "재질 대비"가 보존됨

**보컬:**
- 리드: 명료도는 유지한 채 "질감"이 더해짐
- BVs: 각 BV가 개별적으로 풍부하면서도 리드보다 뒤에 있음
- BV bus glue로 합창이 하나의 덩어리처럼 움직임

**마스터:**
- M/S mode, Mid only: 스테레오 이미지가 그대로 유지되면서 중앙 요소만 글루
- True Peak limit −1 dBTP로 안전

### 순서대로 청취할 때

1. Solo 드럼 버스 (TubeAmp bypass ↔ engage) — 글루감 확인
2. Solo 베이스 (DI TubeAmp bypass ↔ engage) — 작은 스피커 감각
3. Solo 보컬 (리드 TubeAmp bypass ↔ engage) — 질감 변화
4. 전체 믹스 (마스터 TubeAmp bypass ↔ engage) — 전반 캐릭터
5. 전체 믹스 (모든 TubeAmp 일괄 bypass ↔ engage) — 플러그인의 누적 효과

> **믹스의 맛**은 단일 인스턴스에서 오지 않고 여러 인스턴스의 "공명"에서 온다. 이것이 빈티지 콘솔 믹스가 "완성도"있게 들리는 물리적 이유이기도 하다.

---

## 부록 A: 긴급 구조 레시피

급히 세션을 마무리해야 하는 상황의 "일단 걸어두면 좋은" 설정.

| 상황 | 즉효 레시피 |
|------|-------------|
| 드럼이 너무 "건조"해서 방 느낌 없음 | Drum Bus에 TubeAmp (Drive 0.25, Transformer 0.5, Mode Vintage) |
| 보컬이 믹스에서 "갇힌" 느낌 | Lead Vocal에 TubeAmp (Drive 0.2, Character 0.55, Mode Modern) |
| 베이스가 스마트폰에서 안 들림 | Bass DI에 TubeAmp (Drive 0.45, Transformer 0.75) |
| 믹스 전체가 "디지털"함 | Master에 TubeAmp (Drive 0.15, M/S Mid, Age 0.5, Mode Vintage) |
| 신디가 "너무 깨끗"함 | Synth에 TubeAmp (Drive 0.3, Age 0.6, Mode Vintage) |

---

## 부록 B: 관련 문서

- **문서 01:** 진공관 물리 — Character 파라미터의 출처
- **문서 02:** 트랜스포머 — Transformer 노브가 실제로 뭘 하는지
- **문서 03:** Time-varying nonlinearity — Dynamics 노브의 이론
- **문서 06:** Stochastic modeling — Seed와 Age 파라미터
- **문서 08:** Competitive analysis — 다른 플러그인과의 차이
- **문서 11:** Target hardware catalog — 각 Mode가 참조한 실제 장비
- **문서 13:** Mastering features — 마스터 버스 사용 시 주의사항

---

*본 가이드는 추천일 뿐이다. 모든 규칙은 "왜 그런가"를 이해한 후에 깨뜨릴 수 있다. 귀가 최종 심판이다.*
