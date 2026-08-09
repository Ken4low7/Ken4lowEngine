#pragma once

#include "Matrix4x4.h"

#include <vector>

class EnemyBase;

namespace K4E = ::Ken4lowEngine;

/// 通常敵のWorldGauge表示時間だけを管理し、旧Sprite HPバーを生成しない。
class EnemyHPBarManager
{
public:
	void Initialize();

	void Update(
		const std::vector<EnemyBase*>& enemies,
		const K4E::Matrix4x4& viewMatrix,
		const K4E::Matrix4x4& projMatrix,
		float screenWidth,
		float screenHeight,
		float deltaTime,
		const EnemyBase* aimedEnemy = nullptr,
		bool showOnlyWhenAimed = true,
		float visibleHoldTime = 0.3f);

	void Draw();
	void DrawImGuiContent() const;

private:
	struct Entry
	{
		EnemyBase* enemy = nullptr;
		float aimVisibleTimer = 0.0f;
		bool updatedThisFrame = false;
		bool visibleThisFrame = false;
	};

	Entry* FindEntry(EnemyBase* enemy);
	Entry& FindOrCreateEntry(EnemyBase* enemy);

private:
	std::vector<Entry> entries_{};
};
