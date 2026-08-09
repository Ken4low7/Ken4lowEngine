#include "ExampleJsonAsset.h"

namespace Ken4lowEngine
{
	void ExampleJsonAsset::ToJson(nlohmann::json& outJson) const
	{
		outJson["sampleInt"] = sampleInt;
		outJson["sampleFloat"] = sampleFloat;
		outJson["note"] = note;
	}

	void ExampleJsonAsset::FromJson(const nlohmann::json& inJson)
	{
		sampleInt = inJson.value("sampleInt", sampleInt);
		sampleFloat = inJson.value("sampleFloat", sampleFloat);
		note = inJson.value("note", note);
	}
}
