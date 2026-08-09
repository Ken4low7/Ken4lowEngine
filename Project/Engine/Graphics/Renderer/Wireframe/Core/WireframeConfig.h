#pragma once
// Wireframeの最大描画数などの設定値。

#include <cstdint>

namespace Ken4lowEngine
{
	inline constexpr uint32_t kWireframeTriangleMaxCount = 30096;
	inline constexpr uint32_t kWireframeTriangleVertexCount = 3;

	inline constexpr uint32_t kWireframeBoxMaxCount = 30096;
	inline constexpr uint32_t kWireframeBoxIndexCount = 6;
	inline constexpr uint32_t kWireframeBoxVertexCount = 4;

	inline constexpr uint32_t kWireframeLineMaxCount = 1000000;
	inline constexpr uint32_t kWireframeLineVertexCount = 2;

	// AABB / OBBは8頂点・12辺の共有メッシュを使い、ここで指定した個数まで一括描画する。
	inline constexpr uint32_t kWireframeBoxWireVertexCount = 8;
	inline constexpr uint32_t kWireframeBoxWireIndexCount = 24;
	inline constexpr uint32_t kWireframeBoxWireMaxInstanceCount = 100000;

	// Sphereは原点中心・半径1のXY / XZ / YZリングを共有して一括描画する。
	inline constexpr uint32_t kWireframeSphereRingSegmentCount = 32;
	inline constexpr uint32_t kWireframeSphereRingCount = 3;
	inline constexpr uint32_t kWireframeSphereBaseVertexCount = kWireframeSphereRingSegmentCount * kWireframeSphereRingCount;
	inline constexpr uint32_t kWireframeSphereIndexCount = kWireframeSphereRingSegmentCount * 2 * kWireframeSphereRingCount;
	inline constexpr uint32_t kWireframeSphereMaxInstanceCount = 2048;

	// CapsuleはY軸基準・半径1・円柱半分高さ1の軽量共有メッシュを使う。
	inline constexpr uint32_t kWireframeCapsuleSegmentCount = 16;
	inline constexpr uint32_t kWireframeCapsuleArchCount = 4;
	inline constexpr uint32_t kWireframeCapsuleHemisphereSegmentCount = kWireframeCapsuleSegmentCount / 4;
	inline constexpr uint32_t kWireframeCapsuleBaseVertexCount =
		kWireframeCapsuleSegmentCount * 2 +
		kWireframeCapsuleArchCount * (kWireframeCapsuleHemisphereSegmentCount + 1) * 2;
	inline constexpr uint32_t kWireframeCapsuleIndexCount =
		kWireframeCapsuleSegmentCount * 2 * 2 +
		kWireframeCapsuleSegmentCount * 2 +
		kWireframeCapsuleArchCount * kWireframeCapsuleHemisphereSegmentCount * 2 * 2;
	inline constexpr uint32_t kWireframeCapsuleMaxInstanceCount = 2048;
}
