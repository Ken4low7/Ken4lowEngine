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

struct VfxGraphSpriteRendererNode
{
	std::string texturePath = "Effects/white.dds";
	GpuParticleBlendMode blendMode = GpuParticleBlendMode::Additive;
	bool billboard = true;
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
	VfxGraphSpriteRendererNode>;

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

	uint32_t schemaVersion = kSchemaVersion;
	std::string graphName = "NewVfxGraph";
	std::vector<GpuParticleUserParameterDesc> userParameters;
	std::vector<VfxGraphEmitterDesc> emitters;
};

VfxGraphNodeStage GetExpectedVfxGraphNodeStage(VfxGraphNodeType type);
const char* ToString(VfxGraphNodeStage stage);
const char* ToString(VfxGraphNodeType type);
bool TryParseVfxGraphNodeStage(const std::string& text, VfxGraphNodeStage& outStage);
bool TryParseVfxGraphNodeType(const std::string& text, VfxGraphNodeType& outType);

} // namespace Ken4lowEngine
