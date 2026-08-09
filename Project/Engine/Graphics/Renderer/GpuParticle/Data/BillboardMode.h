#pragma once
#include <cstdint>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　	ビルボードモード列挙型
/// -------------------------------------------------------------
enum class BillboardMode : uint32_t
{
	None = 0,        // ビルボードしない（メッシュパーティクルなど）
	Camera = 1 << 0, // 0x1 : フルビルボード（常にカメラ正面）
	YAxis = 1 << 1,  // 0x2 : Y軸だけビルボード（木・縦の煙など）
	Ribbon = 1 << 2, // 0x4 : リボン
};
} // namespace Ken4lowEngine
