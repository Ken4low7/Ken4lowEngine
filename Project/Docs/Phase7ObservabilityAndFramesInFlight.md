# Phase 7 — Observability / Frames in Flight Hardening

## 目的

Phase 7では、Frames in Flightを単に有効化するだけでなく、CPU/GPUを並行実行しても前フレームのGPU参照中データをCPUが上書きしないことを保証し、その効果をCPU・GPU両方の計測値で確認する。

## Frames in Flight 基本契約

- FrameResourceはSwapChain BackBufferと同じindexで進める。
- CommandAllocator / FrameUploadArenaは、そのFrameResourceに紐づくFence完了後だけResetする。
- UIからのON/OFF要求は記録中のCommandListへ即時反映せず、フレーム境界で適用する。
- GPUが参照する動的Uploadデータは、PerFrameUploadBufferまたはFrameUploadArenaから供給する。
- 初期化時に一度だけ書き込む静的Upload Bufferは共有してよい。
- READBACK Bufferは対応GPU処理完了後だけMapする。

## Mapped Buffer監査

### Per-Frame化済み

- WorldTransform
- Material
- LightGpuBuffer
- Sprite
- SkyBox / CloudLayer
- ShadowSystem
- ParticleMaterial
- GPU Particleの動的PerView / PerFrame / Emitter
- InstancedObject3DRendererのMain / Shadow / Picking instance stream
- InstancedObject3DRendererのPerView / Camera / Dissolve / Shadow parameter
- Object3DCommonのPoint / Spot Shadow pass constants
- SkinClusterの動的Palette upload source
- WireframeのLine / Triangle / Box / Shape instance / Transform
- PostEffect動的ConstantBuffer
  - Absorb
  - Bloom
  - DepthOutline
  - Dissolve
  - GaussianFilter
  - GrayScale
  - LuminanceOutline
  - Pixelate
  - PlayerHealth
  - RadialBlur
  - Random
  - Smoothing
  - ToneMapping
  - Vignette

PostEffectの既存Mapped BufferはEditor/Runtime設定用のCPU stagingとして残すが、GPUへは直接bindしない。Apply時にFrameUploadArenaへ値を複製して、そのFrame専用GPU Addressをbindする。

### 静的用途として共有可能

- Mesh / AnimationMeshの初期頂点・indexデータ
- WireframeのBox/Sphere/Capsule共有base meshとindex
- SkinClusterのVertexInfluence（初期構築後は更新しない）

静的BufferはGPU実行中にCPUから再書き込みしないため、Frames in Flightでも共有可能。

### READBACK用途

Editor GPU Picking等のREADBACK Mapは、対応CommandをExecuteしてFence完了を待った後に読むため、Upload Buffer競合とは別分類とする。

## GPU Timestamp Profiler

RenderPipelineControllerへD3D12 Timestamp Queryを追加する。

計測対象:

- BeginDraw
- Shadow Prepare
- Shadow Render
- Editor UI Build
- Editor Picking
- Main World Render
- PostEffect
- Selection Outline
- Scene Overlay
- ImGui Render
- BackBuffer PostEffect
- BackBuffer Rebind
- Game UI
- GPU Pipeline Total

Query HeapとREADBACK BufferはFrameResource数分の領域を持つ。現在Frameの結果をその場で待たず、同じFrameResourceが次回再利用される時点でFence完了済み結果を回収する。これによりProfiler自身が追加のGPU Stallを作らない。

## Performance Window追加項目

- Frames in Flight requested / active state
- BackBuffer index
- Command Frame index
- FrameUploadArena Frame index
- Stable Frames
- Frame index mismatch count
- Frame Upload Arena used / capacity / high water / overflow
- GPU Pipeline Total Last / EMA / Max
- GPU Pass Last / EMA / Max

## 基準値

Phase 7 hardening前の代表値:

| Metric | OFF | ON |
|---|---:|---:|
| Frame Interval | 16.07 ms | 15.53 ms |
| Draw | 6.004 ms | 5.856 ms |
| EndDraw / Present Block | 11.913 ms | 10.190 ms |
| Total Frame | 18.173 ms | 16.278 ms |
| SwapChain Present | 0.717 ms | 0.249 ms |
| Fence Wait | 11.003 ms | 9.765 ms |

この値は最終比較用Baselineであり、GPU Timestamp導入後に同一Scene・同一Camera・同一VSync条件で再計測する。

## Stress Test

Frames in FlightをONにして以下を確認する。

1. Editorを30〜60分連続実行する。
2. カメラを高速移動・回転する。
3. Play / Stopを連続で切り替える。
4. Level / Sceneを切り替える。
5. Actor生成・削除を繰り返す。
6. Instancingを大量表示する。
7. Shadow caster / Point / Spot lightを切り替える。
8. Animation / Skinningを継続再生する。
9. Wireframe / Bounds Debugを表示する。
10. PostEffect設定を実行中に変更する。
11. Window resizeを繰り返す。

## 完了条件

- 30〜60分Stress Testでちらつき0回。
- D3D12 Debug Layer Error 0件。
- Frame index mismatch 0件。
- FrameUploadArena overflow 0件を基本目標とする。
- Play / Stop / Level切替後も描画破損なし。
- Animation / Wireframe / PostEffect変更中も描画破損なし。
- OFF / ON双方でGPU Timestampが継続取得できる。
- 同条件比較でCPU Fence Wait、GPU Pipeline Time、Total Frameの差を記録する。

上記を満たした後にExperimental表記を外し、Frames in Flightの既定ONを判断する。
