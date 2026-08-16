#pragma once

#include "Engine/Vfx/Graph/Data/VfxGraphTypes.h"

#include <string>

namespace Ken4lowEngine
{

class VfxGraphSerializer
{
public:
	static bool Load(VfxGraphDesc& outGraph, const std::string& filePath);
	static bool Save(const VfxGraphDesc& graph, const std::string& filePath);
};

} // namespace Ken4lowEngine
