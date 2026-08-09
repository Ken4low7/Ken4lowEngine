#define NOMINMAX
#include "CrystalManager.h"

#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "CharacterWorld.h"
#include "CameraManager.h"
#include "CollisionManager.h"
#include "EnemyBase.h"
#include "SkyBox.h"

#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	float Clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

	float SaturateInverseLerp(float minValue, float maxValue, float value)
	{
		if (std::fabs(maxValue - minValue) <= 0.0001f) return value >= maxValue ? 1.0f : 0.0f;
		return Clamp01((value - minValue) / (maxValue - minValue));
	}
}

CrystalManager::~CrystalManager() { Finalize(); }

void CrystalManager::Initialize(
	const std::vector<CrystalSpawnPoint>& spawnPoints,
	CollisionManager* collisionManager,
	const std::vector<Ken4lowEngine::AABB>* floorAABBs,
	const std::vector<Ken4lowEngine::AABB>* obstacleAABBs)
{
	Finalize();

	baseLightingSettings_ = Ken4lowEngine::LightManager::GetInstance()->GetLightingSettings();
	crystalParameterController_.RegisterReactionParameters(BuildReactionParameterBinding());
	crystalParameterController_.ApplyReactionParameters(BuildReactionParameterBinding());
	crystalParameterController_.RegisterHpBarParameters(BuildHpBarParameterBinding());
	crystalParameterController_.ApplyHpBarParameters(BuildHpBarParameterBinding());
	crystalParameterController_.RegisterSkyColorParameters(BuildSkyColorParameterBinding());
	crystalParameterController_.ApplySkyColorParameters(BuildSkyColorParameterBinding());
	ApplySkyColor(normalSkyColor_);

	collisionManager_ = collisionManager;
	floorAABBs_ = floorAABBs;
	obstacleAABBs_ = obstacleAABBs;
	spawnPoints_ = spawnPoints;
	for (CrystalSpawnPoint& spawnPoint : spawnPoints_)
	{
		crystalParameterController_.RegisterCrystalParameters(spawnPoint, [this]() { SyncCrystalsFromParameterManager(); });
		crystalParameterController_.ApplyParameterToSpawnPoint(spawnPoint);
	}

	crystals_.reserve(spawnPoints_.size());
	for (const CrystalSpawnPoint& spawnPoint : spawnPoints_)
	{
		EnemySpawnCrystal crystal;
		crystal.Initialize(spawnPoint, floorAABBs_, obstacleAABBs_);
		crystals_.push_back(std::move(crystal));
	}
	crystalHpBarController_.Initialize(crystals_.size());

	if (collisionManager_)
	{
		for (EnemySpawnCrystal& crystal : crystals_) collisionManager_->AddCollider(&crystal);
	}

	selectedCrystalIndex_ = 0;
	nextSpawnCrystalIndex_ = 0;
	enableCrystalEnemySpawn_ = true;
	maxSpawnPerInterval_ = 1;
	difficultyRuntime_ = {};
	difficultyRuntime_.maxAliveEnemiesPerCrystal = difficultySettings_.normalMaxAlivePerCrystal;
	difficultyRuntime_.maxAliveEnemiesTotal = difficultySettings_.normalMaxAliveEnemiesTotal;
	hasCrystalBroken_ = false;
	isFinalPhaseReady_ = false;
	requestBossAppear_ = false;
	worldColorChanging_ = false;
	worldColorChangeComplete_ = false;
	worldColorChangeTimer_ = 0.0f;
	debugAliveNormalEnemyCount_ = 0;
	debugBossSpawnConditionMet_ = false;
	debugBossSpawned_ = false;
	debugBossSpawnPosition_ = {};
}

void CrystalManager::Finalize()
{
	if (collisionManager_)
	{
		for (EnemySpawnCrystal& crystal : crystals_) collisionManager_->RemoveCollider(&crystal);
	}
	crystalParameterController_.UnregisterCrystalParameters();
	crystalParameterController_.UnregisterReactionParameters();
	crystalParameterController_.UnregisterHpBarParameters();
	crystalParameterController_.UnregisterSkyColorParameters();
	RestoreWorldColor();
	crystals_.clear();
	crystalHpBarController_.Finalize();
	spawnPoints_.clear();
	collisionManager_ = nullptr;
	floorAABBs_ = nullptr;
	obstacleAABBs_ = nullptr;
}

void CrystalManager::Update(CharacterWorld& characters, float deltaTime)
{
	SyncCrystalsFromParameterManager();
	crystalParameterController_.ApplyReactionParameters(BuildReactionParameterBinding());
	crystalParameterController_.ApplyHpBarParameters(BuildHpBarParameterBinding());
	crystalParameterController_.ApplySkyColorParameters(BuildSkyColorParameterBinding());

	for (EnemySpawnCrystal& crystal : crystals_) crystal.Update(characters, deltaTime, reactionSettings_);
	UpdateDifficultyDirector(characters, deltaTime);
	ApplyDifficultyDirectorToCrystals();
	HandleCrystalBreakEvents();
	UpdateWorldColorChange(deltaTime);
	UpdateSkyColorChange(deltaTime);

	if (!enableCrystalEnemySpawn_ || AreAllCrystalsDestroyed() || GetAliveCrystalCount() <= 0) return;

	const float safeDeltaTime = std::clamp(deltaTime, 0.0f, kMaxUpdateDeltaTime);
	for (EnemySpawnCrystal& crystal : crystals_) crystal.AdvanceSpawnTimer(safeDeltaTime);

	for (int spawnedCount = 0; spawnedCount < maxSpawnPerInterval_; ++spawnedCount)
	{
		if (characters.GetAliveNormalEnemyCount() >= difficultyRuntime_.maxAliveEnemiesTotal) break;
		EnemySpawnCrystal* crystal = FindNextSpawnableCrystal();
		if (!crystal) break;

		if (crystal->GetSpawnPattern() == "Burst")
		{
			while (crystal->CanSpawnEnemy() && characters.GetAliveNormalEnemyCount() < difficultyRuntime_.maxAliveEnemiesTotal)
			{
				crystal->SpawnEnemy(characters, difficultyRuntime_.enemyMoveSpeedMultiplier, difficultyRuntime_.attackCooldownMultiplier, difficultyRuntime_.damageMultiplier);
			}
		}
		else
		{
			crystal->SpawnEnemy(characters, difficultyRuntime_.enemyMoveSpeedMultiplier, difficultyRuntime_.attackCooldownMultiplier, difficultyRuntime_.damageMultiplier);
		}
		crystal->ConsumeSpawnTimer();
	}
}

void CrystalManager::UpdatePresentationOnly(CharacterWorld& characters, float deltaTime)
{
	SyncCrystalsFromParameterManager();
	crystalParameterController_.ApplyReactionParameters(BuildReactionParameterBinding());
	for (EnemySpawnCrystal& crystal : crystals_) crystal.Update(characters, deltaTime, reactionSettings_);
	HandleCrystalBreakEvents();
	UpdateWorldColorChange(deltaTime);
	UpdateSkyColorChange(deltaTime);
}

void CrystalManager::Draw() const
{
	for (const EnemySpawnCrystal& crystal : crystals_) crystal.Draw();
}

void CrystalManager::UpdateHpBars(
	const Ken4lowEngine::Matrix4x4& viewMatrix,
	const Ken4lowEngine::Matrix4x4& projMatrix,
	float screenWidth,
	float screenHeight,
	float deltaTime,
	const EnemySpawnCrystal* aimedCrystal,
	bool showOnlyWhenAimed,
	float visibleHoldTime)
{
	crystalHpBarController_.Update(crystals_, viewMatrix, projMatrix, screenWidth, screenHeight, deltaTime, aimedCrystal, showOnlyWhenAimed, visibleHoldTime);
}

void CrystalManager::DrawHpBars() { crystalHpBarController_.Draw(); }

void CrystalManager::SetStage1BeginnerBalanceEnabled(bool enabled)
{
	stage1BeginnerBalanceEnabled_ = enabled;
	if (!stage1BeginnerBalanceEnabled_)
	{
		if (collisionManager_)
		{
			for (EnemySpawnCrystal& crystal : crystals_) collisionManager_->RemoveCollider(&crystal);
		}
		crystalParameterController_.UnregisterCrystalParameters();
		crystals_.clear();
		spawnPoints_.clear();
		crystalHpBarController_.Finalize();
		enableCrystalEnemySpawn_ = false;
		selectedCrystalIndex_ = 0;
		nextSpawnCrystalIndex_ = 0;
		hasCrystalBroken_ = false;
		isFinalPhaseReady_ = false;
		requestBossAppear_ = false; // Stage2以降ではStage1専用の見た目・Collider・敵生成・ボス要求を残さない。
		RestoreWorldColor();
		return;
	}
	for (size_t i = 0; i < spawnPoints_.size() && i < crystals_.size(); ++i)
	{
		ApplyStage1BeginnerBalance(spawnPoints_[i]);
		crystals_[i].ApplyInitialHpSettings(spawnPoints_[i]);
		SyncCrystalFromSpawnPoint(i);
	}
}

void CrystalManager::SetFirstAliveCrystalGuideHighlight(float alpha)
{
	const float highlightAlpha = std::clamp(alpha, 0.0f, 1.0f);
	bool applied = false;
	for (EnemySpawnCrystal& crystal : crystals_)
	{
		const bool shouldHighlight = !applied && crystal.IsAlive();
		crystal.SetGuideHighlight(shouldHighlight ? highlightAlpha : 0.0f);
		if (shouldHighlight) applied = true;
	}
}

int CrystalManager::GetAliveCrystalCount() const
{
	return static_cast<int>(std::count_if(crystals_.begin(), crystals_.end(), [](const EnemySpawnCrystal& crystal) { return crystal.IsAlive(); }));
}

const EnemySpawnCrystal* CrystalManager::GetFirstAliveCrystal() const
{
	for (const EnemySpawnCrystal& crystal : crystals_) if (crystal.IsAlive()) return &crystal;
	return nullptr;
}

bool CrystalManager::TryGetFirstAliveCrystalPosition(K4E::Vector3& outPosition) const
{
	if (const EnemySpawnCrystal* crystal = GetFirstAliveCrystal())
	{
		outPosition = crystal->GetPosition();
		return true;
	}
	return false;
}

int CrystalManager::GetAliveCrystalSpawnEnemyCount() const
{
	int totalCount = 0;
	for (const EnemySpawnCrystal& crystal : crystals_) totalCount += crystal.GetAliveSpawnedEnemyCount();
	return totalCount;
}

bool CrystalManager::AreAllCrystalsDestroyed() const { return !crystals_.empty() && GetAliveCrystalCount() == 0; }

void CrystalManager::SetProgressDebugStatus(int aliveNormalEnemyCount, bool bossSpawnConditionMet, bool bossSpawned, const Ken4lowEngine::Vector3& bossSpawnPosition)
{
	debugAliveNormalEnemyCount_ = aliveNormalEnemyCount;
	debugBossSpawnConditionMet_ = bossSpawnConditionMet;
	debugBossSpawned_ = bossSpawned;
	debugBossSpawnPosition_ = bossSpawnPosition;
}

EnemySpawnCrystal* CrystalManager::GetSelectedCrystal()
{
	return selectedCrystalIndex_ < crystals_.size() ? &crystals_[selectedCrystalIndex_] : nullptr;
}

const EnemySpawnCrystal* CrystalManager::GetSelectedCrystal() const
{
	return selectedCrystalIndex_ < crystals_.size() ? &crystals_[selectedCrystalIndex_] : nullptr;
}

EnemySpawnCrystal* CrystalManager::FindNextSpawnableCrystal()
{
	for (size_t offset = 0; offset < crystals_.size(); ++offset)
	{
		const size_t crystalIndex = (nextSpawnCrystalIndex_ + offset) % crystals_.size();
		if (crystals_[crystalIndex].IsSpawnReady())
		{
			nextSpawnCrystalIndex_ = (crystalIndex + 1) % crystals_.size();
			return &crystals_[crystalIndex];
		}
	}
	return nullptr;
}

void CrystalManager::ApplyStage1BeginnerBalance(CrystalSpawnPoint& spawnPoint)
{
	spawnPoint.spawnEnemyType = EnemyType::Melee;
	spawnPoint.hp = 350;
	spawnPoint.maxHp = 350;
	spawnPoint.spawnInterval = 7.0f;
	spawnPoint.initialDelay = 7.0f;
	spawnPoint.maxSpawnCount = 0;
	spawnPoint.maxAliveEnemies = difficultyRuntime_.maxAliveEnemiesPerCrystal;
	spawnPoint.spawnRadius = 4.0f;
	spawnPoint.spawnPattern = "Interval";
	spawnPoint.enableInfiniteSpawn = spawnPoint.isActive;
}

void CrystalManager::UpdateDifficultyDirector(CharacterWorld& characters, float deltaTime)
{
	if (!stage1BeginnerBalanceEnabled_ || !difficultyDirectorEnabled_)
	{
		difficultyRuntime_.level = PressureLevel::NormalPressure;
		difficultyRuntime_.pressureScore = 0.0f;
		difficultyRuntime_.maxAliveEnemiesPerCrystal = difficultySettings_.normalMaxAlivePerCrystal;
		difficultyRuntime_.maxAliveEnemiesTotal = difficultySettings_.normalMaxAliveEnemiesTotal;
		difficultyRuntime_.spawnIntervalMultiplier = difficultySettings_.normalSpawnIntervalMultiplier;
		difficultyRuntime_.enemyMoveSpeedMultiplier = difficultySettings_.normalEnemyMoveSpeedMultiplier;
		difficultyRuntime_.attackCooldownMultiplier = difficultySettings_.normalAttackCooldownMultiplier;
		difficultyRuntime_.damageMultiplier = difficultySettings_.normalDamageMultiplier;
		return;
	}

	IPlayerRuntime* player = characters.GetPlayerRuntime();
	const float previousHpRate = difficultyRuntime_.playerHpRate;
	difficultyRuntime_.playerHpRate = player && player->GetMaxHP() > 0.0f
		? std::clamp(player->GetHP() / player->GetMaxHP(), 0.0f, 1.0f)
		: 1.0f; // P13ではDifficulty Directorも旧Player型ではなくRuntime HPを正本にする。

	if (difficultyRuntime_.playerHpRate < previousHpRate - 0.001f) difficultyRuntime_.noDamageTimer = 0.0f;
	else difficultyRuntime_.noDamageTimer += deltaTime;

	const int aliveEnemies = characters.GetAliveNormalEnemyCount();
	if (aliveEnemies < difficultyRuntime_.previousAliveEnemyCount)
	{
		difficultyRuntime_.recentKillCount += difficultyRuntime_.previousAliveEnemyCount - aliveEnemies;
		difficultyRuntime_.fastKillTimer = 0.0f;
	}
	else
	{
		difficultyRuntime_.fastKillTimer += deltaTime;
		if (difficultyRuntime_.fastKillTimer > difficultySettings_.fastKillWindow)
		{
			difficultyRuntime_.recentKillCount = 0;
			difficultyRuntime_.fastKillTimer = 0.0f;
		}
	}
	difficultyRuntime_.previousAliveEnemyCount = aliveEnemies;

	const float hpComfort = SaturateInverseLerp(difficultySettings_.lowHpRate, difficultySettings_.comfortableHpRate, difficultyRuntime_.playerHpRate);
	const float noDamageComfort = SaturateInverseLerp(0.0f, difficultySettings_.noDamageComfortTime, difficultyRuntime_.noDamageTimer);
	const float killComfort = Clamp01(static_cast<float>(difficultyRuntime_.recentKillCount) / 3.0f);
	const float lowEnemyBonus = 1.0f - Clamp01(static_cast<float>(aliveEnemies) / static_cast<float>(std::max(1, difficultyRuntime_.maxAliveEnemiesTotal)));
	const float crystalPressure = Clamp01(static_cast<float>(GetAliveCrystalCount()) / static_cast<float>(std::max(1, GetCrystalCount())));

	float pressureScore = hpComfort * 0.35f + noDamageComfort * 0.25f + killComfort * 0.20f + lowEnemyBonus * 0.15f + crystalPressure * 0.05f;
	if (difficultyRuntime_.playerHpRate <= difficultySettings_.lowHpRate) pressureScore -= 0.35f;
	difficultyRuntime_.pressureScore = std::clamp(pressureScore, 0.0f, 1.0f);

	if (difficultyRuntime_.playerHpRate <= difficultySettings_.lowHpRate) difficultyRuntime_.level = PressureLevel::EasyPressure;
	else if (difficultyRuntime_.pressureScore >= 0.82f && difficultyRuntime_.noDamageTimer >= difficultySettings_.noDamagePanicTime) difficultyRuntime_.level = PressureLevel::PanicPressure;
	else if (difficultyRuntime_.pressureScore >= 0.58f) difficultyRuntime_.level = PressureLevel::HighPressure;
	else difficultyRuntime_.level = PressureLevel::NormalPressure;

	switch (difficultyRuntime_.level)
	{
	case PressureLevel::EasyPressure:
		difficultyRuntime_.maxAliveEnemiesPerCrystal = difficultySettings_.easyMaxAlivePerCrystal;
		difficultyRuntime_.maxAliveEnemiesTotal = difficultySettings_.easyMaxAliveEnemiesTotal;
		difficultyRuntime_.spawnIntervalMultiplier = difficultySettings_.easySpawnIntervalMultiplier;
		difficultyRuntime_.enemyMoveSpeedMultiplier = difficultySettings_.easyEnemyMoveSpeedMultiplier;
		difficultyRuntime_.attackCooldownMultiplier = difficultySettings_.easyAttackCooldownMultiplier;
		difficultyRuntime_.damageMultiplier = difficultySettings_.easyDamageMultiplier;
		break;
	case PressureLevel::HighPressure:
		difficultyRuntime_.maxAliveEnemiesPerCrystal = difficultySettings_.highMaxAlivePerCrystal;
		difficultyRuntime_.maxAliveEnemiesTotal = difficultySettings_.highMaxAliveEnemiesTotal;
		difficultyRuntime_.spawnIntervalMultiplier = difficultySettings_.highSpawnIntervalMultiplier;
		difficultyRuntime_.enemyMoveSpeedMultiplier = difficultySettings_.highEnemyMoveSpeedMultiplier;
		difficultyRuntime_.attackCooldownMultiplier = difficultySettings_.highAttackCooldownMultiplier;
		difficultyRuntime_.damageMultiplier = difficultySettings_.highDamageMultiplier;
		break;
	case PressureLevel::PanicPressure:
		difficultyRuntime_.maxAliveEnemiesPerCrystal = difficultySettings_.panicMaxAlivePerCrystal;
		difficultyRuntime_.maxAliveEnemiesTotal = difficultySettings_.panicMaxAliveEnemiesTotal;
		difficultyRuntime_.spawnIntervalMultiplier = difficultySettings_.panicSpawnIntervalMultiplier;
		difficultyRuntime_.enemyMoveSpeedMultiplier = difficultySettings_.panicEnemyMoveSpeedMultiplier;
		difficultyRuntime_.attackCooldownMultiplier = difficultySettings_.panicAttackCooldownMultiplier;
		difficultyRuntime_.damageMultiplier = difficultySettings_.panicDamageMultiplier;
		break;
	case PressureLevel::NormalPressure:
	default:
		difficultyRuntime_.maxAliveEnemiesPerCrystal = difficultySettings_.normalMaxAlivePerCrystal;
		difficultyRuntime_.maxAliveEnemiesTotal = difficultySettings_.normalMaxAliveEnemiesTotal;
		difficultyRuntime_.spawnIntervalMultiplier = difficultySettings_.normalSpawnIntervalMultiplier;
		difficultyRuntime_.enemyMoveSpeedMultiplier = difficultySettings_.normalEnemyMoveSpeedMultiplier;
		difficultyRuntime_.attackCooldownMultiplier = difficultySettings_.normalAttackCooldownMultiplier;
		difficultyRuntime_.damageMultiplier = difficultySettings_.normalDamageMultiplier;
		break;
	}
}

void CrystalManager::ApplyDifficultyDirectorToCrystals()
{
	if (!stage1BeginnerBalanceEnabled_) return;
	for (size_t i = 0; i < crystals_.size() && i < spawnPoints_.size(); ++i)
	{
		crystals_[i].SetMaxAliveEnemies(difficultyRuntime_.maxAliveEnemiesPerCrystal);
		crystals_[i].SetSpawnInterval(spawnPoints_[i].spawnInterval * difficultyRuntime_.spawnIntervalMultiplier);
	}
}

const char* CrystalManager::GetPressureLevelName() const
{
	switch (difficultyRuntime_.level)
	{
	case PressureLevel::EasyPressure: return "EasyPressure";
	case PressureLevel::HighPressure: return "HighPressure";
	case PressureLevel::PanicPressure: return "PanicPressure";
	case PressureLevel::NormalPressure:
	default: return "NormalPressure";
	}
}

void CrystalManager::SyncCrystalsFromParameterManager()
{
	for (size_t i = 0; i < spawnPoints_.size() && i < crystals_.size(); ++i)
	{
		crystalParameterController_.ApplyParameterToSpawnPoint(spawnPoints_[i]);
		if (stage1BeginnerBalanceEnabled_) ApplyStage1BeginnerBalance(spawnPoints_[i]);
		SyncCrystalFromSpawnPoint(i);
	}
}

void CrystalManager::SyncCrystalFromSpawnPoint(size_t index)
{
	if (index >= spawnPoints_.size() || index >= crystals_.size()) return;
	crystals_[index].ApplySpawnerSettings(spawnPoints_[index], floorAABBs_, obstacleAABBs_);
}

CrystalParameterController::ReactionBinding CrystalManager::BuildReactionParameterBinding()
{
	return { &reactionSettings_, &worldColorChangeTime_, &worldDarkness_, &worldRedTint_ };
}

CrystalParameterController::HpBarBinding CrystalManager::BuildHpBarParameterBinding()
{
	CrystalHpBarController::Settings& settings = crystalHpBarController_.GetSettings();
	return { &settings.visible, &settings.alwaysVisible, &settings.offsetY, &settings.width, &settings.height, &settings.showTime };
}

CrystalParameterController::SkyColorBinding CrystalManager::BuildSkyColorParameterBinding()
{
	return { &skyColorChangeEnabled_, &changeSkyOnAllCrystalsBroken_, &skyColorChangeTime_, &normalSkyColor_, &brokenSkyColor_, &skyDarkness_, &skyRedTint_, &skyPurpleTint_ };
}

void CrystalManager::HandleCrystalBreakEvents()
{
	for (EnemySpawnCrystal& crystal : crystals_)
	{
		if (!crystal.WasJustBroken()) continue;
		hasCrystalBroken_ = true;
		requestBossAppear_ = AreAllCrystalsDestroyed();
		isFinalPhaseReady_ = requestBossAppear_;
		BeginWorldColorChange();
		if (requestBossAppear_ && changeSkyOnAllCrystalsBroken_) BeginSkyColorChange();
		crystal.ClearJustBrokenFlag();
	}
}

void CrystalManager::BeginWorldColorChange()
{
	if (worldColorChanging_ || worldColorChangeComplete_) return;
	worldColorChanging_ = true;
	worldColorChangeComplete_ = false;
	worldColorChangeTimer_ = 0.0f;
	baseLightingSettings_ = Ken4lowEngine::LightManager::GetInstance()->GetLightingSettings();
}

void CrystalManager::UpdateWorldColorChange(float deltaTime)
{
	if (!worldColorChanging_) return;
	worldColorChangeTimer_ += deltaTime;
	const float t = std::clamp(worldColorChangeTimer_ / std::max(0.1f, worldColorChangeTime_), 0.0f, 1.0f);
	auto& lighting = Ken4lowEngine::LightManager::GetInstance()->GetMutableLightingSettingsForEditor();
	const float dark = std::clamp(worldDarkness_, 0.0f, 1.0f) * t;
	const float red = std::clamp(worldRedTint_, 0.0f, 1.0f) * t;
	lighting.ambientColor = {
		baseLightingSettings_.ambientColor.x * (1.0f - dark) + red,
		baseLightingSettings_.ambientColor.y * (1.0f - dark * 0.9f),
		baseLightingSettings_.ambientColor.z * (1.0f - dark),
		baseLightingSettings_.ambientColor.w
	};
	lighting.fogColor = {
		baseLightingSettings_.fogColor.x * (1.0f - dark) + red * 0.45f,
		baseLightingSettings_.fogColor.y * (1.0f - dark),
		baseLightingSettings_.fogColor.z * (1.0f - dark),
		baseLightingSettings_.fogColor.w
	};
	lighting.exposure = std::max(0.15f, baseLightingSettings_.exposure * (1.0f - dark * 0.55f));
	lighting.diffuseStrength = std::max(0.2f, baseLightingSettings_.diffuseStrength * (1.0f - dark * 0.35f));
	if (t >= 1.0f)
	{
		worldColorChanging_ = false;
		worldColorChangeComplete_ = true;
	}
}

void CrystalManager::BeginSkyColorChange()
{
	if (!skyColorChangeEnabled_ || skyColorChanging_ || skyColorChangeComplete_) return;
	skyColorChanging_ = true;
	skyColorChangeComplete_ = false;
	skyColorChangeTimer_ = 0.0f;
}

void CrystalManager::UpdateSkyColorChange(float deltaTime)
{
	if (!skyColorChanging_) return;
	skyColorChangeTimer_ += deltaTime;
	const float t = std::clamp(skyColorChangeTimer_ / std::max(0.1f, skyColorChangeTime_), 0.0f, 1.0f);
	K4E::Vector4 target = brokenSkyColor_;
	target.x = std::clamp(target.x + skyRedTint_, 0.0f, 1.0f);
	target.y = std::clamp(target.y * (1.0f - skyDarkness_), 0.0f, 1.0f);
	target.z = std::clamp(target.z + skyPurpleTint_, 0.0f, 1.0f);
	ApplySkyColor(LerpColor(normalSkyColor_, target, t));
	if (t >= 1.0f)
	{
		skyColorChanging_ = false;
		skyColorChangeComplete_ = true;
	}
}

void CrystalManager::ApplySkyColor(const K4E::Vector4& color)
{
	if (!skyBox_) return;
	skyBox_->SetColor(color);
	const K4E::Vector4 top{ color.x, color.y, color.z, color.w };
	const K4E::Vector4 horizon{ std::min(1.0f, color.x + 0.08f), std::min(1.0f, color.y + 0.04f), std::min(1.0f, color.z + 0.08f), color.w };
	const K4E::Vector4 bottom{ std::max(0.0f, color.x * 0.65f), std::max(0.0f, color.y * 0.55f), std::max(0.0f, color.z * 0.70f), color.w };
	skyBox_->SetGradientColors(top, bottom, horizon);
}

K4E::Vector4 CrystalManager::LerpColor(const K4E::Vector4& a, const K4E::Vector4& b, float t) const
{
	t = std::clamp(t, 0.0f, 1.0f);
	return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t };
}

void CrystalManager::RestoreWorldColor()
{
	if (!worldColorChanging_ && !worldColorChangeComplete_ && !skyColorChanging_ && !skyColorChangeComplete_) return;
	if (worldColorChanging_ || worldColorChangeComplete_) Ken4lowEngine::LightManager::GetInstance()->GetMutableLightingSettingsForEditor() = baseLightingSettings_;
	worldColorChanging_ = false;
	worldColorChangeComplete_ = false;
	worldColorChangeTimer_ = 0.0f;
	skyColorChanging_ = false;
	skyColorChangeComplete_ = false;
	skyColorChangeTimer_ = 0.0f;
	ApplySkyColor(normalSkyColor_);
}

void CrystalManager::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("クリスタル デバッグ")) return;
	ImGui::Checkbox("クリスタル敵スポーン有効", &enableCrystalEnemySpawn_);
	ImGui::Checkbox("Difficulty Director有効", &difficultyDirectorEnabled_);
	ImGui::Text("Pressure Level: %s", GetPressureLevelName());
	ImGui::Text("Player HP Rate: %.2f", difficultyRuntime_.playerHpRate);
	ImGui::Text("生存クリスタル: %d / %d", GetAliveCrystalCount(), GetCrystalCount());
	ImGui::Text("現在のクリスタル由来敵数: %d", GetAliveCrystalSpawnEnemyCount());
	ImGui::Text("現在敵数 / 全体上限: %d / %d", debugAliveNormalEnemyCount_, difficultyRuntime_.maxAliveEnemiesTotal);
	ImGui::Text("ボス出現条件: %s / 出現済み: %s", debugBossSpawnConditionMet_ ? "はい" : "いいえ", debugBossSpawned_ ? "はい" : "いいえ");
	ImGui::DragInt("1回の最大スポーン数", &maxSpawnPerInterval_, 1.0f, 1, 9);
	maxSpawnPerInterval_ = std::max(1, maxSpawnPerInterval_);

	if (crystals_.empty()) return;
	int selectedIndex = static_cast<int>(selectedCrystalIndex_);
	if (ImGui::SliderInt("選択中クリスタル", &selectedIndex, 0, static_cast<int>(crystals_.size()) - 1)) selectedCrystalIndex_ = static_cast<size_t>(selectedIndex);
	EnemySpawnCrystal* crystal = GetSelectedCrystal();
	if (!crystal) return;
	ImGui::Text("HP: %d / %d", crystal->GetHp(), crystal->GetMaxHp());
	ImGui::Text("Spawn Interval: %.2f / Alive: %d / Total: %d", crystal->GetSpawnInterval(), crystal->GetAliveSpawnedEnemyCount(), crystal->GetTotalSpawnedCount());
	if (ImGui::Button("選択中クリスタルに10ダメージ")) crystal->ApplyDamage(10);
	ImGui::SameLine();
	if (ImGui::Button("全クリスタルに10ダメージ"))
	{
		for (EnemySpawnCrystal& target : crystals_) target.ApplyDamage(10);
	}
	crystalHpBarController_.DrawImGui();
#else
	(void)this;
#endif
}
