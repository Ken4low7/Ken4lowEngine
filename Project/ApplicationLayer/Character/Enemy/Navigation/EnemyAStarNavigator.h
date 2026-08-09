#pragma once

#include "AABB.h"
#include "Vector3.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace K4E = ::Ken4lowEngine;

class EnemyAStarNavigator
{
public:
	struct Settings
	{
		float cellSize = 1.5f;
		float agentRadius = 0.7f;
		float agentHalfHeight = 2.0f;
		float floorHeightTolerance = 1.0f;
		int searchRangeCells = 28;
		int maxExpandedNodes = 4000;
		float waypointReachDistance = 0.6f;
		float repathIntervalSec = 0.25f;
		bool disableCornerCutting = true;
	};
	struct TemporaryBlockedArea
	{
		K4E::Vector3 center{};
		float radius = 1.0f;
		float remainingSec = 0.0f;
		std::string reason{};
	};

public:
	/// Navigationで回避する障害物AABBを設定する。参照先の所有権は移さない。
	void SetWorldAABBs(const std::vector<K4E::AABB>* worldAABBs);

	/// Navigationで歩行可能とみなす床AABBを設定する。未設定時は従来どおり範囲制限を行わない。
	void SetWalkableAABBs(const std::vector<K4E::AABB>* walkableAABBs);

	/// Grid、Agentサイズ、探索上限を更新し、条件が変わった場合は現在経路を破棄する。
	void SetSettings(const Settings& settings);
	const Settings& GetSettings() const { return settings_; }

	void Reset();
	bool GetNextWaypoint(
		const K4E::Vector3& current,
		const K4E::Vector3& goal,
		float sampleY,
		float deltaTime,
		K4E::Vector3& outWaypoint);
	bool IsSegmentBlockedByObstacle(
		const K4E::Vector3& from,
		const K4E::Vector3& to,
		float sampleY,
		int* outBlockedObstacleIndex = nullptr) const;

	/// Stage床上から決定的な巡回候補を選び、複数個体が同じ遠方地点へ集中しない目標を返す。
	bool TrySelectPatrolGoal(
		const K4E::Vector3& current,
		float sampleY,
		std::uint32_t sequence,
		float minimumDistance,
		K4E::Vector3& outGoal) const;

	void TickTemporaryBlocks(float deltaTime);
	void AddTemporaryBlockedArea(const K4E::Vector3& center, float radius, float durationSec, const char* reason);
	void ClearTemporaryBlockedAreas();
	const std::vector<K4E::AABB>& GetInflatedObstacleAABBs() const { return inflatedObstacleAABBs_; }
	const std::vector<K4E::Vector3>& GetCurrentPath() const { return path_; }
	const std::vector<TemporaryBlockedArea>& GetTemporaryBlockedAreas() const { return temporaryBlockedAreas_; }
	int GetCurrentPathIndex() const { return currentPathIndex_; }
	float GetRepathTimer() const { return repathTimer_; }
	size_t GetObstacleCount() const { return worldAABBs_ ? worldAABBs_->size() : 0u; }
	size_t GetWalkableAreaCount() const { return walkableAABBs_ ? walkableAABBs_->size() : 0u; }

private:
	struct GridNode
	{
		int x = 0;
		int z = 0;
		float g = 0.0f;
		float h = 0.0f;
		int parentIndex = -1;
		bool closed = false;
		bool opened = false;
	};

	bool RebuildPath(const K4E::Vector3& current, const K4E::Vector3& goal, float sampleY);
	bool IsWalkableCell(int x, int z, float sampleY) const;
	bool IsStaticWalkableCell(int x, int z, float sampleY) const;
	bool HasFloorSupport(const K4E::Vector3& point, float sampleY) const;
	bool IntersectsAgentHeight(const K4E::AABB& aabb, float sampleY) const;
	void UpdateInflatedObstacleCache() const;
	void InvalidateWalkabilityCache() const;
	std::uint64_t MakeCellCacheKey(int x, int z) const;
	bool FindNearestWalkableCell(int centerX, int centerZ, float sampleY, int maxRadius, int& outX, int& outZ) const;
	K4E::Vector3 CellToWorld(int x, int z, float y) const;
	void WorldToCell(const K4E::Vector3& p, int& outX, int& outZ) const;

private:
	const std::vector<K4E::AABB>* worldAABBs_ = nullptr;
	const std::vector<K4E::AABB>* walkableAABBs_ = nullptr;
	Settings settings_{};

	std::vector<K4E::Vector3> path_{};
	mutable std::vector<K4E::AABB> inflatedObstacleAABBs_{};
	mutable const std::vector<K4E::AABB>* obstacleCacheSource_ = nullptr;
	mutable size_t obstacleCacheSourceCount_ = 0;
	mutable float obstacleCacheAgentRadius_ = -1.0f;
	mutable std::unordered_map<std::uint64_t, bool> staticWalkabilityCache_{};
	mutable float walkabilityCacheSampleY_ = 1.0e30f;
	std::vector<TemporaryBlockedArea> temporaryBlockedAreas_{};
	int currentPathIndex_ = 0;
	float repathTimer_ = 0.0f;
	int lastGoalX_ = 0;
	int lastGoalZ_ = 0;
	bool hasLastGoal_ = false;
};