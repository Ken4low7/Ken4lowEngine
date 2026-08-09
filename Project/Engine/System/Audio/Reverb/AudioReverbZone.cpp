#include "AudioReverbZone.h"

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{
	const char* AudioReverbPresetToString(AudioReverbPreset preset)
	{
		switch (preset)
		{
		case AudioReverbPreset::None: return "None";
		case AudioReverbPreset::Room: return "Room";
		case AudioReverbPreset::Hall: return "Hall";
		case AudioReverbPreset::Cave: return "Cave";
		case AudioReverbPreset::Corridor: return "Corridor";
		case AudioReverbPreset::Outdoor: return "Outdoor";
		}
		return "Room";
	}

	AudioReverbPreset AudioReverbPresetFromString(const std::string& value, AudioReverbPreset fallback)
	{
		if (value == "None") return AudioReverbPreset::None;
		if (value == "Room") return AudioReverbPreset::Room;
		if (value == "Hall") return AudioReverbPreset::Hall;
		if (value == "Cave") return AudioReverbPreset::Cave;
		if (value == "Corridor") return AudioReverbPreset::Corridor;
		if (value == "Outdoor") return AudioReverbPreset::Outdoor;
		return fallback;
	}

	void AudioReverbZone::ApplyPreset(AudioReverbPreset preset)
	{
		preset_ = preset;
		switch (preset_)
		{
		case AudioReverbPreset::None:
			reverbSend_ = 0.0f;
			decayTime_ = 0.1f;
			wetLevel_ = 0.0f;
			dryLevel_ = 1.0f;
			break;
		case AudioReverbPreset::Room:
			reverbSend_ = 0.45f;
			decayTime_ = 1.2f;
			wetLevel_ = 0.25f;
			dryLevel_ = 1.0f;
			break;
		case AudioReverbPreset::Hall:
			reverbSend_ = 0.7f;
			decayTime_ = 2.8f;
			wetLevel_ = 0.45f;
			dryLevel_ = 0.95f;
			break;
		case AudioReverbPreset::Cave:
			reverbSend_ = 0.9f;
			decayTime_ = 4.0f;
			wetLevel_ = 0.6f;
			dryLevel_ = 0.85f;
			break;
		case AudioReverbPreset::Corridor:
			reverbSend_ = 0.55f;
			decayTime_ = 1.8f;
			wetLevel_ = 0.35f;
			dryLevel_ = 0.95f;
			break;
		case AudioReverbPreset::Outdoor:
			reverbSend_ = 0.15f;
			decayTime_ = 0.7f;
			wetLevel_ = 0.1f;
			dryLevel_ = 1.0f;
			break;
		}
	}

	bool AudioReverbZone::Contains(const Vector3& position) const
	{
		if (!enabled_)
		{
			return false;
		}

		const Vector3 halfSize = size_ * 0.5f;
		return std::fabs(position.x - center_.x) <= halfSize.x &&
			std::fabs(position.y - center_.y) <= halfSize.y &&
			std::fabs(position.z - center_.z) <= halfSize.z;
	}

	void AudioReverbZone::SetSize(const Vector3& size)
	{
		size_ = {
			std::max(size.x, 0.01f),
			std::max(size.y, 0.01f),
			std::max(size.z, 0.01f)
		};
	}

	void AudioReverbZone::SetReverbSend(float reverbSend)
	{
		reverbSend_ = std::clamp(reverbSend, 0.0f, 1.0f);
	}

	void AudioReverbZone::SetDecayTime(float decayTime)
	{
		decayTime_ = std::clamp(decayTime, 0.1f, 10.0f);
	}

	void AudioReverbZone::SetWetLevel(float wetLevel)
	{
		wetLevel_ = std::clamp(wetLevel, 0.0f, 1.0f);
	}

	void AudioReverbZone::SetDryLevel(float dryLevel)
	{
		dryLevel_ = std::clamp(dryLevel, 0.0f, 1.0f);
	}
}
