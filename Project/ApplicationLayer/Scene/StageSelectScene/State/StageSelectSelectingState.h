#pragma once
#include "IStageSelectSceneState.h"

/// -------------------------------------------------------------
///				ステージセレクトシーン：選択中状態
/// -------------------------------------------------------------
class StageSelectSelectingState : public IStageSelectSceneState
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~StageSelectSelectingState() = default;

	/// <summary>
	/// ステートに入った瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	void Enter(StageSelectScene* scene) override;

	/// <summary>
	/// ステート中の更新処理
	/// </summary>
	/// <param name="scene"></param>
	void Update(StageSelectScene* scene, float deltaTime) override;

	/// <summary>
	/// ステートから抜ける瞬間の処理
	/// </summary>
	/// <param name="scene"></param>
	void Exit(StageSelectScene* scene) override;

private:
	enum class UnlockPhase
	{
		None,
		WaitForUncover,
		MoveToTarget,
		UnlockPulse,
		Settle,
	};

	bool UpdateUnlockSequence(StageSelectScene* scene, float deltaTime);
	void CommitUnlock(StageSelectScene* scene);
	void FinishUnlock(StageSelectScene* scene);

private:
	UnlockPhase unlockPhase_ = UnlockPhase::None;
	float unlockPhaseTimer_ = 0.0f;
	int unlockSourceIndex_ = -1;
	int unlockTargetIndex_ = -1;
	bool unlockCommitted_ = false;
};