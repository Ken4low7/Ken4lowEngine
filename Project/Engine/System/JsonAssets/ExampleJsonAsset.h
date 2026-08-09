#pragma once

#include "JsonSerializable.h"

#include <string>

namespace Ken4lowEngine
{
	struct ExampleJsonAsset : public JsonSerializable
	{
		int sampleInt = 10;
		float sampleFloat = 1.0f;
		std::string note = "example";

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};
}
