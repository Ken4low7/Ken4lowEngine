#pragma once

#include "EditorAssetRegistryV2.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace Ken4lowEngine
{
	/// <summary>
	/// ImGuiのContent BrowserとMain Viewportで共有するDrag＆Drop Payload名です。
	/// </summary>
	inline constexpr const char* kEditorAssetDragDropPayloadType = "KEN4LOW_EDITOR_ASSET";

	/// <summary>
	/// ImGui::SetDragDropPayloadで安全にコピーできる固定長のアセット情報です。
	/// </summary>
	struct EditorAssetDragDropPayload
	{
		uint32_t assetType = static_cast<uint32_t>(EditorAssetType::Other);
		std::array<char, 512> relativePath{};
		std::array<char, 160> displayName{};
	};

	/// <summary>
	/// 現在Main Viewportへ配置できるアセット種別か判定します。
	/// </summary>
	inline bool IsViewportPlaceableAsset(EditorAssetType type)
	{
		return type == EditorAssetType::Model || type == EditorAssetType::ActorPrefab;
	}

	/// <summary>
	/// Content Browserのアセット情報から固定長Payloadを作成します。
	/// </summary>
	inline EditorAssetDragDropPayload MakeEditorAssetDragDropPayload(const EditorAssetData& asset)
	{
		EditorAssetDragDropPayload payload{};
		payload.assetType = static_cast<uint32_t>(asset.type);
		const std::string relativePath = asset.relativePath.generic_string();
		std::snprintf(payload.relativePath.data(), payload.relativePath.size(), "%s", relativePath.c_str());
		std::snprintf(payload.displayName.data(), payload.displayName.size(), "%s", asset.name.c_str());
		return payload;
	}

	/// <summary>
	/// Payloadへ保存された整数値をEditorAssetTypeへ戻します。
	/// </summary>
	inline EditorAssetType GetPayloadAssetType(const EditorAssetDragDropPayload& payload)
	{
		return static_cast<EditorAssetType>(payload.assetType);
	}
} // namespace Ken4lowEngine
