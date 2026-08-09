#pragma once

#include <json.hpp>

namespace Ken4lowEngine
{
	class JsonSerializable
	{
	public:
		virtual ~JsonSerializable() = default;
		virtual void ToJson(nlohmann::json& outJson) const = 0;
		virtual void FromJson(const nlohmann::json& inJson) = 0;
	};
}
