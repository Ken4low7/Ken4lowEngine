#pragma once

#include "ITitleSceneState.h"

/// タイトルロゴと周回カメラを表示して入力を待つ状態。
class TitleAttractState : public ITitleSceneState
{
public:
	~TitleAttractState() override = default;
	void Enter(TitleScene* scene) override;
	void Update(TitleScene* scene, float deltaTime) override;
	void Exit(TitleScene* scene) override;
};
