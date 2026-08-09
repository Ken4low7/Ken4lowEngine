#include "CollisionSystemPolicy.h"

#include <algorithm>

namespace Ken4lowEngine
{
	namespace
	{
		CollisionSystemRule MakeRule(
			CollisionSystemPair pair,
			CollisionSystemOwner owner,
			const char* status,
			const char* note,
			bool doubleProcessingRisk)
		{
			return CollisionSystemRule{
				pair,
				owner,
				CollisionSystemPolicy::ToString(pair),
				status,
				note,
				doubleProcessingRisk,
			};
		}
	}

	void CollisionSystemPolicy::InitializeDefaults()
	{
		// 既存ゲーム挙動を守るため、移行済みテスト以外はLegacyCollisionManager側に残す。
		rules_[ToIndex(CollisionSystemPair::PlayerStage)] = MakeRule(
			CollisionSystemPair::PlayerStage,
			CollisionSystemOwner::LegacyCollisionManager,
			"PhysicsWorldへ一部移行済み",
			"床判定/壁押し戻しだけPhysicsWorldで確認可能。Player移動・ジャンプ本体は既存処理のまま。",
			true);
		rules_[ToIndex(CollisionSystemPair::PlayerEnemyBullet)] = MakeRule(
			CollisionSystemPair::PlayerEnemyBullet,
			CollisionSystemOwner::LegacyCollisionManager,
			"既存CollisionManagerに残す",
			"Player被弾処理は既存イベント経路を維持し、二重ダメージを避ける。",
			false);
		rules_[ToIndex(CollisionSystemPair::PlayerBulletEnemy)] = MakeRule(
			CollisionSystemPair::PlayerBulletEnemy,
			CollisionSystemOwner::LegacyCollisionManager,
			"PhysicsWorldへ移行予定",
			"PhysicsTestBulletでTriggerEnter確認済み。既存Bulletのダメージ処理はまだ移行しない。",
			true);
		rules_[ToIndex(CollisionSystemPair::PlayerBulletBoss)] = MakeRule(
			CollisionSystemPair::PlayerBulletBoss,
			CollisionSystemOwner::LegacyCollisionManager,
			"PhysicsWorldへ移行予定",
			"Bossダメージは既存CollisionManager側に残し、TriggerEvent移行時は二重ヒットを明示的に防ぐ。",
			true);
		rules_[ToIndex(CollisionSystemPair::BossAttackPlayer)] = MakeRule(
			CollisionSystemPair::BossAttackPlayer,
			CollisionSystemOwner::LegacyCollisionManager,
			"既存CollisionManagerに残す",
			"BossAttackのPlayerダメージは本Phaseでは触らない。",
			false);
		rules_[ToIndex(CollisionSystemPair::EnemyStage)] = MakeRule(
			CollisionSystemPair::EnemyStage,
			CollisionSystemOwner::LegacyCollisionManager,
			"今回は触らない",
			"Enemy移動はStage AABB/既存移動解決を維持する。",
			false);
		rules_[ToIndex(CollisionSystemPair::ItemPlayer)] = MakeRule(
			CollisionSystemPair::ItemPlayer,
			CollisionSystemOwner::LegacyCollisionManager,
			"既存CollisionManagerに残す",
			"Item取得処理は既存Colliderイベント/距離判定を維持する。",
			false);
		rules_[ToIndex(CollisionSystemPair::EnemyEnemy)] = MakeRule(
			CollisionSystemPair::EnemyEnemy,
			CollisionSystemOwner::LegacyCollisionManager,
			"今回は触らない",
			"群衆/押し合いはまだPhysicsWorldへ移さない。",
			false);
		rules_[ToIndex(CollisionSystemPair::BulletStage)] = MakeRule(
			CollisionSystemPair::BulletStage,
			CollisionSystemOwner::LegacyCollisionManager,
			"既存CollisionManagerに残す",
			"既存Bulletのステージ接触/寿命処理はそのまま。",
			false);
		rules_[ToIndex(CollisionSystemPair::TriggerTest)] = MakeRule(
			CollisionSystemPair::TriggerTest,
			CollisionSystemOwner::Disabled,
			"PhysicsWorldへ移行済み",
			"PhysicsTestBullet専用。既存Bulletとは別管理にして二重ダメージを避ける。",
			false);
		rules_[ToIndex(CollisionSystemPair::PhysicsTestObjectStage)] = MakeRule(
			CollisionSystemPair::PhysicsTestObjectStage,
			CollisionSystemOwner::Disabled,
			"PhysicsWorldへ移行済み",
			"本編挙動に影響しない明示ONの落下/押し戻し確認。",
			false);
	}

	void CollisionSystemPolicy::SetOwner(CollisionSystemPair pair, CollisionSystemOwner owner)
	{
		rules_[ToIndex(pair)].owner = owner;
	}

	CollisionSystemOwner CollisionSystemPolicy::GetOwner(CollisionSystemPair pair) const
	{
		return rules_[ToIndex(pair)].owner;
	}

	const CollisionSystemRule& CollisionSystemPolicy::GetRule(CollisionSystemPair pair) const
	{
		return rules_[ToIndex(pair)];
	}

	const char* CollisionSystemPolicy::ToString(CollisionSystemOwner owner)
	{
		switch (owner)
		{
		case CollisionSystemOwner::LegacyCollisionManager:
			return "LegacyCollisionManager";
		case CollisionSystemOwner::PhysicsWorld:
			return "PhysicsWorld";
		case CollisionSystemOwner::Disabled:
			return "Disabled";
		default:
			return "Unknown";
		}
	}

	const char* CollisionSystemPolicy::ToString(CollisionSystemPair pair)
	{
		switch (pair)
		{
		case CollisionSystemPair::PlayerStage:
			return "Player vs Stage";
		case CollisionSystemPair::PlayerEnemyBullet:
			return "Player vs EnemyBullet";
		case CollisionSystemPair::PlayerBulletEnemy:
			return "PlayerBullet vs Enemy";
		case CollisionSystemPair::PlayerBulletBoss:
			return "PlayerBullet vs Boss";
		case CollisionSystemPair::BossAttackPlayer:
			return "BossAttack vs Player";
		case CollisionSystemPair::EnemyStage:
			return "Enemy vs Stage";
		case CollisionSystemPair::ItemPlayer:
			return "Item vs Player";
		case CollisionSystemPair::EnemyEnemy:
			return "Enemy vs Enemy";
		case CollisionSystemPair::BulletStage:
			return "Bullet vs Stage";
		case CollisionSystemPair::TriggerTest:
			return "PhysicsTestBullet vs PhysicsTestTarget";
		case CollisionSystemPair::PhysicsTestObjectStage:
			return "PhysicsTestObject vs Stage";
		default:
			return "Unknown";
		}
	}

	size_t CollisionSystemPolicy::ToIndex(CollisionSystemPair pair)
	{
		const size_t index = static_cast<size_t>(pair);
		return std::min(index, kPairCount - 1);
	}

} // namespace Ken4lowEngine
