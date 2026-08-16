# Phase 21 — Particle Modules / Curves / Gradients

Phase20のNiagara-like VFX Graph Foundationを維持したまま、寿命正規化時間を使うCurve/Gradient Authoringと基本Particle Moduleを追加する。

## 実装チェックリスト

- [x] 21.1 Float Curve / Color GradientのAuthoringデータ型を追加
- [x] 21.2 Linear / Step / SmoothStep補間を追加
- [x] 21.3 Curve / Gradientキー数・時刻・有限値のCompiler検証を追加
- [x] 21.4 InitialRotation Moduleを追加
- [x] 21.5 RotationRate Moduleを追加
- [x] 21.6 SizeOverLife Curve Moduleを追加
- [x] 21.7 ColorOverLife Gradient Moduleを追加
- [x] 21.8 JSON Serializerの保存・読込を追加
- [x] 21.9 既存GPU Particleの4点LUTへCompiler Bakeを追加
- [x] 21.10 Sample Graph / Static Test / CIを追加

## 設計方針

### AuthoringとGPU Runtimeを分離する

Graph側では最大32キーのCurve/Gradientを保持する。GPU側の既存契約は変更せず、Compilerが正規化寿命 `0..1` の4サンプルへBakeする。

これによりPhase21では編集自由度を上げながら、Phase13以降で使っているGPU Particle backendを二重化しない。

### 補間

`VfxCurveInterpolation` は次の3種類を持つ。

- `Linear`: 線形補間
- `Step`: 次キーまで前キー値を保持
- `SmoothStep`: 区間内をSmoothStepで補間

Curve/Gradientのキー時刻は `0..1`、strict ascending、最大32キーとする。範囲外入力は先頭または末尾キーへクランプされる。

### Particle Modules

Phase21で追加するGraph Moduleは次の4つ。

- `InitialRotation` — Initialize Stage
- `RotationRate` — Update Stage
- `SizeOverLife` — Update Stage / Float Curve
- `ColorOverLife` — Update Stage / Color Gradient

既存の `Gravity` / `Drag` も引き続きUpdate Moduleとして利用する。

### GPU Bake

`SizeOverLife` は以下の4点を `GpuParticleEmitterDesc::sizeCurveLut` へBakeする。

- `t = 0`
- `t = 1/3`
- `t = 2/3`
- `t = 1`

`ColorOverLife` も同じ4時刻で `colorGradientLut` へBakeする。既存Compute Shaderの `SampleScalarLut` / `SampleColorGradient` をそのまま使用する。

## Sample

`Resources/VfxGraph/Phase21/CurveGradientBurst.vfxgraph.json`

Burst + Sphere Spawnに、Rotation、Size Curve、Color Gradientを組み合わせたPhase21確認用Graph。

## Phase22への境界

Phase21ではParticle同士・SceneとのCollision、Event生成、Sub Emitter生成は実装しない。

Phase22では現在のStage/Module/Curve基盤を利用して次を追加する。

- Collision Module
- Particle Event Queue
- Death / Collision Event
- Sub Emitter Trigger
