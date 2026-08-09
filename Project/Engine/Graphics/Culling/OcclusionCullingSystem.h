#pragma once
#include "BoundingVolume.h"
#include "Matrix4x4.h"
#include "Occluder.h"

#include <vector>

namespace Ken4lowEngine
{
	class StageChunk;

	/// <summary>
	/// Occluder と StageChunk Bounds をスクリーン矩形に投影して判定する CPU 側の簡易 Occlusion Culling。
	/// </summary>
	class OcclusionCullingSystem
	{
	public:
		struct Statistics
		{
			int occluderCount = 0;
			int testedChunkCount = 0;
			int occludedChunkCount = 0;
			int coverageFailedCount = 0;
			int depthFailedCount = 0;
			int frustumOutsideCount = 0;
			int inFrontOfOccluderCount = 0;
		};

		struct ScreenRect
		{
			float minX = 0.0f;
			float maxX = 0.0f;
			float minY = 0.0f;
			float maxY = 0.0f;
			float minDepth = 1.0f;
			float maxDepth = 0.0f;
			float averageDepth = 0.0f;
		};

		enum class DebugFailReason
		{
			None,
			CoverageInsufficient,
			DepthInsufficient,
			FrustumOutside,
			InFrontOfOccluder,
			NoOccluder
		};

		struct ChunkDebugInfo
		{
			int chunkId = -1;
			BoundingAABB bounds{};
			ScreenRect rect{};
			ScreenRect matchedOccluderRect{};
			bool hasRect = false;
			bool hasMatchedOccluder = false;
			bool tested = false;
			bool occluded = false;
			bool depthPassed = false;
			bool coveragePassed = false;
			float bestCoverage = 0.0f;
			float bestDepthDelta = -1.0f;
			DebugFailReason failReason = DebugFailReason::None;
		};

		struct OccluderDebugInfo
		{
			ScreenRect rect{};
			bool hasRect = false;
		};

		void ClearOccluders();
		void AddOccluder(const Occluder& occluder);
		void BuildAutoOccludersFromWorldAABBs(const std::vector<AABB>& worldAABBs);
		void ApplyToStageChunks(std::vector<StageChunk>& chunks, const Matrix4x4& viewProjection);
		void DrawDebugBounds() const;

		void SetEnabled(bool enabled) { enabled_ = enabled; }
		bool IsEnabled() const { return enabled_; }
		void SetShowOccluderBounds(bool visible) { showOccluderBounds_ = visible; }
		bool IsShowOccluderBounds() const { return showOccluderBounds_; }
		void SetShowOccludedBounds(bool visible) { showOccludedBounds_ = visible; }
		bool IsShowOccludedBounds() const { return showOccludedBounds_; }
		void SetShowOccluderScreenRects(bool visible) { showOccluderScreenRects_ = visible; }
		bool IsShowOccluderScreenRects() const { return showOccluderScreenRects_; }
		void SetShowChunkScreenRects(bool visible) { showChunkScreenRects_ = visible; }
		bool IsShowChunkScreenRects() const { return showChunkScreenRects_; }
		void SetCoverageThreshold(float threshold);
		float GetCoverageThreshold() const { return coverageThreshold_; }
		void SetDepthBias(float depthBias);
		float GetDepthBias() const { return depthBias_; }
		void SetOcclusionMargin(float margin);
		float GetOcclusionMargin() const { return occlusionMargin_; }
		void SetConservativeMode(bool enabled) { conservativeMode_ = enabled; }
		bool IsConservativeMode() const { return conservativeMode_; }
		void SetDebugSelectedChunkId(int chunkId) { debugSelectedChunkId_ = chunkId; }
		int GetDebugSelectedChunkId() const { return debugSelectedChunkId_; }
		const ChunkDebugInfo* GetSelectedChunkDebugInfo() const;
		const Statistics& GetStatistics() const { return statistics_; }
		const std::vector<Occluder>& GetOccluders() const { return occluders_; }
		const std::vector<ChunkDebugInfo>& GetChunkDebugInfos() const { return chunkDebugInfos_; }
		const std::vector<OccluderDebugInfo>& GetOccluderDebugInfos() const { return occluderDebugInfos_; }

	private:
		bool EvaluateOcclusion(const BoundingAABB& bounds, const Matrix4x4& viewProjection, ChunkDebugInfo& debugInfo);
		bool ProjectAABB(const BoundingAABB& bounds, const Matrix4x4& viewProjection, ScreenRect& outRect) const;
		static ScreenRect ApplyMargin(const ScreenRect& rect, float margin);
		static bool IsValidRect(const ScreenRect& rect);
		static float CalculateCoverage(const ScreenRect& occluderRect, const ScreenRect& targetRect);

		std::vector<Occluder> occluders_{};
		std::vector<ChunkDebugInfo> chunkDebugInfos_{};
		std::vector<OccluderDebugInfo> occluderDebugInfos_{};
		Statistics statistics_{};
		bool enabled_ = false;
		bool showOccluderBounds_ = false;
		bool showOccludedBounds_ = false;
		bool showOccluderScreenRects_ = false;
		bool showChunkScreenRects_ = false;
		float coverageThreshold_ = 0.98f;
		float depthBias_ = 0.02f;
		float occlusionMargin_ = 0.02f;
		bool conservativeMode_ = false;
		int debugSelectedChunkId_ = 0;
	};
}
