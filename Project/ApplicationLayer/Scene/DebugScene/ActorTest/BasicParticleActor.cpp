#include "BasicParticleActor.h"
#include "SceneComponent.h"
#include "GpuParticleComponent.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void BasicParticleActor::Initialize()
{
	// Actor全体の基準となるRootComponentを生成する
	auto& root = CreateRootComponent < Ken4lowEngine::SceneComponent>();
	root.SetName("VFX Root"); // RootComponentの型名をデフォルト名として設定する
	root.SetLocalPosition({ 0.0f, 2.0f, 0.0f });

	// GPUパーティクルを生成する
	auto& core = AddComponent<Ken4lowEngine::GpuParticleComponent>();
	core.SetName("Orb Core"); // GpuParticleComponentの型名をデフォルト名として設定する
	core.AttachTo(&root);			// RootComponentを親として設定する
	core.ApplyPreset("Default");	// DefaultプリセットをComponent全体へ適用する
	core.SetPlayOnStart(true);		// ゲーム開始時に自動再生する
	core.SetLoop(false);			// ループ再生しない
	core.SetFollowOwner(true);		// Actorの移動に追従する

	auto& glow = AddComponent<Ken4lowEngine::GpuParticleComponent>();
	glow.SetName("Orb Glow"); // GpuParticleComponentの型名をデフォルト名として設定する
	glow.AttachTo(&root);			// RootComponentを親として設定する
	glow.ApplyPreset("Default");	// DefaultプリセットをComponent全体へ適用する
	glow.SetPlayOnStart(true);		// ゲーム開始時に自動再生する
	glow.SetLoop(false);			// ループ再生しない
	glow.SetFollowOwner(true);		// Actorの移動に追従する

	auto& spark = AddComponent<Ken4lowEngine::GpuParticleComponent>();
	spark.SetName("Orb Spark");		// GpuParticleComponentの型名をデフォルト名として設定する
	spark.AttachTo(&root);			// RootComponentを親として設定する
	spark.ApplyPreset("HitSpark");	// HitSparkプリセットをComponent全体へ適用する
	spark.SetPlayOnStart(true);		// ゲーム開始時に自動再生する
	spark.SetLoop(false);			// ループ再生しない
	spark.SetFollowOwner(true);		// Actorの移動に追従する

	// ActorのComponent初期化を実行する
	Actor::Initialize();
}

void BasicParticleActor::DrawImGui()
{
#ifdef USE_IMGUI
	// 編集UIだけを条件付きにし、ReleaseでもActorとParticleの実行処理を維持する。
	Actor::DrawImGui();

	ImGui::SeparatorText("VFX");

	// Editor上でActorのパラメータを調整するUIを描画する
	if (ImGui::Button("Play VFX"))
	{
		PlayVFX();
	}
#endif
}

void BasicParticleActor::PlayVFX()
{
	// ActorについているGPUパーティクルを全て取得する
	const auto particles = GetComponents<Ken4lowEngine::GpuParticleComponent>();

	// 同じ呼び出し内ですべてのパーティクルを再生する
	for (auto* particle : particles)
	{
		if (particle)
		{
			particle->Play();
		}
	}
}
