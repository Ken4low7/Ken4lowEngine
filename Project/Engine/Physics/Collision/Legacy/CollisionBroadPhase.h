#pragma once

#include "Collider.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Ken4lowEngine
{
	class Collider;
}

namespace K4E = ::Ken4lowEngine;

// Broad Phaseが返す候補ペア。Narrow Phase側で実際の形状判定とイベント登録を行う。
struct CollisionPair
{
	K4E::Collider* a = nullptr;
	K4E::Collider* b = nullptr;
};

// 既存CheckAllCollisionsのTypeIDペア列挙をBroad Phase互換実装へ渡すための軽量データ。
struct CollisionBroadPhaseTypePair
{
	uint32_t aTypeId = 0;
	uint32_t bTypeId = 0;
};

// Broad Phaseは衝突する可能性のあるColliderペアだけを集め、ResponseMatrixの最終判断はCollisionManager側に残す。
class ICollisionBroadPhase
{
public:
	static constexpr uint32_t kMaxTypes = 32;
	using ColliderBuckets = std::array<std::vector<K4E::Collider*>, kMaxTypes>;

	virtual ~ICollisionBroadPhase() = default;

	virtual void CollectPairs(
		const ColliderBuckets& buckets,
		const std::vector<CollisionBroadPhaseTypePair>& typePairs,
		std::vector<CollisionPair>& outPairs) const = 0;
};

// 既存のTypeIDバケット総当たりと同じ候補を返す互換Broad Phase実装。
class BruteForceBroadPhase final : public ICollisionBroadPhase
{
public:
	void CollectPairs(
		const ColliderBuckets& buckets,
		const std::vector<CollisionBroadPhaseTypePair>& typePairs,
		std::vector<CollisionPair>& outPairs) const override
	{
		outPairs.clear();

		for (const CollisionBroadPhaseTypePair& typePair : typePairs)
		{
			if (typePair.aTypeId >= kMaxTypes || typePair.bTypeId >= kMaxTypes) continue;

			const auto& bucketA = buckets[typePair.aTypeId];
			const auto& bucketB = buckets[typePair.bTypeId];
			if (bucketA.empty() || bucketB.empty()) continue;

			if (typePair.aTypeId == typePair.bTypeId)
			{
				// 同一Type内ではjをi+1から始め、同じColliderペアの重複収集を避ける。
				for (size_t i = 0; i < bucketA.size(); ++i)
				{
					K4E::Collider* a = bucketA[i];
					if (!a || !a->IsCollisionEnabledForQuery()) continue;
					for (size_t j = i + 1; j < bucketA.size(); ++j)
					{
						K4E::Collider* b = bucketA[j];
						if (!b || !b->IsCollisionEnabledForQuery()) continue;
						outPairs.push_back({ a, b });
					}
				}
				continue;
			}

			// 既存pairLoopと同じ片方向ペアだけを集め、イベント順の変更を避ける。
			for (K4E::Collider* a : bucketA)
			{
				if (!a || !a->IsCollisionEnabledForQuery()) continue;
				for (K4E::Collider* b : bucketB)
				{
					if (!b || !b->IsCollisionEnabledForQuery()) continue;
					outPairs.push_back({ a, b });
				}
			}
		}
	}
};

// Uniform Gridのセル座標。ColliderのBounding AABBがまたぐセルへ登録する。
struct UniformGridCellCoord
{
	int x = 0;
	int y = 0;
	int z = 0;

	bool operator==(const UniformGridCellCoord& other) const
	{
		return x == other.x && y == other.y && z == other.z;
	}
};

struct UniformGridCellCoordHash
{
	size_t operator()(const UniformGridCellCoord& cell) const
	{
		const size_t hx = std::hash<int>{}(cell.x);
		const size_t hy = std::hash<int>{}(cell.y);
		const size_t hz = std::hash<int>{}(cell.z);
		return hx ^ (hy << 1) ^ (hz << 2);
	}
};

// UniformGridBroadPhaseは近いセル内のColliderだけを候補にする試験実装。
class UniformGridBroadPhase final : public ICollisionBroadPhase
{
public:
	// 1セルの一辺長。初期値はステージ/弾/敵の粗い近傍判定用として大きめに置く。
	static constexpr float kDefaultCellSize = 16.0f;

	explicit UniformGridBroadPhase(float cellSize = kDefaultCellSize)
		: cellSize_(cellSize > 0.0f ? cellSize : kDefaultCellSize)
	{
	}

	float GetCellSize() const { return cellSize_; }
	size_t GetLastRegisteredColliderCount() const { return lastRegisteredColliderCount_; }
	size_t GetLastUsedCellCount() const { return lastUsedCellCount_; }
	size_t GetLastCandidatePairCount() const { return lastCandidatePairCount_; }

	void CollectPairs(
		const ColliderBuckets& buckets,
		const std::vector<CollisionBroadPhaseTypePair>& typePairs,
		std::vector<CollisionPair>& outPairs) const override
	{
		outPairs.clear();
		std::unordered_set<uint64_t> addedPairKeys;
		size_t registeredColliderCount = 0;
		size_t usedCellCount = 0;

		for (const CollisionBroadPhaseTypePair& typePair : typePairs)
		{
			if (typePair.aTypeId >= kMaxTypes || typePair.bTypeId >= kMaxTypes) continue;

			const auto& bucketA = buckets[typePair.aTypeId];
			const auto& bucketB = buckets[typePair.bTypeId];
			if (bucketA.empty() || bucketB.empty()) continue;

			if (typePair.aTypeId == typePair.bTypeId)
			{
				CollectSameTypePairs(bucketA, outPairs, addedPairKeys, registeredColliderCount, usedCellCount);
			}
			else
			{
				CollectCrossTypePairs(bucketA, bucketB, outPairs, addedPairKeys, registeredColliderCount, usedCellCount);
			}
		}

		lastRegisteredColliderCount_ = registeredColliderCount;
		lastUsedCellCount_ = usedCellCount;
		lastCandidatePairCount_ = outPairs.size();
	}

private:
	struct GridEntry
	{
		K4E::Collider* collider = nullptr;
		K4E::AABB bounds{};
	};

	using GridCellMap = std::unordered_map<UniformGridCellCoord, std::vector<GridEntry>, UniformGridCellCoordHash>;

	UniformGridCellCoord ToCell(const K4E::Vector3& point) const
	{
		return {
			static_cast<int>(std::floor(point.x / cellSize_)),
			static_cast<int>(std::floor(point.y / cellSize_)),
			static_cast<int>(std::floor(point.z / cellSize_)),
		};
	}

	void InsertEntry(GridCellMap& grid, K4E::Collider* collider) const
	{
		if (!collider || !collider->IsCollisionEnabledForQuery()) return;

		// 現在のColliderはGetAABBを常に返せるため、取れない形状が出たらBruteForceフォールバック対象にする。
		const K4E::AABB bounds = collider->GetAABB();
		const UniformGridCellCoord minCell = ToCell(bounds.min);
		const UniformGridCellCoord maxCell = ToCell(bounds.max);
		const GridEntry entry{ collider, bounds };

		for (int z = minCell.z; z <= maxCell.z; ++z)
		{
			for (int y = minCell.y; y <= maxCell.y; ++y)
			{
				for (int x = minCell.x; x <= maxCell.x; ++x)
				{
					grid[{ x, y, z }].push_back(entry);
				}
			}
		}
	}

	void BuildGrid(const std::vector<K4E::Collider*>& colliders, GridCellMap& outGrid, size_t& registeredColliderCount, size_t& usedCellCount) const
	{
		outGrid.clear();
		for (K4E::Collider* collider : colliders)
		{
			if (collider && collider->IsCollisionEnabledForQuery()) ++registeredColliderCount;
			InsertEntry(outGrid, collider);
		}
		usedCellCount += outGrid.size();
	}

	static uint64_t MakePairKey(K4E::Collider* a, K4E::Collider* b)
	{
		const uint32_t idA = a ? a->GetUniqueID() : 0;
		const uint32_t idB = b ? b->GetUniqueID() : 0;
		const uint32_t lo = idA < idB ? idA : idB;
		const uint32_t hi = idA < idB ? idB : idA;
		return (static_cast<uint64_t>(lo) << 32) | hi;
	}

	void TryAddPair(K4E::Collider* a, K4E::Collider* b, std::vector<CollisionPair>& outPairs, std::unordered_set<uint64_t>& addedPairKeys) const
	{
		if (!a || !b || a == b) return;
		if (!a->IsCollisionEnabledForQuery() || !b->IsCollisionEnabledForQuery()) return;

		const uint64_t key = MakePairKey(a, b);
		if (!addedPairKeys.insert(key).second) return;

		// 同一ペアが複数セルに登録されても、UniqueIDキーで候補ペアは一度だけ返す。
		outPairs.push_back({ a, b });
	}

	void CollectSameTypePairs(
		const std::vector<K4E::Collider*>& bucket,
		std::vector<CollisionPair>& outPairs,
		std::unordered_set<uint64_t>& addedPairKeys,
		size_t& registeredColliderCount,
		size_t& usedCellCount) const
	{
		GridCellMap grid;
		BuildGrid(bucket, grid, registeredColliderCount, usedCellCount);

		for (const auto& [_, entries] : grid)
		{
			for (size_t i = 0; i < entries.size(); ++i)
			{
				for (size_t j = i + 1; j < entries.size(); ++j)
				{
					TryAddPair(entries[i].collider, entries[j].collider, outPairs, addedPairKeys);
				}
			}
		}
	}

	void CollectCrossTypePairs(
		const std::vector<K4E::Collider*>& bucketA,
		const std::vector<K4E::Collider*>& bucketB,
		std::vector<CollisionPair>& outPairs,
		std::unordered_set<uint64_t>& addedPairKeys,
		size_t& registeredColliderCount,
		size_t& usedCellCount) const
	{
		GridCellMap gridA;
		BuildGrid(bucketA, gridA, registeredColliderCount, usedCellCount);

		for (K4E::Collider* b : bucketB)
		{
			if (!b || !b->IsCollisionEnabledForQuery()) continue;

			// B側のBounding AABBがまたぐセルだけを調べ、遠いColliderとの候補化を避ける。
			const K4E::AABB bounds = b->GetAABB();
			const UniformGridCellCoord minCell = ToCell(bounds.min);
			const UniformGridCellCoord maxCell = ToCell(bounds.max);

			for (int z = minCell.z; z <= maxCell.z; ++z)
			{
				for (int y = minCell.y; y <= maxCell.y; ++y)
				{
					for (int x = minCell.x; x <= maxCell.x; ++x)
					{
						const auto it = gridA.find({ x, y, z });
						if (it == gridA.end()) continue;

						for (const GridEntry& aEntry : it->second)
						{
							TryAddPair(aEntry.collider, b, outPairs, addedPairKeys);
						}
					}
				}
			}
		}
	}

	float cellSize_ = kDefaultCellSize;
	mutable size_t lastRegisteredColliderCount_ = 0;
	mutable size_t lastUsedCellCount_ = 0;
	mutable size_t lastCandidatePairCount_ = 0;
};

// TODO: QuadTreeBroadPhase / OctreeBroadPhase / BVHBroadPhase はこのインターフェースへ差し替える。
