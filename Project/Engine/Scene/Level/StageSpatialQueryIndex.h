#pragma once

#include "AABB.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <unordered_map>
#include <vector>

namespace Ken4lowEngine
{
	/// Static Stage AABBをXZ Uniform Gridへ保持し、読み取り専用の近傍候補検索を提供する。
	class StageSpatialQueryIndex final
	{
	public:
		static constexpr float kDefaultCellSize = 16.0f;

		struct Stats final
		{
			float cellSize = kDefaultCellSize;
			std::size_t indexedItemCount = 0;
			std::size_t usedCellCount = 0;
			std::size_t totalCellReferences = 0;
			std::size_t largestCellOccupancy = 0;
		};

		explicit StageSpatialQueryIndex(float cellSize = kDefaultCellSize)
			: cellSize_(cellSize > 0.0f ? cellSize : kDefaultCellSize)
		{
			stats_.cellSize = cellSize_;
		}

		void Build(const std::vector<AABB>& bounds)
		{
			Clear();
			for (std::size_t index = 0; index < bounds.size(); ++index)
			{
				CellCoord minCell{};
				CellCoord maxCell{};
				if (!TryGetCellRange(bounds[index], minCell, maxCell)) continue;

				++stats_.indexedItemCount;
				for (int z = minCell.z; z <= maxCell.z; ++z)
				{
					for (int x = minCell.x; x <= maxCell.x; ++x)
					{
						auto& entries = cells_[{ x, z }];
						entries.push_back(index);
						++stats_.totalCellReferences;
						stats_.largestCellOccupancy = (std::max)(stats_.largestCellOccupancy, entries.size());
					}
				}
			}
			stats_.usedCellCount = cells_.size();
		}

		void Clear()
		{
			cells_.clear();
			stats_ = {};
			stats_.cellSize = cellSize_;
		}

		/// Query結果は元配列indexの昇順へ正規化するため、複数セルを跨いでも重複せず決定的になる。
		void Query(const AABB& queryBounds, std::vector<std::size_t>& outIndices) const
		{
			outIndices.clear();
			CellCoord minCell{};
			CellCoord maxCell{};
			if (!TryGetCellRange(queryBounds, minCell, maxCell)) return;

			for (int z = minCell.z; z <= maxCell.z; ++z)
			{
				for (int x = minCell.x; x <= maxCell.x; ++x)
				{
					const auto found = cells_.find({ x, z });
					if (found == cells_.end()) continue;
					outIndices.insert(outIndices.end(), found->second.begin(), found->second.end());
				}
			}

			std::sort(outIndices.begin(), outIndices.end());
			outIndices.erase(std::unique(outIndices.begin(), outIndices.end()), outIndices.end());
		}

		[[nodiscard]] const Stats& GetStats() const { return stats_; }
		[[nodiscard]] float GetCellSize() const { return cellSize_; }
		[[nodiscard]] bool IsEmpty() const { return cells_.empty(); }

	private:
		struct CellCoord final
		{
			int x = 0;
			int z = 0;

			bool operator==(const CellCoord& other) const
			{
				return x == other.x && z == other.z;
			}
		};

		struct CellCoordHash final
		{
			std::size_t operator()(const CellCoord& cell) const
			{
				const std::size_t xHash = std::hash<int>{}(cell.x);
				const std::size_t zHash = std::hash<int>{}(cell.z);
				return xHash ^ (zHash + 0x9e3779b9u + (xHash << 6) + (xHash >> 2));
			}
		};

		bool TryToCell(float coordinate, int& outCell) const
		{
			if (!std::isfinite(coordinate)) return false;
			const double cell = std::floor(static_cast<double>(coordinate) / static_cast<double>(cellSize_));
			if (cell < static_cast<double>((std::numeric_limits<int>::min)()) ||
				cell > static_cast<double>((std::numeric_limits<int>::max)()))
			{
				return false;
			}
			outCell = static_cast<int>(cell);
			return true;
		}

		bool TryGetCellRange(const AABB& bounds, CellCoord& outMinCell, CellCoord& outMaxCell) const
		{
			const float minX = (std::min)(bounds.min.x, bounds.max.x);
			const float maxX = (std::max)(bounds.min.x, bounds.max.x);
			const float minZ = (std::min)(bounds.min.z, bounds.max.z);
			const float maxZ = (std::max)(bounds.min.z, bounds.max.z);
			return TryToCell(minX, outMinCell.x) &&
				TryToCell(minZ, outMinCell.z) &&
				TryToCell(maxX, outMaxCell.x) &&
				TryToCell(maxZ, outMaxCell.z);
		}

		float cellSize_ = kDefaultCellSize;
		std::unordered_map<CellCoord, std::vector<std::size_t>, CellCoordHash> cells_;
		Stats stats_{};
	};
} // namespace Ken4lowEngine
