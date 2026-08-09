#pragma once

#include "DX12Include.h"

#include <DirectXTex.h>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{
	class DirectXCommon;

	struct EditorTexturePreview
	{
		bool loaded = false;
		bool failed = false;
		std::string message;
		uint32_t width = 0;
		uint32_t height = 0;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU{};
	};

	class EditorTexturePreviewCache
	{
	public:
		~EditorTexturePreviewCache();

		const EditorTexturePreview& GetOrLoad(const std::filesystem::path& filePath);
		void Clear();

		static bool IsPreviewableImage(const std::string& extension);

	private:
		struct CachedTexture
		{
			EditorTexturePreview preview{};
			ComPtr<ID3D12Resource> resource;
			uint32_t srvIndex = UINT32_MAX;
		};

		CachedTexture LoadPreview(const std::filesystem::path& filePath) const;
		std::string NormalizeKey(const std::filesystem::path& filePath) const;
		static std::string ToLower(std::string value);
		static std::string ToUtf8Path(const std::filesystem::path& path);

		std::unordered_map<std::string, CachedTexture> cache_;
		EditorTexturePreview emptyPreview_{};
	};

} // namespace Ken4lowEngine
