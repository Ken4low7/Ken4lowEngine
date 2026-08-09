#define NOMINMAX
#include "GamePlayStageContext.h"

#include "StageRepository.h"
#include "LevelLoader.h"

#include <algorithm>

using namespace Ken4lowEngine;

namespace
{
	std::vector<StageInfo> BuildDefaultStages()
	{
		std::vector<StageInfo> stages;
		stages.push_back({
			0u,
			"始まりの平原",
			"WAVE",
			"UI/StageSelect/stage01.dds",
			"基本戦闘を学ぶウェーブ制ステージ",
			"",
			false,
			0u,
			{ 0.18f, 0.49f, 0.20f, 1.0f },
			false
			});

		stages.push_back({
			1u,
			"忘れられた坑道",
			"SEARCH",
			"UI/StageSelect/stage02.dds",
			"ルート探索と装置起動を進める探索ステージ",
			"Stage 1 クリアで解放",
			true,
			0u,
			{ 0.43f, 0.30f, 0.25f, 1.0f },
			false
			});

		stages.push_back({
			2u,
			"旧防衛拠点",
			"DEFENSE",
			"UI/StageSelect/stage03.dds",
			"波状攻撃から拠点を守り抜く防衛ステージ",
			"Stage 2 クリアで解放",
			true,
			0u,
			{ 0.25f, 0.38f, 0.62f, 1.0f },
			false
			});

		stages.push_back({
			3u,
			"崩落都市圏",
			"ESCAPE",
			"UI/StageSelect/stage04.dds",
			"敵をかわしながら出口を目指す脱出ステージ",
			"Stage 3 クリアで解放",
			true,
			0u,
			{ 0.60f, 0.32f, 0.22f, 1.0f },
			false
			});

		stages.push_back({
			4u,
			"中枢制御塔",
			"BOSS",
			"UI/StageSelect/stage05.dds",
			"最終ボスとの決戦に挑む最終ステージ",
			"Stage 4 クリアで解放",
			true,
			0u,
			{ 0.45f, 0.18f, 0.18f, 1.0f },
			false
			});
		return stages;
	}

	bool IsPlayerSpawnType(const std::string& type) { return type == "PlayerSpawnPoint"; }
	bool IsEnemySpawnType(const std::string& type) { return type == "EnemySpawnPoint"; }
	bool IsBossSpawnType(const std::string& type) { return type == "BossSpawnPoint"; }

	bool IsDevicePointType(const std::string& type) { return type == "DevicePoint" || type == "DeviceObjective"; }
	bool IsDefenseTargetPointType(const std::string& type) { return type == "DefenseTargetPoint" || type == "DefenseTarget"; }
	bool IsGoalPointType(const std::string& type) { return type == "GoalPoint" || type == "EscapePoint"; }

	std::vector<GamePlayStageContext::DevicePointInfo> BuildStage2FallbackDevicePoints()
	{
		return {
			{ "MineDevice_West",  { -31.0f, 0.0f, -7.0f } },
			{ "MineDevice_East",  {  31.0f, 0.0f,  0.0f } },
			{ "MineDevice_Deep",  {   0.0f, 0.0f, 28.0f } }
		};
	}
}

void GamePlayStageContext::InitializeFromRepository()
{
	auto& repo = StageRepository::GetInstance();

	auto stages = repo.GetStages();
	if (stages.empty())
	{
		stages = BuildDefaultStages();
		repo.SetStages(stages);
	}

	int stageIndex = repo.GetStartIndex().value_or(0);

	if (stageIndex < 0)
	{
		stageIndex = 0;
	}
	if (!stages.empty() && stageIndex >= static_cast<int>(stages.size()))
	{
		stageIndex = static_cast<int>(stages.size()) - 1;
	}

	currentStageIndex_ = stageIndex;
	repo.SetStartIndex(currentStageIndex_);
}

GamePlayStageContext::StageAssetPaths GamePlayStageContext::GetCurrentStageAssets() const
{
	return GetStageAssetPaths(currentStageIndex_);
}

GamePlayStageContext::StageRule GamePlayStageContext::GetCurrentStageRule() const
{
	return GetStageRule(currentStageIndex_);
}

bool GamePlayStageContext::IsBeginningPlainStage() const
{
	const auto& stages = StageRepository::GetInstance().GetStages();
	if (currentStageIndex_ >= 0 && currentStageIndex_ < static_cast<int>(stages.size()))
	{
		return stages[static_cast<size_t>(currentStageIndex_)].name == "始まりの平原";
	}

	return currentStageIndex_ == 0;
}

GamePlayStageContext::StageAssetPaths GamePlayStageContext::GetStageAssetPaths(int stageIndex) const
{
	switch (stageIndex)
	{
	case 0: return { "stages/hajimarinoheigen.json", "Stages/hajimarinoheigen.gltf", false };
	case 1: return { "stages/wasureraretakoudou.json", "", true }; // Stage2は一体型glTFを読まず、JSONのモジュール配置だけをインスタンス描画する。
	case 2: return { "stages/kyuuboueikyoten.json", "", true }; // Stage3は防衛拠点のモジュール配置だけをインスタンス描画する。
	case 3: return { "stages/fps_stage03.json", "Stages/fps_stage03.gltf", false };
	case 4: return { "stages/fps_stage04.json", "Stages/fps_stage04.gltf", false };
	default:
		return { "stages/fps_stage00.json", "Stages/fps_stage00.gltf", false };
	}
}

GamePlayStageContext::StageRule GamePlayStageContext::GetStageRule(int stageIndex) const
{
	switch (stageIndex)
	{
	case 0:
		return {
			StageObjectiveType::ClearAllWaves,
			true,
			false,
			0,
			0.0f,
			0.0f
		};

	case 1:
		return {
			StageObjectiveType::ActivateDevices,
			false,
			false,
			3,
			0.0f,
			0.0f
		};

	case 2:
		return {
			StageObjectiveType::DefendTarget,
			true,
			false,
			0,
			90.0f,
			0.0f
		};

	case 3:
		return {
			StageObjectiveType::ReachGoal,
			false,
			false,
			0,
			0.0f,
			180.0f
		};

	case 4:
		return {
			StageObjectiveType::DefeatBoss,
			false,
			true,
			0,
			0.0f,
			0.0f
		};

	default:
		return {
			StageObjectiveType::ClearAllWaves,
			true,
			false,
			0,
			0.0f,
			0.0f
		};
	}
}

void GamePlayStageContext::LoadSpawnPointsFromLevel(const std::string& jsonPath)
{
	devicePoints_.clear();
	defenseTargetPoints_.clear();
	goalPoints_.clear();

	enemySpawnInfos_.clear();
	introCameraPoints_.clear();
	introLookAtPoints_.clear();

	hasPlayerSpawnPoint_ = false;
	hasBossSpawnPoint_ = false;

	playerSpawnPoint_ = { 0.0f, 0.0f, 0.0f };
	bossSpawnPoint_ = { 0.0f, 0.0f, 0.0f };

	const std::unique_ptr<K4E::LevelData> levelData = K4E::LevelLoader::LoadLevel(jsonPath);
	if (!levelData)
	{
		if (currentStageIndex_ == 1)
		{
			devicePoints_ = BuildStage2FallbackDevicePoints();
			playerSpawnPoint_ = { 0.0f, 1.0f, -43.0f };
			hasPlayerSpawnPoint_ = true; // Level読込失敗時もStage2の操作確認だけは開始できる安全なFallbackを持つ。
		}
		return;
	}

	for (const auto& object : levelData->objects)
	{
		if (IsPlayerSpawnType(object.type))
		{
			if (!hasPlayerSpawnPoint_)
			{
				playerSpawnPoint_ = object.position;
				hasPlayerSpawnPoint_ = true;
			}
		}
		else if (IsEnemySpawnType(object.type))
		{
			EnemySpawnInfo info{};
			info.position = object.position;

			if (object.hasSpawnProps)
			{
				info.wave = (object.spawnProps.wave > 0) ? object.spawnProps.wave : 1;
				info.group = object.spawnProps.group;
				info.count = (object.spawnProps.count > 0) ? object.spawnProps.count : 1;
				if (object.spawnProps.hasEnemyType)
				{
					info.enemyType = ParseEnemyType(object.spawnProps.enemyType); // JSONの敵種別をWave生成へ渡す。
				}
			}

			enemySpawnInfos_.push_back(info);
		}
		else if (IsBossSpawnType(object.type))
		{
			if (!hasBossSpawnPoint_)
			{
				bossSpawnPoint_ = object.position;
				hasBossSpawnPoint_ = true;
			}
		}
		else if (object.type == "IntroCameraPoint")
		{
			IntroCameraPointInfo info{};
			info.position = object.position;
			info.rotation = object.rotation;

			if (object.hasIntroCameraProps)
			{
				info.order = object.introCameraProps.order;
				info.duration = object.introCameraProps.duration;
				info.fov = object.introCameraProps.fov;
				info.targetName = object.introCameraProps.targetName;
				info.interpMode = object.introCameraProps.interpMode;
				info.aimMode = object.introCameraProps.aimMode;
			}

			introCameraPoints_.push_back(info);
		}
		else if (object.type == "IntroLookAtPoint")
		{
			IntroLookAtPointInfo info{};
			info.name = object.name;
			info.position = object.position;
			introLookAtPoints_.push_back(info);
		}
		else if (IsDevicePointType(object.type))
		{
			DevicePointInfo info{};
			info.name = object.hasDeviceObjectiveProps && !object.deviceObjectiveProps.objectiveId.empty()
				? object.deviceObjectiveProps.objectiveId
				: object.name;
			info.position = object.position;
			devicePoints_.push_back(info); // Instanced Stage内のDeviceObjectiveを装置Actor配置へ変換する。
		}
		else if (IsDefenseTargetPointType(object.type))
		{
			DefenseTargetPointInfo info{};
			info.name = object.name;
			info.position = object.position;
			defenseTargetPoints_.push_back(info);
		}
		else if (IsGoalPointType(object.type))
		{
			GoalPointInfo info{};
			info.name = object.name;
			info.position = object.position;
			goalPoints_.push_back(info);
		}
	}

	if (currentStageIndex_ == 1 && devicePoints_.empty())
	{
		devicePoints_ = BuildStage2FallbackDevicePoints(); // JSONのObjectiveが欠けても3基の進行不能を避ける。
	}

	std::sort(
		introCameraPoints_.begin(),
		introCameraPoints_.end(),
		[](const IntroCameraPointInfo& a, const IntroCameraPointInfo& b)
		{
			return a.order < b.order;
		});
}

void GamePlayStageContext::SetupWaves(WaveManager* waveManager) const
{
	if (!waveManager) return;

	std::vector<WaveDefinition> waves;

	if (!enemySpawnInfos_.empty())
	{
		int maxWave = 0;
		for (const auto& spawn : enemySpawnInfos_)
		{
			if (spawn.wave > maxWave) maxWave = spawn.wave;
		}

		if (maxWave <= 0) maxWave = 1;
		waves.resize(static_cast<size_t>(maxWave));

		for (int i = 0; i < maxWave; ++i)
		{
			if (i == 0) waves[static_cast<size_t>(i)].delayBeforeSpawnSec = 0.0f;
			else if (i == 1) waves[static_cast<size_t>(i)].delayBeforeSpawnSec = 2.0f;
			else waves[static_cast<size_t>(i)].delayBeforeSpawnSec = 2.5f;
		}

		for (const auto& spawn : enemySpawnInfos_)
		{
			const int waveIndex = std::max(0, spawn.wave - 1);

			WaveSpawnEntry entry{};
			entry.position = spawn.position;
			entry.enemyType = spawn.enemyType;
			if (IsBeginningPlainStage() && entry.enemyType == EnemyType::MidRange)
			{
				entry.enemyType = EnemyType::Melee; // Stage1は初心者向けに近接敵へ統一する。
			}

			const int spawnCount = std::max(1, spawn.count);
			for (int i = 0; i < spawnCount; ++i)
			{
				waves[static_cast<size_t>(waveIndex)].enemies.push_back(entry);
			}
		}
	}
	else
	{
		WaveDefinition wave;
		wave.delayBeforeSpawnSec = 0.0f;
		waves.push_back(wave);
	}

	waveManager->SetWaves(waves);
}

void GamePlayStageContext::UnlockNextStage()
{
	auto& repo = StageRepository::GetInstance();

	auto stages = repo.GetStages();
	if (stages.empty())
	{
		stages = BuildDefaultStages();
	}

	const int clearedStageIndex = currentStageIndex_;
	if (clearedStageIndex < 0 || clearedStageIndex >= static_cast<int>(stages.size()))
	{
		repo.SetStages(stages);
		repo.SetStartIndex(0);
		return;
	}

	const int nextStageIndex = clearedStageIndex + 1;

	for (auto& stage : stages)
	{
		stage.justUnlocked = false;
	}

	if (nextStageIndex >= static_cast<int>(stages.size()))
	{
		repo.SetStages(stages);
		repo.SetStartIndex(clearedStageIndex);
		return;
	}

	stages[nextStageIndex].locked = false;
	stages[nextStageIndex].justUnlocked = true;

	repo.SetStages(stages);
	repo.SetStartIndex(nextStageIndex);
}
