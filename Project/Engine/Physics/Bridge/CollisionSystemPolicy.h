#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// 既存CollisionManagerとPhysicsWorldの担当先
	/// -------------------------------------------------------------
	enum class CollisionSystemOwner : uint8_t
	{
		LegacyCollisionManager,
		PhysicsWorld,
		Disabled,
	};

	/// -------------------------------------------------------------
	/// 段階移行中に担当を切り替える代表的な判定ペア
	/// -------------------------------------------------------------
	enum class CollisionSystemPair : uint8_t
	{
		PlayerStage,
		PlayerEnemyBullet,
		PlayerBulletEnemy,
		PlayerBulletBoss,
		BossAttackPlayer,
		EnemyStage,
		ItemPlayer,
		EnemyEnemy,
		BulletStage,
		TriggerTest,
		PhysicsTestObjectStage,
		Count,
	};

	/// -------------------------------------------------------------
	/// 判定ペアごとの移行方針メモ
	/// -------------------------------------------------------------
	struct CollisionSystemRule
	{
		CollisionSystemPair pair = CollisionSystemPair::PlayerStage;
		CollisionSystemOwner owner = CollisionSystemOwner::LegacyCollisionManager;
		const char* pairName = "";
		const char* migrationStatus = "";
		const char* note = "";
		bool doubleProcessingRisk = false;
	};

	/// -------------------------------------------------------------
	/// 既存CollisionManagerとPhysicsWorldの担当範囲を管理するクラス
	///
	/// 現在の整理表:
	/// - Player vs Stage: PhysicsWorldへ一部移行済み。床判定/押し戻しのみ、既存移動は維持。
	/// - Player vs EnemyBullet: 既存CollisionManagerに残す。
	/// - PlayerBullet vs Enemy: PhysicsWorldへ移行予定。現状は既存CollisionManagerに残す。
	/// - PlayerBullet vs Boss: PhysicsWorldへ移行予定。現状は既存CollisionManagerに残す。
	/// - BossAttack vs Player: 既存CollisionManagerに残す。
	/// - Enemy vs Stage: 今回は触らない。既存Stage AABB/移動解決を維持。
	/// - Item vs Player: 既存CollisionManagerに残す。
	/// - Enemy vs Enemy: 今回は触らない。
	/// - Bullet vs Stage: 既存CollisionManagerに残す。
	/// - PhysicsTestBullet/PhysicsTestObject: テスト専用としてPhysicsWorldで扱う。
	/// -------------------------------------------------------------
	class CollisionSystemPolicy
	{
	public:
		static constexpr size_t kPairCount = static_cast<size_t>(CollisionSystemPair::Count);

		// 既存挙動を壊さない安全側の担当表へ初期化する。
		void InitializeDefaults();

		// 段階移行中に旧判定と新Physics判定の担当先を切り替える。
		void SetOwner(CollisionSystemPair pair, CollisionSystemOwner owner);

		// 現在の担当先を取得する。
		CollisionSystemOwner GetOwner(CollisionSystemPair pair) const;

		// Debug表示や移行確認用に判定ペアのルールを取得する。
		const CollisionSystemRule& GetRule(CollisionSystemPair pair) const;
		const std::array<CollisionSystemRule, kPairCount>& GetRules() const { return rules_; }

		static const char* ToString(CollisionSystemOwner owner);
		static const char* ToString(CollisionSystemPair pair);

	private:
		static size_t ToIndex(CollisionSystemPair pair);

	private:
		std::array<CollisionSystemRule, kPairCount> rules_{};
	};

} // namespace Ken4lowEngine
