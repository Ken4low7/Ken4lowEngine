#define NOMINMAX
#include "TextureManager.h"

#include "DirectXCommon.h"
#include "SRVManager.h"
#include "ResourceManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>

#include <d3dx12.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace Ken4lowEngine
{

	TextureManager* TextureManager::GetInstance()
	{
		static TextureManager instance;
		return &instance;
	}

	void TextureManager::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;

		// Compiled 配下の .dds 一覧を先に索引化
		BuildTexturePathIndex();

		LoadTexture("Sample/uvChecker.dds");
	}

	void TextureManager::Finalize()
	{
		for (auto& [path, tex] : textureDatas)
		{
			if (tex.srvIndex != UINT32_MAX)
			{
				SRVManager::GetInstance()->Free(tex.srvIndex);
				tex.srvIndex = UINT32_MAX;
			}
			tex.resource.Reset();
			tex.srvHandleCPU = {};
			tex.srvHandleGPU = {};
		}

		textureDatas.clear();
		texturePathIndex_.clear();
		dxCommon_ = nullptr;
	}

	ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata)
	{
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Width = UINT(metadata.width);
		resourceDesc.Height = UINT(metadata.height);
		resourceDesc.MipLevels = UINT16(metadata.mipLevels);
		resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
		resourceDesc.Format = metadata.format;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

		ComPtr<ID3D12Resource> resource = nullptr;
		HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource));
		assert(SUCCEEDED(hr));

		return resource;
	}

	[[nodiscard]]
	ComPtr<ID3D12Resource> TextureManager::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
	{
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);

		uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
		ComPtr<ID3D12Resource> intermediateResource = ResourceManager::CreateBufferResource(device, intermediateSize);

		UpdateSubresources(commandList, texture, intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = texture;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;

		commandList->ResourceBarrier(1, &barrier);

		return intermediateResource;
	}

	DirectX::ScratchImage TextureManager::LoadTextureData(const std::string& filePath)
	{
		DirectX::ScratchImage image{};
		std::wstring filePathW = ConvertString(filePath);

		HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
		assert(SUCCEEDED(hr));

		return image;
	}

	void TextureManager::LoadTexture(const std::string& filePath)
	{
		HRESULT hr{};

		std::string filePathStr = NormalizeTexturePath(filePath);

		if (textureDatas.contains(filePathStr))
		{
			return;
		}

		DirectX::ScratchImage image{};
		std::wstring filePathW = ConvertString(filePathStr);

		const bool isDDS = filePathW.ends_with(L".dds");

		if (isDDS)
		{
			hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
			assert(SUCCEEDED(hr));
		}
		else
		{
			hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
			assert(SUCCEEDED(hr));
		}

		const auto& meta0 = image.GetMetadata();
#ifdef _DEBUG
		Log(std::format("[TextureManager] Loaded texture={} source={} format={} size={}x{} mipLevels={} srgb={} alphaMode={}\n",
			filePathStr, isDDS ? "DDS" : "WIC", static_cast<int>(meta0.format),
			meta0.width, meta0.height, meta0.mipLevels,
			DirectX::IsSRGB(meta0.format) ? "true" : "false", static_cast<int>(meta0.GetAlphaMode())));
#endif
		const bool isLegacyTall = (!isDDS) && (meta0.width == meta0.height * 2);

		DirectX::ScratchImage normalized{};
		const DirectX::ScratchImage* uploadImage = &image;

		if (isLegacyTall)
		{
			DirectX::TexMetadata metaData0 = meta0;
			metaData0.height = meta0.width;
			metaData0.mipLevels = 1;
			metaData0.arraySize = 1;

			hr = normalized.Initialize2D(metaData0.format, metaData0.width, metaData0.height, metaData0.arraySize, metaData0.mipLevels);
			assert(SUCCEEDED(hr));

			const DirectX::Image* srcImage = image.GetImage(0, 0, 0);
			const DirectX::Image* destImage = normalized.GetImage(0, 0, 0);

			DirectX::Rect srcRect = {
				0, 0,
				static_cast<size_t>(srcImage->width),
				static_cast<size_t>(srcImage->height)
			};

			hr = DirectX::CopyRectangle(*srcImage, srcRect, *destImage, DirectX::TEX_FILTER_DEFAULT, 0, UINT(srcImage->height));
			assert(SUCCEEDED(hr));

			uploadImage = &normalized;
		}

		TextureData& textureData = textureDatas[filePathStr];
		textureData.metaData = uploadImage->GetMetadata();
#ifdef _DEBUG
		// DDS 読み込み後の最終メタデータを出力し、sRGB/通常フォーマット取り違えを切り分ける。
		Log(std::format("[TextureManager] Upload texture={} format={} mipLevels={} srgb={}\n",
			filePathStr, static_cast<int>(textureData.metaData.format), textureData.metaData.mipLevels,
			DirectX::IsSRGB(textureData.metaData.format) ? "true" : "false"));
#endif
		textureData.resource = CreateTextureResource(dxCommon_->GetDevice(), textureData.metaData);
		textureData.resource->SetName(L"TextureResource");

		ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(textureData.resource.Get(), *uploadImage, dxCommon_->GetDevice(), dxCommon_->GetCommandManager()->GetCommandList());

		dxCommon_->GetCommandManager()->ExecuteAndWait();

		textureData.srvIndex = SRVManager::GetInstance()->Allocate();
		textureData.srvHandleCPU = SRVManager::GetInstance()->GetCPUDescriptorHandle(textureData.srvIndex);
		textureData.srvHandleGPU = SRVManager::GetInstance()->GetGPUDescriptorHandle(textureData.srvIndex);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = textureData.metaData.format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		if (textureData.metaData.IsCubemap())
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.MipLevels = UINT_MAX;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = UINT(textureData.metaData.mipLevels);
		}

		dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);
	}

	void TextureManager::ReloadTexture(const std::string& filePath)
	{
		std::string key = NormalizeTexturePath(filePath);

		auto it = textureDatas.find(key);
		if (it != textureDatas.end())
		{
			if (it->second.srvIndex != UINT32_MAX)
			{
				SRVManager::GetInstance()->Free(it->second.srvIndex);
				it->second.srvIndex = UINT32_MAX;
			}
			it->second.resource.Reset();
			textureDatas.erase(it);
		}

		LoadTexture(key);
	}

	void TextureManager::CreateSolidColorTexture(const std::string& key, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint32_t width, uint32_t height)
	{
		std::string filePathStr = NormalizeTexturePath(key);

		if (textureDatas.contains(filePathStr))	return;


		DirectX::ScratchImage baseImage{};
		HRESULT hr = baseImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, width, height, 1, 1);
		assert(SUCCEEDED(hr));

		auto img = baseImage.GetImage(0, 0, 0);
		for (uint32_t y = 0; y < height; ++y)
		{
			uint8_t* row = img->pixels + y * img->rowPitch;
			for (uint32_t x = 0; x < width; ++x)
			{
				row[x * 4 + 0] = r;
				row[x * 4 + 1] = g;
				row[x * 4 + 2] = b;
				row[x * 4 + 3] = a;
			}
		}

		TextureData& textureData = textureDatas[filePathStr];
		textureData.metaData = baseImage.GetMetadata();
		textureData.resource = CreateTextureResource(dxCommon_->GetDevice(), textureData.metaData);

		ComPtr<ID3D12Resource> intermediateResource = UploadTextureData(textureData.resource.Get(), baseImage, dxCommon_->GetDevice(), dxCommon_->GetCommandManager()->GetCommandList());

		dxCommon_->GetCommandManager()->ExecuteAndWait();

		textureData.srvIndex = SRVManager::GetInstance()->Allocate();
		textureData.srvHandleCPU = SRVManager::GetInstance()->GetCPUDescriptorHandle(textureData.srvIndex);
		textureData.srvHandleGPU = SRVManager::GetInstance()->GetGPUDescriptorHandle(textureData.srvIndex);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = textureData.metaData.format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(textureData.metaData.mipLevels);

		dxCommon_->GetDevice()->CreateShaderResourceView(
			textureData.resource.Get(),
			&srvDesc,
			textureData.srvHandleCPU);
	}

	void TextureManager::SetGraphicsRootDescriptorTable(ID3D12GraphicsCommandList* commandList, UINT rootParameter, D3D12_GPU_DESCRIPTOR_HANDLE textureSRVHandleGPU)
	{
		commandList->SetGraphicsRootDescriptorTable(rootParameter, textureSRVHandleGPU);
	}

	uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
	{
		std::string key = NormalizeTexturePath(filePath);

		auto it = textureDatas.find(key);
		if (it != textureDatas.end())
		{
			return it->second.srvIndex;
		}

		throw std::runtime_error("Texture not found: " + key);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath)
	{
		std::string key = NormalizeTexturePath(filePath);

		auto it = textureDatas.find(key);
		if (it == textureDatas.end())
		{
			LoadTexture(filePath);
			it = textureDatas.find(key);
		}

		assert(it != textureDatas.end());
		return it->second.srvHandleGPU;
	}

	uint32_t TextureManager::GetSrvIndex(const std::string& filePath)
	{
		std::string key = NormalizeTexturePath(filePath);

		auto it = textureDatas.find(key);
		if (it == textureDatas.end())
		{
			LoadTexture(filePath);
			it = textureDatas.find(key);
		}

		assert(it != textureDatas.end());
		return it->second.srvIndex;
	}

	const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
	{
		std::string filePathStr = NormalizeTexturePath(filePath);

		assert(textureDatas.find(filePathStr) != textureDatas.end());

		TextureData& textureData = textureDatas[filePathStr];
		return textureData.metaData;
	}

	ID3D12Resource* TextureManager::GetResource(const std::string& filePath)
	{
		std::string filePathStr = NormalizeTexturePath(filePath);
		auto it = textureDatas.find(filePathStr);
		assert(it != textureDatas.end());
		return it->second.resource.Get();
	}

	void TextureManager::BuildTexturePathIndex()
	{
		texturePathIndex_.clear();

		const std::filesystem::path root = kTextureRootDir;
		if (!std::filesystem::exists(root))
		{
			return;
		}

		for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
		{
			if (!entry.is_regular_file()) continue;

			const std::filesystem::path path = entry.path();
			if (!path.has_extension()) continue;

			if (ToLowerString(path.extension().generic_string()) != ".dds")	continue;

			const std::string fullPath = NormalizeSlashes(path.generic_string());
			const std::string fileName = ToLowerString(path.filename().generic_string());
			const std::string relativePath = ToLowerString(NormalizeSlashes(std::filesystem::relative(path, root).generic_string()));

			texturePathIndex_[fileName].push_back(fullPath);
			texturePathIndex_[relativePath].push_back(fullPath);
		}
	}

	std::string TextureManager::FindCompiledTexturePath(const std::string& query) const
	{
		if (query.empty()) return "";

		std::string normalized = ToLowerString(NormalizeSlashes(query));

		// 1. そのまま検索
		auto it = texturePathIndex_.find(normalized);
		if (it != texturePathIndex_.end() && !it->second.empty())
		{
			return it->second.front();
		}

		// 2. ファイル名だけで検索
		std::filesystem::path p(normalized);
		const std::string fileName = ToLowerString(p.filename().generic_string());

		it = texturePathIndex_.find(fileName);
		if (it != texturePathIndex_.end() && !it->second.empty())
		{
			return it->second.front();
		}

		// 3. query が Compiled フルパス風なら root 以下相対にして再検索
		const std::string compiledRoot = ToLowerString(NormalizeSlashes(kTextureRootDir));
		if (normalized.rfind(compiledRoot + "/", 0) == 0)
		{
			const std::string relative = normalized.substr(compiledRoot.size() + 1);
			it = texturePathIndex_.find(relative);
			if (it != texturePathIndex_.end() && !it->second.empty())
			{
				return it->second.front();
			}
		}

		return "";
	}

	std::string TextureManager::NormalizeTexturePath(const std::string& filePath)
	{
		std::string path = NormalizeSlashes(filePath);

		while (path.rfind("./", 0) == 0)
		{
			path.erase(0, 2);
		}

		if (path.size() >= 2 &&
			std::isalpha(static_cast<unsigned char>(path[0])) &&
			path[1] == ':')
		{
			return path;
		}

		const std::string compiledRoot = NormalizeSlashes(std::string(kTextureRootDir));

		if (path.rfind(compiledRoot, 0) == 0)
		{
			return path;
		}

		const std::string oldRoot = "Resources/Textures/";
		if (path.rfind(oldRoot, 0) == 0)
		{
			std::string sub = path.substr(oldRoot.size());

			if (sub.rfind("Compiled/", 0) == 0)
			{
				return oldRoot + sub;
			}

			return compiledRoot + "/" + sub;
		}

		// Compiled 内索引から実体を探す
		std::string found = FindCompiledTexturePath(path);
		if (!found.empty())
		{
			return found;
		}

		return compiledRoot + "/" + path;
	}

	std::string TextureManager::NormalizeSlashes(std::string path)
	{
		std::replace(path.begin(), path.end(), '\\', '/');
		return path;
	}

	std::string TextureManager::ToLowerString(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	}

} // namespace Ken4lowEngine