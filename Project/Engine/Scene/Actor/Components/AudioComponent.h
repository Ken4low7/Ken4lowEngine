#pragma once
#include "ActorComponent.h"
#include "AudioManager.h"
#include "ComponentProperty.h"

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///   Actorに2D音声再生機能を追加するComponentクラス
	/// -------------------------------------------------------------
	class AudioComponent : public ActorComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		void Initialize() override;
		void Update(float deltaTime) override;
		void DrawImGui() override;
		void Finalize() override;

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		std::string GetClassTypeName() const override
		{
			return "AudioComponent"; // AudioComponentとして保存する。
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 再生制御 ---------- ///

		void Play();
		void Stop();
		bool IsPlaying() const;

	public: /// ---------- 設定取得 ---------- ///

		const std::string& GetSoundPath() const { return soundPath_; }
		void SetSoundPath(const std::string& soundPath);

		float GetVolume() const { return volume_; }
		void SetVolume(float volume);

		float GetPitch() const { return pitch_; }
		void SetPitch(float pitch);

		bool IsLoop() const { return loop_; }
		void SetLoop(bool loop) { loop_ = loop; }

		bool IsPlayOnStart() const { return playOnStart_; }
		void SetPlayOnStart(bool playOnStart) { playOnStart_ = playOnStart; }

		bool IsEnabled() const { return enabled_; }
		void SetEnabled(bool enabled);

		std::vector<ComponentProperty> CreateProperties(bool includeSoundPath = true);

	private: /// ---------- メンバ変数 ---------- ///

		std::string soundPath_;
		float volume_ = 1.0f;
		float pitch_ = 1.0f;
		bool loop_ = false;
		bool playOnStart_ = false;
		bool enabled_ = true;

		AudioManager::AudioHandle audioHandle_ = AudioManager::InvalidAudioHandle;
	};
}
