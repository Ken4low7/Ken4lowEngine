#define NOMINMAX
#include "BossWeakPointComponent.h"

#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void BossWeakPointComponent::Initialize()
	{
		if (weakPoints_.empty())
		{
			weakPoints_ = { { "Head", 2.0f, true }, { "LeftArm", 1.25f, true }, { "RightArm", 1.25f, true } };
		}
		SanitizeDefinitions();
	}

	void BossWeakPointComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("ボス弱点");
		ImGui::Text("部位参照: %s", HasValidPartReferences() ? "OK" : "未解決");
		for (const WeakPointDefinition& weakPoint : weakPoints_)
		{
			ImGui::BulletText("%s x%.2f / %s", weakPoint.partId.c_str(), weakPoint.damageMultiplier, weakPoint.enabled ? "有効" : "停止");
		}
#endif
	}

	void BossWeakPointComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		nlohmann::json definitions = nlohmann::json::array();
		for (const WeakPointDefinition& weakPoint : weakPoints_)
		{
			definitions.push_back({ { "PartId", weakPoint.partId }, { "DamageMultiplier", weakPoint.damageMultiplier }, { "Enabled", weakPoint.enabled } });
		}
		outJson["WeakPoints"] = std::move(definitions);
	}

	void BossWeakPointComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		const auto definitionsIt = inJson.find("WeakPoints");
		if (definitionsIt == inJson.end() || !definitionsIt->is_array()) return;

		std::vector<WeakPointDefinition> restored;
		for (const nlohmann::json& definitionJson : *definitionsIt)
		{
			if (!definitionJson.is_object()) continue;
			WeakPointDefinition definition{};
			definition.partId = definitionJson.value("PartId", std::string{});
			definition.damageMultiplier = definitionJson.value("DamageMultiplier", 1.0f);
			definition.enabled = definitionJson.value("Enabled", true);
			restored.push_back(std::move(definition));
		}
		weakPoints_ = std::move(restored);
		SanitizeDefinitions();
	}

	float BossWeakPointComponent::ResolveDamageMultiplier(std::string_view partId) const
	{
		const auto weakPointIt = std::find_if(weakPoints_.begin(), weakPoints_.end(), [partId](const WeakPointDefinition& weakPoint) { return weakPoint.partId == partId; });
		return weakPointIt != weakPoints_.end() && weakPointIt->enabled ? weakPointIt->damageMultiplier : 1.0f;
	}

	CharacterDamageResult BossWeakPointComponent::ApplyDamageToPart(std::string_view partId, float baseDamage)
	{
		CharacterDamageResult result{};
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		const auto* visual = owner ? owner->GetComponent<HumanoidVisualComponent>() : nullptr;
		if (!owner || !visual || !visual->FindPart(partId)) return result; // 弱点側は部位を生成せず、VisualのID解決にだけ依存する。
		return owner->ApplyDamage(baseDamage * ResolveDamageMultiplier(partId));
	}

	bool BossWeakPointComponent::HasValidPartReferences() const
	{
		Actor* owner = GetOwner();
		const auto* visual = owner ? owner->GetComponent<HumanoidVisualComponent>() : nullptr;
		if (!visual || weakPoints_.empty()) return false;
		return std::all_of(weakPoints_.begin(), weakPoints_.end(), [visual](const WeakPointDefinition& weakPoint) { return visual->FindPart(weakPoint.partId) != nullptr; });
	}

	void BossWeakPointComponent::SanitizeDefinitions()
	{
		std::unordered_set<std::string> registeredIds;
		auto invalid = [&registeredIds](WeakPointDefinition& weakPoint)
		{
			if (weakPoint.partId.empty() || !registeredIds.insert(weakPoint.partId).second) return true;
			weakPoint.damageMultiplier = std::isfinite(weakPoint.damageMultiplier) ? std::max(0.0f, weakPoint.damageMultiplier) : 1.0f;
			return false;
		};
		weakPoints_.erase(std::remove_if(weakPoints_.begin(), weakPoints_.end(), invalid), weakPoints_.end());
	}
} // namespace Ken4lowEngine
