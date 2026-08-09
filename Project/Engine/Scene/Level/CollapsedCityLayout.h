#pragma once

#include "LevelData.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// 崩落都市圏を単一Cubeモデルのインスタンス群として構築するレイアウト定義。
	class CollapsedCityLayout final
	{
	public:
		static bool IsTargetLevel(const LevelData& levelData)
		{
			for (const ObjectData& data : levelData.objects)
			{
				if (data.name == "CollapsedCity_InstancedOnlyMarker") return true;
			}
			return false;
		}

		static std::vector<ObjectData> Build(const LevelData& levelData)
		{
			if (!IsTargetLevel(levelData)) return {};

			std::vector<ObjectData> objects;
			objects.reserve(180u);

			BuildSouthAvenue(objects);
			BuildSinkholeBypass(objects);
			BuildCivicPlaza(objects);
			BuildCollapsedFreeway(objects);
			BuildEvacuationTerminal(objects);
			BuildBoundaryRuins(objects);
			return objects;
		}

	private:
		static void AddBox(
			std::vector<ObjectData>& objects,
			std::string name,
			const Vector3& position,
			const Vector3& scale,
			const char* collisionType = nullptr,
			const Vector3& rotation = {},
			const Vector4& color = { 1.0f, 1.0f, 1.0f, 1.0f })
		{
			ObjectData data{};
			data.name = std::move(name);
			data.type = "StaticMesh";
			data.modelName = "Sample/cube.gltf";
			data.position = position;
			data.scale = scale;
			data.rotation = rotation;
			data.color = color;
			if (collisionType)
			{
				data.collider.enabled = true;
				data.collider.type = "BOX";
				data.collider.size = { 2.0f, 2.0f, 2.0f };
				data.collider.collisionType = collisionType;
			}
			objects.push_back(std::move(data));
		}

		static void AddBuilding(
			std::vector<ObjectData>& objects,
			const std::string& prefix,
			float x,
			float z,
			float halfWidth,
			float halfDepth,
			float height,
			float lean,
			float side)
		{
			AddBox(
				objects,
				prefix + "_Base",
				{ x, height * 0.42f, z },
				{ halfWidth, height * 0.42f, halfDepth },
				"Obstacle",
				{},
				{ 0.28f, 0.30f, 0.34f, 1.0f });
			AddBox(
				objects,
				prefix + "_LeaningUpper",
				{ x + side * lean * 2.0f, height * 0.88f, z },
				{ halfWidth * 0.88f, height * 0.30f, halfDepth * 0.88f },
				nullptr,
				{ side * lean * 0.18f, lean * 0.45f, side * lean * 0.10f },
				{ 0.16f, 0.17f, 0.20f, 1.0f });
		}

		static void BuildSouthAvenue(std::vector<ObjectData>& objects)
		{
			const std::array<float, 4> roadZ = { -120.0f, -98.0f, -76.0f, -56.0f };
			for (size_t index = 0; index < roadZ.size(); ++index)
			{
				const float z = roadZ[index];
				AddBox(objects, "CityRoad_South_" + std::to_string(index + 1), { 0.0f, -0.45f, z }, { 22.0f, 0.45f, 10.0f }, "Floor");
				AddBox(objects, "RaisedFloor_SidewalkL_" + std::to_string(index + 1), { -28.0f, 0.15f, z }, { 5.5f, 0.15f, 10.0f }, "Floor");
				AddBox(objects, "RaisedFloor_SidewalkR_" + std::to_string(index + 1), { 28.0f, 0.15f, z }, { 5.5f, 0.15f, 10.0f }, "Floor");
				const float side = index % 2 == 0 ? -1.0f : 1.0f;
				AddBox(objects, "BrokenBeam_RoadCrack_" + std::to_string(index + 1), { side * 7.0f, 0.04f, z }, { 4.0f, 0.03f, 0.12f }, nullptr, { 0.0f, side * 0.20f, 0.0f });
			}

			AddBuilding(objects, "CityBuilding_SW1", -44.0f, -110.0f, 10.0f, 10.0f, 24.0f, 0.16f, 1.0f);
			AddBuilding(objects, "CityBuilding_SE1", 44.0f, -109.0f, 11.0f, 9.0f, 28.0f, -0.14f, -1.0f);
			AddBuilding(objects, "CityBuilding_SW2", -45.0f, -78.0f, 11.0f, 9.0f, 31.0f, -0.12f, -1.0f);
			AddBuilding(objects, "CityBuilding_SE2", 45.0f, -77.0f, 9.0f, 11.0f, 23.0f, 0.18f, 1.0f);
			AddBuilding(objects, "CityBuilding_SW3", -43.0f, -53.0f, 10.0f, 10.0f, 25.0f, 0.15f, 1.0f);
			AddBuilding(objects, "CityBuilding_SE3", 46.0f, -52.0f, 12.0f, 9.0f, 33.0f, -0.10f, -1.0f);

			const std::array<Vector3, 4> rubblePositions = {{
				{ -11.0f, 0.70f, -109.0f }, { 12.0f, 0.70f, -88.0f },
				{ -13.0f, 0.70f, -69.0f }, { 10.0f, 0.70f, -57.0f }
			}};
			for (size_t index = 0; index < rubblePositions.size(); ++index)
			{
				const float yaw = index % 2 == 0 ? 0.40f : -0.35f;
				AddBox(objects, "Rubble_Boulevard_" + std::to_string(index + 1), rubblePositions[index], { 2.3f, 0.70f, 1.4f }, "Obstacle", { 0.0f, yaw, 0.0f });
			}
			for (size_t index = 0; index < 3; ++index)
			{
				AddBox(objects, "DefenseHazard_Median_" + std::to_string(index + 1), { 0.0f, 0.50f, -108.0f + static_cast<float>(index) * 21.5f }, { 0.50f, 0.50f, 3.0f }, "Obstacle");
			}
		}

		static void BuildSinkholeBypass(std::vector<ObjectData>& objects)
		{
			struct RimSpec { Vector3 position; Vector3 scale; };
			const std::array<RimSpec, 6> rim = {{
				{ { -18.0f, 3.0f, -47.0f }, { 14.0f, 3.0f, 2.0f } },
				{ { 18.0f, 3.0f, -47.0f }, { 14.0f, 3.0f, 2.0f } },
				{ { -31.0f, 3.0f, -29.0f }, { 2.0f, 3.0f, 16.0f } },
				{ { 31.0f, 3.0f, -29.0f }, { 2.0f, 3.0f, 16.0f } },
				{ { -18.0f, 3.0f, -11.0f }, { 14.0f, 3.0f, 2.0f } },
				{ { 18.0f, 3.0f, -11.0f }, { 14.0f, 3.0f, 2.0f } }
			}};
			for (size_t index = 0; index < rim.size(); ++index)
			{
				AddBox(objects, "CitySinkholeWall_" + std::to_string(index + 1), rim[index].position, rim[index].scale, "Obstacle");
			}
			AddBox(objects, "CitySinkholeDepth", { 0.0f, -7.0f, -29.0f }, { 27.0f, 0.8f, 16.0f }, nullptr, {}, { 0.055f, 0.060f, 0.070f, 1.0f });

			struct BypassSpec { Vector3 position; Vector3 scale; };
			const std::array<BypassSpec, 5> bypass = {{
				{ { -14.0f, 0.45f, -50.0f }, { 8.0f, 0.45f, 5.0f } },
				{ { -24.0f, 0.90f, -42.0f }, { 7.0f, 0.90f, 5.0f } },
				{ { -28.0f, 1.35f, -30.0f }, { 6.0f, 1.35f, 6.0f } },
				{ { -26.0f, 1.00f, -18.0f }, { 6.0f, 1.00f, 5.0f } },
				{ { -16.0f, 0.60f, -9.0f }, { 8.0f, 0.60f, 5.0f } }
			}};
			for (size_t index = 0; index < bypass.size(); ++index)
			{
				AddBox(objects, "RaisedFloor_Bypass_" + std::to_string(index + 1), bypass[index].position, bypass[index].scale, "Floor");
			}

			const std::array<Vector3, 6> stepPositions = {{
				{ -7.0f, 0.15f, -54.0f }, { -11.0f, 0.30f, -52.0f }, { -15.0f, 0.45f, -50.0f },
				{ -12.0f, 0.40f, -6.0f }, { -8.0f, 0.25f, -4.0f }, { -4.0f, 0.125f, -2.0f }
			}};
			const std::array<float, 6> stepHeights = { 0.30f, 0.60f, 0.90f, 0.80f, 0.50f, 0.25f };
			for (size_t index = 0; index < stepPositions.size(); ++index)
			{
				AddBox(objects, "Step_Bypass_" + std::to_string(index + 1), stepPositions[index], { 2.3f, stepHeights[index] * 0.5f, 2.0f }, "Floor");
			}

			AddBox(objects, "CityBusWreck", { -28.0f, 2.2f, -29.0f }, { 3.2f, 1.6f, 6.0f }, "Obstacle", { 0.08f, 0.15f, 0.0f });
			const std::array<Vector3, 6> rubble = {{
				{ -18.0f, 0.75f, -38.0f }, { -8.0f, 0.75f, -25.0f }, { 15.0f, 0.75f, -17.0f },
				{ 21.0f, 0.75f, -35.0f }, { 7.0f, 0.75f, -43.0f }, { -24.0f, 0.75f, -12.0f }
			}};
			for (size_t index = 0; index < rubble.size(); ++index)
			{
				AddBox(objects, "Rubble_Sinkhole_" + std::to_string(index + 1), rubble[index], { 2.0f, 0.75f, 1.5f }, "Obstacle", { 0.0f, 0.25f * static_cast<float>(index), 0.0f });
			}
		}

		static void BuildCivicPlaza(std::vector<ObjectData>& objects)
		{
			AddBox(objects, "CityRoad_PlazaSouth", { 0.0f, -0.45f, 4.0f }, { 24.0f, 0.45f, 8.0f }, "Floor");
			AddBox(objects, "CityRoad_PlazaMain", { 0.0f, -0.45f, 24.0f }, { 30.0f, 0.45f, 12.0f }, "Floor");
			AddBox(objects, "CityRoad_PlazaNorth", { 0.0f, -0.45f, 47.0f }, { 24.0f, 0.45f, 10.0f }, "Floor");
			AddBox(objects, "RaisedFloor_PlazaLeft", { -29.0f, 0.35f, 26.0f }, { 5.0f, 0.35f, 22.0f }, "Floor");
			AddBox(objects, "RaisedFloor_PlazaRight", { 29.0f, 0.35f, 26.0f }, { 5.0f, 0.35f, 22.0f }, "Floor");

			AddBox(objects, "CityMonument_Base", { 0.0f, 1.2f, 26.0f }, { 5.5f, 1.2f, 5.5f }, "Obstacle");
			AddBox(objects, "CityMonument_Pillar", { 0.0f, 5.5f, 26.0f }, { 1.3f, 4.3f, 1.3f }, "Pillar", { 0.12f, 0.16f, 0.0f });
			AddBox(objects, "BrokenBeam_MonumentTop", { 2.5f, 9.0f, 26.0f }, { 4.5f, 0.45f, 0.75f }, "Obstacle", { -0.22f, 0.45f, 0.0f });

			AddBuilding(objects, "CityBuilding_WP1", -45.0f, 12.0f, 11.0f, 10.0f, 27.0f, 0.12f, 1.0f);
			AddBuilding(objects, "CityBuilding_EP1", 45.0f, 13.0f, 11.0f, 10.0f, 24.0f, -0.16f, -1.0f);
			AddBuilding(objects, "CityBuilding_WP2", -46.0f, 44.0f, 12.0f, 11.0f, 36.0f, -0.13f, -1.0f);
			AddBuilding(objects, "CityBuilding_EP2", 46.0f, 45.0f, 10.0f, 12.0f, 31.0f, 0.15f, 1.0f);

			AddBox(objects, "CityFacadeWall_Left", { -15.0f, 5.0f, 53.0f }, { 7.0f, 5.0f, 1.2f }, "Obstacle");
			AddBox(objects, "CityFacadeWall_Right", { 15.0f, 5.0f, 53.0f }, { 7.0f, 5.0f, 1.2f }, "Obstacle");
			AddBox(objects, "BrokenBeam_FacadeArch", { 0.0f, 9.6f, 53.0f }, { 12.0f, 0.7f, 0.7f }, "Obstacle", { 0.0f, 0.0f, 0.10f });

			const std::array<Vector3, 6> coverPositions = {{
				{ -13.0f, 1.0f, 15.0f }, { 14.0f, 1.0f, 18.0f }, { -17.0f, 1.0f, 39.0f },
				{ 17.0f, 1.0f, 45.0f }, { -9.0f, 1.0f, 31.0f }, { 10.0f, 1.0f, 34.0f }
			}};
			for (size_t index = 0; index < coverPositions.size(); ++index)
			{
				const float yaw = index % 2 == 0 ? 0.30f : -0.40f;
				AddBox(objects, "CityCover_Plaza_" + std::to_string(index + 1), coverPositions[index], { 2.5f, 1.0f, 1.2f }, "Obstacle", { 0.0f, yaw, 0.0f });
			}
		}

		static void BuildCollapsedFreeway(std::vector<ObjectData>& objects)
		{
			AddBox(objects, "CityRoad_FreewayApproach", { 0.0f, -0.45f, 64.0f }, { 20.0f, 0.45f, 7.0f }, "Floor");

			AddBox(objects, "CityBusWreck_Checkpoint", { -8.5f, 1.65f, 64.0f }, { 5.2f, 1.65f, 2.25f }, "Obstacle", { 0.04f, 0.18f, -0.02f });
			AddBox(objects, "DefenseBarricade_Checkpoint_Right", { 10.5f, 0.65f, 65.5f }, { 4.0f, 0.65f, 0.65f }, "Obstacle", { 0.0f, -0.16f, 0.0f });
			AddBox(objects, "CityCheckpoint_Pillar", { 15.0f, 2.8f, 61.5f }, { 0.55f, 2.8f, 0.55f }, "Pillar", { 0.0f, 0.08f, 0.02f });
			AddBox(objects, "BrokenBeam_CheckpointArm", { 11.0f, 5.35f, 61.5f }, { 4.4f, 0.28f, 0.38f }, nullptr, { 0.05f, -0.08f, -0.14f });
			AddBox(objects, "Rubble_Checkpoint_Left", { -1.0f, 0.42f, 67.8f }, { 1.8f, 0.42f, 1.0f }, "Obstacle", { 0.0f, 0.42f, 0.0f });
			for (size_t index = 0; index < 3; ++index)
			{
				AddBox(objects, "Strip_CheckpointRoute_" + std::to_string(index + 1), { 4.0f, 0.05f, 61.0f + static_cast<float>(index) * 2.6f }, { 1.2f, 0.03f, 0.18f });
			}

			const std::array<float, 6> rampZ = { 72.0f, 77.0f, 82.0f, 87.0f, 92.0f, 97.0f };
			for (size_t index = 0; index < rampZ.size(); ++index)
			{
				const float top = static_cast<float>(index + 1);
				AddBox(objects, "Step_FreewayRamp_" + std::to_string(index + 1), { 0.0f, top * 0.5f, rampZ[index] }, { 11.0f, top * 0.5f, 2.8f }, "Floor");
			}

			const std::array<Vector3, 4> supports = {{
				{ -8.0f, 3.0f, 103.0f }, { 8.0f, 3.0f, 103.0f },
				{ -8.0f, 3.0f, 124.0f }, { 8.0f, 3.0f, 124.0f }
			}};
			for (size_t index = 0; index < supports.size(); ++index)
			{
				AddBox(objects, "CityFreeway_Pillar_" + std::to_string(index + 1), supports[index], { 1.2f, 3.0f, 1.2f }, "Pillar");
			}

			AddBox(objects, "CityFreeway_CollapsedLane_Left", { -17.0f, 7.4f, 117.0f }, { 8.0f, 0.55f, 3.5f }, nullptr, { -0.22f, -0.28f, 0.0f }, { 0.18f, 0.19f, 0.21f, 1.0f });
			AddBox(objects, "CityFreeway_CollapsedLane_Right", { 17.0f, 5.3f, 124.0f }, { 8.0f, 0.55f, 3.5f }, nullptr, { 0.30f, 0.25f, 0.0f }, { 0.18f, 0.19f, 0.21f, 1.0f });

			AddBuilding(objects, "CityBuilding_WF1", -43.0f, 82.0f, 12.0f, 10.0f, 33.0f, 0.12f, 1.0f);
			AddBuilding(objects, "CityBuilding_EF1", 43.0f, 83.0f, 11.0f, 12.0f, 37.0f, -0.10f, -1.0f);
			AddBuilding(objects, "CityBuilding_WF2", -45.0f, 119.0f, 10.0f, 12.0f, 25.0f, -0.18f, -1.0f);
			AddBuilding(objects, "CityBuilding_EF2", 46.0f, 121.0f, 12.0f, 11.0f, 29.0f, 0.16f, 1.0f);
			// 高架下の固定床と復帰階段は置かず、崩落足場から落ちた場合はそのまま落下失敗にする。
		}

		static void BuildEvacuationTerminal(std::vector<ObjectData>& objects)
		{
			for (size_t index = 0; index < 2; ++index)
			{
				const float z = index == 0 ? 160.0f : 176.0f;
				AddBox(objects, "CityRoad_Evac_" + std::to_string(index + 1), { 0.0f, -0.45f, z }, { 22.0f, 0.45f, 7.5f }, "Floor");
				AddBox(objects, "RaisedFloor_EvacL_" + std::to_string(index + 1), { -28.0f, 0.20f, z }, { 5.5f, 0.20f, 7.5f }, "Floor");
				AddBox(objects, "RaisedFloor_EvacR_" + std::to_string(index + 1), { 28.0f, 0.20f, z }, { 5.5f, 0.20f, 7.5f }, "Floor");
			}

			AddBox(objects, "CityEvacGate_PillarLeft", { -13.0f, 5.5f, 185.0f }, { 1.3f, 5.5f, 1.3f }, "Pillar");
			AddBox(objects, "CityEvacGate_PillarRight", { 13.0f, 5.5f, 185.0f }, { 1.3f, 5.5f, 1.3f }, "Pillar");
			AddBox(objects, "BrokenBeam_EvacGateTop", { 0.0f, 10.2f, 185.0f }, { 14.0f, 0.8f, 1.0f }, "Obstacle");
			AddBox(objects, "CityEvacTerminal_Back", { 0.0f, 4.0f, 195.0f }, { 26.0f, 4.0f, 2.0f }, "Obstacle");
			AddBox(objects, "CityEvac_BeaconPillar", { 0.0f, 6.5f, 190.0f }, { 1.0f, 6.5f, 1.0f }, "Pillar");

			for (size_t index = 0; index < 3; ++index)
			{
				AddBox(objects, "Strip_EvacBeacon_" + std::to_string(index + 1), { 0.0f, 5.0f + static_cast<float>(index) * 2.5f, 190.0f }, { 2.2f, 0.12f, 2.2f }, nullptr, { 0.0f, static_cast<float>(index) * 0.45f, 0.0f });
			}
			for (int index = -2; index <= 2; ++index)
			{
				AddBox(objects, "Strip_EvacPath_" + std::to_string(index + 3), { static_cast<float>(index) * 4.0f, 0.08f, 181.0f }, { 1.3f, 0.05f, 0.25f });
			}

			AddBuilding(objects, "CityBuilding_WEvac", -44.0f, 166.0f, 12.0f, 12.0f, 27.0f, 0.12f, 1.0f);
			AddBuilding(objects, "CityBuilding_EEvac", 44.0f, 169.0f, 11.0f, 11.0f, 23.0f, -0.15f, -1.0f);
		}

		static void BuildBoundaryRuins(std::vector<ObjectData>& objects)
		{
			AddBox(objects, "CityBoundaryWall_WestSouth", { -57.0f, 7.0f, -94.0f }, { 3.0f, 7.0f, 40.0f }, "Obstacle");
			AddBox(objects, "CityBoundaryWall_EastSouth", { 57.0f, 7.0f, -94.0f }, { 3.0f, 7.0f, 40.0f }, "Obstacle");
			AddBox(objects, "CityBoundaryWall_WestNorth", { -57.0f, 7.0f, 125.0f }, { 3.0f, 7.0f, 72.0f }, "Obstacle");
			AddBox(objects, "CityBoundaryWall_EastNorth", { 57.0f, 7.0f, 125.0f }, { 3.0f, 7.0f, 72.0f }, "Obstacle");
			// 一本道に見えないよう、道路・陥没迂回路・広場・高架・避難ゲートの五区画で景観を切り替える。
		}
	};
} // namespace Ken4lowEngine
