#define NOMINMAX
#include "StageObjectiveManager.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

void StageObjectiveManager::Initialize(const GamePlayStageContext& stageContext)
{
	stageRule_ = stageContext.GetCurrentStageRule();
	devicePointCount_ = static_cast<int>(stageContext.GetDevicePoints().size());
	defenseTargetPointCount_ = static_cast<int>(stageContext.GetDefenseTargetPoints().size());
	goalPointCount_ = static_cast<int>(stageContext.GetGoalPoints().size());
	goalPosition_ = {};
	goalDistance_ = -1.0f;
	hasGoalPoint_ = false;
	if (!stageContext.GetGoalPoints().empty())
	{
		goalPosition_ = stageContext.GetGoalPoints().front().position;
		hasGoalPoint_ = true;
	}
	else if (stageRule_.objectiveType == GamePlayStageContext::StageObjectiveType::ReachGoal)
	{
		goalPosition_ = { 0.0f, 1.0f, 181.0f };
		hasGoalPoint_ = true; // GoalPointが欠けた場合も崩落都市の終端へ到達すれば進行不能にならない。
	}
	hasBossSpawnPoint_ = stageContext.HasBossSpawnPoint();
	activatedDeviceCount_ = 0;
	defendElapsedSec_ = 0.0f;
	stageElapsedSec_ = 0.0f;
	reachedGoal_ = false;
	bossDefeated_ = false;
	bossArenaReached_ = false;
	defenseTargetDestroyed_ = false;
	allWavesCleared_ = false;
	requiresBossAfterDevices_ = stageRule_.objectiveType == GamePlayStageContext::StageObjectiveType::ActivateDevices && stageRule_.hasBoss;
	activatedDeviceIds_.clear();
	status_ = Status::Active;
	RefreshSnapshot();
}

void StageObjectiveManager::Update(float deltaTime)
{
	if (status_ != Status::Active)
	{
		RefreshSnapshot();
		return;
	}
	const float safeDeltaTime = std::max(0.0f, deltaTime);
	stageElapsedSec_ += safeDeltaTime;
	if (stageRule_.objectiveType == GamePlayStageContext::StageObjectiveType::DefendTarget && !defenseTargetDestroyed_)
	{
		defendElapsedSec_ += safeDeltaTime;
	}
	UpdateReachGoalState();
	RefreshOutcome();
}

void StageObjectiveManager::SetRequiresBossAfterDevices(bool required)
{
	requiresBossAfterDevices_ = required && stageRule_.objectiveType == GamePlayStageContext::StageObjectiveType::ActivateDevices;
	if (!requiresBossAfterDevices_) bossArenaReached_ = false;
	RefreshOutcome(); // Stage2初期化時だけ三段階条件を有効にし、通常の装置Objectiveへ波及させない。
}

void StageObjectiveManager::SetBossArenaReached(bool reached)
{
	if (!RequiresBossAfterDevices() || !AreRequiredDevicesActivated()) return;
	bossArenaReached_ = reached;
	RefreshOutcome(); // 大広間への入場時点で初めて「ボスを撃破せよ」へ切り替える。
}

bool StageObjectiveManager::IsStageObjectiveCleared(bool allWavesCleared)
{
	allWavesCleared_ = allWavesCleared;
	RefreshOutcome();
	return status_ == Status::Cleared;
}

bool StageObjectiveManager::IsStageObjectiveFailed()
{
	RefreshOutcome();
	return status_ == Status::Failed;
}

bool StageObjectiveManager::NotifyDeviceActivated(const std::string& deviceId)
{
	if (status_ != Status::Active || deviceId.empty()) return false;
	const auto insertResult = activatedDeviceIds_.insert(deviceId);
	if (!insertResult.second) return false;
	activatedDeviceCount_ = std::max(activatedDeviceCount_ + 1, static_cast<int>(activatedDeviceIds_.size()));
	RefreshOutcome(); // 3基達成後は即Boss表示にせず、隠し通路探索Objectiveへ移行する。
	return true;
}

void StageObjectiveManager::AddActivatedDeviceCount(int amount)
{
	if (status_ != Status::Active) return;
	activatedDeviceCount_ = std::max(0, activatedDeviceCount_ + amount);
	RefreshOutcome();
}

void StageObjectiveManager::SetReachedGoal(bool reached)
{
	if (status_ != Status::Active && reached) return;
	reachedGoal_ = reached;
	if (reached) goalDistance_ = 0.0f;
	RefreshOutcome();
}

void StageObjectiveManager::SetBossDefeated(bool defeated)
{
	if (status_ != Status::Active && defeated) return;
	bossDefeated_ = defeated;
	RefreshOutcome();
}

void StageObjectiveManager::SetDefenseTargetDestroyed(bool destroyed)
{
	if (status_ != Status::Active && destroyed) return;
	defenseTargetDestroyed_ = destroyed;
	RefreshOutcome();
}

const char* StageObjectiveManager::GetStatusDebugName(Status status)
{
	switch (status)
	{
	case Status::Inactive: return "Inactive";
	case Status::Active: return "Active";
	case Status::Cleared: return "Cleared";
	case Status::Failed: return "Failed";
	default: return "Unknown";
	}
}

int StageObjectiveManager::GetRequiredDeviceCount() const
{
	const int configuredCount = stageRule_.requiredDeviceCount > 0 ? stageRule_.requiredDeviceCount : devicePointCount_;
	return std::max(1, configuredCount);
}

bool StageObjectiveManager::IsDevicePassagePhase() const
{
	return RequiresBossAfterDevices() && AreRequiredDevicesActivated() && !bossArenaReached_;
}

bool StageObjectiveManager::IsDeviceBossPhase() const
{
	return RequiresBossAfterDevices() && AreRequiredDevicesActivated() && bossArenaReached_;
}

bool StageObjectiveManager::EvaluateCleared() const
{
	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::ClearAllWaves:
		return allWavesCleared_;
	case GamePlayStageContext::StageObjectiveType::ActivateDevices:
		return AreRequiredDevicesActivated() && (!RequiresBossAfterDevices() || bossDefeated_);
	case GamePlayStageContext::StageObjectiveType::DefendTarget:
		return !defenseTargetDestroyed_ && defendElapsedSec_ >= std::max(0.0f, stageRule_.defendTimeSec);
	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		return reachedGoal_;
	case GamePlayStageContext::StageObjectiveType::DefeatBoss:
		return bossDefeated_;
	}
	return false;
}

bool StageObjectiveManager::EvaluateFailed() const
{
	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::DefendTarget:
		return defenseTargetDestroyed_;
	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		return stageRule_.timeLimitSec > 0.0f && stageElapsedSec_ >= stageRule_.timeLimitSec && !reachedGoal_;
	default:
		return false;
	}
}

void StageObjectiveManager::UpdateReachGoalState()
{
	if (stageRule_.objectiveType != GamePlayStageContext::StageObjectiveType::ReachGoal || reachedGoal_ || !hasGoalPoint_) return;
	IPlayerRuntime* player = IPlayerRuntime::GetActiveRuntime();
	if (!player) return;

	const K4E::Vector3 playerPosition = player->GetWorldPosition();
	const float deltaX = playerPosition.x - goalPosition_.x;
	const float deltaZ = playerPosition.z - goalPosition_.z;
	const float deltaY = std::fabs(playerPosition.y - goalPosition_.y);
	goalDistance_ = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
	if (goalDistance_ <= 6.0f && deltaY <= 8.0f)
	{
		reachedGoal_ = true; // JSONのEscapePoint半径へ入った瞬間に共通Objectiveを達成させる。
		goalDistance_ = 0.0f;
	}
}

void StageObjectiveManager::RefreshOutcome()
{
	if (status_ == Status::Active)
	{
		if (EvaluateCleared())
		{
			TransitionTo(Status::Cleared);
			return;
		}
		if (EvaluateFailed())
		{
			TransitionTo(Status::Failed);
			return;
		}
	}
	RefreshSnapshot();
}

void StageObjectiveManager::TransitionTo(Status nextStatus)
{
	if (status_ == nextStatus)
	{
		RefreshSnapshot();
		return;
	}
	status_ = nextStatus;
	RefreshSnapshot();
	if (statusChangedCallback_) statusChangedCallback_(status_, snapshot_);
}

void StageObjectiveManager::RefreshSnapshot()
{
	snapshot_ = {};
	snapshot_.type = stageRule_.objectiveType;
	snapshot_.status = status_;
	if (IsDevicePassagePhase()) snapshot_.title = "隠し通路の奥へ進め";
	else if (IsDeviceBossPhase()) snapshot_.title = "ボスを撃破せよ";
	else snapshot_.title = GetObjectiveTitle(stageRule_.objectiveType);
	snapshot_.detail = BuildActiveDetail();
	snapshot_.elapsedSec = stageElapsedSec_;

	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::ClearAllWaves:
		snapshot_.currentValue = allWavesCleared_ ? 1 : 0;
		snapshot_.targetValue = 1;
		snapshot_.normalizedProgress = allWavesCleared_ ? 1.0f : 0.0f;
		break;
	case GamePlayStageContext::StageObjectiveType::ActivateDevices:
		if (IsDevicePassagePhase() || IsDeviceBossPhase())
		{
			snapshot_.currentValue = IsDeviceBossPhase() && bossDefeated_ ? 1 : 0;
			snapshot_.targetValue = 1;
			snapshot_.normalizedProgress = snapshot_.currentValue > 0 ? 1.0f : 0.0f;
			snapshot_.usesCount = false;
		}
		else
		{
			snapshot_.usesCount = true;
			snapshot_.currentValue = activatedDeviceCount_;
			snapshot_.targetValue = GetRequiredDeviceCount();
			snapshot_.normalizedProgress = std::clamp(
				static_cast<float>(snapshot_.currentValue) / static_cast<float>(snapshot_.targetValue),
				0.0f,
				1.0f);
		}
		break;
	case GamePlayStageContext::StageObjectiveType::DefendTarget:
	{
		const float targetSec = std::max(0.0f, stageRule_.defendTimeSec);
		snapshot_.usesTimer = true;
		snapshot_.currentValue = static_cast<int>(std::floor(defendElapsedSec_));
		snapshot_.targetValue = static_cast<int>(std::ceil(targetSec));
		snapshot_.remainingSec = std::max(0.0f, targetSec - defendElapsedSec_);
		snapshot_.normalizedProgress = targetSec > 0.0f ? std::clamp(defendElapsedSec_ / targetSec, 0.0f, 1.0f) : 1.0f;
		break;
	}
	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		snapshot_.usesTimer = stageRule_.timeLimitSec > 0.0f;
		snapshot_.remainingSec = snapshot_.usesTimer ? std::max(0.0f, stageRule_.timeLimitSec - stageElapsedSec_) : 0.0f;
		snapshot_.normalizedProgress = snapshot_.usesTimer
			? std::clamp(snapshot_.remainingSec / stageRule_.timeLimitSec, 0.0f, 1.0f)
			: (reachedGoal_ ? 1.0f : 0.0f);
		break;
	case GamePlayStageContext::StageObjectiveType::DefeatBoss:
		snapshot_.currentValue = bossDefeated_ ? 1 : 0;
		snapshot_.targetValue = 1;
		snapshot_.normalizedProgress = bossDefeated_ ? 1.0f : 0.0f;
		break;
	}

	if (status_ == Status::Cleared)
	{
		snapshot_.detail = "目標達成";
		snapshot_.normalizedProgress = 1.0f;
	}
	else if (status_ == Status::Failed)
	{
		snapshot_.detail = "任務失敗";
	}
}

std::string StageObjectiveManager::BuildActiveDetail() const
{
	char text[96]{};
	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::ClearAllWaves:
		return "すべての敵ウェーブを撃破";
	case GamePlayStageContext::StageObjectiveType::ActivateDevices:
		if (IsDevicePassagePhase()) return "封鎖壁が開いた　坑道最奥の大広間を探索";
		if (IsDeviceBossPhase()) return bossDefeated_ ? "ボス撃破" : "坑道破砕体と交戦中";
		std::snprintf(text, sizeof(text), "起動済み %d / %d", activatedDeviceCount_, GetRequiredDeviceCount());
		return text;
	case GamePlayStageContext::StageObjectiveType::DefendTarget:
		std::snprintf(text, sizeof(text), "残り %.0f 秒", std::max(0.0f, stageRule_.defendTimeSec - defendElapsedSec_));
		return text;
	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		if (reachedGoal_) return "脱出地点に到達";
		if (hasGoalPoint_ && goalDistance_ >= 0.0f)
		{
			std::snprintf(text, sizeof(text), "青いビーコンまで %.0f m　残り %.0f 秒", goalDistance_, std::max(0.0f, stageRule_.timeLimitSec - stageElapsedSec_));
			return text;
		}
		if (stageRule_.timeLimitSec > 0.0f)
		{
			std::snprintf(text, sizeof(text), "青いビーコンを探せ　残り %.0f 秒", std::max(0.0f, stageRule_.timeLimitSec - stageElapsedSec_));
			return text;
		}
		return "青いビーコンを探せ";
	case GamePlayStageContext::StageObjectiveType::DefeatBoss:
		return "ボス戦進行中";
	}
	return {};
}

const char* StageObjectiveManager::GetObjectiveTitle(GamePlayStageContext::StageObjectiveType type)
{
	switch (type)
	{
	case GamePlayStageContext::StageObjectiveType::ClearAllWaves: return "敵部隊を殲滅せよ";
	case GamePlayStageContext::StageObjectiveType::ActivateDevices: return "装置を起動せよ";
	case GamePlayStageContext::StageObjectiveType::DefendTarget: return "防衛対象を守り抜け";
	case GamePlayStageContext::StageObjectiveType::ReachGoal: return "脱出地点へ到達せよ";
	case GamePlayStageContext::StageObjectiveType::DefeatBoss: return "ボスを撃破せよ";
	default: return "ステージ目標を達成せよ";
	}
}
