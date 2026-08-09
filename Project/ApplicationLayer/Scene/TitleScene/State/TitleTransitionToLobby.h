#pragma once

#include "ITitleSceneState.h"

/// タイトルのアトラクト表示からロビーへ移動するカメラ遷移状態。
class TitleTransitionToLobby : public ITitleSceneState
{
public:
	~TitleTransitionToLobby() override = default;
	void Enter(TitleScene* scene) override;
	void Update(TitleScene* scene, float deltaTime) override;
	void Exit(TitleScene* scene) override;
};
