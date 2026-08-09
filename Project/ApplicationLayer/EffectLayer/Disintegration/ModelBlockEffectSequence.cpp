#define NOMINMAX
#include "ModelBlockEffectSequence.h"

#include "ModelDisintegrationEffect.h"
#include "ModelReconstructionEffect.h"

#include <algorithm>

void ModelBlockEffectSequence::Initialize(ModelReconstructionEffect* reconstructionEffect, ModelDisintegrationEffect* disintegrationEffect)
{
	reconstructionEffect_ = reconstructionEffect;
	disintegrationEffect_ = disintegrationEffect;
	Reset();
}

void ModelBlockEffectSequence::PlaySpawnThenDisintegrate(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix)
{
	Begin(modelPath, worldMatrix, SequenceMode::SpawnThenDisintegrate, parameters_.loopEnabled);
	StartReconstruction();
}

void ModelBlockEffectSequence::PlayDisintegrateThenReconstruct(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix)
{
	Begin(modelPath, worldMatrix, SequenceMode::DisintegrateThenReconstruct, parameters_.loopEnabled);
	modelVisible_ = true;
	StartDisintegration();
}

void ModelBlockEffectSequence::PlayLoop(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix)
{
	Begin(modelPath, worldMatrix, SequenceMode::SpawnThenDisintegrate, true);
	StartReconstruction();
}

void ModelBlockEffectSequence::Stop(bool showModelAfterStop)
{
	ResetEffects();
	modelVisible_ = showModelAfterStop;
	state_ = SequenceState::Idle;
	waitingReason_ = WaitingReason::None;
	sequenceElapsed_ = 0.0f;
	stateElapsed_ = 0.0f;
	sharedCompletedSamples_.clear();
}

void ModelBlockEffectSequence::Reset()
{
	ResetEffects();
	modelVisible_ = true;
	state_ = SequenceState::Idle;
	mode_ = SequenceMode::SpawnThenDisintegrate;
	waitingReason_ = WaitingReason::None;
	sequenceElapsed_ = 0.0f;
	stateElapsed_ = 0.0f;
	reconstructionCompleted_ = false;
	disintegrationCompleted_ = false;
	sharedCompletedSamples_.clear();
}

void ModelBlockEffectSequence::Update(float deltaTime)
{
	if (state_ == SequenceState::Idle || state_ == SequenceState::Completed)
	{
		return;
	}

	sequenceElapsed_ += deltaTime;
	stateElapsed_ += deltaTime;

	if (reconstructionEffect_)
	{
		reconstructionEffect_->Update(deltaTime);
	}
	if (disintegrationEffect_)
	{
		disintegrationEffect_->Update(deltaTime);
	}

	switch (state_)
	{
	case SequenceState::Reconstructing:
		if (reconstructionEffect_ && reconstructionEffect_->IsComplete())
		{
			reconstructionCompleted_ = true;
			sharedCompletedSamples_ = reconstructionEffect_->GetTargetSamples();
			SetReconstructionAlpha(1.0f);
			modelVisible_ = false;
			if (mode_ == SequenceMode::DisintegrateThenReconstruct && !parameters_.loopEnabled)
			{
				if (!parameters_.keepBlocksAfterComplete) { ResetEffects(); }
				EnterState(SequenceState::Completed);
			}
			else
			{
				EnterState(SequenceState::HoldingBlocks);
			}
		}
		break;
	case SequenceState::HoldingBlocks:
		modelVisible_ = false;
		SetReconstructionAlpha(1.0f);
		if (stateElapsed_ >= std::max(parameters_.blockHoldDuration, 0.0f))
		{
			if (parameters_.skipNormalModelInSequence)
			{
				StartDisintegrationFromSharedSamples();
			}
			else if (parameters_.useModelBlend && parameters_.useBlockToModelFade && GetBlendDuration() > 0.0f)
			{
				modelVisible_ = true;
				EnterState(SequenceState::BlendingBlockToModel);
			}
			else
			{
				modelVisible_ = true;
				if (!parameters_.keepBlocksUntilDisintegration) { ResetEffects(); }
				EnterState(SequenceState::ShowingModel);
			}
		}
		break;
	case SequenceState::BlendingBlockToModel:
	{
		const float t = Clamp01(stateElapsed_ / GetBlendDuration());
		modelVisible_ = true;
		SetReconstructionAlpha(1.0f - t);
		if (t >= 1.0f)
		{
			if (!parameters_.keepBlocksUntilDisintegration) { ResetEffects(); }
			EnterState(SequenceState::ShowingModel);
		}
		break;
	}
	case SequenceState::ShowingModel:
		modelVisible_ = !parameters_.skipNormalModelInSequence;
		if (stateElapsed_ >= std::max(parameters_.showDuration, 0.0f))
		{
			if (parameters_.useModelBlend && parameters_.useModelToBlockFade && !sharedCompletedSamples_.empty() && GetBlendDuration() > 0.0f)
			{
				ResetEffects();
				ApplySharedParameters();
				if (disintegrationEffect_)
				{
					disintegrationEffect_->GetParameters().useSurfaceInset = false;
					disintegrationEffect_->PrepareFromSamples(sharedCompletedSamples_, worldMatrix_);
				}
				SetDisintegrationAlpha(0.0f);
				modelVisible_ = true;
				EnterState(SequenceState::BlendingModelToBlock);
			}
			else if (parameters_.disintegrateAsCompleteBlocks && !sharedCompletedSamples_.empty())
			{
				StartDisintegrationFromSharedSamples();
			}
			else
			{
				StartDisintegration();
			}
		}
		break;
	case SequenceState::BlendingModelToBlock:
	{
		const float t = Clamp01(stateElapsed_ / GetBlendDuration());
		modelVisible_ = true;
		SetDisintegrationAlpha(t);
		if (t >= 1.0f)
		{
			modelVisible_ = false;
			SetDisintegrationAlpha(1.0f);
			if (disintegrationEffect_)
			{
				disintegrationEffect_->StartPrepared();
			}
			EnterState(SequenceState::Disintegrating);
		}
		break;
	}
	case SequenceState::Disintegrating:
		if (disintegrationEffect_ && !disintegrationEffect_->IsActive())
		{
			disintegrationCompleted_ = true;
			if (mode_ == SequenceMode::DisintegrateThenReconstruct)
			{
				sharedCompletedSamples_ = disintegrationEffect_->GetInitialSamples();
			}
			modelVisible_ = false;
			if (mode_ == SequenceMode::DisintegrateThenReconstruct)
			{
				waitingReason_ = WaitingReason::BeforeReconstruct;
				EnterState(SequenceState::Waiting);
			}
			else
			{
				CompleteOrLoop();
			}
		}
		break;
	case SequenceState::Waiting:
		if (stateElapsed_ >= std::max(parameters_.waitDuration, 0.0f))
		{
			if (waitingReason_ == WaitingReason::BeforeReconstruct)
			{
				StartReconstruction();
			}
			else if (waitingReason_ == WaitingReason::BeforeLoopRestart)
			{
				StartReconstruction();
			}
		}
		break;
	default:
		break;
	}
}

const char* ModelBlockEffectSequence::GetStateName() const
{
	switch (state_)
	{
	case SequenceState::Idle: return "待機中";
	case SequenceState::Reconstructing: return "再構築中";
	case SequenceState::HoldingBlocks: return "ブロック完全体保持中";
	case SequenceState::BlendingBlockToModel: return "ブロックから通常モデルへブレンド中";
	case SequenceState::ShowingModel: return "通常モデル表示中";
	case SequenceState::BlendingModelToBlock: return "通常モデルからブロックへブレンド中";
	case SequenceState::Disintegrating: return "崩壊中";
	case SequenceState::Waiting: return "待機中";
	case SequenceState::Completed: return "完了";
	default: return "不明";
	}
}

void ModelBlockEffectSequence::Begin(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix, SequenceMode mode, bool loopEnabled)
{
	modelPath_ = modelPath;
	worldMatrix_ = worldMatrix;
	mode_ = mode;
	parameters_.loopEnabled = loopEnabled;
	sequenceElapsed_ = 0.0f;
	stateElapsed_ = 0.0f;
	reconstructionCompleted_ = false;
	disintegrationCompleted_ = false;
	waitingReason_ = WaitingReason::None;
	parameters_.fullBodyHoldTime = parameters_.blockHoldDuration;
	parameters_.modelSwitchBlendTime = parameters_.modelBlendDuration;
	parameters_.keepBlocksAfterComplete = parameters_.keepBlocksUntilDisintegration;
	parameters_.fadeToModelAfterComplete = parameters_.useBlockToModelFade;
	sharedCompletedSamples_.clear();
	ResetEffects();
	ApplySharedParameters();
	modelVisible_ = false;
}

void ModelBlockEffectSequence::ResetEffects()
{
	if (reconstructionEffect_)
	{
		reconstructionEffect_->Initialize();
	}
	if (disintegrationEffect_)
	{
		disintegrationEffect_->Initialize();
	}
}

void ModelBlockEffectSequence::ApplySharedParameters()
{
	if (reconstructionEffect_)
	{
		auto& reconstructionParams = reconstructionEffect_->GetParameters();
		reconstructionParams.blockCount = parameters_.blockCount;
		reconstructionParams.blockSize = parameters_.blockSize;
		reconstructionParams.surfaceSampling = parameters_.surfaceSampling;
		reconstructionParams.useSurfaceInset = parameters_.useSurfaceInset;
		reconstructionParams.surfaceInset = parameters_.surfaceInset;
		reconstructionParams.autoSurfaceInsetFromBlockSize = parameters_.autoSurfaceInsetFromBlockSize;
		reconstructionParams.placementMode = parameters_.placementMode;
		reconstructionParams.useRandomScale = parameters_.useRandomScale;
		reconstructionParams.scaleVariation = parameters_.scaleVariation;
		reconstructionParams.useRandomRotation = parameters_.useRandomRotation;
		reconstructionParams.rotationRandomness = parameters_.rotationRandomness;
		reconstructionParams.placementSeed = parameters_.placementSeed;
		reconstructionParams.placementSpacing = parameters_.placementSpacing;
		reconstructionParams.voxelSpacing = parameters_.voxelSpacing;
		reconstructionParams.maxVoxelBlockCount = parameters_.maxVoxelBlockCount;
		reconstructionParams.voxelSurfaceThickness = parameters_.voxelSurfaceThickness;
		reconstructionParams.useVoxelInsideTest = parameters_.useVoxelInsideTest;
		reconstructionParams.useVoxelSurfaceNearTest = parameters_.useVoxelSurfaceNearTest;
		reconstructionParams.alignVoxelGridToCenter = parameters_.alignVoxelGridToCenter;
		reconstructionParams.useFadeIn = parameters_.useFadeIn;
		reconstructionParams.fadeInDuration = parameters_.fadeInDuration;
		reconstructionParams.fadeInDelayRange = parameters_.fadeInDelayRange;
		reconstructionParams.initialAlpha = parameters_.initialAlpha;
		reconstructionParams.targetAlpha = parameters_.targetAlpha;
		reconstructionParams.fadeInEasePower = parameters_.fadeInEasePower;
		reconstructionParams.fadeInByDistance = parameters_.fadeInByDistance;
		reconstructionParams.fadeInNearTarget = parameters_.fadeInNearTarget;
		reconstructionParams.holdTime = 0.0f;
		reconstructionParams.showFinalModel = false;
		reconstructionParams.autoHideBlocksAfterComplete = false;
	}

	if (disintegrationEffect_)
	{
		auto& disintegrationParams = disintegrationEffect_->GetParameters();
		disintegrationParams.particleCount = parameters_.blockCount;
		disintegrationParams.particleSize = parameters_.blockSize;
		disintegrationParams.blockSize = parameters_.blockSize;
		disintegrationParams.surfaceSampling = parameters_.surfaceSampling;
		disintegrationParams.useSurfaceInset = parameters_.useSurfaceInset;
		disintegrationParams.surfaceInset = parameters_.surfaceInset;
		disintegrationParams.autoSurfaceInsetFromBlockSize = parameters_.autoSurfaceInsetFromBlockSize;
		disintegrationParams.placementMode = parameters_.placementMode;
		disintegrationParams.useRandomScale = parameters_.useRandomScale;
		disintegrationParams.scaleVariation = parameters_.scaleVariation;
		disintegrationParams.useRandomRotation = parameters_.useRandomRotation;
		disintegrationParams.rotationRandomness = parameters_.rotationRandomness;
		disintegrationParams.placementSeed = parameters_.placementSeed;
		disintegrationParams.placementSpacing = parameters_.placementSpacing;
		disintegrationParams.voxelSpacing = parameters_.voxelSpacing;
		disintegrationParams.maxVoxelBlockCount = parameters_.maxVoxelBlockCount;
		disintegrationParams.voxelSurfaceThickness = parameters_.voxelSurfaceThickness;
		disintegrationParams.useVoxelInsideTest = parameters_.useVoxelInsideTest;
		disintegrationParams.useVoxelSurfaceNearTest = parameters_.useVoxelSurfaceNearTest;
		disintegrationParams.alignVoxelGridToCenter = parameters_.alignVoxelGridToCenter;
		disintegrationParams.useSweepErosion = parameters_.useSweepErosion;
		disintegrationParams.erosionMode = parameters_.erosionMode;
		disintegrationParams.erosionCenter = parameters_.erosionCenter;
		disintegrationParams.useModelCenterAsErosionCenter = parameters_.useModelCenterAsErosionCenter;
		disintegrationParams.centerErosionDuration = parameters_.centerErosionDuration;
		disintegrationParams.centerErosionNoisePower = parameters_.centerErosionNoisePower;
		disintegrationParams.centerGlowWidth = parameters_.centerGlowWidth;
		disintegrationParams.sweepDirection = parameters_.sweepDirection;
		disintegrationParams.sweepDuration = parameters_.sweepDuration;
		disintegrationParams.erosionNoisePower = parameters_.erosionNoisePower;
		disintegrationParams.erosionBandWidth = parameters_.erosionBandWidth;
		disintegrationParams.preGlowWidth = parameters_.preGlowWidth;
		disintegrationParams.postGlowWidth = parameters_.postGlowWidth;
		disintegrationParams.glowEdgeWidth = parameters_.glowEdgeWidth;
		disintegrationParams.glowIntensity = parameters_.glowIntensity;
		disintegrationParams.glowSharpness = parameters_.glowSharpness;
		disintegrationParams.glowColor = parameters_.glowColor;
	}
}

void ModelBlockEffectSequence::StartReconstruction()
{
	ResetEffects();
	ApplySharedParameters();
	modelVisible_ = false;
	SetReconstructionAlpha(1.0f);
	reconstructionCompleted_ = false;
	waitingReason_ = WaitingReason::None;
	EnterState(SequenceState::Reconstructing);
	if (reconstructionEffect_)
	{
		// 再構築中は通常モデルを隠し、ブロックだけで登場演出を確認する。
		if (!sharedCompletedSamples_.empty())
		{
			reconstructionEffect_->GetParameters().useSurfaceInset = false;
			reconstructionEffect_->PlayFromSamples(sharedCompletedSamples_, worldMatrix_);
		}
		else
		{
			reconstructionEffect_->PlayFromModel(modelPath_, worldMatrix_);
		}
	}
}

void ModelBlockEffectSequence::StartDisintegration()
{
	ResetEffects();
	ApplySharedParameters();
	modelVisible_ = false;
	SetDisintegrationAlpha(1.0f);
	disintegrationCompleted_ = false;
	waitingReason_ = WaitingReason::None;
	EnterState(SequenceState::Disintegrating);
	if (disintegrationEffect_)
	{
		disintegrationEffect_->PlayFromModel(modelPath_, worldMatrix_);
	}
}

void ModelBlockEffectSequence::StartDisintegrationFromSharedSamples()
{
	const auto samples = sharedCompletedSamples_;
	ResetEffects();
	ApplySharedParameters();
	modelVisible_ = false;
	SetDisintegrationAlpha(1.0f);
	disintegrationCompleted_ = false;
	waitingReason_ = WaitingReason::None;
	EnterState(SequenceState::Disintegrating);
	if (disintegrationEffect_)
	{
		// 再構築で到着した表面サンプルを崩壊にも使い、完全体から崩れる瞬間の配置ジャンプを防ぐ。
		disintegrationEffect_->GetParameters().useSurfaceInset = false;
		disintegrationEffect_->PlayFromSamples(samples, worldMatrix_);
	}
}

void ModelBlockEffectSequence::EnterState(SequenceState nextState)
{
	state_ = nextState;
	stateElapsed_ = 0.0f;
}

void ModelBlockEffectSequence::CompleteOrLoop()
{
	if (parameters_.loopEnabled)
	{
		waitingReason_ = WaitingReason::BeforeLoopRestart;
		EnterState(SequenceState::Waiting);
		return;
	}

	ResetEffects();
	modelVisible_ = false;
	waitingReason_ = WaitingReason::None;
	sharedCompletedSamples_.clear();
	EnterState(SequenceState::Completed);
}


float ModelBlockEffectSequence::GetBlendDuration() const
{
	return std::max(parameters_.modelBlendDuration, 0.0f);
}

float ModelBlockEffectSequence::Clamp01(float value) const
{
	return std::clamp(value, 0.0f, 1.0f);
}

void ModelBlockEffectSequence::SetReconstructionAlpha(float alpha)
{
	if (reconstructionEffect_)
	{
		reconstructionEffect_->SetGlobalAlpha(Clamp01(alpha));
	}
}

void ModelBlockEffectSequence::SetDisintegrationAlpha(float alpha)
{
	if (disintegrationEffect_)
	{
		disintegrationEffect_->SetGlobalAlpha(Clamp01(alpha));
	}
}
