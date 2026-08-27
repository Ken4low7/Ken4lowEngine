# Production Engine 開発進捗

## 開発計画

[添付企画書のリポジトリ内コピー](ProductionEngineRoadmap.md)を開発計画とする。コピー元の内容は変更しない。Phase番号は企画書とこの進捗管理にのみ使用し、クラス/関数/UI/コメント/GPU Debug Nameへ追加しない。

## Phase 30 — Engine Stabilization

### 30.1 Engine / Gameplay / Editor依存関係整理

- 基準master: `d472d306`（2026-08-27、originをfetchして一致を確認）。
- ブランチ: `codex/engine-dependency-audit`。
- 調査: [構造・問題一覧](EngineArchitectureAudit.md)、[Manager / Singleton一覧](EngineServiceInventory.md)。
- 調査完了。最小修正と検証は作業中。
- 修正予定: GameApplicationをApplication所有へ移し、既存module validatorの短いinclude名・相対includeによる逆依存の検出漏れを閉じる。
- 維持: Actor / Component、既存処理順、GPU所有/同期、Water/VFX/Physics、既存Manager数。
- Legacy削除予定: なし（旧GameApplication配置の解消はクラス削除ではない）。

### 後続の扱い

30.2以降は未着手。調査で得た棚卸し・実行順・GPU所有表はその判断材料とするが、Phase 30全体の完成とは扱わない。特にGPU Leakなし、Scene反復、Debug/Release安定動作、既存Water/VFX/Physicsの維持は実機検証が必要。

## ビルド・テスト記録

作業後に、成功・失敗・未実施を分けて追記する。生成ログは `Generated/EngineAudit` に保存し、Gitには含めない。
