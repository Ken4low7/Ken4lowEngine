#pragma once
#include "ITitleSceneState.h"

/// -------------------------------------------------------------
///				　	タイトルシーンのロード状態
/// -------------------------------------------------------------
class TitleLoadState : public ITitleSceneState
{
public: /// ---------- 仮想関数のオーバーライド ---------- ///

	/// <summary>
	/// デストラクタ
	/// </summary>
	virtual ~TitleLoadState() override = default;

	/// <summary>
	/// ステートに入った瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Enter(TitleScene* scene) override;

	/// <summary>
	/// ステート中の更新処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Update(TitleScene* scene, float deltaTime) override;

	/// <summary>
	/// ステートから抜ける瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Exit(TitleScene* scene) override;

private: /// ---------- メンバ変数 ---------- ///

	float timer_ = 0.0f; // ロードタイマー
	float duration_ = 0.5f; // ロード演出にかける時間
};

