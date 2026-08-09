#pragma once

#include "BlenderSceneData.h"

#include <exception>
#include <fstream>
#include <string>
#include <utility>

namespace Ken4lowEngine
{
	/// Blender exporter JSONを座標変換せずBlenderSceneDataへ読み込む入力専用Loader。
	class BlenderSceneLoader
	{
	public:
		struct Result
		{
			bool succeeded = false;
			BlenderSceneData scene;
			std::string message;
		};

		static Result Load(const std::string& relativePath)
		{
			const std::string fullPath = fileDirectory_ + relativePath;

			try
			{
				std::ifstream file(fullPath);
				if (!file.is_open())
				{
					return { false, {}, "Blender JSONを開けません: " + fullPath };
				}

				nlohmann::json source;
				file >> source;
				if (!source.is_object())
				{
					return { false, {}, "Blender JSONのルートがobjectではありません: " + fullPath };
				}
				if (!source.contains("objects") || !source["objects"].is_array())
				{
					return { false, {}, "Blender JSONにobjects配列がありません: " + fullPath };
				}

				BlenderSceneData scene{};
				scene.schemaVersion = source.value("schema_version", 0);
				scene.name = source.value("name", std::string{});
				scene.raw = source;
				ReadMetadata(source, scene.metadata);
				ReadStageMetadata(source, scene.stage);

				if (source.contains("entities") && source["entities"].is_object())
				{
					scene.entities = source["entities"]; // 実ステージ固有のSpawn/Trigger/Collision分類を型決め打ちせず保持する。
				}

				for (const nlohmann::json& objectJson : source["objects"])
				{
					if (!objectJson.is_object()) continue;
					scene.objects.push_back(ParseObject(objectJson));
				}

				return { true, std::move(scene), "Blender JSONを入力データとして読み込みました: " + relativePath };
			}
			catch (const std::exception& exception)
			{
				return { false, {}, std::string("Blender JSON読込中に例外が発生しました: ") + exception.what() };
			}
		}

	private:
		static Vector3 ReadVector3(const nlohmann::json& value, const Vector3& fallback)
		{
			if (!value.is_array() || value.size() < 3) return fallback;
			return {
				value[0].get<float>(),
				value[1].get<float>(),
				value[2].get<float>(),
			};
		}

		static void ReadMetadata(const nlohmann::json& source, BlenderSceneMetadata& metadata)
		{
			if (!source.contains("meta") || !source["meta"].is_object()) return;

			const nlohmann::json& meta = source["meta"];
			metadata.units = meta.value("units", std::string{});
			metadata.rotationUnit = meta.value("rotation_unit", std::string{});
			metadata.sourceForward = meta.value("source_forward", std::string{});
			metadata.sourceUp = meta.value("source_up", std::string{});
			metadata.gameForward = meta.value("game_forward", std::string{});
			metadata.gameUp = meta.value("game_up", std::string{});
			metadata.raw = meta;
		}

		static void ReadStageMetadata(const nlohmann::json& source, BlenderStageMetadata& stage)
		{
			if (!source.contains("stage") || !source["stage"].is_object()) return;

			const nlohmann::json& stageJson = source["stage"];
			stage.id = stageJson.value("id", std::string{});
			stage.mode = stageJson.value("mode", std::string{});
			stage.raw = stageJson;
		}

		static BlenderObjectData ParseObject(const nlohmann::json& objectJson)
		{
			BlenderObjectData object{};
			object.type = objectJson.value("type", std::string{});
			object.name = objectJson.value("name", std::string{});
			object.collection = objectJson.value("collection", std::string{});
			object.fileName = objectJson.value("file_name", std::string{});
			object.modelName = objectJson.value("model", std::string{});
			object.disabled = objectJson.value("disable", false);
			object.raw = objectJson;

			if (objectJson.contains("collections") && objectJson["collections"].is_array())
			{
				for (const nlohmann::json& collection : objectJson["collections"])
				{
					if (collection.is_string()) object.collections.push_back(collection.get<std::string>());
				}
			}

			if (objectJson.contains("transform") && objectJson["transform"].is_object())
			{
				const nlohmann::json& transform = objectJson["transform"];
				if (transform.contains("translation")) object.transform.translation = ReadVector3(transform["translation"], {});
				if (transform.contains("rotation")) object.transform.rotation = ReadVector3(transform["rotation"], {});
				if (transform.contains("scaling")) object.transform.scaling = ReadVector3(transform["scaling"], { 1.0f, 1.0f, 1.0f });
			}

			if (objectJson.contains("props") && objectJson["props"].is_object())
			{
				object.properties = objectJson["props"]; // 任意Custom Propertiesは型を決め打ちせず、そのまま次のImport段階へ渡す。
			}
			if (objectJson.contains("collider") && objectJson["collider"].is_object())
			{
				object.collider = objectJson["collider"];
			}

			if (objectJson.contains("children") && objectJson["children"].is_array())
			{
				for (const nlohmann::json& childJson : objectJson["children"])
				{
					if (!childJson.is_object()) continue;
					object.children.push_back(ParseObject(childJson));
				}
			}

			return object;
		}

		static inline const std::string fileDirectory_ = "Resources/JSON/";
	};
} // namespace Ken4lowEngine
