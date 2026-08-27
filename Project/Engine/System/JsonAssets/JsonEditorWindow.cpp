#include "JsonEditorWindow.h"
#include "JsonDataManager.h"
#include "ExampleJsonAsset.h"
#include "DataAssetPresets.h"
#include "AssetPathSelector.h"
#include "MaterialDescJsonConverter.h"
#include "MaterialDescLoader.h"
#include "MaterialRepository.h"
#include "Sprite.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string_view>

namespace
{
	constexpr const char* kAssetTypes[] = {
		"ExampleType", "LightPreset", "PostEffectPreset", "Object3DPreset",
		"SpritePreset", "ParticlePreset", "ModelPreset", "MaterialPreset"
	};

	constexpr const char* kAssetTypeDisplayNames[] = {
		"サンプル", "ライトプリセット", "ポストエフェクトプリセット", "3Dオブジェクトプリセット",
		"スプライトプリセット", "パーティクルプリセット", "モデルプリセット", "マテリアルプリセット"
	};

	const char* GetAssetTypeDisplayName(std::string_view type)
	{
		for (size_t i = 0; i < std::size(kAssetTypes); ++i)
		{
			if (type == kAssetTypes[i]) return kAssetTypeDisplayNames[i];
		}
		return "不明";
	}

	std::string GetDataAssetFolder(const std::string& basePath, const std::string& type)
	{
		std::string folder = basePath + "/";
		if (type == "LightPreset") { folder += "LightPresets/"; }
		else if (type == "PostEffectPreset") { folder += "PostEffectPresets/"; }
		else if (type == "Object3DPreset") { folder += "Object3DPresets/"; }
		else if (type == "SpritePreset") { folder += "SpritePresets/"; }
		else if (type == "ParticlePreset") { folder += "ParticlePresets/"; }
		else if (type == "MaterialPreset") { folder += "Materials/"; }
		return folder;
	}
}

namespace Ken4lowEngine
{
	namespace
	{
		void RegisterMaterialPreset(const JsonAssetEntry& entry)
		{
			if (entry.type != "MaterialPreset" || entry.id.empty())
			{
				return;
			}

			MaterialDescSource source = MaterialDescJsonConverter::FromJson(entry.data);
			source.materialId = entry.id;
			source.materialName = entry.displayName.empty() ? entry.id : entry.displayName;
			source.sourceKind = MaterialSourceKind::MaterialEditor;
			const MaterialDesc desc = MaterialDescLoader::CreateFromSource(source);
			MaterialRepository::GetInstance()->CreateOrReplace(entry.id, desc, source.materialName); // JSON Assetを共有Materialの実体へ同期する。
		}

#ifdef USE_IMGUI
		bool DrawMaterialTextureSelector(const char* label, std::string& value)
		{
			return AssetPathSelector::DrawAssetSelector(label, value, AssetType::Texture); // 存在するDDSだけを選ばせ、入力途中の無効パスを描画へ渡さない。
		}

		bool DrawMaterialPresetEditor(JsonAssetEntry& asset)
		{
			MaterialDescSource source = MaterialDescJsonConverter::FromJson(asset.data);
			source.materialId = asset.id;
			source.materialName = asset.displayName;
			source.sourceKind = MaterialSourceKind::MaterialEditor;

			bool changed = false;
			changed |= ImGui::Checkbox("PBRを使用##MaterialPreset", &source.preferPbrWorkflow);
			if (source.preferPbrWorkflow)
			{
				ImGui::SeparatorText("PBRマテリアル");
				changed |= ImGui::ColorEdit4("ベースカラー##MaterialPreset", &source.baseColorFactor.x);
				changed |= ImGui::DragFloat("メタリック##MaterialPreset", &source.metallicFactor, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::DragFloat("粗さ##MaterialPreset", &source.roughnessFactor, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::DragFloat("法線の強さ##MaterialPreset", &source.normalScale, 0.01f, 0.0f, 2.0f);
				changed |= ImGui::DragFloat("AOの強さ##MaterialPreset", &source.occlusionStrength, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::ColorEdit4("エミッシブカラー##MaterialPreset", &source.emissiveFactor.x);
				changed |= DrawMaterialTextureSelector("ベースカラーテクスチャ##MaterialPreset", source.baseColorTexturePath);
				changed |= DrawMaterialTextureSelector("メタリック・粗さテクスチャ##MaterialPreset", source.metallicRoughnessTexturePath);
				changed |= DrawMaterialTextureSelector("法線テクスチャ##MaterialPreset", source.normalTexturePath);
				changed |= DrawMaterialTextureSelector("オクルージョンテクスチャ##MaterialPreset", source.occlusionTexturePath);
				changed |= DrawMaterialTextureSelector("エミッシブテクスチャ##MaterialPreset", source.emissiveTexturePath);
				ImGui::TextDisabled("5種類のマテリアル用テクスチャを各描画コンポーネントへ反映します。");
			}
			else
			{
				ImGui::SeparatorText("従来マテリアル");
				changed |= ImGui::ColorEdit4("色##MaterialPreset", &source.legacyColor.x);
				changed |= ImGui::DragFloat("光沢度##MaterialPreset", &source.legacyShininess, 1.0f, 0.0f, 256.0f);
				changed |= ImGui::DragFloat("反射率##MaterialPreset", &source.legacyReflectionRate, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::DragFloat("粗さ##LegacyMaterialPreset", &source.legacyRoughness, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::Checkbox("ポイントサンプリング##MaterialPreset", &source.usePointSampling);
				changed |= DrawMaterialTextureSelector("ベースカラーテクスチャ##LegacyMaterialPreset", source.baseColorTexturePath);
			}

			if (changed)
			{
				asset.data = MaterialDescJsonConverter::ToJson(source); // 編集値を既存Material JSON規約へ戻して保存対象にする。
				asset.dirty = true;
			}
			return changed;
		}
#endif // USE_IMGUI
	}

	JsonEditorWindow* JsonEditorWindow::GetInstance()
	{
		static JsonEditorWindow instance;
		return &instance;
	}

	void JsonEditorWindow::Initialize()
	{
		MaterialRepository::GetInstance(); // Default Materialを先に登録してからJSON Assetを上書き登録する。
		LoadAssetsFromDirectory(basePath_);
	}

	void JsonEditorWindow::LoadAssetsFromDirectory(const std::string& rootDirectory)
	{
		namespace fs = std::filesystem;
		if (!fs::exists(rootDirectory)) { return; }
		for (const auto& entry : fs::recursive_directory_iterator(rootDirectory))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".json") { continue; }
			JsonAssetEntry loaded{};
			if (!JsonDataManager::SafeLoad(entry.path().string(), loaded)) { continue; }
			if (loaded.id.empty()) { loaded.id = entry.path().stem().string(); }
			if (loaded.displayName.empty()) { loaded.displayName = loaded.id; }
			registry_.Register(loaded);
			RegisterMaterialPreset(loaded); // 起動時に保存済みMaterialPresetを共有Repositoryへ復元する。
		}
	}

	void JsonEditorWindow::ApplySelectedSpritePresetToPreview()
	{
		if (selectedIndex_ < 0) { return; }
		auto& asset = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
		if (asset.type != "SpritePreset") { return; }
		SpritePreset preset{};
		preset.FromJson(asset.data);
		if (preset.texturePath.empty()) { return; }
		if (!spritePreview_)
		{
			spritePreview_ = std::make_unique<Sprite>();
			spritePreview_->Initialize(preset.texturePath);
		}
		ApplySpritePreset(*spritePreview_, preset); // スプライトプリセットの実描画結果をプレビューで確認できるようにする。
		lastPreviewPresetId_ = asset.id;
	}

	void JsonEditorWindow::Update(float deltaTime)
	{
		TryAutoSave(deltaTime);
	}

	void JsonEditorWindow::TryAutoSave(float deltaTime)
	{
		if (!autoSaveEnabled_)
		{
			return;
		}
		autoSaveElapsedSec_ += deltaTime;
		if (autoSaveElapsedSec_ < autoSaveIntervalSec_)
		{
			return;
		}
		autoSaveElapsedSec_ = 0.0f;
		for (auto& asset : const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets()))
		{
			if (!asset.dirty) { continue; }
			if (JsonDataManager::SafeSave(asset)) { asset.dirty = false; }
		}
	}

	void JsonEditorWindow::CreateNewAsset()
	{
		JsonAssetEntry entry;
		entry.type = kAssetTypes[newTypeIndex_];
		entry.id = registry_.MakeUniqueId(newId_);
		if (entry.type == "MaterialPreset")
		{
			const std::string baseId = entry.id.empty() ? "Material" : entry.id;
			int suffix = 1;
			while (MaterialRepository::GetInstance()->Contains(entry.id))
			{
				entry.id = registry_.MakeUniqueId(baseId + "_" + std::to_string(suffix++));
			}
		}
		entry.displayName = newDisplayName_;
		std::string folder = GetDataAssetFolder(basePath_, entry.type);
		entry.path = folder + entry.id + ".json";
		if (entry.type == "LightPreset") { LightPreset preset; preset.ToJson(entry.data); }
		else if (entry.type == "PostEffectPreset") { PostEffectPreset preset; preset.ToJson(entry.data); }
		else if (entry.type == "Object3DPreset") { Object3DPreset preset; preset.ToJson(entry.data); }
		else if (entry.type == "SpritePreset") { SpritePreset preset; preset.ToJson(entry.data); }
		else if (entry.type == "ParticlePreset") { ParticlePreset preset; preset.ToJson(entry.data); }
		else if (entry.type == "MaterialPreset")
		{
			MaterialDescSource source{};
			source.materialId = entry.id;
			source.materialName = entry.displayName;
			source.sourceKind = MaterialSourceKind::MaterialEditor;
			entry.data = MaterialDescJsonConverter::ToJson(source);
		}
		else { entry.data = nlohmann::json::object(); }
		entry.dirty = true;
		registry_.Register(entry);
		RegisterMaterialPreset(entry);
	}

	void JsonEditorWindow::Draw(bool* pOpen)
	{
#ifdef USE_IMGUI
		if (!pOpen || !(*pOpen)) return;
		if (!ImGui::Begin("JSONアセット管理###Json Asset Manager", pOpen))
		{
			ImGui::End();
			return;
		}

		// JSONアセット編集で利用者が直接触る項目は日本語表記へ統一する。
		ImGui::Checkbox("自動保存", &autoSaveEnabled_);
		ImGui::SliderFloat("自動保存間隔（秒）", &autoSaveIntervalSec_, 0.5f, 30.0f, "%.1f");
		ImGui::InputText("アセット種類フィルター", typeFilter_, IM_ARRAYSIZE(typeFilter_));

		if (ImGui::Button("新規作成")) { CreateNewAsset(); }
		ImGui::SameLine();
		if (ImGui::Button("保存") && selectedIndex_ >= 0)
		{
			auto& asset = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			if (JsonDataManager::SafeSave(asset))
			{
				asset.dirty = false;
				RegisterMaterialPreset(asset);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("再読み込み") && selectedIndex_ >= 0)
		{
			auto& asset = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			if (JsonDataManager::SafeLoad(asset.path, asset))
			{
				RegisterMaterialPreset(asset);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("複製") && selectedIndex_ >= 0)
		{
			auto& src = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			JsonAssetEntry dup;
			const std::string newId = registry_.MakeUniqueId(src.id + "_copy");
			const std::string dstPath = GetDataAssetFolder(basePath_, src.type) + newId + ".json";
			if (JsonDataManager::Duplicate(src, dstPath, newId, dup))
			{
				registry_.Register(dup);
				RegisterMaterialPreset(dup);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("削除") && selectedIndex_ >= 0)
		{
			auto& src = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && JsonDataManager::Delete(src.path))
			{
				if (src.type == "MaterialPreset")
				{
					MaterialRepository::GetInstance()->Unregister(src.id);
				}
				registry_.RemoveById(src.id);
				selectedIndex_ = -1;
			}
		}

		ImGui::Combo("新規アセット種類", &newTypeIndex_, kAssetTypeDisplayNames, IM_ARRAYSIZE(kAssetTypeDisplayNames));
		ImGui::InputText("新規ID", newId_, IM_ARRAYSIZE(newId_));
		ImGui::InputText("新規表示名", newDisplayName_, IM_ARRAYSIZE(newDisplayName_));

		auto indices = registry_.CollectFilteredIndices(typeFilter_);
		ImGui::SeparatorText("アセット一覧");
		for (size_t idx : indices)
		{
			auto& a = registry_.GetAssets()[idx];
			if (ImGui::Selectable((a.id + "##" + std::to_string(idx)).c_str(), selectedIndex_ == static_cast<int>(idx)))
			{
				selectedIndex_ = static_cast<int>(idx);
			}
		}

		if (selectedIndex_ >= 0)
		{
			auto& asset = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			bool metadataChanged = false;
			ImGui::SeparatorText("選択中アセットの詳細");
			ImGui::Text("種類: %s", GetAssetTypeDisplayName(asset.type));
			ImGui::Text("未保存の変更: %s", asset.dirty ? "あり" : "なし");
			ImGui::TextWrapped("ファイルパス: %s", asset.path.c_str());
			char displayNameBuffer[128]{};
			std::snprintf(displayNameBuffer, sizeof(displayNameBuffer), "%s", asset.displayName.c_str());
			if (ImGui::InputText("表示名", displayNameBuffer, IM_ARRAYSIZE(displayNameBuffer)))
			{
				asset.displayName = displayNameBuffer;
				asset.dirty = true;
				metadataChanged = true;
			}

			if (asset.type == "SpritePreset")
			{
				SpritePreset preset{};
				preset.FromJson(asset.data);

				char texturePathBuffer[256]{};
				std::snprintf(texturePathBuffer, sizeof(texturePathBuffer), "%s", preset.texturePath.c_str());
				if (ImGui::InputText("テクスチャパス", texturePathBuffer, IM_ARRAYSIZE(texturePathBuffer))) { preset.texturePath = texturePathBuffer; asset.dirty = true; }
				if (ImGui::InputFloat2("位置", &preset.position.x)) { asset.dirty = true; }
				if (ImGui::InputFloat2("サイズ", &preset.size.x)) { asset.dirty = true; }
				if (ImGui::InputFloat("回転", &preset.rotation)) { asset.dirty = true; }
				if (ImGui::InputFloat2("アンカー", &preset.anchor.x)) { asset.dirty = true; }
				if (ImGui::ColorEdit4("色", &preset.color.x)) { asset.dirty = true; }
				if (ImGui::Checkbox("表示", &preset.visible)) { asset.dirty = true; }
				if (ImGui::InputInt("描画順", &preset.drawOrder)) { asset.dirty = true; }
				if (ImGui::InputFloat2("テクスチャ左上位置（px）", &preset.textureLeftTop.x)) { asset.dirty = true; }
				if (ImGui::InputFloat2("テクスチャサイズ（px、0で全体）", &preset.textureSize.x)) { asset.dirty = true; }
				ImGui::TextDisabled("未反映項目: レイヤー / ピボット / アルファ有効 / 描画順");
				if (ImGui::Button("選択中のスプライトプリセットをテスト表示へ適用"))
				{
					ApplySelectedSpritePresetToPreview();
				}
				if (!lastPreviewPresetId_.empty())
				{
					ImGui::Text("プレビュー適用済み: %s", lastPreviewPresetId_.c_str());
				}

				if (asset.dirty)
				{
					preset.ToJson(asset.data);
				}
			}
			else if (asset.type == "MaterialPreset")
			{
				const bool materialChanged = DrawMaterialPresetEditor(asset);
				if (materialChanged || metadataChanged)
				{
					RegisterMaterialPreset(asset);
				}
				ImGui::TextDisabled("共有マテリアルID: %s", asset.id.c_str());
			}
		}

		if (spritePreview_)
		{
			spritePreview_->Update();
			spritePreview_->Draw();
		}

		ImGui::End();
#else
		(void)pOpen;
#endif
	}
}
