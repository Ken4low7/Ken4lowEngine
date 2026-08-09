# Ken4lowLevel Assets

Scene定義の `Level` には、このディレクトリ配下のKen4lowLevel JSONを指定します。

```json
{
  "Id": "Stage01",
  "Class": "GamePlayScene",
  "Level": "Resources/JSON/Levels/Stage01.json"
}
```

Level読込先のSceneは `BaseScene::GetSceneActorWorld()` から対象 `ActorWorld` を公開してください。
`StartLoad()` をoverrideするSceneでは、先頭で `BaseScene::StartLoad()` を呼び出します。
