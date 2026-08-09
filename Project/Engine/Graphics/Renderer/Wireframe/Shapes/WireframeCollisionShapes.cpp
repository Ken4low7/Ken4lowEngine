// Wireframe のコリジョン確認用デバッグ形状をまとめる。
#include "Wireframe.h"
#include <cmath>
#include <numbers>

namespace Ken4lowEngine
{

	void Wireframe::DrawAABB(const AABB& aabb, const Vector4& color)
	{
		const Vector3 size = aabb.max - aabb.min;
		// 反転または厚み0のAABBは線キューブとして成立しないため登録しない。
		if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f)
		{
			return;
		}

		const Vector3 center = (aabb.min + aabb.max) * 0.5f;
		const Matrix4x4 world = Matrix4x4::MakeAffineMatrix(size, Vector3(0.0f, 0.0f, 0.0f), center);
		AddBoxWireInstance(world, color);
	}

	void Wireframe::DrawOBB(const OBB& obb, const Vector4& color)
	{
		const Vector3 fullSize = obb.size * 2.0f;
		if (fullSize.x <= 0.0f || fullSize.y <= 0.0f || fullSize.z <= 0.0f)
		{
			return;
		}

		// 既存の12本DrawLineをやめ、OBBの姿勢行列を作ってAABBと同じインスタンシング経路へ送る。
		Matrix4x4 world = Matrix4x4::MakeIdentity();
		world.m[0][0] = obb.orientations[0].x * fullSize.x;
		world.m[0][1] = obb.orientations[0].y * fullSize.x;
		world.m[0][2] = obb.orientations[0].z * fullSize.x;
		world.m[1][0] = obb.orientations[1].x * fullSize.y;
		world.m[1][1] = obb.orientations[1].y * fullSize.y;
		world.m[1][2] = obb.orientations[1].z * fullSize.y;
		world.m[2][0] = obb.orientations[2].x * fullSize.z;
		world.m[2][1] = obb.orientations[2].y * fullSize.z;
		world.m[2][2] = obb.orientations[2].z * fullSize.z;
		world.m[3][0] = obb.center.x;
		world.m[3][1] = obb.center.y;
		world.m[3][2] = obb.center.z;
		world.m[3][3] = 1.0f;
		AddBoxWireInstance(world, color);
	}

	void Wireframe::DrawSphere(const Vector3& center, const float radius, const Vector4& color)
	{
		if (radius <= 0.0f)
		{
			return;
		}

		// 既存のDrawLine大量呼び出しをやめ、単位球ワイヤーメッシュを共有してworld行列と色だけを送る。
		const Vector3 scale(radius, radius, radius);
		const Matrix4x4 world = Matrix4x4::MakeAffineMatrix(scale, Vector3(0.0f, 0.0f, 0.0f), center);
		AddSphereWireInstance(world, color);
	}

	void Wireframe::DrawSphere(const Sphere& sphere, const Vector4& color)
	{
		DrawSphere(sphere.center, sphere.radius, color);
	}

	void Wireframe::DrawCapsule(const Vector3& center, float radius, float height, const Vector3& axis, uint32_t segments, const Vector4& color)
	{
		const float axisLength = Vector3::Length(axis);
		if (radius <= 0.0f || height <= 0.0f || axisLength <= 0.000001f)
		{
			return;
		}

		// 共有メッシュは固定16分割のため、呼び出し側のsegmentsは互換性維持のためだけに受け取る。
		(void)segments;
		const Vector3 up = Vector3::Multiply(1.0f / axisLength, axis);
		const Vector3 reference = std::fabs(up.z) < 0.999f ? Vector3(0.0f, 0.0f, 1.0f) : Vector3(1.0f, 0.0f, 0.0f);
		const Vector3 right = Vector3::Normalize(Vector3::Cross(up, reference));
		const Vector3 forward = Vector3::Normalize(Vector3::Cross(right, up));

		const float cylinderHeight = height > radius * 2.0f ? height - radius * 2.0f : 0.0f;
		const float halfCylinderHeight = cylinderHeight * 0.5f;
		// 単一のworld行列だけで描くため、Yは全高を保つ簡易スケールとし、X/Z半径は正確に維持する。
		const float yScale = (halfCylinderHeight + radius) * 0.5f;

		// Y軸基準の単位Capsuleをaxis方向へ向ける直交基底を、エンジンの行ベクトル規約で組み立てる。
		Matrix4x4 world = Matrix4x4::MakeIdentity();
		world.m[0][0] = right.x * radius;
		world.m[0][1] = right.y * radius;
		world.m[0][2] = right.z * radius;
		world.m[1][0] = up.x * yScale;
		world.m[1][1] = up.y * yScale;
		world.m[1][2] = up.z * yScale;
		world.m[2][0] = forward.x * radius;
		world.m[2][1] = forward.y * radius;
		world.m[2][2] = forward.z * radius;
		world.m[3][0] = center.x;
		world.m[3][1] = center.y;
		world.m[3][2] = center.z;
		world.m[3][3] = 1.0f;

		// 既存のDrawLine大量呼び出しをやめ、world行列と色だけをインスタンシング経路へ送る。
		AddCapsuleWireInstance(world, color);
	}

	void Wireframe::DrawCapsule(const Capsule& capsule, const Vector4& color)
	{
		const Vector3& start = capsule.segment.origin;
		const Vector3& end = capsule.segment.origin + capsule.segment.diff;
		float radius = capsule.radius;
		Vector3 center = (start + end) * 0.5f;
		// 高さは radius × 2 加算した「全体の高さ」
		float height = Vector3::Length(end - start) + radius * 2.0f;
		Vector3 axis = Vector3::Normalize(end - start);
		DrawCapsule(center, radius, height, axis, 8, color);
	}

	void Wireframe::DrawPlane(const Plane& plane, float size, const Vector4& color)
	{
		// Plane の法線と距離から原点位置を算出
		Vector3 center = plane.normal * plane.distance;
		// Planeの法線に垂直な2軸を求める
		Vector3 right = Vector3::Cross({ 0, 1, 0 }, plane.normal);
		if (Vector3::Length(right) < 0.001f) {
			right = Vector3::Cross({ 1, 0, 0 }, plane.normal);
		}
		right = Vector3::Normalize(right);
		Vector3 forward = Vector3::Normalize(Vector3::Cross(plane.normal, right));
		// 平面上の4頂点を作成（正方形として描画）
		Vector3 halfRight = right * (size * 0.5f);
		Vector3 halfForward = forward * (size * 0.5f);
		Vector3 p0 = center - halfRight - halfForward;
		Vector3 p1 = center + halfRight - halfForward;
		Vector3 p2 = center + halfRight + halfForward;
		Vector3 p3 = center - halfRight + halfForward;
		// 線で囲む
		DrawLine(p0, p1, color);
		DrawLine(p1, p2, color);
		DrawLine(p2, p3, color);
		DrawLine(p3, p0, color);
		// 法線の可視化（上向きの線）
		DrawLine(center, center + plane.normal * (size * 0.25f), { 1, 0, 0, 1 }); // 赤で表示
	}

	void Wireframe::DrawCylinder(const Vector3& baseCenter, float radius, float height, const Vector3& axis, uint32_t segmentCount, const Vector4& color)
	{
		constexpr float PI = std::numbers::pi_v<float>;
		float angleStep = (2.0f * PI) / float(segmentCount);
		std::vector<Vector3> topVertices(segmentCount);
		std::vector<Vector3> bottomVertices(segmentCount);
		// 円の基準軸を決める（axis に直交する2つのベクトルを求める）
		Vector3 up = Vector3::Normalize(axis);
		Vector3 right = Vector3(1.0f, 0.0f, 0.0f);
		if (std::fabs(Vector3::Dot(up, right)) > 0.99f) {
			right = Vector3(0.0f, 1.0f, 0.0f);
		}
		Vector3 forward = Vector3::Normalize(Vector3::Cross(up, right));
		right = Vector3::Normalize(Vector3::Cross(forward, up));
		// 上面・下面の円の頂点を計算
		for (uint32_t i = 0; i < segmentCount; i++) {
			float angle = angleStep * i;
			Vector3 offset = Vector3::Add(Vector3::Multiply(radius * cosf(angle), right),
				Vector3::Multiply(radius * sinf(angle), forward));
			bottomVertices[i] = Vector3::Add(baseCenter, offset);
			topVertices[i] = Vector3::Add(bottomVertices[i], Vector3::Multiply(height, up));
		}
		// 上面の円を描画
		for (uint32_t i = 0; i < segmentCount; i++) {
			DrawLine(topVertices[i], topVertices[(i + 1) % segmentCount], color);
		}
		// 下面の円を描画
		for (uint32_t i = 0; i < segmentCount; i++) {
			DrawLine(bottomVertices[i], bottomVertices[(i + 1) % segmentCount], color);
		}
		// 上面と下面をつなぐ側面の線を描画
		for (uint32_t i = 0; i < segmentCount; i++) {
			DrawLine(bottomVertices[i], topVertices[i], color);
		}
	}

} // namespace Ken4lowEngine
