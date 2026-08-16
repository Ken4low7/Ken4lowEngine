#include "GameplayAbilityDiagnostics.h"

#include <algorithm>

namespace Ken4lowEngine
{

GameplayAbilityDiagnostics* GameplayAbilityDiagnostics::GetInstance()
{
	static GameplayAbilityDiagnostics instance;
	return &instance;
}

void GameplayAbilityDiagnostics::ResetStats()
{
	stats_ = {};
}

void GameplayAbilityDiagnostics::RecordActivation(bool success, bool budgetRejected, const std::string& status)
{
	++stats_.activationAttempts;
	if (success) ++stats_.activationSuccesses;
	else ++stats_.activationRejects;
	if (budgetRejected) ++stats_.budgetRejects;
	stats_.lastStatus = status;
}

void GameplayAbilityDiagnostics::RecordCompleted()
{
	++stats_.completedAbilities;
}

void GameplayAbilityDiagnostics::RecordCancelled()
{
	++stats_.cancelledAbilities;
}

void GameplayAbilityDiagnostics::RecordEventPublished()
{
	++stats_.eventsPublished;
}

void GameplayAbilityDiagnostics::RecordVfxPlay()
{
	++stats_.vfxPlays;
}

void GameplayAbilityDiagnostics::RecordModifierApplied()
{
	++stats_.modifiersApplied;
}

void GameplayAbilityDiagnostics::ObserveComponentLoad(uint32_t activeAbilities, uint32_t modifiers)
{
	stats_.peakActiveAbilitiesPerComponent = (std::max)(stats_.peakActiveAbilitiesPerComponent, activeAbilities);
	stats_.peakModifiersPerComponent = (std::max)(stats_.peakModifiersPerComponent, modifiers);
}

} // namespace Ken4lowEngine
