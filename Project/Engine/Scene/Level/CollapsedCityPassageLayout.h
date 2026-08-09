#pragma once

#include "LevelData.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// 中央橋の迂回封鎖と、崩落高架アスレチックの入口・出口を追加する補助レイアウト。
	class CollapsedCityPassageLayout final
	{
	public:
		static std::vector<ObjectData> Build(const LevelData& levelData)
		{
			if (!IsTargetLevel(levelData)) return {};

			std::vector<ObjectData> objects;
			objects.reserve(28u);
			AddBox(objects, "CityBypassPermanentCollapse_Main", { -16.0f, 2.6f, -51.0f }, { 9.5f, 2.6f, 1.6f }, "Obstacle", { 0.04f, 0.08f, -0.03f });
			AddBox(objects, "Rubble_BypassPermanentCollapse_Left", { -25.0f, 1.55f, -46.5f }, { 3.6f, 1.55f, 2.4f }, "Obstacle", { -0.08f, -0.30f, 0.06f });
			AddBox(objects, "BrokenBeam_BypassPermanentCollapse_Top", { -14.0f, 5.7f, -50.5f }, { 7.5f, 0.35f, 0.45f }, nullptr, { 0.18f, 0.12f, 0.22f });

			const std::array<float, 4> entryTopHeights = { 7.0f, 8.0f, 9.0f, 10.0f };
			for (size_t index = 0; index < entryTopHeights.size(); ++index)
			{
				const float top = entryTopHeights[index];
				AddBox(
					objects,
					"Step_AthleticEntry_" + std::to_string(index + 1),
					{ 0.0f, top * 0.5f, 98.8f + static_cast<float>(index) * 1.7f },
					{ 10.8f, top * 0.5f, 1.05f },
					"Floor");
			}

			const std::array<float, 5> exitTopHeights = { 10.0f, 8.0f, 6.0f, 4.0f, 2.0f };
			for (size_t index = 0; index < exitTopHeights.size(); ++index)
			{
				const float top = exitTopHeights[index];
				AddBox(
					objects,
					"Step_AthleticExit_" + std::to_string(index + 1),
					{ 0.0f, top * 0.5f, 152.0f + static_cast<float>(index) * 2.8f },
					{ 10.8f, top * 0.5f, 1.65f },
					"Floor");
			}

			AddBox(objects, "BrokenBeam_AthleticEntry_Left", { -10.2f, 10.8f, 103.0f }, { 0.25f, 0.65f, 5.0f }, "Obstacle", { 0.06f, 0.02f, -0.08f });
			AddBox(objects, "BrokenBeam_AthleticEntry_Right", { 10.2f, 10.8f, 103.0f }, { 0.25f, 0.65f, 5.0f }, "Obstacle", { -0.05f, -0.02f, 0.07f });
			AddBox(objects, "BrokenBeam_AthleticExit_Left", { -10.2f, 10.8f, 149.0f }, { 0.25f, 0.65f, 4.0f }, "Obstacle", { -0.08f, 0.03f, 0.06f });
			AddBox(objects, "BrokenBeam_AthleticExit_Right", { 10.2f, 10.8f, 149.0f }, { 0.25f, 0.65f, 4.0f }, "Obstacle", { 0.07f, -0.03f, -0.05f });

			for (size_t index = 0; index < 3; ++index)
			{
				AddBox(
					objects,
					"Strip_AthleticWarning_" + std::to_string(index + 1),
					{ 0.0f, 10.08f, 101.0f + static_cast<float>(index) * 2.0f },
					{ 1.2f, 0.04f, 0.18f });
			}

			AddBox(objects, "Strip_EscapeGoalPad", { 0.0f, 0.08f, 181.0f }, { 5.0f, 0.05f, 5.0f });
			AddBox(objects, "Strip_EscapeBeaconColumn", { 0.0f, 12.0f, 181.0f }, { 0.34f, 12.0f, 0.34f });
			AddBox(objects, "Strip_EscapeBeaconArmX", { 0.0f, 6.0f, 181.0f }, { 4.6f, 0.16f, 0.28f });
			AddBox(objects, "Strip_EscapeBeaconArmZ", { 0.0f, 6.0f, 181.0f }, { 0.28f, 0.16f, 4.6f });
			const std::array<Vector3, 4> goalPosts = {{
				{ -4.3f, 1.8f, 176.7f }, { 4.3f, 1.8f, 176.7f },
				{ -4.3f, 1.8f, 185.3f }, { 4.3f, 1.8f, 185.3f }
			}};
			for (size_t index = 0; index < goalPosts.size(); ++index)
			{
				AddBox(objects, "Strip_EscapeGoalPost_" + std::to_string(index + 1), goalPosts[index], { 0.22f, 1.8f, 0.22f });
			}
			return objects; // 青い大型ビーコンと距離表示を一致させ、終端へ入れば必ずクリア判定へ届くようにする。
		}

	private:
		static bool IsTargetLevel(const LevelData& levelData)
		{
			for (const ObjectData& data : levelData.objects)
			{
				if (data.name == "CollapsedCity_InstancedOnlyMarker") return true;
			}
			return false;
		}

		static void AddBox(
			std::vector<ObjectData>& objects,
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
			data.scale = scale;
			data.rotation = rotation;
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
