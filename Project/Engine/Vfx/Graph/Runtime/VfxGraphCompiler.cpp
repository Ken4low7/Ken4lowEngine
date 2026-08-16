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
	bool IsFinite(const Vector2& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y);
	}

	bool IsFinite(const Vector3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	bool IsFinite(const Vector4& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
	}

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
			if (!std::isfinite(key.time) || !std::isfinite(key.value))
			{
				errors.push_back(prefix + "curve keys must be finite");
				continue;
			}
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
			if (!std::isfinite(key.time) || !IsFinite(key.color))
			{
				errors.push_back(prefix + "gradient keys must be finite");
				continue;
			}
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
			const auto& payload = std::get<VfxGraphSpawnRateNode>(node.payload);
			if (!std::isfinite(payload.rate) || payload.rate < 0.0f) errors.push_back(prefix + "spawn rate must be finite and >= 0");
			break;
		}
		case VfxGraphNodeType::SpawnSphere:
		{
			const auto& payload = std::get<VfxGraphSpawnSphereNode>(node.payload);
			if (!std::isfinite(payload.radius) || payload.radius < 0.0f) errors.push_back(prefix + "sphere radius must be finite and >= 0");
			break;
		}
		case VfxGraphNodeType::SpawnBox:
		{
			const auto& payload = std::get<VfxGraphSpawnBoxNode>(node.payload);
			if (!IsFinite(payload.size) || payload.size.x < 0.0f || payload.size.y < 0.0f || payload.size.z < 0.0f)
			{
				errors.push_back(prefix + "box size must be finite and non-negative");
			}
			break;
		}
		case VfxGraphNodeType::Lifetime:
		{
			const auto& payload = std::get<VfxGraphLifetimeNode>(node.payload);
			if (!std::isfinite(payload.lifetime) || payload.lifetime <= 0.0f || !std::isfinite(payload.random) || payload.random < 0.0f)
			{
				errors.push_back(prefix + "lifetime must be > 0 and random must be >= 0");
			}
			break;
		}
		case VfxGraphNodeType::InitialVelocity:
		{
			const auto& payload = std::get<VfxGraphInitialVelocityNode>(node.payload);
			if (!IsFinite(payload.velocity) || !IsFinite(payload.random) || !std::isfinite(payload.speed) || !std::isfinite(payload.speedRandom) ||
				payload.speed < 0.0f || payload.speedRandom < 0.0f)
			{
				errors.push_back(prefix + "velocity values must be finite and speed ranges non-negative");
			}
			break;
		}
		case VfxGraphNodeType::InitialColor:
		{
			const auto& payload = std::get<VfxGraphInitialColorNode>(node.payload);
			if (!IsFinite(payload.start) || !IsFinite(payload.end)) errors.push_back(prefix + "color values must be finite");
			break;
		}
		case VfxGraphNodeType::InitialSize:
		{
			const auto& payload = std::get<VfxGraphInitialSizeNode>(node.payload);
			if (!IsFinite(payload.start) || !IsFinite(payload.end) || !std::isfinite(payload.random) || payload.random < 0.0f ||
				payload.start.x < 0.0f || payload.start.y < 0.0f || payload.end.x < 0.0f || payload.end.y < 0.0f)
			{
				errors.push_back(prefix + "size values must be finite and non-negative");
			}
			break;
		}
		case VfxGraphNodeType::Gravity:
		{
			if (!IsFinite(std::get<VfxGraphGravityNode>(node.payload).acceleration)) errors.push_back(prefix + "gravity must be finite");
			break;
		}
		case VfxGraphNodeType::Drag:
		{
			const float damping = std::get<VfxGraphDragNode>(node.payload).damping;
			if (!std::isfinite(damping) || damping < 0.0f) errors.push_back(prefix + "drag damping must be finite and >= 0");
			break;
		}
		case VfxGraphNodeType::InitialRotation:
		{
			const auto& payload = std::get<VfxGraphInitialRotationNode>(node.payload);
			if (!std::isfinite(payload.rotation) || !std::isfinite(payload.random) || payload.random < 0.0f)
			{
				errors.push_back(prefix + "rotation must be finite and random must be >= 0");
			}
			break;
		}
		case VfxGraphNodeType::RotationRate:
		{
			if (!std::isfinite(std::get<VfxGraphRotationRateNode>(node.payload).radiansPerSecond)) errors.push_back(prefix + "rotation rate must be finite");
			break;
		}
		case VfxGraphNodeType::SizeOverLife:
			ValidateFloatCurve(std::get<VfxGraphSizeOverLifeNode>(node.payload).multiplier, errors, prefix);
			break;
		case VfxGraphNodeType::ColorOverLife:
			ValidateColorGradient(std::get<VfxGraphColorOverLifeNode>(node.payload).gradient, errors, prefix);
			break;
		case VfxGraphNodeType::SpriteRenderer:
		{
			const auto& payload = std::get<VfxGraphSpriteRendererNode>(node.payload);
			if (payload.texturePath.empty()) errors.push_back(prefix + "SpriteRenderer requires texturePath");
			break;
		}
		case VfxGraphNodeType::Burst:
		case VfxGraphNodeType::SpawnPoint:
			break;
		default:
			errors.push_back(prefix + "unsupported node type");
			break;
		}
	}

	bool BuildExecutionOrder(
		const VfxGraphEmitterDesc& emitter,
		std::vector<uint32_t>& outOrder,
		std::vector<std::string>& errors)
	{
		std::unordered_map<uint32_t, const VfxGraphNodeDesc*> nodesById;
		std::unordered_map<uint32_t, uint32_t> indegree;
		std::unordered_map<uint32_t, std::vector<uint32_t>> outgoing;

		for (const VfxGraphNodeDesc& node : emitter.nodes)
		{
			if (node.id == 0u)
			{
				errors.push_back("Emitter '" + emitter.name + "' has node id 0");
				continue;
			}
			if (!nodesById.emplace(node.id, &node).second)
			{
				errors.push_back("Emitter '" + emitter.name + "' has duplicate node id " + std::to_string(node.id));
				continue;
			}
			if (node.enabled) indegree[node.id] = 0u;
		}
		if (!errors.empty()) return false;

		std::unordered_set<uint64_t> uniqueEdges;
		for (const VfxGraphEdgeDesc& edge : emitter.edges)
		{
			const auto fromIt = nodesById.find(edge.fromNodeId);
			const auto toIt = nodesById.find(edge.toNodeId);
			if (fromIt == nodesById.end() || toIt == nodesById.end())
			{
				errors.push_back("Emitter '" + emitter.name + "' edge references missing node");
				continue;
			}
			if (edge.fromNodeId == edge.toNodeId)
			{
				errors.push_back("Emitter '" + emitter.name + "' contains self edge");
				continue;
			}
			const uint64_t edgeKey = (static_cast<uint64_t>(edge.fromNodeId) << 32u) | edge.toNodeId;
			if (!uniqueEdges.insert(edgeKey).second)
			{
				errors.push_back("Emitter '" + emitter.name + "' contains duplicate edge");
				continue;
			}
			if (static_cast<uint32_t>(fromIt->second->stage) > static_cast<uint32_t>(toIt->second->stage))
			{
				errors.push_back("Emitter '" + emitter.name + "' contains backward stage edge");
				continue;
			}
			if (!fromIt->second->enabled || !toIt->second->enabled) continue;
			outgoing[edge.fromNodeId].push_back(edge.toNodeId);
			++indegree[edge.toNodeId];
		}
		if (!errors.empty()) return false;

		std::vector<uint32_t> ready;
		for (const auto& [nodeId, degree] : indegree)
		{
			if (degree == 0u) ready.push_back(nodeId);
		}
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

	bool CompileEmitter(
		const VfxGraphEmitterDesc& source,
		const std::vector<GpuParticleUserParameterDesc>& userParameters,
		GpuParticleEmitterDesc& outEmitter,
		VfxGraphCompiledEmitter& outCompiled,
		std::vector<std::string>& errors,
		std::vector<std::string>& warnings)
	{
		if (source.name.empty()) errors.push_back("Emitter name is empty");
		if (source.maxParticles == 0u) errors.push_back("Emitter '" + source.name + "' maxParticles must be > 0");
		if (!std::isfinite(source.duration) || source.duration < 0.0f) errors.push_back("Emitter '" + source.name + "' duration must be finite and >= 0");
		if (source.nodes.empty()) errors.push_back("Emitter '" + source.name + "' has no nodes");
		if (source.nodes.size() > VfxGraphDesc::kMaxNodesPerEmitter) errors.push_back("Emitter '" + source.name + "' exceeds kMaxNodesPerEmitter");
		if (source.edges.size() > VfxGraphDesc::kMaxEdgesPerEmitter) errors.push_back("Emitter '" + source.name + "' exceeds kMaxEdgesPerEmitter");

		std::unordered_set<std::string> parameterNames;
		for (const GpuParticleUserParameterDesc& parameter : userParameters) parameterNames.insert(parameter.name);
		for (const GpuParticleParameterBindingDesc& binding : source.parameterBindings)
		{
			if (!parameterNames.contains(binding.parameterName)) errors.push_back("Emitter '" + source.name + "' binding references unknown parameter: " + binding.parameterName);
			if (!std::isfinite(binding.scale) || !std::isfinite(binding.bias)) errors.push_back("Emitter '" + source.name + "' binding scale/bias must be finite");
		}

		std::unordered_map<VfxGraphNodeType, uint32_t> typeCounts;
		uint32_t spawnShapeCount = 0u;
		uint32_t rendererCount = 0u;
		for (const VfxGraphNodeDesc& node : source.nodes)
		{
			if (node.stage != GetExpectedVfxGraphNodeStage(node.type)) errors.push_back("Emitter '" + source.name + "' node '" + node.name + "' is in the wrong stage");
			if (!PayloadMatchesType(node))
			{
				errors.push_back("Emitter '" + source.name + "' node '" + node.name + "' payload does not match node type");
				continue;
			}
			ValidateNodeValues(node, errors, source.name);
			if (!node.enabled) continue;
			++typeCounts[node.type];
			if (node.type == VfxGraphNodeType::SpawnPoint || node.type == VfxGraphNodeType::SpawnSphere || node.type == VfxGraphNodeType::SpawnBox) ++spawnShapeCount;
			if (node.type == VfxGraphNodeType::SpriteRenderer) ++rendererCount;
		}

		for (const auto& [type, count] : typeCounts)
		{
			if (count > 1u) errors.push_back("Emitter '" + source.name + "' has duplicate enabled node type: " + std::string(ToString(type)));
		}
		if (spawnShapeCount > 1u) errors.push_back("Emitter '" + source.name + "' may enable only one spawn shape node");
		if (rendererCount != 1u) errors.push_back("Emitter '" + source.name + "' requires exactly one enabled SpriteRenderer node");
		if (!errors.empty()) return false;

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
		outEmitter.spawnBoxSize = { 0.0f, 0.0f, 0.0f };
		outEmitter.positionRandom = { 0.0f, 0.0f, 0.0f };
		outEmitter.velocity = { 0.0f, 0.0f, 0.0f };
		outEmitter.velocityRandom = { 0.0f, 0.0f, 0.0f };
		outEmitter.speed = 0.0f;
		outEmitter.speedRandom = 0.0f;
		outEmitter.lifeTime = 1.0f;
		outEmitter.lifeTimeRandom = 0.0f;
		outEmitter.gravity = { 0.0f, 0.0f, 0.0f };
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
		outEmitter.parameterBindings = source.parameterBindings;

		std::unordered_map<uint32_t, const VfxGraphNodeDesc*> enabledNodes;
		for (const VfxGraphNodeDesc& node : source.nodes)
		{
			if (node.enabled) enabledNodes[node.id] = &node;
		}
		for (const uint32_t nodeId : outCompiled.executionOrder)
		{
			const VfxGraphNodeDesc& node = *enabledNodes.at(nodeId);
			switch (node.type)
			{
			case VfxGraphNodeType::SpawnRate:
				outEmitter.spawnRate = std::get<VfxGraphSpawnRateNode>(node.payload).rate;
				break;
			case VfxGraphNodeType::Burst:
				outEmitter.burstCount = std::get<VfxGraphBurstNode>(node.payload).count;
				break;
			case VfxGraphNodeType::SpawnPoint:
				outEmitter.spawnShape = GpuParticleSpawnShape::Point;
				break;
			case VfxGraphNodeType::SpawnSphere:
				outEmitter.spawnShape = GpuParticleSpawnShape::Sphere;
				outEmitter.spawnRadius = std::get<VfxGraphSpawnSphereNode>(node.payload).radius;
				break;
			case VfxGraphNodeType::SpawnBox:
				outEmitter.spawnShape = GpuParticleSpawnShape::Box;
				outEmitter.spawnBoxSize = std::get<VfxGraphSpawnBoxNode>(node.payload).size;
				break;
			case VfxGraphNodeType::Lifetime:
			{
				const auto& payload = std::get<VfxGraphLifetimeNode>(node.payload);
				outEmitter.lifeTime = payload.lifetime;
				outEmitter.lifeTimeRandom = payload.random;
				break;
			}
			case VfxGraphNodeType::InitialVelocity:
			{
				const auto& payload = std::get<VfxGraphInitialVelocityNode>(node.payload);
				outEmitter.velocity = payload.velocity;
				outEmitter.velocityRandom = payload.random;
				outEmitter.speed = payload.speed;
				outEmitter.speedRandom = payload.speedRandom;
				break;
			}
			case VfxGraphNodeType::InitialColor:
			{
				const auto& payload = std::get<VfxGraphInitialColorNode>(node.payload);
				outEmitter.startColor = payload.start;
				outEmitter.endColor = payload.end;
				outEmitter.alphaFade = payload.alphaFade;
				break;
			}
			case VfxGraphNodeType::InitialSize:
			{
				const auto& payload = std::get<VfxGraphInitialSizeNode>(node.payload);
				outEmitter.startSize = payload.start;
				outEmitter.endSize = payload.end;
				outEmitter.sizeRandom = payload.random;
				break;
			}
			case VfxGraphNodeType::Gravity:
				outEmitter.gravity = std::get<VfxGraphGravityNode>(node.payload).acceleration;
				break;
			case VfxGraphNodeType::Drag:
				outEmitter.damping = std::get<VfxGraphDragNode>(node.payload).damping;
				break;
			case VfxGraphNodeType::InitialRotation:
			{
				const auto& payload = std::get<VfxGraphInitialRotationNode>(node.payload);
				outEmitter.startRotation = payload.rotation;
				outEmitter.rotationRandom = payload.random;
				break;
			}
			case VfxGraphNodeType::RotationRate:
				outEmitter.rotationSpeed = std::get<VfxGraphRotationRateNode>(node.payload).radiansPerSecond;
				break;
			case VfxGraphNodeType::SizeOverLife:
			{
				const VfxFloatCurve& curve = std::get<VfxGraphSizeOverLifeNode>(node.payload).multiplier;
				outEmitter.useSizeCurve = true;
				// Authoring curves stay flexible while the GPU backend receives its fixed four-sample LUT.
				outEmitter.sizeCurveLut = {
					curve.Evaluate(0.0f),
					curve.Evaluate(1.0f / 3.0f),
					curve.Evaluate(2.0f / 3.0f),
					curve.Evaluate(1.0f),
				};
				break;
			}
			case VfxGraphNodeType::ColorOverLife:
			{
				const VfxColorGradient& gradient = std::get<VfxGraphColorOverLifeNode>(node.payload).gradient;
				outEmitter.useColorGradient = true;
				outEmitter.colorGradientLut = {
					gradient.Evaluate(0.0f),
					gradient.Evaluate(1.0f / 3.0f),
					gradient.Evaluate(2.0f / 3.0f),
					gradient.Evaluate(1.0f),
				};
				break;
			}
			case VfxGraphNodeType::SpriteRenderer:
			{
				const auto& payload = std::get<VfxGraphSpriteRendererNode>(node.payload);
				outEmitter.renderType = GpuParticleRenderType::Sprite;
				outEmitter.texturePath = payload.texturePath;
				outEmitter.blendMode = payload.blendMode;
				outEmitter.billboard = payload.billboard;
				break;
			}
			default:
				break;
			}
		}

		if (outEmitter.spawnRate <= 0.0f && outEmitter.burstCount == 0u)
		{
			warnings.push_back("Emitter '" + source.name + "' has no active SpawnRate or Burst output");
		}
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
	for (const GpuParticleUserParameterDesc& parameter : graph.userParameters)
	{
		if (parameter.name.empty() || !parameterNames.insert(parameter.name).second) result.errors.push_back("User parameter names must be non-empty and unique");
		if (!std::isfinite(parameter.defaultValue) || !std::isfinite(parameter.minValue) || !std::isfinite(parameter.maxValue) ||
			parameter.minValue > parameter.maxValue || parameter.defaultValue < parameter.minValue || parameter.defaultValue > parameter.maxValue)
		{
			result.errors.push_back("User parameter range/default is invalid: " + parameter.name);
		}
	}

	std::unordered_set<std::string> emitterNames;
	for (const VfxGraphEmitterDesc& emitter : graph.emitters)
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
