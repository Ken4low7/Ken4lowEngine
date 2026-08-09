#pragma once

#include <Vector3.h>
#include <json.hpp>

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// Blender JSONのmetaを変換せず保持する入力メタデータ。
	struct BlenderSceneMetadata
	{
		std::string units;
		std::string rotationUnit;
		std::string sourceForward;
		std::string sourceUp;
		std::string gameForward;
		std::string gameUp;
		nlohmann::json raw = nlohmann::json::object();
	};

	/// Blender JSONのstage情報を入力データのまま保持する。
	struct BlenderStageMetadata
	{
		std::string id;
		std::string mode;
		nlohmann::json raw = nlohmann::json::object();
	};

	/// Blender JSONに書かれたローカルTransformを入力座標系のまま保持する。
	struct BlenderTransformData
	{
		Vector3 translation{};
		Vector3 rotation{};
		Vector3 scaling{ 1.0f, 1.0f, 1.0f };
	};

	/// Blenderから出力された1オブジェクト分の入力データ。
	struct BlenderObjectData
	{
		std::string type;
		std::string name;
		std::string collection;
		std::vector<std::string> collections;
		std::string fileName;
		std::string modelName;
		bool disabled = false;

		BlenderTransformData transform;
		nlohmann::json properties = nlohmann::json::object();
		nlohmann::json collider = nlohmann::json::object();
		nlohmann::json raw = nlohmann::json::object();

		std::vector<BlenderObjectData> children;
	};

	/// Blender JSONそのものをエンジン実行用LevelDataへ変換する前の入力データとして保持する。
	struct BlenderSceneData
	{
		int schemaVersion = 0;
		std::string name;
		BlenderSceneMetadata metadata;
		BlenderStageMetadata stage;
		std::vector<BlenderObjectData> objects;
		nlohmann::json entities = nlohmann::json::object();
		nlohmann::json raw = nlohmann::json::object();
	};
} // namespace Ken4lowEngine
