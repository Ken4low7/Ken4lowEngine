#include "AssetPathSelector.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string_view>
#include <unordered_set>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		struct AssetScanRule
		{
			std::filesystem::path root;
			std::vector<std::string_view> extensions;
		};

		struct AssetCache
		{
			bool loaded = false;
			std::vector<std::string> files;
		};

		std::array<AssetCache, 8> g_assetCaches{};

		size_t ToCacheIndex(AssetType type)
		{
			return static_cast<size_t>(type);
		}

		std::string ToLower(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			return value;
		}

		bool HasExtension(const std::filesystem::path& path, const std::vector<std::string_view>& extensions)
		{
			if (extensions.empty())
			{
				return true;
			}

			const std::string extension = ToLower(path.extension().string());
			for (std::string_view allowed : extensions)
			{
				if (extension == allowed)
				{
					return true;
				}
			}

			return false;
		}

		std::vector<AssetScanRule> GetScanRules(AssetType type)
		{
			switch (type)
			{
			case AssetType::Texture:
				return { { "Resources/Textures/Compiled", { ".dds" } } };
			case AssetType::Model:
				return { { "Resources/Models/Sources", { ".obj", ".gltf", ".glb" } } };
			case AssetType::SkeletalMesh:
				return { { "Resources/Models/Sources", { ".gltf", ".glb", ".fbx" } } };
			case AssetType::Audio:
				return { { "Resources/Sounds", { ".wav", ".mp3", ".ogg" } } };
			case AssetType::Font:
				return {
					{ "Resources/Fonts/Sources", { ".ttf", ".otf" } },
					{ "Resources/Fonts/Compiled", { ".json" } }
				};
			case AssetType::Particle:
				return { { "Resources/JSON/GpuParticles", { ".json" } } };
			case AssetType::Animation:
				return { { "Resources/Models/Sources", { ".gltf", ".glb" } } };
			case AssetType::Any:
			default:
				return { { "Resources", {} } };
			}
		}

		std::vector<std::string> ScanFiles(AssetType type)
		{
			std::vector<std::string> files;
			std::unordered_set<std::string> uniqueFiles;

			for (const AssetScanRule& rule : GetScanRules(type))
			{
				std::error_code rootError;
				if (!std::filesystem::exists(rule.root, rootError))
				{
					continue;
				}

				std::error_code iteratorError;
				std::filesystem::recursive_directory_iterator it(
					rule.root,
					std::filesystem::directory_options::skip_permission_denied,
					iteratorError);
				std::filesystem::recursive_directory_iterator end;

				for (; it != end && !iteratorError; it.increment(iteratorError))
				{
					const std::filesystem::directory_entry& entry = *it;
					std::error_code entryError;
					if (!entry.is_regular_file(entryError))
					{
						continue;
					}

					if (!HasExtension(entry.path(), rule.extensions))
					{
						continue;
					}

					std::error_code relativeError;
					const std::filesystem::path relativePath = std::filesystem::relative(entry.path(), rule.root, relativeError);
					if (relativeError)
					{
						continue;
					}

					const std::string genericPath = relativePath.generic_string();
					if (uniqueFiles.insert(genericPath).second)
					{
						files.push_back(genericPath);
					}
				}
			}

			std::sort(files.begin(), files.end());
			return files;
		}
	}

	const std::vector<std::string>& AssetPathSelector::GetFiles(AssetType type)
	{
		AssetCache& cache = g_assetCaches[ToCacheIndex(type)];
		if (!cache.loaded)
		{
			cache.files = ScanFiles(type);
			cache.loaded = true;
		}

		return cache.files;
	}

	void AssetPathSelector::Refresh(AssetType type)
	{
		AssetCache& cache = g_assetCaches[ToCacheIndex(type)];
		cache.files = ScanFiles(type);
		cache.loaded = true;
	}

	void AssetPathSelector::RefreshAll()
	{
		for (AssetCache& cache : g_assetCaches)
		{
			cache.loaded = false;
			cache.files.clear();
		}
	}

	bool AssetPathSelector::DrawAssetSelector(const char* label, std::string& path, AssetType type)
	{
#ifdef USE_IMGUI
		bool changed = false;
		const std::vector<std::string>& files = GetFiles(type);

		ImGui::PushID(label);
		const char* preview = path.empty() ? "未選択" : path.c_str();
		if (ImGui::BeginCombo(label, preview))
		{
			if (ImGui::Selectable("未選択", path.empty()))
			{
				path.clear();
				changed = true;
			}

			if (files.empty())
			{
				ImGui::TextDisabled("Resources内に対象ファイルがありません");
			}

			for (const std::string& file : files)
			{
				const bool selected = path == file;
				if (ImGui::Selectable(file.c_str(), selected))
				{
					path = file;
					changed = true;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		ImGui::SameLine();
		if (ImGui::Button("一覧更新"))
		{
			Refresh(type);
		}
		ImGui::PopID();

		return changed;
#else
		(void)label;
		(void)path;
		(void)type;
		return false;
#endif // USE_IMGUI
	}
}
