#pragma once

#include "DX12Include.h"
#include "DirectXCommon.h"
#include "SRVManager.h"
#include "TextureManager.h"

#include <cstdint>
#include <string>

namespace Ken4lowEngine
{
	/// <summary>
	/// Scene全体で共有するEnvironment Cubemapを管理します。
	/// 通常はSkyBoxのTextureへ追従し、必要なSceneだけ明示Overrideで背景とIBLを分離できます。
	/// </summary>
	class EnvironmentMapManager
	{
	public:
		static EnvironmentMapManager* GetInstance()
		{
			static EnvironmentMapManager instance;
			return &instance;
		}

		bool SetSkyBoxEnvironment(const std::string& texturePath)
		{
			skyBoxTexturePath_ = texturePath;
			if (explicitOverrideEnabled_)
			{
				return true;
			}
			return AdoptLoadedCubeMap(texturePath);
		}

		bool SetEnvironmentMapOverride(const std::string& texturePath, bool reloadTexture = false)
		{
			if (texturePath.empty())
			{
				return UseSkyBoxEnvironment();
			}
			if (!LoadAndAdoptCubeMap(texturePath, reloadTexture))
			{
				return false;
			}
			explicitOverrideEnabled_ = true;
			return true;
		}

		bool UseSkyBoxEnvironment()
		{
			explicitOverrideEnabled_ = false;
			const std::string& path = skyBoxTexturePath_.empty() ? GetFallbackEnvironmentMapPath() : skyBoxTexturePath_;
			return LoadAndAdoptCubeMap(path, false);
		}

		D3D12_GPU_DESCRIPTOR_HANDLE GetEnvironmentMapHandle()
		{
			EnsureEnvironmentMap();
			return environmentMapHandle_;
		}

		const std::string& GetEnvironmentMapPath()
		{
			EnsureEnvironmentMap();
			return environmentMapPath_;
		}

		const std::string& GetSkyBoxTexturePath() const { return skyBoxTexturePath_; }
		bool IsUsingExplicitOverride() const { return explicitOverrideEnabled_; }
		uint64_t GetRevision() const { return revision_; }

		static const std::string& GetFallbackEnvironmentMapPath()
		{
			static const std::string path = "SkyBox/skybox.dds";
			return path;
		}

	private:
		bool EnsureEnvironmentMap()
		{
			if (environmentMapHandle_.ptr != 0)
			{
				return true;
			}
			const std::string& path = skyBoxTexturePath_.empty() ? GetFallbackEnvironmentMapPath() : skyBoxTexturePath_;
			return LoadAndAdoptCubeMap(path, false);
		}

		bool LoadAndAdoptCubeMap(const std::string& texturePath, bool reloadTexture)
		{
			if (texturePath.empty())
			{
				return false;
			}
			TextureManager* textureManager = TextureManager::GetInstance();
			try
			{
				if (reloadTexture) textureManager->ReloadTexture(texturePath);
				else textureManager->LoadTexture(texturePath);
			}
			catch (...)
			{
				return false;
			}
			return AdoptLoadedCubeMap(texturePath);
		}

		bool MirrorIntoLegacyBinding(const std::string& texturePath)
		{
			TextureManager* textureManager = TextureManager::GetInstance();
			const std::string& legacyPath = GetFallbackEnvironmentMapPath();
			textureManager->LoadTexture(legacyPath);

			const uint32_t sourceIndex = textureManager->GetSrvIndex(texturePath);
			const uint32_t legacyIndex = textureManager->GetSrvIndex(legacyPath);
			if (sourceIndex == UINT32_MAX || legacyIndex == UINT32_MAX)
			{
				return false;
			}
			if (sourceIndex == legacyIndex)
			{
				return true;
			}

			ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
			if (!device)
			{
				return false;
			}
			device->CopyDescriptorsSimple(
				1,
				SRVManager::GetInstance()->GetCPUDescriptorHandle(legacyIndex),
				SRVManager::GetInstance()->GetCPUDescriptorHandle(sourceIndex),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			return true; // 旧AnimationModelが保持するdescriptor slotも同じScene Environmentへ追従させる。
		}

		bool AdoptLoadedCubeMap(const std::string& texturePath)
		{
			if (texturePath.empty())
			{
				return false;
			}
			TextureManager* textureManager = TextureManager::GetInstance();
			try
			{
				const DirectX::TexMetadata& metadata = textureManager->GetMetaData(texturePath);
				if (!metadata.IsCubemap())
				{
					return false; // TextureCubeを要求するShaderへ2D Textureを誤Bindしない。
				}
				const D3D12_GPU_DESCRIPTOR_HANDLE handle = textureManager->GetSrvHandleGPU(texturePath);
				if (handle.ptr == 0 || !MirrorIntoLegacyBinding(texturePath))
				{
					return false;
				}
				const std::string resolvedPath = textureManager->ResolveTexturePath(texturePath);
				if (environmentMapPath_ != resolvedPath || environmentMapHandle_.ptr != handle.ptr)
				{
					environmentMapPath_ = resolvedPath;
					environmentMapHandle_ = handle;
					++revision_; // 既存Objectを作り直さず、次のDrawから新Environmentへ切り替える。
				}
				return true;
			}
			catch (...)
			{
				return false;
			}
		}

		EnvironmentMapManager() = default;

		std::string skyBoxTexturePath_{};
		std::string environmentMapPath_{};
		D3D12_GPU_DESCRIPTOR_HANDLE environmentMapHandle_{};
		uint64_t revision_ = 0;
		bool explicitOverrideEnabled_ = false;
	};
} // namespace Ken4lowEngine
