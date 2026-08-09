#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "Collider.h"
#include "CollisionTypes.h"

namespace K4E = ::Ken4lowEngine;

/// Collider用途ごとのObjectChannelとResponse設定を再利用するためのプリセット。
struct CollisionPreset
{
	static constexpr uint32_t kMaxObjectChannels = static_cast<uint32_t>(EObjectChannel::Count);

	std::string name{};
	EObjectChannel objectChannel = EObjectChannel::Default;
	bool queryEnabled = true;
	bool physicsEnabled = true;
	std::array<ECollisionResponse, kMaxObjectChannels> responses{};

	ECollisionResponse GetResponse(EObjectChannel other) const
	{
		const uint32_t index = ToCollisionTypeId(other);
		if (index >= kMaxObjectChannels) return ECollisionResponse::Ignore;
		return responses[index];
	}
};

enum class ECollisionPresetId : uint8_t
{
	WorldStatic,
	WorldDynamic,
	Player,
	Enemy,
	Boss,
	Item,
	PlayerProjectile,
	EnemyProjectile,
	BossProjectile,
	Projectile, // 互換用: PlayerProjectileと同じ内容を返す。
	Trigger,
};

inline CollisionPreset MakeCollisionPreset(std::string_view name, EObjectChannel objectChannel, bool queryEnabled, bool physicsEnabled)
{
	CollisionPreset preset{};
	preset.name = std::string(name);
	preset.objectChannel = objectChannel;
	preset.queryEnabled = queryEnabled;
	preset.physicsEnabled = physicsEnabled;
	preset.responses.fill(ECollisionResponse::Ignore);
	return preset;
}

inline void SetPresetResponse(CollisionPreset& preset, EObjectChannel other, ECollisionResponse response)
{
	const uint32_t index = ToCollisionTypeId(other);
	if (index >= CollisionPreset::kMaxObjectChannels) return;
	preset.responses[index] = response;
}

inline CollisionPreset GetCollisionPreset(ECollisionPresetId presetId)
{
	switch (presetId)
	{
	case ECollisionPresetId::WorldStatic:
	{
		CollisionPreset preset = MakeCollisionPreset("WorldStatic", EObjectChannel::WorldStatic, true, true);
		SetPresetResponse(preset, EObjectChannel::Player, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::Enemy, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::Boss, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::PlayerProjectile, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::EnemyProjectile, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::BossProjectile, ECollisionResponse::Block);
		return preset;
	}
	case ECollisionPresetId::WorldDynamic:
	{
		return MakeCollisionPreset("WorldDynamic", EObjectChannel::Default, true, true);
	}
	case ECollisionPresetId::Player:
	{
		CollisionPreset preset = MakeCollisionPreset("Player", EObjectChannel::Player, true, true);
		SetPresetResponse(preset, EObjectChannel::Enemy, ECollisionResponse::Overlap);
		SetPresetResponse(preset, EObjectChannel::Boss, ECollisionResponse::Overlap);
		SetPresetResponse(preset, EObjectChannel::EnemyProjectile, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::BossProjectile, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::Item, ECollisionResponse::Overlap);
		SetPresetResponse(preset, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		return preset;
	}
	case ECollisionPresetId::Enemy:
	{
		CollisionPreset preset = MakeCollisionPreset("Enemy", EObjectChannel::Enemy, true, true);
		SetPresetResponse(preset, EObjectChannel::Player, ECollisionResponse::Overlap);
		SetPresetResponse(preset, EObjectChannel::PlayerProjectile, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		return preset;
	}
	case ECollisionPresetId::Boss:
	{
		CollisionPreset preset = MakeCollisionPreset("Boss", EObjectChannel::Boss, true, true);
		SetPresetResponse(preset, EObjectChannel::Player, ECollisionResponse::Overlap);
		SetPresetResponse(preset, EObjectChannel::PlayerProjectile, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		return preset;
	}
	case ECollisionPresetId::Item:
	{
		CollisionPreset preset = MakeCollisionPreset("Item", EObjectChannel::Item, true, false);
		SetPresetResponse(preset, EObjectChannel::Player, ECollisionResponse::Overlap);
		return preset;
	}
	case ECollisionPresetId::PlayerProjectile:
	case ECollisionPresetId::Projectile:
	{
		CollisionPreset preset = MakeCollisionPreset(
			presetId == ECollisionPresetId::Projectile ? "Projectile" : "PlayerProjectile",
			EObjectChannel::PlayerProjectile,
			true,
			false);
		SetPresetResponse(preset, EObjectChannel::Enemy, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::Boss, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::Crystal, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		return preset;
	}
	case ECollisionPresetId::EnemyProjectile:
	{
		CollisionPreset preset = MakeCollisionPreset("EnemyProjectile", EObjectChannel::EnemyProjectile, true, false);
		SetPresetResponse(preset, EObjectChannel::Player, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		return preset;
	}
	case ECollisionPresetId::BossProjectile:
	{
		CollisionPreset preset = MakeCollisionPreset("BossProjectile", EObjectChannel::BossProjectile, true, false);
		SetPresetResponse(preset, EObjectChannel::Player, ECollisionResponse::Block);
		SetPresetResponse(preset, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		return preset;
	}
	case ECollisionPresetId::Trigger:
	{
		CollisionPreset preset = MakeCollisionPreset("Trigger", EObjectChannel::Default, true, false);
		SetPresetResponse(preset, EObjectChannel::Player, ECollisionResponse::Overlap);
		return preset;
	}
	default:
		return MakeCollisionPreset("Default", EObjectChannel::Default, true, true);
	}
}

inline std::vector<CollisionPreset> GetDefaultCollisionPresets()
{
	return {
		GetCollisionPreset(ECollisionPresetId::WorldStatic),
		GetCollisionPreset(ECollisionPresetId::WorldDynamic),
		GetCollisionPreset(ECollisionPresetId::Player),
		GetCollisionPreset(ECollisionPresetId::Enemy),
		GetCollisionPreset(ECollisionPresetId::Boss),
		GetCollisionPreset(ECollisionPresetId::Item),
		GetCollisionPreset(ECollisionPresetId::PlayerProjectile),
		GetCollisionPreset(ECollisionPresetId::EnemyProjectile),
		GetCollisionPreset(ECollisionPresetId::BossProjectile),
		GetCollisionPreset(ECollisionPresetId::Projectile),
		GetCollisionPreset(ECollisionPresetId::Trigger),
	};
}

inline void ApplyCollisionPreset(K4E::Collider& collider, const CollisionPreset& preset)
{
	collider.SetTypeID(ToCollisionTypeId(preset.objectChannel));
	collider.SetObjectChannel(preset.objectChannel);
	collider.SetCollisionPreset(preset.name);
	collider.SetEnabled(true);
	collider.SetQueryEnabled(preset.queryEnabled);
	collider.SetPhysicsEnabled(preset.physicsEnabled);
	collider.SetTrigger(preset.queryEnabled && !preset.physicsEnabled);
	collider.ResetCollisionResponses(static_cast<uint8_t>(ECollisionResponse::Ignore));
	for (uint32_t channelIndex = 0; channelIndex < CollisionPreset::kMaxObjectChannels; ++channelIndex)
	{
		collider.SetCollisionResponseId(channelIndex, static_cast<uint8_t>(preset.responses[channelIndex]));
	}
}

inline void ApplyCollisionPreset(K4E::Collider& collider, ECollisionPresetId presetId)
{
	ApplyCollisionPreset(collider, GetCollisionPreset(presetId));
}

/// CharacterActor系はComponent所有ColliderへPresetを適用し、Actor自身の独自Colliderを必要としない。
template<class T>
	requires requires(T& value) { value.GetCollisionPrimitive(); }
inline void ApplyCollisionPreset(T& character, const CollisionPreset& preset)
{
	if (K4E::Collider* collider = character.GetCollisionPrimitive()) ApplyCollisionPreset(*collider, preset);
}

template<class T>
	requires requires(T& value) { value.GetCollisionPrimitive(); }
inline void ApplyCollisionPreset(T& character, ECollisionPresetId presetId)
{
	ApplyCollisionPreset(character, GetCollisionPreset(presetId));
}
