#include <WorldPartitionGrid.h>

#include <cassert>
#include <cmath>
#include <iostream>

using namespace Ken4lowEngine;

int main()
{
	// Negative world coordinates must floor into the previous cell instead of truncating toward zero.
	const WorldPartitionCell negativeCell{ -1, -2 };
	const WorldPartitionCell positiveCell{ 1, 2 };
	const WorldPartitionCell sanitizedCell{ 5, 5 };
	const WorldPartitionCell originCell{ 0, 0 };
	assert(WorldPartitionGrid::WorldToCell(-0.1f, -10.1f, 10.0f) == negativeCell);
	assert(WorldPartitionGrid::WorldToCell(19.9f, 20.0f, 10.0f) == positiveCell);
	assert(WorldPartitionGrid::WorldToCell(5.0f, 5.0f, 0.0f) == sanitizedCell);
	assert(WorldPartitionGrid::WorldToCell(std::nanf(""), std::nanf(""), 32.0f) == originCell);

	const WorldPartitionCell source{ 0, 0 };
	const WorldPartitionCell distanceTarget{ 3, -2 };
	const WorldPartitionCell farTarget{ 99, 99 };
	const WorldPartitionCell loadTarget{ 1, 1 };
	const WorldPartitionCell retainTarget{ 2, 0 };
	const WorldPartitionCell unloadTarget{ 3, 0 };
	assert(WorldPartitionGrid::ChebyshevDistance(source, distanceTarget) == 3);
	assert(WorldPartitionGrid::Evaluate(source, farTarget, true, 1, 2) == WorldPartitionStreamingDecision::AlwaysLoaded);
	assert(WorldPartitionGrid::Evaluate(source, loadTarget, false, 1, 2) == WorldPartitionStreamingDecision::Load);
	assert(WorldPartitionGrid::Evaluate(source, retainTarget, false, 1, 2) == WorldPartitionStreamingDecision::Retain);
	assert(WorldPartitionGrid::Evaluate(source, unloadTarget, false, 1, 2) == WorldPartitionStreamingDecision::Unload);
	assert(WorldPartitionGrid::Evaluate(source, retainTarget, false, -5, 1) == WorldPartitionStreamingDecision::Unload);

	std::cout << "World Partition Grid runtime tests passed\n";
	return 0;
}
