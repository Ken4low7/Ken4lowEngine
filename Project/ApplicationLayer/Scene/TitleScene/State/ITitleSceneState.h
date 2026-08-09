#pragma once

/// ---------- 前方宣言 ---------- ///
class TitleScene;

/// -------------------------------------------------------------
///				　	タイトルシーンの状態基底クラス
/// -------------------------------------------------------------
class ITitleSceneState
{
public: /// ---------- 純粋仮想関数 ---------- ///

	/// <summary>
	/// デストラクタ
	/// </summary>
	virtual ~ITitleSceneState() = default;

	/// <summary>
	/// ステートに入った瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Enter(TitleScene* scene) = 0;

	/// <summary>
	/// ステート中の更新処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Update(TitleScene* scene, float deltaTilme) = 0;

	/// <summary>
	/// ステートから抜ける瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Exit(TitleScene* scene) = 0;
};

