#include "BulletEnemyCollisionSoA.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr int kEnemyChunkSize = 2048;
	}

	BulletEnemyCollisionSoA::BulletEnemyCollisionSoA()
	{
		const unsigned int hardwareThreadCount = std::thread::hardware_concurrency();
		workerCount_ = static_cast<int>(hardwareThreadCount == 0 ? 4 : hardwareThreadCount);
		workerCount_ = std::max(1, workerCount_);
	}

	BulletEnemyCollisionSoA::~BulletEnemyCollisionSoA() = default;

	void BulletEnemyCollisionSoA::Reserve(size_t bulletCapacity, size_t enemyCapacity)
	{
		bulletX_.reserve(bulletCapacity);
		bulletY_.reserve(bulletCapacity);
		bulletZ_.reserve(bulletCapacity);
		bulletRadius_.reserve(bulletCapacity);
		bulletDamage_.reserve(bulletCapacity);
		bulletActive_.reserve(bulletCapacity);
		bulletClaimStamps_.reserve(bulletCapacity);
		bulletNextIndices_.reserve(bulletCapacity);

		enemyX_.reserve(enemyCapacity);
		enemyY_.reserve(enemyCapacity);
		enemyZ_.reserve(enemyCapacity);
		enemyRadius_.reserve(enemyCapacity);
		enemyHp_.reserve(enemyCapacity);
		enemyActive_.reserve(enemyCapacity);
		hitResults_.reserve(std::min(bulletCapacity, enemyCapacity));
	}

	void BulletEnemyCollisionSoA::ClearFrameData()
	{
		bulletX_.clear();
		bulletY_.clear();
		bulletZ_.clear();
		bulletRadius_.clear();
		bulletDamage_.clear();
		bulletActive_.clear();

		enemyX_.clear();
		enemyY_.clear();
		enemyZ_.clear();
		enemyRadius_.clear();
		enemyHp_.clear();
		enemyActive_.clear();

		hitResults_.clear();
		lastFrameStats_ = {};
		ClearUsedGridCells();
	}

	size_t BulletEnemyCollisionSoA::AddBullet(const Vector3& position, float radius, int damage, bool active)
	{
		const size_t index = bulletX_.size();
		bulletX_.push_back(position.x);
		bulletY_.push_back(position.y);
		bulletZ_.push_back(position.z);
		bulletRadius_.push_back(radius);
		bulletDamage_.push_back(damage);
		bulletActive_.push_back(active ? 1u : 0u);
		return index;
	}

	size_t BulletEnemyCollisionSoA::AddEnemy(const Vector3& position, float radius, int hp, bool active)
	{
		const size_t index = enemyX_.size();
		enemyX_.push_back(position.x);
		enemyY_.push_back(position.y);
		enemyZ_.push_back(position.z);
		enemyRadius_.push_back(radius);
		enemyHp_.push_back(hp);
		enemyActive_.push_back(active ? 1u : 0u);
		return index;
	}

	const std::vector<BulletEnemyCollisionSoA::HitResult>& BulletEnemyCollisionSoA::Execute(float cellSize)
	{
		hitResults_.clear();
		lastFrameStats_ = {};

		if (cellSize <= 0.0f || bulletX_.empty() || enemyX_.empty())
		{
			return hitResults_;
		}

		BuildBulletGrid(cellSize);
		if (lastFrameStats_.activeBulletCount == 0 || lastFrameStats_.activeEnemyCount == 0)
		{
			return hitResults_;
		}

		PrepareClaimStamps();
		RunCollisionSearch(cellSize);
		return hitResults_;
	}

	bool BulletEnemyCollisionSoA::IsEnemyActive(size_t enemyIndex) const
	{
		return enemyIndex < enemyActive_.size() && enemyActive_[enemyIndex] != 0u;
	}

	bool BulletEnemyCollisionSoA::IsBulletActive(size_t bulletIndex) const
	{
		return bulletIndex < bulletActive_.size() && bulletActive_[bulletIndex] != 0u;
	}

	int BulletEnemyCollisionSoA::GetEnemyHp(size_t enemyIndex) const
	{
		return enemyIndex < enemyHp_.size() ? enemyHp_[enemyIndex] : 0;
	}

	void BulletEnemyCollisionSoA::BuildBulletGrid(float cellSize)
	{
		ClearUsedGridCells();

		int minCellX = std::numeric_limits<int>::max();
		int minCellZ = std::numeric_limits<int>::max();
		int maxCellX = std::numeric_limits<int>::lowest();
		int maxCellZ = std::numeric_limits<int>::lowest();

		for (size_t i = 0; i < bulletX_.size(); ++i)
		{
			if (bulletActive_[i] == 0u)
			{
				continue;
			}

			const int cellX = ToCell(bulletX_[i], cellSize);
			const int cellZ = ToCell(bulletZ_[i], cellSize);
			minCellX = std::min(minCellX, cellX);
			minCellZ = std::min(minCellZ, cellZ);
			maxCellX = std::max(maxCellX, cellX);
			maxCellZ = std::max(maxCellZ, cellZ);
			++lastFrameStats_.activeBulletCount;
		}

		for (uint8_t active : enemyActive_)
		{
			if (active != 0u)
			{
				++lastFrameStats_.activeEnemyCount;
			}
		}

		if (lastFrameStats_.activeBulletCount == 0)
		{
			gridWidth_ = 0;
			gridDepth_ = 0;
			return;
		}

		gridMinCellX_ = minCellX;
		gridMinCellZ_ = minCellZ;
		gridWidth_ = maxCellX - minCellX + 1;
		gridDepth_ = maxCellZ - minCellZ + 1;

		const size_t cellCount = static_cast<size_t>(gridWidth_) * static_cast<size_t>(gridDepth_);
		if (gridHeads_.size() != cellCount)
		{
			gridHeads_.assign(cellCount, -1);
			usedCellIndices_.clear();
		}

		if (bulletNextIndices_.size() < bulletX_.size())
		{
			bulletNextIndices_.resize(bulletX_.size(), -1);
		}

		for (int bulletIndex = 0; bulletIndex < static_cast<int>(bulletX_.size()); ++bulletIndex)
		{
			if (bulletActive_[bulletIndex] == 0u)
			{
				continue;
			}

			const int cellX = ToCell(bulletX_[bulletIndex], cellSize);
			const int cellZ = ToCell(bulletZ_[bulletIndex], cellSize);
			const int cellIndex = ToGridIndex(cellX, cellZ);

			// XZ平面のセルに登録し、弾と敵の候補判定数を減らす。
			if (gridHeads_[cellIndex] == -1)
			{
				usedCellIndices_.push_back(cellIndex);
			}

			bulletNextIndices_[bulletIndex] = gridHeads_[cellIndex];
			gridHeads_[cellIndex] = bulletIndex;
		}
	}

	void BulletEnemyCollisionSoA::ClearUsedGridCells()
	{
		for (int cellIndex : usedCellIndices_)
		{
			if (0 <= cellIndex && cellIndex < static_cast<int>(gridHeads_.size()))
			{
				gridHeads_[cellIndex] = -1;
			}
		}
		usedCellIndices_.clear();
	}

	int BulletEnemyCollisionSoA::ToCell(float value, float cellSize) const
	{
		return static_cast<int>(std::floor(value / cellSize));
	}

	int BulletEnemyCollisionSoA::ToGridIndex(int cellX, int cellZ) const
	{
		return (cellZ - gridMinCellZ_) * gridWidth_ + (cellX - gridMinCellX_);
	}

	bool BulletEnemyCollisionSoA::IsCellInsideGrid(int cellX, int cellZ) const
	{
		return cellX >= gridMinCellX_ && cellZ >= gridMinCellZ_ &&
			cellX < gridMinCellX_ + gridWidth_ && cellZ < gridMinCellZ_ + gridDepth_;
	}

	bool BulletEnemyCollisionSoA::CheckSphereCollision(size_t bulletIndex, size_t enemyIndex) const
	{
		const float dx = bulletX_[bulletIndex] - enemyX_[enemyIndex];
		const float dy = bulletY_[bulletIndex] - enemyY_[enemyIndex];
		const float dz = bulletZ_[bulletIndex] - enemyZ_[enemyIndex];
		const float radiusSum = bulletRadius_[bulletIndex] + enemyRadius_[enemyIndex];
		return dx * dx + dy * dy + dz * dz <= radiusSum * radiusSum;
	}

	void BulletEnemyCollisionSoA::PrepareClaimStamps()
	{
		if (bulletClaimStamps_.size() < bulletX_.size())
		{
			bulletClaimStamps_.resize(bulletX_.size(), 0u);
		}

		++currentClaimStamp_;
		if (currentClaimStamp_ == 0u)
		{
			std::fill(bulletClaimStamps_.begin(), bulletClaimStamps_.end(), 0u);
			currentClaimStamp_ = 1u;
		}
	}

	void BulletEnemyCollisionSoA::RunCollisionSearch(float cellSize)
	{
		const int enemyCount = static_cast<int>(enemyX_.size());
		const int threadCount = std::max(1, std::min(workerCount_, enemyCount));
		std::atomic<int> nextEnemyIndex = 0;

		std::vector<std::thread> workers;
		std::vector<std::vector<HitResult>> workerHits(static_cast<size_t>(threadCount));
		std::vector<size_t> workerChecks(static_cast<size_t>(threadCount), 0);
		std::vector<size_t> workerHitCounts(static_cast<size_t>(threadCount), 0);

		workers.reserve(static_cast<size_t>(threadCount));
		for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex)
		{
			workers.emplace_back([&, threadIndex]()
				{
					while (true)
					{
						const int enemyBegin = nextEnemyIndex.fetch_add(kEnemyChunkSize, std::memory_order_relaxed);
						if (enemyBegin >= enemyCount)
						{
							break;
						}

						const int enemyEnd = std::min(enemyBegin + kEnemyChunkSize, enemyCount);
						ProcessEnemyRange(enemyBegin, enemyEnd, cellSize, workerChecks[threadIndex], workerHitCounts[threadIndex], workerHits[threadIndex]);
					}
				});
		}

		for (std::thread& worker : workers)
		{
			worker.join();
		}

		for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex)
		{
			lastFrameStats_.collisionChecks += workerChecks[threadIndex];
			lastFrameStats_.hitCount += workerHitCounts[threadIndex];
			hitResults_.insert(hitResults_.end(), workerHits[threadIndex].begin(), workerHits[threadIndex].end());
		}
	}

	void BulletEnemyCollisionSoA::ProcessEnemyRange(int enemyBegin, int enemyEnd, float cellSize, size_t& outChecks, size_t& outHits, std::vector<HitResult>& outHitsList)
	{
		for (int enemyIndex = enemyBegin; enemyIndex < enemyEnd; ++enemyIndex)
		{
			if (enemyActive_[enemyIndex] == 0u || enemyHp_[enemyIndex] <= 0)
			{
				continue;
			}

			const int enemyCellX = ToCell(enemyX_[enemyIndex], cellSize);
			const int enemyCellZ = ToCell(enemyZ_[enemyIndex], cellSize);

			for (int offsetZ = -1; offsetZ <= 1; ++offsetZ)
			{
				for (int offsetX = -1; offsetX <= 1; ++offsetX)
				{
					const int cellX = enemyCellX + offsetX;
					const int cellZ = enemyCellZ + offsetZ;
					if (!IsCellInsideGrid(cellX, cellZ))
					{
						continue;
					}

					const int cellIndex = ToGridIndex(cellX, cellZ);
					for (int bulletIndex = gridHeads_[cellIndex]; bulletIndex != -1; bulletIndex = bulletNextIndices_[bulletIndex])
					{
						if (bulletActive_[bulletIndex] == 0u)
						{
							continue;
						}

						std::atomic_ref<uint32_t> claimStamp(bulletClaimStamps_[bulletIndex]);
						if (claimStamp.load(std::memory_order_relaxed) == currentClaimStamp_)
						{
							continue;
						}

						++outChecks;
						if (!CheckSphereCollision(static_cast<size_t>(bulletIndex), static_cast<size_t>(enemyIndex)))
						{
							continue;
						}

						uint32_t expectedStamp = claimStamp.load(std::memory_order_relaxed);
						if (expectedStamp == currentClaimStamp_ || !claimStamp.compare_exchange_strong(expectedStamp, currentClaimStamp_, std::memory_order_relaxed))
						{
							continue;
						}

						bulletActive_[bulletIndex] = 0u;
						enemyHp_[enemyIndex] -= bulletDamage_[bulletIndex];
						if (enemyHp_[enemyIndex] <= 0)
						{
							enemyActive_[enemyIndex] = 0u;
						}

						++outHits;
						outHitsList.push_back(HitResult{
							static_cast<size_t>(bulletIndex),
							static_cast<size_t>(enemyIndex),
							bulletDamage_[bulletIndex],
							Vector3{ bulletX_[bulletIndex], bulletY_[bulletIndex], bulletZ_[bulletIndex] }
							});

						if (enemyActive_[enemyIndex] == 0u)
						{
							break;
						}
					}

					if (enemyActive_[enemyIndex] == 0u)
					{
						break;
					}
				}

				if (enemyActive_[enemyIndex] == 0u)
				{
					break;
				}
			}
		}
	}
}
