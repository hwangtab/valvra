# Valvra 사용자 매뉴얼 (KO)

## 1. 개요
Valvra는 진공관/트랜스포머 기반 색채를 믹스/마스터에 적용하는 플러그인입니다.
핵심 모드:
- V72 Preamp
- Console Output
- Culture Vulture
- RNDI DI
- HiFi 300B SE

## 2. 빠른 시작
1. 트랙 또는 버스에 Valvra 삽입
2. `Mode` 선택
3. `Drive`, `Output`, `Mix` 조정
4. 필요 시 `Quality`(OS 배수) 상향
5. 마스터 버스라면 Mastering 섹션에서 `TP Mode`와 `Ceiling` 설정

## 3. 주요 기능

### 3.1 Chain Builder / Stage Editor
- `Stage Count`, `Input/Output Transformer`로 체인 구성
- Stage Editor에서 각 단별 Tube/Topology/Drive/Bias 개별 편집

### 3.2 Signature Views
- B-H Hysteresis: 출력 트랜스포머 히스테리시스 추적
- Harmonics: H2~H7 모니터링
- Drift Recorder: Sag/Warmup/Thermal drift 60초 기록
- Reroll Timeline: 최근 seed 즉시 복원

### 3.3 A/B Compare
- `A|B` 비교 전환
- `A->B`, `B->A`, `Reset AB`
- `C/D/E` 스냅샷 뱅크(클릭=로드, Shift+클릭=저장)
- Undo/Redo 32단계
- Blind 모드 지원

### 3.4 Mastering
- TP Safety Mode: Off / Soft / Brick-wall
- TP Ceiling: -3.0 ~ -0.1 dBTP
- TP Lookahead: 1 ~ 10 ms
- TPDF Dither: 16/20/24-bit
- Mid/Side 라우팅

### 3.5 Neural (RTNeural)
- `Neural` 노브: 물리 모델 대비 neural residual blend
- `Load NN`: RTNeural JSON 모델 로드
- `Unload NN`: 모델 해제 (순수 물리 경로)
- 모델 경로는 프로젝트 state에 함께 저장/복원

## 4. 권장 워크플로
- 트랙 색채: Drive 0.8~1.5, Mix 20~60%
- 버스/마스터: Drive 0.3~1.0, Mix 100%, TP Brick-wall(+Ceiling -1 dBTP 전후)
- 비교 시: A/B + LUFS matched 상태 유지 후 판단

## 5. 문제 해결
- 소리가 과도하게 거칠면: Drive 감소, Quality 상향
- CPU 부담: Quality 하향(16x→8x/4x), Stage 수 축소
- Neural 로드 실패: JSON 경로/포맷 재확인, `Unload NN` 후 재시도
