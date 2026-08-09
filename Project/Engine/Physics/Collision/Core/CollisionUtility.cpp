#include "CollisionUtility.h"

#include <algorithm>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///						球と球の衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const Sphere& s1, const Sphere& s2)
	{
		//2つの球の中心点間の距離を求める
		float distance = Vector3::Length(Vector3::Subtract(s2.center, s1.center));

		// 半径の合計よりも短ければ衝突
		return distance <= (s1.radius + s2.radius);
	}

	/// -------------------------------------------------------------
	///						球と平面の衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const Sphere& sphere, const Plane& plane)
	{
		// 平面の法線ベクトルと球の中心点との距離
		float distance = Vector3::Dot(plane.normal, sphere.center) - plane.distance;

		// その距離が球の半径以下なら衝突している
		return fabs(distance) <= sphere.radius;
	}

	/// -------------------------------------------------------------
	///						線分と平面の衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const Segment& segment, const Plane& plane)
	{
		// まず垂直判定を行うために、法線と線の内積を求める
		float dot = Vector3::Dot(plane.normal, segment.diff);

		// 垂直 = 平行であるので、衝突しているはずがない
		// 浮動小数点数の比較は通常、直接の等号判定は避ける
		const float epsilon = 1e-6f;

		// 平行判定
		if (fabs(dot) < epsilon) return false; // 平行なら衝突しない

		// tを求める
		float t = (plane.distance - Vector3::Dot(segment.origin, plane.normal)) / dot;

		// tの値と線の種類によって衝突しているかを判断する
		return t >= 0.0f && t <= 1.0f;
	}

	/// -------------------------------------------------------------
	///						線分と三角形の衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const Triangle& triangle, const Segment& segment)
	{
		// 三角形の辺
		Vector3 edge1 = Vector3::Subtract(triangle.vertices[1], triangle.vertices[0]);
		Vector3 edge2 = Vector3::Subtract(triangle.vertices[2], triangle.vertices[0]);

		// 平面の法線ベクトルを計算
		Vector3 normal = Vector3::Cross(edge1, edge2);
		normal = Vector3::Normalize(normal);

		// 線分の方向ベクトル
		Vector3 dir = segment.diff;
		dir = Vector3::Normalize(dir);

		// 平面と線分の始点のベクトル
		Vector3 diff = Vector3::Subtract(triangle.vertices[0], segment.origin);

		// 線分が平面と平行かどうかをチェック
		float dotND = Vector3::Dot(normal, dir);
		if (fabs(dotND) < 1e-6f) return false; // 線分が平面と平行


		// 線分の始点と平面の交点を計算
		float t = Vector3::Dot(normal, diff) / dotND;

		// tの値が0から線分の長さの範囲内にあるかをチェック
		if (t < 0.0f || t > Vector3::Length(segment.diff)) return false; // 線分上に交点がない

		// 交点の座標を計算
		Vector3 intersection = Vector3::Add(segment.origin, Vector3::Multiply(t, dir));

		// バリツチェックで三角形の内部に交点があるかを確認
		Vector3 c0 = Vector3::Cross(Vector3::Subtract(triangle.vertices[1], triangle.vertices[0]), Vector3::Subtract(intersection, triangle.vertices[0]));
		Vector3 c1 = Vector3::Cross(Vector3::Subtract(triangle.vertices[2], triangle.vertices[1]), Vector3::Subtract(intersection, triangle.vertices[1]));
		Vector3 c2 = Vector3::Cross(Vector3::Subtract(triangle.vertices[0], triangle.vertices[2]), Vector3::Subtract(intersection, triangle.vertices[2]));

		// すべてのクロス積が法線と同じ方向を向いているかをチェック
		if (Vector3::Dot(c0, normal) >= 0.0f && Vector3::Dot(c1, normal) >= 0.0f && Vector3::Dot(c2, normal) >= 0.0f)
		{
			return true; // 衝突
		}

		return false; // 衝突なし
	}

	/// -------------------------------------------------------------
	///						AABBと点の衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const AABB& aabb, const Vector3& point)
	{
		// 点がAABBの範囲内にあるかチェック
		return(
			point.x >= aabb.min.x && point.x <= aabb.max.x && // x軸
			point.y >= aabb.min.y && point.y <= aabb.max.y && // y軸
			point.z >= aabb.min.z && point.z <= aabb.max.z);  // z軸
	}

	/// -------------------------------------------------------------
	///						AABBとAABBの衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const AABB& aabb1, const AABB& aabb2)
	{
		return
			(aabb1.min.x <= aabb2.max.x && aabb1.max.x >= aabb2.min.x) && // x軸
			(aabb1.min.y <= aabb2.max.y && aabb1.max.y >= aabb2.min.y) && // y軸
			(aabb1.min.z <= aabb2.max.z && aabb1.max.z >= aabb2.min.z);	  // z軸
	}

	/// -------------------------------------------------------------
	///						AABBとPlaneの衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const AABB& aabb, const Plane& plane)
	{
		// AABBの8点を調べる
		const Vector3 corners[8] = {
			{ aabb.min.x, aabb.min.y, aabb.min.z }, // 0 : 左下奥
			{ aabb.max.x, aabb.min.y, aabb.min.z }, // 1 : 右下奥
			{ aabb.min.x, aabb.max.y, aabb.min.z }, // 2 ; 左上奥
			{ aabb.max.x, aabb.max.y, aabb.min.z }, // 3 : 右上奥
			{ aabb.min.x, aabb.min.y, aabb.max.z },	// 4 : 左下手前
			{ aabb.max.x, aabb.min.y, aabb.max.z }, // 5 : 右下手前
			{ aabb.min.x, aabb.max.y, aabb.max.z }, // 6 : 左上手前
			{ aabb.max.x, aabb.max.y, aabb.max.z }  // 7 : 右上手前
		};

		// 少なくとも1点がPlaneの負側にあるか
		for (const auto& corner : corners)
		{
			// その点とPlaneの距離を計算
			float distance = Vector3::Dot(plane.normal, corner) - plane.distance;

			// 負側にあるかチェック
			if (distance < 0.0f) return true; // 交差している
		}
		return false;
	}

	/// -------------------------------------------------------------
	///						AABBと球の衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const AABB& aabb, const Sphere& sphere)
	{
		//最近接点を求める
		Vector3 clossestPoint
		{
			std::clamp(sphere.center.x, aabb.min.x, aabb.max.x), // x座標
			std::clamp(sphere.center.y, aabb.min.y, aabb.max.y), // y座標
			std::clamp(sphere.center.z, aabb.min.z, aabb.max.z)  // z座標
		};
		//最近接点と球の中途の距離を求める
		float distance = Vector3::Length(Vector3::Subtract(clossestPoint, sphere.center));

		//距離が半径よりも小さければ衝突
		return distance <= sphere.radius;
	}

	/// -------------------------------------------------------------
	///						AABBと線分の衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const AABB& aabb, const Segment& segment)
	{
		// 3D Slab 法（線分: P(t)=origin + diff*t, t∈[0,1]）
		// ※ diff の各成分が 0 の場合（軸平行）でも安全に判定できるようにする
		const float EPS = 1e-6f;

		float tmin = 0.0f;
		float tmax = 1.0f;

		auto slab = [&](float origin, float dir, float minv, float maxv) -> bool
			{
				if (fabsf(dir) < EPS)
				{
					// その軸方向に動かない：始点が範囲外なら絶対に交差しない
					return (origin >= minv && origin <= maxv);
				}

				const float inv = 1.0f / dir;
				float t1 = (minv - origin) * inv;
				float t2 = (maxv - origin) * inv;
				if (t1 > t2) std::swap(t1, t2);

				tmin = std::max(tmin, t1);
				tmax = std::min(tmax, t2);
				return tmin <= tmax;
			};

		if (!slab(segment.origin.x, segment.diff.x, aabb.min.x, aabb.max.x)) return false;
		if (!slab(segment.origin.y, segment.diff.y, aabb.min.y, aabb.max.y)) return false;
		if (!slab(segment.origin.z, segment.diff.z, aabb.min.z, aabb.max.z)) return false;

		return true;
	}


	/// -------------------------------------------------------------
	///						OBBと球の衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const OBB& obb, const Sphere& sphere)
	{
		// OBBのローカル空間に球の中心を変換
		Matrix4x4 obbWorldMatrix = Matrix4x4::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, obb.center);
		Matrix4x4 obbRotationMatrix = Matrix4x4::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, {}, Vector3());
		obbRotationMatrix.m[0][0] = obb.orientations[0].x;
		obbRotationMatrix.m[0][1] = obb.orientations[0].y;
		obbRotationMatrix.m[0][2] = obb.orientations[0].z;

		obbRotationMatrix.m[1][0] = obb.orientations[1].x;
		obbRotationMatrix.m[1][1] = obb.orientations[1].y;
		obbRotationMatrix.m[1][2] = obb.orientations[1].z;

		obbRotationMatrix.m[2][0] = obb.orientations[2].x;
		obbRotationMatrix.m[2][1] = obb.orientations[2].y;
		obbRotationMatrix.m[2][2] = obb.orientations[2].z;

		obbWorldMatrix = Matrix4x4::Multiply(obbWorldMatrix, obbRotationMatrix);
		Matrix4x4 obbWorldMatrixInverse = Matrix4x4::Inverse(obbWorldMatrix);
		Vector3 centerInOBBLocalSpace = Vector3::Transform(sphere.center, obbWorldMatrixInverse);

		// 球の中心点からOBBの各軸に対して最近接点を求める
		Vector3 closestPoint = centerInOBBLocalSpace;
		closestPoint.x = std::max(-obb.size.x * 0.5f, std::min(closestPoint.x, obb.size.x * 0.5f));
		closestPoint.y = std::max(-obb.size.y * 0.5f, std::min(closestPoint.y, obb.size.y * 0.5f));
		closestPoint.z = std::max(-obb.size.z * 0.5f, std::min(closestPoint.z, obb.size.z * 0.5f));

		// OBBのローカル空間での球の中心点と最近接点の距離を計算
		Vector3 difference = centerInOBBLocalSpace - closestPoint;
		float distanceSquared = Vector3::Dot(difference, difference);

		// 衝突判定
		return distanceSquared <= sphere.radius * sphere.radius;
	}

	/// -------------------------------------------------------------
	///						OBBと線分の衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const OBB& obb, const Segment& segment)
	{
		// OBBの回転行列を作成
		Matrix4x4 rotationMatrix = {
			obb.orientations[0].x, obb.orientations[0].y, obb.orientations[0].z, 0.0f,
			obb.orientations[1].x, obb.orientations[1].y, obb.orientations[1].z, 0.0f,
			obb.orientations[2].x, obb.orientations[2].y, obb.orientations[2].z, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};

		// OBBの逆変換行列（回転の転置行列と平行移動の逆変換）を作成
		Matrix4x4 obbWorldMatrixInverse = Matrix4x4::Inverse(rotationMatrix);
		obbWorldMatrixInverse.m[3][0] = -(obb.center.x * obbWorldMatrixInverse.m[0][0] + obb.center.y * obbWorldMatrixInverse.m[1][0] + obb.center.z * obbWorldMatrixInverse.m[2][0]);
		obbWorldMatrixInverse.m[3][1] = -(obb.center.x * obbWorldMatrixInverse.m[0][1] + obb.center.y * obbWorldMatrixInverse.m[1][1] + obb.center.z * obbWorldMatrixInverse.m[2][1]);
		obbWorldMatrixInverse.m[3][2] = -(obb.center.x * obbWorldMatrixInverse.m[0][2] + obb.center.y * obbWorldMatrixInverse.m[1][2] + obb.center.z * obbWorldMatrixInverse.m[2][2]);

		// セグメントの始点と終点をOBBのローカル空間に変換
		Vector3 localOrigin = Vector3::Transform(segment.origin, obbWorldMatrixInverse);
		Vector3 localEnd = Vector3::Transform(segment.origin + segment.diff, obbWorldMatrixInverse);

		// 変換後のセグメント
		Segment localSegment;
		localSegment.origin = localOrigin;
		localSegment.diff = localEnd - localOrigin;

		// OBBのローカル空間でAABBとの衝突判定を行う
		AABB aabbOBBLocal{ -obb.size, obb.size };
		return IsCollision(aabbOBBLocal, localSegment);
	}

	/// -------------------------------------------------------------
	///						OBBとOBBの衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const OBB& obb1, const OBB& obb2)
	{
		const float epsilon = 1e-5f;

		// OBBの軸
		Vector3 axes[15] = {
			obb1.orientations[0],
			obb1.orientations[1],
			obb1.orientations[2],
			obb2.orientations[0],
			obb2.orientations[1],
			obb2.orientations[2],
			Vector3::Cross(obb1.orientations[0], obb2.orientations[0]),
			Vector3::Cross(obb1.orientations[0], obb2.orientations[1]),
			Vector3::Cross(obb1.orientations[0], obb2.orientations[2]),
			Vector3::Cross(obb1.orientations[1], obb2.orientations[0]),
			Vector3::Cross(obb1.orientations[1], obb2.orientations[1]),
			Vector3::Cross(obb1.orientations[1], obb2.orientations[2]),
			Vector3::Cross(obb1.orientations[2], obb2.orientations[0]),
			Vector3::Cross(obb1.orientations[2], obb2.orientations[1]),
			Vector3::Cross(obb1.orientations[2], obb2.orientations[2]),
		};

		// 各軸に対して分離が存在するかをチェック
		for (int i = 0; i < 15; ++i)
		{
			if (Vector3::Length(axes[i]) < epsilon)	continue; // 無視できる軸

			// 軸を正規化
			Vector3 axis = Vector3::Normalize(axes[i]);

			// OBB1の投影範囲
			float center1 = Vector3::Dot(obb1.center, axis);
			float extent1 =
				std::abs(Vector3::Dot(obb1.orientations[0] * obb1.size.x, axis)) +
				std::abs(Vector3::Dot(obb1.orientations[1] * obb1.size.y, axis)) +
				std::abs(Vector3::Dot(obb1.orientations[2] * obb1.size.z, axis));

			float min1 = center1 - extent1; // 投影範囲の最小値
			float max1 = center1 + extent1; // 投影範囲の最大値

			// OBB2の投影範囲
			float center2 = Vector3::Dot(obb2.center, axis);
			float extent2 =
				std::abs(Vector3::Dot(obb2.orientations[0] * obb2.size.x, axis)) +
				std::abs(Vector3::Dot(obb2.orientations[1] * obb2.size.y, axis)) +
				std::abs(Vector3::Dot(obb2.orientations[2] * obb2.size.z, axis));

			float min2 = center2 - extent2; // 投影範囲の最小値
			float max2 = center2 + extent2; // 投影範囲の最大値

			// 投影範囲が重ならなければ分離している
			if (max1 < min2 || max2 < min1)	return false; // 分離軸が存在する
		}

		// すべての軸で分離がなければ衝突している
		return true;
	}

	bool CollisionUtility::IsCollision(const OBB& obb, const AABB& aabb)
	{
		// AABBを単位軸OBBへ変換し、両形状の全分離軸を使う正式なSAT判定へ統一する。
		OBB aabbAsOBB{};
		aabbAsOBB.center = (aabb.min + aabb.max) * 0.5f;
		aabbAsOBB.size = (aabb.max - aabb.min) * 0.5f;
		aabbAsOBB.orientations[0] = { 1.0f, 0.0f, 0.0f };
		aabbAsOBB.orientations[1] = { 0.0f, 1.0f, 0.0f };
		aabbAsOBB.orientations[2] = { 0.0f, 0.0f, 1.0f };
		return IsCollision(obb, aabbAsOBB);
	}

	/// -------------------------------------------------------------
	///						Capsule–Capsule 衝突判定補助
	/// -------------------------------------------------------------
	static float SegmentSegmentDist2(const Vector3& P0, const Vector3& P1, const Vector3& Q0, const Vector3& Q1)
	{
		// 線分P0P1と線分Q0Q1の最近接距離の2乗を求める
		const Vector3  u = P1 - P0;
		const Vector3  v = Q1 - Q0;
		const Vector3  w = P0 - Q0;
		const float    a = Vector3::Dot(u, u);
		const float    b = Vector3::Dot(u, v);
		const float    c = Vector3::Dot(v, v);
		const float    d = Vector3::Dot(u, w);
		const float    e = Vector3::Dot(v, w);
		const float    D = a * c - b * b;
		const float    EPS = 1e-6f;

		// パラメータ s, t の分子と分母
		float sN, sD = D, tN, tD = D;

		// 線分がほぼ平行
		if (D < EPS) { sN = 0; sD = 1; tN = e; tD = c; }
		else
		{
			sN = (b * e - c * d);
			tN = (a * e - b * d);
			if (sN < 0) { sN = 0;  tN = e;        tD = c; }
			else if (sN > sD) { sN = sD; tN = e + b; tD = c; }
		}

		// t を [0,1] にクランプ
		if (tN < 0)
		{
			tN = 0;
			if (-d < 0)   sN = 0;
			else if (-d > a)   sN = sD;
			else { sN = -d; sD = a; }
		}
		else if (tN > tD)
		{
			tN = tD;
			if ((-d + b) < 0)      sN = 0;
			else if ((-d + b) > a) sN = sD;
			else { sN = -d + b; sD = a; }
		}

		// パラメータの計算
		const float sc = (fabsf(sN) < EPS ? 0.0f : sN / sD);
		const float tc = (fabsf(tN) < EPS ? 0.0f : tN / tD);
		const Vector3  dP = w + u * sc - v * tc;

		return Vector3::Dot(dP, dP);   // 距離²
	}

	/// -------------------------------------------------------------
	///						Capsule–Capsule 衝突判定補助
	/// -------------------------------------------------------------
	inline bool IsCapsuleCapsuleHit(const Capsule& c1, const Capsule& c2)
	{
		// 中心線同士の最近接距離²を取得（Segment.diff は差分ベクトル想定）
		const Vector3 p0 = c1.segment.origin;
		const Vector3 p1 = c1.segment.origin + c1.segment.diff;
		const Vector3 q0 = c2.segment.origin;
		const Vector3 q1 = c2.segment.origin + c2.segment.diff;

		const float dist2 = SegmentSegmentDist2(p0, p1, q0, q1);
		const float rSum = c1.radius + c2.radius;
		return dist2 <= rSum * rSum + 1e-6f;   // EPS で数値誤差吸収
	}

	/// -------------------------------------------------------------
	///					 Capsule–Capsule 衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const Capsule& cap1, const Capsule& cap2)
	{
		// 中心線同士の最近接距離²を取得（Segment.diff は差分ベクトル想定）
		const Vector3 p0 = cap1.segment.origin;
		const Vector3 p1 = cap1.segment.origin + cap1.segment.diff;
		const Vector3 q0 = cap2.segment.origin;
		const Vector3 q1 = cap2.segment.origin + cap2.segment.diff;

		const float dist2 = SegmentSegmentDist2(p0, p1, q0, q1);

		// しきい値 = 半径の和 の²
		const float rSum = cap1.radius + cap2.radius;
		return dist2 <= rSum * rSum + 1e-6f;    // EPS で誤差吸収
	}

	/// -------------------------------------------------------------
	///					 Capsule–AABB 衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const AABB& aabb, const Capsule& capsule)
	{
		const Vector3& p0 = capsule.segment.origin;
		const Vector3  p1 = capsule.segment.origin + capsule.segment.diff;

		// 線分とAABBの最近接点を求める（AABB内にクランプ）
		Vector3 segDir = p1 - p0;
		float segLenSq = Vector3::Dot(segDir, segDir);
		float t = 0.0f;
		if (segLenSq > 1e-6f) {
			// AABBに最も近い位置にtをクランプ
			Vector3 boxCenter = (aabb.min + aabb.max) * 0.5f;
			t = Vector3::Dot(boxCenter - p0, segDir) / segLenSq;
			t = std::clamp(t, 0.0f, 1.0f);
		}

		Vector3 closestOnSegment = p0 + segDir * t;

		// AABB内の最近接点を求める
		Vector3 closestInAABB = {
			std::clamp(closestOnSegment.x, aabb.min.x, aabb.max.x),
			std::clamp(closestOnSegment.y, aabb.min.y, aabb.max.y),
			std::clamp(closestOnSegment.z, aabb.min.z, aabb.max.z),
		};

		// 2点間の距離の2乗
		Vector3 diff = closestOnSegment - closestInAABB;
		float distSq = Vector3::Dot(diff, diff);

		// 半径を考慮して判定
		return distSq <= (capsule.radius * capsule.radius) + 1e-6f;
	}

	/// -------------------------------------------------------------
	///					 Capsule–Sphere 衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const Capsule& capsule, const Sphere& sphere)
	{
		// カプセルの軸線を A-B とする（Segment.diff は「終点ではなく差分ベクトル」として扱う）
		const Vector3 A = capsule.segment.origin;
		const Vector3 B = capsule.segment.origin + capsule.segment.diff;
		const Vector3 C = sphere.center;  // 球の中心

		Vector3 AB = B - A;
		const float ab2 = Vector3::Dot(AB, AB);

		// A-B 上に C に最も近い点 P を求める（線分と点の最近接点）
		float t = 0.0f;
		if (ab2 > 1e-6f)
		{
			t = Vector3::Dot(C - A, AB) / ab2;
			t = std::clamp(t, 0.0f, 1.0f);
		}
		Vector3 P = A + AB * t;

		// P-C 間の距離²
		float dist2 = Vector3::Dot(P - C, P - C);

		// 半径の和²と比較
		float rSum = capsule.radius + sphere.radius;
		return dist2 <= rSum * rSum + 1e-6f;
	}

	/// -------------------------------------------------------------
	///					 Capsule–Segment 衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const Capsule& capsule, const Segment& seg)
	{
		// seg の 2 端点を生成
		const Vector3 p0 = seg.origin;
		const Vector3 p1 = seg.origin + seg.diff;    // diff を足して終点を導出

		// capsule の軸線 [A,B]（diff は差分ベクトル想定）
		const Vector3 A = capsule.segment.origin;
		const Vector3 B = capsule.segment.origin + capsule.segment.diff;

		// 軸線 [A,B] と 線分 [p0,p1] の最近接距離²
		const float dist2 = SegmentSegmentDist2(A, B, p0, p1);

		// 半径² と比較
		const float r2 = capsule.radius * capsule.radius;
		const float EPS = 1e-6f;                     // 浮動小数誤差吸収
		return dist2 <= r2 + EPS;
	}

	/// -------------------------------------------------------------
	///					 Capsule–Plane 衝突判定
	/// -------------------------------------------------------------
	bool CollisionUtility::IsCollision(const Capsule& capsule, const Plane& plane)
	{
		// カプセルの線分の両端点
		const Vector3& p0 = capsule.segment.origin;
		const Vector3& p1 = capsule.segment.origin + capsule.segment.diff;

		// 線分上の点の数（離散化）
		const int kSteps = 10;
		for (int i = 0; i <= kSteps; ++i)
		{
			float t = static_cast<float>(i) / static_cast<float>(kSteps);
			Vector3 point = p0 + (p1 - p0) * t;

			// 点とPlaneとの距離
			float distance = Vector3::Dot(plane.normal, point) - plane.distance;

			if (fabs(distance) <= capsule.radius) return true;
		}
		return false;
	}

} // namespace Ken4lowEngine
