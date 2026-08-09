#pragma once
#include "SceneComponent.h"
#include "AudioManager.h"
#include "ComponentProperty.h"
#include "Vector3.h"

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// Actorの3D位置に基づいて距離減衰する音声を再生するComponentクラス
	/// -------------------------------------------------------------
	class WorldAudioComponent : public SceneComponent
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void DrawImGui() override;
		void Finalize() override;

		std::string GetClassTypeName() const override
		{
			return "WorldAudioComponent";
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		void Play();
		void Stop();
		void Restart();
		bool IsPlaying() const;

		void SetSoundPath(const std::string& soundPath);
		void SetVolume(float volume);
		void SetPitch(float pitch);
		void SetLoop(bool loop) { loop_ = loop; }
		void SetPlayOnStart(bool playOnStart) { playOnStart_ = playOnStart; }
		void SetEnabled(bool enabled);
		void SetMinDistance(float minDistance);
		void SetMaxDistance(float maxDistance);
		void SetRolloff(float rolloff);
		void SetFollowOwner(bool followOwner);
		void SetSpatialPanEnabled(bool enabled);
		void SetPanStrength(float panStrength);
		void SetDopplerEnabled(bool enabled);
		void SetDopplerFactor(float dopplerFactor);
		void SetMinDopplerPitch(float pitch);
		void SetMaxDopplerPitch(float pitch);
		void SetReverbSend(float reverbSend);
		void SetOcclusionEnabled(bool enabled);
		void SetOcclusionVolumeScale(float scale);
		void SetOcclusionLowPassAmount(float amount);

		std::vector<ComponentProperty> CreateProperties(bool includeSoundPath = true);

	private:
		void UpdateRuntimeAudio(float deltaTime);
		float CalculateDistanceToListener(const Vector3& listenerPosition) const;
		float CalculateAttenuatedVolume(float distance) const;
		float CalculatePan(const Vector3& listenerPosition, const Vector3& listenerRight) const;
		float CalculateDopplerPitch(float deltaTime, const Vector3& listenerPosition, const Vector3& listenerVelocity);
		void SanitizeDistanceRange();
		void SanitizeDopplerRange();
		Vector3 GetCurrentSourcePosition() const;

	private:
		std::string soundPath_;
		float volume_ = 1.0f;
		float pitch_ = 1.0f;
		bool loop_ = false;
		bool playOnStart_ = false;
		bool enabled_ = true;
		float minDistance_ = 1.0f;
		float maxDistance_ = 30.0f;
		float rolloff_ = 1.0f;
		bool followOwner_ = true;
		bool spatialPanEnabled_ = true;
		float panStrength_ = 1.0f;
		bool dopplerEnabled_ = false;
		float dopplerFactor_ = 1.0f;
		float minDopplerPitch_ = 0.5f;
		float maxDopplerPitch_ = 2.0f;
		float reverbSend_ = 0.0f;
		bool occlusionEnabled_ = false;
		float occlusionVolumeScale_ = 0.5f;
		float occlusionLowPassAmount_ = 0.5f;

		Vector3 sourcePosition_{ 0.0f, 0.0f, 0.0f };
		Vector3 previousSourcePosition_{ 0.0f, 0.0f, 0.0f };
		Vector3 sourceVelocity_{ 0.0f, 0.0f, 0.0f };
		float currentDistance_ = 0.0f;
		float currentAttenuatedVolume_ = 0.0f;
		float currentPan_ = 0.0f;
		float currentDopplerPitch_ = 1.0f;
		float currentOcclusion_ = 0.0f;
		AudioManager::AudioHandle audioHandle_ = AudioManager::InvalidAudioHandle;
	};
}
