#pragma once

#include "Vector3.h"

#include <string>

namespace Ken4lowEngine
{
	enum class AudioReverbPreset
	{
		None,
		Room,
		Hall,
		Cave,
		Corridor,
		Outdoor
	};

	const char* AudioReverbPresetToString(AudioReverbPreset preset);
	AudioReverbPreset AudioReverbPresetFromString(const std::string& value, AudioReverbPreset fallback = AudioReverbPreset::Room);

	class AudioReverbZone
	{
	public:
		void ApplyPreset(AudioReverbPreset preset);

		bool Contains(const Vector3& position) const;
		float GetReverbSend() const { return reverbSend_; }
		float GetDecayTime() const { return decayTime_; }
		float GetWetLevel() const { return wetLevel_; }
		float GetDryLevel() const { return dryLevel_; }

		void SetCenter(const Vector3& center) { center_ = center; }
		void SetSize(const Vector3& size);
		void SetEnabled(bool enabled) { enabled_ = enabled; }
		void SetReverbSend(float reverbSend);
		void SetDecayTime(float decayTime);
		void SetWetLevel(float wetLevel);
		void SetDryLevel(float dryLevel);

	private:
		Vector3 center_{};
		Vector3 size_{ 10.0f, 10.0f, 10.0f };
		bool enabled_ = true;
		AudioReverbPreset preset_ = AudioReverbPreset::Room;
		float reverbSend_ = 0.5f;
		float decayTime_ = 1.5f;
		float wetLevel_ = 0.3f;
		float dryLevel_ = 1.0f;
	};
}
