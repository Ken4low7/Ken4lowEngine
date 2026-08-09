#pragma once
#include "ReconstructionEmitter.h"
#include "ReconstructionRenderer.h"
#include "Matrix4x4.h"
#include "Vector4.h"
#include "ModelSurfaceSampler.h"

#include <cstdint>
#include <string>
#include <vector>

/// -------------------------------------------------------------
/// モデル登場時に散らばったブロックを表面サンプル位置へ集める再構築エフェクト
/// -------------------------------------------------------------
class ModelReconstructionEffect
{
public:
	struct Parameters
	{
		int blockCount = 1200;
		float blockSize = 0.045f;
		float duration = 1.8f;
		float startScatterRadius = 3.0f;
		float startHeight = 2.5f;
		float startDelayRange = 0.45f;
		float rotationRandomness = 4.0f;
		DisintegrationPlacementMode placementMode = DisintegrationPlacementMode::RandomSurface;
		bool useRandomScale = true;
		float scaleVariation = 0.12f;
		bool useRandomRotation = true;
		uint32_t placementSeed = 0xD157E6A7u;
		float placementSpacing = 0.0f;
		float voxelSpacing = 0.0f;
		int maxVoxelBlockCount = 10000;
		float voxelSurfaceThickness = 0.0675f;
		bool useVoxelInsideTest = true;
		bool useVoxelSurfaceNearTest = true;
		bool alignVoxelGridToCenter = true;
		float easePower = 3.0f;
		K4E::Vector4 color{ 0.64f, 0.72f, 0.86f, 1.0f };
		float colorVariation = 0.18f;
		bool useFadeIn = true;
		float fadeInDuration = 0.4f;
		float fadeInDelayRange = 0.2f;
		float initialAlpha = 0.0f;
		float targetAlpha = 1.0f;
		float fadeInEasePower = 2.0f;
		bool fadeInByDistance = false;
		bool fadeInNearTarget = true;
		float holdTime = 0.60f;
		bool showFinalModel = true;
		bool autoHideBlocksAfterComplete = true;
		bool surfaceSampling = true;
		bool useSurfaceInset = false;
		float surfaceInset = 0.0225f;
		bool autoSurfaceInsetFromBlockSize = true;
	};

	void Initialize();
	void PlayFromModel(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix);
	void PlayFromSamples(const std::vector<DisintegrationSamplePoint>& samples, const K4E::Matrix4x4& worldMatrix);
	void Update(float deltaTime);
	void Draw();
	void DrawImGui();
	void SetGlobalAlpha(float alpha);
	bool IsActive() const { return isActive_; }
	bool IsComplete() const { return isComplete_; }
	bool ShouldShowFinalModel() const { return isComplete_ && parameters_.showFinalModel; }
	bool ShouldDrawBlocks() const { return isActive_ || (!parameters_.autoHideBlocksAfterComplete && !blocks_.empty()); }
	std::vector<DisintegrationSamplePoint> GetTargetSamples() const;

	Parameters& GetParameters() { return parameters_; }
	const Parameters& GetParameters() const { return parameters_; }

private:
	float Clamp01(float value) const;
	float EaseOut(float value) const;
	void ApplyFadeInSettings(ReconstructionEmitter::Settings& settings) const;
	void UpdateBlockFade(ReconstructionBlock& block) const;
	ReconstructionEmitter::Settings BuildEmitterSettings() const;
	void ResetPlaybackState();

	Parameters parameters_{};
	ReconstructionEmitter emitter_{};
	ReconstructionRenderer renderer_{};
	std::vector<ReconstructionBlock> blocks_{};
	bool isActive_ = false;
	bool isComplete_ = false;
	float elapsedTime_ = 0.0f;
	float globalAlpha_ = 1.0f;
};
