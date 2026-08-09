#pragma once

#include "EnemySpawnCrystal.h"
#include "LightManager.h"
#include "Vector2.h"

#include <functional>
#include <string>
#include <vector>

/// -------------------------------------------------------------
/// クリスタル関連のParameterManager登録・反映を担当するクラス。
///
/// CrystalManagerからJson/ImGui調整の責務を切り離し、
/// CrystalManager本体をクリスタル生成・更新・破壊進行に集中させる。
/// -------------------------------------------------------------
class CrystalParameterController
{
public:
	struct ReactionBinding
	{
		CrystalReactionSettings* reactionSettings = nullptr;
		float* worldColorChangeTime = nullptr;
		float* worldDarkness = nullptr;
		float* worldRedTint = nullptr;
	};

	struct HpBarBinding
	{
		bool* visible = nullptr;
		bool* alwaysVisible = nullptr;
		float* offsetY = nullptr;
		float* width = nullptr;
		float* height = nullptr;
		float* showTime = nullptr;
	};

	struct SkyColorBinding
	{
		bool* enabled = nullptr;
		bool* changeOnAllCrystalsBroken = nullptr;
		float* changeTime = nullptr;
		K4E::Vector4* normalColor = nullptr;
		K4E::Vector4* brokenColor = nullptr;
		float* darkness = nullptr;
		float* redTint = nullptr;
		float* purpleTint = nullptr;
	};

public:
	void RegisterCrystalParameters(CrystalSpawnPoint& spawnPoint, const std::function<void()>& onApply);
	void UnregisterCrystalParameters();
	void ApplyParameterToSpawnPoint(CrystalSpawnPoint& spawnPoint) const;

	void RegisterReactionParameters(const ReactionBinding& binding);
	void UnregisterReactionParameters();
	void ApplyReactionParameters(const ReactionBinding& binding) const;

	void RegisterHpBarParameters(const HpBarBinding& binding);
	void UnregisterHpBarParameters();
	void ApplyHpBarParameters(const HpBarBinding& binding) const;

	void RegisterSkyColorParameters(const SkyColorBinding& binding);
	void UnregisterSkyColorParameters();
	void ApplySkyColorParameters(const SkyColorBinding& binding) const;

private:
	std::string BuildCrystalGroupName(const CrystalSpawnPoint& spawnPoint) const;
	const char* ToEnemyTypeName(EnemyType enemyType) const;
	EnemyType ParseCrystalEnemyType(const std::string& enemyTypeName) const;

private:
	std::vector<std::string> parameterGroupNames_;
};
