#pragma once

#include "Vector3.h"

#include <cstdint>

namespace Ken4lowEngine
{
	class Actor;
	class CharacterActor;

	enum class CharacterDamageType : std::uint8_t
	{
		Generic,
		Projectile,
		Melee,
		Explosion,
		Environment,
	};

	/// CharacterActorへ渡すダメージ要求を、発生元や命中位置とまとめて保持する。
	struct CharacterDamageInfo
	{
		float amount = 0.0f;
		const Actor* sourceActor = nullptr;
		std::uint32_t sourceColliderId = 0u;
		std::int32_t weaponId = 0;
		std::uint32_t attackId = 0u;
		CharacterDamageType damageType = CharacterDamageType::Generic;
		Vector3 hitPosition{};
		Vector3 hitDirection{};
		bool hasHitPosition = false;
		bool hasHitDirection = false;
		// Player・Enemy・Bossの演出側が同じDamage情報から位置・方向・武器を参照する。
	};

	/// CharacterHealthComponentが実際に適用したダメージ結果を返す。
	struct CharacterDamageResult
	{
		bool accepted = false;
		bool killed = false;
		float requestedDamage = 0.0f;
		float appliedDamage = 0.0f;
		float healthBefore = 0.0f;
		float healthAfter = 0.0f;
	};

	/// 生存状態がダメージによって死亡へ遷移した際の通知内容を保持する。
	struct CharacterDeathEvent
	{
		CharacterActor* character = nullptr;
		CharacterDamageInfo damage{};
		CharacterDamageResult result{};
	};
} // namespace Ken4lowEngine
