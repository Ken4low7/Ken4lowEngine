# Phase 1 Cleanup Audit

このドキュメントは、FPS削除後の残骸整理と Engine / Gameplay / Editor の責務分類、Legacy候補の棚卸し、プロファイリング導入の進捗を記録するためのものです。

## Phase 1

- [ ] FPS削除後の残骸を洗う
- [ ] Engine / Gameplay / Editor を分類する
- [ ] Legacyコード一覧を作る
- [ ] Assetメモリ使用量を計測できるようにする
- [ ] 毎フレームAllocationを計測できるようにする

## 方針

- 削除は参照元・ビルド設定・実ファイルの存在を確認してから行う。
- RuntimeとEditorの責務を分離し、Gameplay固有処理をEngine基盤へ混在させない。
- メモリ計測値は、実測か概算かをUI上で明示する。
