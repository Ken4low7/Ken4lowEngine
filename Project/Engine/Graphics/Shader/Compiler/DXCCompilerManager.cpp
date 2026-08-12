#include "DXCCompilerManager.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_set>
#include <vector>

namespace Ken4lowEngine
{
	namespace
	{
		std::string WideToUtf8(std::wstring_view value)
		{
			if (value.empty()) return {};
			const int required = WideCharToMultiByte(
				CP_UTF8,
				WC_ERR_INVALID_CHARS,
				value.data(),
				static_cast<int>(value.size()),
				nullptr,
				0,
				nullptr,
				nullptr);
			if (required <= 0) return {};

			std::string result(static_cast<std::size_t>(required), '\0');
			WideCharToMultiByte(
				CP_UTF8,
				WC_ERR_INVALID_CHARS,
				value.data(),
				static_cast<int>(value.size()),
				result.data(),
				required,
				nullptr,
				nullptr);
			return result;
		}

		std::wstring NormalizeWidePath(std::wstring_view value)
		{
			if (value.empty()) return {};
			return std::filesystem::path(value).lexically_normal().generic_wstring();
		}

		void AppendKeyField(std::string& key, std::string_view value)
		{
			key.append(std::to_string(value.size()));
			key.push_back(':');
			key.append(value.data(), value.size());
			key.push_back('|');
		}

		bool ReadFileBytes(const std::filesystem::path& path, std::string& outBytes)
		{
			std::ifstream stream(path, std::ios::binary);
			if (!stream) return false;
			outBytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
			return true;
		}

		std::vector<std::string> ExtractIncludeTokens(std::string_view source)
		{
			std::vector<std::string> includes;
			std::size_t lineBegin = 0;
			while (lineBegin < source.size())
			{
				const std::size_t lineEnd = source.find('\n', lineBegin);
				const std::size_t length = (lineEnd == std::string_view::npos ? source.size() : lineEnd) - lineBegin;
				std::string_view line = source.substr(lineBegin, length);

				std::size_t cursor = 0;
				while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) ++cursor;
				if (cursor < line.size() && line[cursor] == '#')
				{
					++cursor;
					while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) ++cursor;
					constexpr std::string_view includeKeyword = "include";
					if (line.substr(cursor, includeKeyword.size()) == includeKeyword)
					{
						cursor += includeKeyword.size();
						while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) ++cursor;
						if (cursor < line.size() && (line[cursor] == '"' || line[cursor] == '<'))
						{
							const char close = line[cursor] == '"' ? '"' : '>';
							const std::size_t tokenBegin = ++cursor;
							const std::size_t tokenEnd = line.find(close, tokenBegin);
							if (tokenEnd != std::string_view::npos && tokenEnd > tokenBegin)
							{
								includes.emplace_back(line.substr(tokenBegin, tokenEnd - tokenBegin));
							}
						}
					}
				}

				if (lineEnd == std::string_view::npos) break;
				lineBegin = lineEnd + 1;
			}
			return includes;
		}

		void AppendShaderSourceTree(
			const std::filesystem::path& physicalPath,
			std::string logicalPath,
			std::unordered_set<std::wstring>& visited,
			std::string& key)
		{
			std::error_code error;
			std::filesystem::path visitPath = std::filesystem::absolute(physicalPath, error);
			if (error) visitPath = physicalPath;
			visitPath = visitPath.lexically_normal();
			const std::wstring visitKey = visitPath.wstring();

			AppendKeyField(key, logicalPath);
			if (!visited.insert(visitKey).second)
			{
				AppendKeyField(key, "<include-cycle-or-repeat>");
				return;
			}

			std::string source;
			if (!ReadFileBytes(physicalPath, source))
			{
				AppendKeyField(key, "<missing-source>");
				return;
			}

			AppendKeyField(key, source);
			for (const std::string& includeToken : ExtractIncludeTokens(source))
			{
				const std::filesystem::path includeRelative(includeToken);
				const std::filesystem::path includePhysical = (physicalPath.parent_path() / includeRelative).lexically_normal();
				const std::filesystem::path logicalParent = std::filesystem::path(logicalPath).parent_path();
				const std::string includeLogical = (logicalParent / includeRelative).lexically_normal().generic_string();

				// Local includeの内容もKeyへ展開し、.hlsli変更時に親ShaderのCacheを自動的に外す。
				AppendShaderSourceTree(includePhysical, includeLogical, visited, key);
			}
		}
	}

/// -------------------------------------------------------------
///					        初期化処理
/// -------------------------------------------------------------
void DXCCompilerManager::Initialize()
{
	{
		std::scoped_lock lock(shaderCacheMutex_);
		shaderCache_.clear();
		shaderCacheStats_ = {};
	}

	// IDxcUtilsの生成
	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr));

	// IDxcCompiler3の生成
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr));

	// IDxcIncludeHandlerの生成
	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr));
}

void DXCCompilerManager::Finalize()
{
	ClearShaderCache();
	dxcUtils_.Reset();
	dxcCompiler_.Reset();
	includeHandler_.Reset();
}

std::string DXCCompilerManager::BuildShaderCacheKey(
	const wchar_t* filePath,
	const wchar_t* entryPoint,
	const wchar_t* profile) const
{
	std::string key;
	AppendKeyField(key, "Ken4lowShaderCache-v1");
	const std::wstring normalizedPath = NormalizeWidePath(filePath ? std::wstring_view(filePath) : std::wstring_view{});
	AppendKeyField(key, WideToUtf8(normalizedPath));
	AppendKeyField(key, WideToUtf8(entryPoint ? std::wstring_view(entryPoint) : std::wstring_view{}));
	AppendKeyField(key, WideToUtf8(profile ? std::wstring_view(profile) : std::wstring_view{}));
	AppendKeyField(key, "-Zi|-Qembed_debug|-Od|-Zpr");

	std::unordered_set<std::wstring> visited;
	AppendShaderSourceTree(
		std::filesystem::path(filePath ? filePath : L""),
		WideToUtf8(normalizedPath),
		visited,
		key);
	return key;
}

Microsoft::WRL::ComPtr<IDxcBlob> DXCCompilerManager::FindCachedShader(std::string_view cacheKey)
{
	std::scoped_lock lock(shaderCacheMutex_);
	++shaderCacheStats_.requestCount;
	const auto it = shaderCache_.find(std::string(cacheKey));
	if (it == shaderCache_.end())
	{
		++shaderCacheStats_.missCount;
		return nullptr;
	}

	++shaderCacheStats_.hitCount;
	return it->second.blob;
}

void DXCCompilerManager::StoreCachedShader(
	std::string cacheKey,
	std::wstring sourcePath,
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob)
{
	if (!shaderBlob) return;
	std::scoped_lock lock(shaderCacheMutex_);
	ShaderCacheEntry entry{};
	entry.sourcePath = NormalizeWidePath(sourcePath);
	entry.blob = std::move(shaderBlob);
	shaderCache_.insert_or_assign(std::move(cacheKey), std::move(entry));
	++shaderCacheStats_.compileCount;
	shaderCacheStats_.entryCount = static_cast<uint32_t>(shaderCache_.size());
}

uint32_t DXCCompilerManager::InvalidateShader(const wchar_t* filePath)
{
	const std::wstring normalizedPath = NormalizeWidePath(filePath ? std::wstring_view(filePath) : std::wstring_view{});
	std::scoped_lock lock(shaderCacheMutex_);
	uint32_t removed = 0;
	for (auto it = shaderCache_.begin(); it != shaderCache_.end();)
	{
		if (it->second.sourcePath == normalizedPath)
		{
			it = shaderCache_.erase(it);
			++removed;
		}
		else
		{
			++it;
		}
	}
	shaderCacheStats_.invalidationCount += removed;
	shaderCacheStats_.entryCount = static_cast<uint32_t>(shaderCache_.size());
	return removed;
}

void DXCCompilerManager::ClearShaderCache()
{
	std::scoped_lock lock(shaderCacheMutex_);
	shaderCache_.clear();
	shaderCacheStats_.entryCount = 0;
	++shaderCacheStats_.clearCount;
}

DXCCompilerManager::ShaderCacheStats DXCCompilerManager::GetShaderCacheStats() const
{
	std::scoped_lock lock(shaderCacheMutex_);
	ShaderCacheStats stats = shaderCacheStats_;
	stats.entryCount = static_cast<uint32_t>(shaderCache_.size());
	return stats;
}
} // namespace Ken4lowEngine
