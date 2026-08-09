#define NOMINMAX
#include "CharacterAnimationComponent.h"

#include "Actor.h"
#include "CharacterMovementComponent.h"
#include <Scene/Actor/Character/HumanoidVisualComponent.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void CharacterAnimationComponent::Initialize()
	{
		SanitizePlaybackState();
	}

	void CharacterAnimationComponent::Update(float deltaTime)
	{
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
		if (!attackRequestActive_) UpdateLocomotionAnimation();
		if (!isPlaying_)
		{
			ApplyHumanoidPose();
			return;
		}

		playbackTime_ += deltaTime * playbackSpeed_;
		if (loop_)
		{
			playbackTime_ = std::fmod(playbackTime_, duration_);
			ApplyHumanoidPose();
			return;
		}

		if (playbackTime_ >= duration_)
		{
			playbackTime_ = duration_;
			isPlaying_ = false;
		}
		ApplyHumanoidPose(); // Animationだけが人型部位を操作し、攻撃処理からTransform依存を除く。
	}

	void CharacterAnimationComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("キャラクターアニメーション");
		ComponentPropertyUtility::DrawImGui(CreateProperties());
		ImGui::Text("正規化再生位置: %.3f", GetNormalizedTime());
#endif
	}

	void CharacterAnimationComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		ComponentPropertyUtility::ToJson(const_cast<CharacterAnimationComponent*>(this)->CreateProperties(), outJson);
	}

	void CharacterAnimationComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
		SanitizePlaybackState();
	}

	void CharacterAnimationComponent::Play(std::string_view animationName, float duration, bool loop)
	{
		animationName_ = animationName.empty() ? "Idle" : std::string(animationName);
		duration_ = std::isfinite(duration) ? std::max(duration, 0.001f) : 1.0f;
		playbackTime_ = 0.0f;
		loop_ = loop;
		isPlaying_ = true;
	}

	void CharacterAnimationComponent::RequestAttack(std::string_view animationName, float duration)
	{
		attackRequestActive_ = true;
		Play(animationName.empty() ? "Attack.Melee" : animationName, duration, false); // 攻撃要求は必ず先頭から1回だけ再生する。
	}

	void CharacterAnimationComponent::FinishAttack(std::string_view animationName)
	{
		if (!attackRequestActive_ || animationName_ != animationName) return;
		attackRequestActive_ = false;
		Play("Idle", 1.5f, true); // 次UpdateでMovement速度に応じてWalkへ切り替えられる待機状態へ戻す。
	}

	void CharacterAnimationComponent::Stop()
	{
		playbackTime_ = 0.0f;
		isPlaying_ = false;
		attackRequestActive_ = false;
	}

	void CharacterAnimationComponent::Restart()
	{
		playbackTime_ = 0.0f;
		isPlaying_ = true;
	}

	void CharacterAnimationComponent::SetPlaybackSpeed(float playbackSpeed)
	{
		playbackSpeed_ = std::isfinite(playbackSpeed) ? std::max(playbackSpeed, 0.0f) : 1.0f;
	}

	void CharacterAnimationComponent::SetPlaybackTime(float playbackTime)
	{
		playbackTime_ = std::isfinite(playbackTime) ? std::clamp(playbackTime, 0.0f, duration_) : 0.0f;
	}

	float CharacterAnimationComponent::GetNormalizedTime() const
	{
		return duration_ > 0.0f ? playbackTime_ / duration_ : 0.0f;
	}

	std::vector<ComponentProperty> CharacterAnimationComponent::CreateProperties()
	{
		return {
			{ "AnimationName", "アニメーション名", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return animationName_; }, [this](const ComponentPropertyValue& value) { if (const std::string* typedValue = std::get_if<std::string>(&value)) animationName_ = typedValue->empty() ? "Idle" : *typedValue; } },
			{ "Duration", "再生時間", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return duration_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) { duration_ = std::max(*typedValue, 0.001f); SetPlaybackTime(playbackTime_); } }, 0.001f, 3600.0f, 0.01f, true },
			{ "PlaybackTime", "再生位置", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return playbackTime_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) SetPlaybackTime(*typedValue); }, 0.0f, 3600.0f, 0.01f, true },
			{ "PlaybackSpeed", "再生速度", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return playbackSpeed_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) SetPlaybackSpeed(*typedValue); }, 0.0f, 100.0f, 0.05f, true },
			{ "Loop", "ループ", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return loop_; }, [this](const ComponentPropertyValue& value) { if (const bool* typedValue = std::get_if<bool>(&value)) loop_ = *typedValue; } },
			{ "Playing", "再生中", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return isPlaying_; }, [this](const ComponentPropertyValue& value) { if (const bool* typedValue = std::get_if<bool>(&value)) isPlaying_ = *typedValue; } }
		};
	}

	void CharacterAnimationComponent::SanitizePlaybackState()
	{
		if (animationName_.empty()) animationName_ = "Idle";
		duration_ = std::isfinite(duration_) ? std::max(duration_, 0.001f) : 1.0f;
		SetPlaybackSpeed(playbackSpeed_);
		SetPlaybackTime(playbackTime_);
	}

	void CharacterAnimationComponent::UpdateLocomotionAnimation()
	{
		if (animationName_ != "Idle" && animationName_ != "Walk") return; // Editorや外部システムが指定した固有名は自動遷移で上書きしない。
		Actor* owner = GetOwner();
		const auto* movement = owner ? owner->GetComponent<CharacterMovementComponent>() : nullptr;
		const Vector3 velocity = movement ? movement->GetVelocity() : Vector3{};
		const bool moving = Vector3::LengthXZ(velocity) > 0.01f;
		const std::string_view desired = moving ? std::string_view("Walk") : std::string_view("Idle");
		if (animationName_ == desired) return;
		Play(desired, moving ? 0.8f : 1.5f, true);
	}

	void CharacterAnimationComponent::ApplyHumanoidPose()
	{
		Actor* owner = GetOwner();
		auto* visual = owner ? owner->GetComponent<HumanoidVisualComponent>() : nullptr;
		if (!visual) return;

		auto setRotationOffset = [visual](std::string_view partId, const Vector3& offset)
		{
			HumanoidVisualComponent::BodyPart* part = visual->FindPart(partId);
			if (!part) return;
			const HumanoidPartDefinition* definition = visual->GetDefinition().FindPart(partId);
			const Vector3 base = definition ? definition->localRotation : Vector3{};
			part->transform.useQuaternionRotation_ = false;
			part->transform.rotate_ = base + offset; // 毎フレーム定義姿勢から作り直し、別モーションの回転を累積させない。
		};

		setRotationOffset("Body", {});
		setRotationOffset("Head", {});
		setRotationOffset("LeftArm", {});
		setRotationOffset("RightArm", {});
		setRotationOffset("LeftLeg", {});
		setRotationOffset("RightLeg", {});

		const float t = std::clamp(GetNormalizedTime(), 0.0f, 1.0f);
		constexpr float kPi = std::numbers::pi_v<float>;
		constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;
		if (animationName_ == "Walk")
		{
			const float swing = std::sin(t * kTwoPi) * 0.55f;
			setRotationOffset("LeftArm", { -swing, 0.0f, 0.0f });
			setRotationOffset("RightArm", { swing, 0.0f, 0.0f });
			setRotationOffset("LeftLeg", { swing, 0.0f, 0.0f });
			setRotationOffset("RightLeg", { -swing, 0.0f, 0.0f });
			setRotationOffset("Body", { 0.03f + std::abs(std::sin(t * kTwoPi)) * 0.03f, 0.0f, 0.0f });
			return;
		}

		auto threeStageSwing = [t](float windup, float strike)
		{
			if (t < 0.30f) return windup * (t / 0.30f);
			if (t < 0.58f) return windup + (strike - windup) * ((t - 0.30f) / 0.28f);
			return strike * (1.0f - (t - 0.58f) / 0.42f);
		};

		if (animationName_ == "Attack.Scratch")
		{
			const float slash = threeStageSwing(-1.35f, 0.30f);
			const float twist = threeStageSwing(-0.22f, 0.24f);
			setRotationOffset("RightArm", { slash, 0.0f, -0.32f });
			setRotationOffset("LeftArm", { -0.20f * slash, 0.0f, 0.16f });
			setRotationOffset("Body", { 0.06f * std::sin(t * kPi), twist, 0.0f });
			setRotationOffset("Head", { 0.0f, -twist * 0.30f, 0.0f }); // 振り抜き終端を上げ、腕と上体が腰より下へ落ち込み過ぎないようにする。
		}
		else if (animationName_ == "Attack.LungeScratch")
		{
			const float lunge = std::sin(t * kPi);
			const float slash = threeStageSwing(-1.55f, 0.20f);
			setRotationOffset("Body", { 0.34f * lunge, 0.0f, 0.0f });
			setRotationOffset("Head", { -0.10f * lunge, 0.0f, 0.0f });
			setRotationOffset("RightArm", { slash, 0.0f, -0.30f });
			setRotationOffset("LeftArm", { -0.58f * lunge, 0.0f, 0.18f });
			setRotationOffset("LeftLeg", { -0.22f * lunge, 0.0f, 0.0f });
			setRotationOffset("RightLeg", { 0.28f * lunge, 0.0f, 0.0f });
		}
		else if (animationName_ == "Attack.Melee")
		{
			auto smooth01 = [](float value)
			{
				const float clamped = std::clamp(value, 0.0f, 1.0f);
				return clamped * clamped * (3.0f - 2.0f * clamped); // 各動作境界の急な角速度変化を抑える。
			};
			auto overheadSwing = [t, smooth01](float raised, float strike)
			{
				if (t < 0.36f) return raised * smooth01(t / 0.36f); // 腕を下から頭上へ大きく持ち上げる。
				if (t < 0.48f) return raised; // 振り下ろす方向を見せるため頭上で短く構える。
				if (t < 0.70f) return raised + (strike - raised) * smooth01((t - 0.48f) / 0.22f);
				return strike * (1.0f - smooth01((t - 0.70f) / 0.30f));
			};
			auto overheadBodyPitch = [t, smooth01]()
			{
				if (t < 0.36f) return -0.16f * smooth01(t / 0.36f);
				if (t < 0.48f) return -0.16f;
				if (t < 0.70f) return -0.16f + 0.40f * smooth01((t - 0.48f) / 0.22f);
				return 0.24f * (1.0f - smooth01((t - 0.70f) / 0.30f));
			};

			setRotationOffset("RightArm", { overheadSwing(-2.05f, -0.18f), 0.0f, -0.16f });
			setRotationOffset("LeftArm", { overheadSwing(-1.45f, -0.28f), 0.0f, 0.16f });
			setRotationOffset("Body", { overheadBodyPitch(), 0.0f, 0.0f });
		}
		else if (animationName_ == "Attack.Projectile")
		{
			const float recoil = std::sin(t * kPi) * 0.25f;
			setRotationOffset("LeftArm", { -1.15f + recoil, 0.0f, 0.10f });
			setRotationOffset("RightArm", { -1.15f + recoil, 0.0f, -0.10f });
		}
		else if (animationName_ == "Attack.Charge")
		{
			setRotationOffset("Body", { 0.45f * std::sin(t * kPi), 0.0f, 0.0f });
			setRotationOffset("LeftArm", { -0.65f * std::sin(t * kPi), 0.0f, 0.0f });
			setRotationOffset("RightArm", { -0.65f * std::sin(t * kPi), 0.0f, 0.0f });
		}
		else if (animationName_ == "Attack.Shockwave")
		{
			const float slam = threeStageSwing(-1.65f, -0.15f); // 叩き付け後も腕が背中側へ貫通する角度まで回さない。
			setRotationOffset("LeftArm", { slam, 0.0f, 0.15f });
			setRotationOffset("RightArm", { slam, 0.0f, -0.15f });
		}
		else if (animationName_ == "Boss.PhaseTransition")
		{
			const float pulse = std::sin(t * kPi);
			setRotationOffset("Body", { -0.15f * pulse, 0.0f, 0.0f });
			setRotationOffset("LeftArm", { -1.15f * pulse, 0.0f, 0.35f * pulse });
			setRotationOffset("RightArm", { -1.15f * pulse, 0.0f, -0.35f * pulse });
		}
		else if (animationName_ == "Boss.Dead")
		{
			setRotationOffset("Body", { 0.0f, 0.0f, 1.45f * t });
			setRotationOffset("LeftArm", { 0.45f * t, 0.0f, 0.0f });
			setRotationOffset("RightArm", { 0.45f * t, 0.0f, 0.0f });
		}
	}
} // namespace Ken4lowEngine
