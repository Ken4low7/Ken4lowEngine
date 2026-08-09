#pragma once
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

namespace Ken4lowEngine
{

///==========================================================
/// 頂点データの拡張
///==========================================================
struct VertexData
{
	Vector4 position; // 頂点の位置情報
	Vector2 texcoord; // テクスチャ座標
	Vector3 normal;	  // 法線ベクトル
};

} // namespace Ken4lowEngine
