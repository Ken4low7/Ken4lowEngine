#pragma once

#include <string>

namespace Ken4lowEngine
{
	class ActorWorld;
	class BossActor;
	class CharacterActor;
}

namespace K4E = ::Ken4lowEngine;

/// DebugSceneへBossActorを配置し、Target・弱点・死亡地点・Prefabを検証する補助クラス。
class BossMigrationValidation
{
public:
	void Initialize(K4E::ActorWorld& actorWorld);
	void Update();
	void UpdateEditor();
	void DrawImGui();
	void Finalize();

private:
	void RefreshActorReferencesAndBindings();
	void ProcessRequests();

private:
	K4E::ActorWorld* actorWorld_ = nullptr;
	K4E::BossActor* boss_ = nullptr;
	K4E::CharacterActor* target_ = nullptr;
	std::string bossName_ = "ComponentBoss";
	std::string targetName_ = "DebugPlayer";
	std::string jsonPath_ = "Resources/ActorPrefabs/ComponentBoss.json";
	std::string lastMessage_ = "DebugPlayerへのTarget接続を待機中";
	bool lastSucceeded_ = false;
	bool requestHeadDamage_ = false;
	bool requestBodyDamage_ = false;
	bool requestDeathPositionTest_ = false;
	bool requestSelectionRuleValidation_ = false;
	bool requestReset_ = false;
	bool requestSave_ = false;
	bool requestReload_ = false;
};
