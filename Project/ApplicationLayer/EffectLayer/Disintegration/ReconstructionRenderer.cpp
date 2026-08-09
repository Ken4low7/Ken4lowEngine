#define NOMINMAX
#include "ReconstructionRenderer.h"

#include <algorithm>

void ReconstructionRenderer::Draw(const std::vector<ReconstructionBlock>& particles, float globalAlpha)
{
	BlockEffectRenderer::BuildVisibleInstances(particles, globalAlpha, instances_, [](const auto& particle, float alphaScale) {
		K4E::Vector4 color = particle.color;
		color.w *= particle.alpha * alphaScale;
		return color;
	});
	renderer_.Draw(instances_);
}
