#pragma once

#include "DX12Include.h"
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
			if (!IsLoadedCubeMap(texturePath))
			{
				return false;
			}
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
			const std::string& path = skyBoxTexturePath_.empty() ? GetFallbackEnvironmentMapPath() : skyBoxTexturePath_;
			if (!LoadAndAdoptCubeMap(path, false))
			{
				return false;
			}
			explicitOverrideEnabled_ = false; // 復帰成功後だけOverride状態を落とし、失敗時は現在のEnvironmentを維持する。
			return true;
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

		bool IsLoadedCubeMap(const std::string& texturePath)
		{
			if (texturePath.empty())
			{
				return false;
			}
			try
			{
				return TextureManager::GetInstance()->GetMetaData(texturePath).IsCubemap();
			}
			catch (...)
			{
				return false;
			}
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
				if (handle.ptr == 0)
				{
					return false;
				}
				const std::string resolvedPath = textureManager->ResolveTexturePath(texturePath);
				if (environmentMapPath_ != resolvedPath || environmentMapHandle_.ptr != handle.ptr)
				{
					environmentMapPath_ = resolvedPath;
					environmentMapHandle_ = handle;
					++revision_; // RendererはDraw時に現在Handleを取得するので、Frames in Flight中のdescriptor書き換えは行わない。
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
