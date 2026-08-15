#include "VfxTrackAdapters.h"

#include "Engine/Graphics/Camera/Core/Camera.h"
#include "Engine/Graphics/Camera/DebugCamera/DebugCamera.h"
#include "Engine/Graphics/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/PostEffect/Manager/PostEffectManager.h"
#include "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/GpuVolumetricFluidManager.h"
#include "Engine/Scene/Actor/Components/FluidEmitterComponent.h"
#include "Engine/Scene/Actor/Components/LightComponent.h"
#include "Engine/Scene/Actor/Components/SceneComponent.h"
#include "Engine/Scene/Actor/Core/Actor.h"
#include "Engine/Scene/Actor/Core/ActorWorld.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{
namespace
{
	Vector3 Add(const Vector3& a, const Vector3& b)
	{
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	Vector3 Scale(const Vector3& value, float scale)
	{
		return { value.x * scale, value.y * scale, value.z * scale };
	}

	std::string RuntimeActorName(const char* prefix, uint64_t id)
	{
		return std::string("__VFX_") + prefix + "_" + std::to_string(id);
	}
}

bool VfxParticleTrackAdapter::Start(const VfxTrackStartContext& context, VfxTrackRuntimeToken& outToken)
{
	const auto* payload = std::get_if<VfxParticleTrackPayload>(&context.payload);
	if (payload == nullptr || payload->effectName.empty())
	{
		return false;
	}

	GpuParticleEffectRuntime* runtime = GpuParticleEffectRuntime::GetInstance();
	if (!runtime->IsRegistered(payload->effectName))
	{
		if (payload->effectAssetPath.empty() || !runtime->LoadEffect(payload->effectAssetPath))
		{
			return false;
		}
	}

	const Vector3 position = Add(context.worldPosition, context.localOffset);
	outToken.value = context.runtimeTrackId;

	if (payload->loop)
	{
		Entry entry{};
		entry.looping = true;
		entry.effectName = payload->effectName;
		entry.particleHandle = runtime->PlayLoop(payload->effectName, position);
		if (!entry.particleHandle.IsValid())
		{
			return false;
		}

		for (const auto& [name, value] : context.parameters.particleFloatOverrides)
		{
			if (!runtime->SetFloatParameter(entry.particleHandle, name, value))
			{
				runtime->StopLoop(entry.particleHandle);
				return false;
			}
		}
		entries_[outToken.value] = std::move(entry);
		return true;
	}

	// Phase13 one-shotはInstance parameter handleをまだ持たないため、既存Play経路をそのまま使う。
	return runtime->Play(payload->effectName, position);
}

bool VfxParticleTrackAdapter::Update(VfxTrackRuntimeToken token, const VfxTrackStartContext& context, float)
{
	const auto it = entries_.find(token.value);
	if (it == entries_.end())
	{
		return true; // One-shotは開始時だけで完結する。
	}

	GpuParticleEffectRuntime* runtime = GpuParticleEffectRuntime::GetInstance();
	const Vector3 position = Add(context.worldPosition, context.localOffset);
	if (!runtime->SetLoopPosition(it->second.particleHandle, position))
	{
		return false;
	}
	for (const auto& [name, value] : context.parameters.particleFloatOverrides)
	{
		if (!runtime->SetFloatParameter(it->second.particleHandle, name, value))
		{
			return false;
		}
	}
	return true;
}

void VfxParticleTrackAdapter::Stop(VfxTrackRuntimeToken token)
{
	const auto it = entries_.find(token.value);
	if (it == entries_.end()) return;
	if (it->second.looping && it->second.particleHandle.IsValid())
	{
		GpuParticleEffectRuntime::GetInstance()->StopLoop(it->second.particleHandle);
	}
	entries_.erase(it);
}

void VfxParticleTrackAdapter::StopAll()
{
	for (const auto& [id, entry] : entries_)
	{
		(void)id;
		if (entry.looping && entry.particleHandle.IsValid())
		{
			GpuParticleEffectRuntime::GetInstance()->StopLoop(entry.particleHandle);
		}
	}
	entries_.clear();
}

bool VfxFluidTrackAdapter::Start(ActorWorld* world, const VfxTrackStartContext& context, VfxTrackRuntimeToken& outToken)
{
	const auto* payload = std::get_if<VfxFluidTrackPayload>(&context.payload);
	if (world == nullptr || payload == nullptr)
	{
		return false;
	}

	if (context.type == VfxCueTrackType::VolumetricFluid)
	{
		// VFX Cueから3D Sourceを再生した時だけPhase17 Runtimeを遅延有効化し、既存2D Sceneの起動負荷は変えない。
		GpuVolumetricFluidManager::GetInstance()->SetRuntimeEnabled(true);
	}

	Actor& actor = world->SpawnActor<Actor>();
	actor.SetName(RuntimeActorName("Fluid", context.runtimeTrackId));
	actor.AddTag("__VFX_RUNTIME");
	SceneComponent& root = actor.CreateRootComponent<SceneComponent>();
	root.SetLocalPosition(Add(context.worldPosition, context.localOffset));
	root.RefreshWorldTransform();

	FluidEmitterComponent& emitter = actor.AddComponent<FluidEmitterComponent>();
	emitter.AttachTo(&root);
	emitter.SetLocalPosition({ 0.0f, 0.0f, 0.0f });
	emitter.SetTargetDomain(context.type == VfxCueTrackType::VolumetricFluid
		? FluidEmitterTargetDomain::Volumetric3D
		: FluidEmitterTargetDomain::Fluid2D);
	emitter.SetEmissionEnabled(true);
	emitter.SetRadius(payload->radius * context.parameters.radiusScale);
	emitter.SetSourceVelocity(payload->localVelocity);
	emitter.SetVelocityStrength(payload->velocityStrength * context.parameters.intensityScale);
	emitter.SetDensityRate(payload->densityRate * context.parameters.intensityScale);
	emitter.SetTemperatureRate(payload->temperatureRate * context.parameters.intensityScale);
	emitter.SetFalloffExponent(payload->falloffExponent);

	outToken.value = context.runtimeTrackId;
	entries_[outToken.value] = { world, world->MakeActorHandle(&actor) };
	return true;
}

bool VfxFluidTrackAdapter::Update(VfxTrackRuntimeToken token, const VfxTrackStartContext& context, float)
{
	const auto it = entries_.find(token.value);
	if (it == entries_.end() || it->second.world == nullptr)
	{
		return false;
	}
	Actor* actor = it->second.world->ResolveActor(it->second.actor);
	const auto* payload = std::get_if<VfxFluidTrackPayload>(&context.payload);
	if (actor == nullptr || payload == nullptr)
	{
		return false;
	}

	SceneComponent* root = actor->GetRootComponent();
	FluidEmitterComponent* emitter = actor->GetComponent<FluidEmitterComponent>();
	if (root == nullptr || emitter == nullptr)
	{
		return false;
	}
	root->SetLocalPosition(Add(context.worldPosition, context.localOffset));
	root->RefreshWorldTransform();
	emitter->SetRadius(payload->radius * context.parameters.radiusScale);
	emitter->SetSourceVelocity(payload->localVelocity);
	emitter->SetVelocityStrength(payload->velocityStrength * context.parameters.intensityScale);
	emitter->SetDensityRate(payload->densityRate * context.parameters.intensityScale);
	emitter->SetTemperatureRate(payload->temperatureRate * context.parameters.intensityScale);
	return true;
}

void VfxFluidTrackAdapter::Stop(VfxTrackRuntimeToken token)
{
	const auto it = entries_.find(token.value);
	if (it == entries_.end()) return;
	if (it->second.world != nullptr)
	{
		if (Actor* actor = it->second.world->ResolveActor(it->second.actor))
		{
			it->second.world->DestroyActor(actor);
		}
	}
	entries_.erase(it);
}

void VfxFluidTrackAdapter::StopAll()
{
	for (const auto& [id, entry] : entries_)
	{
		(void)id;
		if (entry.world != nullptr)
		{
			if (Actor* actor = entry.world->ResolveActor(entry.actor)) entry.world->DestroyActor(actor);
		}
	}
	entries_.clear();
}

void VfxFluidTrackAdapter::AbandonWorld(const ActorWorld* world)
{
	if (world == nullptr) return;
	std::erase_if(entries_, [world](const auto& pair)
		{
			return pair.second.world == world; // World破棄済みでもpointer値の比較だけ行い、dereferenceしない。
		});
}

bool VfxLightTrackAdapter::Start(ActorWorld* world, const VfxTrackStartContext& context, VfxTrackRuntimeToken& outToken)
{
	const auto* payload = std::get_if<VfxLightTrackPayload>(&context.payload);
	if (world == nullptr || payload == nullptr)
	{
		return false;
	}

	Actor& actor = world->SpawnActor<Actor>();
	actor.SetName(RuntimeActorName("Light", context.runtimeTrackId));
	actor.AddTag("__VFX_RUNTIME");
	LightComponent& light = actor.CreateRootComponent<LightComponent>();
	light.SetLocalPosition(Add(context.worldPosition, context.localOffset));
	light.SetLightType(LightComponent::LightType::Point);
	light.SetColor(payload->color);
	light.SetIntensity(payload->intensity * context.parameters.intensityScale);
	light.SetRange(payload->range * context.parameters.radiusScale);
	light.SetEnabled(true);
	light.RefreshWorldTransform();

	outToken.value = context.runtimeTrackId;
	entries_[outToken.value] = { world, world->MakeActorHandle(&actor) };
	return true;
}

bool VfxLightTrackAdapter::Update(VfxTrackRuntimeToken token, const VfxTrackStartContext& context, float)
{
	const auto it = entries_.find(token.value);
	if (it == entries_.end() || it->second.world == nullptr)
	{
		return false;
	}
	Actor* actor = it->second.world->ResolveActor(it->second.actor);
	const auto* payload = std::get_if<VfxLightTrackPayload>(&context.payload);
	if (actor == nullptr || payload == nullptr)
	{
		return false;
	}
	LightComponent* light = actor->GetComponent<LightComponent>();
	if (light == nullptr) return false;
	light->SetLocalPosition(Add(context.worldPosition, context.localOffset));
	light->SetColor(payload->color);
	light->SetIntensity(payload->intensity * context.parameters.intensityScale);
	light->SetRange(payload->range * context.parameters.radiusScale);
	light->RefreshWorldTransform();
	return true;
}

void VfxLightTrackAdapter::Stop(VfxTrackRuntimeToken token)
{
	const auto it = entries_.find(token.value);
	if (it == entries_.end()) return;
	if (it->second.world != nullptr)
	{
		if (Actor* actor = it->second.world->ResolveActor(it->second.actor)) it->second.world->DestroyActor(actor);
	}
	entries_.erase(it);
}

void VfxLightTrackAdapter::StopAll()
{
	for (const auto& [id, entry] : entries_)
	{
		(void)id;
		if (entry.world != nullptr)
		{
			if (Actor* actor = entry.world->ResolveActor(entry.actor)) entry.world->DestroyActor(actor);
		}
	}
	entries_.clear();
}

void VfxLightTrackAdapter::AbandonWorld(const ActorWorld* world)
{
	if (world == nullptr) return;
	std::erase_if(entries_, [world](const auto& pair)
		{
			return pair.second.world == world;
		});
}

void VfxPostEffectTrackAdapter::SetEntryEnabled(Entry& entry, bool enabled)
{
	if (entry.enabled == enabled) return;
	PostEffectManager* manager = PostEffectManager::GetInstance();
	if (enabled)
	{
		uint32_t& count = effectRefCounts_[entry.effectName];
		if (count++ == 0) manager->EnableEffect(entry.effectName);
	}
	else
	{
		auto countIt = effectRefCounts_.find(entry.effectName);
		if (countIt != effectRefCounts_.end())
		{
			if (countIt->second <= 1)
			{
				manager->DisableEffect(entry.effectName);
				effectRefCounts_.erase(countIt);
			}
			else
			{
				--countIt->second;
			}
		}
	}
	entry.enabled = enabled;
}

bool VfxPostEffectTrackAdapter::Start(const VfxTrackStartContext& context, VfxTrackRuntimeToken& outToken)
{
	const auto* payload = std::get_if<VfxPostEffectTrackPayload>(&context.payload);
	if (payload == nullptr || payload->effectName.empty() || PostEffectManager::GetInstance()->GetEffect(payload->effectName) == nullptr)
	{
		return false;
	}
	outToken.value = context.runtimeTrackId;
	Entry entry{};
	entry.effectName = payload->effectName;
	entries_[outToken.value] = entry;
	SetEntryEnabled(entries_.at(outToken.value), payload->weight * context.parameters.intensityScale > 0.001f);
	return true;
}

bool VfxPostEffectTrackAdapter::Update(VfxTrackRuntimeToken token, const VfxTrackStartContext& context, float)
{
	const auto it = entries_.find(token.value);
	const auto* payload = std::get_if<VfxPostEffectTrackPayload>(&context.payload);
	if (it == entries_.end() || payload == nullptr)
	{
		return false;
	}
	SetEntryEnabled(it->second, payload->weight * context.parameters.intensityScale > 0.001f);
	return true;
}

void VfxPostEffectTrackAdapter::Stop(VfxTrackRuntimeToken token)
{
	const auto it = entries_.find(token.value);
	if (it == entries_.end()) return;
	SetEntryEnabled(it->second, false);
	entries_.erase(it);
}

void VfxPostEffectTrackAdapter::StopAll()
{
	for (auto& [id, entry] : entries_)
	{
		(void)id;
		SetEntryEnabled(entry, false);
	}
	entries_.clear();
	effectRefCounts_.clear();
}

bool VfxCameraShakeTrackAdapter::Start(const VfxTrackStartContext& context, VfxTrackRuntimeToken& outToken)
{
	const auto* payload = std::get_if<VfxCameraShakeTrackPayload>(&context.payload);
	if (payload == nullptr)
	{
		return false;
	}
	outToken.value = context.runtimeTrackId;
	entries_[outToken.value] = { *payload, context.parameters.intensityScale, 0.0f };
	return true;
}

bool VfxCameraShakeTrackAdapter::Update(VfxTrackRuntimeToken token, const VfxTrackStartContext& context, float trackTime)
{
	const auto it = entries_.find(token.value);
	const auto* payload = std::get_if<VfxCameraShakeTrackPayload>(&context.payload);
	if (it == entries_.end() || payload == nullptr)
	{
		return false;
	}
	it->second.payload = *payload;
	it->second.intensityScale = context.parameters.intensityScale;
	it->second.trackTime = trackTime;
	return true;
}

void VfxCameraShakeTrackAdapter::Stop(VfxTrackRuntimeToken token)
{
	entries_.erase(token.value);
}

void VfxCameraShakeTrackAdapter::StopAll()
{
	entries_.clear();
	RestorePreviousOffset();
}

void VfxCameraShakeTrackAdapter::BeginFrame()
{
	RestorePreviousOffset(); // Gameplay/Editor Camera更新前に前Frameのpresentation offsetだけを戻し、shakeを累積させない。
}

void VfxCameraShakeTrackAdapter::RestorePreviousOffset()
{
	if (!hasAppliedOffset_) return;
	CameraManager* manager = CameraManager::GetInstance();
	if (lastAppliedToDebugCamera_)
	{
		if (DebugCamera* camera = manager->GetDebugCamera())
		{
			camera->SetTranslate(Add(camera->GetTranslate(), Scale(lastTranslationOffset_, -1.0f)));
			camera->SetRotate(Add(camera->GetRotate(), Scale(lastRotationOffsetRadians_, -1.0f)));
			camera->SetFovY(camera->GetFovY() - lastFovOffsetRadians_);
			camera->RefreshViewProjection();
		}
	}
	else if (Camera* camera = manager->GetMainCamera())
	{
		camera->SetTranslate(Add(camera->GetTranslate(), Scale(lastTranslationOffset_, -1.0f)));
		camera->SetRotate(Add(camera->GetRotate(), Scale(lastRotationOffsetRadians_, -1.0f)));
		camera->SetFovY(camera->GetFovY() - lastFovOffsetRadians_);
		camera->Update();
	}

	lastTranslationOffset_ = {};
	lastRotationOffsetRadians_ = {};
	lastFovOffsetRadians_ = 0.0f;
	hasAppliedOffset_ = false;
}

void VfxCameraShakeTrackAdapter::Apply()
{
	if (entries_.empty()) return;

	Vector3 translation{};
	Vector3 rotationDegrees{};
	float fovDegrees = 0.0f;
	for (const auto& [id, entry] : entries_)
	{
		const float phase = static_cast<float>(id % 1024ull) * 0.17320508f;
		const float omega = entry.payload.frequency * 2.0f * std::numbers::pi_v<float>;
		const float t = entry.trackTime;
		const float sx = std::sin(omega * t + phase);
		const float sy = std::sin(omega * t * 1.071f + phase + 2.0943951f);
		const float sz = std::sin(omega * t * 0.937f + phase + 4.1887902f);
		translation.x += entry.payload.translationAmplitude.x * sx * entry.intensityScale;
		translation.y += entry.payload.translationAmplitude.y * sy * entry.intensityScale;
		translation.z += entry.payload.translationAmplitude.z * sz * entry.intensityScale;
		rotationDegrees.x += entry.payload.rotationAmplitudeDegrees.x * sy * entry.intensityScale;
		rotationDegrees.y += entry.payload.rotationAmplitudeDegrees.y * sz * entry.intensityScale;
		rotationDegrees.z += entry.payload.rotationAmplitudeDegrees.z * sx * entry.intensityScale;
		fovDegrees += entry.payload.fovAmplitudeDegrees * sx * entry.intensityScale;
	}

	constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
	lastTranslationOffset_ = translation;
	lastRotationOffsetRadians_ = Scale(rotationDegrees, kDegToRad);
	lastFovOffsetRadians_ = fovDegrees * kDegToRad;

	CameraManager* manager = CameraManager::GetInstance();
	lastAppliedToDebugCamera_ = manager->IsUsingDebugCamera();
	if (lastAppliedToDebugCamera_)
	{
		if (DebugCamera* camera = manager->GetDebugCamera())
		{
			camera->SetTranslate(Add(camera->GetTranslate(), lastTranslationOffset_));
			camera->SetRotate(Add(camera->GetRotate(), lastRotationOffsetRadians_));
			camera->SetFovY(camera->GetFovY() + lastFovOffsetRadians_);
			camera->RefreshViewProjection();
			hasAppliedOffset_ = true;
		}
	}
	else if (Camera* camera = manager->GetMainCamera())
	{
		camera->SetTranslate(Add(camera->GetTranslate(), lastTranslationOffset_));
		camera->SetRotate(Add(camera->GetRotate(), lastRotationOffsetRadians_));
		camera->SetFovY(camera->GetFovY() + lastFovOffsetRadians_);
		camera->Update();
		hasAppliedOffset_ = true;
	}
}

bool VfxTrackAdapterSet::Start(
	ActorWorld* world,
	const VfxTrackStartContext& context,
	const VfxRuntimeBudget& budget,
	VfxTrackRuntimeToken& outToken)
{
	switch (context.type)
	{
	case VfxCueTrackType::Particle:
		return particle_.Start(context, outToken);
	case VfxCueTrackType::Fluid2D:
	case VfxCueTrackType::VolumetricFluid:
		if (fluid_.GetActiveCount() >= budget.maxFluidTracks) return false;
		return fluid_.Start(world, context, outToken);
	case VfxCueTrackType::Light:
		if (light_.GetActiveCount() >= budget.maxTransientLights) return false;
		return light_.Start(world, context, outToken);
	case VfxCueTrackType::PostEffect:
		return postEffect_.Start(context, outToken);
	case VfxCueTrackType::CameraShake:
		if (cameraShake_.GetActiveCount() >= budget.maxCameraShakes) return false;
		return cameraShake_.Start(context, outToken);
	default:
		return false;
	}
}

bool VfxTrackAdapterSet::Update(VfxTrackRuntimeToken token, const VfxTrackStartContext& context, float trackTime)
{
	switch (context.type)
	{
	case VfxCueTrackType::Particle: return particle_.Update(token, context, trackTime);
	case VfxCueTrackType::Fluid2D:
	case VfxCueTrackType::VolumetricFluid: return fluid_.Update(token, context, trackTime);
	case VfxCueTrackType::Light: return light_.Update(token, context, trackTime);
	case VfxCueTrackType::PostEffect: return postEffect_.Update(token, context, trackTime);
	case VfxCueTrackType::CameraShake: return cameraShake_.Update(token, context, trackTime);
	default: return false;
	}
}

void VfxTrackAdapterSet::Stop(VfxTrackRuntimeToken token, VfxCueTrackType type)
{
	switch (type)
	{
	case VfxCueTrackType::Particle: particle_.Stop(token); break;
	case VfxCueTrackType::Fluid2D:
	case VfxCueTrackType::VolumetricFluid: fluid_.Stop(token); break;
	case VfxCueTrackType::Light: light_.Stop(token); break;
	case VfxCueTrackType::PostEffect: postEffect_.Stop(token); break;
	case VfxCueTrackType::CameraShake: cameraShake_.Stop(token); break;
	default: break;
	}
}

void VfxTrackAdapterSet::StopAll()
{
	particle_.StopAll();
	fluid_.StopAll();
	light_.StopAll();
	postEffect_.StopAll();
	cameraShake_.StopAll();
}

void VfxTrackAdapterSet::AbandonWorld(const ActorWorld* world)
{
	fluid_.AbandonWorld(world);
	light_.AbandonWorld(world);
}

void VfxTrackAdapterSet::BeginFrame()
{
	cameraShake_.BeginFrame();
}

void VfxTrackAdapterSet::EndUpdate()
{
	cameraShake_.Apply();
}

} // namespace Ken4lowEngine
