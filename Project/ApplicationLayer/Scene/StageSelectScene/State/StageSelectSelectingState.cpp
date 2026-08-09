#define NOMINMAX
#include "StageSelectSelectingState.h"
#include "StageSelectScene.h"

#include <DirectXCommon.h>
#include <Input.h>
#include "GridStageSelector.h"
#include "StageRepository.h"
#include "LinearInterpolation.h"
#include "StageSelectLoadState.h"

#include <algorithm>

void StageSelectSelectingState::Enter(StageSelectScene* scene)
{
	if (!scene) { return; }

	scene->SetState(StageSelectScene::State::Selecting);
	unlockPhase_ = UnlockPhase::None;
	unlockPhaseTimer_ = 0.0f;
	unlockSourceIndex_ = -1;
	unlockTargetIndex_ = scene->GetPendingUnlockIndex();
	unlockCommitted_ = false;

	const auto& stages = scene->GetStages();
	if (unlockTargetIndex_ >= 0 && unlockTargetIndex_ < static_cast<int>(stages.size()))
	{
		unlockSourceIndex_ = std::max(0, unlockTargetIndex_ - 1);
		unlockPhase_ = UnlockPhase::WaitForUncover;
		if (auto* selector = scene->GetActiveSelector())
		{
			selector->FocusToIndex(unlockSourceIndex_, false); // フェード中は解除前のステージを中央へ固定する。
		}
	}
}

void StageSelectSelectingState::Update(StageSelectScene* scene, float deltaTime)
{
	if (!scene) { return; }

	auto* dxCommon = scene->GetDxCommon();
	auto* input = scene->GetInput();
	if (!dxCommon || !input) { return; }

	if (auto* selector = scene->GetActiveSelector())
	{
		selector->Update(deltaTime);
	}

	auto& bgNow = scene->GetBgNow();
	auto& bgTarget = scene->GetBgTarget();
	const float t = std::clamp(deltaTime * 4.0f, 0.0f, 1.0f);
	bgNow = Lerp(bgNow, bgTarget, t);
	bgNow.w = 1.0f;

	if (auto* bgSprite = scene->GetBgSprite())
	{
		bgSprite->SetColor(bgNow);
		bgSprite->Update();
	}

	if (UpdateUnlockSequence(scene, deltaTime))
	{
		return; // 解放演出中は選択・戻る操作より演出進行を優先する。
	}

	if (input->TriggerKey(DIK_ESCAPE))
	{
		if (scene->GetNextScene() == StageSelectScene::NextScene::None)
		{
			scene->SetNextScene(StageSelectScene::NextScene::Title);
			scene->BackToTitle();
		}
		return;
	}
}

void StageSelectSelectingState::Exit(StageSelectScene* scene)
{
	(void)scene;
	unlockPhase_ = UnlockPhase::None;
	unlockPhaseTimer_ = 0.0f;
	unlockSourceIndex_ = -1;
	unlockTargetIndex_ = -1;
	unlockCommitted_ = false;
}

bool StageSelectSelectingState::UpdateUnlockSequence(StageSelectScene* scene, float deltaTime)
{
	if (!scene || unlockPhase_ == UnlockPhase::None)
	{
		return false;
	}

	auto* selector = scene->GetActiveSelector();
	if (!selector || unlockTargetIndex_ < 0 || unlockTargetIndex_ >= static_cast<int>(scene->GetStages().size()))
	{
		FinishUnlock(scene);
		return false;
	}

	if (unlockPhase_ == UnlockPhase::WaitForUncover)
	{
		selector->FocusToIndex(unlockSourceIndex_, false);
		if (scene->IsEntranceTransitionFinished())
		{
			selector->FocusToIndex(unlockTargetIndex_, true); // フェードが完全に終わった最初のフレームで次ステージへ移動する。
			unlockPhase_ = UnlockPhase::MoveToTarget;
			unlockPhaseTimer_ = 0.0f;
		}
		return true;
	}

	unlockPhaseTimer_ += std::max(0.0f, deltaTime);
	switch (unlockPhase_)
	{
	case UnlockPhase::MoveToTarget:
		if (unlockPhaseTimer_ >= 0.42f)
		{
			selector->FocusToIndex(unlockTargetIndex_, false);
			if (auto* grid = dynamic_cast<GridStageSelector*>(selector))
			{
				grid->PlayUnlockPresentation(unlockTargetIndex_);
			}
			unlockPhase_ = UnlockPhase::UnlockPulse;
			unlockPhaseTimer_ = 0.0f;
		}
		break;

	case UnlockPhase::UnlockPulse:
		selector->FocusToIndex(unlockTargetIndex_, false);
		if (!unlockCommitted_ && unlockPhaseTimer_ >= 0.36f)
		{
			CommitUnlock(scene);
		}
		if (unlockPhaseTimer_ >= 0.66f)
		{
			unlockPhase_ = UnlockPhase::Settle;
			unlockPhaseTimer_ = 0.0f;
		}
		break;

	case UnlockPhase::Settle:
		selector->FocusToIndex(unlockTargetIndex_, false);
		if (unlockPhaseTimer_ >= 0.24f)
		{
			FinishUnlock(scene);
		}
		break;

	case UnlockPhase::WaitForUncover:
	case UnlockPhase::None:
	default:
		break;
	}
	return unlockPhase_ != UnlockPhase::None;
}

void StageSelectSelectingState::CommitUnlock(StageSelectScene* scene)
{
	if (!scene || unlockCommitted_) return;
	auto& stages = scene->GetStages();
	if (unlockTargetIndex_ < 0 || unlockTargetIndex_ >= static_cast<int>(stages.size())) return;

	stages[unlockTargetIndex_].locked = false;
	StageRepository::GetInstance().SetStages(stages);
	scene->RefreshStageSelectUI();
	unlockCommitted_ = true;
}

void StageSelectSelectingState::FinishUnlock(StageSelectScene* scene)
{
	if (!scene)
	{
		unlockPhase_ = UnlockPhase::None;
		return;
	}

	auto& stages = scene->GetStages();
	if (unlockTargetIndex_ >= 0 && unlockTargetIndex_ < static_cast<int>(stages.size()))
	{
		stages[unlockTargetIndex_].locked = false;
		stages[unlockTargetIndex_].justUnlocked = false;
		StageRepository::GetInstance().SetStages(stages);
		StageRepository::GetInstance().SetStartIndex(unlockTargetIndex_);
		scene->SetCurrentStageIndex(unlockTargetIndex_);
	}

	scene->GetPendingUnlockIndex() = -1;
	scene->RefreshStageSelectUI();
	unlockPhase_ = UnlockPhase::None;
	unlockPhaseTimer_ = 0.0f;
	unlockSourceIndex_ = -1;
	unlockTargetIndex_ = -1;
	unlockCommitted_ = false;
}