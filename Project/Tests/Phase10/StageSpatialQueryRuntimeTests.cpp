#include "StageSpatialQueryIndex.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

using namespace Ken4lowEngine;

namespace
{
	AABB MakeBounds(float minX, float minZ, float maxX, float maxZ)
	{
		return { { minX, -1.0f, minZ }, { maxX, 1.0f, maxZ } };
	}

	void TestCandidatePruningAndDeterministicDeduplication()
	{
		StageSpatialQueryIndex index(10.0f);
		const std::vector<AABB> bounds = {
			MakeBounds(0.0f, 0.0f, 4.0f, 4.0f),
			MakeBounds(20.0f, 0.0f, 24.0f, 4.0f),
			MakeBounds(-24.0f, 0.0f, -20.0f, 4.0f),
			MakeBounds(8.0f, -2.0f, 22.0f, 2.0f),
		};
		index.Build(bounds);

		std::vector<std::size_t> candidates;
		index.Query(MakeBounds(9.0f, -1.0f, 11.0f, 1.0f), candidates);
		assert(!candidates.empty());
		assert(candidates.size() < bounds.size());
		assert(std::is_sorted(candidates.begin(), candidates.end()));
		assert(std::adjacent_find(candidates.begin(), candidates.end()) == candidates.end());
		assert(std::find(candidates.begin(), candidates.end(), 3u) != candidates.end());
		assert(std::find(candidates.begin(), candidates.end(), 2u) == candidates.end());

		index.Query(MakeBounds(100.0f, 100.0f, 101.0f, 101.0f), candidates);
		assert(candidates.empty());
	}

	void TestStatsAndInvalidBounds()
	{
		StageSpatialQueryIndex index(8.0f);
		std::vector<AABB> bounds = {
			MakeBounds(-2.0f, -2.0f, 2.0f, 2.0f),
			MakeBounds(7.0f, 7.0f, 17.0f, 17.0f),
		};
		AABB invalid = MakeBounds(0.0f, 0.0f, 1.0f, 1.0f);
		invalid.min.x = std::nanf("");
		bounds.push_back(invalid);
		index.Build(bounds);

		const StageSpatialQueryIndex::Stats& stats = index.GetStats();
		assert(stats.cellSize == 8.0f);
		assert(stats.indexedItemCount == 2u);
		assert(stats.usedCellCount > 0u);
		assert(stats.totalCellReferences >= stats.indexedItemCount);
		assert(stats.largestCellOccupancy > 0u);
	}

	void TestConcurrentReadOnlyQueries()
	{
		StageSpatialQueryIndex index(4.0f);
		std::vector<AABB> bounds;
		for (int z = 0; z < 16; ++z)
		{
			for (int x = 0; x < 16; ++x)
			{
				bounds.push_back(MakeBounds(
					static_cast<float>(x * 8),
					static_cast<float>(z * 8),
					static_cast<float>(x * 8 + 2),
					static_cast<float>(z * 8 + 2)));
			}
		}
		index.Build(bounds);

		std::vector<std::thread> workers;
		std::vector<std::size_t> resultSizes(8, 0);
		for (std::size_t workerIndex = 0; workerIndex < resultSizes.size(); ++workerIndex)
		{
			workers.emplace_back([&index, &resultSizes, workerIndex]()
				{
					std::vector<std::size_t> candidates;
					for (int iteration = 0; iteration < 200; ++iteration)
					{
						index.Query(MakeBounds(30.0f, 30.0f, 42.0f, 42.0f), candidates);
						assert(std::is_sorted(candidates.begin(), candidates.end()));
						if (iteration == 0) resultSizes[workerIndex] = candidates.size();
						else assert(resultSizes[workerIndex] == candidates.size());
					}
				});
		}
		for (std::thread& worker : workers) worker.join();
		for (std::size_t size : resultSizes) assert(size == resultSizes.front()); // Queryは内部scratchを共有せず並列readで同じ候補を返す。
	}
}

int main()
{
	TestCandidatePruningAndDeterministicDeduplication();
	TestStatsAndInvalidBounds();
	TestConcurrentReadOnlyQueries();
	std::cout << "Stage Spatial Query runtime tests passed\n";
	return 0;
}
