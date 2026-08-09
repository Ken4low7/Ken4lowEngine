#include "EditorTexturePreviewCache.h"

#include "DirectXCommon.h"
#include "SRVManager.h"
#include "TextureManager.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <system_error>
#include <vector>

#include <d3dx12.h>

namespace Ken4lowEngine
{
	namespace
	{
		std::string HResultToMessage(HRESULT hr)
		{
			return "Preview unavailable (HRESULT=0x" + std::to_string(static_cast<unsigned long>(hr)) + ")";
		}
	}

	EditorTexturePreviewCache::~EditorTexturePreviewCache()
	{
		Clear();
	}

	const EditorTexturePreview& EditorTexturePreviewCache::GetOrLoad(const std::filesystem::path& filePath)
	{
		std::error_code error;
		if (!std::filesystem::exists(filePath, error) || !std::filesystem::is_regular_file(filePath, error))
		{
			emptyPreview_ = {};
			emptyPreview_.failed = true;
			emptyPreview_.message = "Preview unavailable: file does not exist.";
			return emptyPreview_;
		}

		const std::string key = NormalizeKey(filePath);
		auto found = cache_.find(key);
		if (found != cache_.end())
		{
			return found->second.preview;
		}

		CachedTexture texture = LoadPreview(filePath);
		auto [inserted, _] = cache_.emplace(key, std::move(texture));
		return inserted->second.preview;
	}

	void EditorTexturePreviewCache::Clear()
	{
		const bool hasCachedResources = !cache_.empty();
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();
		if (hasCachedResources && dxCommon && dxCommon->GetCommandManager())
		{
			// プレビュー画像を参照していたGPU処理を待ってからD3D12Resource/SRVを破棄する。
			dxCommon->GetCommandManager()->ExecuteAndWait();
		}

		for (auto& [_, texture] : cache_)
		{
			if (texture.srvIndex != UINT32_MAX)
			{
				SRVManager::GetInstance()->Free(texture.srvIndex);
				texture.srvIndex = UINT32_MAX;
			}
			texture.preview = {};
			texture.resource.Reset();
		}
		cache_.clear();
		emptyPreview_ = {};
	}

	bool EditorTexturePreviewCache::IsPreviewableImage(const std::string& extension)
	{
		const std::string ext = ToLower(extension);
		return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".bmp";
	}

	EditorTexturePreviewCache::CachedTexture EditorTexturePreviewCache::LoadPreview(const std::filesystem::path& filePath) const
	{
		CachedTexture cached{};
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();
		if (!dxCommon || !dxCommon->GetDevice() || !dxCommon->GetCommandManager())
		{
			cached.preview.failed = true;
			cached.preview.message = "Preview unavailable: DirectX device is not ready.";
			return cached;
		}

		const std::string extension = ToLower(filePath.extension().string());
		DirectX::ScratchImage image{};
		DirectX::TexMetadata metadata{};
		HRESULT hr = S_OK;
		const std::wstring pathW = filePath.wstring();
		if (extension == ".dds")
		{
			hr = DirectX::LoadFromDDSFile(pathW.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, image);
		}
		else
		{
			hr = DirectX::LoadFromWICFile(pathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, &metadata, image);
		}

		if (FAILED(hr) || image.GetImageCount() == 0)
		{
			cached.preview.failed = true;
			cached.preview.message = HResultToMessage(hr) + ": " + ToUtf8Path(filePath);
			return cached;
		}

		if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || metadata.IsCubemap())
		{
			cached.preview.failed = true;
			cached.preview.width = static_cast<uint32_t>(metadata.width);
			cached.preview.height = static_cast<uint32_t>(metadata.height);
			cached.preview.message = "Preview unavailable: only Texture2D images can be displayed.";
			return cached;
		}

		cached.preview.width = static_cast<uint32_t>(metadata.width);
		cached.preview.height = static_cast<uint32_t>(metadata.height);

		try
		{
			cached.resource = TextureManager::CreateTextureResource(dxCommon->GetDevice(), metadata);
			cached.resource->SetName(L"EditorTexturePreview");
			ComPtr<ID3D12Resource> intermediate = TextureManager::UploadTextureData(
				cached.resource.Get(),
				image,
				dxCommon->GetDevice(),
				dxCommon->GetCommandManager()->GetCommandList());
			dxCommon->GetCommandManager()->ExecuteAndWait();

			cached.srvIndex = SRVManager::GetInstance()->Allocate();
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = metadata.format;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
			dxCommon->GetDevice()->CreateShaderResourceView(
				cached.resource.Get(),
				&srvDesc,
				SRVManager::GetInstance()->GetCPUDescriptorHandle(cached.srvIndex));

			cached.preview.loaded = true;
			cached.preview.srvHandleGPU = SRVManager::GetInstance()->GetGPUDescriptorHandle(cached.srvIndex);
			cached.preview.message.clear();
		}
		catch (const std::exception& e)
		{
			if (cached.srvIndex != UINT32_MAX)
			{
				SRVManager::GetInstance()->Free(cached.srvIndex);
				cached.srvIndex = UINT32_MAX;
			}
			cached.resource.Reset();
			cached.preview.loaded = false;
			cached.preview.failed = true;
			cached.preview.message = std::string("Preview unavailable: ") + e.what();
		}

		return cached;
	}

	std::string EditorTexturePreviewCache::NormalizeKey(const std::filesystem::path& filePath) const
	{
		std::error_code error;
		const std::filesystem::path absolute = std::filesystem::weakly_canonical(filePath, error);
		return ToLower(ToUtf8Path(error ? std::filesystem::absolute(filePath, error) : absolute));
	}

	std::string EditorTexturePreviewCache::ToLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});
		return value;
	}

	std::string EditorTexturePreviewCache::ToUtf8Path(const std::filesystem::path& path)
	{
		return path.generic_string();
	}

} // namespace Ken4lowEngine
