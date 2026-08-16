#pragma once

#include "Engine/Vfx/Graph/Runtime/VfxGraphProgram.h"

namespace Ken4lowEngine
{

class VfxGraphCompiler
{
public:
	static VfxGraphCompileResult Compile(const VfxGraphDesc& graph);
};

} // namespace Ken4lowEngine
