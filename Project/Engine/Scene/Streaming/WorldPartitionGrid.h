#pragma once

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{
	struct WorldPartitionCell final
	{
		int x = 0;
		int z = 0;

		[[nodiscard]] bool operator==(const WorldPartitionCell&) const = default;
	};

	enum class WorldPartitionStreamingDecision
	{
		AlwaysLoaded,
		Load,
		Retain,
		Unload,
	};

	/// RuntimeとEditorが同じCell identity・距離・Residency判定を共有するための純粋Grid計算。
	class WorldPartitionGrid final
	{
	public:
		[[nodiscard]] static float SanitizeCellSize(float cellSize)
		{
			if (!std::isfinite(cellSize)) return 1.0f;
			return (std::max)(1.0f, cellSize);
		}

		[[nodiscard]] static int SanitizeLoadRadius(int radius)
		{
			return (std::max)(0, radius);
		}

		[[nodiscard]] static int SanitizeUnloadRadius(int loadRadius, int unloadRadius)
		{
			return (std::max)(SanitizeLoadRadius(loadRadius), unloadRadius);
		}

		[[nodiscard]] static WorldPartitionCell WorldToCell(float worldX, float worldZ, float cellSize)
		{
			const float safeCellSize = SanitizeCellSize(cellSize);
			const float safeX = std::isfinite(worldX) ? worldX : 0.0f;
			const float safeZ = std::isfinite(worldZ) ? worldZ : 0.0f;
			return {
				static_cast<int>(std::floor(safeX / safeCellSize)),
				static_cast<int>(std::floor(safeZ / safeCellSize)),
			};
		}

		[[nodiscard]] static int ChebyshevDistance(const WorldPartitionCell& lhs, const WorldPartitionCell& rhs)
		{
			return (std::max)(std::abs(lhs.x - rhs.x), std::abs(lhs.z - rhs.z));
		}

		[[nodiscard]] static WorldPartitionStreamingDecision Evaluate(
			const WorldPartitionCell& source,
			const WorldPartitionCell& target,
			bool alwaysLoaded,
			int loadRadiusCells,
			int unloadRadiusCells)
		{
			if (alwaysLoaded) return WorldPartitionStreamingDecision::AlwaysLoaded;
			const int loadRadius = SanitizeLoadRadius(loadRadiusCells);
			const int unloadRadius = SanitizeUnloadRadius(loadRadius, unloadRadiusCells);
			const int distance = ChebyshevDistance(source, target);
			if (distance <= loadRadius) return WorldPartitionStreamingDecision::Load;
			if (distance > unloadRadius) return WorldPartitionStreamingDecision::Unload;
			return WorldPartitionStreamingDecision::Retain; // Hysteresis帯では現在のLoad状態を維持して境界の往復を防ぐ。
		}
	};
} // namespace Ken4lowEngine
