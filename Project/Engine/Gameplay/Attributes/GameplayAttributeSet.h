#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Ken4lowEngine
{

enum class GameplayModifierOperation : uint8_t
{
	Add = 0,
	Multiply,
	Override,
};

enum class GameplayModifierStackingPolicy : uint8_t
{
	Independent = 0,
	RefreshDuration,
	Replace,
};

struct GameplayAttributeDefinition
{
	std::string name;
	float baseValue = 0.0f;
	float minValue = 0.0f;
	float maxValue = 100.0f;
};

struct GameplayModifierSpec
{
	std::string attribute;
	GameplayModifierOperation operation = GameplayModifierOperation::Add;
	float magnitude = 0.0f;
	float durationSeconds = 0.0f; // 0以下は明示削除まで継続。
	std::string stackKey;
	GameplayModifierStackingPolicy stackingPolicy = GameplayModifierStackingPolicy::Independent;
};

struct GameplayModifierHandle
{
	uint64_t value = 0;
	[[nodiscard]] bool IsValid() const { return value != 0; }
	explicit operator bool() const { return IsValid(); }
	bool operator==(const GameplayModifierHandle&) const = default;
};

/// <summary>
/// Health専用Componentを置き換えず、Stamina/Energy/Speed倍率など任意Gameplay値を管理するAttribute集合。
/// </summary>
class GameplayAttributeSet
{
public:
	bool Define(const GameplayAttributeDefinition& definition);
	bool Remove(std::string_view name);
	void Clear();

	[[nodiscard]] bool Has(std::string_view name) const;
	bool SetBaseValue(std::string_view name, float value);
	bool AddBaseValue(std::string_view name, float delta);
	[[nodiscard]] float GetBaseValue(std::string_view name, float fallback = 0.0f) const;
	[[nodiscard]] float GetValue(std::string_view name, float fallback = 0.0f) const;
	[[nodiscard]] std::vector<GameplayAttributeDefinition> GetDefinitions() const;

	GameplayModifierHandle ApplyModifier(const GameplayModifierSpec& spec);
	bool RemoveModifier(GameplayModifierHandle handle);
	void ClearModifiers();
	void Update(float deltaTime);

	[[nodiscard]] uint32_t GetModifierCount() const { return static_cast<uint32_t>(modifiers_.size()); }

private:
	struct AttributeRuntime
	{
		float baseValue = 0.0f;
		float minValue = 0.0f;
		float maxValue = 100.0f;
	};

	struct ModifierRuntime
	{
		GameplayModifierHandle handle{};
		GameplayModifierSpec spec{};
		float remainingSeconds = -1.0f;
		uint64_t sequence = 0;
	};

	ModifierRuntime* FindStackedModifier(const GameplayModifierSpec& spec);
	float Evaluate(std::string_view name, const AttributeRuntime& attribute) const;
	static float ClampToDefinition(float value, const AttributeRuntime& attribute);

	std::unordered_map<std::string, AttributeRuntime> attributes_;
	std::vector<ModifierRuntime> modifiers_;
	uint64_t nextModifierHandle_ = 1;
	uint64_t nextSequence_ = 1;
};

} // namespace Ken4lowEngine
