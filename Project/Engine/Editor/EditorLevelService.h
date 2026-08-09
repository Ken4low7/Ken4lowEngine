#pragma once

#include "EditorActorStateRegistry.h"
#include "EditorCommandHistory.h"
#include "EditorContext.h"
#include "EditorPlayController.h"

#include <ActorJsonSerializer.h>
#include <ActorWorld.h>
#include <BaseScene.h>
#include <CameraComponent.h>
#include <CameraManager.h>
#include <DebugCamera.h>
#include <LightManager.h>
#include <SceneComponent.h>
#include <SceneManager.h>
#include <ShadowSettings.h>
#include <json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>
	/// Editorで編集中のActorWorldをLevel JSONとして保存・復元します。
	/// Actor Prefabとは分離し、Level全体のActor、親子関係、Editor状態、Lighting、Editor Cameraを扱います。
	/// </summary>
	class EditorLevelService
	{
	public:
		static EditorLevelService* GetInstance()
		{
			static EditorLevelService instance;
			return &instance;
		}

		void SetSceneManager(SceneManager* sceneManager)
		{
			sceneManager_ = sceneManager;
			EnsureInitialized();
		}

		void Update(float deltaTime)
		{
			EnsureInitialized();
			if (!autoSaveEnabled_ || !EditorContext::GetInstance()->IsLevelDirty() ||
				!EditorPlayController::GetInstance()->IsEditing())
			{
				autoSaveElapsed_ = 0.0f;
				return;
			}

			autoSaveElapsed_ += (std::max)(0.0f, deltaTime);
			if (autoSaveElapsed_ < autoSaveIntervalSeconds_) return;
			autoSaveElapsed_ = 0.0f;

			const std::string stem = currentLevelPath_.empty()
				? "Untitled"
				: currentLevelPath_.stem().string();
			const std::filesystem::path autoSavePath = kAutoSaveDirectory / (stem + "_Autosave.json");
			SaveLevelToPath(autoSavePath, false, false, false, "Auto Save"); // Auto Saveでは現在のLevelパスとDirty状態を変更しない。
		}

#ifdef USE_IMGUI
		void UpdateShortcuts()
		{
			const ImGuiIO& io = ImGui::GetIO();
			if (io.WantTextInput || ImGui::IsAnyItemActive() || !io.KeyCtrl) return;

			if (ImGui::IsKeyPressed(ImGuiKey_N, false)) RequestNewLevel();
			if (ImGui::IsKeyPressed(ImGuiKey_O, false)) RequestOpenLevel();
			if (ImGui::IsKeyPressed(ImGuiKey_S, false))
			{
				if (io.KeyShift) RequestSaveLevelAs();
				else RequestSaveLevel();
			}
		}

		void DrawFileMenuItems()
		{
			const bool canEdit = EditorPlayController::GetInstance()->IsEditing();
			if (ImGui::MenuItem("New Level", "Ctrl+N", false, canEdit)) RequestNewLevel();
			if (ImGui::MenuItem("Open Level...", "Ctrl+O", false, canEdit)) RequestOpenLevel();
			if (ImGui::MenuItem("Save Level", "Ctrl+S", false, canEdit)) RequestSaveLevel();
			if (ImGui::MenuItem("Save Level As...", "Ctrl+Shift+S", false, canEdit)) RequestSaveLevelAs();

			if (ImGui::BeginMenu("Recent Levels", !recentLevels_.empty() && canEdit))
			{
				for (const std::string& recentPath : recentLevels_)
				{
					const std::filesystem::path path(recentPath);
					const std::string label = path.filename().string() + "##" + recentPath;
					if (ImGui::MenuItem(label.c_str())) RequestOpenLevelPath(path);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", recentPath.c_str());
				}
				ImGui::EndMenu();
			}

			ImGui::Separator();
			if (ImGui::BeginMenu("Auto Save"))
			{
				ImGui::MenuItem("Enabled", nullptr, &autoSaveEnabled_);
				ImGui::SetNextItemWidth(130.0f);
				ImGui::DragFloat("Interval (sec)", &autoSaveIntervalSeconds_, 1.0f, 15.0f, 1800.0f, "%.0f");
				ImGui::TextDisabled("Save to: %s", kAutoSaveDirectory.generic_string().c_str());
				ImGui::EndMenu();
			}
		}

		void DrawDialogs()
		{
			if (requestOpenDialog_)
			{
				RefreshLevelFileList();
				ImGui::OpenPopup("Open Level##EditorLevelOpen");
				requestOpenDialog_ = false;
			}
			if (requestSaveAsDialog_)
			{
				PrepareSaveAsBuffer();
				ImGui::OpenPopup("Save Level As##EditorLevelSaveAs");
				requestSaveAsDialog_ = false;
			}
			if (requestUnsavedDialog_)
			{
				ImGui::OpenPopup("Unsaved Level##EditorLevelUnsaved");
				requestUnsavedDialog_ = false;
			}

			DrawOpenDialog();
			DrawSaveAsDialog();
			DrawUnsavedDialog();
		}
#endif

		void RequestNewLevel()
		{
			if (!CanModifyLevel()) return;
			BeginDestructiveAction(PendingAction::NewLevel, {});
		}

		void RequestOpenLevel()
		{
			if (!CanModifyLevel()) return;
			requestOpenDialog_ = true;
		}

		void RequestOpenLevelPath(const std::filesystem::path& path)
		{
			if (!CanModifyLevel()) return;
			BeginDestructiveAction(PendingAction::OpenLevel, NormalizeLevelPath(path));
		}

		void RequestSaveLevel()
		{
			if (!CanModifyLevel()) return;
			if (currentLevelPath_.empty())
			{
				saveAsContinuePendingAction_ = false;
				requestSaveAsDialog_ = true;
				return;
			}
			SaveLevelToPath(currentLevelPath_, true, true, true, "Save");
		}

		void RequestSaveLevelAs()
		{
			if (!CanModifyLevel()) return;
			saveAsContinuePendingAction_ = false;
			requestSaveAsDialog_ = true;
		}

		bool ConsumeStatus(std::string& outMessage, bool& outSucceeded)
		{
			if (consumedStatusSerial_ == statusSerial_) return false;
			consumedStatusSerial_ = statusSerial_;
			outMessage = statusMessage_;
			outSucceeded = statusSucceeded_;
			return !outMessage.empty();
		}

		const std::filesystem::path& GetCurrentLevelPath() const { return currentLevelPath_; }
		const std::vector<std::string>& GetRecentLevels() const { return recentLevels_; }
		bool IsAutoSaveEnabled() const { return autoSaveEnabled_; }
		float GetAutoSaveIntervalSeconds() const { return autoSaveIntervalSeconds_; }

	private:
		enum class PendingAction
		{
			None,
			NewLevel,
			OpenLevel,
		};

		struct PendingLoadedActor
		{
			std::string id;
			std::string parentId;
			std::filesystem::path temporaryActorPath;
			EditorActorState editorState{};
			nlohmann::json actorJson;
		};

		static inline const std::filesystem::path kLevelDirectory = "Resources/JSON/Levels";
		static inline const std::filesystem::path kAutoSaveDirectory = "../Generated/Autosaves/Levels";
		static inline const std::filesystem::path kBackupDirectory = "../Generated/Backups/Levels";
		static inline const std::filesystem::path kRecentLevelsPath = "../Generated/Editor/RecentLevels.json";
		static inline const std::filesystem::path kTemporaryLoadDirectory = "../Generated/Intermediate/EditorLevelLoad";
		static constexpr int kCurrentFormatVersion = 1;
		static constexpr std::size_t kMaximumRecentLevels = 10;

		EditorLevelService() = default;
		~EditorLevelService() = default;
		EditorLevelService(const EditorLevelService&) = delete;
		EditorLevelService& operator=(const EditorLevelService&) = delete;

		void EnsureInitialized()
		{
			if (initialized_) return;
			std::error_code error;
			std::filesystem::create_directories(kLevelDirectory, error);
			std::filesystem::create_directories(kAutoSaveDirectory, error);
			std::filesystem::create_directories(kBackupDirectory, error);
			LoadRecentLevels();
			initialized_ = true; // File menuを初めて開く前にLevel関連ディレクトリを用意する。
		}

		bool CanModifyLevel()
		{
			if (EditorPlayController::GetInstance()->IsEditing()) return true;
			SetStatus(false, "Level操作はEditorのStop状態で実行してください。");
			return false;
		}

		BaseScene* GetCurrentScene() const
		{
			return sceneManager_ ? sceneManager_->GetCurrentScene() : nullptr;
		}

		ActorWorld* GetCurrentActorWorld() const
		{
			BaseScene* scene = GetCurrentScene();
			return scene ? scene->GetEditorActorWorld() : nullptr;
		}

		void BeginDestructiveAction(PendingAction action, const std::filesystem::path& path)
		{
			pendingAction_ = action;
			pendingOpenPath_ = path;
			if (EditorContext::GetInstance()->IsLevelDirty())
			{
				requestUnsavedDialog_ = true;
				return;
			}
			ExecutePendingAction();
		}

		void ExecutePendingAction()
		{
			const PendingAction action = pendingAction_;
			const std::filesystem::path path = pendingOpenPath_;
			pendingAction_ = PendingAction::None;
			pendingOpenPath_.clear();

			switch (action)
			{
			case PendingAction::NewLevel:
				CreateNewLevel();
				break;
			case PendingAction::OpenLevel:
				LoadLevelFromPath(path);
				break;
			case PendingAction::None:
			default:
				break;
			}
		}

		bool CreateNewLevel()
		{
			ActorWorld* actorWorld = GetCurrentActorWorld();
			if (!actorWorld)
			{
				SetStatus(false, "現在のSceneはEditor用ActorWorldを公開していません。");
				return false;
			}

			EditorContext::GetInstance()->ResetTransientState();
			EditorCommandHistory::GetInstance()->Clear();
			actorWorld->SetSelectedEditorObject(nullptr, nullptr);
			actorWorld->Finalize();
			actorWorld->Initialize(); // 空Worldを即Initializeして以降のViewport配置Actorを通常Spawn扱いにする。
			LightManager::GetInstance()->ResetToDefaultLighting();

			currentLevelPath_.clear();
			EditorContext::GetInstance()->SetActiveLevelName("Untitled");
			EditorContext::GetInstance()->MarkLevelDirty(false);
			autoSaveElapsed_ = 0.0f;
			SetStatus(true, "新しい空Levelを作成しました。");
			return true;
		}

		static nlohmann::json WriteVector3(const Vector3& value)
		{
			return nlohmann::json::array({ value.x, value.y, value.z });
		}

		static nlohmann::json WriteVector4(const Vector4& value)
		{
			return nlohmann::json::array({ value.x, value.y, value.z, value.w });
		}

		static Vector3 ReadVector3(const nlohmann::json& value, const Vector3& fallback)
		{
			if (!value.is_array() || value.size() != 3) return fallback;
			return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>() };
		}

		static Vector4 ReadVector4(const nlohmann::json& value, const Vector4& fallback)
		{
			if (!value.is_array() || value.size() != 4) return fallback;
			return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>() };
		}

		static Actor* GetParentActor(const Actor& actor)
		{
			SceneComponent* root = actor.GetRootComponent();
			SceneComponent* parent = root ? root->GetParent() : nullptr;
			Actor* parentActor = parent ? parent->GetOwner() : nullptr;
			return parentActor != &actor ? parentActor : nullptr;
		}

		std::string ResolveSceneName(BaseScene& scene) const
		{
			std::vector<EditorObjectInfo> editorObjects;
			scene.CollectEditorObjects(editorObjects);
			for (const EditorObjectInfo& object : editorObjects)
			{
				if (!object.sceneName.empty()) return object.sceneName;
			}
			return "UnknownScene";
		}

		nlohmann::json SerializeLighting() const
		{
			LightManager* lightManager = LightManager::GetInstance();
			const LightManager::LightingSettingsGPU& settings = lightManager->GetLightingSettings();
			const ShadowSettings shadow = lightManager->GetShadowSettingsForParameter();

			nlohmann::json lighting;
			lighting["Settings"] = {
				{ "AmbientColor", WriteVector4(settings.ambientColor) },
				{ "FogColor", WriteVector4(settings.fogColor) },
				{ "Exposure", settings.exposure },
				{ "Contrast", settings.contrast },
				{ "FogStart", settings.fogStart },
				{ "FogEnd", settings.fogEnd },
				{ "EnableFog", settings.enableFog != 0 },
				{ "SpecularStrength", settings.specularStrength },
				{ "DiffuseStrength", settings.diffuseStrength },
				{ "SpecularPowerScale", settings.specularPowerScale },
				{ "RimLightStrength", settings.rimLightStrength },
				{ "RimLightPower", settings.rimLightPower },
				{ "EnableRimLight", settings.enableRimLight != 0 },
				{ "EnableHalfLambert", settings.enableHalfLambert != 0 },
				{ "RimLightColor", WriteVector4(settings.rimLightColor) },
				{ "ShadingMode", settings.shadingMode },
				{ "EnableIBL", settings.enableIBL != 0 },
				{ "IblDiffuseStrength", settings.iblDiffuseStrength },
				{ "IblSpecularStrength", settings.iblSpecularStrength },
			};

			lighting["Shadow"] = {
				{ "EnableShadow", shadow.enableShadow },
				{ "ShadowBias", shadow.shadowBias },
				{ "NormalBias", shadow.normalBias },
				{ "ShadowStrength", shadow.shadowStrength },
				{ "ShadowMapSize", shadow.shadowMapSize },
				{ "ShowShadowMapDebug", shadow.showShadowMapDebug },
				{ "ShowShadowFactorDebug", shadow.showShadowFactorDebug },
				{ "ShadowCasterLightIndex", shadow.shadowCasterLightIndex },
				{ "ShadowFocusMode", static_cast<uint32_t>(shadow.shadowFocusMode) },
				{ "ManualShadowFocusPosition", WriteVector3(shadow.manualShadowFocusPosition) },
				{ "DirectionalShadowDistance", shadow.directionalShadowDistance },
				{ "DirectionalShadowWidth", shadow.directionalShadowWidth },
				{ "DirectionalShadowHeight", shadow.directionalShadowHeight },
				{ "DirectionalShadowNearZ", shadow.directionalShadowNearZ },
				{ "DirectionalShadowFarZ", shadow.directionalShadowFarZ },
				{ "DirectionalShadowFocusOffset", shadow.directionalShadowFocusOffset },
				{ "SpotShadowNearZ", shadow.spotShadowNearZ },
				{ "PointShadowNearZ", shadow.pointShadowNearZ },
				{ "EnableCsm", shadow.enableCsm },
				{ "CsmMaxDistance", shadow.csmMaxDistance },
				{ "CsmSplitLambda", shadow.csmSplitLambda },
			};

			lighting["GlobalPunctualLights"] = nlohmann::json::array();
			for (const LightManager::PunctualLightGPU& light : lightManager->GetPunctualLights())
			{
				lighting["GlobalPunctualLights"].push_back({
					{ "LightType", light.lightType },
					{ "Color", WriteVector4(light.color) },
					{ "Intensity", light.intensity },
					{ "Position", WriteVector3(light.position) },
					{ "Radius", light.radius },
					{ "Decay", light.decay },
					{ "Direction", WriteVector3(light.direction) },
					{ "Distance", light.distance },
					{ "CosFalloffStart", light.cosFalloffStart },
					{ "CosAngle", light.cosAngle },
					{ "AreaSize", WriteVector3(light.areaSize) },
					{ "Enabled", light.enabled != 0 },
				});
			}
			return lighting;
		}

		void DeserializeLighting(const nlohmann::json& lighting) const
		{
			if (!lighting.is_object()) return;
			LightManager* lightManager = LightManager::GetInstance();

			if (lighting.contains("Settings") && lighting["Settings"].is_object())
			{
				const nlohmann::json& source = lighting["Settings"];
				LightManager::LightingSettingsGPU& settings = lightManager->GetMutableLightingSettingsForEditor();
				if (source.contains("AmbientColor")) settings.ambientColor = ReadVector4(source["AmbientColor"], settings.ambientColor);
				if (source.contains("FogColor")) settings.fogColor = ReadVector4(source["FogColor"], settings.fogColor);
				settings.exposure = source.value("Exposure", settings.exposure);
				settings.contrast = source.value("Contrast", settings.contrast);
				settings.fogStart = source.value("FogStart", settings.fogStart);
				settings.fogEnd = source.value("FogEnd", settings.fogEnd);
				settings.enableFog = source.value("EnableFog", settings.enableFog != 0) ? 1u : 0u;
				settings.specularStrength = source.value("SpecularStrength", settings.specularStrength);
				settings.diffuseStrength = source.value("DiffuseStrength", settings.diffuseStrength);
				settings.specularPowerScale = source.value("SpecularPowerScale", settings.specularPowerScale);
				settings.rimLightStrength = source.value("RimLightStrength", settings.rimLightStrength);
				settings.rimLightPower = source.value("RimLightPower", settings.rimLightPower);
				settings.enableRimLight = source.value("EnableRimLight", settings.enableRimLight != 0) ? 1u : 0u;
				settings.enableHalfLambert = source.value("EnableHalfLambert", settings.enableHalfLambert != 0) ? 1u : 0u;
				if (source.contains("RimLightColor")) settings.rimLightColor = ReadVector4(source["RimLightColor"], settings.rimLightColor);
				settings.shadingMode = source.value("ShadingMode", settings.shadingMode);
				settings.enableIBL = source.value("EnableIBL", settings.enableIBL != 0) ? 1u : 0u;
				settings.iblDiffuseStrength = source.value("IblDiffuseStrength", settings.iblDiffuseStrength);
				settings.iblSpecularStrength = source.value("IblSpecularStrength", settings.iblSpecularStrength);
			}

			if (lighting.contains("Shadow") && lighting["Shadow"].is_object())
			{
				const nlohmann::json& source = lighting["Shadow"];
				ShadowSettings shadow = lightManager->GetShadowSettingsForParameter();
				shadow.enableShadow = source.value("EnableShadow", shadow.enableShadow);
				shadow.shadowBias = source.value("ShadowBias", shadow.shadowBias);
				shadow.normalBias = source.value("NormalBias", shadow.normalBias);
				shadow.shadowStrength = source.value("ShadowStrength", shadow.shadowStrength);
				shadow.shadowMapSize = source.value("ShadowMapSize", shadow.shadowMapSize);
				shadow.showShadowMapDebug = source.value("ShowShadowMapDebug", shadow.showShadowMapDebug);
				shadow.showShadowFactorDebug = source.value("ShowShadowFactorDebug", shadow.showShadowFactorDebug);
				shadow.shadowCasterLightIndex = source.value("ShadowCasterLightIndex", shadow.shadowCasterLightIndex);
				shadow.shadowFocusMode = static_cast<ShadowFocusMode>(source.value("ShadowFocusMode", static_cast<uint32_t>(shadow.shadowFocusMode)));
				if (source.contains("ManualShadowFocusPosition")) shadow.manualShadowFocusPosition = ReadVector3(source["ManualShadowFocusPosition"], shadow.manualShadowFocusPosition);
				shadow.directionalShadowDistance = source.value("DirectionalShadowDistance", shadow.directionalShadowDistance);
				shadow.directionalShadowWidth = source.value("DirectionalShadowWidth", shadow.directionalShadowWidth);
				shadow.directionalShadowHeight = source.value("DirectionalShadowHeight", shadow.directionalShadowHeight);
				shadow.directionalShadowNearZ = source.value("DirectionalShadowNearZ", shadow.directionalShadowNearZ);
				shadow.directionalShadowFarZ = source.value("DirectionalShadowFarZ", shadow.directionalShadowFarZ);
				shadow.directionalShadowFocusOffset = source.value("DirectionalShadowFocusOffset", shadow.directionalShadowFocusOffset);
				shadow.spotShadowNearZ = source.value("SpotShadowNearZ", shadow.spotShadowNearZ);
				shadow.pointShadowNearZ = source.value("PointShadowNearZ", shadow.pointShadowNearZ);
				shadow.enableCsm = source.value("EnableCsm", shadow.enableCsm);
				shadow.csmMaxDistance = source.value("CsmMaxDistance", shadow.csmMaxDistance);
				shadow.csmSplitLambda = source.value("CsmSplitLambda", shadow.csmSplitLambda);
				lightManager->SetShadowSettingsFromParameter(shadow);
			}

			if (lighting.contains("GlobalPunctualLights") && lighting["GlobalPunctualLights"].is_array())
			{
				std::vector<LightManager::PunctualLightGPU>& lights = lightManager->GetMutablePunctualLightsForEditor();
				lights.clear();
				for (const nlohmann::json& source : lighting["GlobalPunctualLights"])
				{
					if (!source.is_object()) continue;
					LightManager::PunctualLightGPU light{};
					light.lightType = source.value("LightType", 0u);
					if (source.contains("Color")) light.color = ReadVector4(source["Color"], { 1.0f, 1.0f, 1.0f, 1.0f });
					light.intensity = source.value("Intensity", 1.0f);
					if (source.contains("Position")) light.position = ReadVector3(source["Position"], {});
					light.radius = source.value("Radius", 10.0f);
					light.decay = source.value("Decay", 2.0f);
					if (source.contains("Direction")) light.direction = ReadVector3(source["Direction"], { 0.0f, -1.0f, 0.0f });
					light.distance = source.value("Distance", light.radius);
					light.cosFalloffStart = source.value("CosFalloffStart", 1.0f);
					light.cosAngle = source.value("CosAngle", 1.0f);
					if (source.contains("AreaSize")) light.areaSize = ReadVector3(source["AreaSize"], { 1.0f, 1.0f, 0.0f });
					light.enabled = source.value("Enabled", true) ? 1u : 0u;
					lights.push_back(light);
				}
			}
		}

		nlohmann::json SerializeCamera() const
		{
			DebugCamera* camera = CameraManager::GetInstance()->GetDebugCamera();
			if (!camera) return nlohmann::json::object();
			return {
				{ "Position", WriteVector3(camera->GetTranslate()) },
				{ "Rotation", WriteVector3(camera->GetRotate()) },
				{ "FovY", camera->GetFovY() },
				{ "NearClip", camera->GetNearClip() },
				{ "FarClip", camera->GetFarClip() },
			};
		}

		void DeserializeCamera(const nlohmann::json& cameraJson) const
		{
			if (!cameraJson.is_object()) return;
			DebugCamera* camera = CameraManager::GetInstance()->GetDebugCamera();
			if (!camera) return;
			if (cameraJson.contains("Position")) camera->SetTranslate(ReadVector3(cameraJson["Position"], camera->GetTranslate()));
			if (cameraJson.contains("Rotation")) camera->SetRotate(ReadVector3(cameraJson["Rotation"], camera->GetRotate()));
			camera->SetFovY(cameraJson.value("FovY", camera->GetFovY()));
			camera->SetNearClip(cameraJson.value("NearClip", camera->GetNearClip()));
			camera->SetFarClip(cameraJson.value("FarClip", camera->GetFarClip()));
			camera->RefreshViewProjection();
		}

		nlohmann::json SerializeLevel(ActorWorld& actorWorld) const
		{
			BaseScene* scene = GetCurrentScene();
			nlohmann::json level;
			level["Format"] = "Ken4lowLevel";
			level["Version"] = kCurrentFormatVersion;
			level["Name"] = currentLevelPath_.empty() ? "Untitled" : currentLevelPath_.stem().string();
			level["LevelSettings"] = {
				{ "TargetScene", scene ? ResolveSceneName(*scene) : "UnknownScene" },
			};
			level["Actors"] = nlohmann::json::array();

			std::unordered_map<const Actor*, std::string> actorIds;
			std::size_t actorIndex = 0;
			for (const auto& actorOwner : actorWorld.GetActors())
			{
				Actor* actor = actorOwner.get();
				if (!actor || actor->IsPendingDestroy()) continue;
				actorIds.emplace(actor, "Actor_" + std::to_string(actorIndex++));
			}

			for (const auto& actorOwner : actorWorld.GetActors())
			{
				Actor* actor = actorOwner.get();
				if (!actor || actor->IsPendingDestroy()) continue;
				const EditorActorState editorState = EditorActorStateRegistry::GetInstance()->GetState(actor);
				Actor* parentActor = GetParentActor(*actor);
				const auto parentId = parentActor ? actorIds.find(parentActor) : actorIds.end();

				level["Actors"].push_back({
					{ "Id", actorIds.at(actor) },
					{ "ParentId", parentId != actorIds.end() ? parentId->second : std::string{} },
					{ "Editor", {
						{ "Visible", editorState.visible },
						{ "Locked", editorState.locked },
						{ "Folder", editorState.folderPath },
					} },
					{ "Data", ActorJsonSerializer::SerializeActor(*actor) },
				});
			}

			level["Lighting"] = SerializeLighting();
			level["Camera"] = SerializeCamera();
			level["Environment"] = nlohmann::json::object(); // EnvironmentはPhase 10の拡張領域としてLevel形式に先行確保する。
			return level;
		}

		bool SaveLevelToPath(
			const std::filesystem::path& requestedPath,
			bool updateCurrentPath,
			bool createBackup,
			bool clearDirty,
			std::string_view operationName)
		{
			ActorWorld* actorWorld = GetCurrentActorWorld();
			if (!actorWorld)
			{
				SetStatus(false, "現在のSceneはEditor用ActorWorldを公開していません。");
				return false;
			}

			const std::filesystem::path path = NormalizeLevelPath(requestedPath);
			if (path.empty())
			{
				SetStatus(false, "Level保存パスが空です。");
				return false;
			}

			try
			{
				if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
				if (createBackup && std::filesystem::exists(path)) CreateBackup(path);

				const nlohmann::json levelJson = SerializeLevel(*actorWorld);
				const std::filesystem::path temporaryPath = path.string() + ".tmp";
				{
					std::ofstream file(temporaryPath, std::ios::trunc);
					if (!file.is_open())
					{
						SetStatus(false, std::string(operationName) + " failed: " + path.generic_string());
						return false;
					}
					file << levelJson.dump(4);
				}
				if (std::filesystem::exists(path)) std::filesystem::remove(path);
				std::filesystem::rename(temporaryPath, path); // 一時ファイルを書き切ってから置換し、途中終了でLevel本体を壊しにくくする。

				if (updateCurrentPath)
				{
					currentLevelPath_ = path;
					EditorContext::GetInstance()->SetActiveLevelName(path.filename().string());
					AddRecentLevel(path);
				}
				if (clearDirty) EditorContext::GetInstance()->MarkLevelDirty(false);
				SetStatus(true, std::string(operationName) + ": " + path.generic_string());
				return true;
			}
			catch (const std::exception& exception)
			{
				SetStatus(false, std::string(operationName) + " failed: " + exception.what());
				return false;
			}
		}

		static void RestoreCameraRegistration(Actor& actor, const nlohmann::json& actorJson)
		{
			if (!actorJson.contains("Components") || !actorJson["Components"].is_array()) return;
			std::vector<CameraComponent*> cameras = actor.GetComponents<CameraComponent>();
			std::size_t cameraIndex = 0;
			for (const nlohmann::json& componentJson : actorJson["Components"])
			{
				if (!componentJson.is_object() || componentJson.value("Class", std::string{}) != "CameraComponent") continue;
				if (cameraIndex >= cameras.size()) break;
				if (cameras[cameraIndex])
				{
					cameras[cameraIndex]->SetAutoRegisterMainCamera(componentJson.value("AutoRegisterMainCamera", false));
				}
				++cameraIndex;
			}
		}

		bool PrepareActorLoadFiles(const nlohmann::json& actorsJson, std::vector<PendingLoadedActor>& outActors)
		{
			if (!actorsJson.is_array()) return false;
			std::error_code error;
			std::filesystem::remove_all(kTemporaryLoadDirectory, error);
			std::filesystem::create_directories(kTemporaryLoadDirectory, error);
			if (error) return false;

			std::size_t index = 0;
			for (const nlohmann::json& entry : actorsJson)
			{
				if (!entry.is_object() || !entry.contains("Data") || !entry["Data"].is_object()) return false;
				PendingLoadedActor pending{};
				pending.id = entry.value("Id", "Actor_" + std::to_string(index));
				pending.parentId = entry.value("ParentId", std::string{});
				pending.actorJson = entry["Data"];

				if (entry.contains("Editor") && entry["Editor"].is_object())
				{
					const nlohmann::json& editor = entry["Editor"];
					pending.editorState.visible = editor.value("Visible", true);
					pending.editorState.locked = editor.value("Locked", false);
					pending.editorState.folderPath = editor.value("Folder", std::string{});
				}

				pending.temporaryActorPath = kTemporaryLoadDirectory / ("Actor_" + std::to_string(index) + ".json");
				std::ofstream file(pending.temporaryActorPath, std::ios::trunc);
				if (!file.is_open()) return false;
				file << pending.actorJson.dump(4);
				outActors.push_back(std::move(pending));
				++index;
			}
			return true;
		}

		bool LoadLevelFromPath(const std::filesystem::path& requestedPath)
		{
			ActorWorld* actorWorld = GetCurrentActorWorld();
			if (!actorWorld)
			{
				SetStatus(false, "現在のSceneはEditor用ActorWorldを公開していません。");
				return false;
			}

			const std::filesystem::path path = NormalizeLevelPath(requestedPath);
			try
			{
				std::ifstream file(path);
				if (!file.is_open())
				{
					SetStatus(false, "Open failed: " + path.generic_string());
					return false;
				}

				nlohmann::json levelJson;
				file >> levelJson;
				if (!levelJson.is_object() || levelJson.value("Format", std::string{}) != "Ken4lowLevel")
				{
					SetStatus(false, "Ken4lowLevel形式ではありません: " + path.generic_string());
					return false;
				}
				const int version = levelJson.value("Version", 0);
				if (version <= 0 || version > kCurrentFormatVersion)
				{
					SetStatus(false, "未対応のLevel Versionです: " + std::to_string(version));
					return false;
				}

				std::vector<PendingLoadedActor> pendingActors;
				if (!levelJson.contains("Actors") || !PrepareActorLoadFiles(levelJson["Actors"], pendingActors))
				{
					SetStatus(false, "Actor配列の準備に失敗しました: " + path.generic_string());
					return false;
				}

				EditorContext::GetInstance()->ResetTransientState();
				EditorCommandHistory::GetInstance()->Clear();
				actorWorld->SetSelectedEditorObject(nullptr, nullptr);
				actorWorld->Finalize();
				actorWorld->Initialize(); // Level読込後のActorを実行中Spawnと同じライフサイクルで登録する。

				std::unordered_map<std::string, Actor*> loadedActors;
				for (PendingLoadedActor& pending : pendingActors)
				{
					Actor* actor = actorWorld->SpawnActorFromJson(pending.temporaryActorPath.generic_string());
					if (!actor)
					{
						actorWorld->Finalize();
						actorWorld->Initialize();
						std::filesystem::remove_all(kTemporaryLoadDirectory);
						SetStatus(false, "Actorの復元に失敗しました: " + pending.id);
						return false;
					}
					RestoreCameraRegistration(*actor, pending.actorJson);
					EditorActorStateRegistry::GetInstance()->SetState(actor, pending.editorState);
					loadedActors[pending.id] = actor;
				}

				for (const PendingLoadedActor& pending : pendingActors)
				{
					if (pending.parentId.empty()) continue;
					const auto childIt = loadedActors.find(pending.id);
					const auto parentIt = loadedActors.find(pending.parentId);
					if (childIt == loadedActors.end() || parentIt == loadedActors.end()) continue;
					SceneComponent* childRoot = childIt->second ? childIt->second->GetRootComponent() : nullptr;
					SceneComponent* parentRoot = parentIt->second ? parentIt->second->GetRootComponent() : nullptr;
					if (childRoot && parentRoot) childRoot->AttachTo(parentRoot); // Actor JSONのLocal Transformを維持したまま親子関係だけを復元する。
				}

				if (levelJson.contains("Lighting")) DeserializeLighting(levelJson["Lighting"]);
				if (levelJson.contains("Camera")) DeserializeCamera(levelJson["Camera"]);
				std::filesystem::remove_all(kTemporaryLoadDirectory);

				actorWorld->SetSelectedEditorObject(nullptr, nullptr);
				EditorContext::GetInstance()->GetSelection().Clear();
				currentLevelPath_ = path;
				EditorContext::GetInstance()->SetActiveLevelName(path.filename().string());
				EditorContext::GetInstance()->MarkLevelDirty(false);
				autoSaveElapsed_ = 0.0f;
				AddRecentLevel(path);
				SetStatus(true, "Opened: " + path.generic_string());
				return true;
			}
			catch (const std::exception& exception)
			{
				std::error_code error;
				std::filesystem::remove_all(kTemporaryLoadDirectory, error);
				SetStatus(false, std::string("Open failed: ") + exception.what());
				return false;
			}
		}

		static std::string MakeTimestamp()
		{
			const std::time_t currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			std::tm localTime{};
#ifdef _WIN32
			localtime_s(&localTime, &currentTime);
#else
			localtime_r(&currentTime, &localTime);
#endif
			char buffer[32]{};
			std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &localTime);
			return buffer;
		}

		void CreateBackup(const std::filesystem::path& sourcePath) const
		{
			std::filesystem::create_directories(kBackupDirectory);
			const std::filesystem::path backupPath = kBackupDirectory /
				(sourcePath.stem().string() + "_" + MakeTimestamp() + sourcePath.extension().string());
			std::filesystem::copy_file(sourcePath, backupPath, std::filesystem::copy_options::overwrite_existing); // 保存前のLevelを時刻付きバックアップへ退避する。
		}

		static std::filesystem::path NormalizeLevelPath(const std::filesystem::path& requestedPath)
		{
			if (requestedPath.empty()) return {};
			std::filesystem::path path = requestedPath;
			if (!path.has_extension()) path.replace_extension(".json");
			if (!path.has_parent_path()) path = kLevelDirectory / path;
			return path.lexically_normal();
		}

		void RefreshLevelFileList()
		{
			levelFiles_.clear();
			std::error_code error;
			std::filesystem::create_directories(kLevelDirectory, error);
			for (const auto& entry : std::filesystem::recursive_directory_iterator(kLevelDirectory, error))
			{
				if (error) break;
				if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
				levelFiles_.push_back(entry.path());
			}
			std::sort(levelFiles_.begin(), levelFiles_.end());
		}

		void AddRecentLevel(const std::filesystem::path& path)
		{
			const std::string normalized = path.lexically_normal().generic_string();
			std::erase(recentLevels_, normalized);
			recentLevels_.insert(recentLevels_.begin(), normalized);
			if (recentLevels_.size() > kMaximumRecentLevels) recentLevels_.resize(kMaximumRecentLevels);
			SaveRecentLevels();
		}

		void LoadRecentLevels()
		{
			recentLevels_.clear();
			try
			{
				std::ifstream file(kRecentLevelsPath);
				if (!file.is_open()) return;
				nlohmann::json recentJson;
				file >> recentJson;
				if (!recentJson.is_array()) return;
				for (const nlohmann::json& entry : recentJson)
				{
					if (!entry.is_string()) continue;
					const std::string path = entry.get<std::string>();
					if (std::filesystem::exists(path)) recentLevels_.push_back(path);
					if (recentLevels_.size() >= kMaximumRecentLevels) break;
				}
			}
			catch (...) { recentLevels_.clear(); }
		}

		void SaveRecentLevels() const
		{
			try
			{
				if (kRecentLevelsPath.has_parent_path()) std::filesystem::create_directories(kRecentLevelsPath.parent_path());
				std::ofstream file(kRecentLevelsPath, std::ios::trunc);
				if (file.is_open()) file << nlohmann::json(recentLevels_).dump(4);
			}
			catch (...) { /* Recent履歴の失敗はLevel本体の保存結果へ影響させない。 */ }
		}

		void SetStatus(bool succeeded, std::string message)
		{
			statusSucceeded_ = succeeded;
			statusMessage_ = std::move(message);
			++statusSerial_;
		}

#ifdef USE_IMGUI
		void PrepareSaveAsBuffer()
		{
			if (saveAsPathBuffer_[0] != '\0') return;
			const std::filesystem::path defaultPath = currentLevelPath_.empty()
				? (kLevelDirectory / "Untitled.json")
				: currentLevelPath_;
			std::snprintf(saveAsPathBuffer_.data(), saveAsPathBuffer_.size(), "%s", defaultPath.generic_string().c_str());
		}

		void DrawOpenDialog()
		{
			if (!ImGui::BeginPopupModal("Open Level##EditorLevelOpen", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
			ImGui::TextDisabled("Ken4lowLevel JSONを選択してください。");
			ImGui::SetNextItemWidth(560.0f);
			ImGui::InputText("Path##OpenLevelPath", openPathBuffer_.data(), openPathBuffer_.size());

			if (ImGui::BeginChild("##LevelFileList", ImVec2(560.0f, 260.0f), true))
			{
				for (std::size_t index = 0; index < levelFiles_.size(); ++index)
				{
					const std::filesystem::path& path = levelFiles_[index];
					const bool selected = selectedLevelFileIndex_ == static_cast<int>(index);
					const std::string label = path.lexically_relative(kLevelDirectory).generic_string() + "##LevelFile" + std::to_string(index);
					if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
					{
						selectedLevelFileIndex_ = static_cast<int>(index);
						std::snprintf(openPathBuffer_.data(), openPathBuffer_.size(), "%s", path.generic_string().c_str());
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						{
							ImGui::CloseCurrentPopup();
							RequestOpenLevelPath(path);
						}
					}
				}
			}
			ImGui::EndChild();

			if (ImGui::Button("Open", ImVec2(120.0f, 0.0f)))
			{
				const std::filesystem::path path = NormalizeLevelPath(openPathBuffer_.data());
				ImGui::CloseCurrentPopup();
				RequestOpenLevelPath(path);
			}
			ImGui::SameLine();
			if (ImGui::Button("Refresh", ImVec2(120.0f, 0.0f))) RefreshLevelFileList();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		void DrawSaveAsDialog()
		{
			if (!ImGui::BeginPopupModal("Save Level As##EditorLevelSaveAs", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
			ImGui::TextDisabled("Actor Prefabとは別のLevel JSONとして保存します。");
			ImGui::SetNextItemWidth(560.0f);
			ImGui::InputText("Path##SaveLevelPath", saveAsPathBuffer_.data(), saveAsPathBuffer_.size());

			if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
			{
				const std::filesystem::path path = NormalizeLevelPath(saveAsPathBuffer_.data());
				if (SaveLevelToPath(path, true, true, true, "Save As"))
				{
					ImGui::CloseCurrentPopup();
					saveAsPathBuffer_.fill('\0');
					if (saveAsContinuePendingAction_) ExecutePendingAction();
					saveAsContinuePendingAction_ = false;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
			{
				ImGui::CloseCurrentPopup();
				saveAsContinuePendingAction_ = false;
				pendingAction_ = PendingAction::None;
				pendingOpenPath_.clear();
			}
			ImGui::EndPopup();
		}

		void DrawUnsavedDialog()
		{
			if (!ImGui::BeginPopupModal("Unsaved Level##EditorLevelUnsaved", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
			ImGui::Text("%s has unsaved changes.", EditorContext::GetInstance()->GetActiveLevelName().c_str());
			ImGui::TextDisabled("続行する前に保存しますか？");
			ImGui::Separator();

			if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
			{
				if (currentLevelPath_.empty())
				{
					ImGui::CloseCurrentPopup();
					saveAsContinuePendingAction_ = true;
					requestSaveAsDialog_ = true;
				}
				else if (SaveLevelToPath(currentLevelPath_, true, true, true, "Save"))
				{
					ImGui::CloseCurrentPopup();
					ExecutePendingAction();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Don't Save", ImVec2(120.0f, 0.0f)))
			{
				ImGui::CloseCurrentPopup();
				ExecutePendingAction();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
			{
				ImGui::CloseCurrentPopup();
				pendingAction_ = PendingAction::None;
				pendingOpenPath_.clear();
			}
			ImGui::EndPopup();
		}
#endif

		SceneManager* sceneManager_ = nullptr;
		std::filesystem::path currentLevelPath_;
		std::vector<std::filesystem::path> levelFiles_;
		std::vector<std::string> recentLevels_;
		PendingAction pendingAction_ = PendingAction::None;
		std::filesystem::path pendingOpenPath_;
		bool initialized_ = false;
		bool autoSaveEnabled_ = true;
		float autoSaveIntervalSeconds_ = 120.0f;
		float autoSaveElapsed_ = 0.0f;
		bool requestOpenDialog_ = false;
		bool requestSaveAsDialog_ = false;
		bool requestUnsavedDialog_ = false;
		bool saveAsContinuePendingAction_ = false;
		std::array<char, 512> openPathBuffer_{};
		std::array<char, 512> saveAsPathBuffer_{};
		int selectedLevelFileIndex_ = -1;
		std::string statusMessage_;
		bool statusSucceeded_ = true;
		uint64_t statusSerial_ = 0;
		uint64_t consumedStatusSerial_ = 0;
	};
} // namespace Ken4lowEngine
