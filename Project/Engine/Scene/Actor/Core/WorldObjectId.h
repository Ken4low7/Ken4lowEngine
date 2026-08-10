#pragma once

#include <cstdint>

namespace Ken4lowEngine
{
	/// <summary>ActorWorld内でActorを識別する実行時ID。0は無効値。</summary>
	struct ActorId
	{
		uint64_t value = 0;

		constexpr bool IsValid() const { return value != 0; }
		constexpr explicit operator bool() const { return IsValid(); }
		constexpr bool operator==(const ActorId&) const = default;
	};

	/// <summary>ActorWorld内でComponentを識別する実行時ID。0は無効値。</summary>
	struct ComponentId
	{
		uint64_t value = 0;

		constexpr bool IsValid() const { return value != 0; }
		constexpr explicit operator bool() const { return IsValid(); }
		constexpr bool operator==(const ComponentId&) const = default;
	};
} // namespace Ken4lowEngine
