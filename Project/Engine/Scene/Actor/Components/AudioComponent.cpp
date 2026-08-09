#define NOMINMAX
#include "AudioComponent.h"
#include "AssetPathSelector.h"

#include <algorithm>
#include <array>
#include <cstdio>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void AudioComponent::Initialize()
	{
		if (playOnStart_)
		{
			Play(); // Component開始時に指定された音声を再生する
		}
	}

	void AudioComponent::Update([[maybe_unused]] float deltaTime)
	{
		if (!enabled_)
		{
			Stop(); // 無効化されているComponentの再生を停止する
			return;
		}

		if (audioHandle_ != AudioManager::InvalidAudioHandle && !AudioManager::GetInstance()->IsPlaying(audioHandle_))
		{
			audioHandle_ = AudioManager::InvalidAudioHandle;
		}
	}

	void AudioComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("オーディオコンポーネント");

		ImGui::Text("現在の音声: %s", soundPath_.empty() ? "未選択" : soundPath_.c_str());

		std::string selectedSoundPath = soundPath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##AudioComponentSoundPath", selectedSoundPath, AssetType::Audio))
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

		ImGui::Text("再生中: %s", IsPlaying() ? "はい" : "いいえ");
#endif // USE_IMGUI
	}

	void AudioComponent::Finalize()
	{
		Stop();
	}

	void AudioComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson); // ActorComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // AudioComponentとして保存する
		ComponentPropertyUtility::ToJson(const_cast<AudioComponent*>(this)->CreateProperties(), outJson);
	}

	void AudioComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson); // ActorComponent共通情報をJSONから復元する

		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
	}

	void AudioComponent::Play()
	{
		if (!enabled_ || soundPath_.empty())
		{
			return; // 無効またはパス未設定の場合は再生しない
		}

		Stop();

		audioHandle_ = AudioManager::GetInstance()->PlaySEWithHandle(soundPath_, volume_, pitch_, loop_);
	}

	void AudioComponent::Stop()
	{
		if (audioHandle_ == AudioManager::InvalidAudioHandle)
		{
			return;
		}

		AudioManager::GetInstance()->Stop(audioHandle_);
		audioHandle_ = AudioManager::InvalidAudioHandle;
	}

	bool AudioComponent::IsPlaying() const
	{
		return audioHandle_ != AudioManager::InvalidAudioHandle &&
			AudioManager::GetInstance()->IsPlaying(audioHandle_);
	}

	void AudioComponent::SetSoundPath(const std::string& soundPath)
	{
		if (soundPath_ == soundPath)
		{
			return;
		}

		Stop();
		soundPath_ = soundPath;
	}

	void AudioComponent::SetVolume(float volume)
	{
		volume_ = std::clamp(volume, 0.0f, 4.0f);
		if (audioHandle_ != AudioManager::InvalidAudioHandle)
		{
			AudioManager::GetInstance()->SetVoiceVolume(audioHandle_, volume_);
		}
	}

	void AudioComponent::SetPitch(float pitch)
	{
		pitch_ = std::clamp(pitch, 0.25f, 4.0f);
		if (audioHandle_ != AudioManager::InvalidAudioHandle)
		{
			AudioManager::GetInstance()->SetVoicePitch(audioHandle_, pitch_);
		}
	}

	std::vector<ComponentProperty> AudioComponent::CreateProperties(bool includeSoundPath)
	{
		std::vector<ComponentProperty> properties = {
			{ "Enabled", "有効", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return enabled_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetEnabled(*typedValue); } } },
			{ "Volume", "音量", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return volume_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetVolume(*typedValue); } }, 0.0f, 4.0f, 0.01f, true },
			{ "Pitch", "ピッチ", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return pitch_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetPitch(*typedValue); } }, 0.25f, 4.0f, 0.01f, true },
			{ "Loop", "ループ", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return loop_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetLoop(*typedValue); } } },
			{ "PlayOnStart", "開始時に再生", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return playOnStart_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetPlayOnStart(*typedValue); } } }
		};

		if (includeSoundPath)
		{
			properties.insert(properties.begin() + 1,
				{ "SoundPath", "サウンドパス", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return soundPath_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetSoundPath(*typedValue); } } });
		}

		return properties;
	}

	void AudioComponent::SetEnabled(bool enabled)
	{
		enabled_ = enabled;
		if (!enabled_)
		{
			Stop();
		}
	}
}
