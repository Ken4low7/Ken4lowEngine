#define NOMINMAX
#include "AnimationModel.h"
#include <algorithm>
#include <AnimationPipelineBuilder.h>
#include <TextureManager.h>
#include <DirectXCommon.h>
#include <CameraManager.h>
#include <ResourceManager.h>
#include "AssimpLoader.h"
#include <SRVManager.h>
#include <UAVManager.h>
#include <GameTimer.h>
#include <LightManager.h>
#include <chrono>
#include <cmath>

#include "AnimationLoader.h"
#include "AnimationSampler.h"
#include "AnimationModelLODBuilder.h"
#include "AnimationModelDebugView.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				　		 初期化処理
	/// -------------------------------------------------------------
	void AnimationModel::Initialize(const std::string& fileName, bool isSkinning)
	{
		// 既存リソースを破棄（※lodSourceFiles_ は保持）
		Clear();

		fileName_ = fileName; // ファイル名保存
		animationFileName_.clear();

		// 共通セットアップ
		InitializeCommon();

		// モデル＆アニメ読み込み
		LoadBaseModelAndAnimation();

		// スケルトン生成
		CreateSkeletonFromModel();

		// マテリアル＆メッシュ初期化
		InitializeMaterialAndMeshes();

		// 環境マップ
		InitializeEnvironmentMap();

		// 定数バッファ
		CreateConstantBuffers();

		// ボディパートコライダー
		BuildBodyPartColliders();

		// LOD構築（メッシュ＆スキンクラスタ）
		BuildLODsAndSkinClusters();

		// スキニング用リソース
		InitializeSkinningResources(isSkinning);

		// LOD Controller 既定値
		SetupLODControllerDefaults();

		// 現在の LOD にスキンクラスタ頂点数を同期
		SyncSkinningVertexCountToCurrentLOD();
	}

	void AnimationModel::Initialize(const std::string& modelFileName, const std::string& animationFileName, bool isSkinning)
	{
		Clear();

		fileName_ = modelFileName;
		animationFileName_ = animationFileName;

		InitializeCommon();
		LoadBaseModelAndAnimation();
		CreateSkeletonFromModel();
		InitializeMaterialAndMeshes();
		InitializeEnvironmentMap();
		CreateConstantBuffers();
		BuildBodyPartColliders();
		BuildLODsAndSkinClusters();
		InitializeSkinningResources(isSkinning);
		SetupLODControllerDefaults();
		SyncSkinningVertexCountToCurrentLOD();
	}

	/// -------------------------------------------------------------
	///				　共通セットアップ
	/// -------------------------------------------------------------
	void AnimationModel::InitializeCommon()
	{
		dxCommon_ = DirectXCommon::GetInstance();
		camera_ = CameraManager::GetInstance()->GetMainCamera();
	}

	/// -------------------------------------------------------------
	///				　モデル＆アニメ読み込み
	/// -------------------------------------------------------------
	void AnimationModel::LoadBaseModelAndAnimation()
	{
		modelData = AssimpLoader::LoadModel(fileName_);
		const std::string& animationSource = animationFileName_.empty() ? fileName_ : animationFileName_;
		animationClips_ = AnimationLoader::LoadAllAnimations(animationSource);
		currentAnimationIndex_ = animationClips_.empty() ? -1 : 0;
		SyncCurrentAnimationForCompatibility();
	}

	/// -------------------------------------------------------------
	///				　	スケルトン生成
	/// -------------------------------------------------------------
	void AnimationModel::CreateSkeletonFromModel()
	{
		skeleton_ = Skeleton::CreateFromRootNode(modelData.rootNode);
	}

	/// -------------------------------------------------------------
	///				　マテリアル＆メッシュ初期化
	/// -------------------------------------------------------------
	void AnimationModel::InitializeMaterialAndMeshes()
	{
		material_.Initialize();
		materialTextureSlots_.Reset(); // 再初期化時は追加Slotを中立Textureへ戻す。

		// アニメーションメッシュ（非CSパス用）
		animationMesh_ = std::make_unique<AnimationMesh>();
		animationMesh_->Initialize(dxCommon_->GetDevice(), modelData);
	}

	void AnimationModel::ApplyMaterialDesc(const MaterialDesc& desc)
	{
		if (!dxCommon_)
		{
			return; // AnimationModel初期化前はTextureロードを含むMaterial反映を行わない。
		}

		material_.ApplyDesc(desc);
		materialTextureSlots_.ApplyDesc(desc); // BaseColorを含む5 SlotをAnimation Pipelineへまとめて反映する。
	}

	void AnimationModel::ResetMaterialBinding()
	{
		if (!dxCommon_)
		{
			return;
		}

		material_.ResetToDefault();
		materialTextureSlots_.Reset(); // BaseColorはモデル既定へ戻し、追加Slotは中立Textureへ戻す。
	}

	/// -------------------------------------------------------------
	///				　		環境マップ
	/// -------------------------------------------------------------
	void AnimationModel::InitializeEnvironmentMap()
	{
		TextureManager::GetInstance()->LoadTexture("SkyBox/skybox.dds");
		environmentMapHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("SkyBox/skybox.dds");
	}

	/// -------------------------------------------------------------
	///				　		定数バッファ
	/// -------------------------------------------------------------
	void AnimationModel::CreateConstantBuffers()
	{
		// 行列データ（b#1）
		wvpResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationAnimationMatrix));
		wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
		wvpData_->World = Matrix4x4::MakeIdentity();
		wvpData_->WVP = Matrix4x4::MakeIdentity();
		wvpData_->WorldInversedTranspose = Matrix4x4::MakeIdentity();

		// カメラデータ（b#3）
		cameraResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(CameraForGPU));
		cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&cameraData));
		cameraData->worldPosition = camera_ ? CameraManager::GetInstance()->GetActiveCameraPosition() : Vector3{ 0.0f, 0.0f, 0.0f };

		// Skinning PS でも LightingCommon の影パラメータを Object3D と同じ b4 に渡す。
		shadowParameterResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ShadowParameterForGPU));
		shadowParameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&shadowParameterData_));
		shadowParameterData_->lightViewProjection = Matrix4x4::MakeIdentity();
		shadowParameterData_->shadowBias = 0.015f;
		shadowParameterData_->normalBias = 0.02f;
		shadowParameterData_->shadowStrength = 0.6f;
		shadowParameterData_->shadowMode = 0u;
		shadowParameterData_->shadowDebugMode = 0u;
		shadowMapHandle_ = dxCommon_->GetShadowMapSrvHandleGPU();
	}

	/// -------------------------------------------------------------
	///				　	ボディパートコライダー
	/// -------------------------------------------------------------
	void AnimationModel::BuildBodyPartColliders()
	{
		InitializeBones(); // 互換維持：中で colliderController_ を構築
	}

	/// -------------------------------------------------------------
	///				　LOD構築（メッシュ＆スキンクラスタ）
	/// -------------------------------------------------------------
	void AnimationModel::BuildLODsAndSkinClusters()
	{
		AnimationModelLODBuilder::Build(dxCommon_, *skeleton_, fileName_, lodSourceFiles_, lods_, skinClusterLOD_, lodFileNames_);
	}

	/// -------------------------------------------------------------
	///				　スキニング用リソース
	/// -------------------------------------------------------------
	void AnimationModel::InitializeSkinningResources(bool isSkinning)
	{
		if (lods_.empty()) { return; }

		skinningCS_.Initialize(dxCommon_, lods_[0].vertexCount, isSkinning);
		skinningCS_.SetRuntimeEnabled(useComputeSkinning_);
	}

	/// -------------------------------------------------------------
	///				　LOD Controller 既定値
	/// -------------------------------------------------------------
	void AnimationModel::SetupLODControllerDefaults()
	{
		const int lodCount = (int)lods_.size();
		if (lodCount <= 0) { return; }

		// --- LOD Controller の初期設定（未設定なら既定値を入れる） ---
		if (lodController_.GetThresholds().empty() && lodCount > 1)
		{
			// 旧 SetLODByDistance のデフォルトに合わせる
			const float baseIn = 10.0f;
			const float step = 15.0f;

			lodController_.SetHysteresisGap(2.0f);

			std::vector<float> thresholds;
			thresholds.reserve(std::max(0, lodCount - 1));
			for (int i = 0; i < lodCount - 1; ++i)
			{
				thresholds.push_back(baseIn + step * (float)i);
			}
			lodController_.SetThresholds(thresholds);

			// 旧実装の lastOut = baseIn + step*(lodCount-1) + gap に相当
			const float lastOut = baseIn + step * (float)(lodCount - 1) + lodController_.GetHysteresisGap();
			lodController_.SetCullDistance(lastOut);
			lodController_.SetFarCullExtra(20.0f);
		}

		// heavy update 間隔配列のサイズが LOD数と違うなら整える（既定値/外部設定どちらも安全に）
		{
			const auto& cur = lodController_.GetHeavyUpdateEveryByLOD();
			if ((int)cur.size() != lodCount && lodCount > 0)
			{
				std::vector<uint32_t> hv;
				hv.reserve(lodCount);
				if (!cur.empty())
				{
					hv.assign(cur.begin(), cur.end());
				}
				if (hv.empty())
				{
					hv.assign(lodCount, 1u);
				}
				if ((int)hv.size() < lodCount) { hv.resize(lodCount, hv.back()); }
				if ((int)hv.size() > lodCount) { hv.resize(lodCount); }
				lodController_.SetHeavyUpdateEveryByLOD(hv);
			}
		}
	}

	/// -------------------------------------------------------------
	///				　			初期LOD確定
	/// -------------------------------------------------------------
	void AnimationModel::SyncSkinningVertexCountToCurrentLOD()
	{
		const int lodCount = (int)lods_.size();
		if (lodCount <= 0) { return; }

		const float distSq = CalcDistanceSqToCamera();
		lodController_.UpdateByDistanceSq(distSq, lodCount);

		const int li = lodController_.GetLODIndex();
		if (0 <= li && li < lodCount)
		{
			skinningCS_.SetVertexCount(lods_[li].vertexCount);
		}
	}

	/// -------------------------------------------------------------
	///				　　カメラ距離（平方距離）
	/// -------------------------------------------------------------
	float AnimationModel::CalcDistanceSqToCamera() const
	{
		const Vector3 camPos = camera_ ? CameraManager::GetInstance()->GetActiveCameraPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
		const Vector3 objPos = worldTransform.translate_;
		const float dx = camPos.x - objPos.x;
		const float dy = camPos.y - objPos.y;
		const float dz = camPos.z - objPos.z;
		return dx * dx + dy * dy + dz * dz;
	}

	const Animation* AnimationModel::GetCurrentAnimation() const
	{
		if (0 <= currentAnimationIndex_ && currentAnimationIndex_ < static_cast<int>(animationClips_.size()))
		{
			return &animationClips_[currentAnimationIndex_].animation;
		}
		return nullptr;
	}

	Animation* AnimationModel::GetCurrentAnimation()
	{
		if (0 <= currentAnimationIndex_ && currentAnimationIndex_ < static_cast<int>(animationClips_.size()))
		{
			return &animationClips_[currentAnimationIndex_].animation;
		}
		return nullptr;
	}

	void AnimationModel::SyncCurrentAnimationForCompatibility()
	{
		if (const Animation* currentAnimation = GetCurrentAnimation())
		{
			animation = *currentAnimation;
		}
		else
		{
			animation = {};
		}
	}

	const Animation* AnimationModel::GetAnimationByIndex(int index) const
	{
		if (0 <= index && index < static_cast<int>(animationClips_.size()))
		{
			return &animationClips_[index].animation;
		}
		return nullptr;
	}

	void AnimationModel::ClampAnimationTimeToCurrentDuration()
	{
		const Animation* currentAnimation = GetCurrentAnimation();
		if (!currentAnimation || currentAnimation->duration <= 0.0f)
		{
			animationPlayer_.SetTime(0.0f);
			return;
		}
		animationPlayer_.SetTime(std::clamp(animationPlayer_.GetTime(), 0.0f, currentAnimation->duration));
	}

	float AnimationModel::AdvanceAnimationTime(float timeSeconds, float deltaTime, float duration) const
	{
		if (duration <= 0.0f || deltaTime <= 0.0f) { return 0.0f; }

		float nextTime = timeSeconds + deltaTime * animationPlayer_.GetSpeed();
		if (animationPlayer_.IsLoop())
		{
			nextTime = std::fmod(nextTime, duration);
			if (nextTime < 0.0f) { nextTime += duration; }
		}
		else
		{
			nextTime = std::clamp(nextTime, 0.0f, duration);
		}
		return nextTime;
	}

	void AnimationModel::ResetCrossFadeState()
	{
		isCrossFading_ = false;
		previousAnimationIndex_ = -1;
		previousAnimationTime_ = 0.0f;
		crossFadeTime_ = 0.0f;
		crossFadeDuration_ = 0.0f;
	}

	bool AnimationModel::PlayAnimationByIndex(uint32_t index, bool resetTime)
	{
		if (index >= animationClips_.size()) { return false; }

		ResetCrossFadeState();
		currentAnimationIndex_ = static_cast<int>(index);
		SyncCurrentAnimationForCompatibility();
		if (resetTime)
		{
			animationPlayer_.SetTime(0.0f);
		}
		else
		{
			ClampAnimationTimeToCurrentDuration();
		}
		return true;
	}

	bool AnimationModel::PlayAnimationByName(const std::string& name, bool resetTime)
	{
		for (uint32_t index = 0; index < static_cast<uint32_t>(animationClips_.size()); ++index)
		{
			if (animationClips_[index].name == name)
			{
				return PlayAnimationByIndex(index, resetTime);
			}
		}
		return false;
	}

	bool AnimationModel::CrossFadeAnimationByIndex(uint32_t index, float fadeDuration)
	{
		if (index >= animationClips_.size()) { return false; }
		if (static_cast<int>(index) == currentAnimationIndex_ && !isCrossFading_) { return true; }
		if (fadeDuration <= 0.0f || currentAnimationIndex_ < 0 || GetCurrentAnimation() == nullptr)
		{
			return PlayAnimationByIndex(index, true);
		}

		previousAnimationIndex_ = currentAnimationIndex_;
		previousAnimationTime_ = animationPlayer_.GetTime();
		currentAnimationIndex_ = static_cast<int>(index);
		animationPlayer_.SetTime(0.0f);
		crossFadeTime_ = 0.0f;
		crossFadeDuration_ = fadeDuration;
		isCrossFading_ = true;
		SyncCurrentAnimationForCompatibility();
		return true;
	}

	bool AnimationModel::CrossFadeAnimationByName(const std::string& name, float fadeDuration)
	{
		for (uint32_t index = 0; index < static_cast<uint32_t>(animationClips_.size()); ++index)
		{
			if (animationClips_[index].name == name)
			{
				return CrossFadeAnimationByIndex(index, fadeDuration);
			}
		}
		return false;
	}

	std::string AnimationModel::GetCurrentAnimationName() const
	{
		if (0 <= currentAnimationIndex_ && currentAnimationIndex_ < static_cast<int>(animationClips_.size()))
		{
			return animationClips_[currentAnimationIndex_].name;
		}
		return {};
	}

	std::string AnimationModel::GetPreviousAnimationName() const
	{
		if (0 <= previousAnimationIndex_ && previousAnimationIndex_ < static_cast<int>(animationClips_.size()))
		{
			return animationClips_[previousAnimationIndex_].name;
		}
		return {};
	}

	float AnimationModel::GetAnimationDurationForDebugBatchTest() const
	{
		const Animation* currentAnimation = GetCurrentAnimation();
		return currentAnimation ? currentAnimation->duration : 0.0f;
	}

	bool AnimationModel::HasAnimationForDebugBatchTest() const
	{
		const Animation* currentAnimation = GetCurrentAnimation();
		return currentAnimation && currentAnimation->duration > 0.0f && !currentAnimation->nodeAnimations.empty();
	}

	/// -------------------------------------------------------------
	///				　　複数 LOD を直接渡すオーバーロード
	/// -------------------------------------------------------------
	void AnimationModel::Initialize(const std::string& fileName, const std::vector<std::string>& lodFiles, bool isSkinning)
	{
		// 先に LOD リストを設定してから既存 Initialize を呼ぶ
		SetLodFiles(lodFiles);
		Initialize(fileName, isSkinning);
	}

	/// -------------------------------------------------------------
	///				　			更新処理
	/// -------------------------------------------------------------
	void AnimationModel::Update()
	{
		// --- 距離に応じた LOD / カリング更新（毎フレ） ---
		const float distSq = CalcDistanceSqToCamera();

		const int prevLod = lodController_.GetLODIndex();
		const bool changed = lodController_.UpdateByDistanceSq(distSq, (int)lods_.size());

		const int lodIndex = lodController_.GetLODIndex();
		if (changed && lodIndex != prevLod && 0 <= lodIndex && lodIndex < (int)lods_.size())
		{
			// LOD が変わったら頂点数も更新（CSスキニング用）
			skinningCS_.SetVertexCount(lods_[lodIndex].vertexCount);
		}

		// 遠距離カリングされている場合はアニメーション更新をスキップ
		if (lodController_.IsCulled())
		{
			// 遠距離は何もしない
			material_.Update(); // ただしマテリアルの軽い更新は保持
			return;
		}

		const Animation* currentAnimation = GetCurrentAnimation();

		// アニメーション時間の更新
		if (animationPlayer_.IsPlaying() && currentAnimation && currentAnimation->duration > 0.0f)
		{
			const float dt = GameTimer::GetInstance()->GetDeltaTime();
			if (isCrossFading_)
			{
				const Animation* previousAnimation = GetAnimationByIndex(previousAnimationIndex_);
				animationPlayer_.SetTime(AdvanceAnimationTime(animationPlayer_.GetTime(), dt, currentAnimation->duration));
				if (previousAnimation)
				{
					previousAnimationTime_ = AdvanceAnimationTime(previousAnimationTime_, dt, previousAnimation->duration);
				}
				crossFadeTime_ += std::max(dt, 0.0f);
				if (crossFadeDuration_ <= 0.0f || crossFadeTime_ >= crossFadeDuration_)
				{
					isCrossFading_ = false;
					previousAnimationIndex_ = -1;
					previousAnimationTime_ = 0.0f;
					crossFadeTime_ = crossFadeDuration_;
				}
			}
			else
			{
				animationPlayer_.Update(dt, currentAnimation->duration);
			}
		}

		// LODごとの更新間引き（重い処理はスキップ可）
		const bool doHeavy = lodController_.ShouldDoHeavyUpdate();

		// スキニング処理（スケルトン更新は SkeletonAnimator に委譲）
		if (doHeavy && skinningCS_.IsSkinningModel() && skeleton_ && currentAnimation)
		{
			SkinCluster* skinCluster = nullptr;
			if (!skinClusterLOD_.empty() && 0 <= lodIndex && lodIndex < (int)skinClusterLOD_.size())
			{
				skinCluster = skinClusterLOD_[lodIndex].get();
			}

			const Animation* previousAnimation = isCrossFading_ ? GetAnimationByIndex(previousAnimationIndex_) : nullptr;
			if (previousAnimation)
			{
				const float blendRate = crossFadeDuration_ > 0.0f
					? std::clamp(crossFadeTime_ / crossFadeDuration_, 0.0f, 1.0f)
					: 1.0f;
				// LODごとにメッシュは変えるが、再生アニメーションは共有する。
				skeletonAnimator_.UpdateBlend(*skeleton_, *previousAnimation, previousAnimationTime_,
					*currentAnimation, animationPlayer_.GetTime(), blendRate, skinCluster);
			}
			else
			{
				// LODごとにメッシュは変えるが、再生アニメーションは共有する。
				skeletonAnimator_.Update(*skeleton_, *currentAnimation, animationPlayer_.GetTime(), skinCluster);
			}
		}

		// アニメーション行列の更新
		UpdateAnimation();
		UpdateShadowParameters();

		// マテリアルの更新処理
		material_.Update();
	}

	void AnimationModel::RefreshWorldTransform()
	{
		if (!dxCommon_ || !wvpData_)
		{
			return;
		}

		UpdateAnimation();
		UpdateShadowParameters();
		material_.Update();
	}

	/// -------------------------------------------------------------
	/// 			DebugScene専用の区間別更新処理
	/// -------------------------------------------------------------
	AnimationModel::DebugBatchUpdateTimings AnimationModel::UpdateForDebugBatchTest(bool playAnimation, bool forcePoseUpdate, float deltaTime)
	{
		DebugBatchUpdateTimings timings{};

		// 可視判定に必要なLODだけは、再生停止中も現在のカメラ距離へ追従させる。
		const float distSq = CalcDistanceSqToCamera();
		const int previousLod = lodController_.GetLODIndex();
		const bool lodChanged = lodController_.UpdateByDistanceSq(distSq, static_cast<int>(lods_.size()));
		const int lodIndex = lodController_.GetLODIndex();
		if (lodChanged && lodIndex != previousLod && 0 <= lodIndex && lodIndex < static_cast<int>(lods_.size()))
		{
			skinningCS_.SetVertexCount(lods_[lodIndex].vertexCount);
		}

		const Animation* currentAnimation = GetCurrentAnimation();

		animationPlayer_.SetPlaying(playAnimation);
		if (!lodController_.IsCulled() && playAnimation && currentAnimation && currentAnimation->duration > 0.0f)
		{
			const auto animationBegin = std::chrono::steady_clock::now();
			// Editor停止中もDebugSceneから渡された実フレーム時間で、AnimationPlayerを確実に進める。
			const float dt = std::max(deltaTime, 0.0f);
			if (isCrossFading_)
			{
				const Animation* previousAnimation = GetAnimationByIndex(previousAnimationIndex_);
				animationPlayer_.SetTime(AdvanceAnimationTime(animationPlayer_.GetTime(), dt, currentAnimation->duration));
				if (previousAnimation)
				{
					previousAnimationTime_ = AdvanceAnimationTime(previousAnimationTime_, dt, previousAnimation->duration);
				}
				crossFadeTime_ += dt;
				if (crossFadeDuration_ <= 0.0f || crossFadeTime_ >= crossFadeDuration_)
				{
					isCrossFading_ = false;
					previousAnimationIndex_ = -1;
					previousAnimationTime_ = 0.0f;
					crossFadeTime_ = crossFadeDuration_;
				}
			}
			else
			{
				animationPlayer_.Update(dt, currentAnimation->duration);
			}
			timings.animationTimeMilliseconds =
				std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - animationBegin).count();
		}
		timings.playAnimationTimeSeconds = animationPlayer_.GetTime();

		// Play Animation中は姿勢が毎フレーム変化するため、DebugScene専用テストでもposeDirtyを立てて骨更新とPalette更新を実行する。
		// LOD切替時も参照するSkinClusterが変わるので、新しいPaletteを一度だけ初期化する。
		const bool shouldUpdatePose = playAnimation || forcePoseUpdate || lodChanged;
		if (!lodController_.IsCulled() && shouldUpdatePose)
		{
			if (skinningCS_.IsSkinningModel() && skeleton_ && currentAnimation && !lods_.empty())
			{
				SkinCluster* skinCluster = nullptr;
				if (0 <= lodIndex && lodIndex < static_cast<int>(skinClusterLOD_.size()))
				{
					skinCluster = skinClusterLOD_[lodIndex].get();
				}
				SkeletonAnimator::UpdateTimings skeletonTimings{};
				const Animation* previousAnimation = isCrossFading_ ? GetAnimationByIndex(previousAnimationIndex_) : nullptr;
				if (previousAnimation)
				{
					const float blendRate = crossFadeDuration_ > 0.0f
						? std::clamp(crossFadeTime_ / crossFadeDuration_, 0.0f, 1.0f)
						: 1.0f;
					skeletonAnimator_.UpdateBlend(*skeleton_, *previousAnimation, previousAnimationTime_,
						*currentAnimation, animationPlayer_.GetTime(), blendRate, skinCluster, &skeletonTimings);
				}
				else
				{
					skeletonAnimator_.Update(*skeleton_, *currentAnimation, animationPlayer_.GetTime(), skinCluster, &skeletonTimings);
				}
				timings.skeletonMilliseconds = skeletonTimings.skeletonMilliseconds;
				timings.paletteMilliseconds = skeletonTimings.paletteMilliseconds;
				timings.poseUpdated = true;
			}
		}

		// 姿勢が変わらないフレームではSkinClusterのPalette更新を省略し、DebugSceneのAnimationModel大量描画テストで更新コストを分離する。
		const auto worldBegin = std::chrono::steady_clock::now();
		if (!lodController_.IsCulled())
		{
			UpdateAnimation();
			UpdateShadowParameters();
		}
		timings.worldTransformMilliseconds =
			std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - worldBegin).count();
		return timings;
	}

	/// -------------------------------------------------------------
	/// 		DebugScene専用アニメーション再読込
	/// -------------------------------------------------------------
	bool AnimationModel::ReloadAnimationForDebugBatchTest()
	{
		// 通常のAnimationModelロード規約は変えず、DebugSceneテストだけSourcesの論理パスを明示する。
		AnimationLoader::Settings settings{};
		settings.animationFilePath = "Resources/Models/Sources/";
		const std::string previousName = GetCurrentAnimationName();
		animationClips_ = AnimationLoader::LoadAllAnimations(fileName_, settings);
		currentAnimationIndex_ = animationClips_.empty() ? -1 : 0;
		if (!previousName.empty())
		{
			PlayAnimationByName(previousName, false);
		}
		SyncCurrentAnimationForCompatibility();
		animationPlayer_.Reset();
		ResetCrossFadeState();
		return HasAnimationForDebugBatchTest();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE AnimationModel::GetCurrentPaletteSrvForDebugBatchTest() const
	{
		const int lodIndex = lodController_.GetLODIndex();
		if (lodIndex < 0 || lodIndex >= static_cast<int>(skinClusterLOD_.size()) || !skinClusterLOD_[lodIndex])
		{
			return {};
		}
		return skinClusterLOD_[lodIndex]->GetPaletteSrvOnUAVHeap();
	}

	bool AnimationModel::CanSharePaletteWithForDebugBatchTest(const AnimationModel& representative) const
	{
		// DebugScene内でもモデルファイルまたはLODが違う個体へ、互換性のないPaletteを渡さない。
		return fileName_ == representative.fileName_
			&& lodController_.GetLODIndex() == representative.lodController_.GetLODIndex()
			&& GetCurrentPaletteSrvForDebugBatchTest().ptr != 0
			&& representative.GetCurrentPaletteSrvForDebugBatchTest().ptr != 0;
	}

	/// -------------------------------------------------------------
	///				　			描画処理
	/// -------------------------------------------------------------
	void AnimationModel::Draw()
	{
		// Compute 一括セット（スタンドアロン用）
		UAVManager::GetInstance()->PreDispatch();
		AnimationPipelineBuilder::GetInstance()->SetComputeSetting();

		// コンピュートでスキニング（個体差だけを束ねて実行）
		if (useComputeSkinning_ && skinningCS_.IsSkinningModel()) DispatchSkinningCS();

		// Graphics 一括セット（スタンドアロン用）
		SRVManager::GetInstance()->PreDraw();
		AnimationPipelineBuilder::GetInstance()->SetRenderSetting();

		// スキン済みメッシュを描画（個体差だけを束ねて実行）
		DrawSkinned();
	}

	/// -------------------------------------------------------------
	///				　　単一のモデルをまとめて描画
	/// -------------------------------------------------------------
	void AnimationModel::DrawBatched(const std::unique_ptr<AnimationModel>& models)
	{
		if (models) {
			if (models->IsVisible()) models->Draw();
		}
	}

	/// -------------------------------------------------------------
	///				　可視チェックも含めてまとめて描画
	/// -------------------------------------------------------------
	void AnimationModel::DrawBatched(const std::vector<std::unique_ptr<AnimationModel>>& models)
	{
		// Compute パス（全体で一回）
		UAVManager::GetInstance()->PreDispatch();
		AnimationPipelineBuilder::GetInstance()->SetComputeSetting();
		for (auto& m : models)
		{
			if (!m) continue;
			if (m->IsVisible()) m->DispatchSkinningCS();
		}

		// Graphics パス（全体で一回）
		SRVManager::GetInstance()->PreDraw();
		AnimationPipelineBuilder::GetInstance()->SetRenderSetting();
		for (auto& m : models)
		{
			if (!m) continue;
			if (m->IsVisible()) m->DrawSkinned();
		}
	}

	/// -------------------------------------------------------------
	/// 		DebugScene専用のCompute Skinning一括実行
	/// -------------------------------------------------------------
	size_t AnimationModel::DispatchSkinningBatchedForDebugTest(const std::vector<std::unique_ptr<AnimationModel>>& models)
	{
		UAVManager::GetInstance()->PreDispatch();
		AnimationPipelineBuilder::GetInstance()->SetComputeSetting();

		size_t dispatchCount = 0;
		for (const auto& model : models)
		{
			if (!model || !model->IsVisible() || !model->IsComputeSkinningEnabled()) { continue; }
			model->DispatchSkinningCS();
			++dispatchCount;
		}
		return dispatchCount;
	}

	/// -------------------------------------------------------------
	/// 	DebugScene専用の代表Palette共有Compute Skinning
	/// -------------------------------------------------------------
	AnimationModel::DebugSharedPaletteDispatchStats AnimationModel::DispatchSkinningBatchedWithSharedPaletteForDebugTest(
		const std::vector<std::unique_ptr<AnimationModel>>& models, const AnimationModel* representative)
	{
		DebugSharedPaletteDispatchStats stats{};
		if (!representative) { return stats; }
		const D3D12_GPU_DESCRIPTOR_HANDLE sharedPaletteSrv = representative->GetCurrentPaletteSrvForDebugBatchTest();
		stats.sharedPaletteValid = sharedPaletteSrv.ptr != 0;

		UAVManager::GetInstance()->PreDispatch();
		AnimationPipelineBuilder::GetInstance()->SetComputeSetting();
		for (const auto& model : models)
		{
			if (!model || !model->IsVisible() || !model->IsComputeSkinningEnabled()) { continue; }
			if (stats.sharedPaletteValid && model->CanSharePaletteWithForDebugBatchTest(*representative))
			{
				if (model->DispatchSkinningCSWithExternalPaletteForDebugBatchTest(sharedPaletteSrv))
				{
					++stats.sharedPaletteDispatchCount;
				}
				else
				{
					model->DispatchSkinningCS();
					++stats.fallbackDispatchCount;
				}
			}
			else
			{
				model->DispatchSkinningCS();
				++stats.fallbackDispatchCount;
			}
		}
		return stats;
	}

	/// -------------------------------------------------------------
	///				　ポインタ配列版も欲しければオーバーロード
	/// -------------------------------------------------------------
	void AnimationModel::DrawBatched(const std::vector<AnimationModel*>& models)
	{
		// Compute パス（全体で一回）
		UAVManager::GetInstance()->PreDispatch();
		AnimationPipelineBuilder::GetInstance()->SetComputeSetting();

		// Compute パス（全体で一回）
		for (auto* m : models)
		{
			if (!m) continue;
			if (m->IsVisible()) m->DispatchSkinningCS();
		}

		// Graphics パス（全体で一回）
		SRVManager::GetInstance()->PreDraw();
		AnimationPipelineBuilder::GetInstance()->SetRenderSetting();

		// Graphics パス（全体で一回）
		for (auto* m : models)
		{
			if (!m) continue;
			if (m->IsVisible()) m->DrawSkinned();
		}
	}

	/// -------------------------------------------------------------
	///				　		 ImGui描画処理
	/// -------------------------------------------------------------
	void AnimationModel::DrawImGui()
	{
		AnimationModelDebugView::DrawImGui(*this);
	}

	/// -------------------------------------------------------------
	///				　			削除処理
	/// -------------------------------------------------------------
	void AnimationModel::Clear()
	{
		animationMesh_.reset();
		skeleton_.reset();
		for (auto& sc : skinClusterLOD_) { sc.reset(); }
		skinClusterLOD_.clear();

		wvpResource.Reset();
		cameraResource.Reset();
		shadowParameterResource_.Reset();
		wvpData_ = nullptr;
		cameraData = nullptr;
		shadowParameterData_ = nullptr;
		shadowMapHandle_ = {};
		materialTextureSlots_.Clear(); // Clear後に破棄済みTexture Handleを再利用しない。

		// --- LOD 側で確保した UAV ヒープの SRV/UAV インデックスを解放 ---
		for (auto& L : lods_)
		{
			if (L.srvInputVerticesOnUavHeap != UINT32_MAX) {
				UAVManager::GetInstance()->Free(L.srvInputVerticesOnUavHeap);
				L.srvInputVerticesOnUavHeap = UINT32_MAX;
			}
			if (L.uavIndex != UINT32_MAX) {
				UAVManager::GetInstance()->Free(L.uavIndex);
				L.uavIndex = UINT32_MAX;
			}
		}

		skinningCS_.Reset();

		lods_.clear();
		lodFileNames_.clear(); // 出力のみクリア（入力 lodSourceFiles_ は保持）

		modelData = {};
		animation = {};
		animationClips_.clear();
		currentAnimationIndex_ = -1;
		ResetCrossFadeState();
		animationPlayer_.Reset();

		colliderController_.Clear();

		lodController_.ResetRuntimeState();
	}

	/// -------------------------------------------------------------
	///				　		ワイヤーフレーム描画
	/// -------------------------------------------------------------
	void AnimationModel::DrawSkeletonWireframe()
	{
		AnimationModelDebugView::DrawSkeletonWireframe(*this);
	}

	/// -------------------------------------------------------------
	///				　		ボディパートコライダー描画
	/// -------------------------------------------------------------
	void AnimationModel::DrawBodyPartColliders()
	{
		AnimationModelDebugView::DrawBodyPartColliders(*this);
	}

	/// -------------------------------------------------------------
	///				　		ボディパートコライダー情報取得
	/// -------------------------------------------------------------
	std::vector<std::pair<std::string, Capsule>> AnimationModel::GetBodyPartCapsulesWorld() const
	{
		if (!skeleton_) { return {}; }
		return colliderController_.GetCapsulesWorld(*skeleton_, worldTransform);
	}

	/// -------------------------------------------------------------
	///				　		ボディパートコライダー情報取得
	/// -------------------------------------------------------------
	std::vector<std::pair<std::string, Sphere>> AnimationModel::GetBodyPartSpheresWorld() const
	{
		if (!skeleton_) { return {}; }
		return colliderController_.GetSpheresWorld(*skeleton_, worldTransform);
	}

	/// -------------------------------------------------------------
	///				　	アニメーションの更新処理
	/// -------------------------------------------------------------
	void AnimationModel::UpdateAnimation()
	{
		// スキニングモデルか通常モデルかで分岐
		if (skeleton_ && !skeleton_->GetJoints().empty() && skinningCS_.IsSkinningModel())
		{
			// スキニングあり（スキニングモデル用のWVP更新）
			Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);
			Matrix4x4 worldViewProjectionMatrix;

			// カメラがあればビュー射影行列を掛ける
			if (camera_)
			{
				// カメラ行列取得
				const Matrix4x4 viewProjectionMatrix = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();

				// ワールドビュー射影行列計算
				worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix);
			}
			else
			{
				// カメラ無し → ワールド行列のみ
				worldViewProjectionMatrix = worldMatrix;
			}

			// スキニングあり（スキニングモデル用のWVP更新）
			wvpData_->World = worldMatrix;
			wvpData_->WVP = worldViewProjectionMatrix;
			wvpData_->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrix));
		}
		else
		{
			const Animation* currentAnimation = GetCurrentAnimation();
			Vector3 translate{ 0.0f, 0.0f, 0.0f };
			Quaternion rotate{};
			Vector3 scale{ 1.0f, 1.0f, 1.0f };
			if (currentAnimation)
			{
				const auto rootIt = currentAnimation->nodeAnimations.find(modelData.rootNode.name);
				if (rootIt != currentAnimation->nodeAnimations.end())
				{
					const NodeAnimation& rootNodeAnimation = rootIt->second; // ルートノードのアニメーション取得
					if (!rootNodeAnimation.translate.empty())
					{
						translate = AnimationSampler::CalculateValue(rootNodeAnimation.translate, animationPlayer_.GetTime());
					}
					if (!rootNodeAnimation.rotate.empty())
					{
						rotate = AnimationSampler::CalculateValue(rootNodeAnimation.rotate, animationPlayer_.GetTime());
					}
					if (!rootNodeAnimation.scale.empty())
					{
						scale = AnimationSampler::CalculateValue(rootNodeAnimation.scale, animationPlayer_.GetTime());
					}
				}
			}

			// ローカル行列計算
			Matrix4x4 localMatrix = Matrix4x4::MakeAffineMatrix(scale, rotate, translate);

			// ワールド行列計算
			Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);
			Matrix4x4 worldViewProjectionMatrix;

			// カメラ行列取得
			if (camera_)
			{
				// カメラ行列取得
				const Matrix4x4& viewProjectionMatrix = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();

				// ワールドビュー射影行列計算
				worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjectionMatrix);
			}
			else
			{
				// カメラ無し → ワールド行列のみ
				worldViewProjectionMatrix = worldMatrix;
			}

			// 行列更新
			wvpData_->WVP = localMatrix * worldViewProjectionMatrix;
			wvpData_->World = localMatrix * worldMatrix;
			wvpData_->WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(localMatrix * worldMatrix));
		}
	}

	/// -------------------------------------------------------------
	///				　		シャドウパラメータ更新
	/// -------------------------------------------------------------
	void AnimationModel::UpdateShadowParameters()
	{
		if (!shadowParameterData_) { return; }

		const auto* lightMgr = LightManager::GetInstance();
		const Vector3 focusPos = cameraData ? cameraData->worldPosition : CameraManager::GetInstance()->GetActiveCameraPosition();
		shadowParameterData_->lightViewProjection = lightMgr->BuildShadowLightViewProjection(focusPos);
		shadowParameterData_->shadowBias = lightMgr->GetShadowBias();
		shadowParameterData_->normalBias = lightMgr->GetNormalBias();
		shadowParameterData_->shadowStrength = lightMgr->GetShadowStrength();
		shadowParameterData_->shadowMode = lightMgr->GetShadowReceiverMode();
		shadowParameterData_->shadowDebugMode = lightMgr->IsShadowMapDebugEnabled() ? 1u : (lightMgr->IsShadowFactorDebugEnabled() ? 2u : 0u);
	}

	/// -------------------------------------------------------------
	///				　		ボーン初期化処理
	/// -------------------------------------------------------------
	void AnimationModel::InitializeBones()
	{
		if (!skeleton_) { return; }
		colliderController_.BuildMixamoHumanoid(*skeleton_, scaleFactor);
	}

	/// -------------------------------------------------------------
	///				　		LOD距離設定処理
	/// -------------------------------------------------------------
	void AnimationModel::SetLODByDistance(float dist)
	{
		if (lods_.empty()) { return; }

		const int prevLod = lodController_.GetLODIndex();
		const bool changed = lodController_.Update(dist, (int)lods_.size());

		const int lodIndex = lodController_.GetLODIndex();
		if (changed && lodIndex != prevLod && 0 <= lodIndex && lodIndex < (int)lods_.size())
		{
			skinningCS_.SetVertexCount(lods_[lodIndex].vertexCount);
		}
	}

	/// -------------------------------------------------------------
	///				　		スキニング計算処理
	/// -------------------------------------------------------------
	void AnimationModel::DispatchSkinningCS()
	{
		if (lodController_.IsCulled()) { return; }
		if (!useComputeSkinning_) { return; }
		if (!dxCommon_) { return; }
		if (!skinningCS_.IsSkinningModel()) { return; }
		if (lods_.empty() || skinClusterLOD_.empty()) { return; }

		const int lodIndex = lodController_.GetLODIndex();
		if (lodIndex < 0 || lodIndex >= (int)lods_.size()) { return; }
		if (lodIndex < 0 || lodIndex >= (int)skinClusterLOD_.size()) { return; }
		if (!skinClusterLOD_[lodIndex]) { return; }

		auto& L = lods_[lodIndex];

		// UAV/SRVハンドル組み立て（ここはLODEntryに依存するのでModel側でOK）
		const D3D12_GPU_DESCRIPTOR_HANDLE inputSrv = UAVManager::GetInstance()->GetGPUDescriptorHandle(L.srvInputVerticesOnUavHeap);
		const D3D12_GPU_DESCRIPTOR_HANDLE influenceSrv = L.influenceSrvGpuOnUavHeap;
		const D3D12_GPU_DESCRIPTOR_HANDLE outputUav = UAVManager::GetInstance()->GetGPUDescriptorHandle(L.uavIndex);

		skinningCS_.Dispatch(dxCommon_, skinClusterLOD_[lodIndex].get(), inputSrv, influenceSrv, outputUav, L.vertexCount, L.skinnedVB.Get(), L.skinnedState);
	}

	bool AnimationModel::DispatchSkinningCSWithExternalPaletteForDebugBatchTest(D3D12_GPU_DESCRIPTOR_HANDLE sharedPaletteSrv)
	{
		if (sharedPaletteSrv.ptr == 0 || lodController_.IsCulled() || !useComputeSkinning_ || !dxCommon_ || !skinningCS_.IsSkinningModel())
		{
			return false;
		}
		const int lodIndex = lodController_.GetLODIndex();
		if (lodIndex < 0 || lodIndex >= static_cast<int>(lods_.size())
			|| lodIndex >= static_cast<int>(skinClusterLOD_.size()) || !skinClusterLOD_[lodIndex])
		{
			return false;
		}

		auto& lod = lods_[lodIndex];
		const D3D12_GPU_DESCRIPTOR_HANDLE inputSrv = UAVManager::GetInstance()->GetGPUDescriptorHandle(lod.srvInputVerticesOnUavHeap);
		const D3D12_GPU_DESCRIPTOR_HANDLE outputUav = UAVManager::GetInstance()->GetGPUDescriptorHandle(lod.uavIndex);
		// 同じモデル・同じアニメーション時間のDebug用モデル群では、代表モデルのPaletteを共有して各モデルを同じ姿勢でスキニングする。
		skinningCS_.Dispatch(dxCommon_, skinClusterLOD_[lodIndex].get(), inputSrv, lod.influenceSrvGpuOnUavHeap,
			outputUav, lod.vertexCount, lod.skinnedVB.Get(), lod.skinnedState, sharedPaletteSrv);
		return true;
	}

	/// -------------------------------------------------------------
	///				　		スキニング描画処理
	/// -------------------------------------------------------------
	void AnimationModel::DrawSkinned()
	{
		if (lodController_.IsCulled()) { return; } // 描画もしない
		if (!dxCommon_) { return; }
		if (lods_.empty()) { return; }

		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();

		const int lodIndex = lodController_.GetLODIndex();
		if (lodIndex < 0 || lodIndex >= (int)lods_.size()) { return; }

		auto& L = lods_[lodIndex];

		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 4, environmentMapHandle_); // t1: 環境マップ
		commandList->SetGraphicsRootConstantBufferView(7, shadowParameterResource_->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(8, shadowMapHandle_);
		materialTextureSlots_.BindAdditionalSlots(commandList, 10, 11, 12, 13); // t6:MR t7:Normal t8:AO t9:Emissive

		// VB/IB
		if (skinningCS_.IsSkinningModel())
		{
			commandList->IASetVertexBuffers(0, 1, &L.skinnedVBV);  // ← 1本だけ
			commandList->IASetIndexBuffer(&L.ibv);

			// マテリアル＆描画
			material_.SetPipeline();

			// ルート定数バッファ
			commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());     // WVP (b#1)
			commandList->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());  // カメラ (b#3)

			for (const auto& range : L.subMeshRanges)
			{
				const D3D12_GPU_DESCRIPTOR_HANDLE baseColorHandle =
					materialTextureSlots_.ResolveBaseColor(range.baseColorSrvGpuHandle);
				TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 2, baseColorHandle); // Overrideは全LODのSubMeshへ共通適用する。
				commandList->DrawIndexedInstanced(range.indexCount, 1, range.startIndex, 0, 0);
			}
		}
		else
		{
			// 非CS: AnimationMesh が複数VB/IB対応済み前提でループ
			material_.SetPipeline();
			commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(3, cameraResource->GetGPUVirtualAddress());

			const size_t subCount = animationMesh_ ? animationMesh_->GetSubmeshCount() : 0;
			for (size_t i = 0; i < subCount; ++i)
			{
				const auto& vbv = animationMesh_->GetVertexBufferView(i);
				const auto& ibv = animationMesh_->GetIndexBufferView(i);

				commandList->IASetVertexBuffers(0, 1, &vbv);
				commandList->IASetIndexBuffer(&ibv);

				// マテリアルSRV（InitializeLODs と同様、subMeshes[i] のテクスチャを使用）
				const auto& sm = modelData.subMeshes[i];
				const D3D12_GPU_DESCRIPTOR_HANDLE baseColorHandle = materialTextureSlots_.ResolveBaseColor(
					AnimationModelLODBuilder::LoadSrvOrFallback(sm.material.textureFilePath));
				TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 2, baseColorHandle); // 非CS Pathも同じBinding結果を使う。
				commandList->DrawIndexedInstanced(UINT(sm.indices.size()), 1, 0, 0, 0);
			}
		}
	}

} // namespace Ken4lowEngine
