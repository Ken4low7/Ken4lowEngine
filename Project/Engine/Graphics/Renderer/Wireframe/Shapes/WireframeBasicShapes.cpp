// Wireframe の低レベルな線・面プリミティブ描画をまとめる。
#include "Wireframe.h"
#include <cmath>
#include <numbers>

namespace Ken4lowEngine
{

	void Wireframe::DrawLine(const Vector3& start, const Vector3& end, const Vector4& color)
	{
		if (!lineData_ || !lineData_->vertexData || lineIndex_ + 1 >= kWireframeLineMaxCount * kWireframeLineVertexCount)
		{
			return;
		}
		if (!std::isfinite(start.x) || !std::isfinite(start.y) || !std::isfinite(start.z) ||
			!std::isfinite(end.x) || !std::isfinite(end.y) || !std::isfinite(end.z))
		{
			return;
		}
		// 頂点データの設定
		lineData_->vertexData[lineIndex_].position = start;
		lineData_->vertexData[lineIndex_ + 1].position = end;
		// カラーの設定
		lineData_->vertexData[lineIndex_].color = color;
		lineData_->vertexData[lineIndex_ + 1].color = color;
		lineIndex_ += kWireframeLineVertexCount;
	}

	void Wireframe::DrawSegment(const Segment& segment, const Vector4& color)
	{
		DrawLine(segment.origin, segment.origin + segment.diff, color);
		// セグメントの始点と終点を線で結ぶ
		// DrawLine(segment.origin, segment.origin + segment.diff, color);
	}

	void Wireframe::DrawFrustum(const std::array<Vector3, 8>& corners, const Vector4& color)
	{
		// Near/Far の四角形と対応頂点を結び、視錐台を 12 本の線で可視化する。
		for (uint32_t i = 0; i < 4; ++i)
		{
			const uint32_t next = (i + 1) % 4;
			DrawLine(corners[i], corners[next], color);
			DrawLine(corners[i + 4], corners[next + 4], color);
			DrawLine(corners[i], corners[i + 4], color);
		}
	}

	void Wireframe::DrawCircle(const Vector3& center, float radius, uint32_t segmentCount, const Vector4& color)
	{
		const float PI = std::numbers::pi_v<float>;
		float angleStep = (2.0f * PI) / float(segmentCount);
		std::vector<Vector3> points(segmentCount);
		for (uint32_t i = 0; i < segmentCount; i++) {
			float angle = angleStep * i;
			points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
		}
		// 頂点を線で結ぶ
		for (uint32_t i = 0; i < segmentCount; i++) {
			DrawLine(points[i], points[(i + 1) % segmentCount], color);
		}
	}

	void Wireframe::DrawTriangle(const Vector3& position1, const Vector3& position2, const Vector3& position3, const Vector4& color)
	{
		// 頂点データの設定
		triangleData_->vertexData[triangleIndex_].position = position1;
		triangleData_->vertexData[triangleIndex_ + 1].position = position2;
		triangleData_->vertexData[triangleIndex_ + 2].position = position3;
		// カラーデータの設定
		triangleData_->vertexData[triangleIndex_].color = color;
		triangleData_->vertexData[triangleIndex_ + 1].color = color;
		triangleData_->vertexData[triangleIndex_ + 2].color = color;
		triangleIndex_ += kWireframeTriangleVertexCount;
	}

	void Wireframe::DrawBox(const Vector3& position, const Vector3& size, const Vector4& color)
	{
		// 頂点データの設定
		boxData_->vertexData[boxVertexIndex_ + 0].position = Vector3(position.x, position.y, position.z);
		boxData_->vertexData[boxVertexIndex_ + 1].position = Vector3(position.x + size.x, position.y, position.z);
		boxData_->vertexData[boxVertexIndex_ + 2].position = Vector3(position.x + size.x, position.y + size.y, position.z);
		boxData_->vertexData[boxVertexIndex_ + 3].position = Vector3(position.x, position.y + size.y, position.z);
		// カラーデータの設定
		boxData_->vertexData[boxVertexIndex_ + 0].color = color;
		boxData_->vertexData[boxVertexIndex_ + 1].color = color;
		boxData_->vertexData[boxVertexIndex_ + 2].color = color;
		boxData_->vertexData[boxVertexIndex_ + 3].color = color;
		// インデックスデータの設定
		boxData_->indexData[boxIndex_] = boxVertexIndex_ + 0;
		boxData_->indexData[boxIndex_ + 1] = boxVertexIndex_ + 1;
		boxData_->indexData[boxIndex_ + 2] = boxVertexIndex_ + 2;
		boxData_->indexData[boxIndex_ + 3] = boxVertexIndex_ + 0;
		boxData_->indexData[boxIndex_ + 4] = boxVertexIndex_ + 2;
		boxData_->indexData[boxIndex_ + 5] = boxVertexIndex_ + 3;
		// インデックスと頂点インデックスの更新
		boxIndex_ += kWireframeBoxIndexCount;
		boxVertexIndex_ += kWireframeBoxVertexCount;
	}

	void Wireframe::DrawPolygon(const Vector3& center, float radius, uint32_t sides, const Vector4& color)
	{
		if (sides < 3) return; // 三角形以上でないと描画できない
		const float PI = std::numbers::pi_v<float>;
		float angleStep = (2.0f * PI) / float(sides);
		std::vector<Vector3> points(sides);
		for (uint32_t i = 0; i < sides; i++) {
			float angle = angleStep * i;
			points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
		}
		// 頂点を線で結ぶ
		for (uint32_t i = 0; i < sides; i++) {
			DrawLine(points[i], points[(static_cast<std::vector<Vector3, std::allocator<Vector3>>::size_type>(i) + 1) % sides], color);
		}
	}

	void Wireframe::DrawGrid(const float size, const float subdivision, const Vector4& color)
	{
		float halfWidth = size * 0.5f;
		float every = size / subdivision;
		for (uint32_t xIndex = 0; xIndex <= subdivision; xIndex++)
		{
			Vector3 start = Vector3(-halfWidth + every * xIndex, 0.0f, halfWidth);
			Vector3 end = Vector3(-halfWidth + every * xIndex, 0.0f, -halfWidth);
			DrawLine(start, end, color);
		}
		for (uint32_t zIndex = 0; zIndex <= subdivision; zIndex++)
		{
			Vector3 start = Vector3(halfWidth, 0.0f, -halfWidth + every * zIndex);
			Vector3 end = Vector3(-halfWidth, 0.0f, -halfWidth + every * zIndex);
			DrawLine(start, end, color);
		}
		// X軸の線
		DrawLine({ -halfWidth, 0.0f, 0.0f }, { halfWidth, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f });
		// Y軸の線
		DrawLine({ 0.0, -halfWidth, 0.0f }, { 0.0f, halfWidth, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f });
		// Z軸の線
		DrawLine({ 0.0f, 0.0f, -halfWidth }, { 0.0f, 0.0f, halfWidth }, { 0.0f, 0.0f, 1.0f, 1.0f });
	}

} // namespace Ken4lowEngine
