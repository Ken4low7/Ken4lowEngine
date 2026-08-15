#include "VfxCueCompiler.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Ken4lowEngine
{
namespace
{
	bool IsFinite(float value)
	{
		return std::isfinite(value);
	}

	bool IsFinite(const Vector3& value)
	{
		return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
	}

	std::string TrackPrefix(uint32_t trackIndex)
	{
		return "VFX track[" + std::to_string(trackIndex) + "]: ";
	}
}

bool VfxCueCompiler::Compile(
	const VfxCueDesc& desc,
	VfxCueProgram& outProgram,
	std::string* outError)
{
	std::string error;
	if (desc.schemaVersion != VfxCueDesc::kCurrentSchemaVersion)
	{
		error = "Unsupported VFX schemaVersion.";
	}
	else if (desc.cueName.empty())
	{
		error = "VFX cueName must not be empty.";
	}
	else if (!IsFinite(desc.duration) || desc.duration < 0.0f)
	{
		error = "VFX duration must be finite and non-negative.";
	}
	else if (desc.tracks.size() > VfxCueDesc::kMaxTracks)
	{
		error = "VFX track count exceeds kMaxTracks.";
	}

	VfxCueProgram compiled{};
	if (error.empty())
	{
		compiled.cueName = desc.cueName;
		compiled.loop = desc.loop;
		compiled.duration = desc.duration;
		compiled.instructions.reserve(desc.tracks.size());

		for (uint32_t index = 0; index < static_cast<uint32_t>(desc.tracks.size()); ++index)
		{
			const VfxCueTrackDesc& track = desc.tracks[index];
			if (!track.enabled)
			{
				continue;
			}
			if (!ValidateTrack(track, index, error))
			{
				break;
			}

			VfxCueInstruction instruction{};
			instruction.sourceTrackIndex = index;
			instruction.type = track.type;
			instruction.startTime = track.startTime;
			instruction.endTime = track.startTime + track.duration;
			instruction.localOffset = track.localOffset;
			instruction.payload = track.payload;
			compiled.duration = (std::max)(compiled.duration, instruction.endTime);
			compiled.instructions.push_back(std::move(instruction));
		}
	}

	if (error.empty() && desc.loop && compiled.duration <= 0.0f)
	{
		error = "Looping VFX cues require a positive compiled duration.";
	}

	if (!error.empty())
	{
		if (outError != nullptr)
		{
			*outError = error;
		}
		return false;
	}

	std::stable_sort(
		compiled.instructions.begin(),
		compiled.instructions.end(),
		[](const VfxCueInstruction& lhs, const VfxCueInstruction& rhs)
		{
			if (lhs.startTime == rhs.startTime)
			{
				return lhs.sourceTrackIndex < rhs.sourceTrackIndex;
			}
			return lhs.startTime < rhs.startTime;
		});

	// Authoring配列順に依存せず、Schedulerは開始時刻順Programだけを直線走査できるようにする。
	outProgram = std::move(compiled);
	if (outError != nullptr)
	{
		outError->clear();
	}
	return true;
}

bool VfxCueCompiler::ValidateTrack(
	const VfxCueTrackDesc& track,
	uint32_t trackIndex,
	std::string& outError)
{
	const std::string prefix = TrackPrefix(trackIndex);
	if (track.name.empty())
	{
		outError = prefix + "name must not be empty.";
		return false;
	}
	if (!IsFinite(track.startTime) || track.startTime < 0.0f ||
		!IsFinite(track.duration) || track.duration < 0.0f ||
		!IsFinite(track.localOffset))
	{
		outError = prefix + "timing/localOffset contains an invalid value.";
		return false;
	}

	switch (track.type)
	{
	case VfxCueTrackType::Particle:
	{
		const auto* payload = std::get_if<VfxParticleTrackPayload>(&track.payload);
		if (payload == nullptr || payload->effectName.empty())
		{
			outError = prefix + "Particle track requires VfxParticleTrackPayload and effectName.";
			return false;
		}
		break;
	}
	case VfxCueTrackType::Fluid2D:
	case VfxCueTrackType::VolumetricFluid:
	{
		const auto* payload = std::get_if<VfxFluidTrackPayload>(&track.payload);
		if (payload == nullptr ||
			!IsFinite(payload->localVelocity) ||
			!IsFinite(payload->radius) || payload->radius <= 0.0f ||
			!IsFinite(payload->velocityStrength) || payload->velocityStrength < 0.0f ||
			!IsFinite(payload->densityRate) ||
			!IsFinite(payload->temperatureRate) ||
			!IsFinite(payload->falloffExponent) || payload->falloffExponent <= 0.0f)
		{
			outError = prefix + "Fluid payload is invalid.";
			return false;
		}
		break;
	}
	case VfxCueTrackType::Light:
	{
		const auto* payload = std::get_if<VfxLightTrackPayload>(&track.payload);
		if (payload == nullptr || !IsFinite(payload->color) ||
			!IsFinite(payload->intensity) || payload->intensity < 0.0f ||
			!IsFinite(payload->range) || payload->range <= 0.0f)
		{
			outError = prefix + "Light payload is invalid.";
			return false;
		}
		break;
	}
	case VfxCueTrackType::PostEffect:
	{
		const auto* payload = std::get_if<VfxPostEffectTrackPayload>(&track.payload);
		if (payload == nullptr || payload->effectName.empty() ||
			!IsFinite(payload->weight) || payload->weight < 0.0f || payload->weight > 1.0f)
		{
			outError = prefix + "PostEffect payload is invalid.";
			return false;
		}
		break;
	}
	case VfxCueTrackType::CameraShake:
	{
		const auto* payload = std::get_if<VfxCameraShakeTrackPayload>(&track.payload);
		if (payload == nullptr ||
			!IsFinite(payload->translationAmplitude) ||
			!IsFinite(payload->rotationAmplitudeDegrees) ||
			!IsFinite(payload->frequency) || payload->frequency < 0.0f ||
			!IsFinite(payload->fovAmplitudeDegrees))
		{
			outError = prefix + "CameraShake payload is invalid.";
			return false;
		}
		break;
	}
	default:
		outError = prefix + "unsupported track type.";
		return false;
	}

	return true;
}

} // namespace Ken4lowEngine
