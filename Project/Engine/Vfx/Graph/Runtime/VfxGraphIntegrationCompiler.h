#pragma once

#include "Engine/Vfx/Graph/Data/VfxGraphTypes.h"
#include "Engine/Vfx/Data/VfxCueTypes.h"

#include <string>
#include <vector>

namespace Ken4lowEngine
{

class VfxGraphIntegrationCompiler final
{
public:
	static bool Compile(
		const VfxGraphDesc& graph,
		VfxCueDesc& outOneShotCue,
		VfxCueDesc& outLoopCue,
		std::vector<std::string>& errors);
};

} // namespace Ken4lowEngine
