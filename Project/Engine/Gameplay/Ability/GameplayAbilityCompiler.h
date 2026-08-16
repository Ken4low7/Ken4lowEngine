#pragma once

#include "GameplayAbilityTypes.h"

namespace Ken4lowEngine
{

class GameplayAbilityCompiler
{
public:
	static GameplayAbilityCompileResult Compile(const GameplayAbilityDesc& desc);
};

} // namespace Ken4lowEngine
