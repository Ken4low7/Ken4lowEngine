#define NOMINMAX
#include "CrystalParameterController.h"

#include "ParameterManager.h"

#include <algorithm>
#include <cstdint>

namespace
{
	constexpr float kMinimumSpawnInterval = 0.05f;
	constexpr const char* kCrystalSpawnerRootGroup = "CrystalSpawner";
	constexpr const char* kCrystalReactionGroup = "CrystalEffect/CrystalReaction";
	constexpr const char* kCrystalHpBarGroup = "CrystalHpBar";
	constexpr const char* kSkyColorGroup = "SkyColorEffect";

	const std::vector<std::string>& CrystalEnemyTypeOptions()
	{
		static const std::vector<std::string> options = {
			"Melee",
			"MidRange"
		};
		return options;
	}

	const std::vector<std::string>& CrystalSpawnPatternOptions()
	{
		static const std::vector<std::string> options = {
			"Single",
			"Interval",
			"Burst"
		};
		return options;
	}
}

void CrystalParameterController::RegisterCrystalParameters(CrystalSpawnPoint& spawnPoint, const std::function<void()>& onApply)
{
	const std::string groupName = BuildCrystalGroupName(spawnPoint);
	parameterGroupNames_.push_back(groupName);

	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	parameters->CreateGroup(groupName);

	// クリスタル自体をスポナーとしてParameterManagerへ登録し、Json保存/読み込みに乗せる。
	parameters->AddItem(groupName, "isActive", spawnPoint.isActive);
	parameters->AddItem(groupName, "position", spawnPoint.position, Ken4lowEngine::Vector3{ -200.0f, -50.0f, -200.0f }, Ken4lowEngine::Vector3{ 200.0f, 80.0f, 200.0f });
	parameters->AddItem(groupName, "rotation", spawnPoint.rotation, Ken4lowEngine::Vector3{ -3.141592f, -3.141592f, -3.141592f }, Ken4lowEngine::Vector3{ 3.141592f, 3.141592f, 3.141592f });
	parameters->AddItem(groupName, "scale", spawnPoint.scale, Ken4lowEngine::Vector3{ 0.1f, 0.1f, 0.1f }, Ken4lowEngine::Vector3{ 10.0f, 10.0f, 10.0f });
	parameters->AddItem(groupName, "hp", spawnPoint.hp, 1, 10000);
	parameters->AddItem(groupName, "maxHp", spawnPoint.maxHp, 1, 10000);
	parameters->AddStringItem(groupName, "enemyType", ToEnemyTypeName(spawnPoint.spawnEnemyType), CrystalEnemyTypeOptions());
	parameters->AddItem(groupName, "spawnInterval", spawnPoint.spawnInterval, 0.05f, 60.0f);
	parameters->AddItem(groupName, "initialDelay", spawnPoint.initialDelay, 0.0f, 60.0f);
	parameters->AddItem(groupName, "maxSpawnCount", spawnPoint.maxSpawnCount, 0, 1000);
	parameters->AddItem(groupName, "maxAliveCount", spawnPoint.maxAliveEnemies, 0, 100);
	parameters->AddItem(groupName, "spawnRadius", spawnPoint.spawnRadius, 0.0f, 50.0f);
	parameters->AddStringItem(groupName, "spawnPattern", spawnPoint.spawnPattern, CrystalSpawnPatternOptions());

	parameters->SetDisplayName(groupName, "isActive", "有効");
	parameters->SetDisplayName(groupName, "position", "座標");
	parameters->SetDisplayName(groupName, "rotation", "回転");
	parameters->SetDisplayName(groupName, "scale", "スケール");
	parameters->SetDisplayName(groupName, "hp", "初期HP");
	parameters->SetDisplayName(groupName, "maxHp", "最大HP");
	parameters->SetDisplayName(groupName, "enemyType", "敵タイプ");
	parameters->SetDisplayName(groupName, "spawnInterval", "湧き間隔");
	parameters->SetDisplayName(groupName, "initialDelay", "初回湧き遅延");
	parameters->SetDisplayName(groupName, "maxSpawnCount", "最大湧き数");
	parameters->SetDisplayName(groupName, "maxAliveCount", "同時出現数");
	parameters->SetDisplayName(groupName, "spawnRadius", "湧き半径");
	parameters->SetDisplayName(groupName, "spawnPattern", "湧き方");

	parameters->RegisterParameterApplier(groupName, this, onApply);
	parameters->LoadFile(groupName);
}

void CrystalParameterController::UnregisterCrystalParameters()
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	for (const std::string& groupName : parameterGroupNames_)
	{
		parameters->UnregisterParameterApplier(groupName, this);
	}
	parameterGroupNames_.clear();
}

void CrystalParameterController::ApplyParameterToSpawnPoint(CrystalSpawnPoint& spawnPoint) const
{
	const std::string groupName = BuildCrystalGroupName(spawnPoint);
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();

	spawnPoint.isActive = parameters->GetValue<bool>(groupName, "isActive");
	spawnPoint.position = parameters->GetValue<Ken4lowEngine::Vector3>(groupName, "position");
	spawnPoint.rotation = parameters->GetValue<Ken4lowEngine::Vector3>(groupName, "rotation");
	spawnPoint.scale = parameters->GetValue<Ken4lowEngine::Vector3>(groupName, "scale");
	spawnPoint.hp = std::max(1, parameters->GetValue<int32_t>(groupName, "hp"));
	spawnPoint.maxHp = std::max(1, parameters->GetValue<int32_t>(groupName, "maxHp"));
	spawnPoint.spawnEnemyType = ParseCrystalEnemyType(parameters->GetValue<std::string>(groupName, "enemyType"));
	spawnPoint.spawnInterval = std::max(kMinimumSpawnInterval, parameters->GetValue<float>(groupName, "spawnInterval"));
	spawnPoint.initialDelay = std::max(0.0f, parameters->GetValue<float>(groupName, "initialDelay"));
	spawnPoint.maxSpawnCount = std::max(0, parameters->GetValue<int32_t>(groupName, "maxSpawnCount"));
	spawnPoint.maxAliveEnemies = std::max(0, parameters->GetValue<int32_t>(groupName, "maxAliveCount"));
	spawnPoint.spawnRadius = std::max(0.0f, parameters->GetValue<float>(groupName, "spawnRadius"));
	spawnPoint.spawnPattern = parameters->GetValue<std::string>(groupName, "spawnPattern");
	spawnPoint.enableInfiniteSpawn = spawnPoint.isActive;
}

void CrystalParameterController::RegisterReactionParameters(const ReactionBinding& binding)
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	parameters->CreateGroup(kCrystalReactionGroup);

	parameters->AddItem(kCrystalReactionGroup, "hitFlashTime", binding.reactionSettings->hitFlashTime, 0.0f, 2.0f);
	parameters->AddItem(kCrystalReactionGroup, "hitShakePower", binding.reactionSettings->hitShakePower, 0.0f, 2.0f);
	parameters->AddItem(kCrystalReactionGroup, "hitShakeTime", binding.reactionSettings->hitShakeTime, 0.0f, 2.0f);
	parameters->AddItem(kCrystalReactionGroup, "breakingDuration", binding.reactionSettings->breakingDuration, 0.05f, 5.0f);
	parameters->AddItem(kCrystalReactionGroup, "breakEffectScale", binding.reactionSettings->breakEffectScale, 1.0f, 4.0f);
	parameters->AddItem(kCrystalReactionGroup, "worldColorChangeTime", *binding.worldColorChangeTime, 0.1f, 10.0f);
	parameters->AddItem(kCrystalReactionGroup, "worldDarkness", *binding.worldDarkness, 0.0f, 1.0f);
	parameters->AddItem(kCrystalReactionGroup, "worldRedTint", *binding.worldRedTint, 0.0f, 1.0f);
	parameters->AddItem(kCrystalReactionGroup, "criticalHpRate", binding.reactionSettings->criticalHpRate, 0.0f, 1.0f);
	parameters->AddItem(kCrystalReactionGroup, "damagedHpRate", binding.reactionSettings->damagedHpRate, 0.0f, 1.0f);

	parameters->SetDisplayName(kCrystalReactionGroup, "hitFlashTime", "ヒット点滅時間");
	parameters->SetDisplayName(kCrystalReactionGroup, "hitShakePower", "ヒット揺れ強度");
	parameters->SetDisplayName(kCrystalReactionGroup, "hitShakeTime", "ヒット揺れ時間");
	parameters->SetDisplayName(kCrystalReactionGroup, "breakingDuration", "破壊演出時間");
	parameters->SetDisplayName(kCrystalReactionGroup, "breakEffectScale", "破壊拡大率");
	parameters->SetDisplayName(kCrystalReactionGroup, "worldColorChangeTime", "世界色変化時間");
	parameters->SetDisplayName(kCrystalReactionGroup, "worldDarkness", "世界暗さ");
	parameters->SetDisplayName(kCrystalReactionGroup, "worldRedTint", "世界赤み");
	parameters->SetDisplayName(kCrystalReactionGroup, "criticalHpRate", "瀕死HP割合");
	parameters->SetDisplayName(kCrystalReactionGroup, "damagedHpRate", "損傷HP割合");

	parameters->RegisterParameterApplier(kCrystalReactionGroup, this, [this, binding]() { ApplyReactionParameters(binding); });
	parameters->LoadFile(kCrystalReactionGroup);
}

void CrystalParameterController::UnregisterReactionParameters()
{
	Ken4lowEngine::ParameterManager::GetInstance()->UnregisterParameterApplier(kCrystalReactionGroup, this);
}

void CrystalParameterController::ApplyReactionParameters(const ReactionBinding& binding) const
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	binding.reactionSettings->hitFlashTime = parameters->GetValue<float>(kCrystalReactionGroup, "hitFlashTime");
	binding.reactionSettings->hitShakePower = parameters->GetValue<float>(kCrystalReactionGroup, "hitShakePower");
	binding.reactionSettings->hitShakeTime = parameters->GetValue<float>(kCrystalReactionGroup, "hitShakeTime");
	binding.reactionSettings->breakingDuration = parameters->GetValue<float>(kCrystalReactionGroup, "breakingDuration");
	binding.reactionSettings->breakEffectScale = parameters->GetValue<float>(kCrystalReactionGroup, "breakEffectScale");
	*binding.worldColorChangeTime = parameters->GetValue<float>(kCrystalReactionGroup, "worldColorChangeTime");
	*binding.worldDarkness = parameters->GetValue<float>(kCrystalReactionGroup, "worldDarkness");
	*binding.worldRedTint = parameters->GetValue<float>(kCrystalReactionGroup, "worldRedTint");
	binding.reactionSettings->criticalHpRate = parameters->GetValue<float>(kCrystalReactionGroup, "criticalHpRate");
	binding.reactionSettings->damagedHpRate = parameters->GetValue<float>(kCrystalReactionGroup, "damagedHpRate");
	binding.reactionSettings->criticalHpRate = std::clamp(binding.reactionSettings->criticalHpRate, 0.0f, 1.0f);
	binding.reactionSettings->damagedHpRate = std::clamp(binding.reactionSettings->damagedHpRate, binding.reactionSettings->criticalHpRate, 1.0f);
}

void CrystalParameterController::RegisterHpBarParameters(const HpBarBinding& binding)
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	parameters->CreateGroup(kCrystalHpBarGroup);
	parameters->AddItem(kCrystalHpBarGroup, "crystalHpBarVisible", *binding.visible);
	parameters->AddItem(kCrystalHpBarGroup, "crystalHpBarAlwaysVisible", *binding.alwaysVisible);
	parameters->AddItem(kCrystalHpBarGroup, "crystalHpBarOffsetY", *binding.offsetY, -2.0f, 8.0f);
	parameters->AddItem(kCrystalHpBarGroup, "crystalHpBarWidth", *binding.width, 20.0f, 240.0f);
	parameters->AddItem(kCrystalHpBarGroup, "crystalHpBarHeight", *binding.height, 2.0f, 40.0f);
	parameters->AddItem(kCrystalHpBarGroup, "crystalHpBarShowTime", *binding.showTime, 0.0f, 10.0f);
	parameters->SetDisplayName(kCrystalHpBarGroup, "crystalHpBarVisible", "クリスタルHPバー表示");
	parameters->SetDisplayName(kCrystalHpBarGroup, "crystalHpBarAlwaysVisible", "常時表示");
	parameters->SetDisplayName(kCrystalHpBarGroup, "crystalHpBarOffsetY", "頭上オフセットY");
	parameters->SetDisplayName(kCrystalHpBarGroup, "crystalHpBarWidth", "バー幅");
	parameters->SetDisplayName(kCrystalHpBarGroup, "crystalHpBarHeight", "バー高さ");
	parameters->SetDisplayName(kCrystalHpBarGroup, "crystalHpBarShowTime", "被弾後表示時間");
	parameters->RegisterParameterApplier(kCrystalHpBarGroup, this, [this, binding]() { ApplyHpBarParameters(binding); });
	parameters->LoadFile(kCrystalHpBarGroup);
}

void CrystalParameterController::UnregisterHpBarParameters()
{
	Ken4lowEngine::ParameterManager::GetInstance()->UnregisterParameterApplier(kCrystalHpBarGroup, this);
}

void CrystalParameterController::ApplyHpBarParameters(const HpBarBinding& binding) const
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	*binding.visible = parameters->GetValue<bool>(kCrystalHpBarGroup, "crystalHpBarVisible");
	*binding.alwaysVisible = parameters->GetValue<bool>(kCrystalHpBarGroup, "crystalHpBarAlwaysVisible");
	*binding.offsetY = parameters->GetValue<float>(kCrystalHpBarGroup, "crystalHpBarOffsetY");
	*binding.width = std::max(1.0f, parameters->GetValue<float>(kCrystalHpBarGroup, "crystalHpBarWidth"));
	*binding.height = std::max(1.0f, parameters->GetValue<float>(kCrystalHpBarGroup, "crystalHpBarHeight"));
	*binding.showTime = std::max(0.0f, parameters->GetValue<float>(kCrystalHpBarGroup, "crystalHpBarShowTime"));
}

void CrystalParameterController::RegisterSkyColorParameters(const SkyColorBinding& binding)
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	parameters->CreateGroup(kSkyColorGroup);
	parameters->AddItem(kSkyColorGroup, "skyColorChangeEnabled", *binding.enabled);
	parameters->AddItem(kSkyColorGroup, "skyColorChangeTime", *binding.changeTime, 0.1f, 10.0f);
	parameters->AddItem(kSkyColorGroup, "normalSkyColor", *binding.normalColor);
	parameters->AddItem(kSkyColorGroup, "brokenSkyColor", *binding.brokenColor);
	parameters->AddItem(kSkyColorGroup, "skyDarkness", *binding.darkness, 0.0f, 1.0f);
	parameters->AddItem(kSkyColorGroup, "skyRedTint", *binding.redTint, 0.0f, 1.0f);
	parameters->AddItem(kSkyColorGroup, "skyPurpleTint", *binding.purpleTint, 0.0f, 1.0f);
	parameters->AddItem(kSkyColorGroup, "changeSkyOnAllCrystalsBroken", *binding.changeOnAllCrystalsBroken);
	parameters->SetDisplayName(kSkyColorGroup, "skyColorChangeEnabled", "空色変化ON");
	parameters->SetDisplayName(kSkyColorGroup, "skyColorChangeTime", "空色変化時間");
	parameters->SetDisplayName(kSkyColorGroup, "normalSkyColor", "通常空色");
	parameters->SetDisplayName(kSkyColorGroup, "brokenSkyColor", "破壊後空色");
	parameters->SetDisplayName(kSkyColorGroup, "skyDarkness", "空の暗さ");
	parameters->SetDisplayName(kSkyColorGroup, "skyRedTint", "赤み");
	parameters->SetDisplayName(kSkyColorGroup, "skyPurpleTint", "紫み");
	parameters->SetDisplayName(kSkyColorGroup, "changeSkyOnAllCrystalsBroken", "全破壊時に空色変化");
	parameters->RegisterParameterApplier(kSkyColorGroup, this, [this, binding]() { ApplySkyColorParameters(binding); });
	parameters->LoadFile(kSkyColorGroup);
}

void CrystalParameterController::UnregisterSkyColorParameters()
{
	Ken4lowEngine::ParameterManager::GetInstance()->UnregisterParameterApplier(kSkyColorGroup, this);
}

void CrystalParameterController::ApplySkyColorParameters(const SkyColorBinding& binding) const
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	// ParameterManagerから空色設定を反映する処理。
	*binding.enabled = parameters->GetValue<bool>(kSkyColorGroup, "skyColorChangeEnabled");
	*binding.changeTime = std::max(0.1f, parameters->GetValue<float>(kSkyColorGroup, "skyColorChangeTime"));
	*binding.normalColor = parameters->GetValue<K4E::Vector4>(kSkyColorGroup, "normalSkyColor");
	*binding.brokenColor = parameters->GetValue<K4E::Vector4>(kSkyColorGroup, "brokenSkyColor");
	*binding.darkness = std::clamp(parameters->GetValue<float>(kSkyColorGroup, "skyDarkness"), 0.0f, 1.0f);
	*binding.redTint = std::clamp(parameters->GetValue<float>(kSkyColorGroup, "skyRedTint"), 0.0f, 1.0f);
	*binding.purpleTint = std::clamp(parameters->GetValue<float>(kSkyColorGroup, "skyPurpleTint"), 0.0f, 1.0f);
	*binding.changeOnAllCrystalsBroken = parameters->GetValue<bool>(kSkyColorGroup, "changeSkyOnAllCrystalsBroken");
}

std::string CrystalParameterController::BuildCrystalGroupName(const CrystalSpawnPoint& spawnPoint) const
{
	return std::string(kCrystalSpawnerRootGroup) + "/" + spawnPoint.crystalName;
}

const char* CrystalParameterController::ToEnemyTypeName(EnemyType enemyType) const
{
	switch (enemyType)
	{
	case EnemyType::Melee:
		return "Melee";
	case EnemyType::MidRange:
		return "MidRange";
	default:
		return "Melee";
	}
}

EnemyType CrystalParameterController::ParseCrystalEnemyType(const std::string& enemyTypeName) const
{
	if (enemyTypeName == "GuardianBoss")
	{
		// 旧保存値や未接続Boss指定も通常雑魚生成では安全なMeleeへ正規化する。
		return EnemyType::Melee;
	}
	return ParseEnemyType(enemyTypeName);
}
