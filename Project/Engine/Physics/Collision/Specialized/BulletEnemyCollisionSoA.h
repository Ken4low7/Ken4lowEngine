#pragma once

#include "Vector3.h"

#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///       Bullet / Enemy dedicated collision system using SoA
	/// -------------------------------------------------------------
	class BulletEnemyCollisionSoA
	{
	public:
		struct HitResult
		{
			size_t bulletIndex = 0;
			size_t enemyIndex = 0;
			int damage = 0;
			Vector3 hitPosition{};
		};

		struct FrameStats
		{
			size_t collisionChecks = 0;
			size_t hitCount = 0;
			size_t activeBulletCount = 0;
			size_t activeEnemyCount = 0;
		};

	public:
		BulletEnemyCollisionSoA();
		~BulletEnemyCollisionSoA();

		BulletEnemyCollisionSoA(const BulletEnemyCollisionSoA&) = delete;
		BulletEnemyCollisionSoA& operator=(const BulletEnemyCollisionSoA&) = delete;

		void Reserve(size_t bulletCapacity, size_t enemyCapacity);
		void ClearFrameData();

		size_t AddBullet(const Vector3& position, float radius, int damage, bool active = true);
		size_t AddEnemy(const Vector3& position, float radius, int hp, bool active = true);

		const std::vector<HitResult>& Execute(float cellSize);
		const FrameStats& GetLastFrameStats() const { return lastFrameStats_; }

		bool IsEnemyActive(size_t enemyIndex) const;
		bool IsBulletActive(size_t bulletIndex) const;
		int GetEnemyHp(size_t enemyIndex) const;

	private:
		void BuildBulletGrid(float cellSize);
		void ClearUsedGridCells();
		int ToCell(float value, float cellSize) const;
		int ToGridIndex(int cellX, int cellZ) const;
		bool IsCellInsideGrid(int cellX, int cellZ) const;
		bool CheckSphereCollision(size_t bulletIndex, size_t enemyIndex) const;
		void PrepareClaimStamps();
		void RunCollisionSearch(float cellSize);
		void ProcessEnemyRange(int enemyBegin, int enemyEnd, float cellSize, size_t& outChecks, size_t& outHits, std::vector<HitResult>& outHitsList);

	private:
		std::vector<float> bulletX_;
		std::vector<float> bulletY_;
		std::vector<float> bulletZ_;
		std::vector<float> bulletRadius_;
		std::vector<int> bulletDamage_;
		std::vector<uint8_t> bulletActive_;
		std::vector<uint32_t> bulletClaimStamps_;

		std::vector<float> enemyX_;
		std::vector<float> enemyY_;
		std::vector<float> enemyZ_;
		std::vector<float> enemyRadius_;
		std::vector<int> enemyHp_;
		std::vector<uint8_t> enemyActive_;

		std::vector<int> gridHeads_;
		std::vector<int> bulletNextIndices_;
		std::vector<int> usedCellIndices_;

		std::vector<HitResult> hitResults_;
		FrameStats lastFrameStats_{};

		uint32_t currentClaimStamp_ = 0;
		int gridMinCellX_ = 0;
		int gridMinCellZ_ = 0;
		int gridWidth_ = 0;
		int gridDepth_ = 0;
		int workerCount_ = 1;
	};
}
