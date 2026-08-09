#pragma once

/// ---------- 前方宣言 ---------- ///
class StageSelectScene;

/// -------------------------------------------------------------
///			ステージセレクトシーン状態インターフェース
/// -------------------------------------------------------------
class IStageSelectSceneState
{
public: /// ---------- 純粋仮想関数 ---------- ///
	
	/// <summary>
	/// デストラクタ
	/// </summary>
	virtual ~IStageSelectSceneState() = default;

	/// <summary>
	/// ステートに入った瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Enter(StageSelectScene* scene) = 0;

	/// <summary>
	/// ステート中の更新処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Update(StageSelectScene* scene, float deltaTilme) = 0;

	/// <summary>
	/// ステートから抜ける瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	virtual void Exit(StageSelectScene* scene) = 0;

};

