# 30. Neural Training Playbook (Open-Amp + Wright 2020)

## 목적
Tier 3의 Neural Foundation Layer 학습 경로를 고정한다.
- 물리 경로(`y_physics`)는 유지
- neural은 residual(`y_target - y_physics`)만 학습
- 런타임은 `Neural Blend`로 물리/신경 가중 혼합

## 데이터셋
- Open-Amp (입출력 캡처 데이터)
- Wright 2020 계열 공개 캡처 세트(라이선스 확인 후 사용)

필수 컬럼/배열:
- `x`: 입력 파형
- `y_physics`: 동일 입력을 Valvra 물리 경로로 처리한 출력
- `y_target`: 실측/캡처 타깃 출력

## 학습 스크립트
- `scripts/train_neural_residual.py`

예시:
```bash
python3 scripts/train_neural_residual.py \
  --data datasets/openamp_train.npz \
  --out artifacts/neural/bootstrap_weights.json \
  --epochs 30 \
  --lr 0.001
```

## 모델 구조
- Dense 5→8→1 (Tanh)
- feature = `[x, y_physics, dx, dphys, sat]`
- sat = `tanh(0.75*y_physics + 0.25*x)`

## 런타임 통합
- 플러그인에서 RTNeural JSON 로드(`Load NN`)
- 모델 없는 경우 bootstrap residual fallback
- hot-swap 시 neural branch만 10 ms crossfade

## 검증 기준
1. null test로 baseline 대비 residual 유의성 확인
2. 연속성(팝/클릭 없음) 확인
3. CPU 증가량 확인(목표: 단일 인스턴스 추가 +3% 수준)
