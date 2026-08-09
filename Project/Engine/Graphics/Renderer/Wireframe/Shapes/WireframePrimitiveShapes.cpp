// Wireframe の多面体や特殊プリミティブ形状をまとめる。
#include "Wireframe.h"
#include <cmath>
#include <numbers>

namespace Ken4lowEngine
{

	void Wireframe::DrawTetrahedron(const Vector3& baseCenter, float baseSize, float height, const Vector3& axis, const Vector4& color)
	{
		// 底面の外接円の半径
		float radius = baseSize / sqrtf(3.0f); // 正三角形の外接円半径
		// 基準軸（axis）を正規化
		Vector3 up = Vector3::Normalize(axis);
		Vector3 right = Vector3(1.0f, 0.0f, 0.0f);
		// upと直交する軸を求める
		if (std::fabs(Vector3::Dot(up, right)) > 0.99f) {
			right = Vector3(0.0f, 1.0f, 0.0f);
		}
		Vector3 forward = Vector3::Normalize(Vector3::Cross(up, right));
		right = Vector3::Normalize(Vector3::Cross(forward, up));
		// 底面の3頂点を計算（正三角形）
		Vector3 bottomVertices[3] = {
			Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(radius * cosf(0.0f), right), Vector3::Multiply(radius * sinf(0.0f), forward))), // 頂点1
			Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(radius * cosf(2.0f * std::numbers::pi_v<float> / 3.0f), right),
				Vector3::Multiply(radius * sinf(2.0f * std::numbers::pi_v<float> / 3.0f), forward))), // 頂点2
				Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(radius * cosf(4.0f * std::numbers::pi_v<float> / 3.0f), right),
					Vector3::Multiply(radius * sinf(4.0f * std::numbers::pi_v<float> / 3.0f), forward)))  // 頂点3
		};
		// 頂点（ピラミッドの先端）を計算
		Vector3 topVertex = Vector3::Add(baseCenter, Vector3::Multiply(height, up));
		// 底面の3辺を描画
		for (int i = 0; i < 3; i++) {
			DrawLine(bottomVertices[i], bottomVertices[(i + 1) % 3], color);
		}
		// 側面の3辺を描画
		for (int i = 0; i < 3; i++) {
			DrawLine(bottomVertices[i], topVertex, color);
		}
	}

	void Wireframe::DrawPyramid(const Vector3& baseCenter, float baseSize, float height, const Vector3& axis, const Vector4& color)
	{
		// ピラミッドの底面の半径
		float halfSize = baseSize * 0.5f;
		// 基準軸（axis）を正規化
		Vector3 up = Vector3::Normalize(axis);
		Vector3 right = Vector3(1.0f, 0.0f, 0.0f);
		// upと直交する軸を求める
		if (std::fabs(Vector3::Dot(up, right)) > 0.99f) {
			right = Vector3(0.0f, 1.0f, 0.0f);
		}
		Vector3 forward = Vector3::Normalize(Vector3::Cross(up, right));
		right = Vector3::Normalize(Vector3::Cross(forward, up));
		// 底面の4頂点を計算
		Vector3 bottomVertices[4] = {
			Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(-halfSize, right), Vector3::Multiply(-halfSize, forward))), // 左下
			Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(halfSize, right), Vector3::Multiply(-halfSize, forward))), // 右下
			Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(halfSize, right), Vector3::Multiply(halfSize, forward))), // 右上
			Vector3::Add(baseCenter, Vector3::Add(Vector3::Multiply(-halfSize, right), Vector3::Multiply(halfSize, forward)))  // 左上
		};
		// 頂点（ピラミッドの先端）を計算
		Vector3 topVertex = Vector3::Add(baseCenter, Vector3::Multiply(height, up));
		// 底面の四辺
		for (int i = 0; i < 4; i++) {
			DrawLine(bottomVertices[i], bottomVertices[(i + 1) % 4], color);
		}
		// 側面の4辺
		for (int i = 0; i < 4; i++) {
			DrawLine(bottomVertices[i], topVertex, color);
		}
	}

	void Wireframe::DrawOctahedron(const Vector3& center, float size, const Vector4& color)
	{
		float halfSize = size * 0.5f;
		// 正八面体の6つの頂点
		Vector3 top = Vector3::Add(center, Vector3(0.0f, halfSize, 0.0f));   // 上の頂点
		Vector3 bottom = Vector3::Add(center, Vector3(0.0f, -halfSize, 0.0f)); // 下の頂点
		Vector3 midVertices[4] = {
			Vector3::Add(center, Vector3(halfSize, 0.0f, 0.0f)),  // X+ 方向
			Vector3::Add(center, Vector3(0.0f, 0.0f, halfSize)),  // Z+ 方向
			Vector3::Add(center, Vector3(-halfSize, 0.0f, 0.0f)), // X- 方向
			Vector3::Add(center, Vector3(0.0f, 0.0f, -halfSize))  // Z- 方向
		};
		// 上面の4つの三角形
		for (int i = 0; i < 4; i++) {
			DrawLine(top, midVertices[i], color);
			DrawLine(midVertices[i], midVertices[(i + 1) % 4], color);
		}
		// 下面の4つの三角形
		for (int i = 0; i < 4; i++) {
			DrawLine(bottom, midVertices[i], color);
			DrawLine(midVertices[i], midVertices[(i + 1) % 4], color);
		}
	}

	void Wireframe::DrawDodecahedron(const Vector3& center, float size, const Vector4& color)
	{
		constexpr float GOLDEN_RATIO = 1.61803398875f;
		float halfSize = size * 0.5f;
		// 正十二面体の20頂点を計算
		std::vector<Vector3> vertices = {
			// 正六面体の8頂点
			Vector3::Add(center, Vector3(-halfSize, -halfSize, -halfSize)), // 0
			Vector3::Add(center, Vector3(halfSize, -halfSize, -halfSize)),  // 1
			Vector3::Add(center, Vector3(halfSize, halfSize, -halfSize)),   // 2
			Vector3::Add(center, Vector3(-halfSize, halfSize, -halfSize)),  // 3
			Vector3::Add(center, Vector3(-halfSize, -halfSize, halfSize)),  // 4
			Vector3::Add(center, Vector3(halfSize, -halfSize, halfSize)),   // 5
			Vector3::Add(center, Vector3(halfSize, halfSize, halfSize)),    // 6
			Vector3::Add(center, Vector3(-halfSize, halfSize, halfSize)),   // 7
			// 正六面体の面の中心に相当する12頂点（黄金比を用いる）
			Vector3::Add(center, Vector3(0, -halfSize * GOLDEN_RATIO, -halfSize / GOLDEN_RATIO)),  // 8
			Vector3::Add(center, Vector3(0, -halfSize * GOLDEN_RATIO, halfSize / GOLDEN_RATIO)),   // 9
			Vector3::Add(center, Vector3(0, halfSize * GOLDEN_RATIO, -halfSize / GOLDEN_RATIO)),   // 10
			Vector3::Add(center, Vector3(0, halfSize * GOLDEN_RATIO, halfSize / GOLDEN_RATIO)),    // 11
			Vector3::Add(center, Vector3(-halfSize * GOLDEN_RATIO, -halfSize / GOLDEN_RATIO, 0)),  // 12
			Vector3::Add(center, Vector3(-halfSize * GOLDEN_RATIO, halfSize / GOLDEN_RATIO, 0)),   // 13
			Vector3::Add(center, Vector3(halfSize * GOLDEN_RATIO, -halfSize / GOLDEN_RATIO, 0)),   // 14
			Vector3::Add(center, Vector3(halfSize * GOLDEN_RATIO, halfSize / GOLDEN_RATIO, 0)),    // 15
			Vector3::Add(center, Vector3(-halfSize / GOLDEN_RATIO, 0, -halfSize * GOLDEN_RATIO)),  // 16
			Vector3::Add(center, Vector3(halfSize / GOLDEN_RATIO, 0, -halfSize * GOLDEN_RATIO)),   // 17
			Vector3::Add(center, Vector3(-halfSize / GOLDEN_RATIO, 0, halfSize * GOLDEN_RATIO)),   // 18
			Vector3::Add(center, Vector3(halfSize / GOLDEN_RATIO, 0, halfSize * GOLDEN_RATIO))     // 19
		};
		// 正十二面体の12の五角形（エッジの接続情報）
		int pentagons[12][5] = {
			{ 0, 8, 9, 4, 12 }, { 1, 14, 5, 9, 8 }, { 2, 10, 11, 6, 15 }, { 3, 13, 7, 11, 10 },
			{ 0, 12, 13, 3, 16 }, { 1, 17, 2, 15, 14 }, { 4, 18, 19, 6, 11 }, { 5, 15, 6, 19, 9 },
			{ 7, 13, 12, 4, 18 }, { 3, 16, 17, 2, 10 }, { 0, 8, 17, 16, 1 }, { 7, 18, 19, 5, 14 }
		};
		// ワイヤーフレームで描画
		for (int i = 0; i < 12; i++)
		{
			for (int j = 0; j < 5; j++)
			{
				DrawLine(vertices[pentagons[i][j]], vertices[pentagons[i][(j + 1) % 5]], color);
			}
		}
	}

	void Wireframe::DrawIcosahedron(const Vector3& center, float size, const Vector4& color)
	{
		constexpr float GOLDEN_RATIO = 1.61803398875f;
		float scale = size * 0.5f;
		// 正二十面体の12頂点を計算
		std::vector<Vector3> vertices = {
			Vector3::Add(center, Vector3(-scale, GOLDEN_RATIO * scale, 0)),  // 0
			Vector3::Add(center, Vector3(scale, GOLDEN_RATIO * scale, 0)),   // 1
			Vector3::Add(center, Vector3(-scale, -GOLDEN_RATIO * scale, 0)), // 2
			Vector3::Add(center, Vector3(scale, -GOLDEN_RATIO * scale, 0)),  // 3
			Vector3::Add(center, Vector3(0, -scale, GOLDEN_RATIO * scale)),  // 4
			Vector3::Add(center, Vector3(0, scale, GOLDEN_RATIO * scale)),   // 5
			Vector3::Add(center, Vector3(0, -scale, -GOLDEN_RATIO * scale)), // 6
			Vector3::Add(center, Vector3(0, scale, -GOLDEN_RATIO * scale)),  // 7
			Vector3::Add(center, Vector3(GOLDEN_RATIO * scale, 0, -scale)),  // 8
			Vector3::Add(center, Vector3(GOLDEN_RATIO * scale, 0, scale)),   // 9
			Vector3::Add(center, Vector3(-GOLDEN_RATIO * scale, 0, -scale)), // 10
			Vector3::Add(center, Vector3(-GOLDEN_RATIO * scale, 0, scale))   // 11
		};
		// 正二十面体の20の三角形の接続情報
		int triangles[20][3] = {
			{ 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 },
			{ 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 },
			{ 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 },
			{ 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 }
		};
		// ワイヤーフレームで描画
		for (int i = 0; i < 20; i++) {
			DrawLine(vertices[triangles[i][0]], vertices[triangles[i][1]], color);
			DrawLine(vertices[triangles[i][1]], vertices[triangles[i][2]], color);
			DrawLine(vertices[triangles[i][2]], vertices[triangles[i][0]], color);
		}
	}

	void Wireframe::DrawTorus(const Vector3& center, float R, float r, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color)
	{
		const float PI = std::numbers::pi_v<float>;
		std::vector<Vector3> points;
		// 頂点を計算
		for (uint32_t i = 0; i <= ringSegments; i++) {
			float u = (2.0f * PI * i) / ringSegments;
			for (uint32_t j = 0; j <= tubeSegments; j++) {
				float v = (2.0f * PI * j) / tubeSegments;
				float x = (R + r * cos(v)) * cos(u);
				float y = (R + r * cos(v)) * sin(u);
				float z = r * sin(v);
				points.push_back(center + Vector3(x, y, z));
			}
		}
		// 頂点を線で結ぶ
		for (uint32_t i = 0; i < ringSegments; i++) {
			for (uint32_t j = 0; j < tubeSegments; j++) {
				uint32_t index0 = i * (tubeSegments + 1) + j;
				uint32_t index1 = index0 + 1;
				uint32_t index2 = (i + 1) * (tubeSegments + 1) + j;
				//uint32_t index3 = index2 + 1;
				// 小円方向の線
				DrawLine(points[index0], points[index1], color);
				// 大円方向の線
				DrawLine(points[index0], points[index2], color);
			}
		}
	}

	void Wireframe::DrawRotatingTorus(const Vector3& center, float R, float r, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color, float time)
	{
		const float PI = std::numbers::pi_v<float>;
		Matrix4x4 rotationMatrix = Matrix4x4::MakeRotateY(time * 0.5f); // Y軸回転
		std::vector<Vector3> points;
		// 頂点を計算
		for (uint32_t i = 0; i <= ringSegments; i++) {
			float u = (2.0f * PI * i) / ringSegments;
			for (uint32_t j = 0; j <= tubeSegments; j++) {
				float v = (2.0f * PI * j) / tubeSegments;
				float x = (R + r * cos(v)) * cos(u);
				float y = (R + r * cos(v)) * sin(u);
				float z = r * sin(v);
				Vector3 rotatedPoint = Vector3::Transform(center + Vector3(x, y, z), rotationMatrix);
				points.push_back(rotatedPoint);
			}
		}
		// 頂点を線で結ぶ
		for (uint32_t i = 0; i < ringSegments; i++) {
			for (uint32_t j = 0; j < tubeSegments; j++) {
				uint32_t index0 = i * (tubeSegments + 1) + j;
				uint32_t index1 = index0 + 1;
				uint32_t index2 = (i + 1) * (tubeSegments + 1) + j;
				//uint32_t index3 = index2 + 1;
				// 小円方向の線
				DrawLine(points[index0], points[index1], color);
				// 大円方向の線
				DrawLine(points[index0], points[index2], color);
			}
		}
	}

	void Wireframe::DrawMobiusStrip(const Vector3& center, float R, float w, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color)
	{
		const float PI = std::numbers::pi_v<float>;
		std::vector<Vector3> points;
		// 頂点を計算
		for (uint32_t i = 0; i <= ringSegments; i++) {
			float t = (2.0f * PI * i) / ringSegments;
			for (uint32_t j = 0; j <= tubeSegments; j++) {
				float u = w * (2.0f * j / tubeSegments - 1.0f);
				float x = (R + u * cos(t / 2.0f)) * cos(t);
				float y = (R + u * cos(t / 2.0f)) * sin(t);
				float z = u * sin(t / 2.0f);
				points.push_back(center + Vector3(x, y, z));
			}
		}
		// 頂点を線で結ぶ
		for (uint32_t i = 0; i < ringSegments; i++) {
			for (uint32_t j = 0; j < tubeSegments; j++) {
				uint32_t index0 = i * (tubeSegments + 1) + j;
				uint32_t index1 = index0 + 1;
				uint32_t index2 = (i + 1) * (tubeSegments + 1) + j;
				//uint32_t index3 = index2 + 1;
				// 幅方向の線
				DrawLine(points[index0], points[index1], color);
				// 長さ方向の線
				DrawLine(points[index0], points[index2], color);
			}
		}
	}

	void Wireframe::DrawLemniscate3D(const Vector3& center, float a, float b, float c, uint32_t segments, const Vector4& color)
	{
		const float PI = std::numbers::pi_v<float>;
		std::vector<Vector3> points;
		// 頂点を計算
		for (uint32_t i = 0; i <= segments; i++) {
			float t = (2.0f * PI * i) / segments;
			float denominator = 1.0f + sin(t) * sin(t);
			float x = a * cos(t) / denominator;
			float y = b * sin(t) * cos(t) / denominator;
			float z = c * sin(t);
			points.push_back(center + Vector3(x, y, z));
		}
		// 線を結ぶ
		for (uint32_t i = 0; i < segments; i++) {
			DrawLine(points[i], points[static_cast<std::vector<Vector3, std::allocator<Vector3>>::size_type>(i) + 1], color);
		}
	}

	void Wireframe::DrawPentagonalPrism(const Vector3& center, float radius, float height, const Vector4& color)
	{
		constexpr float PI = std::numbers::pi_v<float>;
		float angleStep = 2.0f * PI / 5.0f; // 五角形の角度間隔
		float halfHeight = height * 0.5f;
		Vector3 bottomVertices[5];
		Vector3 topVertices[5];
		// 五角形の頂点を計算（底面と上面）
		for (int i = 0; i < 5; i++) {
			float angle = angleStep * i;
			Vector3 offset = Vector3::Add(Vector3::Multiply(radius * cosf(angle), Vector3(1.0f, 0.0f, 0.0f)),
				Vector3::Multiply(radius * sinf(angle), Vector3(0.0f, 0.0f, 1.0f)));
			bottomVertices[i] = Vector3::Add(center, Vector3::Add(offset, Vector3(0.0f, -halfHeight, 0.0f)));
			topVertices[i] = Vector3::Add(center, Vector3::Add(offset, Vector3(0.0f, halfHeight, 0.0f)));
		}
		// 底面の五角形を描画
		for (int i = 0; i < 5; i++) {
			DrawLine(bottomVertices[i], bottomVertices[(i + 1) % 5], color);
		}
		// 上面の五角形を描画
		for (int i = 0; i < 5; i++) {
			DrawLine(topVertices[i], topVertices[(i + 1) % 5], color);
		}
		// 側面の長方形の辺を描画（底面と上面をつなぐ）
		for (int i = 0; i < 5; i++) {
			DrawLine(bottomVertices[i], topVertices[i], color);
		}
	}

	void Wireframe::DrawPentagonalPyramid(const Vector3& center, float radius, float height, const Vector4& color)
	{
		constexpr float PI = std::numbers::pi_v<float>;
		float angleStep = 2.0f * PI / 5.0f; // 五角形の角度間隔
		Vector3 bottomVertices[5];
		// 五角形の底面の頂点を計算
		for (int i = 0; i < 5; i++) {
			float angle = angleStep * i;
			Vector3 offset = Vector3::Add(Vector3::Multiply(radius * cosf(angle), Vector3(1.0f, 0.0f, 0.0f)),
				Vector3::Multiply(radius * sinf(angle), Vector3(0.0f, 0.0f, 1.0f)));
			bottomVertices[i] = Vector3::Add(center, offset);
		}
		// 頂点（ピラミッドの先端）を計算
		Vector3 topVertex = Vector3::Add(center, Vector3(0.0f, height, 0.0f));
		// 底面の五角形を描画
		for (int i = 0; i < 5; i++) {
			DrawLine(bottomVertices[i], bottomVertices[(i + 1) % 5], color);
		}
		// 各底面の頂点から頂点（先端）へのエッジを描画
		for (int i = 0; i < 5; i++) {
			DrawLine(bottomVertices[i], topVertex, color);
		}
	}

} // namespace Ken4lowEngine
