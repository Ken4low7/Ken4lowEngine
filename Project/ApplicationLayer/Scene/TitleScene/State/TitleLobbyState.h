#pragma once

#include "ITitleSceneState.h"

/// ロビーの選択ボタンと待機カメラを更新する状態。
class TitleLobbyState : public ITitleSceneState
{
public:
	~TitleLobbyState() override = default;
	void Enter(TitleScene* scene) override;
	void Update(TitleScene* scene, float deltaTime) override;
	void Exit(TitleScene* scene) override;
};
