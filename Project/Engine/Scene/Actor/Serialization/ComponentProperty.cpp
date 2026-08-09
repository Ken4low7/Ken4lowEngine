#include "ComponentProperty.h"

#include <algorithm>
#include <array>
#include <cstdio>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		bool IsNumberArray(const nlohmann::json& value, size_t size)
		{
			if (!value.is_array() || value.size() != size)
			{
				return false;
			}

			for (const auto& element : value)
			{
				if (!element.is_number())
				{
					return false;
				}
			}

			return true;
		}

		void WriteValueToJson(const ComponentProperty& property, nlohmann::json& outJson)
		{
			if (!property.getter)
			{
				return;
			}

			const ComponentPropertyValue value = property.getter();
			switch (property.type)
			{
			case ComponentPropertyType::Bool:
				if (const bool* typedValue = std::get_if<bool>(&value))
				{
					outJson[property.name] = *typedValue;
				}
				break;
			case ComponentPropertyType::Int:
				if (const int* typedValue = std::get_if<int>(&value))
				{
					outJson[property.name] = *typedValue;
				}
				break;
			case ComponentPropertyType::Float:
				if (const float* typedValue = std::get_if<float>(&value))
				{
					outJson[property.name] = *typedValue;
				}
				break;
			case ComponentPropertyType::String:
				if (const std::string* typedValue = std::get_if<std::string>(&value))
				{
					outJson[property.name] = *typedValue;
				}
				break;
			case ComponentPropertyType::Vector2:
				if (const Vector2* typedValue = std::get_if<Vector2>(&value))
				{
					outJson[property.name] = { typedValue->x, typedValue->y };
				}
				break;
			case ComponentPropertyType::Vector3:
				if (const Vector3* typedValue = std::get_if<Vector3>(&value))
				{
					outJson[property.name] = { typedValue->x, typedValue->y, typedValue->z };
				}
				break;
			case ComponentPropertyType::Vector4:
				if (const Vector4* typedValue = std::get_if<Vector4>(&value))
				{
					outJson[property.name] = { typedValue->x, typedValue->y, typedValue->z, typedValue->w };
				}
				break;
			}
		}

		void ReadValueFromJson(const ComponentProperty& property, const nlohmann::json& inJson)
		{
			if (!property.setter || property.name.empty() || !inJson.contains(property.name))
			{
				return;
			}

			const nlohmann::json& value = inJson[property.name];
			switch (property.type)
			{
			case ComponentPropertyType::Bool:
				if (value.is_boolean())
				{
					property.setter(value.get<bool>());
				}
				break;
			case ComponentPropertyType::Int:
				if (value.is_number_integer())
				{
					property.setter(value.get<int>());
				}
				break;
			case ComponentPropertyType::Float:
				if (value.is_number())
				{
					property.setter(value.get<float>());
				}
				break;
			case ComponentPropertyType::String:
				if (value.is_string())
				{
					property.setter(value.get<std::string>());
				}
				break;
			case ComponentPropertyType::Vector2:
				if (IsNumberArray(value, 2))
				{
					property.setter(Vector2{ value[0].get<float>(), value[1].get<float>() });
				}
				break;
			case ComponentPropertyType::Vector3:
				if (IsNumberArray(value, 3))
				{
					property.setter(Vector3{ value[0].get<float>(), value[1].get<float>(), value[2].get<float>() });
				}
				break;
			case ComponentPropertyType::Vector4:
				if (IsNumberArray(value, 4))
				{
					property.setter(Vector4{ value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>() });
				}
				break;
			}
		}
	}

	bool ComponentPropertyUtility::DrawImGui(const std::vector<ComponentProperty>& properties)
	{
		bool changed = false;

#ifdef USE_IMGUI
		for (const ComponentProperty& property : properties)
		{
			if (!property.getter || !property.setter)
			{
				continue;
			}

			const ComponentPropertyValue value = property.getter();
			const char* label = property.displayName.empty() ? property.name.c_str() : property.displayName.c_str();

			switch (property.type)
			{
			case ComponentPropertyType::Bool:
				if (const bool* typedValue = std::get_if<bool>(&value))
				{
					bool editValue = *typedValue;
					if (ImGui::Checkbox(label, &editValue))
					{
						property.setter(editValue);
						changed = true;
					}
				}
				break;
			case ComponentPropertyType::Int:
				if (const int* typedValue = std::get_if<int>(&value))
				{
					int editValue = *typedValue;
					if (property.hasRange ? ImGui::DragInt(label, &editValue, property.speed, static_cast<int>(property.min), static_cast<int>(property.max)) : ImGui::DragInt(label, &editValue, property.speed))
					{
						property.setter(editValue);
						changed = true;
					}
				}
				break;
			case ComponentPropertyType::Float:
				if (const float* typedValue = std::get_if<float>(&value))
				{
					float editValue = *typedValue;
					if (property.hasRange ? ImGui::DragFloat(label, &editValue, property.speed, property.min, property.max) : ImGui::DragFloat(label, &editValue, property.speed))
					{
						property.setter(editValue);
						changed = true;
					}
				}
				break;
			case ComponentPropertyType::String:
				if (const std::string* typedValue = std::get_if<std::string>(&value))
				{
					if (!property.choices.empty())
					{
						std::string preview = *typedValue;
						for (const ComponentPropertyChoice& choice : property.choices)
						{
							if (choice.value == *typedValue)
							{
								preview = choice.displayName.empty() ? choice.value : choice.displayName;
								break;
							}
						}

						if (ImGui::BeginCombo(label, preview.c_str()))
						{
							for (const ComponentPropertyChoice& choice : property.choices)
							{
								const bool selected = choice.value == *typedValue;
								const std::string choiceLabel = choice.displayName.empty() ? choice.value : choice.displayName;
								if (ImGui::Selectable(choiceLabel.c_str(), selected))
								{
									property.setter(choice.value);
									changed = true;
								}
								if (selected)
								{
									ImGui::SetItemDefaultFocus();
								}
							}
							ImGui::EndCombo();
						}
					}
					else
					{
						std::array<char, 256> buffer{};
						std::snprintf(buffer.data(), buffer.size(), "%s", typedValue->c_str());
						const bool edited = property.display == ComponentPropertyDisplay::MultilineText
							? ImGui::InputTextMultiline(label, buffer.data(), buffer.size())
							: ImGui::InputText(label, buffer.data(), buffer.size());
						if (edited)
						{
							property.setter(std::string(buffer.data()));
							changed = true;
						}
					}
				}
				break;
			case ComponentPropertyType::Vector2:
				if (const Vector2* typedValue = std::get_if<Vector2>(&value))
				{
					float editValue[2] = { typedValue->x, typedValue->y };
					if (ImGui::DragFloat2(label, editValue, property.speed, property.min, property.max))
					{
						property.setter(Vector2{ editValue[0], editValue[1] });
						changed = true;
					}
				}
				break;
			case ComponentPropertyType::Vector3:
				if (const Vector3* typedValue = std::get_if<Vector3>(&value))
				{
					float editValue[3] = { typedValue->x, typedValue->y, typedValue->z };
					if (ImGui::DragFloat3(label, editValue, property.speed, property.min, property.max))
					{
						property.setter(Vector3{ editValue[0], editValue[1], editValue[2] });
						changed = true;
					}
				}
				break;
			case ComponentPropertyType::Vector4:
				if (const Vector4* typedValue = std::get_if<Vector4>(&value))
				{
					float editValue[4] = { typedValue->x, typedValue->y, typedValue->z, typedValue->w };
					const bool edited = property.display == ComponentPropertyDisplay::Color
						? ImGui::ColorEdit4(label, editValue)
						: ImGui::DragFloat4(label, editValue, property.speed, property.min, property.max);
					if (edited)
					{
						property.setter(Vector4{ editValue[0], editValue[1], editValue[2], editValue[3] });
						changed = true;
					}
				}
				break;
			}
		}
#endif // USE_IMGUI

		return changed;
	}

	void ComponentPropertyUtility::ToJson(const std::vector<ComponentProperty>& properties, nlohmann::json& outJson)
	{
		for (const ComponentProperty& property : properties)
		{
			WriteValueToJson(property, outJson);
		}
	}

	void ComponentPropertyUtility::FromJson(const std::vector<ComponentProperty>& properties, const nlohmann::json& inJson)
	{
		for (const ComponentProperty& property : properties)
		{
			ReadValueFromJson(property, inJson);
		}
	}
}
