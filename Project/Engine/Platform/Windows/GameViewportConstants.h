#pragma once

#include <cstdint>

namespace Ken4lowEngine
{

// Editor上のゲーム内部座標はこの固定1920x1080基準へ統一する。
struct GameViewportConstants
{
	static constexpr uint32_t Width = 1920;
	static constexpr uint32_t Height = 1080;
	static constexpr float Aspect = 16.0f / 9.0f;
};

} // namespace Ken4lowEngine
