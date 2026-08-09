#include "FrustumCullingSystem.h"

namespace Ken4lowEngine
{
	void FrustumCullingSystem::BuildFrustum(const Matrix4x4& viewProjection)
	{
		viewProjection_ = viewProjection;
		frustum_.BuildFromViewProjection(viewProjection_);
	}

	void FrustumCullingSystem::ResetStatistics()
	{
		totalObjectCount_ = 0;
		drawnObjectCount_ = 0;
		culledObjectCount_ = 0;
		cullingDisabledDrawnObjectCount_ = 0;
		missingBoundsDrawnObjectCount_ = 0;
		totalMeshCount_ = 0;
		drawnMeshCount_ = 0;
		culledMeshCount_ = 0;
		totalStageObjectCount_ = 0;
		drawnStageObjectCount_ = 0;
		culledStageObjectCount_ = 0;
		totalStageChunkCount_ = 0;
		drawnStageChunkCount_ = 0;
		culledStageChunkCount_ = 0;
	}

	bool FrustumCullingSystem::IsVisible(const BoundingSphere& bounds, bool ignoreFrustumCulling, bool hasBounds, CullingUnit unit)
	{
		return IsVisibleInternal(hasBounds ? frustum_.Intersects(bounds) : true, ignoreFrustumCulling, hasBounds, unit);
	}

	bool FrustumCullingSystem::IsVisible(const BoundingAABB& bounds, bool ignoreFrustumCulling, bool hasBounds, CullingUnit unit)
	{
		return IsVisibleInternal(hasBounds ? frustum_.Intersects(bounds) : true, ignoreFrustumCulling, hasBounds, unit);
	}

	bool FrustumCullingSystem::IsVisibleInternal(bool intersects, bool ignoreFrustumCulling, bool hasBounds, CullingUnit unit)
	{
		CountTotal(unit);

		if (!enabled_ || ignoreFrustumCulling)
		{
			CountDrawn(unit);
			if (unit == CullingUnit::Object || unit == CullingUnit::StageObject)
			{
				++cullingDisabledDrawnObjectCount_;
			}
			return true;
		}

		if (!hasBounds)
		{
			CountDrawn(unit);
			if (unit == CullingUnit::Object || unit == CullingUnit::StageObject)
			{
				++missingBoundsDrawnObjectCount_;
			}
			return true;
		}

		if (intersects)
		{
			CountDrawn(unit);
			return true;
		}

		CountCulled(unit);
		return false;
	}

	void FrustumCullingSystem::CountTotal(CullingUnit unit)
	{
		switch (unit)
		{
		case CullingUnit::Mesh:
			++totalMeshCount_;
			break;
		case CullingUnit::StageChunk:
			++totalStageChunkCount_;
			break;
		case CullingUnit::StageObject:
			++totalStageObjectCount_;
			[[fallthrough]];
		case CullingUnit::Object:
		default:
			++totalObjectCount_;
			break;
		}
	}

	void FrustumCullingSystem::CountDrawn(CullingUnit unit)
	{
		switch (unit)
		{
		case CullingUnit::Mesh:
			++drawnMeshCount_;
			break;
		case CullingUnit::StageChunk:
			++drawnStageChunkCount_;
			break;
		case CullingUnit::StageObject:
			++drawnStageObjectCount_;
			[[fallthrough]];
		case CullingUnit::Object:
		default:
			++drawnObjectCount_;
			break;
		}
	}

	void FrustumCullingSystem::CountCulled(CullingUnit unit)
	{
		switch (unit)
		{
		case CullingUnit::Mesh:
			++culledMeshCount_;
			break;
		case CullingUnit::StageChunk:
			++culledStageChunkCount_;
			break;
		case CullingUnit::StageObject:
			++culledStageObjectCount_;
			[[fallthrough]];
		case CullingUnit::Object:
		default:
			++culledObjectCount_;
			break;
		}
	}
}
