#pragma once

#include "Engine/Graphics/Renderer/GpuParticle/Data/GpuParticleEffectDesc.h"
#include "Engine/Vfx/Graph/Data/VfxGraphTypes.h"
#include "Engine/Vfx/Data/VfxCueTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Ken4lowEngine
{

struct VfxGraphCompiledEmitter
{
	std::string name;
	std::vector<uint32_t> executionOrder;
};

struct VfxGraphProgram
{
	std::string graphName;
	GpuParticleEffectDesc particleEffect{};
	std::vector<VfxGraphCompiledEmitter> emitters;
	VfxCueDesc integrationOneShotCue{};
	VfxCueDesc integrationLoopCue{};

	[[nodiscard]] bool HasIntegrationTracks() const { return !integrationOneShotCue.tracks.empty(); }
};

struct VfxGraphCompileResult
{
	bool success = false;
	VfxGraphProgram program{};
	std::vector<std::string> errors;
	std::vector<std::string> warnings;
};

} // namespace Ken4lowEngine
