#pragma once
#include "Matrix4x4.h"
#include "Sphere.h"
#include "Plane.h"
#include "Segment.h"
#include "Triangle.h"
#include "AABB.h"
#include "OBB.h"
#include "Capsule.h"
#include "Collider.h"

namespace Ken4lowEngine
{


	/// -------------------------------------------------------------
	///						衝突判定ユーティリティ
	/// -------------------------------------------------------------
	class CollisionUtility
	{
	public: /// ---------- メンバ関数 ---------- ///

		/*
		Primitive対応表:
		- Sphere: Sphere / Plane / AABB / OBB / Capsule
		- AABB: Point / AABB / Plane / Sphere / Segment / OBB / Capsule
		- OBB: Sphere / Segment / OBB / AABB
		- Capsule: Capsule / AABB / Sphere / Segment / Plane
		- Segment: Plane / Triangle / AABB / OBB / Capsule
		TODO: Capsule vs OBB、Mesh/Polygon Collider、複数Hit付きTraceは後続Phaseで扱う。
		*/

		// 球と球の衝突判定
		static bool IsCollision(const Sphere& s1, const Sphere& s2);

		// 球と平面の衝突判定
		static bool IsCollision(const Sphere& sphere, const Plane& plane);

		// 線分と平面の衝突判定
		static bool IsCollision(const Segment& segment, const Plane& plane);

		// 線分と三角形の衝突判定
		static bool IsCollision(const Triangle& triangle, const Segment& segment);

		// AABBと点の衝突判定
		static bool IsCollision(const AABB& aabb, const Vector3& point);

		// AABBとAABBの衝突判定
		static bool IsCollision(const AABB& aabb1, const AABB& aabb2);

		// AABBとPlaneの衝突判定
		static bool IsCollision(const AABB& aabb, const Plane& plane);

		// AABBと球の衝突判定
		static bool IsCollision(const AABB& aabb, const Sphere& sphere);

		// AABBと線分の衝突判定
		static bool IsCollision(const AABB& aabb, const Segment& segment);

		// OBBと球の衝突判定
		static bool IsCollision(const OBB& obb, const Sphere& sphere);

		// OBBと線分の衝突判定
		static bool IsCollision(const OBB& obb, const Segment& segment);

		// 線分とOBBの衝突判定
		static bool IsCollision(const Segment& segment, const OBB& obb) { return IsCollision(obb, segment); }

		// OBBとOBBの衝突判定
		static bool IsCollision(const OBB& obb1, const OBB& obb2);
		static bool IsCollision(const OBB& obb, const AABB& aabb);

		// CapsuleとCapsuleの衝突判定
		static bool IsCollision(const Capsule& capsule1, const Capsule& capsule2);

		// CapsuleとAABBの衝突判定
		static bool IsCollision(const AABB& aabb, const Capsule& capsule);
		static bool IsCollision(const Capsule& capsule, const AABB& aabb) { return IsCollision(aabb, capsule); }

		// CapsuleとSphereの衝突判定
		static bool IsCollision(const Capsule& capsule, const Sphere& sphere);
		static bool IsCollision(const Sphere& sphere, const Capsule& capsule) { return IsCollision(capsule, sphere); }

		// CapsuleとSegmentの衝突判定
		static bool IsCollision(const Capsule& capsule, const Segment& segment);
		static bool IsCollision(const Segment& segment, const Capsule& capsule) { return IsCollision(capsule, segment); }

		// CapsuleとPlaneの衝突判定
		static bool IsCollision(const Capsule& capsule, const Plane& plane);
		static bool IsCollision(const Plane& plane, const Capsule& capsule) { return IsCollision(capsule, plane); }
	};

	/// Primitive形状ペア名で呼べる薄いラッパー群。既存IsCollisionを残したままShapeType分岐への移行口にする。
	namespace CollisionPrimitiveTests
	{
		// Sphere同士の衝突判定。
		inline bool TestSphereSphere(const Sphere& a, const Sphere& b) { return CollisionUtility::IsCollision(a, b); }

		// SphereとPlaneの衝突判定。
		inline bool TestSpherePlane(const Sphere& sphere, const Plane& plane) { return CollisionUtility::IsCollision(sphere, plane); }

		// SphereとAABBの衝突判定。
		inline bool TestSphereAABB(const Sphere& sphere, const AABB& aabb) { return CollisionUtility::IsCollision(aabb, sphere); }

		// SphereとOBBの衝突判定。
		inline bool TestSphereOBB(const Sphere& sphere, const OBB& obb) { return CollisionUtility::IsCollision(obb, sphere); }

		// SphereとCapsuleの衝突判定。
		inline bool TestSphereCapsule(const Sphere& sphere, const Capsule& capsule) { return CollisionUtility::IsCollision(capsule, sphere); }

		// AABBと点の衝突判定。
		inline bool TestAABBPoint(const AABB& aabb, const Vector3& point) { return CollisionUtility::IsCollision(aabb, point); }

		// AABB同士の衝突判定。
		inline bool TestAABBAABB(const AABB& a, const AABB& b) { return CollisionUtility::IsCollision(a, b); }

		// AABBとPlaneの衝突判定。
		inline bool TestAABBPlane(const AABB& aabb, const Plane& plane) { return CollisionUtility::IsCollision(aabb, plane); }

		// AABBとSegmentの衝突判定。
		inline bool TestAABBSegment(const AABB& aabb, const Segment& segment) { return CollisionUtility::IsCollision(aabb, segment); }

		// AABBとOBBの衝突判定。
		inline bool TestAABBOBB(const AABB& aabb, const OBB& obb) { return CollisionUtility::IsCollision(obb, aabb); }

		// AABBとCapsuleの衝突判定。
		inline bool TestAABBCapsule(const AABB& aabb, const Capsule& capsule) { return CollisionUtility::IsCollision(aabb, capsule); }

		// OBBとSegmentの衝突判定。
		inline bool TestOBBSegment(const OBB& obb, const Segment& segment) { return CollisionUtility::IsCollision(obb, segment); }

		// OBB同士の衝突判定。
		inline bool TestOBBOBB(const OBB& a, const OBB& b) { return CollisionUtility::IsCollision(a, b); }

		// Capsule同士の衝突判定。
		inline bool TestCapsuleCapsule(const Capsule& a, const Capsule& b) { return CollisionUtility::IsCollision(a, b); }

		// CapsuleとSegmentの衝突判定。
		inline bool TestCapsuleSegment(const Capsule& capsule, const Segment& segment) { return CollisionUtility::IsCollision(capsule, segment); }

		// CapsuleとPlaneの衝突判定。
		inline bool TestCapsulePlane(const Capsule& capsule, const Plane& plane) { return CollisionUtility::IsCollision(capsule, plane); }

		// TODO: ECollisionShapeType同士のdispatch表を作る段階で、この名前付き入口から選択する。
		// TODO: Ray型は未定義のため、現段階ではSegmentをRay/Trace問い合わせの互換Primitiveとして扱う。

		// ShapeTypeから既存Primitive判定を選ぶ入口。未対応ペアは安全に非衝突として扱う。
		inline bool Test(const CollisionShapeInfo& a, const CollisionShapeInfo& b)
		{
			switch (a.shapeType)
			{
			case ECollisionShapeType::Sphere:
				switch (b.shapeType)
				{
				case ECollisionShapeType::Sphere:
					return TestSphereSphere(a.sphere, b.sphere);
				case ECollisionShapeType::AABB:
					return TestSphereAABB(a.sphere, b.BuildAABB());
				case ECollisionShapeType::OBB:
					return TestSphereOBB(a.sphere, b.BuildOBB());
				case ECollisionShapeType::Capsule:
					return TestSphereCapsule(a.sphere, b.capsule);
				default:
					return false;
				}

			case ECollisionShapeType::AABB:
				switch (b.shapeType)
				{
				case ECollisionShapeType::Sphere:
					return TestSphereAABB(b.sphere, a.BuildAABB());
				case ECollisionShapeType::AABB:
					return TestAABBAABB(a.BuildAABB(), b.BuildAABB());
				case ECollisionShapeType::OBB:
					return TestAABBOBB(a.BuildAABB(), b.BuildOBB());
				case ECollisionShapeType::Capsule:
					return TestAABBCapsule(a.BuildAABB(), b.capsule);
				case ECollisionShapeType::Segment:
					return TestAABBSegment(a.BuildAABB(), b.segment);
				default:
					return false;
				}

			case ECollisionShapeType::OBB:
				switch (b.shapeType)
				{
				case ECollisionShapeType::Sphere:
					return TestSphereOBB(b.sphere, a.BuildOBB());
				case ECollisionShapeType::AABB:
					return TestAABBOBB(b.BuildAABB(), a.BuildOBB());
				case ECollisionShapeType::OBB:
					return TestOBBOBB(a.BuildOBB(), b.BuildOBB());
				case ECollisionShapeType::Segment:
					return TestOBBSegment(a.BuildOBB(), b.segment);
				default:
					return false; // TODO: Capsule vs OBB は既存実装が無いため後続Phaseで対応する。
				}

			case ECollisionShapeType::Capsule:
				switch (b.shapeType)
				{
				case ECollisionShapeType::Sphere:
					return TestSphereCapsule(b.sphere, a.capsule);
				case ECollisionShapeType::AABB:
					return TestAABBCapsule(b.BuildAABB(), a.capsule);
				case ECollisionShapeType::Capsule:
					return TestCapsuleCapsule(a.capsule, b.capsule);
				case ECollisionShapeType::Segment:
					return TestCapsuleSegment(a.capsule, b.segment);
				default:
					return false; // TODO: Capsule vs OBB は既存実装が無いため後続Phaseで対応する。
				}

			case ECollisionShapeType::Segment:
				switch (b.shapeType)
				{
				case ECollisionShapeType::AABB:
					return TestAABBSegment(b.BuildAABB(), a.segment);
				case ECollisionShapeType::OBB:
					return TestOBBSegment(b.BuildOBB(), a.segment);
				case ECollisionShapeType::Capsule:
					return TestCapsuleSegment(b.capsule, a.segment);
				default:
					return false; // TODO: Segment vs Sphereなど未実装ペアは後続Phaseで必要性を確認する。
				}

			case ECollisionShapeType::None:
			default:
				return false;
			}
		}
	}

} // namespace Ken4lowEngine
