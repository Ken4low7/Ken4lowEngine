#pragma once

#include "BlenderSceneData.h"

#include <json.hpp>

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace Ken4lowEngine
{
	/// BlenderSceneDataをActor/Componentで復元可能なKen4lowLevel JSONへ変換するImporter。
	class BlenderLevelImporter
	{
	public:
		struct Options
		{
			std::string levelName = "ImportedBlenderLevel";
			std::string sourceJsonPath;
			std::string stageModelPath;
			std::string stageActorName = "ImportedStage";
			std::string stageActorId = "ImportedStageActor";
			bool importStageColliders = true;
		};

		struct Result
		{
			bool succeeded = false;
			nlohmann::json levelJson = nlohmann::json::object();
			std::size_t sourceObjectCount = 0;
			std::size_t sourceMeshCount = 0;
			std::size_t sourceColliderCount = 0;
			std::size_t sourcePropertyCount = 0;
			std::size_t sourcePlayerSpawnCount = 0;
			std::size_t entityGroupCount = 0;
			std::size_t entityEntryCount = 0;
			std::size_t importedActorCount = 0;
			std::size_t importedColliderComponentCount = 0;
			std::size_t unsupportedColliderCount = 0;
			std::string message;
		};

		static Result Import(const BlenderSceneData& source, const Options& options)
		{
			Result result{};
			if (options.stageModelPath.empty())
			{
				result.message = "一体型ステージをActor化するためのStageModelPathが空です。";
				return result;
			}

			nlohmann::json sourceObjects = nlohmann::json::array();
			nlohmann::json actorComponents = nlohmann::json::array();
			actorComponents.push_back(BuildStageModelComponent(options));

			const ImportTransform rootTransform{};
			for (const BlenderObjectData& object : source.objects)
			{
				AppendSourceObject(object, std::string{}, 0, rootTransform, sourceObjects, actorComponents, options, result);
			}
			CountEntities(source.entities, result);

			const nlohmann::json stageActor = BuildIntegratedStageActor(options, std::move(actorComponents));
			result.levelJson = {
				{ "Format", "Ken4lowLevel" },
				{ "Version", 1 },
				{ "Name", options.levelName },
				{ "LevelSettings", {
					{ "ImportMode", "IntegratedStageModel" },
					{ "SourceJson", options.sourceJsonPath },
					{ "StageModelPath", options.stageModelPath },
					{ "SourceSchemaVersion", source.schemaVersion },
					{ "SourceStageId", source.stage.id },
					{ "SourceStageMode", source.stage.mode },
					{ "StageColliderComponents", result.importedColliderComponentCount },
				} },
				{ "Actors", nlohmann::json::array({ stageActor }) },
				{ "Lighting", nlohmann::json::object() },
				{ "Camera", nlohmann::json::object() },
				{ "Environment", nlohmann::json::object() },
				{ "ImportSource", {
					{ "SchemaVersion", source.schemaVersion },
					{ "SceneName", source.name },
					{ "Metadata", source.metadata.raw },
					{ "Stage", source.stage.raw },
					{ "Entities", source.entities },
					{ "ObjectCount", result.sourceObjectCount },
					{ "MeshCount", result.sourceMeshCount },
					{ "ColliderCount", result.sourceColliderCount },
					{ "PropertyCount", result.sourcePropertyCount },
					{ "PlayerSpawnCount", result.sourcePlayerSpawnCount },
					{ "EntityGroupCount", result.entityGroupCount },
					{ "EntityEntryCount", result.entityEntryCount },
					{ "ImportedColliderComponentCount", result.importedColliderComponentCount },
					{ "UnsupportedColliderCount", result.unsupportedColliderCount },
					{ "Objects", std::move(sourceObjects) },
				} },
			};

			result.importedActorCount = result.levelJson["Actors"].size();
			result.succeeded = true;
			result.message = "実ステージをModelComponentとStageColliderComponent群のKen4lowLevelデータへ変換しました。";
			return result;
		}

	private:
		struct ImportTransform
		{
			Vector3 position{};
			Vector3 rotationDeg{};
			Vector3 scale{ 1.0f, 1.0f, 1.0f };
		};

		static nlohmann::json BuildStageModelComponent(const Options& options)
		{
			return {
				{ "Active", true },
				{ "Class", "ModelComponent" },
				{ "DrawOrder", 0 },
				{ "LocalPosition", { 0.0f, 0.0f, 0.0f } },
				{ "LocalRotation", { 0.0f, 0.0f, 0.0f } },
				{ "LocalScale", { 1.0f, 1.0f, 1.0f } },
				{ "ModelPath", options.stageModelPath },
				{ "Name", "Stage Model Component" },
				{ "Parent", "" },
				{ "Type", "SceneComponent" },
				{ "UpdateOrder", 0 },
				{ "Visible", true },
			};
		}

		static nlohmann::json BuildIntegratedStageActor(const Options& options, nlohmann::json components)
		{
			return {
				{ "Id", options.stageActorId },
				{ "ParentId", "" },
				{ "Editor", {
					{ "Visible", true },
					{ "Locked", false },
					{ "Folder", "Imported/Stage" },
				} },
				{ "Data", {
					{ "Active", true },
					{ "Class", "Actor" },
					{ "Components", std::move(components) },
					{ "Layer", "WorldStatic" },
					{ "Name", options.stageActorName },
					{ "Tags", nlohmann::json::array({ "BlenderImported", "StageVisual", "StageCollision" }) },
				} },
			};
		}

		static void CountEntities(const nlohmann::json& entities, Result& result)
		{
			if (!entities.is_object()) return;
			result.entityGroupCount = entities.size();
			for (const auto& [unusedName, value] : entities.items())
			{
				(void)unusedName;
				if (value.is_array() || value.is_object()) result.entityEntryCount += value.size();
			}
		}

		static Vector3 Multiply(const Vector3& lhs, const Vector3& rhs)
		{
			return { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };
		}

		static Vector3 ConvertPositionToGameAxes(const Vector3& source)
		{
			return { source.x, source.z, source.y };
		}

		static Vector3 ConvertObjectRotationToGameDeg(const Vector3& sourceDeg)
		{
			return { -sourceDeg.x, -sourceDeg.z, -sourceDeg.y };
		}

		static Vector3 ConvertScaleToGameAxes(const Vector3& source)
		{
			return { source.x, source.z, source.y };
		}

		static Vector3 ConvertColliderCenterToGameAxes(const Vector3& source)
		{
			// 既存LevelLoaderと同じCollider中心変換を使い、旧ステージとの配置互換を保つ。
			return { -source.x, source.z, -source.y };
		}

		static Vector3 ConvertColliderSizeToGameAxes(const Vector3& source)
		{
			return { source.x, source.z, source.y };
		}

		static Vector3 ConvertColliderRotationToGameDeg(const Vector3& sourceDeg)
		{
			return { sourceDeg.x, sourceDeg.z, -sourceDeg.y };
		}

		static Vector3 DegToRad(const Vector3& degrees)
		{
			constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
			return { degrees.x * kDegToRad, degrees.y * kDegToRad, degrees.z * kDegToRad };
		}

		static Vector3 ReadJsonVector3(const nlohmann::json& value, const Vector3& fallback = {})
		{
			if (!value.is_array() || value.size() < 3) return fallback;
			return {
				value[0].get<float>(),
				value[1].get<float>(),
				value[2].get<float>(),
			};
		}

		static ImportTransform CombineTransform(const ImportTransform& parent, const BlenderTransformData& local)
		{
			const Vector3 localPosition = ConvertPositionToGameAxes(local.translation);
			const Vector3 localRotationDeg = ConvertObjectRotationToGameDeg(local.rotation);
			const Vector3 localScale = ConvertScaleToGameAxes(local.scaling);

			ImportTransform combined{};
			combined.position = parent.position + localPosition;
			combined.rotationDeg = parent.rotationDeg + localRotationDeg;
			combined.scale = Multiply(parent.scale, localScale);
			return combined;
		}

		static bool BuildStageColliderComponent(
			const BlenderObjectData& object,
			const std::string& objectPath,
			const ImportTransform& objectTransform,
			std::size_t colliderIndex,
			nlohmann::json& outComponent)
		{
			if (!object.collider.is_object() || object.collider.empty()) return false;
			const std::string shapeType = object.collider.value("type", std::string{});
			if (shapeType != "BOX") return false;

			const Vector3 sourceCenter = object.collider.contains("center")
				? ReadJsonVector3(object.collider["center"])
				: Vector3{};
			const Vector3 sourceSize = object.collider.contains("size")
				? ReadJsonVector3(object.collider["size"], { 1.0f, 1.0f, 1.0f })
				: Vector3{ 1.0f, 1.0f, 1.0f };

			const Vector3 centerLocal = ConvertColliderCenterToGameAxes(sourceCenter);
			const Vector3 sizeLocal = ConvertColliderSizeToGameAxes(sourceSize);
			const Vector3 centerWorld = objectTransform.position + Multiply(centerLocal, objectTransform.scale);
			const Vector3 halfSize = {
				0.5f * std::abs(sizeLocal.x * objectTransform.scale.x),
				0.5f * std::abs(sizeLocal.y * objectTransform.scale.y),
				0.5f * std::abs(sizeLocal.z * objectTransform.scale.z),
			};

			const bool hasColliderRotation = object.collider.contains("collider_rotation") && object.collider["collider_rotation"].is_array();
			const bool hasRotation = object.collider.contains("rotation") && object.collider["rotation"].is_array();
			const Vector3 sourceColliderRotationDeg = hasColliderRotation
				? ReadJsonVector3(object.collider["collider_rotation"])
				: (hasRotation ? ReadJsonVector3(object.collider["rotation"]) : Vector3{});
			const Vector3 finalRotationRad = DegToRad(objectTransform.rotationDeg + ConvertColliderRotationToGameDeg(sourceColliderRotationDeg));

			const std::string collisionTag = object.collider.value("collision_type", std::string("WorldStatic"));
			const int collisionTypeId = object.collider.value("collision_type_id", 0);
			const bool isTrigger = collisionTag == "Ladder";
			const std::string componentName = "Stage Collider " + std::to_string(colliderIndex) + " : " + object.name;

			outComponent = {
				{ "Active", true },
				{ "Class", "StageColliderComponent" },
				{ "CollisionLayer", 2 },
				{ "CollisionTag", collisionTag },
				{ "CollisionTypeId", collisionTypeId },
				{ "DrawOrder", 0 },
				{ "HalfSize", { halfSize.x, halfSize.y, halfSize.z } },
				{ "IsTrigger", isTrigger },
				{ "LocalPosition", { centerWorld.x, centerWorld.y, centerWorld.z } },
				{ "LocalRotation", { finalRotationRad.x, finalRotationRad.y, finalRotationRad.z } },
				{ "LocalScale", { 1.0f, 1.0f, 1.0f } },
				{ "Name", componentName },
				{ "Parent", "Stage Model Component" },
				{ "ShapeType", "OBB" },
				{ "SourceObjectPath", objectPath },
				{ "Type", "SceneComponent" },
				{ "UpdateOrder", 0 },
				{ "Visible", true },
			};
			return true;
		}

		static void AppendSourceObject(
			const BlenderObjectData& object,
			const std::string& parentPath,
			std::size_t depth,
			const ImportTransform& parentTransform,
			nlohmann::json& outObjects,
			nlohmann::json& outActorComponents,
			const Options& options,
			Result& result)
		{
			++result.sourceObjectCount;
			if (object.type == "MESH" || object.type == "StaticMesh") ++result.sourceMeshCount;
			if (!object.collider.empty()) ++result.sourceColliderCount;
			if (!object.properties.empty()) ++result.sourcePropertyCount;
			if (object.type == "PlayerSpawn" || object.type == "PlayerSpawnPoint") ++result.sourcePlayerSpawnCount;

			const std::string objectPath = parentPath.empty() ? object.name : parentPath + "/" + object.name;
			const ImportTransform combinedTransform = CombineTransform(parentTransform, object.transform);
			nlohmann::json record = {
				{ "Path", objectPath },
				{ "ParentPath", parentPath },
				{ "Depth", depth },
				{ "Name", object.name },
				{ "Type", object.type },
				{ "Collection", object.collection },
				{ "Collections", object.collections },
				{ "Disabled", object.disabled },
				{ "SourceTransform", {
					{ "Translation", { object.transform.translation.x, object.transform.translation.y, object.transform.translation.z } },
					{ "Rotation", { object.transform.rotation.x, object.transform.rotation.y, object.transform.rotation.z } },
					{ "Scale", { object.transform.scaling.x, object.transform.scaling.y, object.transform.scaling.z } },
				} },
			};
			if (!object.properties.empty()) record["Properties"] = object.properties;
			if (!object.collider.empty()) record["Collider"] = object.collider;
			outObjects.push_back(std::move(record));

			if (options.importStageColliders && !object.disabled && !object.collider.empty())
			{
				nlohmann::json colliderComponent;
				if (BuildStageColliderComponent(object, objectPath, combinedTransform, result.importedColliderComponentCount, colliderComponent))
				{
					outActorComponents.push_back(std::move(colliderComponent));
					++result.importedColliderComponentCount;
				}
				else
				{
					++result.unsupportedColliderCount;
				}
			}

			for (const BlenderObjectData& child : object.children)
			{
				AppendSourceObject(child, objectPath, depth + 1, combinedTransform, outObjects, outActorComponents, options, result);
			}
		}
	};
} // namespace Ken4lowEngine
