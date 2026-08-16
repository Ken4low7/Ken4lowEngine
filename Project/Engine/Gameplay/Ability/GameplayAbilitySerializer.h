#pragma once

#include "GameplayAbilityTypes.h"

#include <string>

namespace Ken4lowEngine
{

class GameplayAbilitySerializer
{
public:
	static bool Load(GameplayAbilityDesc& outDesc, const std::string& filePath);
	static bool Save(const GameplayAbilityDesc& desc, const std::string& filePath);
};

} // namespace Ken4lowEngine
