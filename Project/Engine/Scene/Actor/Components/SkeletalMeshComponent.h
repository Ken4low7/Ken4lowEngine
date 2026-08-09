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
	/// Actorにボーン付き3Dモデル描画とアニメーション再生機能を追加するComponentクラス。
	/// -------------------------------------------------------------
	class SkeletalMeshComponent : public SceneComponent
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void PostPhysicsUpdate(float deltaTime) override;
		void Draw() override;
		void DrawShadow() override
		{
			if (!visible_ || !IsCastShadowEnabled() || !animationModel_ || !hasMesh_)
			{
				return;
			}

			SyncTransformToAnimationModel();
			animationModel_->DrawShadow(); // Compute Skinning済みの現在姿勢をShadow Mapへ描画する。
		}
		bool SupportsShadowCasting() const override { return true; }
		bool SupportsEditorObjectId() const override { return true; }
		void DrawEditorObjectId(uint32_t objectId) override
		{
			if (!visible_ || !IsActiveInHierarchy() || !animationModel_ || !hasMesh_)
			{
				return;
			}

			SyncTransformToAnimationModel();
			DrawAnimationModelObjectId(*animationModel_, objectId); // Skeletal MeshもComponent固有IDとして選択可能にする。
		}

		void DrawImGui() override;
		void Finalize() override;

		std::string GetClassTypeName() const override
		{
			return "SkeletalMeshComponent";
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		void SetModelPath(std::string_view modelPath);
		void SetAnimationName(std::string_view animationName);
		void SetVisible(bool visible);
		void SetLoop(bool loop);
		void SetPlayOnStart(bool playOnStart);
		void SetPlaybackSpeed(float playbackSpeed);

		/// <summary>共有MaterialAssetのIDを設定し、生成済みAnimationModelへ即時反映します。</summary>
		void SetMaterialAssetId(std::string_view assetId);

		/// <summary>Component固有Material Overrideの有効状態を切り替えます。</summary>
		void SetMaterialOverrideEnabled(bool enabled);

		/// <summary>Material Bindingの読み取り専用情報を取得します。</summary>
		const MaterialBinding& GetMaterialBinding() const { return materialBinding_; }

		void Play();
		void Stop();
		void Pause();
		void Restart();
		bool IsPlaying() const;
		std::vector<ComponentProperty> CreateProperties(bool includeModelPath = true, bool includeAnimationName = true);

	private:
		bool ReloadSkeletalModel();
		void RequestReload();
		void ProcessReloadRequest();
		void ReleaseAnimationModel();
		void ApplyPlaybackSettings();
		void SyncTransformToAnimationModel();
		bool SelectConfiguredAnimation(bool resetTime);
		bool EnsureAnimationSelection(bool resetTime);

		/// <summary>共有AssetまたはComponent固有OverrideをAnimationModelへ反映します。</summary>
		void ApplyMaterialBinding();

		/// <summary>共有MaterialAssetの更新世代が変わった場合だけ再反映します。</summary>
		void RefreshSharedMaterialBinding();

		/// <summary>共通Material Binding Editorと現在状態を日本語表示します。</summary>
		void DrawMaterialBindingImGui();

	private:
		std::unique_ptr<AnimationModel> animationModel_;
		std::string modelPath_;
		std::string animationName_;
		std::string modelStatus_ = "Empty";
		MaterialBinding materialBinding_{}; // 共有Asset参照とComponent固有Overrideを保持する。
		std::string materialBindingStatus_ = "モデル既定Materialを使用中";
		uint64_t materialRepositoryRevision_ = 0; // MaterialPresetのライブ更新を検知するRepository世代。
		bool hasMesh_ = false;
		bool hasSkeleton_ = false;
		bool visible_ = true;
		bool loop_ = true;
		bool playOnStart_ = true;
		float playbackSpeed_ = 1.0f;
		bool isPlaying_ = false;
		bool isPaused_ = false;
		bool hasInitialized_ = false;
		bool reloadRequested_ = false;
		bool resumeAfterReload_ = false;
	};
}