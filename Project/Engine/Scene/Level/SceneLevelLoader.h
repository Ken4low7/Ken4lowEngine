#pragma once

#include <ActorFactory.h>
#include <ActorWorld.h>
#include <ComponentFactory.h>
#include <LightManager.h>
#include <SceneComponent.h>
#include <ShadowSettings.h>
#include <json.hpp>

#ifdef USE_IMGUI
#include <CameraManager.h>
#include <DebugCamera.h>
#include <Editor/EditorActorStateRegistry.h>
#endif

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>SceneDefinitionのLevelパスからActorWorldと描画設定を復元します。</summary>
	class SceneLevelLoader
	{
	public:
		struct Result
		{
			bool succeeded = false;
			std::size_t actorCount = 0;
			std::string message;
		};

		static Result Load(const std::filesystem::path& levelPath, ActorWorld& actorWorld)
		{
			bool worldResetStarted = false;
			try
			{
				std::ifstream file(levelPath);
				if (!file.is_open()) return { false, 0, "Levelファイルを開けません: " + levelPath.generic_string() };

				nlohmann::json levelJson;
				file >> levelJson;
				if (!ValidateLevel(levelJson)) return { false, 0, "Ken4lowLevel形式が不正です: " + levelPath.generic_string() };

				const nlohmann::json& actorsJson = levelJson["Actors"];
				std::string validationMessage;
				if (!ValidateActors(actorsJson, validationMessage))
				{
					return { false, 0, validationMessage + ": " + levelPath.generic_string() };
				}

				ActorSpawnOptions spawnOptions{};
				spawnOptions.applySpawnOffset = false;
				spawnOptions.disableAutoRegisterMainCamera = false;

				actorWorld.SetSelectedEditorObject(nullptr, nullptr);
				worldResetStarted = true;
				actorWorld.Finalize();
				actorWorld.Initialize(); // 構造検証成功後にだけ既存Worldを空にし、Actorを実行中Spawnと同じ経路で復元する。

				std::unordered_map<std::string, Actor*> actorsById;
				std::vector<std::pair<std::string, std::string>> pendingParents;
				std::size_t actorIndex = 0;
				for (const nlohmann::json& entry : actorsJson)
				{
					Actor* actor = actorWorld.SpawnActorFromJsonData(entry["Data"], spawnOptions); // 読み込み済みJSON専用APIを使い、ファイルパス版とのオーバーロード衝突を避ける。
					if (!actor)
					{
						actorWorld.Finalize();
						actorWorld.Initialize();
						return { false, actorIndex, "Level Actorの生成に失敗したためWorldを空へ戻しました: " + levelPath.generic_string() };
					}

					const std::string id = entry.value("Id", "Actor_" + std::to_string(actorIndex));
					actorsById[id] = actor;
					pendingParents.emplace_back(id, entry.value("ParentId", std::string{}));
#ifdef USE_IMGUI
					ApplyEditorState(entry, actor);
#endif
					++actorIndex;
				}

				for (const auto& [childId, parentId] : pendingParents)
				{
					if (parentId.empty()) continue;
					const auto childIt = actorsById.find(childId);
					const auto parentIt = actorsById.find(parentId);
					if (childIt == actorsById.end() || parentIt == actorsById.end()) continue;
					SceneComponent* childRoot = childIt->second ? childIt->second->GetRootComponent() : nullptr;
					SceneComponent* parentRoot = parentIt->second ? parentIt->second->GetRootComponent() : nullptr;
					if (childRoot && parentRoot) childRoot->AttachTo(parentRoot); // 保存済みLocal Transformを維持したままActor間階層を復元する。
				}

				if (levelJson.contains("Lighting")) ApplyLighting(levelJson["Lighting"]);
#ifdef USE_IMGUI
				if (levelJson.contains("Camera")) ApplyEditorCamera(levelJson["Camera"]);
#endif
				actorWorld.SetSelectedEditorObject(nullptr, nullptr);
				return { true, actorIndex, "Levelを読み込みました: " + levelPath.generic_string() };
			}
			catch (const std::exception& exception)
			{
				if (worldResetStarted)
				{
					actorWorld.Finalize();
					actorWorld.Initialize(); // 読込途中の例外では不完全なActorを残さず空Worldへ戻す。
				}
				return { false, 0, std::string("Level読込中に例外が発生しました: ") + exception.what() };
			}
		}

	private:
		static bool ValidateLevel(const nlohmann::json& levelJson)
		{
			if (!levelJson.is_object() || levelJson.value("Format", std::string{}) != "Ken4lowLevel") return false;
			const int version = levelJson.value("Version", 0);
			return version == 1 && levelJson.contains("Actors") && levelJson["Actors"].is_array(); // 現在はPhase 10形式Version 1だけを受理する。
		}

		static const ComponentFactory::ComponentTypeInfo* FindRegisteredComponentType(std::string_view className)
		{
			for (const ComponentFactory::ComponentTypeInfo& typeInfo : ComponentFactory::GetRegisteredComponentTypes())
			{
				if (typeInfo.className == className) return &typeInfo;
			}
			return nullptr;
		}

		static bool ValidateActors(const nlohmann::json& actorsJson, std::string& outMessage)
		{
			std::unordered_set<std::string> actorIds;
			std::unordered_map<std::string, std::string> parentByActorId;
			std::size_t actorIndex = 0;

			for (const nlohmann::json& entry : actorsJson)
			{
				if (!ValidateActorEntry(entry, outMessage)) return false;

				const std::string id = entry.value("Id", "Actor_" + std::to_string(actorIndex));
				if (id.empty())
				{
					outMessage = "Level内ActorのIdが空です";
					return false;
				}
				if (!actorIds.insert(id).second)
				{
					outMessage = "Level内ActorのIdが重複しています: " + id;
					return false;
				}
				parentByActorId[id] = entry.value("ParentId", std::string{});
				++actorIndex;
			}

			for (const auto& [actorId, parentId] : parentByActorId)
			{
				if (parentId.empty()) continue;
				if (parentId == actorId)
				{
					outMessage = "Actorが自分自身をParentIdに指定しています: " + actorId;
					return false;
				}
				if (!actorIds.contains(parentId))
				{
					outMessage = "存在しないParentIdが指定されています: " + parentId;
					return false;
				}
			}

			for (const auto& [actorId, unusedParentId] : parentByActorId)
			{
				(void)unusedParentId;
				std::unordered_set<std::string> ancestry;
				std::string currentId = actorId;
				while (!currentId.empty())
				{
					if (!ancestry.insert(currentId).second)
					{
						outMessage = "Actor間の親子関係が循環しています: " + actorId;
						return false;
					}
					const auto parentIt = parentByActorId.find(currentId);
					currentId = parentIt != parentByActorId.end() ? parentIt->second : std::string{};
				}
			}
			return true;
		}

		static bool ValidateActorEntry(const nlohmann::json& entry, std::string& outMessage)
		{
			if (!entry.is_object() || !entry.contains("Data") || !entry["Data"].is_object())
			{
				outMessage = "Level内ActorのDataが不正です";
				return false;
			}

			const nlohmann::json& actorJson = entry["Data"];
			if (!actorJson.contains("Class") || !actorJson["Class"].is_string())
			{
				outMessage = "Level内ActorのClassが不正です";
				return false;
			}
			if (!actorJson.contains("Components") || !actorJson["Components"].is_array())
			{
				outMessage = "Level内ActorのComponentsが不正です";
				return false;
			}

			const std::string actorClassName = actorJson["Class"].get<std::string>();
			if (!ActorFactory::IsRegistered(actorClassName))
			{
				outMessage = "未登録のActor Classです: " + actorClassName;
				return false;
			}

			std::unordered_map<std::string, std::string> parentBySceneComponentName;
			std::size_t rootComponentCount = 0;
			for (const nlohmann::json& componentJson : actorJson["Components"])
			{
				if (!componentJson.is_object() || !componentJson.contains("Class") || !componentJson["Class"].is_string())
				{
					outMessage = "Level内ComponentのClassが不正です: " + actorClassName;
					return false;
				}

				const std::string componentClassName = componentJson["Class"].get<std::string>();
				const ComponentFactory::ComponentTypeInfo* typeInfo = FindRegisteredComponentType(componentClassName);
				if (!typeInfo)
				{
					outMessage = "未登録のComponent Classです: " + componentClassName;
					return false;
				}

				const std::string componentType = componentJson.value("Type", std::string("ActorComponent"));
				const bool isSceneComponent = componentType == "SceneComponent";
				if (isSceneComponent != typeInfo->canBeRoot)
				{
					outMessage = "ComponentのTypeとClassが一致していません: " + componentClassName;
					return false;
				}
				if (!isSceneComponent) continue;

				const std::string componentName = componentJson.value("Name", std::string{});
				const std::string parentName = componentJson.value("Parent", std::string{});
				if (componentName.empty())
				{
					outMessage = "SceneComponentのNameが空です: " + componentClassName;
					return false;
				}
				if (!parentBySceneComponentName.emplace(componentName, parentName).second)
				{
					outMessage = "SceneComponentのNameが重複しています: " + componentName;
					return false;
				}
				if (parentName.empty()) ++rootComponentCount;
			}

			if (!parentBySceneComponentName.empty() && rootComponentCount != 1)
			{
				outMessage = "ActorにはRoot SceneComponentが1つ必要です: " + actorClassName;
				return false;
			}

			for (const auto& [componentName, parentName] : parentBySceneComponentName)
			{
				if (parentName.empty()) continue;
				if (parentName == componentName)
				{
					outMessage = "SceneComponentが自分自身をParentに指定しています: " + componentName;
					return false;
				}
				if (!parentBySceneComponentName.contains(parentName))
				{
					outMessage = "存在しないSceneComponent Parentが指定されています: " + parentName;
					return false;
				}
			}

			for (const auto& [componentName, unusedParentName] : parentBySceneComponentName)
			{
				(void)unusedParentName;
				std::unordered_set<std::string> ancestry;
				std::string currentName = componentName;
				while (!currentName.empty())
				{
					if (!ancestry.insert(currentName).second)
					{
						outMessage = "SceneComponent階層が循環しています: " + componentName;
						return false;
					}
					const auto parentIt = parentBySceneComponentName.find(currentName);
					currentName = parentIt != parentBySceneComponentName.end() ? parentIt->second : std::string{};
				}
			}
			return true;
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

#ifdef USE_IMGUI
		static void ApplyEditorState(const nlohmann::json& entry, Actor* actor)
		{
			if (!actor || !entry.contains("Editor") || !entry["Editor"].is_object()) return;
			const nlohmann::json& editor = entry["Editor"];
			EditorActorState state{};
			state.visible = editor.value("Visible", true);
			state.locked = editor.value("Locked", false);
			state.folderPath = editor.value("Folder", std::string{});
			EditorActorStateRegistry::GetInstance()->SetState(actor, state); // Editor起動時はOutliner固有状態もLevelと同時に復元する。
		}

		static void ApplyEditorCamera(const nlohmann::json& cameraJson)
		{
			if (!cameraJson.is_object()) return;
			DebugCamera* camera = CameraManager::GetInstance()->GetDebugCamera();
			if (!camera) return;
			if (cameraJson.contains("Position")) camera->SetTranslate(ReadVector3(cameraJson["Position"], camera->GetTranslate()));
			if (cameraJson.contains("Rotation")) camera->SetRotate(ReadVector3(cameraJson["Rotation"], camera->GetRotate()));
			camera->SetFovY(cameraJson.value("FovY", camera->GetFovY()));
			camera->SetNearClip(cameraJson.value("NearClip", camera->GetNearClip()));
			camera->SetFarClip(cameraJson.value("FarClip", camera->GetFarClip()));
			camera->RefreshViewProjection(); // SceneDefinition経由のLevel読込でもPhase 10保存時のEditor Cameraを復元する。
		}
#endif

		static void ApplyLighting(const nlohmann::json& lighting)
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
	};
} // namespace Ken4lowEngine
