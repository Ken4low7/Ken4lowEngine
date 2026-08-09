#pragma once
#include "WaveManager.h"

#include <memory>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///                      ゲームプレイのフロー制御
/// -------------------------------------------------------------
class GamePlayStageContext
{
public: /// ---------- 列挙型 ---------- ///

	enum class StageObjectiveType
	{
		ClearAllWaves,   // 全ウェーブ撃破
		ActivateDevices, // 装置起動
		DefendTarget,    // 防衛
		ReachGoal,       // ゴール到達
		DefeatBoss       // ボス撃破
	};

public: /// ---------- 構造体 ---------- ///

	struct StageAssetPaths
	{
		std::string jsonPath;
		std::string modelPath;
		bool instancedOnly = false; // 一体型glTFを使わず、JSON内のStaticMesh配置だけで描画する。
	};

	struct StageRule
	{
		StageObjectiveType objectiveType = StageObjectiveType::ClearAllWaves;

		bool useWaveSystem = true;
		bool hasBoss = false;

		int requiredDeviceCount = 0;
		float defendTimeSec = 0.0f;
		float timeLimitSec = 0.0f;
	};

	struct EnemySpawnInfo
	{
		K4E::Vector3 position{ 0.0f, 0.0f, 0.0f };
		int wave = 1;
		int group = 0;
		int count = 1;
		EnemyType enemyType = EnemyType::Melee;
	};

	struct IntroCameraPointInfo
	{
		K4E::Vector3 position{ 0.0f, 0.0f, 0.0f };
		K4E::Vector3 rotation{ 0.0f, 0.0f, 0.0f };

		int order = 0;
		float duration = 1.5f;
		float fov = 45.0f;
		std::string targetName;
		std::string interpMode = "Linear";
		std::string aimMode = "Target";
	};

	struct IntroLookAtPointInfo
	{
		std::string name;
		K4E::Vector3 position{ 0.0f, 0.0f, 0.0f };
	};

	struct DevicePointInfo
	{
		std::string name;
		K4E::Vector3 position{ 0.0f, 0.0f, 0.0f };
	};

	struct DefenseTargetPointInfo
	{
		std::string name;
		K4E::Vector3 position{ 0.0f, 0.0f, 0.0f };
	};

	struct GoalPointInfo
	{
		std::string name;
		K4E::Vector3 position{ 0.0f, 0.0f, 0.0f };
	};

public: /// ---------- メンバ関数 ---------- ///

	void InitializeFromRepository();
	StageAssetPaths GetCurrentStageAssets() const;
	StageRule GetCurrentStageRule() const;

	void LoadSpawnPointsFromLevel(const std::string& jsonPath);
	void SetupWaves(WaveManager* waveManager) const;
	void UnlockNextStage();

	int GetCurrentStageIndex() const { return currentStageIndex_; }
	bool IsBeginningPlainStage() const;

	bool HasPlayerSpawnPoint() const { return hasPlayerSpawnPoint_; }
	const K4E::Vector3& GetPlayerSpawnPoint() const { return playerSpawnPoint_; }

	bool HasBossSpawnPoint() const { return hasBossSpawnPoint_; }
	const K4E::Vector3& GetBossSpawnPoint() const { return bossSpawnPoint_; }

	const std::vector<EnemySpawnInfo>& GetEnemySpawnInfos() const { return enemySpawnInfos_; }
	const std::vector<IntroCameraPointInfo>& GetIntroCameraPoints() const { return introCameraPoints_; }
	const std::vector<IntroLookAtPointInfo>& GetIntroLookAtPoints() const { return introLookAtPoints_; }

	const std::vector<DevicePointInfo>& GetDevicePoints() const { return devicePoints_; }
	const std::vector<DefenseTargetPointInfo>& GetDefenseTargetPoints() const { return defenseTargetPoints_; }
	const std::vector<GoalPointInfo>& GetGoalPoints() const { return goalPoints_; }

private: /// ---------- メンバ関数 ---------- ///

	StageAssetPaths GetStageAssetPaths(int stageIndex) const;
	StageRule GetStageRule(int stageIndex) const;

private: /// ---------- メンバ変数 ---------- ///

	int currentStageIndex_ = 0;

	std::vector<EnemySpawnInfo> enemySpawnInfos_;

	K4E::Vector3 playerSpawnPoint_{ 0.0f, 0.0f, 0.0f };
	K4E::Vector3 bossSpawnPoint_{ 0.0f, 0.0f, 0.0f };
	bool hasPlayerSpawnPoint_ = false;
	bool hasBossSpawnPoint_ = false;

	std::vector<IntroCameraPointInfo> introCameraPoints_;
	std::vector<IntroLookAtPointInfo> introLookAtPoints_;

	std::vector<DevicePointInfo> devicePoints_;
	std::vector<DefenseTargetPointInfo> defenseTargetPoints_;
	std::vector<GoalPointInfo> goalPoints_;
};
