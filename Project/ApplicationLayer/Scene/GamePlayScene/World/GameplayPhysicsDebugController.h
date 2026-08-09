#pragma once

#include <functional>

namespace Ken4lowEngine
{
	class BossActor;
	class Stage;
}

namespace K4E = ::Ken4lowEngine;

class BulletManager;
class CharacterWorld;
class CollisionManager;

/// Gameplay中のPhysicsWorld移行状態とDebug表示を管理する軽量Controller。
class GameplayPhysicsDebugController
{
public:
	struct Dependencies
	{
		CharacterWorld* characters = nullptr;
		BulletManager* bulletManager = nullptr;
		CollisionManager* collisionManager = nullptr;
		K4E::Stage* stage = nullptr;
		std::function<K4E::BossActor*()> getBoss;
		std::function<bool()> isBossColliderRegistered;
	};

	GameplayPhysicsDebugController() = default;
	~GameplayPhysicsDebugController();
	void Initialize(const Dependencies& deps);
	void Finalize();
	void Update(const Dependencies& deps, float deltaTime);
	void Draw();
	void DrawImGui(const Dependencies& deps);

private:
	Dependencies deps_{}; // Bossを含む本編Physicsの所有権はCharacterWorld側に置く。
};
