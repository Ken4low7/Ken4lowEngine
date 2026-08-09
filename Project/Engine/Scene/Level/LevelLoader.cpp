#include "LevelLoader.h"
#include <fstream>
#include <json.hpp>
#include <cassert>
#include <iostream>
#include <cmath>

namespace Ken4lowEngine
{
	using json = nlohmann::json;

	namespace
	{
		Vector3 ReadTranslationToGameAxes(const json& transform)
		{
			Vector3 result{};
			if (transform.contains("translation") && transform["translation"].is_array() && transform["translation"].size() >= 3)
			{
				result.x = static_cast<float>(transform["translation"][0]);
				result.y = static_cast<float>(transform["translation"][2]);
				result.z = static_cast<float>(transform["translation"][1]);
			}
			return result;
		}

		Vector3 ReadRotationToGameAxes(const json& transform)
		{
			Vector3 result{};
			if (transform.contains("rotation") && transform["rotation"].is_array() && transform["rotation"].size() >= 3)
			{
				result.x = -static_cast<float>(transform["rotation"][0]);
				result.y = -static_cast<float>(transform["rotation"][2]);
				result.z = -static_cast<float>(transform["rotation"][1]);
			}
			return result;
		}

		Vector3 DegToRad(const Vector3& deg)
		{
			constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
			return { deg.x * kDegToRad, deg.y * kDegToRad, deg.z * kDegToRad };
		}

		Vector3 ConvertSourceColliderRotationToGameRotationDeg(const Vector3& sourceRotationDeg)
		{
			// BlenderのZ軸回転をゲーム座標のY軸Yawへ変換して、ステージコライダーの向きを合わせる
			return {
				-sourceRotationDeg.x,
				-sourceRotationDeg.z,
				-sourceRotationDeg.y,
			};
		}

		Vector3 ReadScaleToGameAxes(const json& transform)
		{
			Vector3 result{ 1.0f, 1.0f, 1.0f };
			if (transform.contains("scaling") && transform["scaling"].is_array() && transform["scaling"].size() >= 3)
			{
				result.x = static_cast<float>(transform["scaling"][0]);
				result.y = static_cast<float>(transform["scaling"][2]);
				result.z = static_cast<float>(transform["scaling"][1]);
			}
			return result;
		}

		Vector3 MultiplyScale(const Vector3& a, const Vector3& b)
		{
			return {
				a.x * b.x,
				a.y * b.y,
				a.z * b.z,
			};
		}

		bool IsSpawnPointType(const std::string& type)
		{
			return
				type == "PlayerSpawnPoint" ||
				type == "EnemySpawnPoint" ||
				type == "BossSpawnPoint";
		}

		bool HasSpawnProps(const json& object)
		{
			return object.contains("props") && object["props"].is_object();
		}

		bool IsIntroCamera(const std::string& type)
		{
			return type == "IntroCameraPoint" || type == "IntroLookAtPoint";
		}

		bool IsStaticMeshType(const std::string& type)
		{
			return type == "MESH" || type == "StaticMesh";
		}

		bool IsObjectiveType(const std::string& type)
		{
			return
				type == "DeviceObjective" ||
				type == "DefenseTarget" ||
				type == "EscapePoint" ||
				type == "BossPhaseTrigger";
		}
	}

	std::unique_ptr<LevelData> LevelLoader::LoadLevel(const std::string& filePath)
	{
		std::string fullPath = fileDirectory_ + filePath;
		std::ifstream file(fullPath);

		if (file.fail())
		{
			std::cerr << "ファイルを開くことができません: " << fullPath << std::endl;
			assert(false && "ファイルを開くことができません");
		}

		json deserialised;

		try
		{
			file >> deserialised;
		}
		catch (const std::exception&)
		{
			std::cerr << "JSONの読み込みに失敗しました: " << fullPath << std::endl;
			assert(false && "JSONの読み込みに失敗しました");
		}

		assert(deserialised.is_object() && "JSONの形式が正しくありません");
		assert(deserialised.contains("name") && "JSONに'name'キーが含まれていません");
		assert(deserialised["name"].is_string() && "'name'キーの値がオブジェクトではありません");

		std::string name = deserialised["name"].get<std::string>();
		assert(name.compare("scene") == 0);
		assert(deserialised.contains("objects") && deserialised["objects"].is_array() && "JSONに'objects'配列が含まれていません");

		std::unique_ptr<LevelData> levelData = std::make_unique<LevelData>();

		for (const json& object : deserialised["objects"])
		{
			ParseObjectRecursive(
				object,
				*levelData,
				Vector3{ 0.0f, 0.0f, 0.0f },
				Vector3{ 0.0f, 0.0f, 0.0f },
				Vector3{ 1.0f, 1.0f, 1.0f });
		}

		return levelData;
	}

	void LevelLoader::ParseObjectRecursive(
		const json& object,
		LevelData& levelData,
		const Vector3& parentPosition,
		const Vector3& parentRotation,
		const Vector3& parentScale)
	{
		assert(object.contains("type"));

		if (object.contains("disable") && object["disable"].is_boolean() && object["disable"].get<bool>())
		{
			return;
		}

		const std::string type = object["type"].get<std::string>();

		Vector3 localPosition{};
		Vector3 localRotation{};
		Vector3 localScale{ 1.0f, 1.0f, 1.0f };

		if (object.contains("transform") && object["transform"].is_object())
		{
			const json& transform = object["transform"];
			localPosition = ReadTranslationToGameAxes(transform);
			localRotation = ReadRotationToGameAxes(transform);
			localScale = ReadScaleToGameAxes(transform);
		}

		const Vector3 combinedPosition = {
			parentPosition.x + localPosition.x,
			parentPosition.y + localPosition.y,
			parentPosition.z + localPosition.z,
		};

		const Vector3 combinedRotation = {
			parentRotation.x + localRotation.x,
			parentRotation.y + localRotation.y,
			parentRotation.z + localRotation.z,
		};

		const Vector3 combinedScale = MultiplyScale(parentScale, localScale);

		ObjectData* objectData = nullptr;
		const bool hasCollider = object.contains("collider") && object["collider"].is_object();

		const bool shouldStore =
			IsStaticMeshType(type) ||
			hasCollider ||
			IsSpawnPointType(type) ||
			IsIntroCamera(type) ||
			IsObjectiveType(type);

		if (shouldStore)
		{
			levelData.objects.emplace_back(ObjectData{});
			objectData = &levelData.objects.back();

			objectData->type = type;

			if (object.contains("name") && object["name"].is_string())
			{
				objectData->name = object["name"].get<std::string>();
			}
			if (object.contains("file_name") && object["file_name"].is_string())
			{
				objectData->modelName = object["file_name"].get<std::string>();
			}
			else if (object.contains("model") && object["model"].is_string())
			{
				objectData->modelName = object["model"].get<std::string>();
			}

			objectData->position = combinedPosition;
			objectData->rotation = combinedRotation;
			objectData->scale = combinedScale;

			if (HasSpawnProps(object))
			{
				const json& props = object["props"];
				objectData->hasSpawnProps = true;

				if (props.contains("wave") && props["wave"].is_number_integer())
				{
					objectData->spawnProps.wave = props["wave"].get<int>();
				}
				if (props.contains("group") && props["group"].is_number_integer())
				{
					objectData->spawnProps.group = props["group"].get<int>();
				}
				if (props.contains("count") && props["count"].is_number_integer())
				{
					objectData->spawnProps.count = props["count"].get<int>();
				}
				if (props.contains("archetype") && props["archetype"].is_string())
				{
					objectData->spawnProps.archetype = props["archetype"].get<std::string>();
					objectData->spawnProps.hasArchetype = true;
				}
				if (props.contains("enemyType") && props["enemyType"].is_string())
				{
					objectData->spawnProps.enemyType = props["enemyType"].get<std::string>();
					objectData->spawnProps.hasEnemyType = true;
				}
			}

			if (object.contains("props") && object["props"].is_object())
			{
				const json& props = object["props"];

				if (type == "DeviceObjective")
				{
					objectData->hasDeviceObjectiveProps = true;

					if (props.contains("objective_id") && props["objective_id"].is_string())
					{
						objectData->deviceObjectiveProps.objectiveId = props["objective_id"].get<std::string>();
					}
					if (props.contains("ui_name") && props["ui_name"].is_string())
					{
						objectData->deviceObjectiveProps.uiName = props["ui_name"].get<std::string>();
					}
					if (props.contains("activate_time") && props["activate_time"].is_number())
					{
						objectData->deviceObjectiveProps.activateTime = props["activate_time"].get<float>();
					}
				}
				else if (type == "DefenseTarget")
				{
					objectData->hasDefenseTargetProps = true;

					if (props.contains("objective_id") && props["objective_id"].is_string())
					{
						objectData->defenseTargetProps.objectiveId = props["objective_id"].get<std::string>();
					}
					if (props.contains("ui_name") && props["ui_name"].is_string())
					{
						objectData->defenseTargetProps.uiName = props["ui_name"].get<std::string>();
					}
					if (props.contains("max_hp") && props["max_hp"].is_number_integer())
					{
						objectData->defenseTargetProps.maxHp = props["max_hp"].get<int>();
					}
					if (props.contains("start_hp") && props["start_hp"].is_number_integer())
					{
						objectData->defenseTargetProps.startHp = props["start_hp"].get<int>();
					}
					if (props.contains("defense_time") && props["defense_time"].is_number())
					{
						objectData->defenseTargetProps.defenseTime = props["defense_time"].get<float>();
					}
				}
				else if (type == "EscapePoint")
				{
					objectData->hasEscapePointProps = true;

					if (props.contains("objective_id") && props["objective_id"].is_string())
					{
						objectData->escapePointProps.objectiveId = props["objective_id"].get<std::string>();
					}
					if (props.contains("ui_name") && props["ui_name"].is_string())
					{
						objectData->escapePointProps.uiName = props["ui_name"].get<std::string>();
					}
					if (props.contains("activate_time") && props["activate_time"].is_number())
					{
						objectData->escapePointProps.activateTime = props["activate_time"].get<float>();
					}
				}
				else if (type == "BossPhaseTrigger")
				{
					objectData->hasBossPhaseTriggerProps = true;

					if (props.contains("phase") && props["phase"].is_number_integer())
					{
						objectData->bossPhaseTriggerProps.phase = props["phase"].get<int>();
					}
					if (props.contains("trigger_type") && props["trigger_type"].is_string())
					{
						objectData->bossPhaseTriggerProps.triggerType = props["trigger_type"].get<std::string>();
					}
					if (props.contains("threshold") && props["threshold"].is_number())
					{
						objectData->bossPhaseTriggerProps.threshold = props["threshold"].get<float>();
					}
					if (props.contains("event_id") && props["event_id"].is_string())
					{
						objectData->bossPhaseTriggerProps.eventId = props["event_id"].get<std::string>();
					}
				}
			}

			if (type == "IntroCameraPoint" &&
				object.contains("props") &&
				object["props"].is_object())
			{
				const json& props = object["props"];
				objectData->hasIntroCameraProps = true;

				if (props.contains("order") && props["order"].is_number_integer())
				{
					objectData->introCameraProps.order = props["order"].get<int>();
				}
				if (props.contains("duration") && props["duration"].is_number())
				{
					objectData->introCameraProps.duration = props["duration"].get<float>();
				}
				if (props.contains("fov") && props["fov"].is_number())
				{
					objectData->introCameraProps.fov = props["fov"].get<float>();
				}
				if (props.contains("target_name") && props["target_name"].is_string())
				{
					objectData->introCameraProps.targetName = props["target_name"].get<std::string>();
				}

				if (props.contains("interp_mode") && props["interp_mode"].is_string())
				{
					objectData->introCameraProps.interpMode = props["interp_mode"].get<std::string>();
				}
				if (props.contains("aim_mode") && props["aim_mode"].is_string())
				{
					objectData->introCameraProps.aimMode = props["aim_mode"].get<std::string>();
				}
			}
		}

		if (hasCollider && objectData)
		{
			const json& jc = object["collider"];
			objectData->collider.enabled = true;

			if (jc.contains("type") && jc["type"].is_string())
			{
				objectData->collider.type = jc["type"].get<std::string>();
			}

			if (jc.contains("center") && jc["center"].is_array() && jc["center"].size() >= 3)
			{
				objectData->collider.center.x = -static_cast<float>(jc["center"][0]);
				objectData->collider.center.y = static_cast<float>(jc["center"][2]);
				objectData->collider.center.z = -static_cast<float>(jc["center"][1]);
			}

			if (jc.contains("size") && jc["size"].is_array() && jc["size"].size() >= 3)
			{
				objectData->collider.size.x = static_cast<float>(jc["size"][0]);
				objectData->collider.size.y = static_cast<float>(jc["size"][2]);
				objectData->collider.size.z = static_cast<float>(jc["size"][1]);
			}

			if (jc.contains("collision_type") && jc["collision_type"].is_string())
			{
				objectData->collider.collisionType = jc["collision_type"].get<std::string>();
			}

			if (jc.contains("collision_type_id") && jc["collision_type_id"].is_number_integer())
			{
				objectData->collider.collisionTypeId = jc["collision_type_id"].get<int>();
			}

			// JSONのcollider_rotationを保持し、回転付きステージコライダーとして生成する
			const bool hasColliderRotation = jc.contains("collider_rotation") && jc["collider_rotation"].is_array() && jc["collider_rotation"].size() >= 3;
			const bool hasRotation = jc.contains("rotation") && jc["rotation"].is_array() && jc["rotation"].size() >= 3;
			if (hasColliderRotation || hasRotation)
			{
				const json& rotationJson = hasColliderRotation ? jc["collider_rotation"] : jc["rotation"];
				Vector3 sourceColliderRotationDeg{};
				sourceColliderRotationDeg.x = -static_cast<float>(rotationJson[0]);
				sourceColliderRotationDeg.y = static_cast<float>(rotationJson[1]);
				sourceColliderRotationDeg.z = -static_cast<float>(rotationJson[2]);

				const Vector3 gameColliderRotationDeg = ConvertSourceColliderRotationToGameRotationDeg(sourceColliderRotationDeg);
				objectData->collider.sourceRotationDeg = sourceColliderRotationDeg;
				objectData->collider.convertedRotationDeg = gameColliderRotationDeg;
				objectData->collider.rotation = DegToRad(gameColliderRotationDeg);
				objectData->collider.hasRotation = true;
				objectData->collider.fromColliderRotation = hasColliderRotation;
			}
		}

		if (object.contains("children") && object["children"].is_array())
		{
			for (const json& child : object["children"])
			{
				ParseObjectRecursive(child, levelData, combinedPosition, combinedRotation, combinedScale);
			}
		}
	}
}
