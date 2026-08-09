#pragma once

#include "EditorAssetDragDrop.h"
#include "EditorAssetRegistryV2.h"
#include "EditorGpuPickingManager.h"
#include "EditorPlayController.h"
#include "EditorTexturePreviewCache.h"
#include "EditorTransformGizmo.h"
#include "EditorViewportController.h"
#include "EditorWindowManager.h"

#include <GameViewportConstants.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>
	/// UE風のフォルダツリー・パンくず・アセットグリッドを持つコンテンツブラウザです。
	/// </summary>
	class EditorContentBrowserPanel
	{
	public:
		static EditorContentBrowserPanel* GetInstance();

		void Draw();

		/// <summary>
		/// 選択中のモデルまたはActor PrefabをMain Viewportへ渡すDrag Sourceを更新します。
		/// </summary>
		void UpdateAssetDragSource()
		{
#ifdef USE_IMGUI
			const EditorAssetData* asset = registry_.FindById(selectedAssetId_);
			if (!asset || asset->isDirectory || !IsViewportPlaceableAsset(asset->type))
			{
				return;
			}

			// 同じContent Browserウィンドウへ再度入って、子領域上のドラッグを外部Sourceとして登録する。
			if (!ImGui::Begin("コンテンツブラウザ###Content Browser", nullptr, ImGuiWindowFlags_NoCollapse))
			{
				ImGui::End();
				return;
			}

			const bool canStartDrag =
				ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
				ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f);

			if (canStartDrag && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern))
			{
				const EditorAssetDragDropPayload payload = MakeEditorAssetDragDropPayload(*asset);
				ImGui::SetDragDropPayload(kEditorAssetDragDropPayloadType, &payload, sizeof(payload));
				ImGui::TextUnformatted("ビューポートへ配置");
				ImGui::Text("%s", asset->name.c_str());
				ImGui::TextDisabled("種類: %s", EditorAssetRegistryV2::GetTypeName(asset->type));
				ImGui::EndDragDropSource();
			}
			ImGui::End();
#endif
		}

		/// <summary>Gizmo判定後のMain ViewportクリックをObject-ID Pickingへ渡します。</summary>
		void UpdateViewportPicking()
		{
#ifdef USE_IMGUI
			UpdateViewportPickingInternal();
#endif
		}

	private:
		enum class AssetSortMode
		{
			Name,
			ModifiedTime,
			Size,
		};

		EditorContentBrowserPanel() = default;
		~EditorContentBrowserPanel() = default;
		EditorContentBrowserPanel(const EditorContentBrowserPanel&) = delete;
		EditorContentBrowserPanel& operator=(const EditorContentBrowserPanel&) = delete;

#ifdef USE_IMGUI
		void UpdateViewportPickingInternal()
		{
			auto* viewportController = EditorViewportController::GetInstance();
			const EditorTransformGizmo* transformGizmo = EditorTransformGizmo::GetInstance();
			if (!viewportController->IsEditorDisplay() || transformGizmo->IsOver() || transformGizmo->IsUsing() ||
				EditorPlayController::GetInstance()->IsPlaying() || ImGui::GetDragDropPayload() != nullptr ||
				!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				return;
			}

			const EditorViewportRect& viewportRect = EditorWindowManager::GetInstance()->GetMainViewportRect();
			if (!viewportRect.valid || !viewportRect.isHovered || viewportRect.imageSize.x <= 1.0f || viewportRect.imageSize.y <= 1.0f)
			{
				return; // 他のEditorウィンドウに遮られたクリックをMain Viewport Pickingへ流さない。
			}

			const ImVec2 mouse = ImGui::GetMousePos();
			const bool insideViewport =
				mouse.x >= viewportRect.screenMin.x && mouse.y >= viewportRect.screenMin.y &&
				mouse.x <= viewportRect.screenMax.x && mouse.y <= viewportRect.screenMax.y;
			const bool insideToolbar = mouse.y <= viewportRect.screenMin.y + 58.0f;
			if (!insideViewport || insideToolbar)
			{
				return;
			}

			const float localX = mouse.x - viewportRect.screenMin.x;
			const float localY = mouse.y - viewportRect.screenMin.y;
			const float scaledX = (localX / viewportRect.imageSize.x) * static_cast<float>(GameViewportConstants::Width);
			const float scaledY = (localY / viewportRect.imageSize.y) * static_cast<float>(GameViewportConstants::Height);
			const uint32_t pixelX = static_cast<uint32_t>(std::clamp(
				std::floor(scaledX),
				0.0f,
				static_cast<float>(GameViewportConstants::Width - 1u)));
			const uint32_t pixelY = static_cast<uint32_t>(std::clamp(
				std::floor(scaledY),
				0.0f,
				static_cast<float>(GameViewportConstants::Height - 1u)));

			EditorGpuPickingManager::GetInstance()->RequestPick(
				pixelX,
				pixelY,
				viewportController->GetSelectionMode()); // Gizmo操作以外のクリックはToolを切り替えずObject選択へ使う。
		}

		void InitializeIfNeeded();
		void RefreshRegistry();
		void NavigateTo(const std::filesystem::path& directory, bool recordHistory);

		bool CanNavigateBack() const
		{
			return historyIndex_ > 0 && !history_.empty();
		}

		bool CanNavigateForward() const
		{
			return !history_.empty() && historyIndex_ + 1 < history_.size();
		}

		void NavigateBack();
		void NavigateForward();
		void NavigateUp();

		void DrawToolbar();
		void DrawBreadcrumbs();
		void DrawBrowserBody();
		void DrawFolderTree();
		void DrawFolderNode(const EditorAssetData& directory);
		void DrawAssetGrid();
		void DrawAssetCard(const EditorAssetData& asset, float width, bool showRelativePath);
		void DrawAssetContextMenu(const EditorAssetData& asset);
		void DrawPendingDialogs();
		void DrawSelectedAssetDetails();

		std::vector<const EditorAssetData*> BuildVisibleEntries() const;
		void OpenAsset(const EditorAssetData& asset) const;
		void RevealInExplorer(const EditorAssetData& asset) const;
		void OpenCurrentFolderInExplorer() const;
		void CopyPathToClipboard(const EditorAssetData& asset) const;
		void BeginRename(const EditorAssetData& asset);
		void BeginDelete(const EditorAssetData& asset);
		void ProcessDeferredDuplicate();
		void ApplyRename();
		void ApplyDelete();
		void DuplicateAsset(const EditorAssetData& asset);
		std::filesystem::path MakeUniqueCopyPath(const EditorAssetData& asset) const;
		void SetOperationResult(std::string message, bool failed);

		static void DrawDetailRow(const char* label, const std::string& value);
		static std::string FormatBytes(uintmax_t bytes);
		static std::string TruncateText(const std::string& text, float maxWidth);
		static const char* GetSortModeName(AssetSortMode mode);
#endif

	private:
		EditorAssetRegistryV2 registry_{};
		EditorTexturePreviewCache texturePreviewCache_{}; // 表示中のテクスチャだけをSRV化してサムネイルへ再利用する。
		std::filesystem::path currentDirectory_{};
		std::vector<std::filesystem::path> history_{};
		std::size_t historyIndex_ = 0;
		uint64_t selectedAssetId_ = 0;
		EditorAssetType typeFilter_ = EditorAssetType::All;
		AssetSortMode sortMode_ = AssetSortMode::Name;
		std::array<char, 160> searchBuffer_{};
		std::array<char, 260> renameBuffer_{};
		float cellWidth_ = 128.0f;
		bool initialized_ = false;
		bool searchSubfolders_ = true;
		bool showFolders_ = true;
		bool sortAscending_ = true;
		uint64_t renameAssetId_ = 0;
		uint64_t deleteAssetId_ = 0;
		uint64_t duplicateAssetId_ = 0;
		bool openRenamePopup_ = false;
		bool openDeletePopup_ = false;
		bool duplicateRequested_ = false;
		std::string operationMessage_;
		bool operationFailed_ = false;
	};
} // namespace Ken4lowEngine
