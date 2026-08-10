#include "TransactionalLevelLoader.h"

#include "ActorJsonSerializer.h"
#include "ActorSpawnOptions.h"
#include "ActorWorld.h"
#include "CameraComponent.h"
#include "LevelSerializer.h"
#include "LightManager.h"
#include "PrefabInstanceRegistry.h"
#include "SceneComponent.h"
#include "ShadowSettings.h"
#include <Engine/Scene/Streaming/WorldPartitionManager.h>

#ifdef USE_IMGUI
#include <CameraManager.h>
#include <DebugCamera.h>
#include <Editor/EditorActorStateRegistry.h>
#endif

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	namespace
	{
		Vector3 ReadVector3(const nlohmann::json& value, const Vector3& fallback)
		{
			if (!value.is_array() || value.size() != 3) return fallback;
			return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>() };
		}

		Vector4 ReadVector4(const nlohmann::json& value, const Vector4& fallback)
		{
			if (!value.is_array() || value.size() != 4) return fallback;
			return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>() };
		}

		void FinalizeStagedActors(std::vector<std::unique_ptr<Actor>>& actors)
		{
			for (std::unique_ptr<Actor>& actor : actors)
			{
				if (actor) actor->FinalizeForWorld();
			}
			actors.clear();
		}

		void RestoreCameraRegistration(Actor& actor, const nlohmann::json& actorJson)
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

		void ApplyLighting(const nlohmann::json& lighting)
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

#ifdef USE_IMGUI
		void ApplyEditorState(const LevelActorDocument& document, Actor* actor)
		{
			if (!actor) return;
			EditorActorState state{};
			state.visible = document.editorVisible;
			state.locked = document.editorLocked;
			state.folderPath = document.editorFolder;
			EditorActorStateRegistry::GetInstance()->SetState(actor, state);
		}

		void ApplyEditorCamera(const nlohmann::json& cameraJson)
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
#endif
	}

	TransactionalLevelLoader::Result TransactionalLevelLoader::Load(
		const std::filesystem::path& levelPath,
		ActorWorld& actorWorld)
	{
		LevelDocument document{};
		const LevelSerializer::Result parseResult = LevelSerializer::LoadFromFile(levelPath, document);
		if (!parseResult.succeeded)
		{
			return { false, 0, parseResult.migrated, parseResult.sourceVersion, parseResult.message };
		}

		Result result = LoadDocument(document, actorWorld);
		result.sourceVersion = parseResult.sourceVersion;
		result.migrated = parseResult.migrated;
		if (result.succeeded && parseResult.migrated)
		{
			result.message += " (Version " + std::to_string(parseResult.sourceVersion) + " から移行)";
		}
		return result;
	}

	TransactionalLevelLoader::Result TransactionalLevelLoader::LoadDocument(
		const LevelDocument& document,
		ActorWorld& actorWorld)
	{
		Result result{};
		result.sourceVersion = document.sourceVersion;
		result.migrated = document.migrated;

		std::vector<std::unique_ptr<Actor>> stagedActors;
		std::unordered_map<std::string, Actor*> actorsById;
		stagedActors.reserve(document.actors.size());

		ActorSpawnOptions spawnOptions{};
		spawnOptions.applySpawnOffset = false;
		spawnOptions.disableAutoRegisterMainCamera = true; // Commit前のStaging ActorにGlobal Camera状態を変更させない。

		for (const LevelActorDocument& actorDocument : document.actors)
		{
			std::unique_ptr<Actor> actor = ActorJsonSerializer::CreateActorFromJson(actorDocument.resolvedData, spawnOptions);
			if (!actor)
			{
				FinalizeStagedActors(stagedActors);
				result.message = "Level ActorのStagingに失敗しました。現在のWorldは維持されます: " + actorDocument.id;
				return result;
			}

			Actor* actorPointer = actor.get();
			actorsById[actorDocument.id] = actorPointer;
			stagedActors.push_back(std::move(actor));
		}

		for (const LevelActorDocument& actorDocument : document.actors)
		{
			if (actorDocument.parentId.empty()) continue;
			const auto childIt = actorsById.find(actorDocument.id);
			const auto parentIt = actorsById.find(actorDocument.parentId);
			if (childIt == actorsById.end() || parentIt == actorsById.end())
			{
				FinalizeStagedActors(stagedActors);
				result.message = "Level Actorの親子関係解決に失敗しました。現在のWorldは維持されます: " + actorDocument.id;
				return result;
			}

			SceneComponent* childRoot = childIt->second ? childIt->second->GetRootComponent() : nullptr;
			SceneComponent* parentRoot = parentIt->second ? parentIt->second->GetRootComponent() : nullptr;
			if (!childRoot || !parentRoot)
			{
				FinalizeStagedActors(stagedActors);
				result.message = "親子関係を持つActorにRootComponentがありません: " + actorDocument.id;
				return result;
			}
			childRoot->AttachTo(parentRoot);
		}

		std::vector<Actor*> committedActors;
		if (!actorWorld.CommitStagedActors(std::move(stagedActors), &committedActors) ||
			committedActors.size() != document.actors.size())
		{
			result.message = "LevelのCommitを開始できませんでした。現在のWorldは維持されます。";
			return result;
		}

		for (std::size_t index = 0; index < document.actors.size(); ++index)
		{
			Actor* actor = committedActors[index];
			const LevelActorDocument& actorDocument = document.actors[index];
			if (!actor) continue;

			RestoreCameraRegistration(*actor, actorDocument.resolvedData);
			if (actorDocument.prefab.IsSet())
			{
				PrefabInstanceRegistry::GetInstance()->Register(actor, actorDocument.prefab.path);
			}
#ifdef USE_IMGUI
			ApplyEditorState(actorDocument, actor);
#endif
		}

		ApplyLighting(document.lighting);
#ifdef USE_IMGUI
		ApplyEditorCamera(document.camera);
#endif
		WorldPartitionManager::GetInstance()->Configure(&actorWorld, document.worldPartition, document.subLevels);
		actorWorld.SetSelectedEditorObject(nullptr, nullptr);

		result.succeeded = true;
		result.actorCount = committedActors.size();
		result.message = "LevelをトランザクションCommitしました: " + document.name;
		return result;
	}
} // namespace Ken4lowEngine
