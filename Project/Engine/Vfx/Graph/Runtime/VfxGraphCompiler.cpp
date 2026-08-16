#include "VfxGraphCompiler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
namespace
{
bool IsFinite(const Vector2& value) { return std::isfinite(value.x) && std::isfinite(value.y); }
bool IsFinite(const Vector3& value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }
bool IsFinite(const Vector4& value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w); }

bool IsValidInterpolation(VfxCurveInterpolation interpolation)
{
	return static_cast<uint32_t>(interpolation) <= static_cast<uint32_t>(VfxCurveInterpolation::SmoothStep);
}

void ValidateFloatCurve(const VfxFloatCurve& curve, std::vector<std::string>& errors, const std::string& prefix)
{
	if (!IsValidInterpolation(curve.interpolation)) errors.push_back(prefix + "curve interpolation is invalid");
	if (curve.keys.empty())
	{
		errors.push_back(prefix + "curve requires at least one key");
		return;
	}
	if (curve.keys.size() > VfxGraphDesc::kMaxCurveKeys) errors.push_back(prefix + "curve exceeds kMaxCurveKeys");
	float previousTime = -1.0f;
	for (const VfxFloatCurveKey& key : curve.keys)
	{
		if (!std::isfinite(key.time) || !std::isfinite(key.value)) errors.push_back(prefix + "curve keys must be finite");
		if (key.time < 0.0f || key.time > 1.0f) errors.push_back(prefix + "curve key time must be in [0, 1]");
		if (key.time <= previousTime) errors.push_back(prefix + "curve key times must be strictly increasing");
		previousTime = key.time;
	}
}

void ValidateColorGradient(const VfxColorGradient& gradient, std::vector<std::string>& errors, const std::string& prefix)
{
	if (!IsValidInterpolation(gradient.interpolation)) errors.push_back(prefix + "gradient interpolation is invalid");
	if (gradient.keys.empty())
	{
		errors.push_back(prefix + "gradient requires at least one key");
		return;
	}
	if (gradient.keys.size() > VfxGraphDesc::kMaxGradientKeys) errors.push_back(prefix + "gradient exceeds kMaxGradientKeys");
	float previousTime = -1.0f;
	for (const VfxColorGradientKey& key : gradient.keys)
	{
		if (!std::isfinite(key.time) || !IsFinite(key.color)) errors.push_back(prefix + "gradient keys must be finite");
		if (key.time < 0.0f || key.time > 1.0f) errors.push_back(prefix + "gradient key time must be in [0, 1]");
		if (key.time <= previousTime) errors.push_back(prefix + "gradient key times must be strictly increasing");
		previousTime = key.time;
	}
}

bool PayloadMatchesType(const VfxGraphNodeDesc& node)
{
	switch (node.type)
	{
	case VfxGraphNodeType::SpawnRate: return std::holds_alternative<VfxGraphSpawnRateNode>(node.payload);
	case VfxGraphNodeType::Burst: return std::holds_alternative<VfxGraphBurstNode>(node.payload);
	case VfxGraphNodeType::SpawnPoint: return std::holds_alternative<VfxGraphSpawnPointNode>(node.payload);
	case VfxGraphNodeType::SpawnSphere: return std::holds_alternative<VfxGraphSpawnSphereNode>(node.payload);
	case VfxGraphNodeType::SpawnBox: return std::holds_alternative<VfxGraphSpawnBoxNode>(node.payload);
	case VfxGraphNodeType::Lifetime: return std::holds_alternative<VfxGraphLifetimeNode>(node.payload);
	case VfxGraphNodeType::InitialVelocity: return std::holds_alternative<VfxGraphInitialVelocityNode>(node.payload);
	case VfxGraphNodeType::InitialColor: return std::holds_alternative<VfxGraphInitialColorNode>(node.payload);
	case VfxGraphNodeType::InitialSize: return std::holds_alternative<VfxGraphInitialSizeNode>(node.payload);
	case VfxGraphNodeType::Gravity: return std::holds_alternative<VfxGraphGravityNode>(node.payload);
	case VfxGraphNodeType::Drag: return std::holds_alternative<VfxGraphDragNode>(node.payload);
	case VfxGraphNodeType::SpriteRenderer: return std::holds_alternative<VfxGraphSpriteRendererNode>(node.payload);
	case VfxGraphNodeType::InitialRotation: return std::holds_alternative<VfxGraphInitialRotationNode>(node.payload);
	case VfxGraphNodeType::RotationRate: return std::holds_alternative<VfxGraphRotationRateNode>(node.payload);
	case VfxGraphNodeType::SizeOverLife: return std::holds_alternative<VfxGraphSizeOverLifeNode>(node.payload);
	case VfxGraphNodeType::ColorOverLife: return std::holds_alternative<VfxGraphColorOverLifeNode>(node.payload);
	case VfxGraphNodeType::Collision: return std::holds_alternative<VfxGraphCollisionNode>(node.payload);
	case VfxGraphNodeType::DeathEvent: return std::holds_alternative<VfxGraphDeathEventNode>(node.payload);
	case VfxGraphNodeType::SubEmitter: return std::holds_alternative<VfxGraphSubEmitterNode>(node.payload);
	default: return false;
	}
}

void ValidateNodeValues(const VfxGraphNodeDesc& node, std::vector<std::string>& errors, const std::string& emitterName)
{
	const std::string prefix = "Emitter '" + emitterName + "' node '" + node.name + "': ";
	switch (node.type)
	{
	case VfxGraphNodeType::SpawnRate:
	{
		const auto& p = std::get<VfxGraphSpawnRateNode>(node.payload);
		if (!std::isfinite(p.rate) || p.rate < 0.0f) errors.push_back(prefix + "spawn rate must be finite and >= 0");
		break;
	}
	case VfxGraphNodeType::SpawnSphere:
	{
		const auto& p = std::get<VfxGraphSpawnSphereNode>(node.payload);
		if (!std::isfinite(p.radius) || p.radius < 0.0f) errors.push_back(prefix + "sphere radius must be finite and >= 0");
		break;
	}
	case VfxGraphNodeType::SpawnBox:
	{
		const auto& p = std::get<VfxGraphSpawnBoxNode>(node.payload);
		if (!IsFinite(p.size) || p.size.x < 0.0f || p.size.y < 0.0f || p.size.z < 0.0f) errors.push_back(prefix + "box size must be finite and non-negative");
		break;
	}
	case VfxGraphNodeType::Lifetime:
	{
		const auto& p = std::get<VfxGraphLifetimeNode>(node.payload);
		if (!std::isfinite(p.lifetime) || p.lifetime <= 0.0f || !std::isfinite(p.random) || p.random < 0.0f) errors.push_back(prefix + "lifetime must be > 0 and random must be >= 0");
		break;
	}
	case VfxGraphNodeType::InitialVelocity:
	{
		const auto& p = std::get<VfxGraphInitialVelocityNode>(node.payload);
		if (!IsFinite(p.velocity) || !IsFinite(p.random) || !std::isfinite(p.speed) || !std::isfinite(p.speedRandom) || p.speed < 0.0f || p.speedRandom < 0.0f)
			errors.push_back(prefix + "velocity values must be finite and speed ranges non-negative");
		break;
	}
	case VfxGraphNodeType::InitialColor:
	{
		const auto& p = std::get<VfxGraphInitialColorNode>(node.payload);
		if (!IsFinite(p.start) || !IsFinite(p.end)) errors.push_back(prefix + "color values must be finite");
		break;
	}
	case VfxGraphNodeType::InitialSize:
	{
		const auto& p = std::get<VfxGraphInitialSizeNode>(node.payload);
		if (!IsFinite(p.start) || !IsFinite(p.end) || !std::isfinite(p.random) || p.random < 0.0f || p.start.x < 0.0f || p.start.y < 0.0f || p.end.x < 0.0f || p.end.y < 0.0f)
			errors.push_back(prefix + "size values must be finite and non-negative");
		break;
	}
	case VfxGraphNodeType::Gravity:
		if (!IsFinite(std::get<VfxGraphGravityNode>(node.payload).acceleration)) errors.push_back(prefix + "gravity must be finite");
		break;
	case VfxGraphNodeType::Drag:
	{
		const float damping = std::get<VfxGraphDragNode>(node.payload).damping;
		if (!std::isfinite(damping) || damping < 0.0f) errors.push_back(prefix + "drag damping must be finite and >= 0");
		break;
	}
	case VfxGraphNodeType::InitialRotation:
	{
		const auto& p = std::get<VfxGraphInitialRotationNode>(node.payload);
		if (!std::isfinite(p.rotation) || !std::isfinite(p.random) || p.random < 0.0f) errors.push_back(prefix + "rotation must be finite and random must be >= 0");
		break;
	}
	case VfxGraphNodeType::RotationRate:
		if (!std::isfinite(std::get<VfxGraphRotationRateNode>(node.payload).radiansPerSecond)) errors.push_back(prefix + "rotation rate must be finite");
		break;
	case VfxGraphNodeType::SizeOverLife:
		ValidateFloatCurve(std::get<VfxGraphSizeOverLifeNode>(node.payload).multiplier, errors, prefix);
		break;
	case VfxGraphNodeType::ColorOverLife:
		ValidateColorGradient(std::get<VfxGraphColorOverLifeNode>(node.payload).gradient, errors, prefix);
		break;
	case VfxGraphNodeType::Collision:
	{
		const auto& p = std::get<VfxGraphCollisionNode>(node.payload);
		if (static_cast<uint32_t>(p.shape) > static_cast<uint32_t>(VfxCollisionShape::Sphere)) errors.push_back(prefix + "collision shape is invalid");
		if (static_cast<uint32_t>(p.response) > static_cast<uint32_t>(VfxCollisionResponse::Kill)) errors.push_back(prefix + "collision response is invalid");
		if (!IsFinite(p.planeNormal) || !std::isfinite(p.planeDistance) || !IsFinite(p.sphereCenter) || !std::isfinite(p.sphereRadius) ||
			!std::isfinite(p.particleRadius) || !std::isfinite(p.restitution) || !std::isfinite(p.friction)) errors.push_back(prefix + "collision values must be finite");
		const float normalSq = p.planeNormal.x * p.planeNormal.x + p.planeNormal.y * p.planeNormal.y + p.planeNormal.z * p.planeNormal.z;
		if (p.shape == VfxCollisionShape::Plane && normalSq <= 1.0e-8f) errors.push_back(prefix + "plane normal must be non-zero");
		if (p.shape == VfxCollisionShape::Sphere && p.sphereRadius <= 0.0f) errors.push_back(prefix + "sphere radius must be > 0");
		if (p.particleRadius < 0.0f) errors.push_back(prefix + "particle radius must be >= 0");
		if (p.restitution < 0.0f || p.restitution > 1.0f) errors.push_back(prefix + "restitution must be in [0, 1]");
		if (p.friction < 0.0f || p.friction > 1.0f) errors.push_back(prefix + "friction must be in [0, 1]");
		break;
	}
	case VfxGraphNodeType::SubEmitter:
	{
		const auto& p = std::get<VfxGraphSubEmitterNode>(node.payload);
		if (static_cast<uint32_t>(p.sourceEvent) > static_cast<uint32_t>(VfxParticleEventType::Death)) errors.push_back(prefix + "sub emitter source event is invalid");
		if (p.count == 0u || p.count > VfxGraphDesc::kMaxSubEmitterSpawnCount) errors.push_back(prefix + "sub emitter count must be 1-kMaxSubEmitterSpawnCount");
		if (!std::isfinite(p.lifeTime) || p.lifeTime <= 0.0f || !std::isfinite(p.speed) || p.speed < 0.0f || !std::isfinite(p.spread) || p.spread < 0.0f || !std::isfinite(p.inheritVelocity) || p.inheritVelocity < 0.0f)
			errors.push_back(prefix + "sub emitter motion values are invalid");
		if (!IsFinite(p.startSize) || !IsFinite(p.endSize) || p.startSize.x < 0.0f || p.startSize.y < 0.0f || p.endSize.x < 0.0f || p.endSize.y < 0.0f || !IsFinite(p.startColor) || !IsFinite(p.endColor))
			errors.push_back(prefix + "sub emitter size/color values are invalid");
		break;
	}
	case VfxGraphNodeType::SpriteRenderer:
		if (std::get<VfxGraphSpriteRendererNode>(node.payload).texturePath.empty()) errors.push_back(prefix + "SpriteRenderer requires texturePath");
		break;
	case VfxGraphNodeType::Burst:
	case VfxGraphNodeType::SpawnPoint:
	case VfxGraphNodeType::DeathEvent:
		break;
	default:
		errors.push_back(prefix + "unsupported node type");
		break;
	}
}

bool BuildExecutionOrder(const VfxGraphEmitterDesc& emitter, std::vector<uint32_t>& outOrder, std::vector<std::string>& errors)
{
	std::unordered_map<uint32_t, const VfxGraphNodeDesc*> nodesById;
	std::unordered_map<uint32_t, uint32_t> indegree;
	std::unordered_map<uint32_t, std::vector<uint32_t>> outgoing;
	const size_t errorStart = errors.size();

	for (const VfxGraphNodeDesc& node : emitter.nodes)
	{
		if (node.id == 0u) { errors.push_back("Emitter '" + emitter.name + "' has node id 0"); continue; }
		if (!nodesById.emplace(node.id, &node).second) { errors.push_back("Emitter '" + emitter.name + "' has duplicate node id " + std::to_string(node.id)); continue; }
		if (node.enabled) indegree[node.id] = 0u;
	}
	if (errors.size() != errorStart) return false;

	std::unordered_set<uint64_t> uniqueEdges;
	for (const VfxGraphEdgeDesc& edge : emitter.edges)
	{
		const auto fromIt = nodesById.find(edge.fromNodeId);
		const auto toIt = nodesById.find(edge.toNodeId);
		if (fromIt == nodesById.end() || toIt == nodesById.end()) { errors.push_back("Emitter '" + emitter.name + "' edge references missing node"); continue; }
		if (edge.fromNodeId == edge.toNodeId) { errors.push_back("Emitter '" + emitter.name + "' contains self edge"); continue; }
		const uint64_t edgeKey = (static_cast<uint64_t>(edge.fromNodeId) << 32u) | edge.toNodeId;
		if (!uniqueEdges.insert(edgeKey).second) { errors.push_back("Emitter '" + emitter.name + "' contains duplicate edge"); continue; }
		if (static_cast<uint32_t>(fromIt->second->stage) > static_cast<uint32_t>(toIt->second->stage)) { errors.push_back("Emitter '" + emitter.name + "' contains backward stage edge"); continue; }
		if (!fromIt->second->enabled || !toIt->second->enabled) continue;
		outgoing[edge.fromNodeId].push_back(edge.toNodeId);
		++indegree[edge.toNodeId];
	}
	if (errors.size() != errorStart) return false;

	std::vector<uint32_t> ready;
	for (const auto& [nodeId, degree] : indegree) if (degree == 0u) ready.push_back(nodeId);
	auto sortReady = [&]()
	{
		std::sort(ready.begin(), ready.end(), [&](uint32_t a, uint32_t b)
		{
			const VfxGraphNodeDesc* nodeA = nodesById.at(a);
			const VfxGraphNodeDesc* nodeB = nodesById.at(b);
			if (nodeA->stage != nodeB->stage) return static_cast<uint32_t>(nodeA->stage) < static_cast<uint32_t>(nodeB->stage);
			return a < b;
		});
	};
	sortReady();
	while (!ready.empty())
	{
		const uint32_t nodeId = ready.front();
		ready.erase(ready.begin());
		outOrder.push_back(nodeId);
		for (const uint32_t targetId : outgoing[nodeId])
		{
			uint32_t& degree = indegree[targetId];
			if (degree > 0u) --degree;
			if (degree == 0u) ready.push_back(targetId);
		}
		sortReady();
	}
	if (outOrder.size() != indegree.size())
	{
		errors.push_back("Emitter '" + emitter.name + "' contains a cycle");
		return false;
	}
	return true;
}

GpuParticleCollisionShape ToGpuCollisionShape(VfxCollisionShape shape)
{
	return shape == VfxCollisionShape::Sphere ? GpuParticleCollisionShape::Sphere : GpuParticleCollisionShape::Plane;
}

GpuParticleCollisionResponse ToGpuCollisionResponse(VfxCollisionResponse response)
{
	switch (response)
	{
	case VfxCollisionResponse::Slide: return GpuParticleCollisionResponse::Slide;
	case VfxCollisionResponse::Kill: return GpuParticleCollisionResponse::Kill;
	case VfxCollisionResponse::Bounce:
	default: return GpuParticleCollisionResponse::Bounce;
	}
}

uint32_t EventMask(VfxParticleEventType eventType)
{
	return eventType == VfxParticleEventType::Death ? kGpuParticleEventDeath : kGpuParticleEventCollision;
}

bool CompileEmitter(const VfxGraphEmitterDesc& source, const std::vector<GpuParticleUserParameterDesc>& userParameters,
	GpuParticleEmitterDesc& outEmitter, VfxGraphCompiledEmitter& outCompiled, std::vector<std::string>& errors, std::vector<std::string>& warnings)
{
	const size_t errorStart = errors.size();
	if (source.name.empty()) errors.push_back("Emitter name is empty");
	if (source.maxParticles == 0u) errors.push_back("Emitter '" + source.name + "' maxParticles must be > 0");
	if (!std::isfinite(source.duration) || source.duration < 0.0f) errors.push_back("Emitter '" + source.name + "' duration must be finite and >= 0");
	if (source.nodes.empty()) errors.push_back("Emitter '" + source.name + "' has no nodes");
	if (source.nodes.size() > VfxGraphDesc::kMaxNodesPerEmitter) errors.push_back("Emitter '" + source.name + "' exceeds kMaxNodesPerEmitter");
	if (source.edges.size() > VfxGraphDesc::kMaxEdgesPerEmitter) errors.push_back("Emitter '" + source.name + "' exceeds kMaxEdgesPerEmitter");

	std::unordered_set<std::string> parameterNames;
	for (const auto& parameter : userParameters) parameterNames.insert(parameter.name);
	for (const auto& binding : source.parameterBindings)
	{
		if (!parameterNames.contains(binding.parameterName)) errors.push_back("Emitter '" + source.name + "' binding references unknown parameter: " + binding.parameterName);
		if (!std::isfinite(binding.scale) || !std::isfinite(binding.bias)) errors.push_back("Emitter '" + source.name + "' binding scale/bias must be finite");
	}

	std::unordered_map<VfxGraphNodeType, uint32_t> typeCounts;
	uint32_t spawnShapeCount = 0u;
	uint32_t rendererCount = 0u;
	bool collisionEventProducer = false;
	bool deathEventProducer = false;
	const VfxGraphSubEmitterNode* subEmitterNode = nullptr;

	for (const VfxGraphNodeDesc& node : source.nodes)
	{
		if (node.stage != GetExpectedVfxGraphNodeStage(node.type)) errors.push_back("Emitter '" + source.name + "' node '" + node.name + "' is in the wrong stage");
		if (!PayloadMatchesType(node)) { errors.push_back("Emitter '" + source.name + "' node '" + node.name + "' payload does not match node type"); continue; }
		ValidateNodeValues(node, errors, source.name);
		if (!node.enabled) continue;
		++typeCounts[node.type];
		if (node.type == VfxGraphNodeType::SpawnPoint || node.type == VfxGraphNodeType::SpawnSphere || node.type == VfxGraphNodeType::SpawnBox) ++spawnShapeCount;
		if (node.type == VfxGraphNodeType::SpriteRenderer) ++rendererCount;
		if (node.type == VfxGraphNodeType::Collision && std::get<VfxGraphCollisionNode>(node.payload).generateEvent) collisionEventProducer = true;
		if (node.type == VfxGraphNodeType::DeathEvent) deathEventProducer = true;
		if (node.type == VfxGraphNodeType::SubEmitter) subEmitterNode = &std::get<VfxGraphSubEmitterNode>(node.payload);
	}
	for (const auto& [type, count] : typeCounts) if (count > 1u) errors.push_back("Emitter '" + source.name + "' has duplicate enabled node type: " + std::string(ToString(type)));
	if (spawnShapeCount > 1u) errors.push_back("Emitter '" + source.name + "' may enable only one spawn shape node");
	if (rendererCount != 1u) errors.push_back("Emitter '" + source.name + "' requires exactly one enabled SpriteRenderer node");
	if (subEmitterNode)
	{
		if (subEmitterNode->sourceEvent == VfxParticleEventType::Collision && !collisionEventProducer) errors.push_back("Emitter '" + source.name + "' SubEmitter requires a Collision node with generateEvent=true");
		if (subEmitterNode->sourceEvent == VfxParticleEventType::Death && !deathEventProducer) errors.push_back("Emitter '" + source.name + "' SubEmitter requires an enabled DeathEvent node");
	}
	if (errors.size() != errorStart) return false;

	if (!BuildExecutionOrder(source, outCompiled.executionOrder, errors)) return false;
	outCompiled.name = source.name;
	outEmitter = CreateDefaultSpriteEmitterDesc();
	outEmitter.name = source.name;
	outEmitter.maxParticles = source.maxParticles;
	outEmitter.loop = source.loop;
	outEmitter.duration = source.duration;
	outEmitter.spawnRate = 0.0f;
	outEmitter.burstCount = 0u;
	outEmitter.spawnShape = GpuParticleSpawnShape::Point;
	outEmitter.spawnRadius = 0.0f;
	outEmitter.spawnBoxSize = {};
	outEmitter.positionRandom = {};
	outEmitter.velocity = {};
	outEmitter.velocityRandom = {};
	outEmitter.speed = 0.0f;
	outEmitter.speedRandom = 0.0f;
	outEmitter.lifeTime = 1.0f;
	outEmitter.lifeTimeRandom = 0.0f;
	outEmitter.gravity = {};
	outEmitter.damping = 0.0f;
	outEmitter.startSize = { 0.1f, 0.1f };
	outEmitter.endSize = { 0.1f, 0.1f };
	outEmitter.sizeRandom = 0.0f;
	outEmitter.useSizeCurve = false;
	outEmitter.sizeCurveLut = { 1.0f, 1.0f, 1.0f, 1.0f };
	outEmitter.startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	outEmitter.endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
	outEmitter.alphaFade = true;
	outEmitter.useColorGradient = false;
	outEmitter.startRotation = 0.0f;
	outEmitter.rotationRandom = 0.0f;
	outEmitter.rotationSpeed = 0.0f;
	outEmitter.collisionShape = GpuParticleCollisionShape::None;
	outEmitter.eventMask = 0u;
	outEmitter.subEmitterEventMask = 0u;
	outEmitter.subEmitterCount = 0u;
	outEmitter.parameterBindings = source.parameterBindings;

	std::unordered_map<uint32_t, const VfxGraphNodeDesc*> enabledNodes;
	for (const auto& node : source.nodes) if (node.enabled) enabledNodes[node.id] = &node;
	for (const uint32_t nodeId : outCompiled.executionOrder)
	{
		const VfxGraphNodeDesc& node = *enabledNodes.at(nodeId);
		switch (node.type)
		{
		case VfxGraphNodeType::SpawnRate: outEmitter.spawnRate = std::get<VfxGraphSpawnRateNode>(node.payload).rate; break;
		case VfxGraphNodeType::Burst: outEmitter.burstCount = std::get<VfxGraphBurstNode>(node.payload).count; break;
		case VfxGraphNodeType::SpawnPoint: outEmitter.spawnShape = GpuParticleSpawnShape::Point; break;
		case VfxGraphNodeType::SpawnSphere: outEmitter.spawnShape = GpuParticleSpawnShape::Sphere; outEmitter.spawnRadius = std::get<VfxGraphSpawnSphereNode>(node.payload).radius; break;
		case VfxGraphNodeType::SpawnBox: outEmitter.spawnShape = GpuParticleSpawnShape::Box; outEmitter.spawnBoxSize = std::get<VfxGraphSpawnBoxNode>(node.payload).size; break;
		case VfxGraphNodeType::Lifetime:
		{
			const auto& p = std::get<VfxGraphLifetimeNode>(node.payload); outEmitter.lifeTime = p.lifetime; outEmitter.lifeTimeRandom = p.random; break;
		}
		case VfxGraphNodeType::InitialVelocity:
		{
			const auto& p = std::get<VfxGraphInitialVelocityNode>(node.payload); outEmitter.velocity = p.velocity; outEmitter.velocityRandom = p.random; outEmitter.speed = p.speed; outEmitter.speedRandom = p.speedRandom; break;
		}
		case VfxGraphNodeType::InitialColor:
		{
			const auto& p = std::get<VfxGraphInitialColorNode>(node.payload); outEmitter.startColor = p.start; outEmitter.endColor = p.end; outEmitter.alphaFade = p.alphaFade; break;
		}
		case VfxGraphNodeType::InitialSize:
		{
			const auto& p = std::get<VfxGraphInitialSizeNode>(node.payload); outEmitter.startSize = p.start; outEmitter.endSize = p.end; outEmitter.sizeRandom = p.random; break;
		}
		case VfxGraphNodeType::Gravity: outEmitter.gravity = std::get<VfxGraphGravityNode>(node.payload).acceleration; break;
		case VfxGraphNodeType::Drag: outEmitter.damping = std::get<VfxGraphDragNode>(node.payload).damping; break;
		case VfxGraphNodeType::InitialRotation:
		{
			const auto& p = std::get<VfxGraphInitialRotationNode>(node.payload); outEmitter.startRotation = p.rotation; outEmitter.rotationRandom = p.random; break;
		}
		case VfxGraphNodeType::RotationRate: outEmitter.rotationSpeed = std::get<VfxGraphRotationRateNode>(node.payload).radiansPerSecond; break;
		case VfxGraphNodeType::SizeOverLife:
		{
			const VfxFloatCurve& curve = std::get<VfxGraphSizeOverLifeNode>(node.payload).multiplier;
			outEmitter.useSizeCurve = true;
			// Authoring curves stay flexible while the GPU backend receives its fixed four-sample LUT.
			outEmitter.sizeCurveLut = { curve.Evaluate(0.0f), curve.Evaluate(1.0f / 3.0f), curve.Evaluate(2.0f / 3.0f), curve.Evaluate(1.0f) };
			break;
		}
		case VfxGraphNodeType::ColorOverLife:
		{
			const VfxColorGradient& gradient = std::get<VfxGraphColorOverLifeNode>(node.payload).gradient;
			outEmitter.useColorGradient = true;
			outEmitter.colorGradientLut = { gradient.Evaluate(0.0f), gradient.Evaluate(1.0f / 3.0f), gradient.Evaluate(2.0f / 3.0f), gradient.Evaluate(1.0f) };
			break;
		}
		case VfxGraphNodeType::Collision:
		{
			const auto& p = std::get<VfxGraphCollisionNode>(node.payload);
			outEmitter.collisionShape = ToGpuCollisionShape(p.shape);
			outEmitter.collisionResponse = ToGpuCollisionResponse(p.response);
			outEmitter.collisionPlaneNormal = p.planeNormal;
			outEmitter.collisionPlaneDistance = p.planeDistance;
			outEmitter.collisionSphereCenter = p.sphereCenter;
			outEmitter.collisionSphereRadius = p.sphereRadius;
			outEmitter.collisionParticleRadius = p.particleRadius;
			outEmitter.collisionRestitution = p.restitution;
			outEmitter.collisionFriction = p.friction;
			if (p.generateEvent) outEmitter.eventMask |= kGpuParticleEventCollision;
			break;
		}
		case VfxGraphNodeType::DeathEvent: outEmitter.eventMask |= kGpuParticleEventDeath; break;
		case VfxGraphNodeType::SubEmitter:
		{
			const auto& p = std::get<VfxGraphSubEmitterNode>(node.payload);
			outEmitter.subEmitterEventMask = EventMask(p.sourceEvent);
			outEmitter.subEmitterCount = p.count;
			outEmitter.subEmitterLifeTime = p.lifeTime;
			outEmitter.subEmitterSpeed = p.speed;
			outEmitter.subEmitterSpread = p.spread;
			outEmitter.subEmitterInheritVelocity = p.inheritVelocity;
			outEmitter.subEmitterStartSize = p.startSize;
			outEmitter.subEmitterEndSize = p.endSize;
			outEmitter.subEmitterStartColor = p.startColor;
			outEmitter.subEmitterEndColor = p.endColor;
			outEmitter.subEmitterAlphaFade = p.alphaFade;
			break;
		}
		case VfxGraphNodeType::SpriteRenderer:
		{
			const auto& p = std::get<VfxGraphSpriteRendererNode>(node.payload); outEmitter.renderType = GpuParticleRenderType::Sprite; outEmitter.texturePath = p.texturePath; outEmitter.blendMode = p.blendMode; outEmitter.billboard = p.billboard; break;
		}
		default: break;
		}
	}

	if (outEmitter.spawnRate <= 0.0f && outEmitter.burstCount == 0u) warnings.push_back("Emitter '" + source.name + "' has no active SpawnRate or Burst output");
	return true;
}
}

VfxGraphCompileResult VfxGraphCompiler::Compile(const VfxGraphDesc& graph)
{
	VfxGraphCompileResult result{};
	result.program.graphName = graph.graphName;
	result.program.particleEffect.effectName = graph.graphName;
	result.program.particleEffect.userParameters = graph.userParameters;
	if (graph.schemaVersion != VfxGraphDesc::kSchemaVersion) result.errors.push_back("Unsupported VFX Graph schemaVersion");
	if (graph.graphName.empty() || graph.graphName.size() > 96u) result.errors.push_back("graphName must contain 1-96 characters");
	if (graph.emitters.empty()) result.errors.push_back("VFX Graph must contain at least one emitter");
	if (graph.emitters.size() > VfxGraphDesc::kMaxEmitters) result.errors.push_back("VFX Graph exceeds kMaxEmitters");

	std::unordered_set<std::string> parameterNames;
	for (const auto& parameter : graph.userParameters)
	{
		if (parameter.name.empty() || !parameterNames.insert(parameter.name).second) result.errors.push_back("User parameter names must be non-empty and unique");
		if (!std::isfinite(parameter.defaultValue) || !std::isfinite(parameter.minValue) || !std::isfinite(parameter.maxValue) || parameter.minValue > parameter.maxValue || parameter.defaultValue < parameter.minValue || parameter.defaultValue > parameter.maxValue)
			result.errors.push_back("User parameter range/default is invalid: " + parameter.name);
	}

	std::unordered_set<std::string> emitterNames;
	for (const auto& emitter : graph.emitters)
	{
		if (!emitterNames.insert(emitter.name).second) result.errors.push_back("Duplicate emitter name: " + emitter.name);
		GpuParticleEmitterDesc compiledEmitter{};
		VfxGraphCompiledEmitter compiledMetadata{};
		const size_t errorCountBefore = result.errors.size();
		CompileEmitter(emitter, graph.userParameters, compiledEmitter, compiledMetadata, result.errors, result.warnings);
		if (result.errors.size() == errorCountBefore)
		{
			result.program.particleEffect.emitters.push_back(std::move(compiledEmitter));
			result.program.emitters.push_back(std::move(compiledMetadata));
		}
	}
	result.success = result.errors.empty();
	if (!result.success)
	{
		result.program.particleEffect.emitters.clear();
		result.program.emitters.clear();
	}
	return result;
}

} // namespace Ken4lowEngine
