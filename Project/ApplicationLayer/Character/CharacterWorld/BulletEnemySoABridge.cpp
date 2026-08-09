#define NOMINMAX
#include "BulletEnemySoABridge.h"

void BulletEnemySoABridge::BuildCollisionData(const BulletManager& bulletManager, const CharacterWorld& characterWorld, Ken4lowEngine::BulletEnemyCollisionSoA& outCollisionSoA)
{
	// 本編オブジェクトはそのまま残し、判定に必要なデータだけをSoAへ転送する。
	outCollisionSoA.ClearFrameData();
	outCollisionSoA.Reserve(bulletManager.GetActiveCount(), static_cast<size_t>(characterWorld.GetAliveNormalEnemyCount()));
	bulletManager.AppendCollisionSoABullets(outCollisionSoA);
	AppendEnemies(characterWorld, outCollisionSoA);
}

void BulletEnemySoABridge::AppendEnemies(const CharacterWorld& characterWorld, Ken4lowEngine::BulletEnemyCollisionSoA& outCollisionSoA)
{
	const auto& enemies = characterWorld.GetEnemies();
	for (const auto& enemy : enemies)
	{
		if (!enemy || enemy->IsDead() || enemy->IsRemovable())
		{
			continue;
		}

		const Ken4lowEngine::Vector3 halfSize = enemy->GetOBBHalfSize();
		const float radius = std::max(halfSize.x, halfSize.z);

		// 敵の見た目やAIはEnemyBase側に残し、中心座標・半径・HPだけをSoAへ渡す。
		outCollisionSoA.AddEnemy(
			enemy->GetCenterPosition(),
			radius,
			enemy->GetHp(),
			true);
	}
}