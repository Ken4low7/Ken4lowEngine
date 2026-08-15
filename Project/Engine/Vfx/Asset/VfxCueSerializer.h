#pragma once

#include "../Data/VfxCueTypes.h"

#include <optional>
#include <string>

namespace Ken4lowEngine
{

std::string ToString(VfxCueTrackType type);
bool TryParseVfxCueTrackType(const std::string& text, VfxCueTrackType& outType);

class VfxCueSerializer
{
public:
	static bool Load(VfxCueDesc& desc, const std::string& filePath);
	static bool Save(const VfxCueDesc& desc, const std::string& filePath);

	static std::optional<VfxCueDesc> LoadFromFile(const std::string& filePath);
};

} // namespace Ken4lowEngine
