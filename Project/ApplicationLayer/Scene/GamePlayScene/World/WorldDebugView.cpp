#include "WorldDebugView.h"

#include "AmmoRecoveryItemSpawner.h"
#include "AimTargetDetector.h"
#include "BulletManager.h"
#include "BulletEnemySoABridge.h"
#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "CrystalManager.h"
#include "EnemyHPBarManager.h"
#include "StageObjectiveManager.h"
#include "Player.h"
#include "CollisionTypeIdDef.h"
#include "GpuParticleManager.h"
#include "ParticleManager.h"
#include "Wireframe.h"

#include <chrono>
#include <cstddef>
#include <cstdint>

#ifdef USE_IMGUI
#include <GameTimer.h>
#include <imgui.h>
#endif

namespace K4E = ::Ken4lowEngine;

void WorldDebugView::DrawGameDebugImGui(const Dependencies& deps)
{
#ifdef USE_IMGUI
	// ステージ目的の進行状態はStageObjectiveManagerから参照して表示する。
	if (deps.stageObjectiveManager)
	{
		const StageObjectiveManager::Snapshot& snapshot = deps.stageObjectiveManager->GetSnapshot();
		ImGui::SeparatorText("Stage Objective");
		ImGui::Text("Objective: %s", snapshot.title.c_str());
		ImGui::Text("Status: %s", StageObjectiveManager::GetStatusDebugName(snapshot.status));
		ImGui::Text("Detail: %s", snapshot.detail.c_str());
		ImGui::Text("Objective Value: %d / %d", snapshot.currentValue, snapshot.targetValue);
		ImGui::Text("Progress: %.1f%%", snapshot.normalizedProgress * 100.0f);
		ImGui::Text("Stage Time: %.2f sec", deps.stageObjectiveManager->GetStageElapsedSec());
		ImGui::Text("Device Points: %d", deps.stageObjectiveManager->GetDevicePointCount());
		ImGui::Text("Defense Targets: %d", deps.stageObjectiveManager->GetDefenseTargetPointCount());
		ImGui::Text("Goal Points: %d", deps.stageObjectiveManager->GetGoalPointCount());
		ImGui::Text("Boss Spawn Point: %s", deps.stageObjectiveManager->HasBossSpawnPoint() ? "true" : "false");
		ImGui::Text("Reached Goal: %s", deps.stageObjectiveManager->HasReachedGoal() ? "true" : "false");
		ImGui::Text("Boss Defeated: %s", deps.stageObjectiveManager->IsBossDefeated() ? "true" : "false");
		ImGui::Text("Defense Target Destroyed: %s", deps.stageObjectiveManager->IsDefenseTargetDestroyed() ? "true" : "false");
	}

	ImGui::Text("Player Dead: %s", deps.isPlayerDead && deps.isPlayerDead() ? "true" : "false");
	ImGui::Text("Enemies: %d", deps.characters ? deps.characters->GetEnemyCount() : 0);

	if (deps.drawGameplayPhysicsDebugImGui)
	{
		deps.drawGameplayPhysicsDebugImGui();
	}
	if (deps.drawBossBattleDebugImGui)
	{
		deps.drawBossBattleDebugImGui();
	}

	if (deps.characters && deps.characters->GetPlayer())
	{
		const auto* player = deps.characters->GetPlayer();
		ImGui::Text("Player HP: %.1f / %.1f", player->GetHP(), player->GetMaxHP());
	}
	else
	{
		ImGui::Text("Player HP: 0.0 / 0.0");
	}

	if (deps.aimTargetDetector)
	{
		deps.aimTargetDetector->DrawImGui();
	}
	if (deps.crystalManager)
	{
		deps.crystalManager->DrawImGui();
	}
	if (deps.ammoRecoveryItemSpawner)
	{
		deps.ammoRecoveryItemSpawner->DrawImGui();
	}

	auto* wireframe = K4E::Wireframe::GetInstance();
	bool debugDrawEnabled = wireframe->IsDebugDrawEnabled();
#ifdef _DEBUG
	if (ImGui::Checkbox("デバッグ描画有効", &debugDrawEnabled))
	{
		// Debug描画のON/OFFはWireframeへ集約し、各描画クラス側の状態ずれを防ぐ。
		wireframe->SetDebugDrawEnabled(debugDrawEnabled);
	}
#else
	ImGui::Text("デバッグ描画有効: いいえ");
#endif
	ImGui::Text("Release時デバッグ描画無効: %s", K4E::Wireframe::IsDebugDrawSupported() ? "Debugビルド" : "はい");

	const float fps = K4E::GameTimer::GetInstance()->GetFPS();
	const auto* particleManager = K4E::ParticleManager::GetInstance();
	const auto* gpuParticleManager = K4E::GpuParticleManager::GetInstance();

	const size_t activeBulletCount = deps.bulletManager ? deps.bulletManager->GetActiveCount() : 0;
	const size_t totalBulletCount = deps.bulletManager ? deps.bulletManager->GetCount() : 0;
	const size_t activeParticleCount = particleManager ? particleManager->GetActiveParticleCount() : 0;
	const size_t totalParticleCount = particleManager ? particleManager->GetTotalParticleCount() : 0;
	const uint32_t gpuParticleActiveCount = gpuParticleManager ? gpuParticleManager->GetEstimatedActiveParticleCount() : 0;
	const size_t particleEmitterCount = gpuParticleManager ? gpuParticleManager->GetEmitterCount() : 0;
	const size_t activeParticleEmitterCount = gpuParticleManager ? gpuParticleManager->GetActiveEmitterCount() : 0;
	const size_t colliderCount = deps.collisionManager ? deps.collisionManager->GetColliderCount() : 0;
	const size_t bulletColliderCount = deps.collisionManager
		? deps.collisionManager->GetColliderCountByType(static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
		: 0;
	const size_t enemyBulletColliderCount = deps.collisionManager
		? deps.collisionManager->GetColliderCountByType(static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet)) +
		deps.collisionManager->GetColliderCountByType(static_cast<uint32_t>(CollisionTypeIdDef::kBossBullet))
		: 0;
	const uint32_t drawCallCount = gpuParticleManager ? gpuParticleManager->GetLastDrawCallCount() : 0;

	ImGui::SeparatorText("Performance Counters");
	ImGui::Text("FPS: %.1f", fps);
	ImGui::Text("Active Bullet Count: %zu", activeBulletCount);
	ImGui::Text("Total Bullet Count: %zu", totalBulletCount);
	ImGui::Text("Active Particle Count: %zu", activeParticleCount);
	ImGui::Text("Total Particle Count: %zu", totalParticleCount);
	ImGui::Text("GPU Particle Active Count (estimated): %u", gpuParticleActiveCount);
	ImGui::Text("Particle Emitter Count: %zu (active: %zu)", particleEmitterCount, activeParticleEmitterCount);
	ImGui::Text("CollisionManager Collider Count: %zu", colliderCount);
	ImGui::Text("Bullet Collider Count: %zu (enemy/boss: %zu)", bulletColliderCount, enemyBulletColliderCount);
	ImGui::Text("Draw Call Count (GPU Particle): %u", drawCallCount);
	ImGui::SeparatorText("Simple Profile");
	ImGui::Text("BulletManager::Update: %.3f ms", deps.lastBulletUpdateMs);
	ImGui::Text("CollisionManager::CheckAllCollisions: %.3f ms", deps.lastCollisionUpdateMs);

	static K4E::BulletEnemyCollisionSoA bulletEnemySoAProbe;
	static K4E::BulletEnemyCollisionSoA::FrameStats bulletEnemySoAStats{};
	static float bulletEnemySoAMs = 0.0f;
	if (deps.bulletManager && deps.characters)
	{
		const auto begin = std::chrono::steady_clock::now();
		// 本編の弾・敵データをSoAへ転送して判定する。まだダメージ適用や弾削除には使わない。
		BulletEnemySoABridge::BuildCollisionData(*deps.bulletManager, *deps.characters, bulletEnemySoAProbe);
		bulletEnemySoAProbe.Execute(2.0f);
		bulletEnemySoAStats = bulletEnemySoAProbe.GetLastFrameStats();
		bulletEnemySoAMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - begin).count();
	}
	else
	{
		bulletEnemySoAStats = {};
		bulletEnemySoAMs = 0.0f;
	}

	ImGui::SeparatorText("BulletEnemy SoA Probe");
	ImGui::Text("Enabled: debug view only");
	ImGui::Text("SoA Probe Time: %.3f ms", bulletEnemySoAMs);
	ImGui::Text("SoA Active Bullets: %zu", bulletEnemySoAStats.activeBulletCount);
	ImGui::Text("SoA Active Enemies: %zu", bulletEnemySoAStats.activeEnemyCount);
	ImGui::Text("SoA Hit Count: %zu", bulletEnemySoAStats.hitCount);
	ImGui::Text("SoA Collision Checks: %zu", bulletEnemySoAStats.collisionChecks);
#else
(void)deps;
#endif
}

void WorldDebugView::DrawEnemyDebugImGui(const Dependencies& deps)
{
#ifdef USE_IMGUI
	// Enemy DebugにはEnemy HPBar Managerの軽量統計を追加する。
	if (ImGui::CollapsingHeader("HPBar Debug", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (deps.enemyHpBarManager)
		{
			deps.enemyHpBarManager->DrawImGuiContent();
		}
	}
#else
	(void)deps;
#endif
}

void WorldDebugView::DrawCollisionDebugImGui(const Dependencies& deps)
{
#ifdef USE_IMGUI
	// Collision Debugには当たり判定関連の表示切替と補助情報を集約する。
	if (deps.collisionManager)
	{
		deps.collisionManager->DrawImGui();
	}
#else
	(void)deps;
#endif
}
