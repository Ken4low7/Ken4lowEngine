#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <Vector2.h>
#include <Vector3.h>
#include <Vector4.h>

namespace Ken4lowEngine
{

enum class GpuParticleRenderType : uint32_t
{
	Sprite = 0,
	Mesh = 1,
	Ribbon = 2,
	Trail = 3,
};

enum class GpuParticleBlendMode : uint32_t
{
	Alpha = 0,
	Additive,
	Multiply,
};

enum class GpuParticleSpawnShape : uint32_t
{
	Point = 0,
	Sphere,
	Box,
	Cone,
	Circle,
	Ring,
	Hemisphere,
};

enum class GpuParticleParameterTarget : uint32_t
{
	SpawnRate = 0,
	BurstCount,
	LifeTime,
	Speed,
	Size,
	Alpha,
	Force,
};

enum class GpuParticleCollisionShape : uint32_t
{
	None = 0,
	Plane,
	Sphere,
};

enum class GpuParticleCollisionResponse : uint32_t
{
	Bounce = 0,
	Slide,
	Kill,
};

enum class GpuParticleEventType : uint32_t
{
	Collision = 0,
	Death,
};

inline constexpr uint32_t kGpuParticleEventCollision = 1u << static_cast<uint32_t>(GpuParticleEventType::Collision);
inline constexpr uint32_t kGpuParticleEventDeath = 1u << static_cast<uint32_t>(GpuParticleEventType::Death);

struct GpuParticleUserParameterDesc
{
	std::string name = "Parameter";
	float defaultValue = 1.0f;
	float minValue = 0.0f;
	float maxValue = 10.0f;
};

struct GpuParticleParameterBindingDesc
{
	std::string parameterName;
	GpuParticleParameterTarget target = GpuParticleParameterTarget::Speed;
	float scale = 1.0f;
	float bias = 0.0f;
};

struct GpuParticleEmitterDesc
{
	std::string name = "Emitter";
	GpuParticleRenderType renderType = GpuParticleRenderType::Sprite;

	std::string texturePath;
	std::string meshPath;
	uint32_t meshSubMeshIndex = 0;

	uint32_t maxParticles = 1024;
	bool loop = true;
	float duration = 1.0f;
	float spawnRate = 10.0f;
	uint32_t burstCount = 16;
	float lifeTime = 1.0f;
	float lifeTimeRandom = 0.0f;

	Vector3 position{ 0.0f, 0.0f, 0.0f };
	Vector3 positionRandom{ 0.0f, 0.0f, 0.0f };
	GpuParticleSpawnShape spawnShape = GpuParticleSpawnShape::Point;
	float spawnRadius = 0.0f;
	Vector3 spawnBoxSize{ 0.0f, 0.0f, 0.0f };

	Vector3 velocity{ 0.0f, 0.0f, 0.0f };
	Vector3 velocityRandom{ 0.0f, 0.0f, 0.0f };
	Vector3 gravity{ 0.0f, 0.0f, 0.0f };
	float damping = 0.0f;
	float speed = 0.0f;
	float speedRandom = 0.0f;

	float noiseStrength = 0.0f;
	float noiseFrequency = 1.0f;
	Vector3 vortexAxis{ 0.0f, 1.0f, 0.0f };
	float vortexStrength = 0.0f;
	Vector3 attractorPosition{ 0.0f, 0.0f, 0.0f };
	float attractorStrength = 0.0f;
	float attractorRadius = 0.0f;

	Vector2 startSize{ 1.0f, 1.0f };
	Vector2 endSize{ 1.0f, 1.0f };
	float sizeRandom = 0.0f;
	bool useSizeCurve = false;
	Vector4 sizeCurveLut{ 1.0f, 1.0f, 1.0f, 1.0f };

	Vector4 startColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 endColor{ 1.0f, 1.0f, 1.0f, 0.0f };
	Vector4 colorRandom{ 0.0f, 0.0f, 0.0f, 0.0f };
	bool alphaFade = true;
	bool useColorGradient = false;
	std::array<Vector4, 4> colorGradientLut{
		Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
		Vector4{ 1.0f, 1.0f, 1.0f, 0.66f },
		Vector4{ 1.0f, 1.0f, 1.0f, 0.33f },
		Vector4{ 1.0f, 1.0f, 1.0f, 0.0f }
	};

	float startRotation = 0.0f;
	float rotationSpeed = 0.0f;
	float rotationRandom = 0.0f;

	bool billboard = true;
	GpuParticleBlendMode blendMode = GpuParticleBlendMode::Additive;
	bool useSpriteSheet = false;
	int spriteSheetRows = 1;
	int spriteSheetColumns = 1;
	float spriteSheetFrameRate = 0.0f;

	Vector3 startScale3D{ 1.0f, 1.0f, 1.0f };
	Vector3 endScale3D{ 1.0f, 1.0f, 1.0f };
	Vector3 startRotation3D{ 0.0f, 0.0f, 0.0f };
	Vector3 rotationRandom3D{ 0.0f, 0.0f, 0.0f };
	Vector3 angularVelocity{ 0.0f, 0.0f, 0.0f };
	Vector3 angularVelocityRandom{ 0.0f, 0.0f, 0.0f };

	GpuParticleCollisionShape collisionShape = GpuParticleCollisionShape::None;
	GpuParticleCollisionResponse collisionResponse = GpuParticleCollisionResponse::Bounce;
	Vector3 collisionPlaneNormal{ 0.0f, 1.0f, 0.0f };
	float collisionPlaneDistance = 0.0f;
	Vector3 collisionSphereCenter{ 0.0f, 0.0f, 0.0f };
	float collisionSphereRadius = 1.0f;
	float collisionParticleRadius = 0.02f;
	float collisionRestitution = 0.5f;
	float collisionFriction = 0.1f;
	uint32_t eventMask = 0u;

	// Phase22 keeps collision/death events GPU-local so child particles can spawn without a CPU readback stall.
	uint32_t subEmitterEventMask = 0u;
	uint32_t subEmitterCount = 0u;
	float subEmitterLifeTime = 0.2f;
	float subEmitterSpeed = 1.5f;
	float subEmitterSpread = 1.0f;
	float subEmitterInheritVelocity = 0.2f;
	Vector2 subEmitterStartSize{ 0.04f, 0.04f };
	Vector2 subEmitterEndSize{ 0.12f, 0.12f };
	Vector4 subEmitterStartColor{ 1.0f, 0.8f, 0.2f, 1.0f };
	Vector4 subEmitterEndColor{ 1.0f, 0.1f, 0.0f, 0.0f };
	bool subEmitterAlphaFade = true;

	std::vector<GpuParticleParameterBindingDesc> parameterBindings;
};

struct GpuParticleEffectDesc
{
	std::string effectName = "Effect";
	std::vector<GpuParticleUserParameterDesc> userParameters;
	std::vector<GpuParticleEmitterDesc> emitters;
};

inline GpuParticleEmitterDesc CreateDefaultSpriteEmitterDesc()
{
	GpuParticleEmitterDesc desc{};
	desc.name = "SpriteEmitter";
	desc.renderType = GpuParticleRenderType::Sprite;
	desc.texturePath = "Effects/white.dds";
	desc.maxParticles = 1024;
	desc.loop = true;
	desc.duration = 1.0f;
	desc.spawnRate = 50.0f;
	desc.burstCount = 32;
	desc.lifeTime = 2.0f;
	desc.velocity = { 0.0f, 2.0f, 0.0f };
	desc.velocityRandom = { 1.0f, 1.0f, 1.0f };
	desc.gravity = { 0.0f, -2.0f, 0.0f };
	desc.startSize = { 0.1f, 0.1f };
	desc.endSize = { 0.8f, 0.8f };
	desc.startColor = { 1.0f, 0.8f, 0.2f, 1.0f };
	desc.endColor = { 1.0f, 0.0f, 0.0f, 0.0f };
	desc.spawnShape = GpuParticleSpawnShape::Point;
	desc.blendMode = GpuParticleBlendMode::Additive;
	desc.billboard = true;
	return desc;
}

inline GpuParticleEmitterDesc CreateDefaultMeshEmitterDesc()
{
	GpuParticleEmitterDesc desc{};
	desc.name = "MeshEmitter";
	desc.renderType = GpuParticleRenderType::Mesh;
	desc.texturePath.clear();
	desc.meshPath = "Sample/cube.gltf";
	desc.maxParticles = 256;
	desc.loop = false;
	desc.duration = 1.0f;
	desc.spawnRate = 10.0f;
	desc.burstCount = 8;
	desc.lifeTime = 2.0f;
	desc.velocity = { 0.0f, 2.0f, 0.0f };
	desc.gravity = { 0.0f, -9.8f, 0.0f };
	desc.startScale3D = { 0.2f, 0.2f, 0.2f };
	desc.endScale3D = { 0.1f, 0.1f, 0.1f };
	desc.angularVelocity = { 0.0f, 2.0f, 0.0f };
	desc.spawnShape = GpuParticleSpawnShape::Sphere;
	desc.blendMode = GpuParticleBlendMode::Alpha;
	desc.billboard = false;
	return desc;
}

inline GpuParticleEffectDesc CreateDefaultGpuParticleEffectDesc()
{
	GpuParticleEffectDesc desc{};
	desc.effectName = "NewGpuParticleEffect";
	desc.emitters.push_back(CreateDefaultSpriteEmitterDesc());
	return desc;
}

} // namespace Ken4lowEngine
