#pragma once

#include "BulletEnemyCollisionSoA.h"
#include "BulletManager.h"
#include "CharacterWorld.h"

#include <algorithm>

/// -------------------------------------------------------------
/// Gameplay側のBulletManager / CharacterWorldからSoA判定用データを作る橋渡し
/// -------------------------------------------------------------
class BulletEnemySoABridge
{
public:
	static void BuildCollisionData(const BulletManager& bulletManager, const CharacterWorld& characterWorld, Ken4lowEngine::BulletEnemyCollisionSoA& outCollisionSoA);

private:
	static void AppendEnemies(const CharacterWorld& characterWorld, Ken4lowEngine::BulletEnemyCollisionSoA& outCollisionSoA);

};
