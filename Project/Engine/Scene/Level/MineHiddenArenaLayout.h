#pragma once

#include "LevelData.h"
#include "Vector3.h"

#include <cmath>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// Stage 2の生成済み配置にだけ隠し通路とドーム広間を追加する共通レイアウト。
	class MineHiddenArenaLayout final
	{
	public:
		static bool IsTargetMine(const LevelData& levelData)
		{
			for (const ObjectData& data : levelData.objects)
			{
				if (data.name == "Ceiling_Blocker" || data.name == "RaisedFloor_Boss_Main") return true;
			}
			return false;
		}

		static std::vector<ObjectData> Build(const LevelData& levelData)
		{
			std::vector<ObjectData> objects;
			if (!IsTargetMine(levelData)) return objects;
			objects.reserve(128);

			AddBox(objects, "MineOuterSeal_West", { -51.0f, 8.5f, 35.0f }, { 1.0f, 0.5f, 91.0f }, "Obstacle");
			AddBox(objects, "MineOuterSeal_East", { 51.0f, 8.5f, 35.0f }, { 1.0f, 0.5f, 91.0f }, "Obstacle");
			AddBox(objects, "MineOuterSeal_South", { 0.0f, 8.5f, -56.0f }, { 51.0f, 0.5f, 1.0f }, "Obstacle");
			AddBox(objects, "MineOuterNorthWall_L", { -32.0f, 4.5f, 126.0f }, { 19.0f, 4.5f, 1.0f }, "Obstacle");
			AddBox(objects, "MineOuterNorthWall_R", { 32.0f, 4.5f, 126.0f }, { 19.0f, 4.5f, 1.0f }, "Obstacle"); // 中央開口以外を岩壁で閉じ、封鎖壁の左右から奥を見せない。

			AddBox(objects, "HiddenPassageFloor", { 0.0f, 1.0f, 137.0f }, { 12.0f, 1.0f, 18.0f }, "Floor");
			AddBox(objects, "HiddenPassageWall_L", { -13.0f, 7.0f, 137.0f }, { 1.0f, 5.0f, 18.0f }, "Obstacle");
			AddBox(objects, "HiddenPassageWall_R", { 13.0f, 7.0f, 137.0f }, { 1.0f, 5.0f, 18.0f }, "Obstacle");
			AddBox(objects, "HiddenPassageCeiling", { 0.0f, 12.5f, 137.0f }, { 12.0f, 0.5f, 18.0f }, "Obstacle");
			AddBox(objects, "HiddenGateLintel", { 0.0f, 10.5f, 119.0f }, { 13.0f, 1.5f, 1.4f }, "Obstacle");

			for (int index = 0; index < 5; ++index)
			{
				const float z = 123.0f + static_cast<float>(index) * 7.0f;
				AddBox(objects, "HiddenSupport_L_" + std::to_string(index + 1), { -11.5f, 7.0f, z }, { 0.65f, 5.0f, 0.65f }, "Obstacle");
				AddBox(objects, "HiddenSupport_R_" + std::to_string(index + 1), { 11.5f, 7.0f, z }, { 0.65f, 5.0f, 0.65f }, "Obstacle");
				AddBox(objects, "HiddenSupportTop_" + std::to_string(index + 1), { 0.0f, 11.3f, z }, { 12.0f, 0.65f, 0.65f }, "Obstacle");
			}

			for (int index = 0; index < 4; ++index)
			{
				const float z = 124.0f + static_cast<float>(index) * 9.0f;
				AddBox(objects, "HiddenPassageGuide_" + std::to_string(index + 1), { 0.0f, 2.04f, z }, { 0.35f, 0.04f, 3.6f });
			}

			AddBox(objects, "DomeArenaFloor", { 0.0f, 1.0f, 180.0f }, { 50.0f, 1.0f, 48.0f }, "Floor");
			AddBox(objects, "DomeArenaCeiling", { 0.0f, 17.0f, 180.0f }, { 50.0f, 0.8f, 48.0f }, "Obstacle");
			AddBox(objects, "DomeEntryWall_L", { -31.0f, 9.0f, 156.0f }, { 19.0f, 7.0f, 1.5f }, "Obstacle", { 0.0f, -0.25f, 0.0f });
			AddBox(objects, "DomeEntryWall_R", { 31.0f, 9.0f, 156.0f }, { 19.0f, 7.0f, 1.5f }, "Obstacle", { 0.0f, 0.25f, 0.0f });
			AddBox(objects, "DomeEntryCeiling", { 0.0f, 16.2f, 155.0f }, { 31.0f, 0.8f, 5.5f }, "Obstacle");

			constexpr int wallSegmentCount = 16;
			constexpr float arenaCenterZ = 180.0f;
			constexpr float wallRadius = 46.0f;
			for (int index = 0; index < wallSegmentCount; ++index)
			{
				if (index >= 11 && index <= 13) continue; // 南側3区画だけを隠し通路の入口として開ける。
				const float angle = std::numbers::pi_v<float> * 2.0f * static_cast<float>(index) / static_cast<float>(wallSegmentCount);
				const Vector3 position{ std::cos(angle) * wallRadius, 9.0f, arenaCenterZ + std::sin(angle) * wallRadius };
				AddBox(objects, "DomeWall_" + std::to_string(index + 1), position, { 10.0f, 7.0f, 1.6f }, "Obstacle", { 0.0f, -angle - std::numbers::pi_v<float> * 0.5f, 0.0f });
			}

			for (int index = 0; index < 12; ++index)
			{
				const float angle = std::numbers::pi_v<float> * static_cast<float>(index) / 12.0f;
				AddBox(objects, "DomeRib_" + std::to_string(index + 1), { 0.0f, 15.6f, arenaCenterZ }, { 0.48f, 0.48f, 45.0f }, nullptr, { 0.0f, angle, 0.0f });
			}

			for (int index = 0; index < wallSegmentCount; ++index)
			{
				const float angle = std::numbers::pi_v<float> * 2.0f * static_cast<float>(index) / static_cast<float>(wallSegmentCount);
				const Vector3 position{ std::cos(angle) * 30.0f, 13.5f, arenaCenterZ + std::sin(angle) * 30.0f };
				AddBox(objects, "DomeRing_" + std::to_string(index + 1), position, { 6.2f, 0.45f, 0.7f }, nullptr, { 0.0f, -angle - std::numbers::pi_v<float> * 0.5f, 0.0f });
			}

			const std::vector<Vector3> arenaRocks = {
				{ -42.0f, 3.0f, 161.0f }, { 42.0f, 2.7f, 162.0f }, { -42.0f, 3.4f, 199.0f },
				{ 42.0f, 3.1f, 201.0f }, { -24.0f, 2.8f, 220.0f }, { 24.0f, 3.3f, 221.0f }
			};
			for (size_t index = 0; index < arenaRocks.size(); ++index)
			{
				AddBox(objects, "DomeRockSeal_" + std::to_string(index + 1), arenaRocks[index], { 4.0f + static_cast<float>(index % 2), 2.3f, 3.0f }, "Obstacle", { 0.12f, 0.48f * static_cast<float>(index), 0.08f });
			}

			return objects; // 直径約92mの広間でも描画とCollisionを同じ配置から生成し、外周の隙間を残さない。
		}

	private:
		static void AddBox(std::vector<ObjectData>& objects, std::string name, const Vector3& position, const Vector3& scale, const char* collisionType = nullptr, const Vector3& rotation = {})
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
			objects.push_back(std::move(data));
		}
	};
} // namespace Ken4lowEngine
