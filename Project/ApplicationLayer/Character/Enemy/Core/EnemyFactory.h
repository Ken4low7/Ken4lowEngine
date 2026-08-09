#pragma once

#include "EnemyType.h"

#include <memory>

class EnemyBase;

/// 通常ゲーム用のMelee/MidRange雑魚敵だけを生成する境界を集約する。
class EnemyFactory
{
public:
	static std::unique_ptr<EnemyBase> Create(EnemyType enemyType);
};
