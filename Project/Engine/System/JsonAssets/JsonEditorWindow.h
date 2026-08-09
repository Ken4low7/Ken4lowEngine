#pragma once

#include "JsonAssetRegistry.h"

#include <memory>

namespace Ken4lowEngine
{
	class Sprite;

	class JsonEditorWindow
	{
	public:
		static JsonEditorWindow* GetInstance();
		void Initialize();
		void Update(float deltaTime);
		void Draw(bool* pOpen);

	private:
		void CreateNewAsset();
		void TryAutoSave(float deltaTime);
		void LoadAssetsFromDirectory(const std::string& rootDirectory);
		void ApplySelectedSpritePresetToPreview();

		JsonAssetRegistry registry_{};
		int selectedIndex_ = -1;
		bool autoSaveEnabled_ = false;
		float autoSaveIntervalSec_ = 2.0f;
		float autoSaveElapsedSec_ = 0.0f;
		char typeFilter_[64] = "All";
		int newTypeIndex_ = 0;
		char newId_[64] = "example_asset";
		char newDisplayName_[64] = "Example Asset";
		char basePath_[256] = "Project/Resources/DataAssets";
		std::unique_ptr<Sprite> spritePreview_{};
		std::string lastPreviewPresetId_{};
	};
}
