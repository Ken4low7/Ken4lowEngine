#include <WorldPartitionGrid.h>

#include <cassert>
#include <cmath>
#include <iostream>

using namespace Ken4lowEngine;

int main()
{
	// Negative world coordinates must floor into the previous cell instead of truncating toward zero.
	assert(WorldPartitionGrid::WorldToCell(-0.1f, -10.1f, 10.0f) == WorldPartitionCell{ -1, -2 });
	assert(WorldPartitionGrid::WorldToCell(19.9f, 20.0f, 10.0f) == WorldPartitionCell{ 1, 2 });
	assert(WorldPartitionGrid::WorldToCell(5.0f, 5.0f, 0.0f) == WorldPartitionCell{ 5, 5 });
	assert(WorldPartitionGrid::WorldToCell(std::nanf(""), std::nanf(""), 32.0f) == WorldPartitionCell{ 0, 0 });

	const WorldPartitionCell source{ 0, 0 };
	assert(WorldPartitionGrid::ChebyshevDistance(source, WorldPartitionCell{ 3, -2 }) == 3);
	assert(WorldPartitionGrid::Evaluate(source, WorldPartitionCell{ 99, 99 }, true, 1, 2) == WorldPartitionStreamingDecision::AlwaysLoaded);
	assert(WorldPartitionGrid::Evaluate(source, WorldPartitionCell{ 1, 1 }, false, 1, 2) == WorldPartitionStreamingDecision::Load);
	assert(WorldPartitionGrid::Evaluate(source, WorldPartitionCell{ 2, 0 }, false, 1, 2) == WorldPartitionStreamingDecision::Retain);
	assert(WorldPartitionGrid::Evaluate(source, WorldPartitionCell{ 3, 0 }, false, 1, 2) == WorldPartitionStreamingDecision::Unload);
	assert(WorldPartitionGrid::Evaluate(source, WorldPartitionCell{ 2, 0 }, false, -5, 1) == WorldPartitionStreamingDecision::Unload);

	std::cout << "World Partition Grid runtime tests passed\n";
	return 0;
}
