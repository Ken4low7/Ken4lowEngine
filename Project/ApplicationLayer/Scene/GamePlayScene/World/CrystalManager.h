#pragma once

#include "EnemySpawnCrystal.h"
#include "CrystalParameterController.h"
#include "CrystalHpBarController.h"

#include <cstddef>
#include <string>
#include <vector>

#include "Matrix4x4.h"
#include "LightManager.h"
#include "Vector2.h"

class CharacterWorld;
class CollisionManager;
class Player;
namespace Ken4lowEngine { class SkyBox; }

/// -------------------------------------------------------------
/// 複数クリスタルの進行、雑魚敵生成要求、ボス通知を集約する。
///
/// クリスタル自体を敵スポナーとして扱い、ParameterManager経由で
/// Transform / HP / 湧き設定をJson保存・ImGui編集できるようにする。
/// -------------------------------------------------------------
class CrystalManager
{
public:
	~CrystalManager();
	void Initialize(const std::vector<CrystalSpawnPoint>& spawnPoints, CollisionManager* collisionManager = nullptr, const std::vector<K4E::AABB>* floorAABBs = nullptr, const std::vector<K4E::AABB>* obstacleAABBs = nullptr);
	void Finalize();
	void Update(CharacterWorld& characters, float deltaTime);
	// ボス登場演出中など、敵スポーンを止めたまま破壊演出と世界色変化だけ進める。
	void UpdatePresentationOnly(CharacterWorld& characters, float deltaTime);
	void Draw() const;
	void UpdateHpBars(const K4E::Matrix4x4& viewMatrix, const K4E::Matrix4x4& projMatrix, float screenWidth, float screenHeight, float deltaTime, const EnemySpawnCrystal* aimedCrystal = nullptr, bool showOnlyWhenAimed = true, float visibleHoldTime = 0.3f);
	void DrawHpBars();
	void DrawImGui();
	void SetSkyBox(K4E::SkyBox* skyBox) { skyBox_ = skyBox; }
	void SetStage1BeginnerBalanceEnabled(bool enabled);
	void SetDifficultyDirectorEnabled(bool enabled) { difficultyDirectorEnabled_ = enabled; }
	void SetFirstAliveCrystalGuideHighlight(float alpha);

	int GetCrystalCount() const { return static_cast<int>(crystals_.size()); }
	int GetAliveCrystalCount() const;
	int GetDestroyedCrystalCount() const { return GetCrystalCount() - GetAliveCrystalCount(); }
	const EnemySpawnCrystal* GetFirstAliveCrystal() const;
	bool TryGetFirstAliveCrystalPosition(K4E::Vector3& outPosition) const;
	int GetAliveCrystalSpawnEnemyCount() const;
	bool AreAllCrystalsDestroyed() const;
	bool IsCrystalEnemySpawnEnabled() const { return enableCrystalEnemySpawn_; }
	bool HasCrystalBroken() const { return hasCrystalBroken_; }
	bool IsFinalPhaseReady() const { return isFinalPhaseReady_; }
	bool IsBossAppearRequested() const { return requestBossAppear_; }
	bool IsWorldColorChangeComplete() const { return worldColorChangeComplete_; }
	void SetCrystalEnemySpawnEnabled(bool enabled) { enableCrystalEnemySpawn_ = enabled; }
	void SetProgressDebugStatus(int aliveNormalEnemyCount, bool bossSpawnConditionMet, bool bossSpawned, const K4E::Vector3& bossSpawnPosition);
	static constexpr float GetMaxUpdateDeltaTime() { return kMaxUpdateDeltaTime; }

private:
	EnemySpawnCrystal* GetSelectedCrystal();
	const EnemySpawnCrystal* GetSelectedCrystal() const;
	EnemySpawnCrystal* FindNextSpawnableCrystal();
	void ApplyStage1BeginnerBalance(CrystalSpawnPoint& spawnPoint);
	void UpdateDifficultyDirector(CharacterWorld& characters, float deltaTime);
	void ApplyDifficultyDirectorToCrystals();
	const char* GetPressureLevelName() const;
	void SyncCrystalsFromParameterManager();
	void SyncCrystalFromSpawnPoint(size_t index);
	CrystalParameterController::ReactionBinding BuildReactionParameterBinding();
	CrystalParameterController::HpBarBinding BuildHpBarParameterBinding();
	CrystalParameterController::SkyColorBinding BuildSkyColorParameterBinding();
	void HandleCrystalBreakEvents();
	void BeginWorldColorChange();
	void UpdateWorldColorChange(float deltaTime);
	void BeginSkyColorChange();
	void UpdateSkyColorChange(float deltaTime);
	void ApplySkyColor(const K4E::Vector4& color);
	K4E::Vector4 LerpColor(const K4E::Vector4& a, const K4E::Vector4& b, float t) const;
	void RestoreWorldColor();

private:
	static constexpr float kMaxUpdateDeltaTime = 1.0f / 30.0f;
	enum class PressureLevel
	{
		EasyPressure,
		NormalPressure,
		HighPressure,
		PanicPressure,
	};
	struct DifficultyDirectorSettings
	{
		float lowHpRate = 0.35f;
		float comfortableHpRate = 0.75f;
		float noDamageComfortTime = 10.0f;
		float noDamagePanicTime = 18.0f;
		float fastKillWindow = 8.0f;
		int easyMaxAlivePerCrystal = 1;
		int normalMaxAlivePerCrystal = 2;
		int highMaxAlivePerCrystal = 3;
		int panicMaxAlivePerCrystal = 4;
		int easyMaxAliveEnemiesTotal = 7;
		int normalMaxAliveEnemiesTotal = 10;
		int highMaxAliveEnemiesTotal = 12;
		int panicMaxAliveEnemiesTotal = 14;
		float easySpawnIntervalMultiplier = 1.35f;
		float normalSpawnIntervalMultiplier = 1.0f;
		float highSpawnIntervalMultiplier = 0.82f;
		float panicSpawnIntervalMultiplier = 0.68f;
		float easyEnemyMoveSpeedMultiplier = 0.95f;
		float normalEnemyMoveSpeedMultiplier = 1.0f;
		float highEnemyMoveSpeedMultiplier = 1.08f;
		float panicEnemyMoveSpeedMultiplier = 1.15f;
		float easyAttackCooldownMultiplier = 1.10f;
		float normalAttackCooldownMultiplier = 1.0f;
		float highAttackCooldownMultiplier = 0.94f;
		float panicAttackCooldownMultiplier = 0.88f;
		float easyDamageMultiplier = 0.90f;
		float normalDamageMultiplier = 1.0f;
		float highDamageMultiplier = 1.05f;
		float panicDamageMultiplier = 1.10f;
	};
	struct DifficultyDirectorRuntime
	{
		PressureLevel level = PressureLevel::NormalPressure;
		float pressureScore = 0.0f;
		float playerHpRate = 1.0f;
		float noDamageTimer = 0.0f;
		float fastKillTimer = 0.0f;
		int recentKillCount = 0;
		int previousAliveEnemyCount = 0;
		int maxAliveEnemiesPerCrystal = 2;
		int maxAliveEnemiesTotal = 10;
		float spawnIntervalMultiplier = 1.0f;
		float enemyMoveSpeedMultiplier = 1.0f;
		float attackCooldownMultiplier = 1.0f;
		float damageMultiplier = 1.0f;
	};
	std::vector<CrystalSpawnPoint> spawnPoints_;
	CrystalParameterController crystalParameterController_;
	CrystalReactionSettings reactionSettings_{};
	std::vector<EnemySpawnCrystal> crystals_;
	CollisionManager* collisionManager_ = nullptr;
	const std::vector<K4E::AABB>* floorAABBs_ = nullptr;
	const std::vector<K4E::AABB>* obstacleAABBs_ = nullptr;
	size_t selectedCrystalIndex_ = 0;
	size_t nextSpawnCrystalIndex_ = 0;
	bool enableCrystalEnemySpawn_ = true;
	int maxSpawnPerInterval_ = 1;
	bool hasCrystalBroken_ = false;
	bool isFinalPhaseReady_ = false;
	bool requestBossAppear_ = false;
	bool worldColorChanging_ = false;
	bool worldColorChangeComplete_ = false;
	float worldColorChangeTimer_ = 0.0f;
	float worldColorChangeTime_ = 3.0f;
	float worldDarkness_ = 0.45f;
	float worldRedTint_ = 0.35f;
	CrystalHpBarController crystalHpBarController_;
	K4E::SkyBox* skyBox_ = nullptr;
	bool skyColorChangeEnabled_ = true;
	bool changeSkyOnAllCrystalsBroken_ = true;
	bool skyColorChanging_ = false;
	bool skyColorChangeComplete_ = false;
	float skyColorChangeTime_ = 3.0f;
	float skyColorChangeTimer_ = 0.0f;
	float skyDarkness_ = 0.35f;
	float skyRedTint_ = 0.25f;
	float skyPurpleTint_ = 0.25f;
	K4E::Vector4 normalSkyColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
	K4E::Vector4 brokenSkyColor_{ 0.55f, 0.18f, 0.28f, 1.0f };
	K4E::LightManager::LightingSettingsGPU baseLightingSettings_{};
	bool stage1BeginnerBalanceEnabled_ = false;
	bool difficultyDirectorEnabled_ = true;
	DifficultyDirectorSettings difficultySettings_{};
	DifficultyDirectorRuntime difficultyRuntime_{};
	int debugAliveNormalEnemyCount_ = 0;
	bool debugBossSpawnConditionMet_ = false;
	bool debugBossSpawned_ = false;
	K4E::Vector3 debugBossSpawnPosition_{};
};
