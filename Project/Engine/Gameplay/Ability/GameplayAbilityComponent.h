#pragma once

#include "ActorComponent.h"
#include "GameplayAbilityCompiler.h"
#include "GameplayAbilityDiagnostics.h"
#include "GameplayAttributeSet.h"
#include "GameplayTag.h"
#include "Engine/Vfx/Runtime/VfxRuntimeTypes.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Ken4lowEngine
{

struct GameplayAbilityComponentStats
{
	uint64_t activationAttempts = 0;
	uint64_t activationSuccesses = 0;
	uint64_t activationRejects = 0;
	uint64_t completedAbilities = 0;
	uint64_t cancelledAbilities = 0;
	uint64_t stressActivationRequests = 0;
	uint32_t registeredAbilityCount = 0;
	uint32_t activeAbilityCount = 0;
	uint32_t activeModifierCount = 0;
	uint32_t peakActiveAbilityCount = 0;
	uint32_t activationsThisFrame = 0;
	std::string lastStatus;
};

/// <summary>
/// Actor単位でGameplayTag / Attribute / Ability実行状態を所有するComponent。
/// CharacterHealthComponentのHP責務は奪わず、Ability用の汎用値だけを扱う。
/// </summary>
class GameplayAbilityComponent : public ActorComponent
{
public:
	void Initialize() override;
	void Update(float deltaTime) override;
	void DrawImGui() override;
	void Finalize() override;
	std::string GetClassTypeName() const override { return "GameplayAbilityComponent"; }
	void ToJson(nlohmann::json& outJson) const override;
	void FromJson(const nlohmann::json& inJson) override;

	bool RegisterAbility(const GameplayAbilityDesc& desc, const std::string& sourcePath = {});
	bool LoadAbility(const std::string& filePath);
	bool ReloadAbility(const std::string& abilityName);
	bool UnregisterAbility(const std::string& abilityName);

	GameplayAbilityHandle TryActivateAbility(const std::string& abilityName, GameplayAbilityContext context = {});
	bool CancelAbility(GameplayAbilityHandle handle);
	uint32_t CancelAllAbilities();
	[[nodiscard]] bool IsAbilityActive(GameplayAbilityHandle handle) const;
	[[nodiscard]] float GetCooldownRemaining(const std::string& abilityName) const;

	bool AddGameplayTag(std::string_view tag) { return authoredTags_.Add(tag); }
	bool RemoveGameplayTag(std::string_view tag) { return authoredTags_.Remove(tag); }
	[[nodiscard]] bool HasMatchingTag(std::string_view query) const;
	[[nodiscard]] bool HasExactTag(std::string_view tag) const;
	[[nodiscard]] std::vector<std::string> GetCombinedTags() const;

	GameplayAttributeSet& GetAttributes() { return attributes_; }
	const GameplayAttributeSet& GetAttributes() const { return attributes_; }
	[[nodiscard]] std::vector<std::string> GetRegisteredAbilityNames() const;
	[[nodiscard]] const GameplayAbilityProgram* GetRegisteredProgram(const std::string& abilityName) const;
	[[nodiscard]] const GameplayAbilityComponentStats& GetStats() const { return stats_; }
	[[nodiscard]] const std::vector<std::string>& GetAbilityAssetPaths() const { return abilityAssetPaths_; }

	uint32_t RunStressBurst(const std::string& abilityName, uint32_t count);

private:
	struct RegisteredAbility
	{
		GameplayAbilityProgram program{};
		std::string sourcePath;
	};

	struct ActiveModifier
	{
		GameplayModifierHandle handle{};
		bool removeOnAbilityEnd = true;
	};

	struct ActiveAbility
	{
		GameplayAbilityHandle handle{};
		GameplayAbilityProgram program{};
		GameplayAbilityContext context{};
		float remainingSeconds = 0.0f;
		VfxCueHandle vfxHandle{};
		std::vector<ActiveModifier> modifiers;
		std::vector<std::string> grantedTags;
	};

	bool CanActivate(const RegisteredAbility& ability, const GameplayAbilityContext& context, std::string& outReason, bool& outBudgetRejected) const;
	GameplayAbilityContext ResolveContext(const GameplayAbilityProgram& program, GameplayAbilityContext context) const;
	ActorHandle BuildOwnerHandle() const;
	Vector3 ResolveWorldPosition(const GameplayAbilityContext& context) const;
	VfxCueHandle PlayAbilityVfx(const GameplayAbilityProgram& program, const GameplayAbilityContext& context);
	void PublishAbilityEvent(const std::string& eventTag, const GameplayAbilityProgram& program, const GameplayAbilityContext& context);
	void GrantRuntimeTags(ActiveAbility& active);
	void ReleaseRuntimeTags(const ActiveAbility& active);
	void ApplySelfModifiers(ActiveAbility& active);
	void RemoveEndModifiers(const ActiveAbility& active);
	void CompleteAbility(size_t activeIndex);
	void SetStatus(bool success, std::string message, bool budgetRejected = false);
	void RefreshStats();
	GameplayAbilityHandle AllocateHandle();

	std::unordered_map<std::string, RegisteredAbility> registeredAbilities_;
	std::unordered_map<std::string, float> cooldownRemaining_;
	std::unordered_map<std::string, uint32_t> runtimeGrantedTagCounts_;
	std::vector<ActiveAbility> activeAbilities_;
	std::vector<std::string> abilityAssetPaths_;
	GameplayTagContainer authoredTags_;
	GameplayAttributeSet attributes_;
	GameplayAbilityComponentStats stats_{};
	uint64_t nextAbilityHandleValue_ = 1;
};

} // namespace Ken4lowEngine
