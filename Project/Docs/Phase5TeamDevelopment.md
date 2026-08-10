# Phase 5 — チーム制作基盤

## 完了対象

- [x] Engine module分割
- [x] Automated tests
- [x] CI
- [x] Asset validation
- [x] Broken reference checker
- [x] Missing asset fallback
- [x] Project Settings

## 1. Engine module分割

`Build/Modules/EngineModules.json` を追加し、ソース所有境界を次の4 Moduleへ分割しました。

- `Core`: `Engine/Core`, `Engine/Math`, `Engine/Platform`
- `Runtime`: `Engine/Graphics`, `Engine/Physics`, `Engine/Scene`, `Engine/System`, `Engine/Misc`, `Engine/DebugTools`
- `Editor`: `Engine/Editor`
- `Application`: `ApplicationLayer`

`Tools/Scripts/ValidateEngineModules.py` がEngine / ApplicationLayer内のC++ソースを走査し、全ファイルが必ず1つのModuleに所属することをCIで検証します。Engine側からApplicationLayerへの明示的な逆依存もPhase 5以降は禁止します。

### なぜPhase 5でvcxprojを4つへ物理分割しないか

現在の `Ken4lowEngine.vcxproj` にはGraphics / Scene / Editor / Applicationと外部依存が長期間同居しており、`USE_IMGUI` を介したRuntimeとEditorの既存Bridgeも残っています。この状態で数百Translation Unitを一度にStatic Libraryへ移すと、「Module境界の整理」と「リンク構成の変更」が同時に起き、問題の切り分けが難しくなります。

そのためPhase 5では、まず **所有境界をデータ化してCIで固定**します。今後Runtime→EditorのLegacy Bridgeを解消した時点で、このManifestをそのまま複数vcxproj / Static Library化の入力として利用できます。

## 2. Automated tests

`Tests/Phase5/test_project_validation.py` を追加しました。

現在はGPUやWindowを必要としないTeam CI向けのテストとして、以下を自動検証します。

- Resource root path解決
- 未対応Project Settings Versionの拒否
- Level内Prefab referenceの抽出
- Repository上のProject Settingsが有効であること

実行方法:

```text
cd Project
python -m unittest discover -s Tests/Phase5 -p "test_*.py" -v
```

今後Math / Physics / Serializationの純粋C++テストを追加する場合も、GPU初期化を要求しないTest targetから増やします。

## 3. CI

既存 `.github/workflows/DebugReleseBuild.yml` を `TeamDevelopmentCI` に更新しました。

Push / Pull Request時に次の順序で検証します。

```text
Module ownership
  ↓
Project / Asset validation
  ↓
Broken reference check
  ↓
Automated tests
  ↓
Debug C++ compile
  ↓
Release C++ compile
```

Debug / Releaseは `Ken4lowEngine.vcxproj /t:ClCompile` で全C++ Translation Unitを検証します。`WarningLevel=Level4` と `TreatWarningAsError=true` は既存Project設定をそのまま使用します。

Hosted runnerにはRepositoryで利用している `assimp-vc143-mtd.lib` が存在しないため、CIではフルDebugリンク成功を偽装せず、C++コンパイルを明示的な検証対象とします。

## 4. Asset validation

共通処理を `Tools/Scripts/project_validation.py` にまとめ、`ValidateProjectAssets.py` から実行します。

現在の検証対象:

- Project Settings Format / Version
- Resources配下の全JSON構文
- Scene Registry Format
- Scene定義ファイルの存在
- Scene ID重複
- Startup / DebugStartup Scene
- SceneのLevel / BGM reference
- NextScene / RetryScene
- Ken4lowLevel Version
- Level Actor ID重複
- Level Prefab reference

実行方法:

```text
cd Project
python Tools/Scripts/ValidateProjectAssets.py
```

## 5. Broken reference checker

`Tools/Scripts/CheckBrokenReferences.py` を追加しました。

Asset validationのうち「ファイルやScene IDの参照切れ」に特化した軽量チェックです。Scene Registry、SceneDefinition、Level、Prefabの参照を追跡し、参照元ファイルと壊れた値をCIログへ表示します。

実行方法:

```text
cd Project
python Tools/Scripts/CheckBrokenReferences.py
```

存在しないAssetをRuntime fallbackで隠して終わるのではなく、**CIでは参照切れを失敗にし、Runtimeではクラッシュを避ける**という役割分担にしています。

## 6. Missing asset fallback

### Texture

`TextureManager` 初期化時にマゼンタ色の生成Textureを作成します。

Texture decode / file openに失敗した場合はDebug logへ元パスと理由を残し、SRV / Resource / Metadata取得時には生成Fallback Textureを返します。

これによりTexture欠損が即時クラッシュにならず、画面上でもマゼンタ色として発見できます。

### Model

`ModelManager` はModel import失敗時に `nullptr` を返す代わりに、単純な生成三角形Modelを返します。

MaterialにはTexture fallback keyを設定するため、欠損Modelはマゼンタのプレースホルダーとして表示できます。

### Audio

Audio fileが存在しない場合はDecoderへ渡さず、`Silent` fallbackとして再生をスキップします。Debug outputには欠損パスを残します。

この設計により、Team memberのローカル環境で一時的にAssetが不足していてもEditor全体を停止させず、同時にCIではBroken referenceとして検出できます。

## 7. Project Settings

`Resources/JSON/ProjectSettings.json` を追加しました。

現在のFormat:

```json
{
  "Format": "Ken4lowProjectSettings",
  "Version": 1,
  "ProjectName": "Ken4lowEngine",
  "ResourceRoot": "Resources",
  "SceneRegistry": "Resources/JSON/Scenes/SceneRegistry.json",
  "StartupSceneOverride": "",
  "FallbackAssets": {
    "TextureKey": "__Ken4lowMissingTexture",
    "ModelKey": "__Ken4lowMissingModel",
    "AudioMode": "Silent"
  }
}
```

Runtime側は `Engine/Core/Project/ProjectSettings.h` から読み込みます。

`SceneDefinitionRegistry` の標準Registry pathとStartupScene override、Texture / Model fallback keyをProject Settingsから解決できるようにしました。

Project Settings自体が壊れている場合は、既存の標準Scene Registry pathと組み込みFallback keyを使うため、設定ファイル1つの破損でEngine起動経路全体を失わないようにしています。

## チーム制作時の運用

変更をpushする前にローカルで次を実行すると、CIと同じデータ検証を先に行えます。

```text
cd Project
python Tools/Scripts/ValidateEngineModules.py
python Tools/Scripts/ValidateProjectAssets.py
python Tools/Scripts/CheckBrokenReferences.py
python -m unittest discover -s Tests/Phase5 -p "test_*.py" -v
```

## Phase 5以降に残す項目

- Core / Runtime / Editor / Applicationの実Static Library化
- Runtime内の `USE_IMGUI` Editor Bridge解消
- C++ Unit Test executableの独立Project化
- Asset import時のschema validation
- GUID / Asset IDベースのBroken reference追跡
- Missing Shader / Material fallback
- Project SettingsのEditor UI
- Team向けpre-commit hook
- CI cacheによるC++ compile高速化
