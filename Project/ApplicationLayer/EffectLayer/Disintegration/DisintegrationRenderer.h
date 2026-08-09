#pragma once

#include "BlockEffectRenderer.h"
#include "DisintegrationParticle.h"

#include <vector>

/// 崩壊粒子を共通ブロック描画形式へ変換するアダプター。
class DisintegrationRenderer
{
public:
	DisintegrationRenderer() : renderer_(L"DisintegrationRenderer") {}
	void Draw(const std::vector<DisintegrationParticle>& particles, float globalAlpha = 1.0f);

private:
	BlockEffectRenderer renderer_;
	std::vector<BlockEffectRenderer::BlockInstance> instances_;
};
