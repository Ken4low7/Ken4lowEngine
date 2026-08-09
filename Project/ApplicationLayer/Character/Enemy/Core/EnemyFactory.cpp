#include "EnemyFactory.h"

#include "../Actor/EnemyActor.h"

std::unique_ptr<EnemyBase> EnemyFactory::Create(EnemyType enemyType)
{
	const EnemyType resolvedType = enemyType == EnemyType::MidRange ? EnemyType::MidRange : EnemyType::Melee;
	return std::make_unique<Ken4lowEngine::EnemyActor>(resolvedType); // 通常敵の生成型をEnemyActor一種類へ固定し、旧具象クラスを復活させない。
}
