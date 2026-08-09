#pragma once
#include "ModelSurfaceSampler.h"
#include "ModelDisintegrationEffect.h"
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"

#include <cstdint>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

class ModelDisintegrationEffect;
class ModelReconstructionEffect;

/// -------------------------------------------------------------
/// DebugSceneで崩壊/再構築ブロック演出を連続確認するための小さな管理クラス
/// -------------------------------------------------------------
class ModelBlockEffectSequence
{
public:
	enum class SequenceState
	{
		Idle,
		Reconstructing,
		HoldingBlocks,
		BlendingBlockToModel,
		ShowingModel,
		BlendingModelToBlock,
		Disintegrating,
		Waiting,
		Completed,
	};

	struct Parameters
	{
		float showDuration = 1.2f;
		float waitDuration = 0.8f;
		bool useModelBlend = true;
		float modelBlendDuration = 0.25f;
		float blockHoldDuration = 0.8f;
		bool keepBlocksUntilDisintegration = true;
		bool skipNormalModelInSequence = true;
		bool useBlockToModelFade = true;
		bool useModelToBlockFade = true;
		bool showFinalModelAfterComplete = false;
		bool keepBlocksAfterComplete = true;
		bool fadeToModelAfterComplete = false;
		bool disintegrateAsCompleteBlocks = true;
		float fullBodyHoldTime = 0.8f;
		float modelSwitchBlendTime = 0.25f;
		bool loopEnabled = false;
		int blockCount = 1200;
		float blockSize = 0.045f;
		float placementSpacing = 0.0f;
		float voxelSpacing = 0.0f;
		int maxVoxelBlockCount = 10000;
		float voxelSurfaceThickness = 0.0675f;
		bool useVoxelInsideTest = true;
		bool useVoxelSurfaceNearTest = true;
		bool alignVoxelGridToCenter = true;
		bool surfaceSampling = true;
		bool useSurfaceInset = true;
		float surfaceInset = 0.0225f;
		bool autoSurfaceInsetFromBlockSize = true;
		DisintegrationPlacementMode placementMode = DisintegrationPlacementMode::UniformSurface;
		bool useRandomScale = false;
		float scaleVariation = 0.18f;
		bool useRandomRotation = false;
		float rotationRandomness = 1.2f;
		uint32_t placementSeed = 0xD157E6A7u;
		bool useFadeIn = true;
		float fadeInDuration = 0.4f;
		float fadeInDelayRange = 0.2f;
		float initialAlpha = 0.0f;
		float targetAlpha = 1.0f;
		float fadeInEasePower = 2.0f;
		bool fadeInByDistance = false;
		bool fadeInNearTarget = true;
		bool useSweepErosion = true;
		ErosionMode erosionMode = ErosionMode::DirectionalSweep;
		K4E::Vector3 erosionCenter{ 0.0f, 0.0f, 0.0f };
		bool useModelCenterAsErosionCenter = true;
		float centerErosionDuration = 1.6f;
		float centerErosionNoisePower = 0.35f;
		float centerGlowWidth = 0.24f;
		K4E::Vector3 sweepDirection{ 1.0f, 0.0f, 0.0f };
		float sweepDuration = 1.6f;
		float erosionNoisePower = 0.35f;
		float erosionBandWidth = 0.10f;
		float preGlowWidth = 0.20f;
		float postGlowWidth = 0.16f;
		float glowEdgeWidth = 0.24f;
		float glowIntensity = 1.8f;
		float glowSharpness = 1.6f;
		K4E::Vector4 glowColor{ 1.0f, 0.55f, 0.18f, 1.0f };
	};

	void Initialize(ModelReconstructionEffect* reconstructionEffect, ModelDisintegrationEffect* disintegrationEffect);
	void PlaySpawnThenDisintegrate(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix);
	void PlayDisintegrateThenReconstruct(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix);
	void PlayLoop(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix);
	void Stop(bool showModelAfterStop = false);
	void Reset();
	void Update(float deltaTime);

	Parameters& GetParameters() { return parameters_; }
	const Parameters& GetParameters() const { return parameters_; }

	SequenceState GetState() const { return state_; }
	const char* GetStateName() const;
	float GetSequenceElapsed() const { return sequenceElapsed_; }
	bool IsModelVisible() const { return modelVisible_; }
	bool IsRunning() const { return state_ != SequenceState::Idle && state_ != SequenceState::Completed; }
	bool IsReconstructionCompleted() const { return reconstructionCompleted_; }
	bool IsDisintegrationCompleted() const { return disintegrationCompleted_; }

private:
	enum class SequenceMode
	{
		SpawnThenDisintegrate,
		DisintegrateThenReconstruct,
	};

	enum class WaitingReason
	{
		None,
		BeforeReconstruct,
		BeforeLoopRestart,
	};

	void Begin(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix, SequenceMode mode, bool loopEnabled);
	void ResetEffects();
	void ApplySharedParameters();
	void StartReconstruction();
	void StartDisintegration();
	void StartDisintegrationFromSharedSamples();
	void EnterState(SequenceState nextState);
	void CompleteOrLoop();
	float GetBlendDuration() const;
	float Clamp01(float value) const;
	void SetReconstructionAlpha(float alpha);
	void SetDisintegrationAlpha(float alpha);

	ModelReconstructionEffect* reconstructionEffect_ = nullptr;
	ModelDisintegrationEffect* disintegrationEffect_ = nullptr;
	Parameters parameters_{};
	SequenceState state_ = SequenceState::Idle;
	SequenceMode mode_ = SequenceMode::SpawnThenDisintegrate;
	WaitingReason waitingReason_ = WaitingReason::None;
	std::string modelPath_;
	K4E::Matrix4x4 worldMatrix_{};
	float sequenceElapsed_ = 0.0f;
	float stateElapsed_ = 0.0f;
	bool modelVisible_ = true;
	bool reconstructionCompleted_ = false;
	bool disintegrationCompleted_ = false;
	std::vector<DisintegrationSamplePoint> sharedCompletedSamples_;
};
