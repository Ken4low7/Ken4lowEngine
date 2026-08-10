#include "AssetSystem.h"

#include "AssimpLoader.h"
#include "DirectXCommon.h"
#include "GpuDeferredReleaseQueue.h"
#include "ModelManager.h"
#include "SRVManager.h"
#include "TextureManager.h"

#include <chrono>
#include <exception>
#include <utility>

namespace Ken4lowEngine
{
	AssetSystem* AssetSystem::GetInstance()
	{
		static AssetSystem instance;
		return &instance;
	}

	void AssetSystem::Initialize(DirectXCommon* dxCommon)
	{
		Finalize();
		dxCommon_ = dxCommon;
		currentFrame_ = 0;
		GpuDeferredReleaseQueue::GetInstance()->Initialize(dxCommon_, SRVManager::GetInstance());
	}

	void AssetSystem::Finalize()
	{
		PumpAsyncLoads(true);
		pendingAsyncLoads_.clear();
		AssetRegistry::GetInstance()->Clear();
		GpuDeferredReleaseQueue::GetInstance()->Finalize();
		dxCommon_ = nullptr;
		currentFrame_ = 0;
	}

	void AssetSystem::Update()
	{
		++currentFrame_;
		PumpAsyncLoads(false);
		CollectGarbage();
		GpuDeferredReleaseQueue::GetInstance()->Collect();
	}

	AssetHandle AssetSystem::LoadTexture(const std::string& filePath)
	{
		const std::string key = TextureManager::GetInstance()->ResolveTexturePath(filePath);
		AssetHandle handle = RegisterAndAcquire(key, AssetType::Texture);
		if (!handle) return {};

		AssetRecordSnapshot snapshot{};
		if (AssetRegistry::GetInstance()->GetSnapshot(handle, snapshot) && snapshot.state == AssetLoadState::Loaded)
		{
			return handle;
		}

		try
		{
			TextureManager::GetInstance()->LoadTexture(key);
			AssetRegistry::GetInstance()->SetLoadState(handle, AssetLoadState::Loaded);
		}
		catch (const std::exception& exception)
		{
			AssetRegistry::GetInstance()->SetLoadState(handle, AssetLoadState::Failed, exception.what());
		}
		catch (...)
		{
			AssetRegistry::GetInstance()->SetLoadState(handle, AssetLoadState::Failed, "Texture load failed with unknown error.");
		}
		return handle;
	}

	AssetHandle AssetSystem::LoadModel(const std::string& filePath)
	{
		AssetHandle handle = RegisterAndAcquire(filePath, AssetType::Model);
		if (!handle) return {};

		AssetRecordSnapshot snapshot{};
		if (AssetRegistry::GetInstance()->GetSnapshot(handle, snapshot) && snapshot.state == AssetLoadState::Loaded)
		{
			return handle;
		}

		try
		{
			ModelManager::GetInstance()->LoadModel(filePath);
			AssetRegistry::GetInstance()->SetLoadState(handle, AssetLoadState::Loaded);
		}
		catch (const std::exception& exception)
		{
			AssetRegistry::GetInstance()->SetLoadState(handle, AssetLoadState::Failed, exception.what());
		}
		catch (...)
		{
			AssetRegistry::GetInstance()->SetLoadState(handle, AssetLoadState::Failed, "Model load failed with unknown error.");
		}
		return handle;
	}

	AssetHandle AssetSystem::RequestTextureAsync(const std::string& filePath)
	{
		const std::string key = TextureManager::GetInstance()->ResolveTexturePath(filePath);
		AssetHandle handle = RegisterAndAcquire(key, AssetType::Texture);
		if (!handle) return {};

		AssetRecordSnapshot snapshot{};
		if (!AssetRegistry::GetInstance()->GetSnapshot(handle, snapshot)) return {};
		if (snapshot.state == AssetLoadState::Loaded || snapshot.state == AssetLoadState::Loading) return handle;

		AssetRegistry::GetInstance()->SetLoadState(handle, AssetLoadState::Loading);
		PendingAsyncLoad pending{};
		pending.handle = handle;
		pending.future = std::async(std::launch::async, [handle, key]()
			{
				AsyncLoadResult result{};
				result.handle = handle;
				result.type = AssetType::Texture;
				result.key = key;
				try
				{
					// File decodeだけをWorkerで行い、D3D12 Resource / SRV生成はMain Threadへ戻す。
					result.textureData = std::make_shared<DirectX::ScratchImage>(TextureManager::LoadTextureData(key));
				}
				catch (const std::exception& exception)
				{
					result.error = exception.what();
				}
				catch (...)
				{
					result.error = "Texture async load failed with unknown error.";
				}
				return result;
			});
		pendingAsyncLoads_.push_back(std::move(pending));
		return handle;
	}

	AssetHandle AssetSystem::RequestModelAsync(const std::string& filePath)
	{
		AssetHandle handle = RegisterAndAcquire(filePath, AssetType::Model);
		if (!handle) return {};

		AssetRecordSnapshot snapshot{};
		if (!AssetRegistry::GetInstance()->GetSnapshot(handle, snapshot)) return {};
		if (snapshot.state == AssetLoadState::Loaded || snapshot.state == AssetLoadState::Loading) return handle;

		AssetRegistry::GetInstance()->SetLoadState(handle, AssetLoadState::Loading);
		PendingAsyncLoad pending{};
		pending.handle = handle;
		pending.future = std::async(std::launch::async, [handle, filePath]()
			{
				AsyncLoadResult result{};
				result.handle = handle;
				result.type = AssetType::Model;
				result.key = filePath;
				try
				{
					// Assimp ImportはCPU処理なのでWorkerで完了させ、MeshのGPU生成だけMain Threadへ戻す。
					result.modelData = std::make_shared<ModelData>(AssimpLoader::LoadModel(filePath));
				}
				catch (const std::exception& exception)
				{
					result.error = exception.what();
				}
				catch (...)
				{
					result.error = "Model async load failed with unknown error.";
				}
				return result;
			});
		pendingAsyncLoads_.push_back(std::move(pending));
		return handle;
	}

	bool AssetSystem::Acquire(AssetHandle handle)
	{
		return AssetRegistry::GetInstance()->Acquire(handle, currentFrame_);
	}

	bool AssetSystem::Release(AssetHandle handle)
	{
		return AssetRegistry::GetInstance()->Release(handle, currentFrame_);
	}

	bool AssetSystem::SetGarbageCollectible(AssetHandle handle, bool enabled)
	{
		return AssetRegistry::GetInstance()->SetGarbageCollectible(handle, enabled);
	}

	bool AssetSystem::IsLoaded(AssetHandle handle) const
	{
		AssetRecordSnapshot snapshot{};
		return AssetRegistry::GetInstance()->GetSnapshot(handle, snapshot) && snapshot.state == AssetLoadState::Loaded;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE AssetSystem::ResolveTexture(AssetHandle handle)
	{
		AssetRecordSnapshot snapshot{};
		if (handle.GetType() != AssetType::Texture ||
			!AssetRegistry::GetInstance()->GetSnapshot(handle, snapshot) ||
			snapshot.state != AssetLoadState::Loaded)
		{
			return {};
		}

		AssetRegistry::GetInstance()->Touch(handle, currentFrame_);
		return TextureManager::GetInstance()->GetSrvHandleGPU(snapshot.key);
	}

	std::shared_ptr<Model> AssetSystem::ResolveModel(AssetHandle handle)
	{
		AssetRecordSnapshot snapshot{};
		if (handle.GetType() != AssetType::Model ||
			!AssetRegistry::GetInstance()->GetSnapshot(handle, snapshot) ||
			snapshot.state != AssetLoadState::Loaded)
		{
			return nullptr;
		}

		AssetRegistry::GetInstance()->Touch(handle, currentFrame_);
		return ModelManager::GetInstance()->GetLoadedModel(snapshot.key);
	}

	AssetHandle AssetSystem::RegisterAndAcquire(const std::string& key, AssetType type)
	{
		AssetHandle handle = AssetRegistry::GetInstance()->RegisterAsset(key, type);
		if (!handle) return {};
		AssetRegistry::GetInstance()->Acquire(handle, currentFrame_);
		return handle;
	}

	void AssetSystem::PumpAsyncLoads(bool waitForAll)
	{
		for (size_t index = 0; index < pendingAsyncLoads_.size();)
		{
			PendingAsyncLoad& pending = pendingAsyncLoads_[index];
			if (waitForAll) pending.future.wait();
			if (!waitForAll && pending.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			{
				++index;
				continue;
			}

			ApplyAsyncResult(pending.future.get());
			pendingAsyncLoads_.erase(pendingAsyncLoads_.begin() + static_cast<std::ptrdiff_t>(index));
		}
	}

	void AssetSystem::ApplyAsyncResult(AsyncLoadResult result)
	{
		if (!result.error.empty())
		{
			AssetRegistry::GetInstance()->SetLoadState(result.handle, AssetLoadState::Failed, result.error);
			return;
		}

		try
		{
			if (result.type == AssetType::Texture && result.textureData)
			{
				TextureManager::GetInstance()->LoadTextureFromData(result.key, std::move(*result.textureData));
			}
			else if (result.type == AssetType::Model && result.modelData)
			{
				ModelManager::GetInstance()->LoadModelFromData(result.key, std::move(*result.modelData));
			}
			else
			{
				AssetRegistry::GetInstance()->SetLoadState(result.handle, AssetLoadState::Failed, "Async result did not contain payload.");
				return;
			}
			AssetRegistry::GetInstance()->SetLoadState(result.handle, AssetLoadState::Loaded);
		}
		catch (const std::exception& exception)
		{
			AssetRegistry::GetInstance()->SetLoadState(result.handle, AssetLoadState::Failed, exception.what());
		}
		catch (...)
		{
			AssetRegistry::GetInstance()->SetLoadState(result.handle, AssetLoadState::Failed, "Async GPU finalize failed with unknown error.");
		}
	}

	void AssetSystem::CollectGarbage()
	{
		const std::vector<AssetHandle> candidates = AssetRegistry::GetInstance()->CollectGarbageCandidates(
			currentFrame_, garbageCollectionDelayFrames_);

		for (AssetHandle handle : candidates)
		{
			AssetRecordSnapshot snapshot{};
			if (!AssetRegistry::GetInstance()->GetSnapshot(handle, snapshot)) continue;

			bool unloaded = false;
			if (handle.GetType() == AssetType::Texture)
			{
				unloaded = TextureManager::GetInstance()->UnloadTexture(snapshot.key, true);
			}
			else if (handle.GetType() == AssetType::Model)
			{
				unloaded = ModelManager::GetInstance()->UnloadModel(snapshot.key, true);
			}

			if (unloaded)
			{
				AssetRegistry::GetInstance()->SetLoadState(handle, AssetLoadState::Unloaded);
			}
			else
			{
				// legacy shared_ptr等がまだModelを保持している場合は次のGC判定を少し先送りする。
				AssetRegistry::GetInstance()->Touch(handle, currentFrame_);
			}
		}
	}
} // namespace Ken4lowEngine
