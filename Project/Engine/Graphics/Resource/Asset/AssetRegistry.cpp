#include "AssetRegistry.h"

#include <algorithm>
#include <cctype>
#include <limits>

namespace Ken4lowEngine
{
	AssetRegistry* AssetRegistry::GetInstance()
	{
		static AssetRegistry instance;
		return &instance;
	}

	AssetHandle AssetRegistry::RegisterAsset(std::string_view key, AssetType type)
	{
		if (key.empty() || type == AssetType::Unknown)
		{
			return {};
		}

		const std::string normalizedKey = NormalizeKey(key);
		const std::string lookupKey = MakeLookupKey(normalizedKey, type);
		std::scoped_lock lock(mutex_);

		if (const auto found = idByLookupKey_.find(lookupKey); found != idByLookupKey_.end())
		{
			const auto record = recordsById_.find(found->second);
			return record != recordsById_.end() ? record->second.handle : AssetHandle{};
		}

		AssetId id{ nextAssetId_++ };
		if (!id.IsValid())
		{
			// 0は無効値なので、uint64_tが周回した極端なケースでも再利用しない。
			id = AssetId{ nextAssetId_++ };
		}

		AssetRecord record{};
		record.handle = AssetHandle(id, type);
		record.key = normalizedKey;
		recordsById_.emplace(id.value, record);
		idByLookupKey_.emplace(lookupKey, id.value);
		return record.handle;
	}

	AssetHandle AssetRegistry::FindAsset(std::string_view key, AssetType type) const
	{
		if (key.empty() || type == AssetType::Unknown)
		{
			return {};
		}

		const std::string lookupKey = MakeLookupKey(NormalizeKey(key), type);
		std::scoped_lock lock(mutex_);
		const auto found = idByLookupKey_.find(lookupKey);
		if (found == idByLookupKey_.end()) return {};
		const auto record = recordsById_.find(found->second);
		return record != recordsById_.end() ? record->second.handle : AssetHandle{};
	}

	bool AssetRegistry::Contains(AssetHandle handle) const
	{
		if (!handle.IsSet()) return false;
		std::scoped_lock lock(mutex_);
		const auto found = recordsById_.find(handle.GetId().value);
		return found != recordsById_.end() && found->second.handle.GetType() == handle.GetType();
	}

	bool AssetRegistry::Acquire(AssetHandle handle, uint64_t currentFrame)
	{
		if (!handle.IsSet()) return false;
		std::scoped_lock lock(mutex_);
		const auto found = recordsById_.find(handle.GetId().value);
		if (found == recordsById_.end() || found->second.handle.GetType() != handle.GetType()) return false;
		if (found->second.referenceCount < std::numeric_limits<uint32_t>::max()) ++found->second.referenceCount;
		found->second.lastUsedFrame = currentFrame;
		return true;
	}

	bool AssetRegistry::Release(AssetHandle handle, uint64_t currentFrame)
	{
		if (!handle.IsSet()) return false;
		std::scoped_lock lock(mutex_);
		const auto found = recordsById_.find(handle.GetId().value);
		if (found == recordsById_.end() || found->second.handle.GetType() != handle.GetType()) return false;
		if (found->second.referenceCount == 0) return false;
		--found->second.referenceCount;
		found->second.lastUsedFrame = currentFrame;
		return true;
	}

	bool AssetRegistry::Touch(AssetHandle handle, uint64_t currentFrame)
	{
		if (!handle.IsSet()) return false;
		std::scoped_lock lock(mutex_);
		const auto found = recordsById_.find(handle.GetId().value);
		if (found == recordsById_.end() || found->second.handle.GetType() != handle.GetType()) return false;
		found->second.lastUsedFrame = currentFrame;
		return true;
	}

	bool AssetRegistry::SetLoadState(AssetHandle handle, AssetLoadState state, std::string_view error)
	{
		if (!handle.IsSet()) return false;
		std::scoped_lock lock(mutex_);
		const auto found = recordsById_.find(handle.GetId().value);
		if (found == recordsById_.end() || found->second.handle.GetType() != handle.GetType()) return false;
		found->second.state = state;
		found->second.lastError = std::string(error);
		return true;
	}

	bool AssetRegistry::SetGarbageCollectible(AssetHandle handle, bool enabled)
	{
		if (!handle.IsSet()) return false;
		std::scoped_lock lock(mutex_);
		const auto found = recordsById_.find(handle.GetId().value);
		if (found == recordsById_.end() || found->second.handle.GetType() != handle.GetType()) return false;
		found->second.garbageCollectible = enabled;
		return true;
	}

	bool AssetRegistry::GetSnapshot(AssetHandle handle, AssetRecordSnapshot& outSnapshot) const
	{
		if (!handle.IsSet()) return false;
		std::scoped_lock lock(mutex_);
		const auto found = recordsById_.find(handle.GetId().value);
		if (found == recordsById_.end() || found->second.handle.GetType() != handle.GetType()) return false;
		outSnapshot = MakeSnapshot(found->second);
		return true;
	}

	std::vector<AssetRecordSnapshot> AssetRegistry::GetAllSnapshots() const
	{
		std::vector<AssetRecordSnapshot> snapshots;
		std::scoped_lock lock(mutex_);
		snapshots.reserve(recordsById_.size());
		for (const auto& [id, record] : recordsById_)
		{
			(void)id;
			snapshots.push_back(MakeSnapshot(record));
		}
		return snapshots;
	}

	std::vector<AssetHandle> AssetRegistry::CollectGarbageCandidates(uint64_t currentFrame, uint64_t unusedFrameThreshold) const
	{
		std::vector<AssetHandle> candidates;
		std::scoped_lock lock(mutex_);
		for (const auto& [id, record] : recordsById_)
		{
			(void)id;
			if (!record.garbageCollectible || record.referenceCount != 0 || record.state != AssetLoadState::Loaded) continue;
			if (currentFrame < record.lastUsedFrame) continue;
			if (currentFrame - record.lastUsedFrame < unusedFrameThreshold) continue;
			candidates.push_back(record.handle);
		}
		return candidates;
	}

	void AssetRegistry::Clear()
	{
		std::scoped_lock lock(mutex_);
		recordsById_.clear();
		idByLookupKey_.clear();
		// nextAssetId_は戻さず、同一プロセス中に古いHandleが別Assetへ再利用されないようにする。
	}

	std::string AssetRegistry::NormalizeKey(std::string_view key)
	{
		std::string normalized(key);
		std::replace(normalized.begin(), normalized.end(), '\\', '/');
		while (normalized.rfind("./", 0) == 0) normalized.erase(0, 2);
		return normalized;
	}

	std::string AssetRegistry::MakeLookupKey(std::string_view key, AssetType type)
	{
		std::string lookup(key);
		std::transform(lookup.begin(), lookup.end(), lookup.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return std::to_string(static_cast<unsigned int>(type)) + ":" + lookup;
	}

	AssetRecordSnapshot AssetRegistry::MakeSnapshot(const AssetRecord& record)
	{
		AssetRecordSnapshot snapshot{};
		snapshot.handle = record.handle;
		snapshot.key = record.key;
		snapshot.state = record.state;
		snapshot.referenceCount = record.referenceCount;
		snapshot.lastUsedFrame = record.lastUsedFrame;
		snapshot.garbageCollectible = record.garbageCollectible;
		snapshot.lastError = record.lastError;
		return snapshot;
	}
} // namespace Ken4lowEngine
