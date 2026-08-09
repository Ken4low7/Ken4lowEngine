#pragma once

#include "EditorActorStateRegistry.h"
#include "EditorSelection.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace Ken4lowEngine
{
	/// <summary>
	/// Place ActorsからViewport配置へ渡す生成対象の種類です。
	/// </summary>
	enum class EditorPlaceableType : uint8_t
	{
		None = 0,
		EmptyActor,
		Cube,
		Sphere,
		Plane,
		DirectionalLight,
		PointLight,
		SpotLight,
		TriggerBox,
		TriggerSphere,
	};

	/// <summary>
	/// Place Actorsで選択された配置要求を保持します。
	/// </summary>
	struct EditorPlacementRequest
	{
		EditorPlaceableType type = EditorPlaceableType::None;
		std::string displayName;
		uint64_t serial = 0;
		bool pending = false;
	};

	/// <summary>
	/// Editor全体で共有する選択状態、Level状態、配置要求を一元管理します。
	/// </summary>
	class EditorContext
	{
	public:
		static EditorContext* GetInstance()
		{
			static EditorContext instance;
			return &instance;
		}

		EditorSelection& GetSelection() { return selection_; }
		const EditorSelection& GetSelection() const { return selection_; }

		void SetActiveLevelName(std::string levelName)
		{
			activeLevelName_ = std::move(levelName);
		}

		const std::string& GetActiveLevelName() const { return activeLevelName_; }

		void MarkLevelDirty(bool dirty = true) { levelDirty_ = dirty; }
		bool IsLevelDirty() const { return levelDirty_; }

		void QueuePlacement(EditorPlaceableType type, std::string_view displayName)
		{
			placementRequest_.type = type;
			placementRequest_.displayName.assign(displayName.begin(), displayName.end());
			placementRequest_.serial = ++placementSerial_;
			placementRequest_.pending = type != EditorPlaceableType::None;
			// Phase 6ではこの要求をViewportのRaycast位置へ生成するCommandへ接続する。
		}

		const EditorPlacementRequest& GetPlacementRequest() const { return placementRequest_; }
		bool HasPendingPlacement() const { return placementRequest_.pending; }

		void ClearPlacementRequest()
		{
			placementRequest_.type = EditorPlaceableType::None;
			placementRequest_.displayName.clear();
			placementRequest_.pending = false;
		}

		void ResetTransientState()
		{
			selection_.Clear();
			ClearPlacementRequest();
			EditorActorStateRegistry::GetInstance()->Clear(); // Scene切り替え時に古いActorポインタへ紐づくOutliner状態を破棄する。
		}

	private:
		EditorContext() = default;
		~EditorContext() = default;
		EditorContext(const EditorContext&) = delete;
		EditorContext& operator=(const EditorContext&) = delete;

		EditorSelection selection_{};
		std::string activeLevelName_ = "Untitled";
		bool levelDirty_ = false;
		uint64_t placementSerial_ = 0;
		EditorPlacementRequest placementRequest_{};
	};
} // namespace Ken4lowEngine
