#pragma once

#include <cstdint>
#include <string>

namespace Ken4lowEngine
{

struct GameplayAbilityBudget
{
	uint32_t maxRegisteredAbilitiesPerComponent = 64u;
	uint32_t maxActiveAbilitiesPerComponent = 32u;
	uint32_t maxModifiersPerComponent = 128u;
	uint32_t maxActivationsPerFramePerComponent = 32u;
};

struct GameplayAbilityGlobalStats
{
	uint64_t activationAttempts = 0;
	uint64_t activationSuccesses = 0;
	uint64_t activationRejects = 0;
	uint64_t completedAbilities = 0;
	uint64_t cancelledAbilities = 0;
	uint64_t eventsPublished = 0;
	uint64_t vfxPlays = 0;
	uint64_t modifiersApplied = 0;
	uint64_t budgetRejects = 0;
	uint32_t peakActiveAbilitiesPerComponent = 0;
	uint32_t peakModifiersPerComponent = 0;
	std::string lastStatus;
};

/// <summary>
/// Ability Component間で共有するBudgetと集計値だけを持つ。Ability実体の所有権は各Componentへ残す。
/// </summary>
class GameplayAbilityDiagnostics
{
public:
	static GameplayAbilityDiagnostics* GetInstance();

	GameplayAbilityBudget& GetEditableBudget() { return budget_; }
	[[nodiscard]] const GameplayAbilityBudget& GetBudget() const { return budget_; }
	[[nodiscard]] const GameplayAbilityGlobalStats& GetStats() const { return stats_; }
	void ResetStats();

	void RecordActivation(bool success, bool budgetRejected, const std::string& status);
	void RecordCompleted();
	void RecordCancelled();
	void RecordEventPublished();
	void RecordVfxPlay();
	void RecordModifierApplied();
	void ObserveComponentLoad(uint32_t activeAbilities, uint32_t modifiers);

private:
	GameplayAbilityDiagnostics() = default;
	GameplayAbilityBudget budget_{};
	GameplayAbilityGlobalStats stats_{};
};

} // namespace Ken4lowEngine
