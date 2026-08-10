#pragma once

#include "AssetId.h"

namespace Ken4lowEngine
{
	/// <summary>
	/// Asset本体を所有せず、AssetIdとAssetTypeからAssetRegistryへ再解決する軽量Handle。
	/// Unload後もID値は再利用しないため、古いHandleが別Assetへ化けない。
	/// </summary>
	class AssetHandle
	{
	public:
		constexpr AssetHandle() = default;
		constexpr AssetHandle(AssetId id, AssetType type) : id_(id), type_(type) {}

		constexpr AssetId GetId() const { return id_; }
		constexpr AssetType GetType() const { return type_; }
		constexpr bool IsSet() const { return id_.IsValid() && type_ != AssetType::Unknown; }
		constexpr void Reset() { id_ = {}; type_ = AssetType::Unknown; }
		constexpr explicit operator bool() const { return IsSet(); }
		constexpr bool operator==(const AssetHandle&) const = default;

	private:
		AssetId id_{};
		AssetType type_ = AssetType::Unknown;
	};
} // namespace Ken4lowEngine
