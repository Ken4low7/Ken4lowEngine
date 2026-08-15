#include "GpuFluidColliderObstacleAdapter.h"

#include "ColliderComponent.h"
#include "../Core/Actor.h"
#include "../Core/ActorWorld.h"

namespace Ken4lowEngine
{

bool GpuFluidColliderObstacleAdapter::BuildSource(
	const ColliderComponent& component,
	GpuFluidObstacleSource& outSource)
{
	outSource = {};
	const Collider* collider = component.GetCollider();
	if (collider == nullptr ||
		!component.IsActiveInHierarchy() ||
		!collider->IsCollisionEnabledForPhysics())
	{
		return false;
	}

	switch (collider->GetShapeType())
	{
	case ECollisionShapeType::Sphere:
	{
		const Sphere sphere = collider->GetSphere();
		outSource.shape = GpuFluidObstacleShape::Sphere;
		outSource.worldCenter = sphere.center;
		outSource.radius = sphere.radius;
		break;
	}
	case ECollisionShapeType::AABB:
	{
		const AABB aabb = collider->GetAABB();
		outSource.shape = GpuFluidObstacleShape::Box;
		outSource.worldCenter = (aabb.min + aabb.max) * 0.5f;
		outSource.halfSize = (aabb.max - aabb.min) * 0.5f;
		outSource.axisX = { 1.0f, 0.0f, 0.0f };
		outSource.axisY = { 0.0f, 1.0f, 0.0f };
		outSource.axisZ = { 0.0f, 0.0f, 1.0f };
		break;
	}
	case ECollisionShapeType::OBB:
	{
		const OBB obb = collider->GetOBB();
		outSource.shape = GpuFluidObstacleShape::Box;
		outSource.worldCenter = obb.center;
		outSource.halfSize = obb.size;
		outSource.axisX = obb.orientations[0];
		outSource.axisY = obb.orientations[1];
		outSource.axisZ = obb.orientations[2];
		break;
	}
	case ECollisionShapeType::Capsule:
	case ECollisionShapeType::Segment:
	case ECollisionShapeType::None:
	default:
		return false; // Phase16.8は既存ColliderComponentで実体を持つSphere/AABB/OBBからObstacle化する。
	}

	outSource.enabled = true;
	return outSource.IsValid();
}

void GpuFluidColliderObstacleAdapter::CollectSources(
	const ActorWorld& world,
	std::vector<GpuFluidObstacleSource>& outSources)
{
	outSources.clear();

	for (const auto& actor : world.GetActors())
	{
		if (!actor || actor->IsPendingDestroy())
		{
			continue;
		}

		for (const auto& component : actor->GetComponents())
		{
			const auto* colliderComponent = dynamic_cast<const ColliderComponent*>(component.get());
			if (colliderComponent == nullptr)
			{
				continue;
			}

			GpuFluidObstacleSource source{};
			if (BuildSource(*colliderComponent, source))
			{
				outSources.push_back(source); // PhysicsでBlockするColliderだけをFluidのSolid Cell候補へ追加する。
			}
		}
	}
}

} // namespace Ken4lowEngine
