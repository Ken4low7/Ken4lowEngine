#include "EditorAssetBrowser.h"

#include "EditorOutputLog.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Ken4lowEngine
{
	namespace
	{
		std::string ToUtf8Path(const std::filesystem::path& path)
		{
			return path.generic_string();
		}

		std::string ToLower(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
				{
					return static_cast<char>(std::tolower(c));
				});
			return value;
		}

		std::string FormatFileTime(const std::filesystem::file_time_type& fileTime)
		{
			const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
				fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
			const std::time_t timeValue = std::chrono::system_clock::to_time_t(systemTime);
			std::tm localTime{};
#ifdef _WIN32
			localtime_s(&localTime, &timeValue);
#else
			localtime_r(&timeValue, &localTime);
#endif
			std::ostringstream stream;
			stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
			return stream.str();
		}

		std::string GetIcon(EditorAssetCategory category, const std::filesystem::path& extension)
		{
			const std::string ext = ToLower(extension.string());
			if (ext == ".png")
			{
				return "[PNG]";
			}
			if (ext == ".jpg" || ext == ".jpeg")
			{
				return "[JPG]";
			}
			if (ext == ".dds")
			{
				return "[DDS]";
			}

			switch (category)
			{
			case EditorAssetCategory::Textures:
				return "[TEX]";
			case EditorAssetCategory::Models:
				if (ext == ".gltf" || ext == ".glb")
				{
					return "[GLTF]";
				}
				return "[KMESH]";
			case EditorAssetCategory::Shaders:
				return "[HLSL]";
			case EditorAssetCategory::Fonts:
				return (ext == ".ttf") ? "[TTF]" : "[FONT]";
			default:
				return "[FILE]";
			}
		}
	}

	void EditorAssetBrowser::Initialize(EditorOutputLog* log)
	{
		log_ = log;
		projectDir_ = ResolveProjectDir();
		// Editor起動時にResources配下の実ファイルを初回スキャンする。
		Refresh();
	}

	void EditorAssetBrowser::Refresh()
	{
		entries_.clear();
		const std::filesystem::path root = ResolveAssetRoot(category_, viewMode_);
		if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root))
		{
			if (log_)
			{
				log_->Warning("Content Browser folder not found: " + ToUtf8Path(root));
			}
			RebuildFilteredEntries();
			return;
		}

		std::error_code error;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, error))
		{
			if (error)
			{
				if (log_)
				{
					log_->Warning("Content Browser scan warning: " + error.message());
				}
				error.clear();
				continue;
			}
			if (!entry.is_regular_file(error))
			{
				error.clear();
				continue;
			}

			EditorAssetEntry asset{};
			asset.label = ToUtf8Path(entry.path().filename());
			asset.icon = GetIcon(category_, entry.path().extension());
			asset.absolutePath = ToUtf8Path(std::filesystem::absolute(entry.path(), error));
			if (error)
			{
				asset.absolutePath = ToUtf8Path(entry.path());
				error.clear();
			}
			asset.relativePath = ToUtf8Path(std::filesystem::relative(entry.path(), projectDir_, error));
			if (error)
			{
				asset.relativePath = ToUtf8Path(entry.path());
				error.clear();
			}
			asset.extension = entry.path().extension().string();
			asset.sizeBytes = entry.file_size(error);
			if (error)
			{
				asset.sizeBytes = 0;
				error.clear();
			}
			asset.modifiedTime = FormatFileTime(entry.last_write_time(error));
			if (error)
			{
				asset.modifiedTime = "Unknown";
				error.clear();
			}
			entries_.push_back(asset);
		}

		std::sort(entries_.begin(), entries_.end(), [this](const EditorAssetEntry& lhs, const EditorAssetEntry& rhs)
			{
				if (category_ == EditorAssetCategory::Textures)
				{
					const bool lhsImage = IsImageExtension(lhs.extension);
					const bool rhsImage = IsImageExtension(rhs.extension);
					if (lhsImage != rhsImage)
					{
						return lhsImage;
					}
				}
				return lhs.relativePath < rhs.relativePath;
			});
		RebuildFilteredEntries();

		if (log_)
		{
			log_->Info("Content Browser refreshed: " + std::to_string(entries_.size()) + " files from " + ToUtf8Path(root));
		}
	}

	void EditorAssetBrowser::SetCategory(EditorAssetCategory category)
	{
		if (category_ == category)
		{
			return;
		}
		category_ = category;
		selectedRelativePath_.clear();
		// カテゴリ変更時は対象Resourceフォルダを即座に再スキャンする。
		Refresh();
	}

	void EditorAssetBrowser::SetViewMode(EditorAssetViewMode mode)
	{
		if (viewMode_ == mode)
		{
			return;
		}
		viewMode_ = mode;
		selectedRelativePath_.clear();
		// Sources/Compiled切替時もBlender出力や変換済みファイルを取り直す。
		Refresh();
	}

	void EditorAssetBrowser::SetSearchFilter(const std::string& filter)
	{
		searchFilter_ = filter;
		RebuildFilteredEntries();
	}

	void EditorAssetBrowser::Select(std::size_t filteredIndex)
	{
		if (filteredIndex < filteredEntries_.size())
		{
			selectedRelativePath_ = filteredEntries_[filteredIndex].relativePath;
		}
	}

	const EditorAssetEntry* EditorAssetBrowser::GetSelectedEntry() const
	{
		if (selectedRelativePath_.empty())
		{
			return nullptr;
		}
		auto found = std::find_if(filteredEntries_.begin(), filteredEntries_.end(), [this](const EditorAssetEntry& entry)
			{
				return entry.relativePath == selectedRelativePath_;
			});
		return found == filteredEntries_.end() ? nullptr : &(*found);
	}

	std::filesystem::path EditorAssetBrowser::GetCurrentRoot() const
	{
		return ResolveAssetRoot(category_, viewMode_);
	}

	const char* EditorAssetBrowser::GetCategoryName(EditorAssetCategory category)
	{
		switch (category)
		{
		case EditorAssetCategory::Textures:
			return "Textures";
		case EditorAssetCategory::Models:
			return "Models";
		case EditorAssetCategory::Shaders:
			return "Shaders";
		case EditorAssetCategory::Fonts:
			return "Fonts";
		default:
			return "Assets";
		}
	}

	const char* EditorAssetBrowser::GetViewModeName(EditorAssetViewMode mode)
	{
		return mode == EditorAssetViewMode::Sources ? "Sources" : "Compiled";
	}

	bool EditorAssetBrowser::IsImageExtension(const std::string& extension)
	{
		const std::string ext = ToLower(extension);
		return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".bmp";
	}

	void EditorAssetBrowser::RebuildFilteredEntries()
	{
		filteredEntries_.clear();
		const std::string filter = ToLower(searchFilter_);
		for (const EditorAssetEntry& entry : entries_)
		{
			const std::string haystack = ToLower(entry.relativePath + " " + entry.label);
			if (filter.empty() || haystack.find(filter) != std::string::npos)
			{
				filteredEntries_.push_back(entry);
			}
		}
	}

	std::filesystem::path EditorAssetBrowser::ResolveProjectDir() const
	{
		std::error_code error;
		std::filesystem::path cursor = std::filesystem::current_path(error);
		for (int i = 0; i < 8 && !cursor.empty(); ++i)
		{
			const std::filesystem::path direct = cursor;
			if (std::filesystem::exists(direct / "Resources", error) && std::filesystem::exists(direct / "Tools" / "Scripts", error))
			{
				return direct;
			}
			const std::filesystem::path childProject = cursor / "Project";
			if (std::filesystem::exists(childProject / "Resources", error) && std::filesystem::exists(childProject / "Tools" / "Scripts", error))
			{
				return childProject;
			}
			cursor = cursor.parent_path();
		}
		return std::filesystem::current_path(error) / "Project";
	}

	std::filesystem::path EditorAssetBrowser::ResolveAssetRoot(EditorAssetCategory category, EditorAssetViewMode mode) const
	{
		switch (category)
		{
		case EditorAssetCategory::Textures:
			return projectDir_ / "Resources" / "Textures" / GetViewModeName(mode);
		case EditorAssetCategory::Models:
			return projectDir_ / "Resources" / "Models" / GetViewModeName(mode);
		case EditorAssetCategory::Shaders:
			return projectDir_ / "Resources" / "Shaders";
		case EditorAssetCategory::Fonts:
			return projectDir_ / "Resources" / "Fonts" / GetViewModeName(mode);
		default:
			return projectDir_ / "Resources";
		}
	}

} // namespace Ken4lowEngine
