#pragma once

#include "EnemyType.h"
#include "Vector3.h"

#include <cstdint>
#include <string>
#include <vector>

class CharacterWorld;

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// ParameterManagerで保存・編集する敵スポナー1件分の設定。
///
/// Jsonの独自読み書きは行わず、EnemySpawnerManagerがこの値を
/// ParameterManagerの各グループへ登録して保存対象にする。
/// -------------------------------------------------------------
struct EnemySpawnerData
{
	std::string spawnerName = "EnemySpawner_01";
	bool isActive = false;
	K4E::Vector3 position{ 0.0f, 0.0f, 0.0f };
	K4E::Vector3 rotation{ 0.0f, 0.0f, 0.0f };
	std::string enemyType = "Melee";
	float spawnInterval = 3.0f;
	float initialDelay = 0.0f;
	int32_t maxSpawnCount = 1;
	float spawnRadius = 0.0f;
	std::string spawnPattern = "Interval";
};

/// -------------------------------------------------------------
/// 敵スポナーをParameterManagerへ登録し、実行時にCharacterWorldへ
/// 既存の敵生成リクエストを渡す管理クラス。
///
/// GamePlayWorldからInitialize / Update / DrawDebug / DrawImGuiContent
/// を呼ぶだけにし、GamePlaySceneへスポーン処理を広げない。
/// -------------------------------------------------------------
class EnemySpawnerManager
{
private:
	struct RuntimeSpawner
	{
		EnemySpawnerData data{};
		std::string groupName;
		float timer = 0.0f;
		int32_t spawnedCount = 0;
		bool initialDelayElapsed = false;
		bool unsupportedEnemyTypeReported = false;
	};

public:
	/// ParameterManagerへスポナー項目を登録し、保存済みJsonがあれば読み込む。
	void Initialize();

	/// ParameterManagerの反映コールバックを解除し、スポナー状態を破棄する。
	void Finalize();

	/// 有効なスポナーだけを進め、生成条件を満たしたらCharacterWorldへ接続する。
	void Update(CharacterWorld& characters, float deltaTime);

	/// 既存Wireframeが使えるDebugビルドのみ、スポナー位置と湧き範囲を表示する。
	void DrawDebug() const;

	/// Game Debugなどにスポナーの実行状態だけを表示する補助UI。
	void DrawImGuiContent() const;

	/// 登録済みParameterManagerグループ名一覧を取得する。
	std::vector<std::string> GetGroupNames() const;

private:
	void BuildDefaultSpawners();
	void RegisterSpawnerParameters(RuntimeSpawner& spawner);
	void UnregisterSpawnerParameters(RuntimeSpawner& spawner);
	void ApplyParameters(RuntimeSpawner& spawner);
	void SyncAllFromParameterManager();

	bool ShouldSpawn(RuntimeSpawner& spawner, float deltaTime) const;
	void SpawnByPattern(RuntimeSpawner& spawner, CharacterWorld& characters);
	bool SpawnOne(RuntimeSpawner& spawner, CharacterWorld& characters);

	K4E::Vector3 BuildSpawnPosition(const RuntimeSpawner& spawner) const;
	bool TryResolveEnemyType(const std::string& enemyTypeName, EnemyType& outEnemyType) const;

private:
	std::vector<RuntimeSpawner> spawners_;
};
