#define NOMINMAX
#include "WorldAudioComponent.h"

#include "AssetPathSelector.h"
#include "CameraManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void WorldAudioComponent::Initialize()
	{
		SceneComponent::Initialize();
		sourcePosition_ = GetWorldPosition();
		previousSourcePosition_ = sourcePosition_;
		UpdateRuntimeAudio(0.0f);

		if (playOnStart_)
		{
			Play(); // Component開始時に3D音声を再生する
		}
	}

	void WorldAudioComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);

		if (!enabled_)
		{
			Stop(); // 無効化されているComponentの再生を停止する
			return;
		}

		UpdateRuntimeAudio(deltaTime);

		if (audioHandle_ != AudioManager::InvalidAudioHandle && !AudioManager::GetInstance()->IsPlaying(audioHandle_))
		{
			audioHandle_ = AudioManager::InvalidAudioHandle;
		}
	}

	void WorldAudioComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();

#ifdef USE_IMGUI
		ImGui::SeparatorText("ワールドオーディオコンポーネント");

		ImGui::Text("現在の音声: %s", soundPath_.empty() ? "未選択" : soundPath_.c_str());

		std::string selectedSoundPath = soundPath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##WorldAudioComponentSoundPath", selectedSoundPath, AssetType::Audio))
		{
			SetSoundPath(selectedSoundPath);
		}

		ComponentPropertyUtility::DrawImGui(CreateProperties(false));

		if (ImGui::Button("再生"))
		{
			Play();
		}
		ImGui::SameLine();
		if (ImGui::Button("停止"))
		{
			Stop();
		}
		ImGui::SameLine();
		if (ImGui::Button("再再生"))
		{
			Restart();
		}

		ImGui::Text("再生中: %s", IsPlaying() ? "はい" : "いいえ");
		ImGui::Text("Listenerまでの距離: %.2f", currentDistance_);
		ImGui::Text("減衰後の音量: %.2f", currentAttenuatedVolume_);
		ImGui::Text("現在パン: %.2f", currentPan_);
		ImGui::Text("現在ドップラーピッチ: %.2f", currentDopplerPitch_);
		ImGui::Text("音源速度: %.2f, %.2f, %.2f", sourceVelocity_.x, sourceVelocity_.y, sourceVelocity_.z);
		const Vector3& listenerVelocity = CameraManager::GetInstance()->GetAudioListener().GetVelocity();
		ImGui::Text("Listener速度: %.2f, %.2f, %.2f", listenerVelocity.x, listenerVelocity.y, listenerVelocity.z);
		ImGui::Text("現在遮蔽量: %.2f", currentOcclusion_);
#endif // USE_IMGUI
	}

	void WorldAudioComponent::Finalize()
	{
		Stop();
	}

	void WorldAudioComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);

		outJson["Class"] = GetClassTypeName();
		ComponentPropertyUtility::ToJson(const_cast<WorldAudioComponent*>(this)->CreateProperties(), outJson);
	}

	void WorldAudioComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);

		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);

		SanitizeDistanceRange();
		SanitizeDopplerRange();
	}

	void WorldAudioComponent::Play()
	{
		if (!enabled_ || soundPath_.empty())
		{
			return; // 無効またはパス未設定の場合は再生しない
		}

		Stop();

		sourcePosition_ = GetCurrentSourcePosition();
		previousSourcePosition_ = sourcePosition_;
		UpdateRuntimeAudio(0.0f);
		audioHandle_ = AudioManager::GetInstance()->PlaySEWithHandle(soundPath_, currentAttenuatedVolume_, pitch_, loop_);
		AudioManager::GetInstance()->SetVoicePan(audioHandle_, currentPan_);
		AudioManager::GetInstance()->SetVoicePitch(audioHandle_, pitch_ * currentDopplerPitch_);
	}

	void WorldAudioComponent::Stop()
	{
		if (audioHandle_ == AudioManager::InvalidAudioHandle)
		{
			return;
		}

		AudioManager::GetInstance()->Stop(audioHandle_);
		audioHandle_ = AudioManager::InvalidAudioHandle;
	}

	void WorldAudioComponent::Restart()
	{
		Stop();
		Play();
	}

	bool WorldAudioComponent::IsPlaying() const
	{
		return audioHandle_ != AudioManager::InvalidAudioHandle &&
			AudioManager::GetInstance()->IsPlaying(audioHandle_);
	}

	void WorldAudioComponent::SetSoundPath(const std::string& soundPath)
	{
		if (soundPath_ == soundPath)
		{
			return;
		}

		Stop();
		soundPath_ = soundPath;
	}

	void WorldAudioComponent::SetVolume(float volume)
	{
		volume_ = std::clamp(volume, 0.0f, 4.0f);
		UpdateRuntimeAudio(0.0f);
	}

	void WorldAudioComponent::SetPitch(float pitch)
	{
		pitch_ = std::clamp(pitch, 0.25f, 4.0f);
		if (audioHandle_ != AudioManager::InvalidAudioHandle)
		{
			AudioManager::GetInstance()->SetVoicePitch(audioHandle_, pitch_ * currentDopplerPitch_);
		}
	}

	void WorldAudioComponent::SetEnabled(bool enabled)
	{
		enabled_ = enabled;
		if (!enabled_)
		{
			Stop();
		}
	}

	void WorldAudioComponent::SetMinDistance(float minDistance)
	{
		minDistance_ = std::max(0.0f, minDistance);
		SanitizeDistanceRange();
		UpdateRuntimeAudio(0.0f);
	}

	void WorldAudioComponent::SetMaxDistance(float maxDistance)
	{
		maxDistance_ = std::max(0.01f, maxDistance);
		SanitizeDistanceRange();
		UpdateRuntimeAudio(0.0f);
	}

	void WorldAudioComponent::SetRolloff(float rolloff)
	{
		rolloff_ = std::max(0.001f, rolloff);
		UpdateRuntimeAudio(0.0f);
	}

	void WorldAudioComponent::SetFollowOwner(bool followOwner)
	{
		followOwner_ = followOwner;
		if (followOwner_)
		{
			sourcePosition_ = GetWorldPosition();
			UpdateRuntimeAudio(0.0f);
		}
	}

	void WorldAudioComponent::SetSpatialPanEnabled(bool enabled)
	{
		spatialPanEnabled_ = enabled;
		UpdateRuntimeAudio(0.0f);
	}

	void WorldAudioComponent::SetPanStrength(float panStrength)
	{
		panStrength_ = std::clamp(panStrength, 0.0f, 4.0f);
		UpdateRuntimeAudio(0.0f);
	}

	void WorldAudioComponent::SetDopplerEnabled(bool enabled)
	{
		dopplerEnabled_ = enabled;
		UpdateRuntimeAudio(0.0f);
	}

	void WorldAudioComponent::SetDopplerFactor(float dopplerFactor)
	{
		dopplerFactor_ = std::clamp(dopplerFactor, 0.0f, 10.0f);
		UpdateRuntimeAudio(0.0f);
	}

	void WorldAudioComponent::SetMinDopplerPitch(float pitch)
	{
		minDopplerPitch_ = std::clamp(pitch, 0.25f, 4.0f);
		SanitizeDopplerRange();
		UpdateRuntimeAudio(0.0f);
	}

	void WorldAudioComponent::SetMaxDopplerPitch(float pitch)
	{
		maxDopplerPitch_ = std::clamp(pitch, 0.25f, 4.0f);
		SanitizeDopplerRange();
		UpdateRuntimeAudio(0.0f);
	}

	void WorldAudioComponent::SetReverbSend(float reverbSend)
	{
		reverbSend_ = std::clamp(reverbSend, 0.0f, 1.0f);
	}

	void WorldAudioComponent::SetOcclusionEnabled(bool enabled)
	{
		occlusionEnabled_ = enabled;
		UpdateRuntimeAudio(0.0f);
	}

	void WorldAudioComponent::SetOcclusionVolumeScale(float scale)
	{
		occlusionVolumeScale_ = std::clamp(scale, 0.0f, 1.0f);
		UpdateRuntimeAudio(0.0f);
	}

	void WorldAudioComponent::SetOcclusionLowPassAmount(float amount)
	{
		occlusionLowPassAmount_ = std::clamp(amount, 0.0f, 1.0f);
	}

	void WorldAudioComponent::UpdateRuntimeAudio(float deltaTime)
	{
		CameraManager::GetInstance()->UpdateAudioListener(deltaTime);
		const auto& listener = CameraManager::GetInstance()->GetAudioListener();

		previousSourcePosition_ = sourcePosition_;
		if (followOwner_)
		{
			sourcePosition_ = GetWorldPosition();
		}
		else
		{
			sourcePosition_ = GetCurrentSourcePosition();
		}

		if (deltaTime > 0.0f && std::isfinite(deltaTime))
		{
			sourceVelocity_ = (sourcePosition_ - previousSourcePosition_) / deltaTime;
			sourceVelocity_.x = std::clamp(sourceVelocity_.x, -10000.0f, 10000.0f);
			sourceVelocity_.y = std::clamp(sourceVelocity_.y, -10000.0f, 10000.0f);
			sourceVelocity_.z = std::clamp(sourceVelocity_.z, -10000.0f, 10000.0f);
		}

		SanitizeDistanceRange();
		SanitizeDopplerRange();
		currentDistance_ = CalculateDistanceToListener(listener.GetPosition());
		currentAttenuatedVolume_ = CalculateAttenuatedVolume(currentDistance_);
		currentPan_ = CalculatePan(listener.GetPosition(), listener.GetRight());
		currentDopplerPitch_ = CalculateDopplerPitch(deltaTime, listener.GetPosition(), listener.GetVelocity());
		currentOcclusion_ = occlusionEnabled_ ? 0.0f : 0.0f;

		const float occlusionVolume = occlusionEnabled_ ? (1.0f - currentOcclusion_ * (1.0f - occlusionVolumeScale_)) : 1.0f;
		currentAttenuatedVolume_ *= occlusionVolume;

		if (audioHandle_ != AudioManager::InvalidAudioHandle)
		{
			AudioManager::GetInstance()->SetVoiceVolume(audioHandle_, currentAttenuatedVolume_);
			AudioManager::GetInstance()->SetVoicePan(audioHandle_, currentPan_);
			AudioManager::GetInstance()->SetVoicePitch(audioHandle_, pitch_ * currentDopplerPitch_);
		}
	}

	float WorldAudioComponent::CalculateDistanceToListener(const Vector3& listenerPosition) const
	{
		const Vector3 toListener = {
			listenerPosition.x - sourcePosition_.x,
			listenerPosition.y - sourcePosition_.y,
			listenerPosition.z - sourcePosition_.z
		};

		const float distance = Vector3::Length(toListener);
		return std::isfinite(distance) ? distance : 0.0f;
	}

	float WorldAudioComponent::CalculateAttenuatedVolume(float distance) const
	{
		if (!enabled_ || volume_ <= 0.0f)
		{
			return 0.0f;
		}

		if (distance <= minDistance_)
		{
			return volume_;
		}
		if (distance >= maxDistance_)
		{
			return 0.0f;
		}

		const float range = std::max(maxDistance_ - minDistance_, 0.0001f);
		const float rate = std::clamp((distance - minDistance_) / range, 0.0f, 1.0f);
		const float attenuation = std::pow(1.0f - rate, std::max(rolloff_, 0.001f));
		return volume_ * attenuation;
	}

	float WorldAudioComponent::CalculatePan(const Vector3& listenerPosition, const Vector3& listenerRight) const
	{
		if (!spatialPanEnabled_)
		{
			return 0.0f;
		}

		const Vector3 toSource = sourcePosition_ - listenerPosition;
		if (Vector3::LengthSquared(toSource) <= 0.000001f)
		{
			return 0.0f;
		}

		const float pan = Vector3::Dot(Vector3::NormalizeSafe(toSource), listenerRight) * panStrength_;
		return std::clamp(pan, -1.0f, 1.0f);
	}

	float WorldAudioComponent::CalculateDopplerPitch(float deltaTime, const Vector3& listenerPosition, const Vector3& listenerVelocity)
	{
		if (!dopplerEnabled_)
		{
			return 1.0f;
		}

		const Vector3 toSource = sourcePosition_ - listenerPosition;
		if (deltaTime <= 0.0f || Vector3::LengthSquared(toSource) <= 0.000001f)
		{
			return currentDopplerPitch_;
		}

		const Vector3 direction = Vector3::NormalizeSafe(toSource);
		const float relativeSpeed = Vector3::Dot(sourceVelocity_ - listenerVelocity, direction);
		const float dopplerPitch = 1.0f - relativeSpeed * dopplerFactor_ * 0.001f;
		return std::clamp(dopplerPitch, minDopplerPitch_, maxDopplerPitch_);
	}

	void WorldAudioComponent::SanitizeDistanceRange()
	{
		minDistance_ = std::max(0.0f, minDistance_);
		maxDistance_ = std::max(maxDistance_, minDistance_ + 0.01f);
		rolloff_ = std::max(0.001f, rolloff_);
	}

	void WorldAudioComponent::SanitizeDopplerRange()
	{
		minDopplerPitch_ = std::clamp(minDopplerPitch_, 0.25f, 4.0f);
		maxDopplerPitch_ = std::clamp(maxDopplerPitch_, minDopplerPitch_, 4.0f);
	}

	Vector3 WorldAudioComponent::GetCurrentSourcePosition() const
	{
		return followOwner_ ? GetWorldPosition() : sourcePosition_;
	}

	std::vector<ComponentProperty> WorldAudioComponent::CreateProperties(bool includeSoundPath)
	{
		std::vector<ComponentProperty> properties = {
			{ "Enabled", "有効", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return enabled_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetEnabled(*typedValue); } } },
			{ "Volume", "音量", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return volume_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetVolume(*typedValue); } }, 0.0f, 4.0f, 0.01f, true },
			{ "Pitch", "ピッチ", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return pitch_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetPitch(*typedValue); } }, 0.25f, 4.0f, 0.01f, true },
			{ "Loop", "ループ", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return loop_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetLoop(*typedValue); } } },
			{ "PlayOnStart", "開始時に再生", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return playOnStart_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetPlayOnStart(*typedValue); } } },
			{ "MinDistance", "最小距離", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return minDistance_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetMinDistance(*typedValue); } }, 0.0f, 10000.0f, 0.1f, true },
			{ "MaxDistance", "最大距離", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return maxDistance_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetMaxDistance(*typedValue); } }, 0.01f, 10000.0f, 0.1f, true },
			{ "Rolloff", "減衰カーブ", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return rolloff_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetRolloff(*typedValue); } }, 0.001f, 8.0f, 0.01f, true },
			{ "FollowOwner", "Actorに追従", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return followOwner_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetFollowOwner(*typedValue); } } },
			{ "SpatialPanEnabled", "空間パンニング", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return spatialPanEnabled_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetSpatialPanEnabled(*typedValue); } } },
			{ "PanStrength", "パンニング強度", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return panStrength_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetPanStrength(*typedValue); } }, 0.0f, 4.0f, 0.01f, true },
			{ "DopplerEnabled", "ドップラー", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return dopplerEnabled_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetDopplerEnabled(*typedValue); } } },
			{ "DopplerFactor", "ドップラー係数", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return dopplerFactor_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetDopplerFactor(*typedValue); } }, 0.0f, 10.0f, 0.01f, true },
			{ "MinDopplerPitch", "最小ドップラーピッチ", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return minDopplerPitch_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetMinDopplerPitch(*typedValue); } }, 0.25f, 4.0f, 0.01f, true },
			{ "MaxDopplerPitch", "最大ドップラーピッチ", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return maxDopplerPitch_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetMaxDopplerPitch(*typedValue); } }, 0.25f, 4.0f, 0.01f, true },
			{ "ReverbSend", "リバーブ送信量", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return reverbSend_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetReverbSend(*typedValue); } }, 0.0f, 1.0f, 0.01f, true },
			{ "OcclusionEnabled", "遮蔽", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return occlusionEnabled_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetOcclusionEnabled(*typedValue); } } },
			{ "OcclusionVolumeScale", "遮蔽時音量倍率", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return occlusionVolumeScale_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetOcclusionVolumeScale(*typedValue); } }, 0.0f, 1.0f, 0.01f, true },
			{ "OcclusionLowPassAmount", "ローパス量", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return occlusionLowPassAmount_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetOcclusionLowPassAmount(*typedValue); } }, 0.0f, 1.0f, 0.01f, true }
		};

		if (includeSoundPath)
		{
			properties.insert(properties.begin() + 1,
				{ "SoundPath", "サウンドパス", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return soundPath_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetSoundPath(*typedValue); } } });
		}

		return properties;
	}
}
