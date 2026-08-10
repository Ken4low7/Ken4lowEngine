#pragma once

#include <cstdint>

namespace Ken4lowEngine
{
	/// <summary>AssetRegistry内でAssetを識別するRuntime ID。0は無効値。</summary>
	struct AssetId
	{
		uint64_t value = 0;

		constexpr bool IsValid() const { return value != 0; }
		constexpr explicit operator bool() const { return IsValid(); }
		constexpr bool operator==(const AssetId&) const = default;
	};

	/// <summary>Phase 3で共通管理するAsset種別。</summary>
	enum class AssetType : uint8_t
	{
		Unknown = 0,
		Texture,
		Model,
	};

	/// <summary>Assetの非同期ロードを含む現在状態。</summary>
	enum class AssetLoadState : uint8_t
	{
		Unloaded = 0,
		Loading,
		Loaded,
		Failed,
	};
} // namespace Ken4lowEngine
