#pragma once

#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"

#include <string>

/// -------------------------------------------------------------
/// DebugSceneでPlayerActorの本番構成を操作・物理検証する一時ホスト。
/// PlayerActor単体で入力元まで接続できた段階で削除できる。
/// -------------------------------------------------------------
class TestActor final : public Ken4lowEngine::PlayerActor
{
public:
	/// PIE複製ではDebugScene固有入力を持つTestActor型を維持する。
	std::string GetClassTypeName() const override
	{
		return "TestActor";
	}

	/// PlayerActorのComponent構成を生成し、新規生成時だけDebugScene用の初期位置へ配置する。
	void Initialize() override;

	/// DebugSceneの実入力をPlayerInputComponentへ要求として渡してからPlayerActorを更新する。
	void Update(float deltaTime) override;

	/// Physics補正後のPlayer位置を確定してから、そのフレーム最後にPlayer Cameraを同期する。
	void PostPhysicsUpdate(float deltaTime) override;

private:
	bool wasControllingPlayer_ = false; // キャプチャ開始直後のカーソルワープをLook入力として扱わないための状態。
};
