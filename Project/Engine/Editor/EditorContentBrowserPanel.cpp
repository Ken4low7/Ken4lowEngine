#define NOMINMAX
#include "EditorContentBrowserPanel.h"

#include <algorithm>
#include <cstdio>
#include <system_error>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
#ifdef USE_IMGUI
		template <class Action>
		void DrawNavigationButton(const char* label, const char* tooltip, bool enabled, Action action)
		{
			if (!enabled)
			{
				ImGui::BeginDisabled();
			}
			if (ImGui::Button(label, ImVec2(30.0f, 0.0f)) && enabled)
			{
				action();
			}
			if (!enabled)
			{
				ImGui::EndDisabled();
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("%s", tooltip);
			}
		}

		ImVec2 FitImageSize(uint32_t width, uint32_t height, const ImVec2& bounds)
		{
			if (width == 0 || height == 0 || bounds.x <= 0.0f || bounds.y <= 0.0f)
			{
				return ImVec2(0.0f, 0.0f);
			}
			const float scale = std::min(
				bounds.x / static_cast<float>(width),
				bounds.y / static_cast<float>(height));
			return ImVec2(
				std::floor(static_cast<float>(width) * scale),
				std::floor(static_cast<float>(height) * scale));
		}

		void DrawCheckerBackground(ImDrawList* drawList, const ImVec2& min, const ImVec2& max)
		{
			constexpr float squareSize = 8.0f;
			const ImU32 light = ImGui::GetColorU32(ImVec4(0.28f, 0.28f, 0.30f, 1.0f));
			const ImU32 dark = ImGui::GetColorU32(ImVec4(0.18f, 0.18f, 0.20f, 1.0f));

			for (float y = min.y; y < max.y; y += squareSize)
			{
				for (float x = min.x; x < max.x; x += squareSize)
				{
					const int column = static_cast<int>((x - min.x) / squareSize);
					const int row = static_cast<int>((y - min.y) / squareSize);
					const ImU32 color = ((column + row) % 2 == 0) ? light : dark;
					drawList->AddRectFilled(
						ImVec2(x, y),
						ImVec2(std::min(x + squareSize, max.x), std::min(y + squareSize, max.y)),
						color);
				}
			}
		}

		ImU32 GetAssetAccentColor(EditorAssetType type)
		{
			switch (type)
			{
			case EditorAssetType::Folder: return ImGui::GetColorU32(ImVec4(0.91f, 0.66f, 0.24f, 1.0f));
			case EditorAssetType::Texture: return ImGui::GetColorU32(ImVec4(0.35f, 0.74f, 0.95f, 1.0f));
			case EditorAssetType::Model: return ImGui::GetColorU32(ImVec4(0.36f, 0.82f, 0.56f, 1.0f));
			case EditorAssetType::Animation: return ImGui::GetColorU32(ImVec4(0.76f, 0.48f, 0.94f, 1.0f));
			case EditorAssetType::Material: return ImGui::GetColorU32(ImVec4(0.95f, 0.46f, 0.36f, 1.0f));
			case EditorAssetType::ActorPrefab: return ImGui::GetColorU32(ImVec4(0.36f, 0.58f, 0.96f, 1.0f));
			case EditorAssetType::Level: return ImGui::GetColorU32(ImVec4(0.42f, 0.88f, 0.82f, 1.0f));
			case EditorAssetType::Shader: return ImGui::GetColorU32(ImVec4(0.96f, 0.74f, 0.30f, 1.0f));
			case EditorAssetType::Font: return ImGui::GetColorU32(ImVec4(0.90f, 0.90f, 0.92f, 1.0f));
			case EditorAssetType::Audio: return ImGui::GetColorU32(ImVec4(0.94f, 0.40f, 0.62f, 1.0f));
			case EditorAssetType::Json: return ImGui::GetColorU32(ImVec4(0.70f, 0.78f, 0.36f, 1.0f));
			default: return ImGui::GetColorU32(ImVec4(0.62f, 0.66f, 0.72f, 1.0f));
			}
		}

		void DrawFolderIcon(ImDrawList* drawList, const ImVec2& min, const ImVec2& max)
		{
			const ImU32 folderColor = GetAssetAccentColor(EditorAssetType::Folder);
			const float width = max.x - min.x;
			const float height = max.y - min.y;
			const ImVec2 bodyMin(min.x + width * 0.12f, min.y + height * 0.30f);
			const ImVec2 bodyMax(max.x - width * 0.12f, max.y - height * 0.13f);
			const ImVec2 tabMin(bodyMin.x + width * 0.06f, min.y + height * 0.18f);
			const ImVec2 tabMax(bodyMin.x + width * 0.42f, bodyMin.y + height * 0.08f);
			drawList->AddRectFilled(tabMin, tabMax, folderColor, 4.0f);
			drawList->AddRectFilled(bodyMin, bodyMax, folderColor, 5.0f);
			drawList->AddLine(
				ImVec2(bodyMin.x + width * 0.05f, bodyMin.y + height * 0.10f),
				ImVec2(bodyMax.x - width * 0.05f, bodyMin.y + height * 0.10f),
				ImGui::GetColorU32(ImVec4(1.0f, 0.83f, 0.48f, 1.0f)),
				2.0f);
		}

		void DrawAssetTypeIcon(
			ImDrawList* drawList,
			const ImVec2& min,
			const ImVec2& max,
			EditorAssetType type,
			const char* badge)
		{
			const ImU32 accent = GetAssetAccentColor(type);
			const float width = max.x - min.x;
			const float height = max.y - min.y;
			const ImVec2 fileMin(min.x + width * 0.23f, min.y + height * 0.12f);
			const ImVec2 fileMax(max.x - width * 0.23f, max.y - height * 0.12f);
			drawList->AddRectFilled(fileMin, fileMax, accent, 5.0f);
			drawList->AddTriangleFilled(
				ImVec2(fileMax.x - width * 0.18f, fileMin.y),
				ImVec2(fileMax.x, fileMin.y),
				ImVec2(fileMax.x, fileMin.y + height * 0.18f),
				ImGui::GetColorU32(ImVec4(0.15f, 0.16f, 0.18f, 0.72f)));

			const ImVec2 textSize = ImGui::CalcTextSize(badge);
			drawList->AddText(
				ImVec2(
					(fileMin.x + fileMax.x - textSize.x) * 0.5f,
					(fileMin.y + fileMax.y - textSize.y) * 0.5f + height * 0.08f),
				ImGui::GetColorU32(ImVec4(0.08f, 0.09f, 0.11f, 1.0f)),
				badge);
		}

		bool IsValidAssetName(const std::string& name)
		{
			if (name.empty() || name == "." || name == "..")
			{
				return false;
			}
			// Windowsで使用できない文字とパス区切りを拒否し、同じ親フォルダ内だけで名前を変更する。
			return name.find_first_of("<>:\"/\\|?*") == std::string::npos;
		}
#endif
	}

	EditorContentBrowserPanel* EditorContentBrowserPanel::GetInstance()
	{
		static EditorContentBrowserPanel instance;
		return &instance;
	}

	void EditorContentBrowserPanel::Draw()
	{
#ifdef USE_IMGUI
		InitializeIfNeeded();

		// 旧Content Browserを停止し、同じDock IDへV2だけを登録する。
		EditorWindowManager::GetInstance()->GetWindowState().showContentBrowser = false;

		if (!ImGui::Begin("コンテンツブラウザ###Content Browser", nullptr, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		DrawToolbar();
		ImGui::Separator();
		DrawBrowserBody();
		ProcessDeferredDuplicate();
		DrawPendingDialogs();
		ImGui::End();
#endif
	}

#ifdef USE_IMGUI
	void EditorContentBrowserPanel::InitializeIfNeeded()
	{
		if (initialized_)
		{
			return;
		}

		registry_.Initialize();
		history_.push_back({});
		historyIndex_ = 0;
		initialized_ = true;
	}

	void EditorContentBrowserPanel::RefreshRegistry()
	{
		// ファイル更新後に古いSRVを表示し続けないよう、再走査前にサムネイルキャッシュを破棄する。
		texturePreviewCache_.Clear();
		registry_.Refresh();
		if (!registry_.IsValidDirectory(currentDirectory_))
		{
			currentDirectory_.clear();
			history_.clear();
			history_.push_back({});
			historyIndex_ = 0;
			selectedAssetId_ = 0;
		}
		if (selectedAssetId_ != 0 && registry_.FindById(selectedAssetId_) == nullptr)
		{
			selectedAssetId_ = 0;
		}
	}

	void EditorContentBrowserPanel::NavigateTo(const std::filesystem::path& directory, bool recordHistory)
	{
		const std::filesystem::path normalized = EditorAssetRegistryV2::NormalizeRelative(directory);
		if (!registry_.IsValidDirectory(normalized) || currentDirectory_ == normalized)
		{
			return;
		}

		currentDirectory_ = normalized;
		selectedAssetId_ = 0;

		if (recordHistory)
		{
			if (historyIndex_ + 1 < history_.size())
			{
				history_.erase(
					history_.begin() + static_cast<std::ptrdiff_t>(historyIndex_ + 1),
					history_.end());
			}
			history_.push_back(currentDirectory_);
			historyIndex_ = history_.size() - 1;
		}
	}

	void EditorContentBrowserPanel::NavigateBack()
	{
		if (!CanNavigateBack())
		{
			return;
		}
		--historyIndex_;
		currentDirectory_ = history_[historyIndex_];
		selectedAssetId_ = 0;
	}

	void EditorContentBrowserPanel::NavigateForward()
	{
		if (!CanNavigateForward())
		{
			return;
		}
		++historyIndex_;
		currentDirectory_ = history_[historyIndex_];
		selectedAssetId_ = 0;
	}

	void EditorContentBrowserPanel::NavigateUp()
	{
		if (currentDirectory_.empty())
		{
			return;
		}
		NavigateTo(currentDirectory_.parent_path(), true);
	}

	void EditorContentBrowserPanel::DrawToolbar()
	{
		ImGui::BeginDisabled();
		ImGui::Button("+ 追加");
		ImGui::SameLine();
		ImGui::Button("インポート");
		ImGui::EndDisabled();

		ImGui::SameLine();
		DrawNavigationButton("<", "戻る", CanNavigateBack(), [this]() { NavigateBack(); });
		ImGui::SameLine();
		DrawNavigationButton(">", "進む", CanNavigateForward(), [this]() { NavigateForward(); });
		ImGui::SameLine();
		DrawNavigationButton("^", "ひとつ上のフォルダ", !currentDirectory_.empty(), [this]() { NavigateUp(); });
		ImGui::SameLine();
		if (ImGui::Button("フォルダを開く"))
		{
			OpenCurrentFolderInExplorer();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("現在表示しているフォルダをエクスプローラーで開きます。");
		}

		ImGui::SameLine();
		if (ImGui::Button("更新"))
		{
			RefreshRegistry();
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::BeginCombo("##AssetTypeFilter", EditorAssetRegistryV2::GetTypeName(typeFilter_)))
		{
			constexpr std::array<EditorAssetType, 12> filterTypes = {
				EditorAssetType::All,
				EditorAssetType::Texture,
				EditorAssetType::Model,
				EditorAssetType::Animation,
				EditorAssetType::Material,
				EditorAssetType::ActorPrefab,
				EditorAssetType::Level,
				EditorAssetType::Shader,
				EditorAssetType::Font,
				EditorAssetType::Audio,
				EditorAssetType::Json,
				EditorAssetType::Other,
			};
			for (const EditorAssetType type : filterTypes)
			{
				const bool selected = typeFilter_ == type;
				if (ImGui::Selectable(EditorAssetRegistryV2::GetTypeName(type), selected))
				{
					typeFilter_ = type;
					selectedAssetId_ = 0;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(115.0f);
		if (ImGui::BeginCombo("##AssetSortMode", GetSortModeName(sortMode_)))
		{
			constexpr std::array<AssetSortMode, 3> sortModes = {
				AssetSortMode::Name,
				AssetSortMode::ModifiedTime,
				AssetSortMode::Size,
			};
			for (const AssetSortMode mode : sortModes)
			{
				const bool selected = sortMode_ == mode;
				if (ImGui::Selectable(GetSortModeName(mode), selected))
				{
					sortMode_ = mode;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button(sortAscending_ ? "昇順" : "降順"))
		{
			sortAscending_ = !sortAscending_;
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(105.0f);
		ImGui::SliderFloat("##AssetCellSize", &cellWidth_, 88.0f, 176.0f, "表示 %.0f");

		ImGui::Spacing();
		DrawBreadcrumbs();
		ImGui::SameLine();
		ImGui::Checkbox("サブフォルダ", &searchSubfolders_);
		ImGui::SameLine();
		ImGui::Checkbox("フォルダ表示", &showFolders_);
		ImGui::SameLine();

		const float searchWidth = std::max(180.0f, ImGui::GetContentRegionAvail().x - 72.0f);
		ImGui::SetNextItemWidth(searchWidth);
		ImGui::InputTextWithHint(
			"##ContentBrowserV2Search",
			"名前・パス・種類を検索...",
			searchBuffer_.data(),
			searchBuffer_.size());
		ImGui::SameLine();
		if (ImGui::Button("クリア"))
		{
			searchBuffer_.fill('\0');
			selectedAssetId_ = 0;
		}

		if (!operationMessage_.empty())
		{
			const ImVec4 color = operationFailed_
				? ImVec4(1.0f, 0.42f, 0.36f, 1.0f)
				: ImVec4(0.42f, 0.90f, 0.54f, 1.0f);
			ImGui::TextColored(color, "%s", operationMessage_.c_str());
		}
	}

	void EditorContentBrowserPanel::DrawBreadcrumbs()
	{
		std::filesystem::path requestedDirectory;
		bool navigationRequested = false;

		if (ImGui::Button("コンテンツ"))
		{
			requestedDirectory.clear();
			navigationRequested = true;
		}

		// 描画中にcurrentDirectory_を書き換えるとpath iteratorが無効になるため、スナップショットを走査する。
		const std::filesystem::path directorySnapshot = currentDirectory_;
		std::filesystem::path accumulated;
		for (const std::filesystem::path& part : directorySnapshot)
		{
			accumulated /= part;
			ImGui::SameLine();
			ImGui::TextDisabled(">");
			ImGui::SameLine();
			const std::string label = part.generic_string() + "##Breadcrumb" + accumulated.generic_string();
			if (ImGui::Button(label.c_str()))
			{
				requestedDirectory = accumulated;
				navigationRequested = true;
			}
		}

		if (navigationRequested)
		{
			NavigateTo(requestedDirectory, true);
		}
	}

	void EditorContentBrowserPanel::DrawBrowserBody()
	{
		const float availableWidth = ImGui::GetContentRegionAvail().x;
		const bool showDetails = availableWidth >= 980.0f;
		const float treeWidth = std::clamp(availableWidth * 0.18f, 190.0f, 270.0f);
		const float detailsWidth = showDetails ? std::clamp(availableWidth * 0.22f, 260.0f, 360.0f) : 0.0f;

		if (ImGui::BeginChild("##ContentSources", ImVec2(treeWidth, 0.0f), true))
		{
			ImGui::TextUnformatted("ソース");
			ImGui::Separator();
			DrawFolderTree();
		}
		ImGui::EndChild();

		ImGui::SameLine();
		const float centerWidth = showDetails
			? -(detailsWidth + ImGui::GetStyle().ItemSpacing.x)
			: 0.0f;
		if (ImGui::BeginChild("##ContentAssets", ImVec2(centerWidth, 0.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
		{
			DrawAssetGrid();
		}
		ImGui::EndChild();

		if (showDetails)
		{
			ImGui::SameLine();
			if (ImGui::BeginChild("##ContentDetails", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
			{
				DrawSelectedAssetDetails();
			}
			ImGui::EndChild();
		}
	}

	void EditorContentBrowserPanel::DrawFolderTree()
	{
		ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen |
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth;
		if (currentDirectory_.empty())
		{
			rootFlags |= ImGuiTreeNodeFlags_Selected;
		}
		const bool rootOpen = ImGui::TreeNodeEx("コンテンツ##ContentRoot", rootFlags);
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			NavigateTo({}, true);
		}
		if (rootOpen)
		{
			for (const EditorAssetData* directory : registry_.GetDirectories({}))
			{
				DrawFolderNode(*directory);
			}
			ImGui::TreePop();
		}
	}

	void EditorContentBrowserPanel::DrawFolderNode(const EditorAssetData& directory)
	{
		const std::vector<const EditorAssetData*> children = registry_.GetDirectories(directory.relativePath);
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_OpenOnDoubleClick |
			ImGuiTreeNodeFlags_SpanAvailWidth;
		if (children.empty())
		{
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		if (currentDirectory_ == directory.relativePath)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		ImGui::PushID(reinterpret_cast<void*>(static_cast<uintptr_t>(directory.id)));
		const std::string label = "[DIR] " + directory.name;
		const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			NavigateTo(directory.relativePath, true);
		}
		if (ImGui::BeginPopupContextItem("##FolderTreeContext"))
		{
			DrawAssetContextMenu(directory);
			ImGui::EndPopup();
		}
		if (!children.empty() && open)
		{
			for (const EditorAssetData* child : children)
			{
				DrawFolderNode(*child);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	std::vector<const EditorAssetData*> EditorContentBrowserPanel::BuildVisibleEntries() const
	{
		const std::string_view search(searchBuffer_.data());
		std::vector<const EditorAssetData*> entries = registry_.Query(currentDirectory_, search, typeFilter_);

		if (!search.empty() && !searchSubfolders_)
		{
			entries.erase(
				std::remove_if(entries.begin(), entries.end(), [this](const EditorAssetData* asset)
					{
						return asset->parentPath != currentDirectory_;
					}),
				entries.end());
		}
		if (!showFolders_)
		{
			entries.erase(
				std::remove_if(entries.begin(), entries.end(), [](const EditorAssetData* asset)
					{
						return asset->isDirectory;
					}),
				entries.end());
		}

		std::stable_sort(entries.begin(), entries.end(), [this](const EditorAssetData* lhs, const EditorAssetData* rhs)
			{
				if (lhs->isDirectory != rhs->isDirectory)
				{
					return lhs->isDirectory;
				}

				bool less = false;
				bool greater = false;
				switch (sortMode_)
				{
				case AssetSortMode::ModifiedTime:
					less = lhs->modifiedTime < rhs->modifiedTime;
					greater = lhs->modifiedTime > rhs->modifiedTime;
					break;
				case AssetSortMode::Size:
					less = lhs->sizeBytes < rhs->sizeBytes;
					greater = lhs->sizeBytes > rhs->sizeBytes;
					break;
				case AssetSortMode::Name:
				default:
					less = lhs->name < rhs->name;
					greater = lhs->name > rhs->name;
					break;
				}

				if (!less && !greater)
				{
					return lhs->name < rhs->name;
				}
				return sortAscending_ ? less : greater;
			});
		return entries;
	}

	void EditorContentBrowserPanel::DrawAssetGrid()
	{
		const std::string_view search(searchBuffer_.data());
		const std::vector<const EditorAssetData*> entries = BuildVisibleEntries();
		const std::string currentPath = currentDirectory_.empty()
			? std::string("コンテンツ")
			: currentDirectory_.generic_string();

		ImGui::Text("%s  (%zu項目)", currentPath.c_str(), entries.size());
		if (!search.empty())
		{
			ImGui::SameLine();
			ImGui::TextDisabled(searchSubfolders_ ? "サブフォルダを含む検索結果" : "現在のフォルダ内の検索結果");
		}
		ImGui::Separator();

		if (!registry_.GetLastError().empty())
		{
			ImGui::TextWrapped("%s", registry_.GetLastError().c_str());
		}
		if (entries.empty())
		{
			ImGui::TextDisabled("表示できるアセットがありません。");
			return;
		}

		const float availableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const int columnCount = std::max(1, static_cast<int>((availableWidth + spacing) / (cellWidth_ + spacing)));
		const float actualCellWidth = std::floor(
			(availableWidth - spacing * static_cast<float>(columnCount - 1)) /
			static_cast<float>(columnCount));

		for (std::size_t index = 0; index < entries.size(); ++index)
		{
			DrawAssetCard(*entries[index], actualCellWidth, !search.empty());
			if ((static_cast<int>(index) + 1) % columnCount != 0)
			{
				ImGui::SameLine();
			}
		}
	}

	void EditorContentBrowserPanel::DrawAssetCard(const EditorAssetData& asset, float width, bool showRelativePath)
	{
		const float previewHeight = std::max(52.0f, width * 0.58f);
		const float cardHeight = previewHeight + (showRelativePath ? 66.0f : 48.0f);
		const bool selected = selectedAssetId_ == asset.id;

		ImGui::PushID(reinterpret_cast<void*>(static_cast<uintptr_t>(asset.id)));
		ImGui::InvisibleButton("##AssetCard", ImVec2(width, cardHeight));
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			selectedAssetId_ = asset.id;
		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (asset.isDirectory)
			{
				NavigateTo(asset.relativePath, true);
			}
			else
			{
				OpenAsset(asset);
			}
		}
		if (ImGui::BeginPopupContextItem("##AssetContextMenu"))
		{
			DrawAssetContextMenu(asset);
			ImGui::EndPopup();
		}

		if (ImGui::IsItemVisible())
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const ImVec2 min = ImGui::GetItemRectMin();
			const ImVec2 max = ImGui::GetItemRectMax();
			const ImU32 background = ImGui::GetColorU32(
				selected ? ImVec4(0.20f, 0.33f, 0.52f, 0.95f) : ImVec4(0.09f, 0.10f, 0.12f, 0.95f));
			const ImU32 border = ImGui::GetColorU32(
				selected ? ImVec4(0.95f, 0.53f, 0.08f, 1.0f) : ImVec4(0.24f, 0.25f, 0.29f, 1.0f));
			drawList->AddRectFilled(min, max, background, 4.0f);
			drawList->AddRect(min, max, border, 4.0f, 0, selected ? 2.0f : 1.0f);

			const ImVec2 previewMin(min.x + 7.0f, min.y + 7.0f);
			const ImVec2 previewMax(max.x - 7.0f, min.y + previewHeight);
			drawList->AddRectFilled(
				previewMin,
				previewMax,
				ImGui::GetColorU32(ImVec4(0.14f, 0.15f, 0.18f, 1.0f)),
				3.0f);

			bool previewDrawn = false;
			if (asset.type == EditorAssetType::Texture &&
				EditorTexturePreviewCache::IsPreviewableImage(asset.extension))
			{
				const EditorTexturePreview& preview = texturePreviewCache_.GetOrLoad(asset.absolutePath);
				if (preview.loaded && preview.srvHandleGPU.ptr != 0 && preview.width > 0 && preview.height > 0)
				{
					DrawCheckerBackground(drawList, previewMin, previewMax);
					const ImVec2 imageSize = FitImageSize(
						preview.width,
						preview.height,
						ImVec2(previewMax.x - previewMin.x - 8.0f, previewMax.y - previewMin.y - 8.0f));
					const ImVec2 imageMin(
						(previewMin.x + previewMax.x - imageSize.x) * 0.5f,
						(previewMin.y + previewMax.y - imageSize.y) * 0.5f);
					const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
					drawList->AddImage(static_cast<ImTextureID>(preview.srvHandleGPU.ptr), imageMin, imageMax);
					previewDrawn = true;
				}
			}

			if (!previewDrawn)
			{
				if (asset.isDirectory)
				{
					DrawFolderIcon(drawList, previewMin, previewMax);
				}
				else
				{
					DrawAssetTypeIcon(
						drawList,
						previewMin,
						previewMax,
						asset.type,
						EditorAssetRegistryV2::GetTypeBadge(asset.type));
				}
			}

			const std::string displayName = TruncateText(asset.name, width - 14.0f);
			drawList->AddText(
				ImVec2(min.x + 7.0f, previewMax.y + 7.0f),
				ImGui::GetColorU32(ImGuiCol_Text),
				displayName.c_str());
			if (showRelativePath)
			{
				const std::string pathText = TruncateText(asset.parentPath.generic_string(), width - 14.0f);
				drawList->AddText(
					ImVec2(min.x + 7.0f, previewMax.y + 27.0f),
					ImGui::GetColorU32(ImGuiCol_TextDisabled),
					pathText.c_str());
			}
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(
				"%s\n種類: %s\nパス: %s\nダブルクリックで%s\n右クリックで操作メニュー",
				asset.name.c_str(),
				EditorAssetRegistryV2::GetTypeName(asset.type),
				asset.relativePath.generic_string().c_str(),
				asset.isDirectory ? "フォルダを開く" : "既定アプリで開く");
		}
		ImGui::PopID();
	}

	void EditorContentBrowserPanel::DrawAssetContextMenu(const EditorAssetData& asset)
	{
		if (ImGui::MenuItem(asset.isDirectory ? "このフォルダを開く" : "開く"))
		{
			if (asset.isDirectory)
			{
				NavigateTo(asset.relativePath, true);
			}
			else
			{
				OpenAsset(asset);
			}
		}
		if (ImGui::MenuItem("エクスプローラーで表示"))
		{
			RevealInExplorer(asset);
		}
		if (ImGui::MenuItem("パスをコピー"))
		{
			CopyPathToClipboard(asset);
		}

		ImGui::Separator();
		if (ImGui::MenuItem("複製"))
		{
			// Registryを再走査する操作はカード描画完了後まで遅延し、走査中ポインタを無効化しない。
			duplicateAssetId_ = asset.id;
			duplicateRequested_ = true;
		}
		if (ImGui::MenuItem("名前を変更"))
		{
			BeginRename(asset);
		}
		if (ImGui::MenuItem("削除"))
		{
			BeginDelete(asset);
		}
	}

	void EditorContentBrowserPanel::DrawPendingDialogs()
	{
		if (openRenamePopup_)
		{
			ImGui::OpenPopup("名前を変更###ContentBrowserRename");
			openRenamePopup_ = false;
		}
		if (ImGui::BeginPopupModal("名前を変更###ContentBrowserRename", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("新しい名前を入力してください。");
			ImGui::SetNextItemWidth(420.0f);
			const bool submit = ImGui::InputText(
				"##RenameAsset",
				renameBuffer_.data(),
				renameBuffer_.size(),
				ImGuiInputTextFlags_EnterReturnsTrue);
			if (submit || ImGui::Button("変更", ImVec2(100.0f, 0.0f)))
			{
				ApplyRename();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("キャンセル", ImVec2(100.0f, 0.0f)))
			{
				renameAssetId_ = 0;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (openDeletePopup_)
		{
			ImGui::OpenPopup("アセットを削除###ContentBrowserDelete");
			openDeletePopup_ = false;
		}
		if (ImGui::BeginPopupModal("アセットを削除###ContentBrowserDelete", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			const EditorAssetData* asset = registry_.FindById(deleteAssetId_);
			ImGui::TextUnformatted("この操作は元に戻せません。");
			if (asset)
			{
				ImGui::TextWrapped("削除対象: %s", asset->relativePath.generic_string().c_str());
			}
			if (ImGui::Button("削除する", ImVec2(110.0f, 0.0f)))
			{
				ApplyDelete();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("キャンセル", ImVec2(110.0f, 0.0f)))
			{
				deleteAssetId_ = 0;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void EditorContentBrowserPanel::OpenAsset(const EditorAssetData& asset) const
	{
#ifdef _WIN32
		const std::wstring path = asset.absolutePath.wstring();
		ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
		(void)asset;
#endif
	}

	void EditorContentBrowserPanel::RevealInExplorer(const EditorAssetData& asset) const
	{
#ifdef _WIN32
		const std::wstring path = asset.absolutePath.wstring();
		if (asset.isDirectory)
		{
			ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			return;
		}
		const std::wstring parameters = L"/select,\"" + path + L"\"";
		ShellExecuteW(nullptr, L"open", L"explorer.exe", parameters.c_str(), nullptr, SW_SHOWNORMAL);
#else
		(void)asset;
#endif
	}

	void EditorContentBrowserPanel::OpenCurrentFolderInExplorer() const
	{
#ifdef _WIN32
		const std::filesystem::path target = currentDirectory_.empty()
			? registry_.GetContentRoot()
			: registry_.GetContentRoot() / currentDirectory_;
		const std::wstring path = target.wstring();
		ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
	}

	void EditorContentBrowserPanel::CopyPathToClipboard(const EditorAssetData& asset) const
	{
		ImGui::SetClipboardText(asset.absolutePath.generic_string().c_str());
	}

	void EditorContentBrowserPanel::BeginRename(const EditorAssetData& asset)
	{
		renameAssetId_ = asset.id;
		renameBuffer_.fill('\0');
		std::snprintf(renameBuffer_.data(), renameBuffer_.size(), "%s", asset.name.c_str());
		openRenamePopup_ = true;
	}

	void EditorContentBrowserPanel::BeginDelete(const EditorAssetData& asset)
	{
		deleteAssetId_ = asset.id;
		openDeletePopup_ = true;
	}

	void EditorContentBrowserPanel::ProcessDeferredDuplicate()
	{
		if (!duplicateRequested_)
		{
			return;
		}
		duplicateRequested_ = false;

		const EditorAssetData* asset = registry_.FindById(duplicateAssetId_);
		if (!asset)
		{
			SetOperationResult("複製対象が見つかりませんでした。", true);
			duplicateAssetId_ = 0;
			return;
		}
		DuplicateAsset(*asset);
		duplicateAssetId_ = 0;
	}

	void EditorContentBrowserPanel::ApplyRename()
	{
		const EditorAssetData* asset = registry_.FindById(renameAssetId_);
		if (!asset)
		{
			SetOperationResult("名前変更対象が見つかりませんでした。", true);
			return;
		}

		const std::string newName(renameBuffer_.data());
		if (!IsValidAssetName(newName))
		{
			SetOperationResult("使用できない名前です。", true);
			return;
		}
		if (newName == asset->name)
		{
			SetOperationResult("名前は変更されていません。", false);
			return;
		}

		const std::filesystem::path destination = asset->absolutePath.parent_path() / newName;
		std::error_code error;
		if (std::filesystem::exists(destination, error))
		{
			SetOperationResult("同じ名前の項目が既に存在します。", true);
			return;
		}

		texturePreviewCache_.Clear();
		std::filesystem::rename(asset->absolutePath, destination, error);
		if (error)
		{
			SetOperationResult("名前変更に失敗しました: " + error.message(), true);
			return;
		}

		SetOperationResult("名前を変更しました: " + newName, false);
		renameAssetId_ = 0;
		RefreshRegistry();
	}

	void EditorContentBrowserPanel::ApplyDelete()
	{
		const EditorAssetData* asset = registry_.FindById(deleteAssetId_);
		if (!asset)
		{
			SetOperationResult("削除対象が見つかりませんでした。", true);
			return;
		}

		const std::string deletedName = asset->name;
		std::error_code error;
		texturePreviewCache_.Clear();
		if (asset->isDirectory)
		{
			std::filesystem::remove_all(asset->absolutePath, error);
		}
		else
		{
			std::filesystem::remove(asset->absolutePath, error);
		}
		if (error)
		{
			SetOperationResult("削除に失敗しました: " + error.message(), true);
			return;
		}

		SetOperationResult("削除しました: " + deletedName, false);
		deleteAssetId_ = 0;
		RefreshRegistry();
	}

	void EditorContentBrowserPanel::DuplicateAsset(const EditorAssetData& asset)
	{
		const std::filesystem::path destination = MakeUniqueCopyPath(asset);
		std::error_code error;
		if (asset.isDirectory)
		{
			std::filesystem::copy(
				asset.absolutePath,
				destination,
				std::filesystem::copy_options::recursive,
				error);
		}
		else
		{
			std::filesystem::copy_file(
				asset.absolutePath,
				destination,
				std::filesystem::copy_options::none,
				error);
		}

		if (error)
		{
			SetOperationResult("複製に失敗しました: " + error.message(), true);
			return;
		}

		SetOperationResult("複製しました: " + destination.filename().generic_string(), false);
		RefreshRegistry();
	}

	std::filesystem::path EditorContentBrowserPanel::MakeUniqueCopyPath(const EditorAssetData& asset) const
	{
		const std::filesystem::path parent = asset.absolutePath.parent_path();
		const std::string stem = asset.isDirectory
			? asset.absolutePath.filename().generic_string()
			: asset.absolutePath.stem().generic_string();
		const std::string extension = asset.isDirectory
			? std::string{}
			: asset.absolutePath.extension().generic_string();

		for (int copyIndex = 1; copyIndex < 10000; ++copyIndex)
		{
			const std::string suffix = copyIndex == 1
				? " コピー"
				: " コピー (" + std::to_string(copyIndex) + ")";
			const std::filesystem::path candidate = parent / (stem + suffix + extension);
			std::error_code error;
			if (!std::filesystem::exists(candidate, error))
			{
				return candidate;
			}
		}
		return parent / (stem + " コピー 最終" + extension);
	}

	void EditorContentBrowserPanel::SetOperationResult(std::string message, bool failed)
	{
		operationMessage_ = std::move(message);
		operationFailed_ = failed;
	}

	void EditorContentBrowserPanel::DrawSelectedAssetDetails()
	{
		ImGui::TextUnformatted("選択中のアセット");
		ImGui::Separator();

		const EditorAssetData* asset = registry_.FindById(selectedAssetId_);
		if (!asset)
		{
			ImGui::TextDisabled("アセットが選択されていません。");
			return;
		}

		if (asset->type == EditorAssetType::Texture &&
			EditorTexturePreviewCache::IsPreviewableImage(asset->extension))
		{
			const EditorTexturePreview& preview = texturePreviewCache_.GetOrLoad(asset->absolutePath);
			if (preview.loaded && preview.srvHandleGPU.ptr != 0)
			{
				const float maxWidth = std::max(80.0f, ImGui::GetContentRegionAvail().x);
				const ImVec2 imageSize = FitImageSize(preview.width, preview.height, ImVec2(maxWidth, 220.0f));
				ImGui::Image(static_cast<ImTextureID>(preview.srvHandleGPU.ptr), imageSize);
				ImGui::Spacing();
			}
		}

		DrawDetailRow("名前", asset->name);
		DrawDetailRow("種類", EditorAssetRegistryV2::GetTypeName(asset->type));
		DrawDetailRow("相対パス", asset->relativePath.generic_string());
		DrawDetailRow("更新日時", asset->modifiedTime);
		if (asset->isDirectory)
		{
			DrawDetailRow("項目数", std::to_string(registry_.CountChildren(asset->relativePath)));
			if (ImGui::Button("このフォルダを開く", ImVec2(-1.0f, 0.0f)))
			{
				NavigateTo(asset->relativePath, true);
			}
		}
		else
		{
			DrawDetailRow("拡張子", asset->extension.empty() ? "なし" : asset->extension);
			DrawDetailRow("サイズ", FormatBytes(asset->sizeBytes));
			DrawDetailRow("絶対パス", asset->absolutePath.generic_string());
			if (ImGui::Button("開く", ImVec2(-1.0f, 0.0f)))
			{
				OpenAsset(*asset);
			}
		}
		if (ImGui::Button("エクスプローラーで表示", ImVec2(-1.0f, 0.0f)))
		{
			RevealInExplorer(*asset);
		}
	}

	void EditorContentBrowserPanel::DrawDetailRow(const char* label, const std::string& value)
	{
		ImGui::TextDisabled("%s", label);
		ImGui::TextWrapped("%s", value.empty() ? "なし" : value.c_str());
		ImGui::Spacing();
	}

	std::string EditorContentBrowserPanel::FormatBytes(uintmax_t bytes)
	{
		std::ostringstream stream;
		if (bytes >= 1024ull * 1024ull)
		{
			stream.setf(std::ios::fixed);
			stream.precision(2);
			stream << static_cast<double>(bytes) / (1024.0 * 1024.0) << " MB";
		}
		else if (bytes >= 1024ull)
		{
			stream.setf(std::ios::fixed);
			stream.precision(2);
			stream << static_cast<double>(bytes) / 1024.0 << " KB";
		}
		else
		{
			stream << bytes << " bytes";
		}
		return stream.str();
	}

	std::string EditorContentBrowserPanel::TruncateText(const std::string& text, float maxWidth)
	{
		if (text.empty() || ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
		{
			return text;
		}

		std::string result;
		for (const char character : text)
		{
			const std::string candidate = result + character;
			if (ImGui::CalcTextSize((candidate + "...").c_str()).x > maxWidth)
			{
				break;
			}
			result = candidate;
		}
		return result + "...";
	}

	const char* EditorContentBrowserPanel::GetSortModeName(AssetSortMode mode)
	{
		switch (mode)
		{
		case AssetSortMode::ModifiedTime: return "更新日時";
		case AssetSortMode::Size: return "サイズ";
		case AssetSortMode::Name:
		default: return "名前";
		}
	}
#endif
} // namespace Ken4lowEngine
