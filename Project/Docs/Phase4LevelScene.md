# Phase 4 — Level / Scene基盤

## 完了対象

- [x] LevelDocument
- [x] LevelSerializer
- [x] Transactional Loader
- [x] Version migration
- [x] SceneDefinition整理
- [x] Prefab reference / override

## LevelDocument

`LevelDocument` をEditor / Runtime共通のLevel中間表現として追加しました。

JSONを読み込んだ直後にActorWorldへ直接反映せず、まず `LevelDocument` へ変換します。Actor、親子関係、Editor状態、Lighting、Camera、Environment、TargetSceneをこのDocumentで保持します。

現在のLevel Format Versionは `2` です。

## LevelSerializer

`LevelSerializer` がKen4lowLevel JSONの読み書きを一元管理します。

従来は `EditorLevelService` と `SceneLevelLoader` がそれぞれ独自にJSONを解釈していましたが、Phase 4以降は双方が同じ `LevelSerializer` を通ります。

保存時は一度 `.tmp` へ完全なJSONを書き出してからLevelファイルへ反映します。読込時はJSON構文、Version、Actor定義、Actor ID、ParentId、親子循環をActorWorld変更前に検証します。

## Transactional Loader

Level読込は次の順序で行います。

1. JSONをLevelDocumentへDeserialize
2. Version migration
3. Prefab reference / overrideを解決
4. Actor定義と親子グラフを検証
5. Actorを現在Worldとは別にStaging
6. Staging Actor同士の親子関係を構築
7. 全Staging成功後だけActorWorldをCommit
8. Lighting / Camera / Editor状態 / Prefab参照情報を適用

Actor生成、Prefab解決、親子関係構築のいずれかがCommit前に失敗した場合、現在のActorWorldは破棄しません。

Staging中の `CameraComponent` は `AutoRegisterMainCamera` を一時的に無効化し、まだCommitしていないActorがGlobal MainCameraを変更しないようにしています。Commit後にLevel JSONの設定値を復元します。

既存の `SceneLevelLoader` APIは残し、内部で `TransactionalLevelLoader` へ転送する互換Facadeに変更しています。

## Version migration

Version 1のKen4lowLevelをVersion 2へインメモリ移行できます。

Version 1で使用していたActorの `Data` 埋め込み形式はVersion 2でも互換形式として有効です。そのため既存Level JSONを一斉に手作業で書き換える必要はありません。Version 2として再保存した時点で新しいFormatへ更新されます。

現在のMigration経路:

```text
Version 1
   ↓
Version 2
```

Engineより新しいVersionのLevelは推測で読み込まず、未対応として失敗させます。

## SceneDefinition整理

Scene JSONのフィールド解釈を `SceneDefinitionRegistry` から `SceneDefinitionSerializer` へ分離しました。

`SceneDefinitionRegistry` はRegistryの列挙とScene ID検索を担当し、個々のScene JSONのFormat / Version検証と `SceneDefinition` 生成はSerializerが担当します。

Phase 4以前のScene JSONにはFormat / Versionが無いため、それらはVersion 1として互換読込します。

将来Formatを明示する場合は次の形式を使用できます。

```json
{
  "Format": "Ken4lowSceneDefinition",
  "Version": 1,
  "Id": "DebugScene",
  "Class": "DebugScene"
}
```

## Prefab reference / override

Level Version 2ではActor JSONを丸ごと複製する方法に加えて、Prefab参照を保存できます。

```json
{
  "Id": "Actor_0",
  "ParentId": "",
  "Editor": {
    "Visible": true,
    "Locked": false,
    "Folder": ""
  },
  "Prefab": {
    "Path": "Resources/ActorPrefabs/TestActor.json",
    "Overrides": {
      "Name": "TestActor_Instance"
    }
  }
}
```

読込時はPrefab本体を読み、`Overrides` をJSON Merge Patchとして適用して最終Actor定義を生成します。

PrefabをContent BrowserまたはActor Prefab UIから配置した場合は `PrefabInstanceRegistry` が参照元を追跡します。Level保存時には現在ActorとPrefab原本を比較し、差分をOverrideとして保存します。

Prefab原本が削除されている状態で保存する場合は、Level保存そのものを失敗させず現在Actorの `Data` 直書きへFallbackします。

### Overrideの現在仕様

OverrideはJSON Merge Patch方式です。

- object: 子フィールド単位で再帰的に差分化
- scalar: 値を置換
- array: 配列全体を置換
- `null`: Prefab側フィールドを削除

Actorの `Components` は配列なので、Component内部を変更した場合は現段階では `Components` 配列全体がOverrideになります。Component ID単位の差分パッチは後続拡張対象です。

## 互換性方針

Phase 4では既存APIを一斉削除しません。

- `SceneLevelLoader` は互換Facadeとして維持
- Version 1 LevelはMigrationして読込
- Prefabを使わないActorは従来どおり `Data` 形式で保存可能
- 既存のraw Actor Prefab JSONをそのまま利用可能
- 将来のPrefab wrapperとして `Ken4lowActorPrefab` 形式も解決可能

## 検証

GitHub ActionsのVisual Studio 2026環境で以下を実行し、両方のC++コンパイル成功を確認しています。

```text
Debug   / ClCompile / WarningLevel=Level4 / TreatWarningAsError=true
Release / ClCompile / WarningLevel=Level4 / TreatWarningAsError=true
```

Phase 3時点と同様、CI環境には `assimp-vc143-mtd.lib` が無いため、Solution全体のDebugリンクを含むRebuildは外部依存で停止します。Phase 4ではリンク結果を成功扱いせず、C++コンパイルを検証対象としています。

## Phase 4以降に残す項目

- Component ID単位のPrefab Override
- Nested Prefab
- Prefab Variant
- Prefab dependency graph
- Level streaming / sub-level
- Asset dependency preload
- Scene遷移中の非同期Level staging
- Level migrationの自動バックアップ保存
