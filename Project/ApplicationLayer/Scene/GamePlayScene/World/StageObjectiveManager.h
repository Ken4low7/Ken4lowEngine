#pragma once
#include "GamePlayStageContext.h"

#include <functional>
#include <string>
#include <unordered_set>
#include <utility>

class StageObjectiveManager
{
public:
	enum class Status
	{
		Inactive,
		Active,
		Cleared,
		Failed,
	};

	struct Snapshot
	{
		GamePlayStageContext::StageObjectiveType type = GamePlayStageContext::StageObjectiveType::ClearAllWaves;
		Status status = Status::Inactive;
		std::string title;
		std::string detail;
		int currentValue = 0;
		int targetValue = 0;
		float normalizedProgress = 0.0f;
		float elapsedSec = 0.0f;
		float remainingSec = 0.0f;
		bool usesCount = false;
		bool usesTimer = false;
	};

	using StatusChangedCallback = std::function<void(Status, const Snapshot&)>;

	void Initialize(const GamePlayStageContext& stageContext);
	void Update(float deltaTime);
	void SetRequiresBossAfterDevices(bool required);
	void SetBossArenaReached(bool reached);

	bool UsesWaveSystem() const { return stageRule_.useWaveSystem; }
	float GetStageElapsedSec() const { return stageElapsedSec_; }
	int GetActivatedDeviceCount() const { return activatedDeviceCount_; }
	int GetDevicePointCount() const { return devicePointCount_; }
	int GetDefenseTargetPointCount() const { return defenseTargetPointCount_; }
	int GetGoalPointCount() const { return goalPointCount_; }
	bool HasBossSpawnPoint() const { return hasBossSpawnPoint_; }
	bool HasReachedGoal() const { return reachedGoal_; }
	bool IsBossDefeated() const { return bossDefeated_; }
	bool IsDefenseTargetDestroyed() const { return defenseTargetDestroyed_; }
	bool AreRequiredDevicesActivated() const { return activatedDeviceCount_ >= GetRequiredDeviceCount(); }
	bool RequiresBossAfterDevices() const { return requiresBossAfterDevices_; }
	bool HasReachedBossArena() const { return bossArenaReached_; }
	Status GetStatus() const { return status_; }
	const Snapshot& GetSnapshot() const { return snapshot_; }

	bool IsStageObjectiveCleared(bool allWavesCleared);
	bool IsStageObjectiveFailed();
	bool NotifyDeviceActivated(const std::string& deviceId);
	void AddActivatedDeviceCount(int amount = 1);
	void SetReachedGoal(bool reached);
	void SetBossDefeated(bool defeated);
	void SetDefenseTargetDestroyed(bool destroyed);

	void SetStatusChangedCallback(StatusChangedCallback callback) { statusChangedCallback_ = std::move(callback); }
	static const char* GetStatusDebugName(Status status);

private:
	int GetRequiredDeviceCount() const;
	bool IsDevicePassagePhase() const;
	bool IsDeviceBossPhase() const;
	bool EvaluateCleared() const;
	bool EvaluateFailed() const;
	void UpdateReachGoalState();
	void RefreshOutcome();
	void TransitionTo(Status nextStatus);
	void RefreshSnapshot();
	std::string BuildActiveDetail() const;
	static const char* GetObjectiveTitle(GamePlayStageContext::StageObjectiveType type);

private:
	GamePlayStageContext::StageRule stageRule_{};
	int devicePointCount_ = 0;
	int defenseTargetPointCount_ = 0;
	int goalPointCount_ = 0;
	K4E::Vector3 goalPosition_{};
	float goalDistance_ = -1.0f;
	bool hasGoalPoint_ = false;
	bool hasBossSpawnPoint_ = false;
	int activatedDeviceCount_ = 0;
	float defendElapsedSec_ = 0.0f;
	float stageElapsedSec_ = 0.0f;
	bool reachedGoal_ = false;
	bool bossDefeated_ = false;
	bool bossArenaReached_ = false;
	bool defenseTargetDestroyed_ = false;
	bool allWavesCleared_ = false;
	bool requiresBossAfterDevices_ = false;
	Status status_ = Status::Inactive;
	Snapshot snapshot_{};
	std::unordered_set<std::string> activatedDeviceIds_;
	StatusChangedCallback statusChangedCallback_;
};
