#pragma once

#include "WorldObjectId.h"

namespace Ken4lowEngine
{
	/// <summary>
	/// Actorの生ポインタを長期間保持せず、ActorIdからActorWorldへ解決するための軽量Handle。
	/// Worldを所有しないため、World破棄後にdangling pointerを残さない。
	/// </summary>
	class ActorHandle
	{
	public:
		constexpr ActorHandle() = default;
		explicit constexpr ActorHandle(ActorId id) : id_(id) {}

		constexpr ActorId GetId() const { return id_; }
		constexpr bool IsSet() const { return id_.IsValid(); }
		constexpr void Reset() { id_ = {}; }
		constexpr explicit operator bool() const { return IsSet(); }
		constexpr bool operator==(const ActorHandle&) const = default;

	private:
		ActorId id_{};
	};
} // namespace Ken4lowEngine
