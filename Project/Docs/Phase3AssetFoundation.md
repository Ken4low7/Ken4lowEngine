# Phase 3 — Asset基盤

## 完了対象

- [x] AssetId
- [x] AssetHandle
- [x] AssetRegistry
- [x] Model / Texture統合
- [x] Unload / GC
- [x] GPU Deferred Delete
- [x] Async Loading

## AssetId / AssetHandle

`AssetId` は0を無効値とするRuntime IDで、同一プロセス中は再利用しません。`AssetHandle` はPayloadやManagerを所有せず、`AssetId + AssetType`だけを保持します。

## AssetRegistry

Texture / Modelを共通Registryへ登録し、Load状態、参照数、最終使用Frame、GC対象可否、失敗理由を管理します。既存のManager CacheはPayload所有を継続し、Phase 3で描画側を一斉変更しない構成です。

## Model / Texture統合

`AssetSystem` を共通入口として同期Load、非同期Load、Resolve、Acquire / Releaseを提供します。旧`ModelManager` / `TextureManager` APIは互換経路として維持します。

## Unload / GC

GCは`AssetSystem`で明示的に`garbageCollectible`へ設定したAssetだけを対象にします。参照数0かつ未使用期間を超えたAssetをUnloadし、legacyコードがRegistry外で使うAssetを勝手に破棄しません。Modelはlegacy `shared_ptr`が残っている場合Unloadを延期します。

## GPU Deferred Delete

Texture Resource / SRVおよびModelの最終所有権は`GpuDeferredReleaseQueue`へ移し、現在のFence値より後のSignalが完了するまで保持します。これによりGPU参照中のDescriptor再利用を避けます。終了時はGPU完了を待ってQueueをFlushします。

## Async Loading

ModelはAssimp ImportをWorker Threadで行い、Mesh / Texture等のD3D12 Resource生成をMain Threadへ戻します。TextureはDirectXTex DecodeをWorker Threadで行い、GPU Resource / SRV生成をMain Threadへ戻します。D3D12 CommandListを複数Threadから直接操作しません。

## Phase 3の境界

Phase 3はAssetのIdentity / Lifetime / Async基盤までを対象とします。Copy Queue専用Upload、Streaming Mip、Bindless Descriptor、Asset dependency graphの完全自動化は後続Phaseへ残します。
