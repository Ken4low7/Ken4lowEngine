#pragma once

#include "LevelData.h"

#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// ローマ式コロッセウムの外観と制御塔の機械設備を組み合わせた、Stage 5専用の円形Boss Arena。
	class CentralControlColosseumLayout final
	{
	public:
		static std::vector<ObjectData> Build(const LevelData& levelData)
		{
			if (!IsTargetLevel(levelData)) return {};

			std::vector<ObjectData> objects;
			objects.reserve(430u);

			BuildArenaFloor(objects);
			BuildOuterWall(objects);
			BuildSpectatorTiers(objects);
			BuildColumnArcades(objects);
			BuildControlRing(objects);
			BuildImperialGate(objects);
			return objects; // 床・外壁・観客席・列柱・制御装置を同一Cubeモデルへ集約し、1つのInstance Batchで描画する。
		}

	private:
		static constexpr float kPi = 3.14159265358979323846f;

		static bool IsTargetLevel(const LevelData& levelData)
		{
			for (const ObjectData& data : levelData.objects)
			{
				if (data.name == "CentralControlColosseum_InstancedOnlyMarker") return true;
			}
			return false;
		}

		static Vector3 PolarPosition(float radius, float angle, float y)
		{
			return { std::sin(angle) * radius, y, std::cos(angle) * radius };
		}

		static void BuildArenaFloor(std::vector<ObjectData>& objects)
		{
			AddFloorColliderOnly(objects, "ControlArenaSafetyFloor", { 0.0f, -0.70f, 0.0f }, { 45.0f, 0.65f, 45.0f }); // 描画床の継ぎ目を踏んでもBossとPlayerが奈落へ落ちない安全床を一段下へ敷く。
			AddBox(objects, "ControlArenaFloor_Center", { 0.0f, -0.45f, 0.0f }, { 29.5f, 0.45f, 29.5f }, "Floor");

			constexpr int kFloorSegments = 32;
			for (int index = 0; index < kFloorSegments; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kFloorSegments) * kPi * 2.0f;
				AddBox(
					objects,
					"ControlArenaFloor_Outer_" + std::to_string(index + 1),
					PolarPosition(35.0f, angle, -0.45f),
					{ 4.8f, 0.45f, 10.5f },
					"Floor",
					{ 0.0f, angle, 0.0f });
			}

			AddBox(objects, "ControlArenaDais_Center", { 0.0f, 0.18f, 0.0f }, { 8.5f, 0.18f, 8.5f }, "Floor");
			constexpr int kDaisSegments = 20;
			for (int index = 0; index < kDaisSegments; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kDaisSegments) * kPi * 2.0f;
				AddBox(
					objects,
					"ControlGlow_DaisRing_" + std::to_string(index + 1),
					PolarPosition(10.0f, angle, 0.08f),
					{ 1.75f, 0.04f, 0.30f },
					nullptr,
					{ 0.0f, angle, 0.0f });
			}

			constexpr int kArenaGuideSegments = 40;
			for (int index = 0; index < kArenaGuideSegments; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kArenaGuideSegments) * kPi * 2.0f;
				AddBox(
					objects,
					"ControlGlow_ArenaRing_" + std::to_string(index + 1),
					PolarPosition(43.2f, angle, 0.06f),
					{ 1.9f, 0.035f, 0.18f },
					nullptr,
					{ 0.0f, angle, 0.0f });
			}
		}

		static void BuildOuterWall(std::vector<ObjectData>& objects)
		{
			constexpr int kWallSegments = 56;
			for (int index = 0; index < kWallSegments; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kWallSegments) * kPi * 2.0f;
				AddBox(
					objects,
					"ColosseumOuterWall_" + std::to_string(index + 1),
					PolarPosition(66.0f, angle, 7.0f),
					{ 4.4f, 7.0f, 1.55f },
					"Obstacle",
					{ 0.0f, angle, 0.0f });
			}

			constexpr int kCrownSegments = 56;
			for (int index = 0; index < kCrownSegments; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kCrownSegments) * kPi * 2.0f;
				AddBox(
					objects,
					"ColosseumCrownRing_" + std::to_string(index + 1),
					PolarPosition(66.0f, angle, 14.8f),
					{ 4.5f, 0.48f, 1.85f },
					nullptr,
					{ 0.0f, angle, 0.0f });
			}
		}

		static void BuildSpectatorTiers(std::vector<ObjectData>& objects)
		{
			struct TierSpec
			{
				float radius;
				float y;
				float halfHeight;
				float radialHalf;
			};
			const std::array<TierSpec, 3> tiers = {{
				{ 50.0f, 1.45f, 1.45f, 3.4f },
				{ 56.0f, 3.15f, 3.15f, 3.4f },
				{ 62.0f, 5.10f, 5.10f, 3.4f }
			}};

			constexpr int kTierSegments = 40;
			for (size_t tierIndex = 0; tierIndex < tiers.size(); ++tierIndex)
			{
				for (int index = 0; index < kTierSegments; ++index)
				{
					const float angle = static_cast<float>(index) / static_cast<float>(kTierSegments) * kPi * 2.0f;
					AddBox(
						objects,
						"ColosseumSeatTier_" + std::to_string(tierIndex + 1) + "_" + std::to_string(index + 1),
						PolarPosition(tiers[tierIndex].radius, angle, tiers[tierIndex].y),
						{ 4.8f, tiers[tierIndex].halfHeight, tiers[tierIndex].radialHalf },
						"Obstacle",
						{ 0.0f, angle, 0.0f });
				}
			}
		}

		static void BuildColumnArcades(std::vector<ObjectData>& objects)
		{
			constexpr int kColumnCount = 28;
			for (int index = 0; index < kColumnCount; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kColumnCount) * kPi * 2.0f;
				const Vector3 columnPosition = PolarPosition(61.8f, angle, 11.6f);
				AddBox(
					objects,
					"ColosseumColumn_" + std::to_string(index + 1),
					columnPosition,
					{ 0.90f, 6.3f, 0.90f },
					"Pillar");
				AddBox(
					objects,
					"ColosseumCapital_" + std::to_string(index + 1),
					columnPosition + Vector3{ 0.0f, 6.78f, 0.0f },
					{ 1.45f, 0.38f, 1.45f });
				AddBox(
					objects,
					"ColosseumArcLintel_" + std::to_string(index + 1),
					PolarPosition(61.8f, angle + kPi / static_cast<float>(kColumnCount), 18.4f),
					{ 7.0f, 0.42f, 0.72f },
					nullptr,
					{ 0.0f, angle, 0.0f });
			}
		}

		static void BuildControlRing(std::vector<ObjectData>& objects)
		{
			constexpr int kControlPylonCount = 8;
			for (int index = 0; index < kControlPylonCount; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kControlPylonCount) * kPi * 2.0f;
				const Vector3 pylonPosition = PolarPosition(46.5f, angle, 4.1f);
				AddBox(
					objects,
					"ControlPylon_" + std::to_string(index + 1),
					pylonPosition,
					{ 1.35f, 4.1f, 1.35f },
					"Pillar",
					{ 0.0f, angle, 0.0f });
				AddBox(
					objects,
					"ControlGlow_PylonCore_" + std::to_string(index + 1),
					pylonPosition + Vector3{ 0.0f, 0.45f, 0.0f },
					{ 0.42f, 2.9f, 0.42f });
				AddBox(
					objects,
					"ControlPanel_PylonHead_" + std::to_string(index + 1),
					pylonPosition + Vector3{ 0.0f, 4.65f, 0.0f },
					{ 1.85f, 0.32f, 1.85f });
			}

			constexpr int kUpperRingSegments = 40;
			for (int index = 0; index < kUpperRingSegments; ++index)
			{
				const float angle = static_cast<float>(index) / static_cast<float>(kUpperRingSegments) * kPi * 2.0f;
				AddBox(
					objects,
					"ControlUpperRing_" + std::to_string(index + 1),
					PolarPosition(46.7f, angle, 9.0f),
					{ 4.3f, 0.24f, 0.38f },
					nullptr,
					{ 0.0f, angle, 0.0f });
			}
		}

		static void BuildImperialGate(std::vector<ObjectData>& objects)
		{
			AddBox(objects, "ColosseumImperialGate_Left", { -11.0f, 8.0f, 61.5f }, { 2.7f, 8.0f, 2.4f }, "Obstacle");
			AddBox(objects, "ColosseumImperialGate_Right", { 11.0f, 8.0f, 61.5f }, { 2.7f, 8.0f, 2.4f }, "Obstacle");
			AddBox(objects, "ColosseumImperialGate_Lintel", { 0.0f, 16.0f, 61.5f }, { 13.7f, 1.3f, 2.4f }, "Obstacle");
			AddBox(objects, "ControlGlow_ImperialGate", { 0.0f, 12.3f, 58.9f }, { 7.5f, 0.32f, 0.18f });

			AddBox(objects, "ColosseumEntryGate_Left", { -9.0f, 5.2f, -61.5f }, { 1.7f, 5.2f, 2.3f }, "Obstacle");
			AddBox(objects, "ColosseumEntryGate_Right", { 9.0f, 5.2f, -61.5f }, { 1.7f, 5.2f, 2.3f }, "Obstacle");
			AddBox(objects, "ColosseumEntryGate_Lintel", { 0.0f, 10.7f, -61.5f }, { 10.7f, 0.8f, 2.3f }, "Obstacle");
			AddBox(objects, "ControlGlow_EntryGuide", { 0.0f, 0.06f, -44.0f }, { 2.6f, 0.035f, 9.0f });
		}

		static void AddFloorColliderOnly(
			std::vector<ObjectData>& objects,
			std::string name,
			const Vector3& position,
			const Vector3& scale)
		{
			ObjectData data{};
			data.name = std::move(name);
			data.type = "EMPTY";
			data.position = position;
			data.scale = scale;
			data.collider.enabled = true;
			data.collider.type = "BOX";
			data.collider.size = { 2.0f, 2.0f, 2.0f };
			data.collider.collisionType = "Floor";
			objects.push_back(std::move(data));
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
