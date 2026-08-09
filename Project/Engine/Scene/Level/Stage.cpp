#include "Stage.h"

#include "CollisionManager.h"
#include "CollisionUtility.h"
#include "LevelLoader.h"
#include "StageAssetLoader.h"
#include "StageCollisionBuilder.h"
#include "Object3DCommon.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace
{
	using Ken4lowEngine::LevelData;
	using Ken4lowEngine::ObjectData;
	using Ken4lowEngine::Vector3;

	bool IsExpandedMineStage(const std::string& levelJsonPath)
	{
		return levelJsonPath.find("wasureraretakoudou") != std::string::npos;
	}

	void AddMineBox(
		LevelData& levelData,
		std::string name,
		const Vector3& position,
		const Vector3& scale,
		const char* collisionType = nullptr,
		const Vector3& rotation = {})
	{
		ObjectData data{};
		data.name = std::move(name);
		data.type = "StaticMesh";
		data.modelName = "Sample/cube.gltf";
		data.position = position;
		data.rotation = rotation;
		data.scale = scale;
		if (collisionType)
		{
			data.collider.enabled = true;
			data.collider.type = "BOX";
			data.collider.size = { 2.0f, 2.0f, 2.0f };
			data.collider.collisionType = collisionType;
		}
		levelData.objects.push_back(std::move(data));
	}

	void AddMineStairs(
		LevelData& levelData,
		const std::string& prefix,
		const Vector3& start,
		const Vector3& stepOffset,
		const Vector3& footprint,
		int stepCount,
		float heightPerStep)
	{
		for (int index = 0; index < stepCount; ++index)
		{
			const float topHeight = heightPerStep * static_cast<float>(index + 1);
			Vector3 position = start + stepOffset * static_cast<float>(index);
			position.y = topHeight * 0.5f;
			AddMineBox(
				levelData,
				prefix + std::to_string(index + 1),
				position,
				{ footprint.x, topHeight * 0.5f, footprint.z },
				"Floor");
		}
	}

	void BuildExpandedMineLayout(LevelData& levelData)
	{
		levelData.objects.erase(
			std::remove_if(
				levelData.objects.begin(),
				levelData.objects.end(),
				[](const ObjectData& data)
				{
					return data.type == "StaticMesh" || data.type == "MESH";
				}),
			levelData.objects.end());
		levelData.objects.reserve(levelData.objects.size() + 260u);

		for (int index = 0; index < 10; ++index)
		{
			const float z = -46.0f + static_cast<float>(index) * 18.0f;
			AddMineBox(levelData, "Floor_Long_" + std::to_string(index + 1), { 0.0f, -0.5f, z }, { 50.0f, 0.5f, 9.0f }, "Floor");
		}

		AddMineBox(levelData, "Wall_South", { 0.0f, 4.0f, -56.0f }, { 51.0f, 4.0f, 1.0f }, "Obstacle");
		AddMineBox(levelData, "Wall_North", { 0.0f, 4.0f, 126.0f }, { 51.0f, 4.0f, 1.0f }, "Obstacle");
		AddMineBox(levelData, "Wall_West", { -51.0f, 4.0f, 35.0f }, { 1.0f, 4.0f, 91.0f }, "Obstacle");
		AddMineBox(levelData, "Wall_East", { 51.0f, 4.0f, 35.0f }, { 1.0f, 4.0f, 91.0f }, "Obstacle");
		AddMineBox(levelData, "Ceiling_Blocker", { 0.0f, 9.5f, 35.0f }, { 50.0f, 0.5f, 90.0f }, "Obstacle"); // 外周や梁を越えて坑道上へ抜ける経路を天井側からも塞ぐ。

		struct BoxSpec
		{
			const char* name;
			Vector3 position;
			Vector3 scale;
		};

		const std::array<BoxSpec, 18> walls = {{
			{ "EntryGate_L", { -30.0f, 3.0f, -34.0f }, { 20.0f, 3.0f, 1.0f } },
			{ "EntryGate_R", { 30.0f, 3.0f, -34.0f }, { 20.0f, 3.0f, 1.0f } },
			{ "MainWall_L_01", { -14.0f, 3.0f, -24.0f }, { 1.0f, 3.0f, 8.0f } },
			{ "MainWall_L_02", { -14.0f, 3.0f, 10.0f }, { 1.0f, 3.0f, 16.0f } },
			{ "MainWall_L_03", { -14.0f, 3.0f, 45.0f }, { 1.0f, 3.0f, 8.0f } },
			{ "MainWall_R_01", { 14.0f, 3.0f, -18.0f }, { 1.0f, 3.0f, 14.0f } },
			{ "MainWall_R_02", { 14.0f, 3.0f, 20.0f }, { 1.0f, 3.0f, 10.0f } },
			{ "MainWall_R_03", { 14.0f, 3.0f, 60.0f }, { 1.0f, 3.0f, 7.0f } },
			{ "WestRoom_South", { -33.0f, 3.0f, -25.0f }, { 17.0f, 3.0f, 1.0f } },
			{ "WestRoom_North", { -33.0f, 3.0f, 7.0f }, { 17.0f, 3.0f, 1.0f } },
			{ "EastRoom_South", { 33.0f, 3.5f, 30.0f }, { 17.0f, 3.5f, 1.0f } },
			{ "EastRoom_North", { 33.0f, 3.5f, 62.0f }, { 17.0f, 3.5f, 1.0f } },
			{ "DeepGate_L", { -31.0f, 4.0f, 72.0f }, { 19.0f, 4.0f, 1.0f } },
			{ "DeepGate_R", { 31.0f, 4.0f, 72.0f }, { 19.0f, 4.0f, 1.0f } },
			{ "DeepRoom_L", { -32.0f, 4.0f, 99.0f }, { 1.0f, 4.0f, 26.0f } },
			{ "DeepRoom_R", { 32.0f, 4.0f, 99.0f }, { 1.0f, 4.0f, 26.0f } },
			{ "DeepBack_L", { -22.0f, 4.0f, 119.0f }, { 10.0f, 4.0f, 1.0f } },
			{ "DeepBack_R", { 22.0f, 4.0f, 119.0f }, { 10.0f, 4.0f, 1.0f } }
		}};
		for (const BoxSpec& wall : walls) AddMineBox(levelData, wall.name, wall.position, wall.scale, "Obstacle");

		const std::array<Vector3, 24> pillarPositions = {{
			{ -8.0f, 2.8f, -44.0f }, { 8.0f, 2.8f, -44.0f },
			{ -8.0f, 2.8f, -24.0f }, { 8.0f, 2.8f, -24.0f },
			{ -40.0f, 3.2f, -18.0f }, { -28.0f, 3.2f, -18.0f },
			{ -40.0f, 3.2f, 0.0f }, { -28.0f, 3.2f, 0.0f },
			{ -8.0f, 2.8f, 4.0f }, { 8.0f, 2.8f, 4.0f },
			{ -8.0f, 3.0f, 28.0f }, { 8.0f, 3.0f, 28.0f },
			{ 28.0f, 3.8f, 36.0f }, { 40.0f, 3.8f, 36.0f },
			{ 28.0f, 3.8f, 56.0f }, { 40.0f, 3.8f, 56.0f },
			{ -8.0f, 3.0f, 58.0f }, { 8.0f, 3.0f, 58.0f },
			{ -24.0f, 4.0f, 82.0f }, { 24.0f, 4.0f, 82.0f },
			{ -24.0f, 4.0f, 104.0f }, { 24.0f, 4.0f, 104.0f },
			{ -8.0f, 4.0f, 112.0f }, { 8.0f, 4.0f, 112.0f }
		}};
		for (size_t index = 0; index < pillarPositions.size(); ++index)
		{
			AddMineBox(levelData, "Pillar_" + std::to_string(index + 1), pillarPositions[index], { 1.2f, 2.8f, 1.2f }, "Pillar");
		}

		const std::array<BoxSpec, 14> covers = {{
			{ "Cover_01", { -6.0f, 1.1f, -46.0f }, { 2.2f, 1.1f, 1.2f } },
			{ "Cover_02", { 7.0f, 1.1f, -40.0f }, { 1.5f, 1.1f, 1.8f } },
			{ "Cover_03", { -6.0f, 1.1f, -14.0f }, { 2.0f, 1.1f, 1.0f } },
			{ "Cover_04", { -41.0f, 2.3f, -8.0f }, { 2.5f, 1.1f, 1.2f } },
			{ "Cover_05", { -26.0f, 2.3f, -2.0f }, { 1.8f, 1.1f, 1.8f } },
			{ "Cover_06", { 6.0f, 1.1f, 18.0f }, { 1.3f, 1.1f, 2.0f } },
			{ "Cover_07", { 36.0f, 3.1f, 38.0f }, { 2.3f, 1.1f, 1.1f } },
			{ "Cover_08", { 27.0f, 3.1f, 49.0f }, { 1.7f, 1.1f, 1.7f } },
			{ "Cover_09", { 42.0f, 3.1f, 55.0f }, { 2.0f, 1.1f, 1.0f } },
			{ "Cover_10", { -5.0f, 2.5f, 50.0f }, { 2.0f, 1.1f, 1.0f } },
			{ "Cover_11", { 6.0f, 2.5f, 68.0f }, { 1.2f, 1.1f, 2.2f } },
			{ "Cover_12", { -18.0f, 3.1f, 84.0f }, { 2.3f, 1.1f, 1.2f } },
			{ "Cover_13", { 18.0f, 3.1f, 94.0f }, { 1.8f, 1.1f, 1.8f } },
			{ "Cover_14", { -12.0f, 3.1f, 110.0f }, { 2.0f, 1.1f, 1.0f } }
		}};
		for (const BoxSpec& cover : covers) AddMineBox(levelData, cover.name, cover.position, cover.scale, "Obstacle");

		const std::array<float, 9> beamZ = { -44.0f, -24.0f, -4.0f, 16.0f, 36.0f, 56.0f, 78.0f, 98.0f, 116.0f };
		for (size_t index = 0; index < beamZ.size(); ++index)
		{
			const float halfWidth = beamZ[index] < 72.0f ? 14.0f : 30.0f;
			AddMineBox(levelData, "Beam_" + std::to_string(index + 1), { 0.0f, 6.2f, beamZ[index] }, { halfWidth, 0.35f, 0.7f }, "Obstacle");
		}

		const std::array<float, 10> stripZ = { -46.0f, -28.0f, -10.0f, 8.0f, 26.0f, 44.0f, 62.0f, 80.0f, 98.0f, 116.0f };
		for (size_t index = 0; index < stripZ.size(); ++index)
		{
			AddMineBox(levelData, "Strip_Main_" + std::to_string(index + 1), { 0.0f, 0.04f, stripZ[index] }, { 0.65f, 0.03f, 8.0f });
		}

		const std::array<BoxSpec, 14> raisedFloors = {{
			{ "RaisedFloor_Entry_L", { -22.0f, 0.18f, -43.0f }, { 9.0f, 0.18f, 5.0f } },
			{ "RaisedFloor_Entry_R", { 23.0f, 0.25f, -37.0f }, { 10.0f, 0.25f, 4.0f } },
			{ "RaisedFloor_West_Main", { -34.0f, 0.60f, -9.0f }, { 16.0f, 0.60f, 15.0f } },
			{ "RaisedFloor_West_Back", { -41.0f, 0.60f, 13.0f }, { 9.0f, 0.60f, 6.0f } },
			{ "RaisedFloor_Central_Left", { -27.0f, 0.70f, 25.0f }, { 12.0f, 0.70f, 14.0f } },
			{ "RaisedFloor_Central_Right", { 27.0f, 0.70f, 18.0f }, { 12.0f, 0.70f, 12.0f } },
			{ "RaisedFloor_East_Main", { 34.0f, 1.00f, 46.0f }, { 16.0f, 1.00f, 15.0f } },
			{ "RaisedFloor_East_Back", { 42.0f, 1.00f, 68.0f }, { 8.0f, 1.00f, 6.0f } },
			{ "RaisedFloor_Upper_West", { -40.0f, 1.60f, 54.0f }, { 8.0f, 1.60f, 12.0f } },
			{ "RaisedFloor_Upper_East", { 40.0f, 1.60f, 82.0f }, { 8.0f, 1.60f, 10.0f } },
			{ "RaisedFloor_Deep_Entry", { 0.0f, 0.75f, 82.0f }, { 12.0f, 0.75f, 6.0f } },
			{ "RaisedFloor_Boss_Main", { 0.0f, 1.00f, 103.0f }, { 30.0f, 1.00f, 17.0f } },
			{ "RaisedFloor_Boss_Left", { -23.0f, 1.00f, 113.0f }, { 7.0f, 1.00f, 6.0f } },
			{ "RaisedFloor_Boss_Right", { 23.0f, 1.00f, 113.0f }, { 7.0f, 1.00f, 6.0f } }
		}};
		for (const BoxSpec& floor : raisedFloors) AddMineBox(levelData, floor.name, floor.position, floor.scale, "Floor");

		AddMineStairs(levelData, "Step_West_", { -17.0f, 0.0f, -9.0f }, { -2.2f, 0.0f, 0.0f }, { 1.4f, 0.0f, 4.0f }, 4, 0.30f);
		AddMineStairs(levelData, "Step_East_", { 17.0f, 0.0f, 46.0f }, { 1.9f, 0.0f, 0.0f }, { 1.2f, 0.0f, 4.0f }, 8, 0.25f);
		AddMineStairs(levelData, "Step_UpperWest_", { -31.0f, 0.0f, 48.0f }, { -1.6f, 0.0f, 1.2f }, { 1.2f, 0.0f, 2.0f }, 8, 0.40f);
		AddMineStairs(levelData, "Step_Deep_", { 0.0f, 0.0f, 72.5f }, { 0.0f, 0.0f, 2.5f }, { 11.0f, 0.0f, 1.25f }, 8, 0.25f);

		struct RubbleSpec
		{
			Vector3 position;
			Vector3 scale;
			float yaw;
		};
		const std::array<RubbleSpec, 34> rubble = {{
			{ { -35.0f, 0.45f, -45.0f }, { 2.1f, 0.45f, 1.2f }, 0.32f },
			{ { -29.0f, 0.30f, -39.0f }, { 1.2f, 0.30f, 1.7f }, -0.48f },
			{ { 35.0f, 0.55f, -43.0f }, { 2.5f, 0.55f, 1.0f }, 0.72f },
			{ { 43.0f, 0.35f, -31.0f }, { 1.4f, 0.35f, 1.9f }, -0.25f },
			{ { -45.0f, 1.80f, -18.0f }, { 2.2f, 0.60f, 1.5f }, 0.55f },
			{ { -23.0f, 1.58f, -13.0f }, { 1.3f, 0.38f, 2.0f }, -0.62f },
			{ { -39.0f, 1.48f, 4.0f }, { 1.8f, 0.28f, 1.2f }, 0.18f },
			{ { -19.0f, 1.88f, 12.0f }, { 2.4f, 0.48f, 1.1f }, 0.88f },
			{ { 21.0f, 1.82f, 4.0f }, { 1.5f, 0.42f, 2.1f }, -0.37f },
			{ { 42.0f, 1.98f, 17.0f }, { 2.6f, 0.58f, 1.4f }, 0.41f },
			{ { -10.0f, 0.34f, 20.0f }, { 1.7f, 0.34f, 1.3f }, -0.76f },
			{ { 10.0f, 0.52f, 34.0f }, { 2.2f, 0.52f, 1.2f }, 0.29f },
			{ { 24.0f, 1.70f, 37.0f }, { 1.2f, 0.30f, 1.8f }, 0.61f },
			{ { 45.0f, 2.46f, 48.0f }, { 2.0f, 0.46f, 1.1f }, -0.53f },
			{ { 24.0f, 2.62f, 58.0f }, { 2.5f, 0.62f, 1.5f }, 0.22f },
			{ { -10.0f, 0.40f, 54.0f }, { 1.5f, 0.40f, 2.0f }, -0.44f },
			{ { 9.0f, 0.30f, 67.0f }, { 1.8f, 0.30f, 1.1f }, 0.70f },
			{ { -43.0f, 3.75f, 59.0f }, { 2.4f, 0.55f, 1.3f }, -0.18f },
			{ { 43.0f, 3.56f, 81.0f }, { 1.4f, 0.36f, 2.0f }, 0.49f },
			{ { -27.0f, 2.44f, 83.0f }, { 2.0f, 0.44f, 1.1f }, -0.66f },
			{ { 27.0f, 2.52f, 87.0f }, { 2.3f, 0.52f, 1.4f }, 0.35f },
			{ { -24.0f, 2.32f, 96.0f }, { 1.3f, 0.32f, 1.9f }, -0.28f },
			{ { 24.0f, 2.60f, 101.0f }, { 2.5f, 0.60f, 1.2f }, 0.74f },
			{ { -27.0f, 2.42f, 113.0f }, { 1.8f, 0.42f, 1.6f }, -0.57f },
			{ { 27.0f, 2.36f, 115.0f }, { 1.5f, 0.36f, 2.0f }, 0.24f },
			{ { -6.0f, 2.25f, 120.0f }, { 1.2f, 0.25f, 1.5f }, 0.62f },
			{ { 7.0f, 2.48f, 118.0f }, { 2.0f, 0.48f, 1.1f }, -0.31f },
			{ { 0.0f, 2.32f, 105.0f }, { 1.1f, 0.32f, 1.1f }, 0.45f },
			{ { -9.0f, 0.55f, 8.0f }, { 2.6f, 0.55f, 0.9f }, 0.25f },
			{ { 9.0f, 0.40f, 24.0f }, { 2.0f, 0.40f, 1.1f }, -0.35f },
			{ { -7.0f, 0.65f, 42.0f }, { 2.5f, 0.65f, 1.2f }, 0.70f },
			{ { 8.0f, 0.50f, 58.0f }, { 2.2f, 0.50f, 1.0f }, -0.52f },
			{ { -13.0f, 1.90f, 92.0f }, { 2.4f, 0.50f, 1.4f }, 0.40f },
			{ { 14.0f, 2.45f, 108.0f }, { 2.0f, 0.45f, 1.5f }, -0.46f }
		}};
		for (size_t index = 0; index < rubble.size(); ++index)
		{
			AddMineBox(levelData, "Rubble_" + std::to_string(index + 1), rubble[index].position, rubble[index].scale, "Obstacle", { 0.0f, rubble[index].yaw, 0.0f });
		}

		const std::array<RubbleSpec, 10> fallenBeams = {{
			{ { -37.0f, 1.0f, -31.0f }, { 7.0f, 0.45f, 0.55f }, 0.42f },
			{ { 35.0f, 1.2f, -12.0f }, { 8.0f, 0.50f, 0.55f }, -0.58f },
			{ { -34.0f, 2.1f, 20.0f }, { 6.0f, 0.45f, 0.55f }, -0.35f },
			{ { 36.0f, 3.1f, 24.0f }, { 7.0f, 0.50f, 0.55f }, 0.55f },
			{ { -38.0f, 3.8f, 48.0f }, { 6.5f, 0.45f, 0.55f }, 0.31f },
			{ { 37.0f, 3.2f, 68.0f }, { 8.0f, 0.50f, 0.55f }, -0.46f },
			{ { -23.0f, 2.8f, 91.0f }, { 6.0f, 0.45f, 0.55f }, 0.64f },
			{ { 24.0f, 3.0f, 108.0f }, { 6.5f, 0.45f, 0.55f }, -0.39f },
			{ { -2.0f, 1.2f, 30.0f }, { 8.0f, 0.45f, 0.55f }, 0.12f },
			{ { 3.0f, 2.9f, 99.0f }, { 9.0f, 0.50f, 0.55f }, -0.18f }
		}};
		for (size_t index = 0; index < fallenBeams.size(); ++index)
		{
			AddMineBox(levelData, "BrokenBeam_" + std::to_string(index + 1), fallenBeams[index].position, fallenBeams[index].scale, "Obstacle", { 0.0f, fallenBeams[index].yaw, 0.0f });
		}

		const std::array<Vector3, 20> rockDetails = {{
			{ -48.5f, 5.5f, -40.0f }, { 48.5f, 4.8f, -26.0f }, { -48.0f, 6.0f, -8.0f },
			{ 48.0f, 5.2f, 7.0f }, { -48.5f, 4.5f, 22.0f }, { 48.5f, 6.2f, 34.0f },
			{ -48.0f, 5.0f, 45.0f }, { 48.0f, 4.6f, 56.0f }, { -48.5f, 6.0f, 70.0f },
			{ 48.5f, 5.4f, 82.0f }, { -30.0f, 6.3f, 91.0f }, { 30.0f, 5.8f, 99.0f },
			{ -30.0f, 4.7f, 111.0f }, { 30.0f, 6.0f, 116.0f }, { -12.0f, 7.1f, 76.0f },
			{ 12.0f, 6.7f, 86.0f }, { -12.0f, 6.5f, 103.0f }, { 12.0f, 6.9f, 113.0f },
			{ -22.0f, 7.0f, 38.0f }, { 23.0f, 7.2f, 64.0f }
		}};
		for (size_t index = 0; index < rockDetails.size(); ++index)
		{
			const float scale = 1.2f + static_cast<float>(index % 3) * 0.45f;
			AddMineBox(levelData, "RockDetail_" + std::to_string(index + 1), rockDetails[index], { scale, 1.1f + static_cast<float>(index % 2) * 0.6f, scale * 0.8f }, "Obstacle", { 0.18f * static_cast<float>(index % 2), 0.37f * static_cast<float>(index), 0.12f });
		}
		// 地上・作業床・上層通路・最奥採掘場を段階的につなぎ、坑道らしい縦方向の探索を作る。
	}
}

namespace Ken4lowEngine
{
	void Stage::Initialize(const std::string& levelJsonPath, const std::string& defaultModelName, bool instancedOnly)
	{
		Clear();
		levelData_ = LevelLoader::LoadLevel(levelJsonPath);
		if (!levelData_) return;
		if (IsExpandedMineStage(levelJsonPath)) BuildExpandedMineLayout(*levelData_);

		const bool effectiveInstancedOnly = instancedOnly || defaultModelName.empty();
		const std::string primaryStageModelPath = effectiveInstancedOnly
			? std::string{}
			: StageAssetLoader::ResolveStageModelName(*levelData_, defaultModelName);
		if (!effectiveInstancedOnly)
		{
			stageModel_ = StageAssetLoader::BuildStageModel(*levelData_, defaultModelName, offset_);
			if (stageModel_)
			{
				stageModel_->Update();
				RebuildStageChunks();
			}
		}
		stageInstancingManager_.Build(*levelData_, primaryStageModelPath, offset_);

		StageCollisionBuildResult collisionResult = StageCollisionBuilder::Build(*levelData_, offset_);
		worldAABBs_ = std::move(collisionResult.worldAABBs);
		floorAABBs_ = std::move(collisionResult.floorAABBs);
		wallObstacleAABBs_ = std::move(collisionResult.wallObstacleAABBs);
		navigationObstacleAABBs_ = std::move(collisionResult.navigationObstacleAABBs);
		worldColliders_ = std::move(collisionResult.worldColliders);
		wallObstacleOBBs_ = std::move(collisionResult.wallObstacleOBBs);
		wallObstacleWalkable_ = std::move(collisionResult.wallObstacleWalkable);
		navigationObstacleOBBs_ = std::move(collisionResult.navigationObstacleOBBs);
		ladderColliders_ = std::move(collisionResult.ladderColliders);
		ladderAABBs_ = std::move(collisionResult.ladderAABBs);
		ladderOBBs_ = std::move(collisionResult.ladderOBBs);
		occlusionCullingSystem_.BuildAutoOccludersFromWorldAABBs(worldAABBs_);
	}

	void Stage::Clear()
	{
		levelData_.reset();
		stageModel_.reset();
		stageInstancingManager_.Clear();
		stageChunkManager_.Clear();
		occlusionCullingSystem_.ClearOccluders();
		worldAABBs_.clear();
		floorAABBs_.clear();
		wallObstacleAABBs_.clear();
		navigationObstacleAABBs_.clear();
		worldColliders_.clear();
		wallObstacleOBBs_.clear();
		wallObstacleWalkable_.clear();
		navigationObstacleOBBs_.clear();
		ladderColliders_.clear();
		ladderAABBs_.clear();
		ladderOBBs_.clear();
	}

	void Stage::Update()
	{
		if (stageModel_) stageModel_->Update();
	}

	void Stage::Draw()
	{
		if (useNormalStageDraw_ && stageModel_ && stageChunkManager_.NeedsRebuild()) RebuildStageChunks();
		if (useNormalStageDraw_ && stageModel_)
		{
			if (stageChunkManager_.IsEnabled() && !stageChunkManager_.GetChunks().empty())
			{
				stageChunkManager_.UpdateVisibility(true);
				stageChunkManager_.ApplyOcclusionCulling(
					occlusionCullingSystem_,
					Object3DCommon::GetInstance()->GetFrustumCullingSystem().GetViewProjectionMatrix());
				stageChunkManager_.DrawVisibleChunks();
			}
			else
			{
				stageChunkManager_.UpdateVisibility(false);
				stageModel_->Draw();
			}
		}
		if (useNormalStageDraw_)
		{
			stageInstancingManager_.DrawUniqueObjects();
			if (!stageInstancingEnabled_ || !useInstancedStageDraw_) stageInstancingManager_.DrawBatchSourcesNormally();
		}
		if (stageInstancingEnabled_ && useInstancedStageDraw_) stageInstancingManager_.DrawInstancedBatches();
	}

	void Stage::DrawChunkDebug()
	{
		stageChunkManager_.SetShowOccludedBounds(occlusionCullingSystem_.IsShowOccludedBounds());
		stageChunkManager_.DrawDebugBounds();
		occlusionCullingSystem_.DrawDebugBounds();
	}

	void Stage::DrawShadow()
	{
		if (useNormalStageDraw_ && stageModel_) stageModel_->DrawShadow();
		stageInstancingManager_.DrawShadow(stageInstancingEnabled_, useInstancedStageDraw_, useNormalStageDraw_);
	}

	void Stage::UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
	{
		if (stageModel_) stageModel_->UpdateShadowMatrix(lightViewProjection);
		stageInstancingManager_.UpdateShadowMatrix(lightViewProjection);
	}

	void Stage::SetFrustumCullingEnabled(bool enabled)
	{
		if (stageModel_) stageModel_->SetFrustumCullingEnabled(enabled);
	}

	void Stage::SetStageChunkCullingEnabled(bool enabled) { stageChunkManager_.SetEnabled(enabled); }
	bool Stage::IsStageChunkCullingEnabled() const { return stageChunkManager_.IsEnabled(); }
	void Stage::SetStageChunkBoundsVisible(bool visible) { stageChunkManager_.SetShowBounds(visible); }
	bool Stage::IsStageChunkBoundsVisible() const { return stageChunkManager_.IsShowBounds(); }
	void Stage::SetStageChunkObjectBoundsVisible(bool visible) { stageChunkManager_.SetShowObjectBounds(visible); }
	bool Stage::IsStageChunkObjectBoundsVisible() const { return stageChunkManager_.IsShowObjectBounds(); }
	void Stage::SetStageChunkAutoExcludeLargeObjects(bool enabled) { stageChunkManager_.SetAutoExcludeLargeObjects(enabled); }
	bool Stage::IsStageChunkAutoExcludeLargeObjects() const { return stageChunkManager_.IsAutoExcludeLargeObjects(); }

	void Stage::SetStageChunkSize(float chunkSize)
	{
		stageChunkManager_.SetChunkSize(chunkSize);
		stageChunkManager_.MarkRebuildRequested();
	}

	float Stage::GetStageChunkSize() const { return stageChunkManager_.GetChunkSize(); }

	void Stage::RebuildStageChunks()
	{
		if (!stageModel_)
		{
			stageChunkManager_.Clear();
			return;
		}
		stageChunkManager_.Rebuild(stageModel_.get(), stageChunkManager_.GetChunkSize());
	}

	void Stage::RegisterColliders(CollisionManager* collisionManager)
	{
		(void)collisionManager;
		// Static ColliderはPlayer Runtimeが周辺集合だけをPhysicsとLegacyへ同期する。
	}

	bool Stage::CheckLadderOverlap(const AABB& playerAABB) const
	{
		for (const AABB& ladderAABB : ladderAABBs_)
		{
			if (CollisionUtility::IsCollision(playerAABB, ladderAABB)) return true;
		}
		return false;
	}

	std::vector<Collider*> Stage::GetWorldColliderPointers() const
	{
		std::vector<Collider*> colliders{};
		colliders.reserve(worldColliders_.size());
		for (const std::unique_ptr<Collider>& collider : worldColliders_)
		{
			if (collider) colliders.push_back(collider.get());
		}
		return colliders;
	}
} // namespace Ken4lowEngine
