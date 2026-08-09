#pragma once
#include <cstdint>

namespace Ken4lowEngine
{

/// ---------- ブレンドモードの列挙型 ---------- ///
enum class BlendMode
{
	kBlendModeNone,		// ブレンドなし
	kBlendModeNormal,	// 通常αブレンド、デフォルト。Src * srcA + Dest * (1 - SrcA)
	kBlendModeAdd,		// 加算 Src * SrcA + Dest * 1
	kBlendModeSubtract,	// 減算 Dest * 1 - Src * SrcA
	kBlendModeMultiply, // 乗算 Src * 0 + Dest * Src
	kBlendModeScreen,	// スクリーン Src * (1 - Dest) + Dest * 1
	kcountOfBlendMode,	// 利用してはいけない
};

// ブレンドモードの数
static inline const uint32_t blendModeNum = static_cast<size_t>(BlendMode::kcountOfBlendMode);


} // namespace Ken4lowEngine
