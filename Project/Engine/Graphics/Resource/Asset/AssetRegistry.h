#pragma once

#include "AssetHandle.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Ken4lowEngine
{
	struct AssetRecordSnapshot
	{
		AssetHandle handle{};
		std::string key;
		AssetLoadState state = AssetLoadState::Unloaded;
		uint32_t referenceCount = 0;
		uint64_t lastUsedFrame = 0;
		bool garbageCollectible = false;
		std::string lastError;
	};

	/// <summary>
	/// Texture / Modelを共通AssetIdで索引化し、参照数・Load状態・GC判定を一元管理する。
	/// Payload自体の所有は各Managerへ残し、Phase 3では既存描画コードとの互換性を維持する。
	/// </summary>
	class AssetRegistry
	{
	public:
		static AssetRegistry* GetInstance();

		AssetHandle RegisterAsset(std::string_view key, AssetType type);
		AssetHandle FindAsset(std::string_view key, AssetType type) const;
		bool Contains(AssetHandle handle) const;

		bool Acquire(AssetHandle handle, uint64_t currentFrame);
		bool Release(AssetHandle handle, uint64_t currentFrame);
		bool Touch(AssetHandle handle, uint64_t currentFrame);

		bool SetLoadState(AssetHandle handle, AssetLoadState state, std::string_view error = {});
		bool SetGarbageCollectible(AssetHandle handle, bool enabled);
		bool GetSnapshot(AssetHandle handle, AssetRecordSnapshot& outSnapshot) const;
		std::vector<AssetRecordSnapshot> GetAllSnapshots() const;
		std::vector<AssetHandle> CollectGarbageCandidates(uint64_t currentFrame, uint64_t unusedFrameThreshold) const;

		void Clear();

	private:
		struct AssetRecord
		{
			AssetHandle handle{};
			std::string key;
			AssetLoadState state = AssetLoadState::Unloaded;
			uint32_t referenceCount = 0;
			uint64_t lastUsedFrame = 0;
			bool garbageCollectible = false;
			std::string lastError;
		};

		static std::string NormalizeKey(std::string_view key);
		static std::string MakeLookupKey(std::string_view key, AssetType type);
		static AssetRecordSnapshot MakeSnapshot(const AssetRecord& record);

	private:
		mutable std::mutex mutex_;
		std::unordered_map<uint64_t, AssetRecord> recordsById_;
		std::unordered_map<std::string, uint64_t> idByLookupKey_;
		uint64_t nextAssetId_ = 1;

	private:
		AssetRegistry() = default;
		~AssetRegistry() = default;
		AssetRegistry(const AssetRegistry&) = delete;
		AssetRegistry& operator=(const AssetRegistry&) = delete;
	};
} // namespace Ken4lowEngine
