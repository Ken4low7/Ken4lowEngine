#pragma once
#include "SceneComponent.h"
#include "AnimationModel.h"
#include "AnimationModelObjectIdRenderer.h"
#include "ComponentProperty.h"
#include "MaterialBinding.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// ボーンを使わないNode Transform Animation付きモデルを再生するComponentクラス。
	/// -------------------------------------------------------------
	class AnimatedModelComponent : public SceneComponent
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void PostPhysicsUpdate(float deltaTime) override;
		void Draw() override;
		void DrawShadow() override
		{
			if (!visible_ || !IsCastShadowEnabled() || !animatedModel_ || !hasMesh_)
			{
				return;
			}

			SyncTransformToAnimatedModel();
			animatedModel_->DrawShadow(); // 現在のNode Animation姿勢をShadow Mapへ反映する。
		}
		bool SupportsShadowCasting() const override { return true; }
		bool SupportsEditorObjectId() const override { return true; }
		void DrawEditorObjectId(uint32_t objectId) override
		{
			if (!visible_ || !IsActiveInHierarchy() || !animatedModel_ || !hasMesh_)
			{
				return;
			}

			SyncTransformToAnimatedModel();
			DrawAnimationModelObjectId(*animatedModel_, objectId); // Node Animationモデルも同じComponent IDで専用Targetへ描画する。
		}

		void DrawImGui() override;
		void Finalize() override;

		std::string GetClassTypeName() const override
		{
			return "AnimatedModelComponent";
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		void SetModelPath(std::string_view modelPath);
		void SetAnimationName(std::string_view animationName);
		void SetVisible(bool visible) { visible_ = visible; }
		void SetLoop(bool loop);
		void SetPlayOnStart(bool playOnStart) { playOnStart_ = playOnStart; }
		void SetPlaybackSpeed(float playbackSpeed);

		/// <summary>共有MaterialAssetのIDを設定し、生成済みAnimationModelへ即時反映します。</summary>
		void SetMaterialAssetId(std::string_view assetId);

		/// <summary>Component固有Material Overrideの有効状態を切り替えます。</summary>
		void SetMaterialOverrideEnabled(bool enabled);

		/// <summary>Material Bindingの読み取り専用情報を取得します。</summary>
		const MaterialBinding& GetMaterialBinding() const { return materialBinding_; }

		void Play();
		void Stop();
		void Restart();
		bool IsPlaying() const;

		std::vector<ComponentProperty> CreateProperties(bool includeModelPath = true, bool includeAnimationName = true);

	private:
		bool ReloadAnimatedModel();
		void RequestReload();
		void ProcessReloadRequest();
		void ReleaseAnimatedModel();
		void ApplyPlaybackSettings();
		void SyncTransformToAnimatedModel();
		bool SelectConfiguredAnimation(bool resetTime);
		bool EnsureAnimationSelection(bool resetTime);

		/// <summary>共有AssetまたはComponent固有OverrideをAnimationModelへ反映します。</summary>
		void ApplyMaterialBinding();

		/// <summary>共有MaterialAssetの更新世代が変わった場合だけ再反映します。</summary>
		void RefreshSharedMaterialBinding();

		/// <summary>共通Material Binding Editorと現在状態を日本語表示します。</summary>
		void DrawMaterialBindingImGui();

	private:
		std::unique_ptr<AnimationModel> animatedModel_;
		std::string modelPath_;
		std::string animationName_;
		std::string modelStatus_ = "Empty";
		MaterialBinding materialBinding_{}; // 共有Asset参照とComponent固有Overrideを保持する。
		std::string materialBindingStatus_ = "モデル既定Materialを使用中";
		uint64_t materialRepositoryRevision_ = 0; // MaterialPresetのライブ更新を検知するRepository世代。
		bool hasMesh_ = false;
		bool visible_ = true;
		bool loop_ = true;
		bool playOnStart_ = true;
		float playbackSpeed_ = 1.0f;
		bool isPlaying_ = false;
		bool hasInitialized_ = false;
		bool reloadRequested_ = false;
		bool resumeAfterReload_ = false;
	};
}