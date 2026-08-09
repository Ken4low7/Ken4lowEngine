#define NOMINMAX
#include "ModelReconstructionEffect.h"

#include "Model.h"
#include "ModelManager.h"
#include "BlockPlacementImGui.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void ModelReconstructionEffect::Initialize()
{
	blocks_.clear();
	isActive_ = false;
	isComplete_ = false;
	elapsedTime_ = 0.0f;
	globalAlpha_ = 1.0f;
}

void ModelReconstructionEffect::PlayFromModel(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix)
{
	auto model = K4E::ModelManager::GetInstance()->LoadModel(modelPath);
	if (!model) { return; }

	blocks_ = emitter_.EmitFromModel(model->GetModelData(), worldMatrix, BuildEmitterSettings());
	ResetPlaybackState();
}


void ModelReconstructionEffect::PlayFromSamples(const std::vector<DisintegrationSamplePoint>& samples, const K4E::Matrix4x4& worldMatrix)
{
	blocks_ = emitter_.EmitFromSamples(samples, worldMatrix.GetTranslation(), BuildEmitterSettings());
	ResetPlaybackState();
}

ReconstructionEmitter::Settings ModelReconstructionEffect::BuildEmitterSettings() const
{
	ReconstructionEmitter::Settings settings{};
	settings.blockCount = parameters_.blockCount;
	settings.blockSize = parameters_.blockSize;
	settings.startScatterRadius = parameters_.startScatterRadius;
	settings.startHeight = parameters_.startHeight;
	settings.startDelayRange = parameters_.startDelayRange;
	settings.rotationRandomness = parameters_.rotationRandomness;
	settings.placementMode = parameters_.placementMode;
	settings.useRandomScale = parameters_.useRandomScale;
	settings.scaleVariation = parameters_.scaleVariation;
	settings.useRandomRotation = parameters_.useRandomRotation;
	settings.placementSeed = parameters_.placementSeed;
	settings.placementSpacing = parameters_.placementSpacing;
	settings.voxelSpacing = parameters_.voxelSpacing;
	settings.maxVoxelBlockCount = parameters_.maxVoxelBlockCount;
	settings.voxelSurfaceThickness = parameters_.voxelSurfaceThickness;
	settings.useVoxelInsideTest = parameters_.useVoxelInsideTest;
	settings.useVoxelSurfaceNearTest = parameters_.useVoxelSurfaceNearTest;
	settings.alignVoxelGridToCenter = parameters_.alignVoxelGridToCenter;
	settings.surfaceSampling = parameters_.surfaceSampling;
	settings.useSurfaceInset = parameters_.useSurfaceInset;
	settings.surfaceInset = parameters_.surfaceInset;
	settings.autoSurfaceInsetFromBlockSize = parameters_.autoSurfaceInsetFromBlockSize;
	settings.color = parameters_.color;
	settings.colorVariation = parameters_.colorVariation;
	ApplyFadeInSettings(settings);
	return settings;
}

void ModelReconstructionEffect::ResetPlaybackState()
{
	// モデルと共有サンプルのどちらから生成しても、再生状態を同じ条件で初期化する。
	isActive_ = !blocks_.empty();
	isComplete_ = false;
	elapsedTime_ = 0.0f;
	globalAlpha_ = 1.0f;
}

void ModelReconstructionEffect::Update(float deltaTime)
{
	if (!isActive_ && parameters_.autoHideBlocksAfterComplete) { return; }
	if (blocks_.empty()) { return; }

	elapsedTime_ += deltaTime;
	bool allArrived = true;

	for (auto& block : blocks_)
	{
		if (!block.alive) { continue; }

		block.age += deltaTime;
		block.appearAge += deltaTime;
		if (block.age < block.startDelay)
		{
			block.position = block.startPosition;
			UpdateBlockFade(block);
			allArrived = false;
			continue;
		}

		const float localT = Clamp01((block.age - block.startDelay) / std::max(parameters_.duration, 0.0001f));
		const float easedT = EaseOut(localT);

		// 到着直前で回転速度を弱め、最終的には表面サンプル位置に静止させる。
		block.position = block.startPosition + (block.targetPosition - block.startPosition) * easedT;
		block.rotation = block.startRotation + block.rotationVelocity * ((1.0f - easedT) * block.age);
		block.arrived = localT >= 1.0f;
		UpdateBlockFade(block);
		if (block.arrived)
		{
			block.position = block.targetPosition;
			block.rotation = { 0.0f, 0.0f, 0.0f };
			block.alpha = parameters_.useFadeIn ? Clamp01(parameters_.targetAlpha) : block.alpha;
			block.visibility = block.alpha;
		}
		else
		{
			allArrived = false;
		}
	}

	const float totalTime = parameters_.startDelayRange + parameters_.duration + parameters_.holdTime;
	if (allArrived && elapsedTime_ >= totalTime)
	{
		isComplete_ = true;
	}

	if (isComplete_ && parameters_.autoHideBlocksAfterComplete)
	{
		isActive_ = false;
		for (auto& block : blocks_)
		{
			block.alive = false;
		}
	}
}

void ModelReconstructionEffect::Draw()
{
	if (!ShouldDrawBlocks()) { return; }
	renderer_.Draw(blocks_, globalAlpha_);
}

void ModelReconstructionEffect::SetGlobalAlpha(float alpha)
{
	globalAlpha_ = Clamp01(alpha);
}

std::vector<DisintegrationSamplePoint> ModelReconstructionEffect::GetTargetSamples() const
{
	std::vector<DisintegrationSamplePoint> samples;
	samples.reserve(blocks_.size());
	for (const auto& block : blocks_)
	{
		DisintegrationSamplePoint sample{};
		sample.position = block.targetPosition;
		sample.normal = block.targetNormal;
		samples.push_back(sample);
	}
	return samples;
}

void ModelReconstructionEffect::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("モデル再構築エフェクト詳細");
	ImGui::Text("再生中: %s", isActive_ ? "はい" : "いいえ");
	ImGui::Text("完了: %s", isComplete_ ? "はい" : "いいえ");
	ImGui::Text("ブロック数: %zu", blocks_.size());
	ImGui::SliderInt("ブロック数", &parameters_.blockCount, 32, 8000);
	ImGui::SliderFloat("ブロックサイズ", &parameters_.blockSize, 0.005f, 0.30f);
	ImGui::SliderFloat("再構築時間", &parameters_.duration, 0.05f, 8.0f);
	ImGui::SliderFloat("開始時の散らばり半径", &parameters_.startScatterRadius, 0.0f, 12.0f);
	ImGui::SliderFloat("開始高さ", &parameters_.startHeight, -2.0f, 10.0f);
	ImGui::SliderFloat("開始遅延幅", &parameters_.startDelayRange, 0.0f, 3.0f);
	ImGui::Checkbox("ランダム回転を使う", &parameters_.useRandomRotation);
	ImGui::SliderFloat("回転ばらつき", &parameters_.rotationRandomness, 0.0f, 12.0f);
	ImGui::Checkbox("ランダムサイズを使う", &parameters_.useRandomScale);
	ImGui::SliderFloat("サイズばらつき", &parameters_.scaleVariation, 0.0f, 0.75f);
	BlockPlacementImGui::DrawPlacementMode(parameters_);
	BlockPlacementImGui::DrawSeedAndVoxelSettings(parameters_);
	ImGui::SliderFloat("イージング強度", &parameters_.easePower, 1.0f, 8.0f);
	ImGui::ColorEdit4("色", &parameters_.color.x);
	ImGui::SliderFloat("色のばらつき", &parameters_.colorVariation, 0.0f, 0.8f);
	ImGui::SeparatorText("フェードイン");
	ImGui::Checkbox("フェードインを使う", &parameters_.useFadeIn);
	ImGui::SliderFloat("初期透明度", &parameters_.initialAlpha, 0.0f, 1.0f);
	ImGui::SliderFloat("目標透明度", &parameters_.targetAlpha, 0.0f, 1.0f);
	ImGui::SliderFloat("フェードイン時間", &parameters_.fadeInDuration, 0.01f, 3.0f);
	ImGui::SliderFloat("フェードイン遅延幅", &parameters_.fadeInDelayRange, 0.0f, 2.0f);
	ImGui::SliderFloat("フェードインの鋭さ", &parameters_.fadeInEasePower, 0.1f, 8.0f);
	ImGui::Checkbox("距離でフェードイン", &parameters_.fadeInByDistance);
	ImGui::Checkbox("到着付近で不透明化", &parameters_.fadeInNearTarget);
	ImGui::SliderFloat("完了後の保持時間", &parameters_.holdTime, 0.0f, 5.0f);
	ImGui::Checkbox("完了後にモデル表示", &parameters_.showFinalModel);
	ImGui::Checkbox("完了後にブロック自動非表示", &parameters_.autoHideBlocksAfterComplete);
	BlockPlacementImGui::DrawSurfaceSettings(parameters_);
	ImGui::End();
#endif
}

float ModelReconstructionEffect::Clamp01(float value) const
{
	return std::clamp(value, 0.0f, 1.0f);
}

float ModelReconstructionEffect::EaseOut(float value) const
{
	const float t = Clamp01(value);
	return 1.0f - std::pow(1.0f - t, std::max(parameters_.easePower, 1.0f));
}

void ModelReconstructionEffect::ApplyFadeInSettings(ReconstructionEmitter::Settings& settings) const
{
	settings.useFadeIn = parameters_.useFadeIn;
	settings.fadeInDuration = std::max(parameters_.fadeInDuration, 0.0001f);
	settings.fadeInDelayRange = std::max(parameters_.fadeInDelayRange, 0.0f);
	settings.initialAlpha = Clamp01(parameters_.initialAlpha);
	settings.targetAlpha = Clamp01(parameters_.targetAlpha);
}

void ModelReconstructionEffect::UpdateBlockFade(ReconstructionBlock& block) const
{
	if (!parameters_.useFadeIn)
	{
		block.alpha = Clamp01(parameters_.targetAlpha);
		block.visibility = block.alpha;
		return;
	}

	const float fadeDuration = std::max(block.fadeInDuration, 0.0001f);
	const float fadeT = Clamp01((block.appearAge - block.fadeInDelay) / fadeDuration);
	const float easedT = 1.0f - std::pow(1.0f - fadeT, std::max(parameters_.fadeInEasePower, 0.0001f));
	const float initialAlpha = Clamp01(parameters_.initialAlpha);
	const float targetAlpha = Clamp01(parameters_.targetAlpha);
	float alpha = initialAlpha + (targetAlpha - initialAlpha) * easedT;

	if (parameters_.fadeInByDistance || parameters_.fadeInNearTarget)
	{
		const float startDistance = std::max(block.startDistance, 0.0001f);
		const float distanceToTarget = K4E::Vector3::Length(block.targetPosition - block.position);
		const float distanceRate = 1.0f - Clamp01(distanceToTarget / startDistance);
		const float distanceAlpha = initialAlpha + (targetAlpha - initialAlpha) * distanceRate;

		// 到着に近づくほど透明度を押し上げ、再構築中のブロックを徐々に物質化させる。
		alpha = parameters_.fadeInNearTarget ? std::max(alpha, distanceAlpha) : distanceAlpha;
	}

	block.alpha = Clamp01(alpha);
	block.visibility = block.alpha;
}
