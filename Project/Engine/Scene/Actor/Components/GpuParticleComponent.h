#pragma once
#include "SceneComponent.h"
#include "ComponentProperty.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class GpuParticleEmitter;

	/// -------------------------------------------------------------
	///   ActorにGPUパーティクル演出を追加するComponentクラス
	/// -------------------------------------------------------------
	class GpuParticleComponent : public SceneComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		void Initialize() override;
		void Update(float deltaTime) override;
		void DrawImGui() override;
		void Finalize() override;

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		std::string GetClassTypeName() const override
		{
			return "GpuParticleComponent"; // GpuParticleComponentとして保存する。
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 再生制御 ---------- ///

		void Play();
		void Stop();
		void Restart();
		bool IsPlaying() const;

	public: /// ---------- ミニリフレクション ---------- ///

		std::vector<ComponentProperty> CreateProperties(bool includeAssetPaths = true);

	public: /// ---------- 設定取得 ---------- ///

		const std::string& GetEffectName() const { return effectName_; }
		void SetEffectName(const std::string& effectName) { effectName_ = effectName; }

		const std::string& GetEmitterName() const { return emitterName_; }
		void SetEmitterName(const std::string& emitterName) { emitterName_ = emitterName; }

		bool IsPlayOnStart() const { return playOnStart_; }
		void SetPlayOnStart(bool playOnStart) { playOnStart_ = playOnStart; }

		bool IsLoop() const { return loop_; }
		void SetLoop(bool loop) { loop_ = loop; }

		bool IsVisible() const { return visible_; }
		void SetVisible(bool visible);

		bool IsFollowOwner() const { return followOwner_; }
		void SetFollowOwner(bool followOwner) { followOwner_ = followOwner; }

		const Vector3& GetLocalOffset() const { return localOffset_; }
		void SetLocalOffset(const Vector3& localOffset) { localOffset_ = localOffset; }

		const Vector3& GetScale() const { return scale_; }
		void SetScale(const Vector3& scale) { scale_ = scale; }

	private: /// ---------- 内部処理 ---------- ///

		std::string ResolveEmitterName() const;
		Vector3 CalculateEmitterPosition() const;
		GpuParticleEmitter* EnsureEmitter();
		void SyncEmitterPosition();
		void ApplyPreset(const std::string& presetName);
		uint32_t CalculateBurstCount() const;

	private: /// ---------- メンバ変数 ---------- ///

		std::string effectName_ = "Smoke";
		std::string emitterName_;
		std::string presetName_ = "Smoke";
		std::string particleType_ = "Sprite";
		bool playOnStart_ = false;
		bool loop_ = false;
		bool visible_ = true;
		bool followOwner_ = true;
		Vector3 localOffset_{ 0.0f, 0.0f, 0.0f };
		Vector3 scale_{ 1.0f, 1.0f, 1.0f };

		float emissionRate_ = 10.0f;
		float lifeTime_ = 1.0f;
		Vector4 startColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 endColor_{ 1.0f, 1.0f, 1.0f, 0.0f };
		float startSize_ = 1.0f;
		float endSize_ = 1.0f;
		float velocityScale_ = 1.0f;

		std::string texturePath_ = "Effects/white.dds";
		std::string billboardMode_ = "Camera";
		std::string blendMode_ = "Alpha";
		std::string sortMode_ = "None";
		Vector2 startSize2D_{ 1.0f, 1.0f };
		Vector2 midSize2D_{ 1.0f, 1.0f };
		Vector2 endSize2D_{ 1.0f, 1.0f };
		float startRotation_ = 0.0f;
		float rotationSpeed_ = 0.0f;
		float rotationRandom_ = 0.0f;
		bool useSpriteSheet_ = false;
		int spriteSheetRows_ = 1;
		int spriteSheetColumns_ = 1;
		float spriteSheetFrameRate_ = 0.0f;

		std::string meshPath_;
		int meshId_ = 1000;
		Vector3 meshStartScale_{ 1.0f, 1.0f, 1.0f };
		Vector3 meshEndScale_{ 1.0f, 1.0f, 1.0f };
		Vector3 meshScaleRandom_{ 0.0f, 0.0f, 0.0f };
		Vector3 meshAngularVelocity_{ 0.0f, 0.0f, 0.0f };
		Vector3 meshAngularVelocityRandom_{ 0.0f, 0.0f, 0.0f };
		std::string meshAlignMode_ = "None";
		Vector4 meshTint_{ 1.0f, 1.0f, 1.0f, 1.0f };

		std::string emitterShape_ = "Point";
		float spawnRadius_ = 0.0f;
		Vector3 spawnBoxSize_{ 0.0f, 0.0f, 0.0f };
		float coneAngle_ = 30.0f;
		float coneLength_ = 1.0f;
		float ringRadius_ = 1.0f;
		float ringThickness_ = 0.1f;

		Vector3 baseVelocity_{ 0.0f, 1.0f, 0.0f };
		Vector3 velocityRandom_{ 0.0f, 0.0f, 0.0f };
		Vector3 direction_{ 0.0f, 1.0f, 0.0f };
		float speed_ = 0.0f;
		float speedRandom_ = 0.0f;
		float randomAngle_ = 0.0f;
		float outwardVelocity_ = 0.0f;
		float tangentialVelocity_ = 0.0f;
		float inheritActorVelocity_ = 0.0f;

		bool burstEnabled_ = true;
		int burstCount_ = 16;
		int burstRandom_ = 0;
		int burstRepeat_ = 1;
		float burstInterval_ = 0.0f;

		Vector4 midColor_{ 1.0f, 1.0f, 1.0f, 0.5f };
		float midTime_ = 0.5f;
		bool alphaFade_ = true;
		float lifeTimeRandom_ = 0.0f;
		Vector3 acceleration_{ 0.0f, 0.0f, 0.0f };
		Vector3 gravity_{ 0.0f, 0.0f, 0.0f };
		float damping_ = 0.0f;

		bool trailEnabled_ = false;
		std::string trailTexturePath_;
		float trailLength_ = 1.0f;
		float trailWidth_ = 0.1f;
		Vector4 trailColor_{ 1.0f, 1.0f, 1.0f, 0.6f };
		float trailFadeTime_ = 0.25f;
		float trailLifeTime_ = 0.25f;
		int trailSegments_ = 8;

		bool playing_ = false;
		std::string activeEmitterName_;
		Vector3 lockedPosition_{ 0.0f, 0.0f, 0.0f };
		float burstTimer_ = 0.0f;
		int burstRepeatRemaining_ = 0;
		Vector3 previousWorldPosition_{ 0.0f, 0.0f, 0.0f };
		Vector3 actorVelocity_{ 0.0f, 0.0f, 0.0f };
	};
}
