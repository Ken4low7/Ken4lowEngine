#include "StageChunk.h"

#include "Wireframe.h"

#include <algorithm>

namespace Ken4lowEngine
{
	StageChunk::StageChunk(int chunkId, const Vector3& center, const Vector3& size)
		: chunkId_(chunkId), center_(center), size_(size)
	{
		const Vector3 half = size_ * 0.5f;
		bounds_.min = center_ - half;
		bounds_.max = center_ + half;
	}

	void StageChunk::AddMesh(Object3D* object, size_t meshIndex)
	{
		if (!object) { return; }

		if (std::none_of(meshes_.begin(), meshes_.end(), [object](const StageChunkMeshRef& ref) { return ref.object == object; }))
		{
			++objectCount_;
		}

		meshes_.push_back({ object, meshIndex });
	}

	void StageChunk::ClearMeshes()
	{
		meshes_.clear();
		objectCount_ = 0;
	}

	void StageChunk::Draw() const
	{
		if (!visible_) { return; }

		Object3D* currentObject = nullptr;
		std::vector<size_t> meshIndices;

		const auto flush = [&]()
			{
				if (currentObject && !meshIndices.empty())
				{
					currentObject->DrawMeshes(meshIndices);
				}
			};

		for (const StageChunkMeshRef& ref : meshes_)
		{
			if (ref.object != currentObject)
			{
				flush();
				currentObject = ref.object;
				meshIndices.clear();
			}

			meshIndices.push_back(ref.meshIndex);
		}

		flush();
	}

	void StageChunk::DrawBoundsDebug(bool showBounds, bool showOccludedBounds) const
	{
		if (!showBounds && !(showOccludedBounds && occludedByOcclusion_)) { return; }

		Vector4 color = visible_ ? Vector4{ 0.1f, 0.9f, 0.2f, 1.0f } : Vector4{ 1.0f, 0.2f, 0.1f, 1.0f };
		if (occludedByOcclusion_)
		{
			color = Vector4{ 0.75f, 0.15f, 1.0f, 1.0f };
		}
		Wireframe::GetInstance()->DrawAABB(GetDebugAABB(), color);
	}

	AABB StageChunk::GetDebugAABB() const
	{
		AABB aabb{};
		aabb.min = bounds_.min;
		aabb.max = bounds_.max;
		return aabb;
	}
}
