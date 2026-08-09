#pragma once
#include <array>
#include <cstdint>

#include "CollisionTypes.h"

/// TraceChannelごとに、ObjectChannelへの問い合わせ反応を保持する。
class TraceResponseMatrix
{
public:
	static constexpr uint32_t kMaxTraceChannels = ToTraceChannelId(ETraceChannel::Count);
	static constexpr uint32_t kMaxObjectChannels = static_cast<uint32_t>(EObjectChannel::Count);

	TraceResponseMatrix()
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

	void SetResponse(ETraceChannel traceChannel, EObjectChannel objectChannel, ECollisionResponse response)
	{
		const uint32_t trace = ToTraceChannelId(traceChannel);
		const uint32_t object = ToCollisionTypeId(objectChannel);
		if (trace >= kMaxTraceChannels || object >= kMaxObjectChannels) return;
		responses_[trace][object] = response;
	}

	ECollisionResponse GetResponse(ETraceChannel traceChannel, EObjectChannel objectChannel) const
	{
		const uint32_t trace = ToTraceChannelId(traceChannel);
		const uint32_t object = ToCollisionTypeId(objectChannel);
		if (trace >= kMaxTraceChannels || object >= kMaxObjectChannels) return ECollisionResponse::Ignore;
		return responses_[trace][object];
	}

	ECollisionResponse GetResponse(ETraceChannel traceChannel, uint32_t objectTypeId) const
	{
		if (objectTypeId >= kMaxObjectChannels) return ECollisionResponse::Ignore;
		return GetResponse(traceChannel, ToObjectChannel(objectTypeId));
	}

	void InitializeLegacyDefaults()
	{
		// TraceChannelは新規入口専用なので、明示した対象以外は問い合わせ対象外にする。
		SetAll(ECollisionResponse::Ignore);

		// 既存の遮蔽用途に近いChannelは、まずWorldStaticだけをBlock対象にする。
		SetResponse(ETraceChannel::Visibility, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		SetResponse(ETraceChannel::Camera, EObjectChannel::WorldStatic, ECollisionResponse::Block);
		SetResponse(ETraceChannel::AI, EObjectChannel::WorldStatic, ECollisionResponse::Block);

		// Weaponは射撃用途の予定地として、敵/ボス/クリスタル/ワールドをBlock対象にする。
		SetResponse(ETraceChannel::Weapon, EObjectChannel::Enemy, ECollisionResponse::Block);
		SetResponse(ETraceChannel::Weapon, EObjectChannel::Boss, ECollisionResponse::Block);
		SetResponse(ETraceChannel::Weapon, EObjectChannel::Crystal, ECollisionResponse::Block);
		SetResponse(ETraceChannel::Weapon, EObjectChannel::WorldStatic, ECollisionResponse::Block);

		// Interactionはアイテムやロック対象などのOverlap問い合わせへ広げる予定地。
		SetResponse(ETraceChannel::Interaction, EObjectChannel::Item, ECollisionResponse::Overlap);
	}

private:
	std::array<std::array<ECollisionResponse, kMaxObjectChannels>, kMaxTraceChannels> responses_{};
};
