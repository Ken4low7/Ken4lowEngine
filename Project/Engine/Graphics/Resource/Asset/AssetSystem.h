#pragma once

#include "AssetRegistry.h"
#include "DX12Include.h"
#include "ModelData.h"

#include <DirectXTex.h>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class DirectXCommon;
	class Model;

	/// <summary>
	/// Texture / ModelをAssetHandleで統一して扱うPhase 3の入口。
	/// CPU側のFile Decode / Model ImportはWorkerへ逃がし、D3D12 Resource生成はMain ThreadのUpdateで確定する。
	/// </summary>
	class AssetSystem
	{
	public:
		static AssetSystem* GetInstance();

		void Initialize(DirectXCommon* dxCommon);
		void Finalize();

		/// 1フレーム終端でAsync完了反映、GC、GPU Deferred Delete回収を行う。
		void Update();

		AssetHandle LoadTexture(const std::string& filePath);
		AssetHandle LoadModel(const std::string& filePath);
		AssetHandle RequestTextureAsync(const std::string& filePath);
		AssetHandle RequestModelAsync(const std::string& filePath);

		bool Acquire(AssetHandle handle);
		bool Release(AssetHandle handle);
		bool SetGarbageCollectible(AssetHandle handle, bool enabled);
		bool IsLoaded(AssetHandle handle) const;

		D3D12_GPU_DESCRIPTOR_HANDLE ResolveTexture(AssetHandle handle);
		std::shared_ptr<Model> ResolveModel(AssetHandle handle);

		void SetGarbageCollectionDelayFrames(uint64_t frames) { garbageCollectionDelayFrames_ = frames; }
		uint64_t GetCurrentFrame() const { return currentFrame_; }
		size_t GetPendingAsyncCount() const { return pendingAsyncLoads_.size(); }

	private:
		struct AsyncLoadResult
		{
			AssetHandle handle{};
			AssetType type = AssetType::Unknown;
			std::string key;
			std::shared_ptr<DirectX::ScratchImage> textureData;
			std::shared_ptr<ModelData> modelData;
			std::string error;
		};

		struct PendingAsyncLoad
		{
			AssetHandle handle{};
			std::future<AsyncLoadResult> future;
		};

		AssetHandle RegisterAndAcquire(const std::string& key, AssetType type);
		void PumpAsyncLoads(bool waitForAll = false);
		void ApplyAsyncResult(AsyncLoadResult result);
		void CollectGarbage();

	private:
		DirectXCommon* dxCommon_ = nullptr;
		uint64_t currentFrame_ = 0;
		uint64_t garbageCollectionDelayFrames_ = 180;
		std::vector<PendingAsyncLoad> pendingAsyncLoads_;

	private:
		AssetSystem() = default;
		~AssetSystem() = default;
		AssetSystem(const AssetSystem&) = delete;
		AssetSystem& operator=(const AssetSystem&) = delete;
	};
} // namespace Ken4lowEngine
