#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace Ken4lowEngine
{
	class AnimationModel;

	enum class AnimationState
	{
		Idle,
		Walk,
		Run,
		Attack,
		Damage,
		Death,
	};

	class AnimationStateController
	{
	public:
		void SetAnimationName(AnimationState state, const std::string& name);
		const std::string& GetAnimationName(AnimationState state) const;

		void SetCrossFadeDuration(float duration);
		float GetCrossFadeDuration() const { return crossFadeDuration_; }

		AnimationState GetCurrentState() const { return currentState_; }
		AnimationState GetRequestedState() const { return requestedState_; }
		bool HasCurrentState() const { return hasCurrentState_; }

		bool RequestState(AnimationState state, AnimationModel& model);
		void Reset();

		static const char* ToString(AnimationState state);

	private:
		static size_t ToIndex(AnimationState state);
		bool HasAnimationClip(const AnimationModel& model, const std::string& name) const;

		std::array<std::string, 6> animationNames_{
			"Idle", "Walk", "Run", "Attack", "Damage", "Death"
		};
		AnimationState currentState_ = AnimationState::Idle;
		AnimationState requestedState_ = AnimationState::Idle;
		float crossFadeDuration_ = 0.2f;
		bool hasCurrentState_ = false;
	};
}
