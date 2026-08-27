# Production Engine 開発進捗

## 開発計画

[添付企画書のリポジトリ内コピー](ProductionEngineRoadmap.md)を開発計画とする。コピー元の内容は変更しない。Phase番号は企画書とこの進捗管理にのみ使用し、クラス/関数/UI/コメント/GPU Debug Nameへ追加しない。

## Phase 30 — Engine Stabilization

### 30.1 Engine / Gameplay / Editor依存関係整理

- 基準master: `d472d306`（2026-08-27、originをfetchして一致を確認）。
- ブランチ: `codex/engine-dependency-audit`。
- 調査: [構造・問題一覧](EngineArchitectureAudit.md)、[Manager / Singleton一覧](EngineServiceInventory.md)。
- 今回の構造調査・最小修正・下記検証を完了。Core/Runtime/Editorの全面分離は未完了で、残課題を調査書に記録した。
- 修正: GameApplicationをApplication所有へ移し、既存module validatorの短いinclude名・相対includeによる逆依存の検出漏れを閉じた。
- ビルドから判明した追加修正: BasicParticleActorのImGui includeとUI本体をUSE_IMGUIで囲み、Releaseコンパイルを復旧した。Actorの生成/Particle再生処理は維持。
- 維持: Actor / Component、既存処理順、GPU所有/同期、Water/VFX/Physics、既存Manager数。
- Legacy削除: なし（旧GameApplication配置の解消はクラス削除ではない）。互換転送ヘッダーや新Manager/Singletonは追加していない。

### コミット

| コミット | 内容 |
| --- | --- |
| `3e15e71` | 開発計画、構造調査、94 Singleton / 11通常Managerの一覧をコード修正前に記録 |
| `4676616` | GameApplicationをApplicationLayerへ移動。WinMain/project/filter参照を更新 |
| `9867942` | BasicParticleActorのEditor UIをReleaseから除外 |
| `43ed0d2` | 逆依存検査と回帰テストを追加 |

この記録の最終更新は別のdocsコミットにまとめる。リモートへのpushは行っていない。

### 後続の扱い

30.2以降は未着手。調査で得た棚卸し・実行順・GPU所有表はその判断材料とするが、Phase 30全体の完成とは扱わない。特にGPU Leakなし、Scene反復、Debug/Release安定動作、既存Water/VFX/Physicsの維持は実機検証が必要。

## ビルド・テスト記録

検証日: 2026-08-27。Visual Studio 18 Community / MSBuild 18.9.1、x64、Python 3.12.13。生成ログは `Generated/EngineAudit` に保存し、Gitには含めない。

| 検証 | 結果 |
| --- | --- |
| 変更前DebugソリューションBuild（リンクを含む） | 成功、208警告 / 0エラー |
| GameApplication移動後Debug Build | 成功、0警告 / 0エラー |
| 最初のRelease Build | 失敗、BasicParticleActorのImGui条件分岐漏れによる4エラーを検出 |
| 修正後ReleaseソリューションBuild（リンクを含む） | 成功、32警告 / 0エラー。C4189/C4100などの残存警告は今回変更しない |
| 最終DebugソリューションBuild（リンクを含む） | 成功、0警告 / 0エラー |
| ValidateEngineModules.py | 成功。Engine→Application候補0件、各sourceの所有が一意 |
| ValidateProjectAssets.py / CheckBrokenReferences.py | ともに成功 |
| unittest discover / CIと同じテストファイル直接実行 | 7テスト成功。うち逆依存は3 Module × 8 include表記のsubtestを含む |
| 既存PowerShellスクリプトの構文解析 | BuildAssetCommon / BuildTextures / BuildMeshes / BuildFonts / PackageRelease / RunSoakTestの6本成功 |
| 起動・正常終了smoke: Debug | 3.00647秒 / 171フレーム、終了コード0、timeoutなし |
| 起動・正常終了smoke: Release | 3.00617秒 / 175フレーム、終了コード0、timeoutなし |
| 変更したソースの工程名検査 | 開発工程番号なし。既存の工程名コメントも変更対象箇所では意図を表すコメントへ変更 |

警告数は各ビルドの出力値であり、最終2回はincremental build。変更前のフルコンパイルと警告数が異なることを「全警告を除去した」とは扱わない。Releaseは最初の全体ビルドで他の翻訳単位もコンパイルされ、修正後に再コンパイル・リンクを完了した。

### 再実行コマンド

リポジトリルートを作業ディレクトリとし、MSBuild/PythonをPATHへ通した環境で実行する。

```powershell
msbuild Project/Ken4lowEngine.sln /m:2 /t:Build /p:Platform=x64 /p:Configuration=Debug
msbuild Project/Ken4lowEngine.sln /m:2 /t:Build /p:Platform=x64 /p:Configuration=Release
python Project/Tools/Scripts/ValidateEngineModules.py
python Project/Tools/Scripts/ValidateProjectAssets.py
python Project/Tools/Scripts/CheckBrokenReferences.py
python -m unittest discover -s Project/Tests -p "test_*.py" -v
python Project/Tests/test_engine_modules.py -v
```

今回のsmokeは `Generated/EngineAudit/RuntimeDebug` / `RuntimeRelease` を作業場所にし、Resources/Config/ExternalsはProjectから参照、Editor layoutはコピーを使った。既存の `KEN4LOW_SOAK_SECONDS=3` と `KEN4LOW_RELIABILITY_CSV` による通常終了を使い、テスト後に環境変数を復元した。CSVと `runtime-smoke-results.json` が結果。`RunSoakTest.ps1` のbudget判定や長時間stressを実行したという意味ではない。

### 残課題・未実施

- DEP-02: FrameworkのCore分類、Runtime→Editor循環、VFX Graph Editor等の所有分類。
- UPD-01〜03: Scene間のPhysics更新差、GPU更新とPIE停止/stepの統一、SampleSceneのEditor二重更新。
- LIFE-01〜03: GPU最終参照と解放、Singleton終了/再初期化、例外時の部分初期化cleanup。
- 開始Scene overrideの扱い、残存ビルド警告、回帰テストの拡充。
- GPU debug layerのメッセージ採取/ライブオブジェクト検査、Scene切替反復、Editor操作、Frames in Flight ON、例外注入、Water/VFX/Physicsの目視比較、長時間stressは未実施。

短時間smokeの成功だけでGPU Leakなし・長時間安定・全機能維持とは判定しない。
