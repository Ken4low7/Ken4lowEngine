#include "AttackComponent.h"

#include "AttackBehaviors.h"
#include "CharacterActor.h"
#include "CharacterAnimationComponent.h"
#include "SceneComponent.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		constexpr size_t kInvalidAttackIndex = static_cast<size_t>(-1);

		/// JSON配列へAttackDataの全調整値を書き出す。
		nlohmann::json AttackDataToJson(const AttackData& data)
		{
			return {
				{ "Id", data.id },
				{ "Behavior", data.behaviorType },
				{ "Animation", data.animationName },
				{ "Damage", data.damage },
				{ "Cooldown", data.cooldown },
				{ "Windup", data.windupTime },
				{ "Active", data.activeTime },
				{ "Recovery", data.recoveryTime },
				{ "MinRange", data.minRange },
				{ "MaxRange", data.maxRange },
				{ "MovementSpeed", data.movementSpeed },
				{ "MaxHeightDifference", data.maxHeightDifference }
			};
		}

		/// 欠けたJSON項目へ既定値を使いながらAttackDataを読み込む。
		AttackData AttackDataFromJson(const nlohmann::json& json)
		{
			AttackData data{};
			data.id = json.value("Id", data.id);
			data.behaviorType = json.value("Behavior", data.behaviorType);
			data.animationName = json.value("Animation", data.animationName);
			data.damage = json.value("Damage", data.damage);
			data.cooldown = json.value("Cooldown", data.cooldown);
			data.windupTime = json.value("Windup", data.windupTime);
			data.activeTime = json.value("Active", data.activeTime);
			data.recoveryTime = json.value("Recovery", data.recoveryTime);
			data.minRange = json.value("MinRange", data.minRange);
			data.maxRange = json.value("MaxRange", data.maxRange);
			data.movementSpeed = json.value("MovementSpeed", data.movementSpeed);
			data.maxHeightDifference = json.value("MaxHeightDifference", data.maxHeightDifference);
			return data;
		}
	}

	void AttackComponent::Initialize()
	{
		for (AttackEntry& entry : attacks_) SanitizeAttackData(entry.data);
		ResetAttackState();
	}

	void AttackComponent::Update(float deltaTime)
	{
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
		elapsedSinceAcceptedHit_ += deltaTime;
		for (AttackEntry& entry : attacks_)
		{
			entry.cooldownRemaining = std::max(0.0f, entry.cooldownRemaining - deltaTime); // 各攻撃は独立Cooldownを持ち、巨大な状態分岐へまとめない。
		}

		if (IsAttacking()) UpdateCurrentAttack(deltaTime);
	}

	void AttackComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("共通攻撃基盤");
		const std::string_view currentAttackId = GetCurrentAttackId();
		ImGui::Text("状態: %s / 現在: %s", attackEnabled_ ? "有効" : "停止", currentAttackId.empty() ? "None" : std::string(currentAttackId).c_str());
		ImGui::Text("命中回数: %d / 実測間隔: %.3f", acceptedHitCount_, lastMeasuredInterval_);
		for (const AttackEntry& entry : attacks_)
		{
			ImGui::BulletText("%s [%s] XZ %.2f-%.2f / Y差 <= %.2f / CD %.2f / %.2f",
				entry.data.id.c_str(), entry.data.behaviorType.c_str(), entry.data.minRange, entry.data.maxRange,
				entry.data.maxHeightDifference, entry.cooldownRemaining, entry.data.cooldown);
		}
#endif
	}

	void AttackComponent::Finalize()
	{
		InterruptCurrentAttack();
		listeners_.clear();
		attacks_.clear();
		targetActor_ = nullptr;
	}

	void AttackComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		nlohmann::json attacksJson = nlohmann::json::array();
		for (const AttackEntry& entry : attacks_) attacksJson.push_back(AttackDataToJson(entry.data));
		outJson["Attacks"] = std::move(attacksJson);
	}

	void AttackComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		const auto attacksIt = inJson.find("Attacks");
		if (attacksIt == inJson.end() || !attacksIt->is_array()) return;

		std::vector<AttackEntry> restored;
		for (const nlohmann::json& attackJson : *attacksIt)
		{
			if (!attackJson.is_object()) continue;
			AttackData data = AttackDataFromJson(attackJson);
			SanitizeAttackData(data);
			std::unique_ptr<IAttackBehavior> behavior = CreateAttackBehavior(data.behaviorType);
			if (!behavior) continue; // 未登録Behaviorだけを無視し、他の攻撃定義は安全に復元する。
			restored.push_back({ std::move(data), std::move(behavior), 0.0f });
		}
		if (!restored.empty()) attacks_ = std::move(restored);
	}

	bool AttackComponent::RegisterAttack(AttackData data, std::unique_ptr<IAttackBehavior> behavior)
	{
		if (!behavior) return false;
		SanitizeAttackData(data);
		data.behaviorType = std::string(behavior->GetTypeName()); // 登録実体とJSON上のBehavior名を常に一致させる。
		const auto duplicateIt = std::find_if(attacks_.begin(), attacks_.end(), [&data](const AttackEntry& entry) { return entry.data.id == data.id; });
		if (duplicateIt != attacks_.end()) return false;
		attacks_.push_back({ std::move(data), std::move(behavior), 0.0f });
		return true;
	}

	bool AttackComponent::StartAttack(std::string_view attackId)
	{
		if (!attackEnabled_ || IsAttacking()) return false;
		const auto attackIt = std::find_if(attacks_.begin(), attacks_.end(), [attackId](const AttackEntry& entry) { return entry.data.id == attackId; });
		if (attackIt == attacks_.end() || attackIt->cooldownRemaining > 0.0f || !attackIt->behavior) return false;

		AttackContext context = MakeContext();
		if (!context.owner || context.owner->IsDead() || !context.target || context.target->IsDead()) return false;
		if (!IsContextWithinAttackRange(context, attackIt->data)) return false;
		if (!attackIt->behavior->CanStart(context, attackIt->data)) return false;

		currentAttackIndex_ = static_cast<size_t>(std::distance(attacks_.begin(), attackIt));
		phase_ = Phase::Windup;
		phaseElapsed_ = 0.0f;
		attackIt->behavior->Begin(context, attackIt->data);
		if (context.animation)
		{
			const float duration = attackIt->data.windupTime + attackIt->data.activeTime + attackIt->data.recoveryTime;
			context.animation->RequestAttack(attackIt->data.animationName, duration); // 攻撃側は部位を触らずAnimationへモーション名と尺だけを要求する。
		}
		NotifyAttackEvent(AttackEventType::Started);
		return true;
	}

	bool AttackComponent::IsTargetWithinAttackRange(std::string_view attackId) const
	{
		const AttackData* data = FindAttackData(attackId);
		if (!data) return false;
		const AttackContext context = MakeContext();
		if (!context.owner || !context.target || context.owner->IsDead() || context.target->IsDead()) return false;
		return IsContextWithinAttackRange(context, *data);
	}

	void AttackComponent::InterruptCurrentAttack()
	{
		if (IsAttacking()) FinishCurrentAttack(true);
	}

	void AttackComponent::SetAttackEnabled(bool enabled)
	{
		if (attackEnabled_ == enabled) return;
		attackEnabled_ = enabled;
		if (!attackEnabled_) InterruptCurrentAttack();
	}

	void AttackComponent::ResetAttackState()
	{
		if (IsAttacking()) FinishCurrentAttack(true);
		for (AttackEntry& entry : attacks_) entry.cooldownRemaining = 0.0f;
		currentAttackIndex_ = kInvalidAttackIndex;
		phase_ = Phase::Idle;
		phaseElapsed_ = 0.0f;
		elapsedSinceAcceptedHit_ = 0.0f;
		lastMeasuredInterval_ = 0.0f;
		acceptedHitCount_ = 0;
		attackEnabled_ = true;
	}

	const AttackData* AttackComponent::FindAttackData(std::string_view attackId) const
	{
		const auto attackIt = std::find_if(attacks_.begin(), attacks_.end(), [attackId](const AttackEntry& entry) { return entry.data.id == attackId; });
		return attackIt != attacks_.end() ? &attackIt->data : nullptr;
	}

	bool AttackComponent::ConfigureAttack(std::string_view attackId, const AttackData& data)
	{
		const auto attackIt = std::find_if(attacks_.begin(), attacks_.end(), [attackId](const AttackEntry& entry) { return entry.data.id == attackId; });
		if (attackIt == attacks_.end()) return false;
		AttackData configured = data;
		configured.id = std::string(attackId); // 登録済みIDは設定変更で別攻撃へ化けないよう固定する。
		configured.behaviorType = attackIt->data.behaviorType;
		SanitizeAttackData(configured);
		attackIt->data = std::move(configured);
		return true;
	}

	float AttackComponent::GetCooldownRemaining(std::string_view attackId) const
	{
		const auto attackIt = std::find_if(attacks_.begin(), attacks_.end(), [attackId](const AttackEntry& entry) { return entry.data.id == attackId; });
		return attackIt != attacks_.end() ? attackIt->cooldownRemaining : 0.0f;
	}

	std::string_view AttackComponent::GetCurrentAttackId() const
	{
		return IsAttacking() ? std::string_view(attacks_[currentAttackIndex_].data.id) : std::string_view{};
	}

	AttackComponent::ListenerId AttackComponent::AddAttackListener(AttackListener listener)
	{
		if (!listener) return 0;
		const ListenerId id = nextListenerId_++;
		listeners_.push_back({ id, std::move(listener) });
		return id;
	}

	bool AttackComponent::RemoveAttackListener(ListenerId listenerId)
	{
		const auto listenerIt = std::find_if(listeners_.begin(), listeners_.end(), [listenerId](const ListenerEntry& entry) { return entry.id == listenerId; });
		if (listenerIt == listeners_.end()) return false;
		listeners_.erase(listenerIt);
		return true;
	}

	AttackContext AttackComponent::MakeContext() const
	{
		AttackContext context{};
		context.owner = dynamic_cast<CharacterActor*>(GetOwner());
		context.target = targetActor_;
		context.animation = context.owner ? context.owner->GetAnimationComponent() : nullptr;
		if (context.owner && context.target)
		{
			const SceneComponent* root = context.owner->GetRootComponent();
			const Vector3 origin = root ? root->GetWorldPosition() : Vector3{};
			const Vector3 targetPosition = context.target->GetTargetPosition();
			const Vector3 toTarget = targetPosition - origin;
			context.distanceToTarget = Vector3::LengthXZ(toTarget);
			context.heightDifferenceToTarget = std::abs(toTarget.y);
		}
		return context;
	}

	bool AttackComponent::IsContextWithinAttackRange(const AttackContext& context, const AttackData& data)
	{
		return context.distanceToTarget >= data.minRange &&
			context.distanceToTarget <= data.maxRange &&
			context.heightDifferenceToTarget <= data.maxHeightDifference;
	}

	void AttackComponent::UpdateCurrentAttack(float deltaTime)
	{
		if (!IsAttacking()) return;
		AttackEntry& entry = attacks_[currentAttackIndex_];
		AttackContext context = MakeContext();
		if (!attackEnabled_ || !context.owner || context.owner->IsDead() || !context.target || context.target->IsDead())
		{
			FinishCurrentAttack(true);
			return;
		}

		phaseElapsed_ += deltaTime;
		if (phase_ == Phase::Windup && phaseElapsed_ >= entry.data.windupTime)
		{
			phaseElapsed_ -= entry.data.windupTime;
			phase_ = Phase::Active;
		}

		if (phase_ == Phase::Active)
		{
			const float normalized = entry.data.activeTime > 0.0f ? std::clamp(phaseElapsed_ / entry.data.activeTime, 0.0f, 1.0f) : 1.0f;
			const AttackExecutionResult result = entry.behavior->Execute(context, entry.data, deltaTime, normalized);
			if (result.executed)
			{
				if (result.accepted)
				{
					lastMeasuredInterval_ = acceptedHitCount_ > 0 ? elapsedSinceAcceptedHit_ : 0.0f;
					elapsedSinceAcceptedHit_ = 0.0f;
					++acceptedHitCount_;
				}
				NotifyAttackEvent(AttackEventType::Executed, result);
			}
			if (phaseElapsed_ >= entry.data.activeTime)
			{
				phaseElapsed_ -= entry.data.activeTime;
				phase_ = Phase::Recovery;
			}
		}

		if (phase_ == Phase::Recovery && phaseElapsed_ >= entry.data.recoveryTime) FinishCurrentAttack(false);
	}

	void AttackComponent::FinishCurrentAttack(bool interrupted)
	{
		if (!IsAttacking()) return;
		AttackEntry& entry = attacks_[currentAttackIndex_];
		AttackContext context = MakeContext();
		if (entry.behavior) entry.behavior->End(context, entry.data, interrupted);
		entry.cooldownRemaining = entry.data.cooldown;
		if (context.animation) context.animation->FinishAttack(entry.data.animationName);
		NotifyAttackEvent(interrupted ? AttackEventType::Interrupted : AttackEventType::Ended);
		currentAttackIndex_ = kInvalidAttackIndex;
		phase_ = Phase::Idle;
		phaseElapsed_ = 0.0f;
	}

	void AttackComponent::NotifyAttackEvent(AttackEventType type, const AttackExecutionResult& result)
	{
		if (!IsAttacking()) return;
		AttackEvent event{};
		event.type = type;
		event.attackId = attacks_[currentAttackIndex_].data.id;
		event.owner = dynamic_cast<CharacterActor*>(GetOwner());
		event.target = targetActor_;
		event.result = result;
		const std::vector<ListenerEntry> listeners = listeners_; // 通知中の登録解除で走査を壊さない。
		for (const ListenerEntry& entry : listeners) if (entry.listener) entry.listener(event);
	}

	void AttackComponent::SanitizeAttackData(AttackData& data)
	{
		if (data.id.empty()) data.id = "Attack";
		if (data.behaviorType.empty()) data.behaviorType = "Melee";
		if (data.animationName.empty()) data.animationName = "Attack." + data.behaviorType;
		auto finiteNonNegative = [](float value, float fallback) { return std::isfinite(value) ? std::max(0.0f, value) : fallback; };
		data.damage = finiteNonNegative(data.damage, 0.0f);
		data.cooldown = finiteNonNegative(data.cooldown, 0.0f);
		data.windupTime = finiteNonNegative(data.windupTime, 0.0f);
		data.activeTime = finiteNonNegative(data.activeTime, 0.0f);
		data.recoveryTime = finiteNonNegative(data.recoveryTime, 0.0f);
		data.minRange = finiteNonNegative(data.minRange, 0.0f);
		data.maxRange = std::max(data.minRange, finiteNonNegative(data.maxRange, data.minRange));
		data.movementSpeed = finiteNonNegative(data.movementSpeed, 0.0f);
		data.maxHeightDifference = finiteNonNegative(data.maxHeightDifference, 2.5f);
	}
} // namespace Ken4lowEngine
