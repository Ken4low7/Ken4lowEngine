#include "HumanoidDefinition.h"

#include "JsonFileIO.h"

#include <algorithm>
#include <exception>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Ken4lowEngine
{
	namespace
	{
		/// JSON配列からVector3を読み込み、形式が違う場合は既定値を返す。
		Vector3 ReadVector3(const nlohmann::json& json, const char* key, const Vector3& defaultValue)
		{
			const auto valueIt = json.find(key);
			if (valueIt == json.end() || !valueIt->is_array() || valueIt->size() != 3)
			{
				return defaultValue;
			}

			return { (*valueIt)[0].get<float>(), (*valueIt)[1].get<float>(), (*valueIt)[2].get<float>() };
		}

		/// Vector3が指定した既定値と完全一致するか判定する。
		bool Equals(const Vector3& value, const Vector3& expected)
		{
			return value.x == expected.x && value.y == expected.y && value.z == expected.z;
		}

		/// 呼び出し側が指定した場合だけ検証エラー文字列を書き込む。
		bool Fail(std::string* outError, std::string message)
		{
			if (outError) *outError = std::move(message);
			return false;
		}
	}

	HumanoidDefinition HumanoidDefinition::CreateDefault()
	{
		HumanoidDefinition definition;
		definition.SetParts({
			{ "Body", "", "Characters/body.gltf", {}, {}, { 1.0f, 1.0f, 1.0f }, true },
			{ "Head", "Body", "Characters/head.gltf", { 0.0f, 0.75f, 0.0f }, {}, { 1.0f, 1.0f, 1.0f }, true },
			{ "LeftArm", "Body", "Characters/left_arm.gltf", { -0.75f, 0.75f, 0.0f }, {}, { 1.0f, 1.0f, 1.0f }, true },
			{ "RightArm", "Body", "Characters/right_arm.gltf", { 0.75f, 0.75f, 0.0f }, {}, { 1.0f, 1.0f, 1.0f }, true },
			{ "LeftLeg", "Body", "Characters/left_leg.gltf", { -0.25f, -0.75f, 0.0f }, {}, { 1.0f, 1.0f, 1.0f }, true },
			{ "RightLeg", "Body", "Characters/right_leg.gltf", { 0.25f, -0.75f, 0.0f }, {}, { 1.0f, 1.0f, 1.0f }, true }
		});
		return definition;
	}

	bool HumanoidDefinition::FromJson(const nlohmann::json& json, std::string* outError)
	{
		try
		{
			const nlohmann::json* definitionJson = &json;
			const auto dataIt = json.find("data");
			if (dataIt != json.end() && dataIt->is_object())
			{
				definitionJson = &(*dataIt); // JsonAsset形式でもdata内の人型定義を同じ経路で読み込む。
			}

			const auto partsIt = definitionJson->find("Parts");
			if (partsIt == definitionJson->end() || !partsIt->is_array())
			{
				return Fail(outError, "HumanoidDefinition.Parts must be an array.");
			}

			HumanoidDefinition loaded;
			for (const nlohmann::json& partJson : *partsIt)
			{
				if (!partJson.is_object())
				{
					return Fail(outError, "Each humanoid part must be an object.");
				}

				HumanoidPartDefinition part{};
				part.id = partJson.value("Id", "");
				part.parentId = partJson.value("Parent", "");
				part.modelPath = partJson.value("Model", "");
				part.localPosition = ReadVector3(partJson, "Position", {});
				part.localRotation = ReadVector3(partJson, "Rotation", {});
				part.localScale = ReadVector3(partJson, "Scale", { 1.0f, 1.0f, 1.0f });
				part.visible = partJson.value("Visible", true);
				loaded.parts_.push_back(std::move(part));
			}

			if (!loaded.Validate(outError)) return false;
			parts_ = std::move(loaded.parts_); // 全項目の検証成功後だけ現在の定義を置き換える。
			if (outError) outError->clear();
			return true;
		}
		catch (const std::exception& exception)
		{
			return Fail(outError, std::string("Failed to parse HumanoidDefinition: ") + exception.what());
		}
	}

	nlohmann::json HumanoidDefinition::ToJson() const
	{
		nlohmann::json partsJson = nlohmann::json::array();
		for (const HumanoidPartDefinition& part : parts_)
		{
			nlohmann::json partJson = {
				{ "Id", part.id },
				{ "Model", part.modelPath }
			};
			if (!part.parentId.empty()) partJson["Parent"] = part.parentId;
			if (!Equals(part.localPosition, {})) partJson["Position"] = { part.localPosition.x, part.localPosition.y, part.localPosition.z };
			if (!Equals(part.localRotation, {})) partJson["Rotation"] = { part.localRotation.x, part.localRotation.y, part.localRotation.z };
			if (!Equals(part.localScale, { 1.0f, 1.0f, 1.0f })) partJson["Scale"] = { part.localScale.x, part.localScale.y, part.localScale.z };
			if (!part.visible) partJson["Visible"] = false;
			partsJson.push_back(std::move(partJson));
		}
		return { { "Parts", std::move(partsJson) } };
	}

	bool HumanoidDefinition::LoadFromFile(std::string_view filePath, std::string* outError)
	{
		nlohmann::json json;
		if (!JsonFileIO::LoadJsonFile(std::string(filePath), json))
		{
			return Fail(outError, "Failed to load HumanoidDefinition file: " + std::string(filePath));
		}
		return FromJson(json, outError);
	}

	bool HumanoidDefinition::SaveToFile(std::string_view filePath, std::string* outError) const
	{
		if (!Validate(outError)) return false;
		if (!JsonFileIO::SaveJsonFile(std::string(filePath), ToJson(), 2))
		{
			return Fail(outError, "Failed to save HumanoidDefinition file: " + std::string(filePath));
		}
		if (outError) outError->clear();
		return true;
	}

	bool HumanoidDefinition::Validate(std::string* outError) const
	{
		if (parts_.empty()) return Fail(outError, "HumanoidDefinition requires at least one part.");

		std::unordered_map<std::string, const HumanoidPartDefinition*> partsById;
		for (const HumanoidPartDefinition& part : parts_)
		{
			if (part.id.empty()) return Fail(outError, "Humanoid part Id must not be empty.");
			if (part.modelPath.empty()) return Fail(outError, "Humanoid part Model must not be empty: " + part.id);
			if (!partsById.emplace(part.id, &part).second) return Fail(outError, "Duplicate humanoid part Id: " + part.id);
		}

		for (const HumanoidPartDefinition& part : parts_)
		{
			if (part.parentId.empty()) continue;
			if (part.parentId == part.id) return Fail(outError, "Humanoid part cannot parent itself: " + part.id);
			if (!partsById.contains(part.parentId)) return Fail(outError, "Unknown humanoid parent Id: " + part.parentId);
		}

		std::unordered_map<std::string, int> visitState;
		std::function<bool(const HumanoidPartDefinition&)> visit = [&](const HumanoidPartDefinition& part)
			{
				int& state = visitState[part.id];
				if (state == 1) return false;
				if (state == 2) return true;
				state = 1;
				if (!part.parentId.empty() && !visit(*partsById.at(part.parentId))) return false;
				state = 2;
				return true;
			};
		for (const HumanoidPartDefinition& part : parts_)
		{
			if (!visit(part)) return Fail(outError, "Humanoid part hierarchy contains a cycle near: " + part.id);
		}

		if (outError) outError->clear();
		return true;
	}

	HumanoidPartDefinition* HumanoidDefinition::FindPart(std::string_view partId)
	{
		const auto partIt = std::find_if(parts_.begin(), parts_.end(), [partId](const HumanoidPartDefinition& part) { return part.id == partId; });
		return partIt != parts_.end() ? &(*partIt) : nullptr;
	}

	const HumanoidPartDefinition* HumanoidDefinition::FindPart(std::string_view partId) const
	{
		const auto partIt = std::find_if(parts_.begin(), parts_.end(), [partId](const HumanoidPartDefinition& part) { return part.id == partId; });
		return partIt != parts_.end() ? &(*partIt) : nullptr;
	}
} // namespace Ken4lowEngine
