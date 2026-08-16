# Phase 23 — Ribbon / Trail / Mesh Particles

Phase20〜22で構築したVFX Graphと既存GPU Particle backendを維持したまま、Render StageへRibbon / Trail / Mesh Particleを追加する。

## 実装チェックリスト

- [x] 23.1 `RibbonRenderer` Graph Nodeを追加
- [x] 23.2 `TrailRenderer` Graph Nodeを追加
- [x] 23.3 `MeshRenderer` Graph Nodeを追加
- [x] 23.4 RendererをSprite / Ribbon / Trail / Meshの排他的1択としてCompiler検証
- [x] 23.5 Ribbon / Trailを既存`GpuParticleKind::Ribbon` backendへlowering
- [x] 23.6 Particleへ1サンプルのprevious-position historyを追加
- [x] 23.7 previous→currentの実移動区間からRibbon / Trail quadを生成
- [x] 23.8 Mesh Graph Nodeを既存Assimp / Mesh Particle pipelineへ接続
- [x] 23.9 Graph / Effect SerializerをRibbon / Trail / Mesh対応
- [x] 23.10 Sample Graph / Regression Test / CIを追加

## 基本方針

Phase23でもGPU Particle backendを二重化しない。

```text
VFX Graph Render Node
        ↓
GpuParticleEmitterDesc
        ↓
GpuParticleEffectCompiler
        ↓
GpuParticleEffectRuntime
        ↓
既存GpuParticleEmitter / Renderer
```

Sprite、Ribbon、Trail、Meshは同じParticle pool、free-list、Update Compute、GPU-driven compactionを共有する。

## Render Type

`GpuParticleRenderType`は既存値を維持したまま末尾へ追加した。

```text
Sprite = 0
Mesh   = 1
Ribbon = 2
Trail  = 3
```

これにより旧Effect / VFX Graphの既存数値契約を壊さない。

## Ribbon Renderer

Authoring値:

- `texturePath`
- `blendMode`
- `width`
- `length`

CompilerはRibbonを次へ変換する。

```text
renderType = Ribbon
startSize.x / endSize.x = width
startSize.y / endSize.y = length
```

Runtimeでは既存の

```text
GpuParticleKind::Ribbon
BillboardMode::Ribbon
```

へ接続する。

## Trail Renderer

TrailはRibbonと同じGPU描画経路を共有するが、Graph上では演出意図を分離したRenderer Nodeとして扱う。

```text
TrailRenderer
    ↓
GpuParticleRenderType::Trail
    ↓
GpuParticleKind::Ribbon
    ↓
BillboardMode::Ribbon
```

将来RendererごとのEditor presetや専用パラメータを増やしても、Particle backendを分裂させないためである。

## Previous-position history

Phase23ではParticleに1点だけ履歴を追加した。

```text
previousTranslate
current translate
```

Update開始時に

```text
previousTranslate = translate
```

を保存し、その後に速度・力・Collisionによって現在位置を更新する。

Vertex Shaderでは

```text
segment = current - previous
```

を求め、segment方向をRibbonのtangentとして使う。

Ribbon quadの中心は

```text
(previous + current) / 2
```

とし、Y方向長さは実移動距離へAuthoring `length` を乗算して決める。

これにより単に「速度方向へ長いSprite」を置くのではなく、実際にそのフレームでParticleが移動した区間を描画する。

### 現在のhistory契約

Phase23のTrail historyは**1 previous sample**である。

そのため現在のRendererは1 Particleあたり1本のprevious→current segmentを描く。複数フレーム分を保持するN-point polyline / spline ribbonは今回のスコープには含めない。

## GPU Layout

Phase22時点:

```text
GpuEmitterCBData = 624 bytes
ParticleCS       = 528 bytes
```

Phase23では`previousTranslate + padding`の16 bytesだけParticleへ追加した。

```text
GpuEmitterCBData = 624 bytes  // unchanged
ParticleCS       = 544 bytes
```

C++ `ParticleCS`とHLSL `Particle`を同じ順序で更新し、`static_assert`と回帰テストでstride driftを防止する。

Emitter Constant Bufferを増やしていないため、Phase22のCollision / Event / Sub Emitter設定はそのまま維持される。

## Mesh Renderer

MeshはPhase13ですでに存在する実Mesh Particle backendをVFX Graphから正式利用する。

Authoring値:

- `meshPath`
- `subMeshIndex`
- `blendMode`
- `startScale`
- `endScale`
- `startRotation`
- `angularVelocity`

Runtime経路:

```text
MeshRenderer
    ↓
GpuParticleRenderType::Mesh
    ↓
ResolveMeshId
    ↓
LoadMeshAssetsFromAssimp
    ↓
"Mesh:<id>"
    ↓
GpuParticleMeshPipeline
```

新しいMesh renderer backendは作らない。

## Renderer exclusivity

1 Emitterにつき有効Rendererは必ず1つだけにする。

許可されるRender Node:

- SpriteRenderer
- RibbonRenderer
- TrailRenderer
- MeshRenderer

0個または2個以上ならVFX Graph Compiler errorにする。

これにより同じParticleが同時にSpriteとMeshとして解釈されるような曖昧なGraphを防ぐ。

## Serialization

VFX Graph SerializerはPhase23 payloadを往復保存する。

### Ribbon / Trail

```json
{
  "texturePath": "Effects/white.dds",
  "blendMode": "Additive",
  "width": 0.05,
  "length": 1.25
}
```

### Mesh

```json
{
  "meshPath": "Sample/cube.gltf",
  "subMeshIndex": 0,
  "blendMode": "Alpha",
  "startScale": [0.18, 0.18, 0.18],
  "endScale": [0.08, 0.08, 0.08],
  "startRotation": [0.0, 0.0, 0.0],
  "angularVelocity": [2.0, 3.0, 1.5]
}
```

既存`.effect.json` Serializerも`Ribbon` / `Trail` render typeをround-tripできる。

## Sample

`Resources/VfxGraph/Phase23/RibbonTrailMeshShowcase.vfxgraph.json`

3 Emitterを収録する。

### EnergyRibbon

```text
Burst
 ↓
SpawnPoint
 ↓
Lifetime
 ↓
InitialVelocity
 ↓
InitialColor
 ↓
ColorOverLife
 ↓
RibbonRenderer
```

### TracerTrail

```text
Burst
 ↓
SpawnPoint
 ↓
Lifetime
 ↓
InitialVelocity
 ↓
InitialColor
 ↓
TrailRenderer
```

### MeshDebris

```text
Burst
 ↓
SpawnSphere
 ↓
Lifetime
 ↓
InitialVelocity
 ↓
Gravity
 ↓
MeshRenderer
```

## Phase22との接続

Phase22のCollision / Death Event / Sub Emitterは同じParticle backend上に残る。

したがって今後、Collisionで生成されたParticleをRibbon系演出へ発展させたり、Death Event後のMesh破片演出へ拡張できる設計を維持する。

Phase23ではPhase22のEvent readback方式を変更しない。

## Phase24への境界

次のPhase24は **GPU Execution Graph / Optimization** を扱う。

Phase23では実装しない項目:

- GPU Execution Graph
- Dispatch dependency graph
- Module fusion / pass fusion
- transient resource scheduling
- async-compute scheduling
- GPU work reduction / dispatch optimization

Phase23はRenderer表現と最小history contractまでで完結させ、実行順・Dispatch最適化はPhase24へ分離する。
