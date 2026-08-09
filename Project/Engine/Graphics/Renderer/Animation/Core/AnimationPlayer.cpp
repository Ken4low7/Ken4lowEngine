#include "AnimationPlayer.h"

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{

	void Ken4lowEngine::AnimationPlayer::Reset()
	{
		time_ = 0.0f;
		deltaTime_ = 0.0f;
		isPlaying_ = true;
		loop_ = true;
		speed_ = 1.0f;
		stopAtEnd_ = true;
	}

	void Ken4lowEngine::AnimationPlayer::Update(float deltaTime, float animationDuration)
	{
		if (!isPlaying_ || animationDuration <= 0.0f || deltaTime <= 0.0f)
		{
			deltaTime_ = 0.0f;
			return;
		}

		deltaTime_ = deltaTime;
		time_ += deltaTime * speed_;

		if (loop_)
		{
			time_ = std::fmod(time_, animationDuration);
			if (time_ < 0.0f) { time_ += animationDuration; }
		}
		else
		{
			if (time_ >= animationDuration)
			{
				time_ = animationDuration;
				if (stopAtEnd_) { isPlaying_ = false; }
			}
			else if (time_ < 0.0f)
			{
				time_ = 0.0f;
			}
		}
	}

}