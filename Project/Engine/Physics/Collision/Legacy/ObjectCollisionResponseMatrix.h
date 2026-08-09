#pragma once
#include <array>
#include <cstdint>

#include "CollisionTypes.h"

/// ObjectChannel同士のIgnore/Overlap/Blockを保持し、既存TypeID判定からUE風Responseへ移るための表。
class ObjectCollisionResponseMatrix
{
public:
	static constexpr uint32_t kMaxChannels = static_cast<uint32_t>(EObjectChannel::Count);

	ObjectCollisionResponseMatrix()
	{
		SetAll(ECollisionResponse::Ignore);
	}

	void SetAll(ECollisionResponse response)
	{
		for (auto& row : responses_)
		{
			row.fill(response);
		}
	}

	void SetResponse(EObjectChannel self, EObjectChannel other, ECollisionResponse response)
	{
		const uint32_t a = ToCollisionTypeId(self);
		const uint32_t b = ToCollisionTypeId(other);
		if (a >= kMaxChannels || b >= kMaxChannels) return;
		responses_[a][b] = response;
	}

	void SetSymmetricResponse(EObjectChannel a, EObjectChannel b, ECollisionResponse response)
	{
		SetResponse(a, b, response);
		SetResponse(b, a, response);
	}

	ECollisionResponse GetResponse(EObjectChannel self, EObjectChannel other) const
	{
		const uint32_t a = ToCollisionTypeId(self);
		const uint32_t b = ToCollisionTypeId(other);
		if (a >= kMaxChannels || b >= kMaxChannels) return ECollisionResponse::Ignore;
		return responses_[a][b];
	}

	ECollisionResponse GetResponse(uint32_t selfTypeId, uint32_t otherTypeId) const
	{
		if (selfTypeId >= kMaxChannels || otherTypeId >= kMaxChannels) return ECollisionResponse::Ignore;
		return responses_[selfTypeId][otherTypeId];
	}

	void InitializeLegacyDefaults()
	{
		// 既存CheckAllCollisionsで列挙されない組み合わせは、従来通り判定不要なIgnoreとして残す。
		SetAll(ECollisionResponse::Ignore);

		// 現在のCheckAllCollisionsが実際に回しているペアだけを非Ignoreとして写す。
		SetSymmetricResponse(EObjectChannel::Boss, EObjectChannel::Player, ECollisionResponse::Overlap);
		SetSymmetricResponse(EObjectChannel::Enemy, EObjectChannel::Player, ECollisionResponse::Overlap);
		SetSymmetricResponse(EObjectChannel::PlayerProjectile, EObjectChannel::Enemy, ECollisionResponse::Block);
		SetSymmetricResponse(EObjectChannel::PlayerProjectile, EObjectChannel::Crystal, ECollisionResponse::Block);
		SetSymmetricResponse(EObjectChannel::Boss, EObjectChannel::PlayerProjectile, ECollisionResponse::Block);
		SetSymmetricResponse(EObjectChannel::EnemyProjectile, EObjectChannel::Player, ECollisionResponse::Block);
		SetSymmetricResponse(EObjectChannel::Player, EObjectChannel::BossProjectile, ECollisionResponse::Block);
		SetSymmetricResponse(EObjectChannel::Player, EObjectChannel::Item, ECollisionResponse::Overlap);
		SetSymmetricResponse(EObjectChannel::Player, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		SetSymmetricResponse(EObjectChannel::Enemy, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		SetSymmetricResponse(EObjectChannel::Boss, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		SetSymmetricResponse(EObjectChannel::PlayerProjectile, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		SetSymmetricResponse(EObjectChannel::EnemyProjectile, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		SetSymmetricResponse(EObjectChannel::BossProjectile, EObjectChannel::WorldStatic, ECollisionResponse::Block);
	}

private:
	std::array<std::array<ECollisionResponse, kMaxChannels>, kMaxChannels> responses_{};
};
