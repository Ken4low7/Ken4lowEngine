#include "BasicParticleActor.h"
#include "SceneComponent.h"
#include "GpuParticleComponent.h"

void BasicParticleActor::Initialize()
{
	// Actor全体の基準となるRootComponentを生成する
	auto& root = CreateRootComponent < Ken4lowEngine::SceneComponent>();
	root.SetName("VFX Root"); // RootComponentの型名をデフォルト名として設定する
	root.SetLocalPosition({ 0.0f, 2.0f, 0.0f });

	// GPUパーティクルを生成する
	auto& particle = AddComponent<Ken4lowEngine::GpuParticleComponent>();
	particle.SetName("Basic Particle"); // GpuParticleComponentの型名をデフォルト名として設定する
	particle.AttachTo(&root);			// RootComponentを親として設定する
	particle.SetEffectName("Smoke");	// 使用するGPUパーティクルエフェクト名を設定する
	particle.SetPlayOnStart(true);		// ゲーム開始時に自動再生する
	particle.SetLoop(false);			// ループ再生しない
	particle.SetFollowOwner(true);		// Actorの移動に追従する

	// ActorのComponent初期化を実行する
	Actor::Initialize();
}
