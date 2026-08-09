#pragma once
#include "Actor.h"

/// -------------------------------------------------------------
/// Staticな床とInstancedModelComponentの動作確認を行うテスト用Actor
/// -------------------------------------------------------------
class TestGroundActor final : public Ken4lowEngine::Actor
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// JSON保存・復元で使用するActorクラス名を取得する。
	/// </summary>
	std::string GetClassTypeName() const override
	{
		return "TestGroundActor"; // TestGroundActorとして保存する。
	}

	/// <summary>
	/// TestGroundActor生成後の初期化処理を行う
	/// </summary>
	void Initialize() override;
};

