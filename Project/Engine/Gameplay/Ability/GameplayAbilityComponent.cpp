#include "GameplayAbilityComponent.h"

#include "GameplayAbilitySerializer.h"
#include "GameplayEventRouter.h"
#include "ComponentFactory.h"
#include "Actor.h"
#include "SceneComponent.h"
#include "Engine/Vfx/Runtime/VfxCueRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
namespace
{
	[[maybe_unused]] const bool kGameplayAbilityComponentRegistered = []()
	{
		ComponentFactory::ComponentTypeInfo typeInfo{};
		typeInfo.className = "GameplayAbilityComponent";
		typeInfo.displayName = "ゲームプレイアビリティ";
		typeInfo.category = "ゲームプレイ";
		typeInfo.description = "GameplayTag、Attribute、Cooldown、Ability Asset、VFX連携をActor単位で管理します。";
		typeInfo.allowMultiple = false;
		typeInfo.canBeRoot = false;
		typeInfo.createFunc = [](Actor* owner) -> ActorComponent*
		{
			return owner ? &owner->AddComponent<GameplayAbilityComponent>() : nullptr;
		};
		typeInfo.createRootFunc = [](Actor*) -> SceneComponent* { return nullptr; };
		ComponentFactory::RegisterComponentType(std::move(typeInfo));
		return true;
	}();
}

void GameplayAbilityComponent::Initialize()
{
	registeredAbilities_.clear();
	cooldownRemaining_.clear();
	activeAbilities_.clear();
	runtimeGrantedTagCounts_.clear();
	stats_ = {};

	const std::vector<std::string> paths = abilityAssetPaths_;
	for (const std::string& path : paths)
	{
		GameplayAbilityDesc desc{};
		if (!GameplayAbilitySerializer::Load(desc, path) || !RegisterAbility(desc, path))
		{
			stats_.lastStatus = "Ability load failed: " + path;
		}
	}
	RefreshStats();
}

void GameplayAbilityComponent::Update(float deltaTime)
{
	stats_.activationsThisFrame = 0u;
	if (!std::isfinite(deltaTime) || deltaTime < 0.0f) deltaTime = 0.0f;

	for (auto it = cooldownRemaining_.begin(); it != cooldownRemaining_.end();)
	{
		it->second = (std::max)(0.0f, it->second - deltaTime);
		if (it->second <= 0.0f) it = cooldownRemaining_.erase(it);
		else ++it;
	}

	attributes_.Update(deltaTime);
	for (size_t i = activeAbilities_.size(); i > 0u; --i)
	{
		ActiveAbility& active = activeAbilities_[i - 1u];
		active.remainingSeconds -= deltaTime;
		if (active.remainingSeconds <= 0.0f) CompleteAbility(i - 1u);
	}
	RefreshStats();
}

void GameplayAbilityComponent::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Text("Registered Abilities: %u", stats_.registeredAbilityCount);
	ImGui::Text("Active Abilities: %u / Peak %u", stats_.activeAbilityCount, stats_.peakActiveAbilityCount);
	ImGui::Text("Active Modifiers: %u", stats_.activeModifierCount);
	ImGui::Text("Activation Success / Reject: %llu / %llu",
		static_cast<unsigned long long>(stats_.activationSuccesses),
		static_cast<unsigned long long>(stats_.activationRejects));
	if (!stats_.lastStatus.empty()) ImGui::TextWrapped("%s", stats_.lastStatus.c_str());

	if (ImGui::CollapsingHeader("Ability Asset", ImGuiTreeNodeFlags_DefaultOpen))
	{
		static char abilityPath[384] = "Resources/Gameplay/Abilities/Phase19Pulse.ability.json";
		ImGui::InputText("Asset Path", abilityPath, sizeof(abilityPath));
		if (ImGui::Button("Load / Register Ability")) LoadAbility(abilityPath);
		ImGui::SameLine();
		if (ImGui::Button("Cancel All")) CancelAllAbilities();
	}

	if (ImGui::CollapsingHeader("Gameplay Attributes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (const GameplayAttributeDefinition& definition : attributes_.GetDefinitions())
		{
			ImGui::PushID(definition.name.c_str());
			float baseValue = definition.baseValue;
			if (ImGui::DragFloat(definition.name.c_str(), &baseValue, 0.1f, definition.minValue, definition.maxValue))
			{
				attributes_.SetBaseValue(definition.name, baseValue);
			}
			ImGui::SameLine();
			ImGui::Text("Final %.2f", attributes_.GetValue(definition.name));
			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("Gameplay Tags", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (const std::string& tag : GetCombinedTags()) ImGui::BulletText("%s", tag.c_str());
	}

	if (ImGui::CollapsingHeader("Abilities", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (const std::string& name : GetRegisteredAbilityNames())
		{
			ImGui::PushID(name.c_str());
			ImGui::Text("%s  cooldown %.2f", name.c_str(), GetCooldownRemaining(name));
			if (ImGui::Button("Activate")) TryActivateAbility(name);
			ImGui::SameLine();
			if (ImGui::Button("Reload")) ReloadAbility(name);
			ImGui::SameLine();
			if (ImGui::Button("Stress x16")) RunStressBurst(name, 16u);
			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("Phase19 Diagnostics"))
	{
		GameplayAbilityDiagnostics* diagnostics = GameplayAbilityDiagnostics::GetInstance();
		GameplayAbilityBudget& budget = diagnostics->GetEditableBudget();
		int maxActive = static_cast<int>(budget.maxActiveAbilitiesPerComponent);
		int maxModifiers = static_cast<int>(budget.maxModifiersPerComponent);
		int maxActivations = static_cast<int>(budget.maxActivationsPerFramePerComponent);
		if (ImGui::DragInt("Max Active Abilities", &maxActive, 1.0f, 1, 256)) budget.maxActiveAbilitiesPerComponent = static_cast<uint32_t>(maxActive);
		if (ImGui::DragInt("Max Modifiers", &maxModifiers, 1.0f, 1, 1024)) budget.maxModifiersPerComponent = static_cast<uint32_t>(maxModifiers);
		if (ImGui::DragInt("Max Activations / Frame", &maxActivations, 1.0f, 1, 256)) budget.maxActivationsPerFramePerComponent = static_cast<uint32_t>(maxActivations);
		const GameplayAbilityGlobalStats& global = diagnostics->GetStats();
		const GameplayEventRouterStats& events = GameplayEventRouter::GetInstance()->GetStats();
		ImGui::Text("Global Activation: %llu / %llu",
			static_cast<unsigned long long>(global.activationSuccesses),
			static_cast<unsigned long long>(global.activationRejects));
		ImGui::Text("Events Published / Delivered: %llu / %llu",
			static_cast<unsigned long long>(events.publishedCount),
			static_cast<unsigned long long>(events.deliveredCount));
		ImGui::Text("VFX Plays: %llu | Budget Rejects: %llu",
			static_cast<unsigned long long>(global.vfxPlays),
			static_cast<unsigned long long>(global.budgetRejects));
	}
#endif
}

void GameplayAbilityComponent::Finalize()
{
	CancelAllAbilities();
	attributes_.ClearModifiers();
	registeredAbilities_.clear();
	cooldownRemaining_.clear();
	runtimeGrantedTagCounts_.clear();
	RefreshStats();
}

void GameplayAbilityComponent::ToJson(nlohmann::json& outJson) const
{
	ActorComponent::ToJson(outJson);
	outJson["AbilityAssets"] = abilityAssetPaths_;
	outJson["GameplayTags"] = nlohmann::json::array();
	for (const GameplayTag& tag : authoredTags_.GetTags()) outJson["GameplayTags"].push_back(tag.GetName());
	outJson["Attributes"] = nlohmann::json::array();
	for (const GameplayAttributeDefinition& definition : attributes_.GetDefinitions())
	{
		outJson["Attributes"].push_back({
			{ "Name", definition.name },
			{ "BaseValue", definition.baseValue },
			{ "MinValue", definition.minValue },
			{ "MaxValue", definition.maxValue }
		});
	}
}

void GameplayAbilityComponent::FromJson(const nlohmann::json& inJson)
{
	ActorComponent::FromJson(inJson);
	authoredTags_.Clear();
	attributes_.Clear();
	abilityAssetPaths_.clear();

	const auto abilityAssets = inJson.find("AbilityAssets");
	if (abilityAssets != inJson.end() && abilityAssets->is_array())
	{
		for (const auto& value : *abilityAssets)
		{
			if (value.is_string()) abilityAssetPaths_.push_back(value.get<std::string>());
		}
	}
	const auto gameplayTags = inJson.find("GameplayTags");
	if (gameplayTags != inJson.end() && gameplayTags->is_array())
	{
		for (const auto& value : *gameplayTags)
		{
			if (value.is_string()) authoredTags_.Add(value.get<std::string>());
		}
	}
	const auto attributes = inJson.find("Attributes");
	if (attributes != inJson.end() && attributes->is_array())
	{
		for (const auto& source : *attributes)
		{
			if (!source.is_object()) continue;
			GameplayAttributeDefinition definition{};
			definition.name = source.value("Name", std::string{});
			definition.baseValue = source.value("BaseValue", 0.0f);
			definition.minValue = source.value("MinValue", 0.0f);
			definition.maxValue = source.value("MaxValue", 100.0f);
			attributes_.Define(definition);
		}
	}
}

bool GameplayAbilityComponent::RegisterAbility(const GameplayAbilityDesc& desc, const std::string& sourcePath)
{
	GameplayAbilityCompileResult compiled = GameplayAbilityCompiler::Compile(desc);
	if (!compiled.success)
	{
		SetStatus(false, compiled.errors.empty() ? "Ability compile failed" : compiled.errors.front());
		return false;
	}

	const GameplayAbilityBudget& budget = GameplayAbilityDiagnostics::GetInstance()->GetBudget();
	const bool replacing = registeredAbilities_.find(desc.abilityName) != registeredAbilities_.end();
	if (!replacing && registeredAbilities_.size() >= budget.maxRegisteredAbilitiesPerComponent)
	{
		SetStatus(false, "Ability register rejected by maxRegisteredAbilitiesPerComponent", true);
		return false;
	}

	registeredAbilities_[desc.abilityName] = RegisteredAbility{ std::move(compiled.program), sourcePath };
	if (!sourcePath.empty() && std::find(abilityAssetPaths_.begin(), abilityAssetPaths_.end(), sourcePath) == abilityAssetPaths_.end())
	{
		abilityAssetPaths_.push_back(sourcePath);
	}
	SetStatus(true, "Ability registered: " + desc.abilityName);
	RefreshStats();
	return true;
}

bool GameplayAbilityComponent::LoadAbility(const std::string& filePath)
{
	GameplayAbilityDesc desc{};
	if (!GameplayAbilitySerializer::Load(desc, filePath))
	{
		SetStatus(false, "Ability load failed: " + filePath);
		return false;
	}
	return RegisterAbility(desc, filePath);
}

bool GameplayAbilityComponent::ReloadAbility(const std::string& abilityName)
{
	const auto it = registeredAbilities_.find(abilityName);
	if (it == registeredAbilities_.end() || it->second.sourcePath.empty())
	{
		SetStatus(false, "Ability reload source is unavailable: " + abilityName);
		return false;
	}
	const std::string sourcePath = it->second.sourcePath;
	GameplayAbilityDesc desc{};
	if (!GameplayAbilitySerializer::Load(desc, sourcePath))
	{
		SetStatus(false, "Ability reload failed: " + sourcePath);
		return false;
	}
	// Active AbilityはProgram snapshotを保持するため、Reloadは次回Activationからだけ反映される。
	return RegisterAbility(desc, sourcePath);
}

bool GameplayAbilityComponent::UnregisterAbility(const std::string& abilityName)
{
	const bool removed = registeredAbilities_.erase(abilityName) > 0u;
	cooldownRemaining_.erase(abilityName);
	if (removed)
	{
		SetStatus(true, "Ability unregistered: " + abilityName);
		RefreshStats();
	}
	return removed;
}

GameplayAbilityHandle GameplayAbilityComponent::TryActivateAbility(const std::string& abilityName, GameplayAbilityContext context)
{
	++stats_.activationAttempts;
	const auto abilityIt = registeredAbilities_.find(abilityName);
	if (abilityIt == registeredAbilities_.end())
	{
		++stats_.activationRejects;
		SetStatus(false, "Unknown ability: " + abilityName);
		GameplayAbilityDiagnostics::GetInstance()->RecordActivation(false, false, stats_.lastStatus);
		return {};
	}

	context = ResolveContext(abilityIt->second.program, context);
	std::string reason;
	bool budgetRejected = false;
	if (!CanActivate(abilityIt->second, context, reason, budgetRejected))
	{
		++stats_.activationRejects;
		SetStatus(false, reason, budgetRejected);
		GameplayAbilityDiagnostics::GetInstance()->RecordActivation(false, budgetRejected, reason);
		return {};
	}

	const GameplayAbilityProgram& program = abilityIt->second.program;
	if (program.desc.cost.amount > 0.0f) attributes_.AddBaseValue(program.desc.cost.attribute, -program.desc.cost.amount);
	if (program.desc.cooldownSeconds > 0.0f) cooldownRemaining_[program.desc.abilityName] = program.desc.cooldownSeconds;

	ActiveAbility active{};
	active.handle = AllocateHandle();
	active.program = program; // Hot Reloadで再生中定義が変わらないようsnapshot化する。
	active.context = context;
	active.remainingSeconds = program.desc.durationSeconds;
	GrantRuntimeTags(active);
	ApplySelfModifiers(active);
	active.vfxHandle = PlayAbilityVfx(program, context);
	PublishAbilityEvent(program.desc.activationEventTag, program, context);

	const GameplayAbilityHandle resultHandle = active.handle;
	activeAbilities_.push_back(std::move(active));
	++stats_.activationSuccesses;
	++stats_.activationsThisFrame;
	SetStatus(true, "Ability activated: " + abilityName);
	GameplayAbilityDiagnostics::GetInstance()->RecordActivation(true, false, stats_.lastStatus);
	RefreshStats();

	if (program.desc.durationSeconds <= 0.0f) CompleteAbility(activeAbilities_.size() - 1u);
	return resultHandle;
}

bool GameplayAbilityComponent::CancelAbility(GameplayAbilityHandle handle)
{
	if (!handle.IsValid()) return false;
	for (size_t i = 0; i < activeAbilities_.size(); ++i)
	{
		ActiveAbility& active = activeAbilities_[i];
		if (active.handle != handle) continue;
		PublishAbilityEvent(active.program.desc.cancelEventTag, active.program, active.context);
		ReleaseRuntimeTags(active);
		RemoveEndModifiers(active);
		if (active.vfxHandle.IsValid()) VfxCueRuntime::GetInstance()->Stop(active.vfxHandle);
		activeAbilities_.erase(activeAbilities_.begin() + static_cast<std::ptrdiff_t>(i));
		++stats_.cancelledAbilities;
		GameplayAbilityDiagnostics::GetInstance()->RecordCancelled();
		SetStatus(true, "Ability cancelled");
		RefreshStats();
		return true;
	}
	return false;
}

uint32_t GameplayAbilityComponent::CancelAllAbilities()
{
	uint32_t cancelled = 0u;
	while (!activeAbilities_.empty())
	{
		const GameplayAbilityHandle handle = activeAbilities_.back().handle;
		if (!CancelAbility(handle)) break;
		++cancelled;
	}
	return cancelled;
}

bool GameplayAbilityComponent::IsAbilityActive(GameplayAbilityHandle handle) const
{
	return std::any_of(activeAbilities_.begin(), activeAbilities_.end(),
		[handle](const ActiveAbility& active) { return active.handle == handle; });
}

float GameplayAbilityComponent::GetCooldownRemaining(const std::string& abilityName) const
{
	const auto it = cooldownRemaining_.find(abilityName);
	return it == cooldownRemaining_.end() ? 0.0f : it->second;
}

bool GameplayAbilityComponent::HasMatchingTag(std::string_view query) const
{
	if (authoredTags_.HasMatching(query)) return true;
	const GameplayTag queryTag{ std::string(query) };
	if (!queryTag.IsValid()) return false;
	for (const auto& [tagName, count] : runtimeGrantedTagCounts_)
	{
		if (count == 0u) continue;
		const GameplayTag runtimeTag{ tagName };
		if (runtimeTag.Matches(queryTag)) return true;
	}
	return false;
}

bool GameplayAbilityComponent::HasExactTag(std::string_view tag) const
{
	if (authoredTags_.HasExact(tag)) return true;
	const auto it = runtimeGrantedTagCounts_.find(std::string(tag));
	return it != runtimeGrantedTagCounts_.end() && it->second > 0u;
}

std::vector<std::string> GameplayAbilityComponent::GetCombinedTags() const
{
	std::vector<std::string> result;
	for (const GameplayTag& tag : authoredTags_.GetTags()) result.push_back(tag.GetName());
	for (const auto& [tagName, count] : runtimeGrantedTagCounts_)
	{
		if (count > 0u && std::find(result.begin(), result.end(), tagName) == result.end()) result.push_back(tagName);
	}
	std::sort(result.begin(), result.end());
	return result;
}

std::vector<std::string> GameplayAbilityComponent::GetRegisteredAbilityNames() const
{
	std::vector<std::string> result;
	result.reserve(registeredAbilities_.size());
	for (const auto& [name, ability] : registeredAbilities_)
	{
		(void)ability;
		result.push_back(name);
	}
	std::sort(result.begin(), result.end());
	return result;
}

const GameplayAbilityProgram* GameplayAbilityComponent::GetRegisteredProgram(const std::string& abilityName) const
{
	const auto it = registeredAbilities_.find(abilityName);
	return it == registeredAbilities_.end() ? nullptr : &it->second.program;
}

uint32_t GameplayAbilityComponent::RunStressBurst(const std::string& abilityName, uint32_t count)
{
	count = (std::min)(count, 256u);
	stats_.stressActivationRequests += count;
	const auto registered = registeredAbilities_.find(abilityName);
	if (registered == registeredAbilities_.end()) return 0u;

	const std::string costAttribute = registered->second.program.desc.cost.attribute;
	const bool hasCostAttribute = !costAttribute.empty() && attributes_.Has(costAttribute);
	const float savedCostBase = hasCostAttribute ? attributes_.GetBaseValue(costAttribute) : 0.0f;
	const float savedCooldown = GetCooldownRemaining(abilityName);
	uint32_t activated = 0u;
	for (uint32_t i = 0; i < count; ++i)
	{
		cooldownRemaining_.erase(abilityName);
		if (hasCostAttribute) attributes_.SetBaseValue(costAttribute, savedCostBase);
		if (TryActivateAbility(abilityName).IsValid()) ++activated;
	}
	if (hasCostAttribute) attributes_.SetBaseValue(costAttribute, savedCostBase);
	if (savedCooldown > 0.0f) cooldownRemaining_[abilityName] = savedCooldown;
	else cooldownRemaining_.erase(abilityName);
	SetStatus(true, "Stress burst activated " + std::to_string(activated) + " / " + std::to_string(count));
	return activated;
}

bool GameplayAbilityComponent::CanActivate(
	const RegisteredAbility& ability,
	const GameplayAbilityContext& context,
	std::string& outReason,
	bool& outBudgetRejected) const
{
	outBudgetRejected = false;
	const GameplayAbilityBudget& budget = GameplayAbilityDiagnostics::GetInstance()->GetBudget();
	if (stats_.activationsThisFrame >= budget.maxActivationsPerFramePerComponent)
	{
		outReason = "Activation rejected by maxActivationsPerFramePerComponent";
		outBudgetRejected = true;
		return false;
	}
	if (activeAbilities_.size() >= budget.maxActiveAbilitiesPerComponent)
	{
		outReason = "Activation rejected by maxActiveAbilitiesPerComponent";
		outBudgetRejected = true;
		return false;
	}
	if (attributes_.GetModifierCount() + ability.program.desc.selfModifiers.size() > budget.maxModifiersPerComponent)
	{
		outReason = "Activation rejected by maxModifiersPerComponent";
		outBudgetRejected = true;
		return false;
	}
	if (GetCooldownRemaining(ability.program.desc.abilityName) > 0.0f)
	{
		outReason = "Ability is on cooldown";
		return false;
	}
	if (ability.program.desc.targetPolicy == GameplayAbilityTargetPolicy::RequiredActor && !context.target.IsSet())
	{
		outReason = "RequiredActor target is missing";
		return false;
	}
	for (const GameplayTag& required : ability.program.requiredTags)
	{
		if (!HasMatchingTag(required.GetName()))
		{
			outReason = "Missing required tag: " + required.GetName();
			return false;
		}
	}
	for (const GameplayTag& blocked : ability.program.blockedTags)
	{
		if (HasMatchingTag(blocked.GetName()))
		{
			outReason = "Blocked by tag: " + blocked.GetName();
			return false;
		}
	}
	if (ability.program.desc.cost.amount > 0.0f)
	{
		if (!attributes_.Has(ability.program.desc.cost.attribute))
		{
			outReason = "Cost attribute is not defined: " + ability.program.desc.cost.attribute;
			return false;
		}
		if (attributes_.GetValue(ability.program.desc.cost.attribute) < ability.program.desc.cost.amount)
		{
			outReason = "Insufficient cost attribute: " + ability.program.desc.cost.attribute;
			return false;
		}
	}
	return true;
}

GameplayAbilityContext GameplayAbilityComponent::ResolveContext(const GameplayAbilityProgram& program, GameplayAbilityContext context) const
{
	if (program.desc.targetPolicy == GameplayAbilityTargetPolicy::Self) context.target = BuildOwnerHandle();
	if (!context.hasWorldPosition)
	{
		context.worldPosition = ResolveWorldPosition(context);
		context.hasWorldPosition = true;
	}
	if (!std::isfinite(context.strength)) context.strength = 1.0f;
	return context;
}

ActorHandle GameplayAbilityComponent::BuildOwnerHandle() const
{
	return GetOwnerHandle();
}

Vector3 GameplayAbilityComponent::ResolveWorldPosition(const GameplayAbilityContext& context) const
{
	if (context.hasWorldPosition) return context.worldPosition;
	Actor* owner = GetOwner();
	SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
	return root ? root->GetWorldPosition() : Vector3{};
}

VfxCueHandle GameplayAbilityComponent::PlayAbilityVfx(const GameplayAbilityProgram& program, const GameplayAbilityContext& context)
{
	if (program.desc.vfxCueName.empty()) return {};
	VfxCueRuntime* vfx = VfxCueRuntime::GetInstance();
	if (!vfx->IsCueRegistered(program.desc.vfxCueName) && !program.desc.vfxCueAssetPath.empty()) vfx->LoadCue(program.desc.vfxCueAssetPath);
	if (!vfx->IsCueRegistered(program.desc.vfxCueName)) return {};
	VfxCueHandle handle = vfx->Play(program.desc.vfxCueName, ResolveWorldPosition(context));
	if (handle.IsValid())
	{
		if (!program.desc.vfxIntensityParameter.empty())
		{
			vfx->SetFloatParameter(handle, program.desc.vfxIntensityParameter, program.desc.vfxIntensity * context.strength);
		}
		GameplayAbilityDiagnostics::GetInstance()->RecordVfxPlay();
	}
	return handle;
}

void GameplayAbilityComponent::PublishAbilityEvent(
	const std::string& eventTag,
	const GameplayAbilityProgram& program,
	const GameplayAbilityContext& context)
{
	if (eventTag.empty()) return;
	GameplayEvent event{};
	event.eventTag = eventTag;
	event.source = BuildOwnerHandle();
	event.target = context.target;
	event.worldPosition = ResolveWorldPosition(context);
	event.direction = context.direction;
	event.magnitude = context.strength;
	event.abilityName = program.desc.abilityName;
	event.hasWorldPosition = true;
	event.hasDirection = context.hasDirection;
	GameplayEventRouter::GetInstance()->Publish(event);
	GameplayAbilityDiagnostics::GetInstance()->RecordEventPublished();
}

void GameplayAbilityComponent::GrantRuntimeTags(ActiveAbility& active)
{
	for (const GameplayTag& tag : active.program.grantedTags)
	{
		++runtimeGrantedTagCounts_[tag.GetName()];
		active.grantedTags.push_back(tag.GetName());
	}
}

void GameplayAbilityComponent::ReleaseRuntimeTags(const ActiveAbility& active)
{
	for (const std::string& tag : active.grantedTags)
	{
		auto it = runtimeGrantedTagCounts_.find(tag);
		if (it == runtimeGrantedTagCounts_.end()) continue;
		if (it->second > 1u) --it->second;
		else runtimeGrantedTagCounts_.erase(it);
	}
}

void GameplayAbilityComponent::ApplySelfModifiers(ActiveAbility& active)
{
	for (const GameplayAbilitySelfModifierDesc& modifierDesc : active.program.desc.selfModifiers)
	{
		GameplayModifierSpec spec = modifierDesc.modifier;
		if (spec.durationSeconds <= 0.0f && active.program.desc.durationSeconds > 0.0f) spec.durationSeconds = active.program.desc.durationSeconds;
		GameplayModifierHandle handle = attributes_.ApplyModifier(spec);
		if (!handle.IsValid()) continue;
		active.modifiers.push_back({ handle, modifierDesc.removeOnAbilityEnd });
		GameplayAbilityDiagnostics::GetInstance()->RecordModifierApplied();
	}
}

void GameplayAbilityComponent::RemoveEndModifiers(const ActiveAbility& active)
{
	for (const ActiveModifier& modifier : active.modifiers)
	{
		if (modifier.removeOnAbilityEnd) attributes_.RemoveModifier(modifier.handle);
	}
}

void GameplayAbilityComponent::CompleteAbility(size_t activeIndex)
{
	if (activeIndex >= activeAbilities_.size()) return;
	ActiveAbility active = std::move(activeAbilities_[activeIndex]);
	activeAbilities_.erase(activeAbilities_.begin() + static_cast<std::ptrdiff_t>(activeIndex));
	PublishAbilityEvent(active.program.desc.completionEventTag, active.program, active.context);
	ReleaseRuntimeTags(active);
	RemoveEndModifiers(active);
	if (active.program.desc.stopVfxOnAbilityEnd && active.vfxHandle.IsValid()) VfxCueRuntime::GetInstance()->Stop(active.vfxHandle);
	++stats_.completedAbilities;
	GameplayAbilityDiagnostics::GetInstance()->RecordCompleted();
	SetStatus(true, "Ability completed: " + active.program.desc.abilityName);
	RefreshStats();
}

void GameplayAbilityComponent::SetStatus(bool success, std::string message, bool budgetRejected)
{
	(void)success;
	(void)budgetRejected;
	stats_.lastStatus = std::move(message);
}

void GameplayAbilityComponent::RefreshStats()
{
	stats_.registeredAbilityCount = static_cast<uint32_t>(registeredAbilities_.size());
	stats_.activeAbilityCount = static_cast<uint32_t>(activeAbilities_.size());
	stats_.activeModifierCount = attributes_.GetModifierCount();
	stats_.peakActiveAbilityCount = (std::max)(stats_.peakActiveAbilityCount, stats_.activeAbilityCount);
	GameplayAbilityDiagnostics::GetInstance()->ObserveComponentLoad(stats_.activeAbilityCount, stats_.activeModifierCount);
}

GameplayAbilityHandle GameplayAbilityComponent::AllocateHandle()
{
	GameplayAbilityHandle handle{ nextAbilityHandleValue_++ };
	if (nextAbilityHandleValue_ == 0) ++nextAbilityHandleValue_;
	return handle;
}

} // namespace Ken4lowEngine
