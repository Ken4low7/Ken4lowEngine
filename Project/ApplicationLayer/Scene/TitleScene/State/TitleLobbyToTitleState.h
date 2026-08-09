#pragma once

#include "ITitleSceneState.h"

/// ロビーからタイトルのアトラクト表示へ戻すカメラ遷移状態。
class TitleLobbyToTitleState : public ITitleSceneState
{
public:
	~TitleLobbyToTitleState() override = default;
	void Enter(TitleScene* scene) override;
	void Update(TitleScene* scene, float deltaTime) override;
	void Exit(TitleScene* scene) override;
};
