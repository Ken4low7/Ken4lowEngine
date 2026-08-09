#include "CharacterHealthComponent.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void CharacterHealthComponent::Initialize()
	{
		SetMaxHealth(maxHealth_);
		SetCurrentHealth(currentHealth_); // JSONや生成直後の値を同じ補正経路へ通す。
	}

	void CharacterHealthComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("キャラクターHP");
		ComponentPropertyUtility::DrawImGui(CreateProperties());
		ImGui::Text("生存状態: %s", IsAlive() ? "生存" : "死亡");
		ImGui::Text("HP割合: %.3f", GetHealthRatio());
#endif
	}

	void CharacterHealthComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		ComponentPropertyUtility::ToJson(const_cast<CharacterHealthComponent*>(this)->CreateProperties(), outJson);
	}

	void CharacterHealthComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
	}

	CharacterDamageResult CharacterHealthComponent::ApplyDamage(const CharacterDamageInfo& damageInfo)
	{
		CharacterDamageResult result{};
		result.requestedDamage = damageInfo.amount;
		result.healthBefore = currentHealth_;
		result.healthAfter = currentHealth_;

		if (!IsActiveInHierarchy() || isInvulnerable_ || !IsAlive() ||
			!std::isfinite(damageInfo.amount) || damageInfo.amount <= 0.0f)
		{
			return result; // 無効・無敵・死亡済み・不正値の場合はHPを変更しない。
		}

		currentHealth_ = std::clamp(currentHealth_ - damageInfo.amount, 0.0f, maxHealth_);
		result.accepted = true;
		result.healthAfter = currentHealth_;
		result.appliedDamage = result.healthBefore - result.healthAfter;
		result.killed = result.healthBefore > 0.0f && result.healthAfter <= 0.0f;
		return result;
	}

	float CharacterHealthComponent::Heal(float amount)
	{
		if (!IsActiveInHierarchy() || !IsAlive() || !std::isfinite(amount) || amount <= 0.0f) return 0.0f;

		const float healthBefore = currentHealth_;
		currentHealth_ = std::clamp(currentHealth_ + amount, 0.0f, maxHealth_);
		return currentHealth_ - healthBefore;
	}

	void CharacterHealthComponent::ResetHealth(float maxHealth)
	{
		maxHealth_ = std::max(std::isfinite(maxHealth) ? maxHealth : 1.0f, 1.0f);
		currentHealth_ = maxHealth_;
	}

	void CharacterHealthComponent::RestoreFullHealth()
	{
		currentHealth_ = maxHealth_;
	}

	void CharacterHealthComponent::SetMaxHealth(float maxHealth)
	{
		maxHealth_ = std::max(std::isfinite(maxHealth) ? maxHealth : 1.0f, 1.0f);
		currentHealth_ = std::clamp(currentHealth_, 0.0f, maxHealth_);
	}

	void CharacterHealthComponent::SetCurrentHealth(float currentHealth)
	{
		const float safeHealth = std::isfinite(currentHealth) ? currentHealth : 0.0f;
		currentHealth_ = std::clamp(safeHealth, 0.0f, maxHealth_);
	}

	float CharacterHealthComponent::GetHealthRatio() const
	{
		return maxHealth_ > 0.0f ? currentHealth_ / maxHealth_ : 0.0f;
	}

	std::vector<ComponentProperty> CharacterHealthComponent::CreateProperties()
	{
		return {
			{ "MaxHealth", "最大HP", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return maxHealth_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) SetMaxHealth(*typedValue); }, 1.0f, 100000.0f, 1.0f, true, {}, ComponentPropertyDisplay::Default },
			{ "CurrentHealth", "現在HP", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return currentHealth_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) SetCurrentHealth(*typedValue); }, 0.0f, 100000.0f, 1.0f, true, {}, ComponentPropertyDisplay::Default },
			{ "Invulnerable", "無敵", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return isInvulnerable_; }, [this](const ComponentPropertyValue& value) { if (const bool* typedValue = std::get_if<bool>(&value)) SetInvulnerable(*typedValue); }, 0.0f, 0.0f, 0.1f, false, {}, ComponentPropertyDisplay::Default }
		};
	}
} // namespace Ken4lowEngine
