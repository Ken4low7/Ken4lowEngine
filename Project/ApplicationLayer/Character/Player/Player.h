#pragma once

#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"

/// 古いinclude名を残す外部境界。実体と機能はPlayerActorへ一本化されています。
class Player : public Ken4lowEngine::PlayerActor
{
public:
	void Heal(float amount) { HealRuntime(amount); } // 旧Item入口だけを現役Healthへ転送する。
};
