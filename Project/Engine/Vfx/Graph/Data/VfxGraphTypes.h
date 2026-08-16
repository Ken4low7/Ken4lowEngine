#pragma once

#include "Engine/Graphics/Renderer/GpuParticle/Data/GpuParticleEffectDesc.h"

#include <Vector2.h>
#include <Vector3.h>
#include <Vector4.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace Ken4lowEngine
{

enum class VfxGraphNodeStage : uint32_t
{
	Spawn = 0,
	Initialize,
	Update,
	Render,
};

enum class VfxCurveInterpolation : uint32_t
{
	Linear = 0,
	Step,
	SmoothStep,
};

enum class VfxParticleEventType : uint32_t
{
	Collision = 0,
	Death,
};

enum class VfxCollisionShape : uint32_t
{
	Plane = 0,
	Sphere,
};

enum class VfxCollisionResponse : uint32_t
{
	Bounce = 0,
	Slide,
	Kill,
};

struct VfxFloatCurveKey
{
	float time = 0.0f;
	float value = 1.0f;
};

struct VfxFloatCurve
{
	VfxCurveInterpolation interpolation = VfxCurveInterpolation::Linear;
	std::vector<VfxFloatCurveKey> keys{
		{ 0.0f, 1.0f },
		{ 1.0f, 1.0f },
	};

	[[nodiscard]] float Evaluate(float normalizedTime) const;
};

struct VfxColorGradientKey
{
	float time = 0.0f;
	Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
};

struct VfxColorGradient
{
	VfxCurveInterpolation interpolation = VfxCurveInterpolation::Linear;
	std::vector<VfxColorGradientKey> keys{
		{ 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f } },
		{ 1.0f, { 1.0f, 1.0f, 1.0f, 0.0f } },
	};

	[[nodiscard]] Vector4 Evaluate(float normalizedTime) const;
};

enum class VfxGraphNodeType : uint32_t
{
	SpawnRate = 0,
	Burst,
	SpawnPoint,
	SpawnSphere,
	SpawnBox,
	Lifetime,
	InitialVelocity,
	InitialColor,
	InitialSize,
	Gravity,
	Drag,
	SpriteRenderer,
	InitialRotation,
	RotationRate,
	SizeOverLife,
	ColorOverLife,
	Collision,
	DeathEvent,
	SubEmitter,
	RibbonRenderer,
	TrailRenderer,
	MeshRenderer,
};

struct VfxGraphSpawnRateNode
{
	float rate = 10.0f;
};

struct VfxGraphBurstNode
{
	uint32_t count = 16u;
};

struct VfxGraphSpawnPointNode
{
};

struct VfxGraphSpawnSphereNode
{
	float radius = 0.5f;
};

struct VfxGraphSpawnBoxNode
{
	Vector3 size{ 1.0f, 1.0f, 1.0f };
};

struct VfxGraphLifetimeNode
{
	float lifetime = 1.0f;
	float random = 0.0f;
};

struct VfxGraphInitialVelocityNode
{
	Vector3 velocity{ 0.0f, 1.0f, 0.0f };
	Vector3 random{ 0.0f, 0.0f, 0.0f };
	float speed = 0.0f;
	float speedRandom = 0.0f;
};

struct VfxGraphInitialColorNode
{
	Vector4 start{ 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 end{ 1.0f, 1.0f, 1.0f, 0.0f };
	bool alphaFade = true;
};

struct VfxGraphInitialSizeNode
{
	Vector2 start{ 0.1f, 0.1f };
	Vector2 end{ 0.5f, 0.5f };
	float random = 0.0f;
};

struct VfxGraphGravityNode
{
	Vector3 acceleration{ 0.0f, -9.8f, 0.0f };
};

struct VfxGraphDragNode
{
	float damping = 0.0f;
};

struct VfxGraphInitialRotationNode
{
	float rotation = 0.0f;
	float random = 0.0f;
};

struct VfxGraphRotationRateNode
{
	float radiansPerSecond = 0.0f;
};

struct VfxGraphSizeOverLifeNode
{
	VfxFloatCurve multiplier{};
};

struct VfxGraphColorOverLifeNode
{
	VfxColorGradient gradient{};
};

struct VfxGraphCollisionNode
{
	VfxCollisionShape shape = VfxCollisionShape::Plane;
	VfxCollisionResponse response = VfxCollisionResponse::Bounce;
	Vector3 planeNormal{ 0.0f, 1.0f, 0.0f };
	float planeDistance = 0.0f;
	Vector3 sphereCenter{ 0.0f, 0.0f, 0.0f };
	float sphereRadius = 1.0f;
	float particleRadius = 0.02f;
	float restitution = 0.5f;
	float friction = 0.1f;
	bool generateEvent = true;
};

struct VfxGraphDeathEventNode
{
};

struct VfxGraphSubEmitterNode
{
	VfxParticleEventType sourceEvent = VfxParticleEventType::Collision;
	uint32_t count = 6u;
	float lifeTime = 0.2f;
	float speed = 1.5f;
	float spread = 1.0f;
	float inheritVelocity = 0.2f;
	Vector2 startSize{ 0.04f, 0.04f };
	Vector2 endSize{ 0.12f, 0.12f };
	Vector4 startColor{ 1.0f, 0.8f, 0.2f, 1.0f };
	Vector4 endColor{ 1.0f, 0.1f, 0.0f, 0.0f };
	bool alphaFade = true;
};

struct VfxGraphSpriteRendererNode
{
	std::string texturePath = "Effects/white.dds";
	GpuParticleBlendMode blendMode = GpuParticleBlendMode::Additive;
	bool billboard = true;
};

struct VfxGraphRibbonRendererNode
{
	std::string texturePath = "Effects/white.dds";
	GpuParticleBlendMode blendMode = GpuParticleBlendMode::Additive;
	float width = 0.08f;
	float length = 0.8f;
};

struct VfxGraphTrailRendererNode
{
	std::string texturePath = "Effects/white.dds";
	GpuParticleBlendMode blendMode = GpuParticleBlendMode::Additive;
	float width = 0.05f;
	float length = 1.25f;
};

struct VfxGraphMeshRendererNode
{
	std::string meshPath = "Sample/cube.gltf";
	uint32_t subMeshIndex = 0u;
	GpuParticleBlendMode blendMode = GpuParticleBlendMode::Alpha;
	Vector3 startScale{ 1.0f, 1.0f, 1.0f };
	Vector3 endScale{ 1.0f, 1.0f, 1.0f };
	Vector3 startRotation{};
	Vector3 angularVelocity{};
};

using VfxGraphNodePayload = std::variant<
	VfxGraphSpawnRateNode,
	VfxGraphBurstNode,
	VfxGraphSpawnPointNode,
	VfxGraphSpawnSphereNode,
	VfxGraphSpawnBoxNode,
	VfxGraphLifetimeNode,
	VfxGraphInitialVelocityNode,
	VfxGraphInitialColorNode,
	VfxGraphInitialSizeNode,
	VfxGraphGravityNode,
	VfxGraphDragNode,
	VfxGraphSpriteRendererNode,
	VfxGraphInitialRotationNode,
	VfxGraphRotationRateNode,
	VfxGraphSizeOverLifeNode,
	VfxGraphColorOverLifeNode,
	VfxGraphCollisionNode,
	VfxGraphDeathEventNode,
	VfxGraphSubEmitterNode,
	VfxGraphRibbonRendererNode,
	VfxGraphTrailRendererNode,
	VfxGraphMeshRendererNode>;

struct VfxGraphNodeDesc
{
	uint32_t id = 0u;
	std::string name = "Node";
	VfxGraphNodeStage stage = VfxGraphNodeStage::Spawn;
	VfxGraphNodeType type = VfxGraphNodeType::SpawnRate;
	bool enabled = true;
	Vector2 editorPosition{ 0.0f, 0.0f };
	VfxGraphNodePayload payload = VfxGraphSpawnRateNode{};
};

struct VfxGraphEdgeDesc
{
	uint32_t fromNodeId = 0u;
	uint32_t toNodeId = 0u;
};

struct VfxGraphEmitterDesc
{
	std::string name = "Emitter";
	uint32_t maxParticles = 1024u;
	bool loop = false;
	float duration = 1.0f;
	std::vector<GpuParticleParameterBindingDesc> parameterBindings;
	std::vector<VfxGraphNodeDesc> nodes;
	std::vector<VfxGraphEdgeDesc> edges;
};

struct VfxGraphDesc
{
	static constexpr uint32_t kSchemaVersion = 1u;
	static constexpr uint32_t kMaxEmitters = 32u;
	static constexpr uint32_t kMaxNodesPerEmitter = 128u;
	static constexpr uint32_t kMaxEdgesPerEmitter = 256u;
	static constexpr uint32_t kMaxCurveKeys = 32u;
	static constexpr uint32_t kMaxGradientKeys = 32u;
	static constexpr uint32_t kMaxSubEmitterSpawnCount = 64u;

	uint32_t schemaVersion = kSchemaVersion;
	std::string graphName = "NewVfxGraph";
	std::vector<GpuParticleUserParameterDesc> userParameters;
	std::vector<VfxGraphEmitterDesc> emitters;
};

VfxGraphNodeStage GetExpectedVfxGraphNodeStage(VfxGraphNodeType type);
const char* ToString(VfxGraphNodeStage stage);
const char* ToString(VfxGraphNodeType type);
const char* ToString(VfxParticleEventType eventType);
const char* ToString(VfxCollisionShape shape);
const char* ToString(VfxCollisionResponse response);
bool TryParseVfxGraphNodeStage(const std::string& text, VfxGraphNodeStage& outStage);
bool TryParseVfxGraphNodeType(const std::string& text, VfxGraphNodeType& outType);
bool TryParseVfxParticleEventType(const std::string& text, VfxParticleEventType& outEventType);
bool TryParseVfxCollisionShape(const std::string& text, VfxCollisionShape& outShape);
bool TryParseVfxCollisionResponse(const std::string& text, VfxCollisionResponse& outResponse);

} // namespace Ken4lowEngine
