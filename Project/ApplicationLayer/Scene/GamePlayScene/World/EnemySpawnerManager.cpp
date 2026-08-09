#define NOMINMAX
#include "EnemySpawnerManager.h"

#include "CharacterWorld.h"
#include "ParameterManager.h"
#include "Wireframe.h"

#include <algorithm>
#include <cmath>
#include <random>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	constexpr const char* kSpawnerRootGroup = "EnemySpawner";

	const std::vector<std::string>& EnemyTypeOptions()
	{
		static const std::vector<std::string> options = {
			"Melee",
			"MidRange"
		};
		return options;
	}

	const std::vector<std::string>& SpawnPatternOptions()
	{
		static const std::vector<std::string> options = {
			"Single",
			"Interval",
			"Burst"
		};
		return options;
	}

	float RandomRange(float minValue, float maxValue)
	{
		static std::mt19937 engine{ std::random_device{}() };
		std::uniform_real_distribution<float> dist(minValue, maxValue);
		return dist(engine);
	}
}

void EnemySpawnerManager::Initialize()
{
	BuildDefaultSpawners();

	for (RuntimeSpawner& spawner : spawners_)
	{
		RegisterSpawnerParameters(spawner);
		ApplyParameters(spawner);
	}
}

void EnemySpawnerManager::Finalize()
{
	for (RuntimeSpawner& spawner : spawners_)
	{
		UnregisterSpawnerParameters(spawner);
	}
	spawners_.clear();
}

void EnemySpawnerManager::Update(CharacterWorld& characters, float deltaTime)
{
	// ParameterManagerのImGui編集値を毎フレーム読むことで、保存前の調整もスポナーへ反映する。
	SyncAllFromParameterManager();

	for (RuntimeSpawner& spawner : spawners_)
	{
		if (!ShouldSpawn(spawner, deltaTime))
		{
			continue;
		}

		SpawnByPattern(spawner, characters);
	}
}

void EnemySpawnerManager::DrawDebug() const
{
#ifdef _DEBUG
	auto* wireframe = K4E::Wireframe::GetInstance();
	if (!wireframe || !wireframe->IsDebugDrawEnabled())
	{
		return;
	}

	for (const RuntimeSpawner& spawner : spawners_)
	{
		const Vector4 color = spawner.data.isActive
			? Vector4{ 0.2f, 1.0f, 0.3f, 1.0f }
			: Vector4{ 0.5f, 0.5f, 0.5f, 0.7f };

		wireframe->DrawSphere(spawner.data.position, 0.35f, color);
		wireframe->DrawLine(
			spawner.data.position,
			spawner.data.position + Vector3{ 0.0f, 2.0f, 0.0f },
			color);

		if (spawner.data.spawnRadius > 0.0f)
		{
			wireframe->DrawSphere(spawner.data.position, spawner.data.spawnRadius, { color.x, color.y, color.z, 0.35f });
		}
	}
#endif
}

void EnemySpawnerManager::DrawImGuiContent() const
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Enemy Spawners");
	ImGui::Text("ParameterManager Group Root: %s", kSpawnerRootGroup);
	ImGui::Text("Spawner Count: %d", static_cast<int>(spawners_.size()));

	for (const RuntimeSpawner& spawner : spawners_)
	{
		if (ImGui::TreeNode(spawner.data.spawnerName.c_str()))
		{
			ImGui::Text("Group: %s", spawner.groupName.c_str());
			ImGui::Text("Active: %s", spawner.data.isActive ? "true" : "false");
			ImGui::Text("EnemyType: %s", spawner.data.enemyType.c_str());
			ImGui::Text("Pattern: %s", spawner.data.spawnPattern.c_str());
			ImGui::Text("Position: %.2f, %.2f, %.2f", spawner.data.position.x, spawner.data.position.y, spawner.data.position.z);
			ImGui::Text("Spawned: %d / %d", spawner.spawnedCount, spawner.data.maxSpawnCount);
			ImGui::TreePop();
		}
	}
#endif
}

std::vector<std::string> EnemySpawnerManager::GetGroupNames() const
{
	std::vector<std::string> groupNames;
	groupNames.reserve(spawners_.size());
	for (const RuntimeSpawner& spawner : spawners_)
	{
		groupNames.push_back(spawner.groupName);
	}
	return groupNames;
}

void EnemySpawnerManager::BuildDefaultSpawners()
{
	spawners_.clear();

	RuntimeSpawner enemySpawner01{};
	enemySpawner01.data.spawnerName = "EnemySpawner_01";
	enemySpawner01.data.isActive = false;
	enemySpawner01.data.position = { 0.0f, 1.0f, 12.0f };
	enemySpawner01.data.enemyType = "Melee";
	enemySpawner01.data.spawnInterval = 3.0f;
	enemySpawner01.data.initialDelay = 0.0f;
	enemySpawner01.data.maxSpawnCount = 3;
	enemySpawner01.data.spawnRadius = 2.0f;
	enemySpawner01.data.spawnPattern = "Interval";
	spawners_.push_back(enemySpawner01);

	RuntimeSpawner enemySpawner02{};
	enemySpawner02.data.spawnerName = "EnemySpawner_02";
	enemySpawner02.data.isActive = false;
	enemySpawner02.data.position = { 8.0f, 1.0f, 20.0f };
	enemySpawner02.data.enemyType = "MidRange";
	enemySpawner02.data.spawnInterval = 4.0f;
	enemySpawner02.data.initialDelay = 1.0f;
	enemySpawner02.data.maxSpawnCount = 2;
	enemySpawner02.data.spawnRadius = 3.0f;
	enemySpawner02.data.spawnPattern = "Interval";
	spawners_.push_back(enemySpawner02);

	RuntimeSpawner bossSpawner01{};
	bossSpawner01.data.spawnerName = "BossSpawner_01";
	bossSpawner01.data.isActive = false;
	bossSpawner01.data.position = { 0.0f, 2.25f, 30.0f };
	bossSpawner01.data.enemyType = "GuardianBoss";
	bossSpawner01.data.spawnInterval = 0.0f;
	bossSpawner01.data.initialDelay = 0.0f;
	bossSpawner01.data.maxSpawnCount = 1;
	bossSpawner01.data.spawnRadius = 0.0f;
	bossSpawner01.data.spawnPattern = "Single";
	spawners_.push_back(bossSpawner01);
}

void EnemySpawnerManager::RegisterSpawnerParameters(RuntimeSpawner& spawner)
{
	spawner.groupName = std::string(kSpawnerRootGroup) + "/" + spawner.data.spawnerName;

	auto* parameters = K4E::ParameterManager::GetInstance();
	parameters->CreateGroup(spawner.groupName);

	// スポナー設定はParameterManagerへ登録し、既存のJson保存・読み込み・ImGui編集に乗せる。
	parameters->AddStringItem(spawner.groupName, "spawnerName", spawner.data.spawnerName, {});
	parameters->AddItem(spawner.groupName, "isActive", spawner.data.isActive);
	parameters->AddItem(spawner.groupName, "position", spawner.data.position, Vector3{ -200.0f, -50.0f, -200.0f }, Vector3{ 200.0f, 50.0f, 200.0f });
	parameters->AddItem(spawner.groupName, "rotation", spawner.data.rotation, Vector3{ -3.141592f, -3.141592f, -3.141592f }, Vector3{ 3.141592f, 3.141592f, 3.141592f });
	parameters->AddStringItem(spawner.groupName, "enemyType", spawner.data.enemyType, EnemyTypeOptions());
	parameters->AddItem(spawner.groupName, "spawnInterval", spawner.data.spawnInterval, 0.0f, 60.0f);
	parameters->AddItem(spawner.groupName, "initialDelay", spawner.data.initialDelay, 0.0f, 60.0f);
	parameters->AddItem(spawner.groupName, "maxSpawnCount", spawner.data.maxSpawnCount, 0, 100);
	parameters->AddItem(spawner.groupName, "spawnRadius", spawner.data.spawnRadius, 0.0f, 50.0f);
	parameters->AddStringItem(spawner.groupName, "spawnPattern", spawner.data.spawnPattern, SpawnPatternOptions());

	parameters->SetDisplayName(spawner.groupName, "spawnerName", "スポナー名");
	parameters->SetDisplayName(spawner.groupName, "isActive", "有効");
	parameters->SetDisplayName(spawner.groupName, "position", "位置");
	parameters->SetDisplayName(spawner.groupName, "rotation", "回転");
	parameters->SetDisplayName(spawner.groupName, "enemyType", "敵タイプ");
	parameters->SetDisplayName(spawner.groupName, "spawnInterval", "湧き間隔");
	parameters->SetDisplayName(spawner.groupName, "initialDelay", "初回遅延");
	parameters->SetDisplayName(spawner.groupName, "maxSpawnCount", "最大湧き数");
	parameters->SetDisplayName(spawner.groupName, "spawnRadius", "湧き範囲");
	parameters->SetDisplayName(spawner.groupName, "spawnPattern", "湧き方");

	parameters->RegisterParameterApplier(spawner.groupName, this, [this, &spawner]() { ApplyParameters(spawner); });
	parameters->LoadFile(spawner.groupName);
}

void EnemySpawnerManager::UnregisterSpawnerParameters(RuntimeSpawner& spawner)
{
	K4E::ParameterManager::GetInstance()->UnregisterParameterApplier(spawner.groupName, this);
}

void EnemySpawnerManager::ApplyParameters(RuntimeSpawner& spawner)
{
	auto* parameters = K4E::ParameterManager::GetInstance();

	spawner.data.spawnerName = parameters->GetValue<std::string>(spawner.groupName, "spawnerName");
	spawner.data.isActive = parameters->GetValue<bool>(spawner.groupName, "isActive");
	spawner.data.position = parameters->GetValue<Vector3>(spawner.groupName, "position");
	spawner.data.rotation = parameters->GetValue<Vector3>(spawner.groupName, "rotation");
	spawner.data.enemyType = parameters->GetValue<std::string>(spawner.groupName, "enemyType");
	spawner.data.spawnInterval = std::max(0.0f, parameters->GetValue<float>(spawner.groupName, "spawnInterval"));
	spawner.data.initialDelay = std::max(0.0f, parameters->GetValue<float>(spawner.groupName, "initialDelay"));
	spawner.data.maxSpawnCount = std::max(0, parameters->GetValue<int32_t>(spawner.groupName, "maxSpawnCount"));
	spawner.data.spawnRadius = std::max(0.0f, parameters->GetValue<float>(spawner.groupName, "spawnRadius"));
	spawner.data.spawnPattern = parameters->GetValue<std::string>(spawner.groupName, "spawnPattern");
}

void EnemySpawnerManager::SyncAllFromParameterManager()
{
	for (RuntimeSpawner& spawner : spawners_)
	{
		ApplyParameters(spawner);
	}
}

bool EnemySpawnerManager::ShouldSpawn(RuntimeSpawner& spawner, float deltaTime) const
{
	if (!spawner.data.isActive || spawner.data.maxSpawnCount <= 0 || spawner.spawnedCount >= spawner.data.maxSpawnCount)
	{
		return false;
	}

	// 初回遅延を満たすまでは、通常の湧き間隔タイマーを進めない。
	if (!spawner.initialDelayElapsed)
	{
		spawner.timer += deltaTime;
		if (spawner.timer < spawner.data.initialDelay)
		{
			return false;
		}

		spawner.initialDelayElapsed = true;
		spawner.timer = 0.0f;
		return true;
	}

	if (spawner.data.spawnPattern == "Single" || spawner.data.spawnPattern == "Burst")
	{
		return spawner.spawnedCount == 0;
	}

	spawner.timer += deltaTime;
	if (spawner.timer < spawner.data.spawnInterval)
	{
		return false;
	}

	spawner.timer = 0.0f;
	return true;
}

void EnemySpawnerManager::SpawnByPattern(RuntimeSpawner& spawner, CharacterWorld& characters)
{
	if (spawner.data.spawnPattern == "Burst")
	{
		// Burstは条件成立フレームに残り数をまとめて生成する。
		while (spawner.spawnedCount < spawner.data.maxSpawnCount)
		{
			if (!SpawnOne(spawner, characters))
			{
				break;
			}
		}
		return;
	}

	SpawnOne(spawner, characters);
}

bool EnemySpawnerManager::SpawnOne(RuntimeSpawner& spawner, CharacterWorld& characters)
{
	EnemyType enemyType = EnemyType::Melee;
	if (!TryResolveEnemyType(spawner.data.enemyType, enemyType))
	{
		if (!spawner.unsupportedEnemyTypeReported)
		{
			spawner.unsupportedEnemyTypeReported = true;
		}
		// GuardianBossなど将来用タイプは今回生成せず、無限に試行しないよう消化済みにする。
		spawner.spawnedCount = spawner.data.maxSpawnCount;
		return false;
	}

	const K4E::Vector3 spawnPosition = BuildSpawnPosition(spawner);
	characters.SpawnEnemyAt(spawnPosition, enemyType);
	++spawner.spawnedCount;
	return true;
}

K4E::Vector3 EnemySpawnerManager::BuildSpawnPosition(const RuntimeSpawner& spawner) const
{
	K4E::Vector3 position = spawner.data.position;
	if (spawner.data.spawnRadius <= 0.0f)
	{
		return position;
	}

	const float angle = RandomRange(0.0f, 6.283184f);
	const float radius = std::sqrt(RandomRange(0.0f, 1.0f)) * spawner.data.spawnRadius;
	position.x += std::cos(angle) * radius;
	position.z += std::sin(angle) * radius;
	return position;
}

bool EnemySpawnerManager::TryResolveEnemyType(const std::string& enemyTypeName, EnemyType& outEnemyType) const
{
	if (enemyTypeName == "GuardianBoss")
	{
		return false;
	}

	outEnemyType = ParseEnemyType(enemyTypeName);
	return true;
}
