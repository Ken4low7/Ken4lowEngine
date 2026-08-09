#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class EditorOutputLog;

	enum class EditorAssetCategory
	{
		Textures,
		Models,
		Shaders,
		Fonts
	};

	enum class EditorAssetViewMode
	{
		Sources,
		Compiled
	};

	struct EditorAssetEntry
	{
		std::string label;
		std::string icon;
		std::string relativePath;
		std::string absolutePath;
		std::string extension;
		std::string modifiedTime;
		uintmax_t sizeBytes = 0;
	};

	class EditorAssetBrowser
	{
	public:
		void Initialize(EditorOutputLog* log);
		void Refresh();
		void SetCategory(EditorAssetCategory category);
		void SetViewMode(EditorAssetViewMode mode);
		void SetSearchFilter(const std::string& filter);
		void Select(std::size_t filteredIndex);

		EditorAssetCategory GetCategory() const { return category_; }
		EditorAssetViewMode GetViewMode() const { return viewMode_; }
		const std::string& GetSearchFilter() const { return searchFilter_; }
		const std::vector<EditorAssetEntry>& GetFilteredEntries() const { return filteredEntries_; }
		const EditorAssetEntry* GetSelectedEntry() const;
		std::filesystem::path GetCurrentRoot() const;

		static const char* GetCategoryName(EditorAssetCategory category);
		static const char* GetViewModeName(EditorAssetViewMode mode);
		static bool IsImageExtension(const std::string& extension);

	private:
		void RebuildFilteredEntries();
		std::filesystem::path ResolveProjectDir() const;
		std::filesystem::path ResolveAssetRoot(EditorAssetCategory category, EditorAssetViewMode mode) const;

		EditorOutputLog* log_ = nullptr;
		std::filesystem::path projectDir_;
		EditorAssetCategory category_ = EditorAssetCategory::Textures;
		EditorAssetViewMode viewMode_ = EditorAssetViewMode::Sources;
		std::string searchFilter_;
		std::vector<EditorAssetEntry> entries_;
		std::vector<EditorAssetEntry> filteredEntries_;
		std::string selectedRelativePath_;
	};

} // namespace Ken4lowEngine
