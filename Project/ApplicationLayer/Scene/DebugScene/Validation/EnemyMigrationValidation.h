#pragma once

#include <AABB.h>

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class ActorWorld;
	class BossActor;
	class CharacterActor;
	class EnemyActor;
}

namespace K4E = ::Ken4lowEngine;

/// DebugSceneへ移行済みEnemyActorを配置し、DebugPlayerの射撃・照準・A*巡回を実戦形式で検証する。
class EnemyMigrationValidation
{
public:
	/// 保存済みPrefabを優先してEnemyActorを生成し、無い場合だけコード既定値へフォールバックする。
	void Initialize(K4E::ActorWorld& actorWorld);

	/// Play中にTarget接続、Camera中心照準、Weapon発射成立後のDamage適用を更新する。
	void Update(float deltaTime);

	/// Edit・Pause中もPIE復元後の参照だけを安全に張り直し、ゲーム用照準表示を解除する。
	void UpdateEditor();

	/// EnemyActorの検証情報だけをImGuiへ表示する。ゲームHUDはActorのUI Componentが描画する。
	void DrawImGui();

	/// ActorWorldへの非所有参照を解除する。
	void Finalize();

private:
	/// PIE複製後の現在WorldからEnemy、Boss、DebugPlayerを再取得して接続する。
	void RefreshActorReferencesAndBindings();

	/// Camera中心RayとCharacter ColliderのAABBから現在照準対象を決め、UI Componentの表示へ反映する。
	void UpdateAimTarget();

	/// WeaponComponentのShotRevision増加を1回だけDamageへ変換する。
	void ProcessWeaponShot();

	/// UIから予約されたDamageとResetを更新フェーズで処理する。
	void ProcessRequests();

private:
	K4E::ActorWorld* actorWorld_ = nullptr;
	K4E::EnemyActor* enemy_ = nullptr;
	K4E::BossActor* boss_ = nullptr;
	K4E::CharacterActor* target_ = nullptr;
	K4E::EnemyActor* aimedEnemy_ = nullptr;
	K4E::BossActor* aimedBoss_ = nullptr;
	std::vector<K4E::AABB> navigationObstacles_;
	std::string enemyName_ = "ComponentEnemy";
	std::string bossName_ = "ComponentBoss";
	std::string targetName_ = "DebugPlayer";
	std::string lastMessage_ = "DebugPlayerへのTarget接続を待機中";
	unsigned int lastProcessedShotRevision_ = 0;
	bool lastSucceeded_ = false;
	bool requestDamage_ = false;
	bool requestLethalDamage_ = false;
	bool requestReset_ = false;
};