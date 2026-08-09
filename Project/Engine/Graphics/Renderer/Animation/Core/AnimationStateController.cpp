#include "AnimationStateController.h"

#include "AnimationModel.h"

#include <algorithm>

namespace Ken4lowEngine
{
	void AnimationStateController::SetAnimationName(AnimationState state, const std::string& name)
	{
		animationNames_[ToIndex(state)] = name;
	}

	const std::string& AnimationStateController::GetAnimationName(AnimationState state) const
	{
		return animationNames_[ToIndex(state)];
	}

	void AnimationStateController::SetCrossFadeDuration(float duration)
	{
		crossFadeDuration_ = (std::max)(duration, 0.0f);
	}

	bool AnimationStateController::RequestState(AnimationState state, AnimationModel& model)
	{
		if (hasCurrentState_ && state == requestedState_)
		{
			return true;
		}

		const std::string& animationName = GetAnimationName(state);
		if (animationName.empty() || model.GetAnimationClips().empty())
		{
			return false;
		}
		if (!HasAnimationClip(model, animationName))
		{
			return false;
		}

		// 入力やキャラクター状態から再生すべきアニメーションを決め、同じ遷移を毎フレーム発行しないようにする。
		if (!model.CrossFadeAnimationByName(animationName, crossFadeDuration_))
		{
			return false;
		}

		requestedState_ = state;
		currentState_ = state;
		hasCurrentState_ = true;
		return true;
	}

	void AnimationStateController::Reset()
	{
		currentState_ = AnimationState::Idle;
		requestedState_ = AnimationState::Idle;
		hasCurrentState_ = false;
	}

	const char* AnimationStateController::ToString(AnimationState state)
	{
		switch (state)
		{
		case AnimationState::Idle: return "Idle";
		case AnimationState::Walk: return "Walk";
		case AnimationState::Run: return "Run";
		case AnimationState::Attack: return "Attack";
		case AnimationState::Damage: return "Damage";
		case AnimationState::Death: return "Death";
		default: return "Unknown";
		}
	}

	size_t AnimationStateController::ToIndex(AnimationState state)
	{
		switch (state)
		{
		case AnimationState::Idle: return 0;
		case AnimationState::Walk: return 1;
		case AnimationState::Run: return 2;
		case AnimationState::Attack: return 3;
		case AnimationState::Damage: return 4;
		case AnimationState::Death: return 5;
		default: return 0;
		}
	}

	bool AnimationStateController::HasAnimationClip(const AnimationModel& model, const std::string& name) const
	{
		const auto& clips = model.GetAnimationClips();
		return std::any_of(clips.begin(), clips.end(), [&](const auto& clip) { return clip.name == name; });
	}
}
