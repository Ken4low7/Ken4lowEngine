#pragma once

#include "WorldObjectId.h"

namespace Ken4lowEngine
{
	/// <summary>
	/// Actorの生ポインタを長期間保持せず、WorldIdとActorIdからActorWorldへ解決するための軽量Handle。
	/// World自体は所有せず、別Worldの同一ActorIdへ誤解決しない。
	/// </summary>
	class ActorHandle
	{
	public:
		constexpr ActorHandle() = default;
		constexpr ActorHandle(WorldId worldId, ActorId actorId) : worldId_(worldId), actorId_(actorId) {}

		constexpr WorldId GetWorldId() const { return worldId_; }
		constexpr ActorId GetId() const { return actorId_; }
		constexpr bool IsSet() const { return worldId_.IsValid() && actorId_.IsValid(); }
		constexpr void Reset() { worldId_ = {}; actorId_ = {}; }
		constexpr explicit operator bool() const { return IsSet(); }
		constexpr bool operator==(const ActorHandle&) const = default;

	private:
		WorldId worldId_{};
		ActorId actorId_{};
	};
} // namespace Ken4lowEngine
