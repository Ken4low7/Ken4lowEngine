#pragma once
#include <cstddef>
#include <functional>

class AmmoRecoveryItemSpawner;
class AimTargetDetector;
class BulletManager;
class CharacterWorld;
class CollisionManager;
class CrystalManager;
class EnemyHPBarManager;
class StageObjectiveManager;

/// -------------------------------------------------------------
/// GamePlayWorld周辺のDebug ImGui表示をまとめる表示専用クラス。
///
/// World本体からデバッグ表示の依存と表示項目を切り離し、
/// GamePlayWorldが通常の進行・描画管理に集中できるようにする。
/// -------------------------------------------------------------
class WorldDebugView
{
public:
	/// GamePlayWorldが所有する各管理クラスへの一時参照。
	struct Dependencies
	{
		StageObjectiveManager* stageObjectiveManager = nullptr;
		CharacterWorld* characters = nullptr;
		AimTargetDetector* aimTargetDetector = nullptr;
		CrystalManager* crystalManager = nullptr;
		AmmoRecoveryItemSpawner* ammoRecoveryItemSpawner = nullptr;
		CollisionManager* collisionManager = nullptr;
		BulletManager* bulletManager = nullptr;
		EnemyHPBarManager* enemyHpBarManager = nullptr;
		float lastBulletUpdateMs = 0.0f;
		float lastCollisionUpdateMs = 0.0f;
		float lastBulletEnemySoAMs = 0.0f;
		size_t bulletEnemySoACollisionChecks = 0;
		size_t bulletEnemySoAHitCount = 0;
		size_t bulletEnemySoAActiveBulletCount = 0;
		size_t bulletEnemySoAActiveEnemyCount = 0;
		bool bulletEnemySoAProbeEnabled = false;
		std::function<bool()> isPlayerDead;
		std::function<void()> drawGameplayPhysicsDebugImGui;
		std::function<void()> drawBossBattleDebugImGui;
	};

	// GamePlay全体の進行状態・パフォーマンス・各ControllerのDebug項目を描画する。
	void DrawGameDebugImGui(const Dependencies& deps);
	// Enemy Debugタブに、敵HPバー管理の補助情報を描画する。
	void DrawEnemyDebugImGui(const Dependencies& deps);
	// Collision Debugタブに、当たり判定ManagerのDebug表示を描画する。
	void DrawCollisionDebugImGui(const Dependencies& deps);
};
