#define NOMINMAX
#include "OcclusionCullingSystem.h"

#include "StageChunk.h"
#include "Wireframe.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kClipEpsilon = 1.0e-4f;
		constexpr float kMinOccluderHeight = 2.0f;
		constexpr float kMinOccluderLongSide = 4.0f;
		constexpr float kMaxWallThickness = 3.0f;
		constexpr float kMinOccluderBoxSide = 4.0f;

		BoundingAABB ToBoundingAABB(const AABB& aabb)
		{
			return { aabb.min, aabb.max };
		}

		Vector3 GetSize(const BoundingAABB& bounds)
		{
			return bounds.max - bounds.min;
		}
	}

	void OcclusionCullingSystem::ClearOccluders()
	{
		occluders_.clear();
		chunkDebugInfos_.clear();
		occluderDebugInfos_.clear();
		statistics_ = {};
	}

	void OcclusionCullingSystem::AddOccluder(const Occluder& occluder)
	{
		occluders_.push_back(occluder);
	}

	void OcclusionCullingSystem::BuildAutoOccludersFromWorldAABBs(const std::vector<AABB>& worldAABBs)
	{
		ClearOccluders();
		for (const AABB& aabb : worldAABBs)
		{
			const BoundingAABB bounds = ToBoundingAABB(aabb);
			const Vector3 size = GetSize(bounds);
			const float longSide = std::max(size.x, size.z);
			const float shortSide = std::min(size.x, size.z);
			const bool wallLike = longSide >= kMinOccluderLongSide && shortSide <= kMaxWallThickness;
			const bool boxLike = size.x >= kMinOccluderBoxSide && size.y >= kMinOccluderHeight && size.z >= kMinOccluderBoxSide;
			if (size.y < kMinOccluderHeight || (!wallLike && !boxLike))
			{
				continue;
			}

			AddOccluder(Occluder(bounds, "AutoWallOccluder"));
		}
		statistics_.occluderCount = static_cast<int>(occluders_.size());
	}

	void OcclusionCullingSystem::ApplyToStageChunks(std::vector<StageChunk>& chunks, const Matrix4x4& viewProjection)
	{
		statistics_ = {};
		statistics_.occluderCount = static_cast<int>(occluders_.size());
		chunkDebugInfos_.clear();
		chunkDebugInfos_.reserve(chunks.size());
		occluderDebugInfos_.clear();
		occluderDebugInfos_.reserve(occluders_.size());

		for (const Occluder& occluder : occluders_)
		{
			OccluderDebugInfo occluderDebug{};
			occluderDebug.hasRect = occluder.IsEnabled() && ProjectAABB(occluder.GetWorldBounds(), viewProjection, occluderDebug.rect);
			occluderDebugInfos_.push_back(occluderDebug);
		}

		for (StageChunk& chunk : chunks)
		{
			ChunkDebugInfo debugInfo{};
			debugInfo.chunkId = chunk.GetChunkId();
			debugInfo.bounds = chunk.GetBounds();
			chunk.SetOccludedByOcclusion(false);

			if (!chunk.IsVisible())
			{
				debugInfo.failReason = DebugFailReason::FrustumOutside;
				++statistics_.frustumOutsideCount;
				chunkDebugInfos_.push_back(debugInfo);
				continue;
			}

			++statistics_.testedChunkCount;
			debugInfo.tested = true;

			// 判定詳細を保存して、Draw を止めた理由／止めなかった理由を ImGui で追跡できるようにする。
			const bool wouldBeOccluded = EvaluateOcclusion(chunk.GetBounds(), viewProjection, debugInfo);
			if (enabled_ && wouldBeOccluded)
			{
				debugInfo.occluded = true;
				chunk.SetOccludedByOcclusion(true);
				chunk.SetVisible(false);
				++statistics_.occludedChunkCount;
			}

			chunkDebugInfos_.push_back(debugInfo);
		}
	}

	void OcclusionCullingSystem::DrawDebugBounds() const
	{
		if (showOccluderBounds_)
		{
			const Vector4 occluderColor{ 1.0f, 0.7f, 0.05f, 1.0f };
			for (const Occluder& occluder : occluders_)
			{
				if (!occluder.IsEnabled()) { continue; }
				Wireframe::GetInstance()->DrawAABB(occluder.GetDebugAABB(), occluderColor);
			}
		}

		if (!showOccludedBounds_) { return; }

		const Vector4 depthFailedColor{ 0.1f, 0.45f, 1.0f, 1.0f };
		const Vector4 inFrontColor{ 1.0f, 0.45f, 0.05f, 1.0f };
		for (const ChunkDebugInfo& debugInfo : chunkDebugInfos_)
		{
			Vector4 color{};
			if (debugInfo.failReason == DebugFailReason::DepthInsufficient)
			{
				color = depthFailedColor;
			}
			else if (debugInfo.failReason == DebugFailReason::InFrontOfOccluder)
			{
				color = inFrontColor;
			}
			else
			{
				continue;
			}

			AABB aabb{};
			aabb.min = debugInfo.bounds.min;
			aabb.max = debugInfo.bounds.max;
			Wireframe::GetInstance()->DrawAABB(aabb, color);
		}
	}

	void OcclusionCullingSystem::SetCoverageThreshold(float threshold)
	{
		coverageThreshold_ = std::clamp(threshold, 0.0f, 1.0f);
	}

	void OcclusionCullingSystem::SetDepthBias(float depthBias)
	{
		depthBias_ = std::max(0.0f, depthBias);
	}

	void OcclusionCullingSystem::SetOcclusionMargin(float margin)
	{
		occlusionMargin_ = std::max(0.0f, margin);
	}

	const OcclusionCullingSystem::ChunkDebugInfo* OcclusionCullingSystem::GetSelectedChunkDebugInfo() const
	{
		for (const ChunkDebugInfo& debugInfo : chunkDebugInfos_)
		{
			if (debugInfo.chunkId == debugSelectedChunkId_)
			{
				return &debugInfo;
			}
		}
		return nullptr;
	}

	bool OcclusionCullingSystem::EvaluateOcclusion(const BoundingAABB& bounds, const Matrix4x4& viewProjection, ChunkDebugInfo& debugInfo)
	{
		ScreenRect targetRect{};
		if (!ProjectAABB(bounds, viewProjection, targetRect))
		{
			debugInfo.failReason = DebugFailReason::FrustumOutside;
			++statistics_.frustumOutsideCount;
			return false;
		}

		debugInfo.rect = targetRect;
		debugInfo.hasRect = true;
		debugInfo.bestDepthDelta = std::numeric_limits<float>::lowest();

		bool sawOccluder = false;
		bool sawCoverageFailure = false;
		bool sawDepthFailure = false;
		bool sawInFrontFailure = false;

		for (const Occluder& occluder : occluders_)
		{
			if (!occluder.IsEnabled()) { continue; }

			ScreenRect occluderRect{};
			if (!ProjectAABB(occluder.GetWorldBounds(), viewProjection, occluderRect))
			{
				continue;
			}
			sawOccluder = true;

			const ScreenRect safeOccluderRect = ApplyMargin(occluderRect, occlusionMargin_);
			const bool validSafeRect = IsValidRect(safeOccluderRect);
			const float coverage = validSafeRect ? CalculateCoverage(safeOccluderRect, targetRect) : 0.0f;
			const float occluderCompareDepth = conservativeMode_ ? occluderRect.maxDepth : occluderRect.minDepth;
			// DirectX NDC Z は Near=0 / Far=1 なので、Chunk の最手前Depthが Occluder の手前Depthより大きい場合だけ裏側扱いにする。
			const float depthDelta = targetRect.minDepth - occluderCompareDepth;
			const bool depthPassed = targetRect.minDepth > occluderCompareDepth + depthBias_;
			const bool coveragePassed = validSafeRect && coverage >= coverageThreshold_;

			if (coverage > debugInfo.bestCoverage ||
				(coverage == debugInfo.bestCoverage && depthDelta > debugInfo.bestDepthDelta))
			{
				debugInfo.bestCoverage = coverage;
				debugInfo.bestDepthDelta = depthDelta;
				debugInfo.matchedOccluderRect = occluderRect;
				debugInfo.hasMatchedOccluder = true;
				debugInfo.depthPassed = depthPassed;
				debugInfo.coveragePassed = coveragePassed;
			}

			if (depthDelta <= 0.0f)
			{
				sawInFrontFailure = true;
				continue;
			}
			if (!depthPassed)
			{
				sawDepthFailure = true;
				continue;
			}
			if (!coveragePassed)
			{
				sawCoverageFailure = true;
				continue;
			}

			debugInfo.bestCoverage = coverage;
			debugInfo.bestDepthDelta = depthDelta;
			debugInfo.matchedOccluderRect = occluderRect;
			debugInfo.hasMatchedOccluder = true;
			debugInfo.depthPassed = true;
			debugInfo.coveragePassed = true;
			debugInfo.failReason = DebugFailReason::None;
			return true;
		}

		if (!sawOccluder)
		{
			debugInfo.failReason = DebugFailReason::NoOccluder;
		}
		else if (sawCoverageFailure)
		{
			debugInfo.failReason = DebugFailReason::CoverageInsufficient;
			++statistics_.coverageFailedCount;
		}
		else if (sawDepthFailure)
		{
			debugInfo.failReason = DebugFailReason::DepthInsufficient;
			++statistics_.depthFailedCount;
		}
		else if (sawInFrontFailure)
		{
			debugInfo.failReason = DebugFailReason::InFrontOfOccluder;
			++statistics_.inFrontOfOccluderCount;
		}
		else
		{
			debugInfo.failReason = DebugFailReason::NoOccluder;
		}

		return false;
	}

	bool OcclusionCullingSystem::ProjectAABB(const BoundingAABB& bounds, const Matrix4x4& viewProjection, ScreenRect& outRect) const
	{
		const std::array<Vector3, 8> corners = {
			Vector3{ bounds.min.x, bounds.min.y, bounds.min.z },
			Vector3{ bounds.max.x, bounds.min.y, bounds.min.z },
			Vector3{ bounds.min.x, bounds.max.y, bounds.min.z },
			Vector3{ bounds.max.x, bounds.max.y, bounds.min.z },
			Vector3{ bounds.min.x, bounds.min.y, bounds.max.z },
			Vector3{ bounds.max.x, bounds.min.y, bounds.max.z },
			Vector3{ bounds.min.x, bounds.max.y, bounds.max.z },
			Vector3{ bounds.max.x, bounds.max.y, bounds.max.z },
		};

		outRect.minX = std::numeric_limits<float>::max();
		outRect.minY = std::numeric_limits<float>::max();
		outRect.maxX = std::numeric_limits<float>::lowest();
		outRect.maxY = std::numeric_limits<float>::lowest();
		outRect.minDepth = std::numeric_limits<float>::max();
		outRect.maxDepth = std::numeric_limits<float>::lowest();
		outRect.averageDepth = 0.0f;
		int projectedCornerCount = 0;

		for (const Vector3& corner : corners)
		{
			const float clipX = corner.x * viewProjection.m[0][0] + corner.y * viewProjection.m[1][0] + corner.z * viewProjection.m[2][0] + viewProjection.m[3][0];
			const float clipY = corner.x * viewProjection.m[0][1] + corner.y * viewProjection.m[1][1] + corner.z * viewProjection.m[2][1] + viewProjection.m[3][1];
			const float clipZ = corner.x * viewProjection.m[0][2] + corner.y * viewProjection.m[1][2] + corner.z * viewProjection.m[2][2] + viewProjection.m[3][2];
			const float clipW = corner.x * viewProjection.m[0][3] + corner.y * viewProjection.m[1][3] + corner.z * viewProjection.m[2][3] + viewProjection.m[3][3];
			if (clipW <= kClipEpsilon)
			{
				return false;
			}

			const float invW = 1.0f / clipW;
			const float ndcX = clipX * invW;
			const float ndcY = clipY * invW;
			const float ndcZ = clipZ * invW;
			if (ndcZ < 0.0f || ndcZ > 1.0f)
			{
				return false;
			}

			const float screenX = ndcX * 0.5f + 0.5f;
			const float screenY = -ndcY * 0.5f + 0.5f;
			outRect.minX = std::min(outRect.minX, screenX);
			outRect.maxX = std::max(outRect.maxX, screenX);
			outRect.minY = std::min(outRect.minY, screenY);
			outRect.maxY = std::max(outRect.maxY, screenY);
			outRect.minDepth = std::min(outRect.minDepth, ndcZ);
			outRect.maxDepth = std::max(outRect.maxDepth, ndcZ);
			outRect.averageDepth += ndcZ;
			++projectedCornerCount;
		}

		if (projectedCornerCount <= 0)
		{
			return false;
		}
		outRect.averageDepth /= static_cast<float>(projectedCornerCount);

		outRect.minX = std::clamp(outRect.minX, 0.0f, 1.0f);
		outRect.maxX = std::clamp(outRect.maxX, 0.0f, 1.0f);
		outRect.minY = std::clamp(outRect.minY, 0.0f, 1.0f);
		outRect.maxY = std::clamp(outRect.maxY, 0.0f, 1.0f);
		return IsValidRect(outRect);
	}

	OcclusionCullingSystem::ScreenRect OcclusionCullingSystem::ApplyMargin(const ScreenRect& rect, float margin)
	{
		ScreenRect result = rect;
		result.minX += margin;
		result.maxX -= margin;
		result.minY += margin;
		result.maxY -= margin;
		return result;
	}

	bool OcclusionCullingSystem::IsValidRect(const ScreenRect& rect)
	{
		return rect.minX < rect.maxX && rect.minY < rect.maxY;
	}

	float OcclusionCullingSystem::CalculateCoverage(const ScreenRect& occluderRect, const ScreenRect& targetRect)
	{
		const float targetWidth = targetRect.maxX - targetRect.minX;
		const float targetHeight = targetRect.maxY - targetRect.minY;
		const float targetArea = targetWidth * targetHeight;
		if (targetArea <= 0.0f)
		{
			return 0.0f;
		}

		const float overlapMinX = std::max(occluderRect.minX, targetRect.minX);
		const float overlapMaxX = std::min(occluderRect.maxX, targetRect.maxX);
		const float overlapMinY = std::max(occluderRect.minY, targetRect.minY);
		const float overlapMaxY = std::min(occluderRect.maxY, targetRect.maxY);
		const float overlapWidth = std::max(0.0f, overlapMaxX - overlapMinX);
		const float overlapHeight = std::max(0.0f, overlapMaxY - overlapMinY);
		return (overlapWidth * overlapHeight) / targetArea;
	}
}
