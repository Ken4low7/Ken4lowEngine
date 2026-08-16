#include "GameplayAttributeSet.h"

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{

bool GameplayAttributeSet::Define(const GameplayAttributeDefinition& definition)
{
	if (definition.name.empty() || !std::isfinite(definition.baseValue) ||
		!std::isfinite(definition.minValue) || !std::isfinite(definition.maxValue) ||
		definition.minValue > definition.maxValue)
	{
		return false;
	}
	AttributeRuntime runtime{};
	runtime.minValue = definition.minValue;
	runtime.maxValue = definition.maxValue;
	runtime.baseValue = std::clamp(definition.baseValue, runtime.minValue, runtime.maxValue);
	attributes_[definition.name] = runtime;
	return true;
}

bool GameplayAttributeSet::Remove(std::string_view name)
{
	const std::string key(name);
	const bool removed = attributes_.erase(key) > 0u;
	if (!removed) return false;
	modifiers_.erase(
		std::remove_if(modifiers_.begin(), modifiers_.end(),
			[&key](const ModifierRuntime& modifier) { return modifier.spec.attribute == key; }),
		modifiers_.end());
	return true;
}

void GameplayAttributeSet::Clear()
{
	attributes_.clear();
	modifiers_.clear();
}

bool GameplayAttributeSet::Has(std::string_view name) const
{
	return attributes_.find(std::string(name)) != attributes_.end();
}

bool GameplayAttributeSet::SetBaseValue(std::string_view name, float value)
{
	if (!std::isfinite(value)) return false;
	auto it = attributes_.find(std::string(name));
	if (it == attributes_.end()) return false;
	it->second.baseValue = ClampToDefinition(value, it->second);
	return true;
}

bool GameplayAttributeSet::AddBaseValue(std::string_view name, float delta)
{
	if (!std::isfinite(delta)) return false;
	auto it = attributes_.find(std::string(name));
	if (it == attributes_.end()) return false;
	it->second.baseValue = ClampToDefinition(it->second.baseValue + delta, it->second);
	return true;
}

float GameplayAttributeSet::GetBaseValue(std::string_view name, float fallback) const
{
	const auto it = attributes_.find(std::string(name));
	return it == attributes_.end() ? fallback : it->second.baseValue;
}

float GameplayAttributeSet::GetValue(std::string_view name, float fallback) const
{
	const auto it = attributes_.find(std::string(name));
	return it == attributes_.end() ? fallback : Evaluate(name, it->second);
}

std::vector<GameplayAttributeDefinition> GameplayAttributeSet::GetDefinitions() const
{
	std::vector<GameplayAttributeDefinition> definitions;
	definitions.reserve(attributes_.size());
	for (const auto& [name, attribute] : attributes_)
	{
		definitions.push_back({ name, attribute.baseValue, attribute.minValue, attribute.maxValue });
	}
	std::sort(definitions.begin(), definitions.end(),
		[](const GameplayAttributeDefinition& a, const GameplayAttributeDefinition& b) { return a.name < b.name; });
	return definitions;
}

GameplayModifierHandle GameplayAttributeSet::ApplyModifier(const GameplayModifierSpec& spec)
{
	if (spec.attribute.empty() || !Has(spec.attribute) || !std::isfinite(spec.magnitude) || !std::isfinite(spec.durationSeconds)) return {};

	if (!spec.stackKey.empty() && spec.stackingPolicy != GameplayModifierStackingPolicy::Independent)
	{
		if (ModifierRuntime* existing = FindStackedModifier(spec))
		{
			existing->spec = spec;
			existing->remainingSeconds = spec.durationSeconds > 0.0f ? spec.durationSeconds : -1.0f;
			existing->sequence = nextSequence_++;
			return existing->handle;
		}
	}

	GameplayModifierHandle handle{ nextModifierHandle_++ };
	if (nextModifierHandle_ == 0) ++nextModifierHandle_;
	ModifierRuntime runtime{};
	runtime.handle = handle;
	runtime.spec = spec;
	runtime.remainingSeconds = spec.durationSeconds > 0.0f ? spec.durationSeconds : -1.0f;
	runtime.sequence = nextSequence_++;
	modifiers_.push_back(std::move(runtime));
	return handle;
}

bool GameplayAttributeSet::RemoveModifier(GameplayModifierHandle handle)
{
	if (!handle.IsValid()) return false;
	const auto before = modifiers_.size();
	modifiers_.erase(
		std::remove_if(modifiers_.begin(), modifiers_.end(),
			[handle](const ModifierRuntime& modifier) { return modifier.handle == handle; }),
		modifiers_.end());
	return modifiers_.size() != before;
}

void GameplayAttributeSet::ClearModifiers()
{
	modifiers_.clear();
}

void GameplayAttributeSet::Update(float deltaTime)
{
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
	for (ModifierRuntime& modifier : modifiers_)
	{
		if (modifier.remainingSeconds > 0.0f) modifier.remainingSeconds -= deltaTime;
	}
	modifiers_.erase(
		std::remove_if(modifiers_.begin(), modifiers_.end(),
			[](const ModifierRuntime& modifier)
			{
				return modifier.remainingSeconds != -1.0f && modifier.remainingSeconds <= 0.0f;
			}),
		modifiers_.end());
}

GameplayAttributeSet::ModifierRuntime* GameplayAttributeSet::FindStackedModifier(const GameplayModifierSpec& spec)
{
	for (ModifierRuntime& modifier : modifiers_)
	{
		if (modifier.spec.attribute == spec.attribute && modifier.spec.stackKey == spec.stackKey)
		{
			return &modifier;
		}
	}
	return nullptr;
}

float GameplayAttributeSet::Evaluate(std::string_view name, const AttributeRuntime& attribute) const
{
	float value = attribute.baseValue;
	for (const ModifierRuntime& modifier : modifiers_)
	{
		if (modifier.spec.attribute == name && modifier.spec.operation == GameplayModifierOperation::Add)
		{
			value += modifier.spec.magnitude;
		}
	}
	for (const ModifierRuntime& modifier : modifiers_)
	{
		if (modifier.spec.attribute == name && modifier.spec.operation == GameplayModifierOperation::Multiply)
		{
			value *= modifier.spec.magnitude;
		}
	}
	const ModifierRuntime* lastOverride = nullptr;
	for (const ModifierRuntime& modifier : modifiers_)
	{
		if (modifier.spec.attribute == name && modifier.spec.operation == GameplayModifierOperation::Override &&
			(!lastOverride || modifier.sequence > lastOverride->sequence))
		{
			lastOverride = &modifier;
		}
	}
	if (lastOverride) value = lastOverride->spec.magnitude;
	return ClampToDefinition(value, attribute);
}

float GameplayAttributeSet::ClampToDefinition(float value, const AttributeRuntime& attribute)
{
	return std::clamp(value, attribute.minValue, attribute.maxValue);
}

} // namespace Ken4lowEngine
