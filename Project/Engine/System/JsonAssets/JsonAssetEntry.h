#pragma once

#include <string>
#include <json.hpp>

namespace Ken4lowEngine
{
	struct JsonAssetEntry
	{
		int version = 1;
		std::string id;
		std::string displayName;
		std::string type;
		std::string path;
		nlohmann::json data = nlohmann::json::object();
		bool dirty = false;
	};
}
