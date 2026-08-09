#pragma once

#include "BlockEffectRenderer.h"
#include "ReconstructionBlock.h"

#include <vector>

/// 再構築ブロックを共通ブロック描画形式へ変換するアダプター。
class ReconstructionRenderer
{
public:
	ReconstructionRenderer() : renderer_(L"ReconstructionRenderer") {}
	void Draw(const std::vector<ReconstructionBlock>& particles, float globalAlpha = 1.0f);

private:
	BlockEffectRenderer renderer_;
	std::vector<BlockEffectRenderer::BlockInstance> instances_;
};
