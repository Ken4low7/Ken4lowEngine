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

namespace
{
	constexpr const char* kAssetTypes[] = {
		"ExampleType", "LightPreset", "PostEffectPreset", "Object3DPreset",
		"SpritePreset", "ParticlePreset", "ModelPreset", "MaterialPreset"
	};

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
				ImGui::SeparatorText("PBR Material");
				changed |= ImGui::ColorEdit4("ベースカラー##MaterialPreset", &source.baseColorFactor.x);
				changed |= ImGui::DragFloat("メタリック##MaterialPreset", &source.metallicFactor, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::DragFloat("粗さ##MaterialPreset", &source.roughnessFactor, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::DragFloat("法線の強さ##MaterialPreset", &source.normalScale, 0.01f, 0.0f, 2.0f);
				changed |= ImGui::DragFloat("AOの強さ##MaterialPreset", &source.occlusionStrength, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::ColorEdit4("エミッシブカラー##MaterialPreset", &source.emissiveFactor.x);
				changed |= DrawMaterialTextureSelector("BaseColor Texture##MaterialPreset", source.baseColorTexturePath);
				changed |= DrawMaterialTextureSelector("MetallicRoughness Texture##MaterialPreset", source.metallicRoughnessTexturePath);
				changed |= DrawMaterialTextureSelector("Normal Texture##MaterialPreset", source.normalTexturePath);
				changed |= DrawMaterialTextureSelector("Occlusion Texture##MaterialPreset", source.occlusionTexturePath);
				changed |= DrawMaterialTextureSelector("Emissive Texture##MaterialPreset", source.emissiveTexturePath);
				ImGui::TextDisabled("5つのMaterial Texture Slotを全描画Componentへ反映します");
			}
			else
			{
				ImGui::SeparatorText("Legacy Material");
				changed |= ImGui::ColorEdit4("色##MaterialPreset", &source.legacyColor.x);
				changed |= ImGui::DragFloat("光沢度##MaterialPreset", &source.legacyShininess, 1.0f, 0.0f, 256.0f);
				changed |= ImGui::DragFloat("反射率##MaterialPreset", &source.legacyReflectionRate, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::DragFloat("粗さ##LegacyMaterialPreset", &source.legacyRoughness, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::Checkbox("ポイントサンプリング##MaterialPreset", &source.usePointSampling);
				changed |= DrawMaterialTextureSelector("BaseColor Texture##LegacyMaterialPreset", source.baseColorTexturePath);
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
		// SpritePresetをPreview用Spriteに適用して、JSON設定が実際の描画へ反映されることを確認できるようにする
		ApplySpritePreset(*spritePreview_, preset);
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
				entry.id = registry_.MakeUniqueId(baseId + "_" + std::to_string(suffix++)); // DefaultMaterialを含む既存共有IDとの衝突を避ける。
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
			entry.data = MaterialDescJsonConverter::ToJson(source); // 新規Materialは既存描画互換のLegacy既定値から開始する。
		}
		else { entry.data = nlohmann::json::object(); }
		entry.dirty = true;
		registry_.Register(entry);
		RegisterMaterialPreset(entry); // Save前でもModelComponentから新規共有Materialを選択可能にする。
	}

	void JsonEditorWindow::Draw(bool* pOpen)
	{
#ifdef USE_IMGUI
		if (!pOpen || !(*pOpen)) return;
		if (!ImGui::Begin("Json Asset Manager", pOpen))
		{
			ImGui::End();
			return;
		}

		ImGui::Checkbox("AutoSave", &autoSaveEnabled_);
		ImGui::SliderFloat("AutoSave Interval(sec)", &autoSaveIntervalSec_, 0.5f, 30.0f, "%.1f");
		ImGui::InputText("Asset Type Filter", typeFilter_, IM_ARRAYSIZE(typeFilter_));

		if (ImGui::Button("New")) { CreateNewAsset(); }
		ImGui::SameLine();
		if (ImGui::Button("Save") && selectedIndex_ >= 0)
		{
			auto& asset = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			if (JsonDataManager::SafeSave(asset))
			{
				asset.dirty = false;
				RegisterMaterialPreset(asset); // 保存時点のMaterialをRepositoryへ確実に同期する。
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload") && selectedIndex_ >= 0)
		{
			auto& asset = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			if (JsonDataManager::SafeLoad(asset.path, asset))
			{
				RegisterMaterialPreset(asset); // 外部編集されたJSONをReloadした場合も描画へ反映する。
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Duplicate") && selectedIndex_ >= 0)
		{
			auto& src = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			JsonAssetEntry dup;
			const std::string newId = registry_.MakeUniqueId(src.id + "_copy");
			const std::string dstPath = GetDataAssetFolder(basePath_, src.type) + newId + ".json";
			if (JsonDataManager::Duplicate(src, dstPath, newId, dup))
			{
				registry_.Register(dup);
				RegisterMaterialPreset(dup); // 複製したMaterialも独立した共有Assetとして登録する。
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete") && selectedIndex_ >= 0)
		{
			auto& src = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && JsonDataManager::Delete(src.path))
			{
				if (src.type == "MaterialPreset")
				{
					MaterialRepository::GetInstance()->Unregister(src.id); // 削除済みMaterialを選択候補と描画更新対象から外す。
				}
				registry_.RemoveById(src.id);
				selectedIndex_ = -1;
			}
		}

		ImGui::Combo("New Type", &newTypeIndex_, kAssetTypes, IM_ARRAYSIZE(kAssetTypes));
		ImGui::InputText("New Id", newId_, IM_ARRAYSIZE(newId_));
		ImGui::InputText("New DisplayName", newDisplayName_, IM_ARRAYSIZE(newDisplayName_));

		auto indices = registry_.CollectFilteredIndices(typeFilter_);
		ImGui::SeparatorText("Asset List");
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
			ImGui::SeparatorText("Selected Asset Detail");
			ImGui::Text("Type: %s", asset.type.c_str());
			ImGui::Text("Dirty: %s", asset.dirty ? "true" : "false");
			ImGui::TextWrapped("File Path: %s", asset.path.c_str());
			char displayNameBuffer[128]{};
			std::snprintf(displayNameBuffer, sizeof(displayNameBuffer), "%s", asset.displayName.c_str());
			if (ImGui::InputText("Display Name", displayNameBuffer, IM_ARRAYSIZE(displayNameBuffer)))
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
				if (ImGui::InputText("Texture Path", texturePathBuffer, IM_ARRAYSIZE(texturePathBuffer))) { preset.texturePath = texturePathBuffer; asset.dirty = true; }
				if (ImGui::InputFloat2("Position", &preset.position.x)) { asset.dirty = true; }
				if (ImGui::InputFloat2("Size", &preset.size.x)) { asset.dirty = true; }
				if (ImGui::InputFloat("Rotation", &preset.rotation)) { asset.dirty = true; }
				if (ImGui::InputFloat2("Anchor", &preset.anchor.x)) { asset.dirty = true; }
				if (ImGui::ColorEdit4("Color", &preset.color.x)) { asset.dirty = true; }
				if (ImGui::Checkbox("Visible", &preset.visible)) { asset.dirty = true; }
				if (ImGui::InputInt("DrawOrder", &preset.drawOrder)) { asset.dirty = true; }
				if (ImGui::InputFloat2("Texture LeftTop(px)", &preset.textureLeftTop.x)) { asset.dirty = true; }
				if (ImGui::InputFloat2("Texture Size(px, 0=full)", &preset.textureSize.x)) { asset.dirty = true; }
				ImGui::TextDisabled("Not applied yet: layer / pivot / enableAlpha / drawOrder");
				if (ImGui::Button("Apply Selected SpritePreset to Test Sprite"))
				{
					ApplySelectedSpritePresetToPreview();
				}
				if (!lastPreviewPresetId_.empty())
				{
					ImGui::Text("Preview Applied: %s", lastPreviewPresetId_.c_str());
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
					RegisterMaterialPreset(asset); // 編集中の共有Materialを次のModelComponent更新へ即時反映する。
				}
				ImGui::TextDisabled("共有Material ID: %s", asset.id.c_str());
			}
		}

		if (spritePreview_)
		{
			spritePreview_->Update();
			spritePreview_->Draw();
		}

		ImGui::End();
#else
		(void)pOpen; // 未使用警告回避
#endif
	}
}
