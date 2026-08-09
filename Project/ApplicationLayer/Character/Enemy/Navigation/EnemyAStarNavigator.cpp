#include "EnemyAStarNavigator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>

namespace
{
	struct CellKey
	{
		int x = 0;
		int z = 0;

		bool operator==(const CellKey& other) const
		{
			return x == other.x && z == other.z;
		}
	};

	struct CellKeyHash
	{
		size_t operator()(const CellKey& key) const
		{
			return (static_cast<size_t>(static_cast<uint32_t>(key.x)) << 32ull)
				^ static_cast<size_t>(static_cast<uint32_t>(key.z));
		}
	};

	float HeuristicCost(int x0, int z0, int x1, int z1)
	{
		const float dx = static_cast<float>(x1 - x0);
		const float dz = static_cast<float>(z1 - z0);
		return std::sqrt(dx * dx + dz * dz);
	}

	bool SegmentIntersectsAABBXZ(const K4E::Vector3& from, const K4E::Vector3& to, const K4E::AABB& aabb)
	{
		const K4E::Vector3 d = to - from;
		float tMin = 0.0f;
		float tMax = 1.0f;
		constexpr float kEps = 1.0e-6f;
		const auto updateAxis = [&](float p, float dp, float minV, float maxV, float& inOutTMin, float& inOutTMax) -> bool
			{
				if (std::abs(dp) <= kEps)
				{
					return p >= minV && p <= maxV;
				}
				const float inv = 1.0f / dp;
				float t0 = (minV - p) * inv;
				float t1 = (maxV - p) * inv;
				if (t0 > t1) std::swap(t0, t1);
				inOutTMin = std::max(inOutTMin, t0);
				inOutTMax = std::min(inOutTMax, t1);
				return inOutTMin <= inOutTMax;
			};
		if (!updateAxis(from.x, d.x, aabb.min.x, aabb.max.x, tMin, tMax)) return false;
		if (!updateAxis(from.z, d.z, aabb.min.z, aabb.max.z, tMin, tMax)) return false;
		return tMax >= 0.0f && tMin <= 1.0f;
	}

	std::uint32_t MixBits(std::uint32_t value)
	{
		value ^= value >> 16u;
		value *= 0x7feb352du;
		value ^= value >> 15u;
		value *= 0x846ca68bu;
		value ^= value >> 16u;
		return value;
	}

	float Hash01(std::uint32_t value)
	{
		return static_cast<float>(MixBits(value) & 0x00ffffffu) / static_cast<float>(0x01000000u);
	}
}

void EnemyAStarNavigator::SetWorldAABBs(const std::vector<K4E::AABB>* worldAABBs)
{
	if (worldAABBs_ == worldAABBs) return;
	worldAABBs_ = worldAABBs;
	obstacleCacheSource_ = nullptr;
	obstacleCacheSourceCount_ = 0;
	obstacleCacheAgentRadius_ = -1.0f;
	inflatedObstacleAABBs_.clear();
	InvalidateWalkabilityCache();
	Reset(); // Stage切替時に前ステージの経路と膨張障害物を持ち越さない。
}

void EnemyAStarNavigator::SetWalkableAABBs(const std::vector<K4E::AABB>* walkableAABBs)
{
	if (walkableAABBs_ == walkableAABBs) return;
	walkableAABBs_ = walkableAABBs;
	InvalidateWalkabilityCache();
	Reset(); // 歩行可能床が変わった場合は現在経路を次フレームで再評価する。
}

void EnemyAStarNavigator::SetSettings(const Settings& settings)
{
	Settings sanitized = settings;
	sanitized.cellSize = std::max(0.1f, sanitized.cellSize);
	sanitized.agentRadius = std::max(0.0f, sanitized.agentRadius);
	sanitized.agentHalfHeight = std::max(0.1f, sanitized.agentHalfHeight);
	sanitized.floorHeightTolerance = std::max(0.0f, sanitized.floorHeightTolerance);
	sanitized.searchRangeCells = std::max(1, sanitized.searchRangeCells);
	sanitized.maxExpandedNodes = std::max(16, sanitized.maxExpandedNodes);
	sanitized.waypointReachDistance = std::max(0.05f, sanitized.waypointReachDistance);
	sanitized.repathIntervalSec = std::max(0.01f, sanitized.repathIntervalSec);

	const bool obstacleCacheChanged = std::abs(settings_.agentRadius - sanitized.agentRadius) > 1.0e-5f;
	const bool pathGridChanged = std::abs(settings_.cellSize - sanitized.cellSize) > 1.0e-5f ||
		std::abs(settings_.agentHalfHeight - sanitized.agentHalfHeight) > 1.0e-5f ||
		std::abs(settings_.floorHeightTolerance - sanitized.floorHeightTolerance) > 1.0e-5f ||
		settings_.searchRangeCells != sanitized.searchRangeCells ||
		settings_.maxExpandedNodes != sanitized.maxExpandedNodes ||
		settings_.disableCornerCutting != sanitized.disableCornerCutting;
	settings_ = sanitized;

	if (obstacleCacheChanged)
	{
		obstacleCacheSource_ = nullptr;
		obstacleCacheAgentRadius_ = -1.0f;
		inflatedObstacleAABBs_.clear();
	}
	if (obstacleCacheChanged || pathGridChanged)
	{
		InvalidateWalkabilityCache();
		Reset();
	}
}

void EnemyAStarNavigator::Reset()
{
	path_.clear();
	currentPathIndex_ = 0;
	repathTimer_ = 0.0f;
	hasLastGoal_ = false;
}

void EnemyAStarNavigator::TickTemporaryBlocks(float deltaTime)
{
	for (auto& block : temporaryBlockedAreas_) block.remainingSec -= deltaTime;
	temporaryBlockedAreas_.erase(
		std::remove_if(temporaryBlockedAreas_.begin(), temporaryBlockedAreas_.end(),
			[](const TemporaryBlockedArea& block) { return block.remainingSec <= 0.0f; }),
		temporaryBlockedAreas_.end());
}

void EnemyAStarNavigator::AddTemporaryBlockedArea(const K4E::Vector3& center, float radius, float durationSec, const char* reason)
{
	TemporaryBlockedArea block{};
	block.center = center;
	block.radius = std::max(0.1f, radius);
	block.remainingSec = std::max(0.1f, durationSec);
	block.reason = reason ? reason : "Unknown";
	temporaryBlockedAreas_.push_back(block);
}

void EnemyAStarNavigator::ClearTemporaryBlockedAreas()
{
	temporaryBlockedAreas_.clear();
}

bool EnemyAStarNavigator::GetNextWaypoint(
	const K4E::Vector3& current,
	const K4E::Vector3& goal,
	float sampleY,
	float deltaTime,
	K4E::Vector3& outWaypoint)
{
	repathTimer_ -= std::max(0.0f, deltaTime);

	int goalX = 0;
	int goalZ = 0;
	WorldToCell(goal, goalX, goalZ);

	const bool goalChanged = !hasLastGoal_ || goalX != lastGoalX_ || goalZ != lastGoalZ_;
	const bool needRepath = path_.empty() || goalChanged;

	if (needRepath)
	{
		if (!RebuildPath(current, goal, sampleY))
		{
			outWaypoint = current;
			repathTimer_ = settings_.repathIntervalSec;
			return false;
		}

		lastGoalX_ = goalX;
		lastGoalZ_ = goalZ;
		hasLastGoal_ = true;
		repathTimer_ = settings_.repathIntervalSec;
	}

	if (path_.empty())
	{
		outWaypoint = current;
		return false;
	}

	while (currentPathIndex_ < static_cast<int>(path_.size()))
	{
		const K4E::Vector3 waypoint = path_[currentPathIndex_];
		const K4E::Vector3 delta = waypoint - current;
		const float distanceSq = delta.x * delta.x + delta.z * delta.z;

		if (distanceSq <= settings_.waypointReachDistance * settings_.waypointReachDistance)
		{
			++currentPathIndex_;
			continue;
		}

		outWaypoint = waypoint;
		return true;
	}

	outWaypoint = goal;
	return true;
}

bool EnemyAStarNavigator::RebuildPath(const K4E::Vector3& current, const K4E::Vector3& goal, float sampleY)
{
	int startX = 0;
	int startZ = 0;
	int goalX = 0;
	int goalZ = 0;
	WorldToCell(current, startX, startZ);
	WorldToCell(goal, goalX, goalZ);

	if (!IsWalkableCell(startX, startZ, sampleY))
	{
		int walkableX = startX;
		int walkableZ = startZ;
		if (FindNearestWalkableCell(startX, startZ, sampleY, 4, walkableX, walkableZ))
		{
			startX = walkableX;
			startZ = walkableZ;
		}
	}

	if (!IsWalkableCell(goalX, goalZ, sampleY))
	{
		int walkableX = goalX;
		int walkableZ = goalZ;
		if (FindNearestWalkableCell(goalX, goalZ, sampleY, 8, walkableX, walkableZ))
		{
			goalX = walkableX;
			goalZ = walkableZ;
		}
	}

	if (!IsWalkableCell(startX, startZ, sampleY) || !IsWalkableCell(goalX, goalZ, sampleY))
	{
		path_.clear();
		currentPathIndex_ = 0;
		return false;
	}

	const int minX = std::min(startX, goalX) - settings_.searchRangeCells;
	const int maxX = std::max(startX, goalX) + settings_.searchRangeCells;
	const int minZ = std::min(startZ, goalZ) - settings_.searchRangeCells;
	const int maxZ = std::max(startZ, goalZ) + settings_.searchRangeCells;

	std::unordered_map<CellKey, int, CellKeyHash> indexOf;
	std::vector<GridNode> nodes;
	nodes.reserve(1024);

	auto addNode = [&](int x, int z, float g, float h, int parent) -> int
		{
			GridNode node{};
			node.x = x;
			node.z = z;
			node.g = g;
			node.h = h;
			node.parentIndex = parent;
			node.opened = true;
			const int index = static_cast<int>(nodes.size());
			nodes.push_back(node);
			indexOf[{x, z}] = index;
			return index;
		};

	struct OpenEntry
	{
		float f = 0.0f;
		int index = -1;
		bool operator<(const OpenEntry& rhs) const { return f > rhs.f; }
	};

	std::priority_queue<OpenEntry> open;
	const int startIndex = addNode(startX, startZ, 0.0f, HeuristicCost(startX, startZ, goalX, goalZ), -1);
	open.push({ nodes[startIndex].g + nodes[startIndex].h, startIndex });

	static constexpr int kDx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	static constexpr int kDz[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
	static constexpr float kCost[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.4142135f, 1.4142135f, 1.4142135f, 1.4142135f };

	int expanded = 0;
	int goalIndex = -1;

	while (!open.empty() && expanded < settings_.maxExpandedNodes)
	{
		const OpenEntry top = open.top();
		open.pop();

		if (top.index < 0 || top.index >= static_cast<int>(nodes.size())) continue;
		GridNode& node = nodes[top.index];
		if (node.closed) continue;

		node.closed = true;
		++expanded;

		if (node.x == goalX && node.z == goalZ)
		{
			goalIndex = top.index;
			break;
		}

		for (int directionIndex = 0; directionIndex < 8; ++directionIndex)
		{
			const int nextX = node.x + kDx[directionIndex];
			const int nextZ = node.z + kDz[directionIndex];

			if (nextX < minX || nextX > maxX || nextZ < minZ || nextZ > maxZ) continue;
			if (!IsWalkableCell(nextX, nextZ, sampleY)) continue;

			if (settings_.disableCornerCutting && directionIndex >= 4)
			{
				if (!IsWalkableCell(node.x, nextZ, sampleY) || !IsWalkableCell(nextX, node.z, sampleY)) continue;
			}

			const float nextCost = node.g + kCost[directionIndex];
			auto existingIt = indexOf.find({ nextX, nextZ });
			if (existingIt == indexOf.end())
			{
				const float heuristic = HeuristicCost(nextX, nextZ, goalX, goalZ);
				const int nextIndex = addNode(nextX, nextZ, nextCost, heuristic, top.index);
				open.push({ nextCost + heuristic, nextIndex });
			}
			else
			{
				GridNode& existing = nodes[existingIt->second];
				if (existing.closed || nextCost >= existing.g) continue;
				existing.g = nextCost;
				existing.parentIndex = top.index;
				open.push({ existing.g + existing.h, existingIt->second });
			}
		}
	}

	if (goalIndex < 0)
	{
		path_.clear();
		currentPathIndex_ = 0;
		return false;
	}

	std::vector<K4E::Vector3> reversed;
	reversed.reserve(64);
	for (int index = goalIndex; index >= 0; index = nodes[index].parentIndex)
	{
		reversed.push_back(CellToWorld(nodes[index].x, nodes[index].z, current.y));
		if (nodes[index].parentIndex < 0) break;
	}

	std::reverse(reversed.begin(), reversed.end());
	path_ = std::move(reversed);
	currentPathIndex_ = path_.size() > 1 ? 1 : 0;
	return true;
}

bool EnemyAStarNavigator::IsWalkableCell(int x, int z, float sampleY) const
{
	if (std::abs(walkabilityCacheSampleY_ - sampleY) > 0.35f)
	{
		InvalidateWalkabilityCache();
		walkabilityCacheSampleY_ = sampleY;
	}

	if (!IsStaticWalkableCell(x, z, sampleY)) return false;

	const K4E::Vector3 point = CellToWorld(x, z, sampleY);
	for (const TemporaryBlockedArea& blocked : temporaryBlockedAreas_)
	{
		const float dx = point.x - blocked.center.x;
		const float dz = point.z - blocked.center.z;
		if (dx * dx + dz * dz <= blocked.radius * blocked.radius) return false;
	}

	return true;
}

bool EnemyAStarNavigator::IsStaticWalkableCell(int x, int z, float sampleY) const
{
	UpdateInflatedObstacleCache();
	const std::uint64_t cacheKey = MakeCellCacheKey(x, z);
	if (const auto found = staticWalkabilityCache_.find(cacheKey); found != staticWalkabilityCache_.end()) return found->second;

	const K4E::Vector3 point = CellToWorld(x, z, sampleY);
	bool walkable = HasFloorSupport(point, sampleY);
	if (walkable)
	{
		for (const K4E::AABB& obstacle : inflatedObstacleAABBs_)
		{
			if (!IntersectsAgentHeight(obstacle, sampleY)) continue;
			if (point.x >= obstacle.min.x && point.x <= obstacle.max.x &&
				point.z >= obstacle.min.z && point.z <= obstacle.max.z)
			{
				walkable = false;
				break;
			}
		}
	}

	staticWalkabilityCache_.emplace(cacheKey, walkable); // Stage2の同一セルを再探索するたび全床・全障害物へ総当たりしない。
	return walkable;
}

bool EnemyAStarNavigator::HasFloorSupport(const K4E::Vector3& point, float sampleY) const
{
	if (!walkableAABBs_ || walkableAABBs_->empty()) return true;

	const float footY = sampleY - settings_.agentHalfHeight;
	for (const K4E::AABB& floor : *walkableAABBs_)
	{
		if (point.x < floor.min.x || point.x > floor.max.x || point.z < floor.min.z || point.z > floor.max.z) continue;
		if (std::abs(floor.max.y - footY) <= settings_.floorHeightTolerance) return true;
	}
	return false;
}

bool EnemyAStarNavigator::IntersectsAgentHeight(const K4E::AABB& aabb, float sampleY) const
{
	const float agentMinY = sampleY - settings_.agentHalfHeight;
	const float agentMaxY = sampleY + settings_.agentHalfHeight;
	return agentMaxY >= aabb.min.y && agentMinY <= aabb.max.y;
}

void EnemyAStarNavigator::UpdateInflatedObstacleCache() const
{
	if (!worldAABBs_)
	{
		inflatedObstacleAABBs_.clear();
		obstacleCacheSource_ = nullptr;
		obstacleCacheSourceCount_ = 0;
		staticWalkabilityCache_.clear();
		return;
	}
	if (obstacleCacheSource_ == worldAABBs_ &&
		obstacleCacheSourceCount_ == worldAABBs_->size() &&
		std::abs(obstacleCacheAgentRadius_ - settings_.agentRadius) <= 1.0e-5f)
	{
		return;
	}

	inflatedObstacleAABBs_.clear();
	inflatedObstacleAABBs_.reserve(worldAABBs_->size());
	for (const K4E::AABB& aabb : *worldAABBs_)
	{
		K4E::AABB inflated = aabb;
		inflated.min.x -= settings_.agentRadius;
		inflated.max.x += settings_.agentRadius;
		inflated.min.z -= settings_.agentRadius;
		inflated.max.z += settings_.agentRadius;
		inflatedObstacleAABBs_.push_back(inflated);
	}
	obstacleCacheSource_ = worldAABBs_;
	obstacleCacheSourceCount_ = worldAABBs_->size();
	obstacleCacheAgentRadius_ = settings_.agentRadius;
	staticWalkabilityCache_.clear();
}

void EnemyAStarNavigator::InvalidateWalkabilityCache() const
{
	staticWalkabilityCache_.clear();
	walkabilityCacheSampleY_ = 1.0e30f;
}

std::uint64_t EnemyAStarNavigator::MakeCellCacheKey(int x, int z) const
{
	return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32ull)
		| static_cast<std::uint64_t>(static_cast<std::uint32_t>(z));
}

bool EnemyAStarNavigator::IsSegmentBlockedByObstacle(
	const K4E::Vector3& from,
	const K4E::Vector3& to,
	float sampleY,
	int* outBlockedObstacleIndex) const
{
	UpdateInflatedObstacleCache();
	for (size_t index = 0; index < inflatedObstacleAABBs_.size(); ++index)
	{
		const K4E::AABB& obstacle = inflatedObstacleAABBs_[index];
		if (!IntersectsAgentHeight(obstacle, sampleY)) continue;
		if (SegmentIntersectsAABBXZ(from, to, obstacle))
		{
			if (outBlockedObstacleIndex) *outBlockedObstacleIndex = static_cast<int>(index);
			return true;
		}
	}
	return false;
}

bool EnemyAStarNavigator::TrySelectPatrolGoal(
	const K4E::Vector3& current,
	float sampleY,
	std::uint32_t sequence,
	float minimumDistance,
	K4E::Vector3& outGoal) const
{
	if (!walkableAABBs_ || walkableAABBs_->empty()) return false;

	const float minimumDistanceSq = std::max(0.0f, minimumDistance) * std::max(0.0f, minimumDistance);
	bool hasFallback = false;
	K4E::Vector3 fallbackGoal{};
	const size_t attemptCount = std::min<size_t>(32u, std::max<size_t>(12u, walkableAABBs_->size() * 2u));

	for (size_t attempt = 0; attempt < attemptCount; ++attempt)
	{
		const std::uint32_t seed = MixBits(sequence + static_cast<std::uint32_t>(attempt * 0x9e3779b9u));
		const size_t floorIndex = static_cast<size_t>(seed) % walkableAABBs_->size();
		const K4E::AABB& floor = (*walkableAABBs_)[floorIndex];
		const float width = floor.max.x - floor.min.x;
		const float depth = floor.max.z - floor.min.z;
		if (width <= 0.2f || depth <= 0.2f) continue;

		const float marginX = std::min(settings_.agentRadius + 0.25f, width * 0.35f);
		const float marginZ = std::min(settings_.agentRadius + 0.25f, depth * 0.35f);
		const float minX = floor.min.x + marginX;
		const float maxX = floor.max.x - marginX;
		const float minZ = floor.min.z + marginZ;
		const float maxZ = floor.max.z - marginZ;
		if (minX > maxX || minZ > maxZ) continue;

		K4E::Vector3 candidate{};
		candidate.x = minX + (maxX - minX) * Hash01(seed ^ 0x68bc21ebu);
		candidate.y = current.y;
		candidate.z = minZ + (maxZ - minZ) * Hash01(seed ^ 0x02e5be93u);

		int cellX = 0;
		int cellZ = 0;
		WorldToCell(candidate, cellX, cellZ);
		if (!IsWalkableCell(cellX, cellZ, sampleY)) continue;

		if (!hasFallback)
		{
			hasFallback = true;
			fallbackGoal = candidate;
		}
		const float dx = candidate.x - current.x;
		const float dz = candidate.z - current.z;
		if (dx * dx + dz * dz >= minimumDistanceSq)
		{
			outGoal = candidate; // 個体Sequenceで最初に成立した候補を採用し、全員が最遠床へ集まる選択を避ける。
			return true;
		}
	}

	if (!hasFallback) return false;
	outGoal = fallbackGoal;
	return true;
}

bool EnemyAStarNavigator::FindNearestWalkableCell(int centerX, int centerZ, float sampleY, int maxRadius, int& outX, int& outZ) const
{
	if (IsWalkableCell(centerX, centerZ, sampleY))
	{
		outX = centerX;
		outZ = centerZ;
		return true;
	}

	for (int radius = 1; radius <= maxRadius; ++radius)
	{
		for (int dz = -radius; dz <= radius; ++dz)
		{
			for (int dx = -radius; dx <= radius; ++dx)
			{
				if (std::abs(dx) != radius && std::abs(dz) != radius) continue;
				const int x = centerX + dx;
				const int z = centerZ + dz;
				if (!IsWalkableCell(x, z, sampleY)) continue;
				outX = x;
				outZ = z;
				return true;
			}
		}
	}

	return false;
}

K4E::Vector3 EnemyAStarNavigator::CellToWorld(int x, int z, float y) const
{
	return {
		static_cast<float>(x) * settings_.cellSize,
		y,
		static_cast<float>(z) * settings_.cellSize
	};
}

void EnemyAStarNavigator::WorldToCell(const K4E::Vector3& p, int& outX, int& outZ) const
{
	outX = static_cast<int>(std::round(p.x / settings_.cellSize));
	outZ = static_cast<int>(std::round(p.z / settings_.cellSize));
}