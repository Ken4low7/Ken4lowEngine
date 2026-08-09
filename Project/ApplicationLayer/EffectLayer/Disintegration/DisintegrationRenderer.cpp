#define NOMINMAX
#include "DisintegrationRenderer.h"

#include <algorithm>

void DisintegrationRenderer::Draw(const std::vector<DisintegrationParticle>& particles, float globalAlpha)
{
	BlockEffectRenderer::BuildVisibleInstances(particles, globalAlpha, instances_, [](const auto& particle, float alphaScale) {
		K4E::Vector4 color = particle.color;
		color.x = std::clamp(color.x + particle.edgeColor.x, 0.0f, 1.0f);
		color.y = std::clamp(color.y + particle.edgeColor.y, 0.0f, 1.0f);
		color.z = std::clamp(color.z + particle.edgeColor.z, 0.0f, 1.0f);
		color.w *= particle.alpha * alphaScale;
		return color;
	});
	renderer_.Draw(instances_);
}
