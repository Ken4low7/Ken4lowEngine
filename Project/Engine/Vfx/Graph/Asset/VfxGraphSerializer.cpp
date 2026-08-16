#include "VfxGraphSerializer.h"

#include "JsonFileIO.h"

#include <json.hpp>

#include <array>
#include <type_traits>
#include <utility>

namespace Ken4lowEngine
{
namespace
{
	using json = nlohmann::json;

	bool ReadVector2(const json& value, Vector2& outValue)
	{
		if (!value.is_array() || value.size() != 2u) return false;
		if (!value[0].is_number() || !value[1].is_number()) return false;
		outValue = { value[0].get<float>(), value[1].get<float>() };
		return true;
	}

	bool ReadVector3(const json& value, Vector3& outValue)
	{
		if (!value.is_array() || value.size() != 3u) return false;
		for (size_t i = 0; i < 3u; ++i)
		{
			if (!value[i].is_number()) return false;
		}
		outValue = { value[0].get<float>(), value[1].get<float>(), value[2].get<float>() };
		return true;
	}

	bool ReadVector4(const json& value, Vector4& outValue)
	{
		if (!value.is_array() || value.size() != 4u) return false;
		for (size_t i = 0; i < 4u; ++i)
		{
			if (!value[i].is_number()) return false;
		}
		outValue = { value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>() };
		return true;
	}

	json WriteVector2(const Vector2& value)
	{
		return json::array({ value.x, value.y });
	}

	json WriteVector3(const Vector3& value)
	{
		return json::array({ value.x, value.y, value.z });
	}

	json WriteVector4(const Vector4& value)
	{
		return json::array({ value.x, value.y, value.z, value.w });
	}

	const char* CurveInterpolationToString(VfxCurveInterpolation interpolation)
	{
		switch (interpolation)
		{
		case VfxCurveInterpolation::Linear: return "Linear";
		case VfxCurveInterpolation::Step: return "Step";
		case VfxCurveInterpolation::SmoothStep: return "SmoothStep";
		default: return "Linear";
		}
	}

	bool TryParseCurveInterpolation(const std::string& text, VfxCurveInterpolation& outInterpolation)
	{
		if (text == "Linear") outInterpolation = VfxCurveInterpolation::Linear;
		else if (text == "Step") outInterpolation = VfxCurveInterpolation::Step;
		else if (text == "SmoothStep") outInterpolation = VfxCurveInterpolation::SmoothStep;
		else return false;
		return true;
	}

	bool ReadFloatCurve(const json& value, VfxFloatCurve& outCurve)
	{
		if (!value.is_object()) return false;
		const std::string interpolationText = value.value("interpolation", std::string("Linear"));
		if (!TryParseCurveInterpolation(interpolationText, outCurve.interpolation)) return false;

		const auto keysIt = value.find("keys");
		if (keysIt == value.end() || !keysIt->is_array()) return false;
		outCurve.keys.clear();
		for (const json& source : *keysIt)
		{
			if (!source.is_object()) return false;
			const auto timeIt = source.find("time");
			const auto valueIt = source.find("value");
			if (timeIt == source.end() || valueIt == source.end() || !timeIt->is_number() || !valueIt->is_number()) return false;
			outCurve.keys.push_back({ timeIt->get<float>(), valueIt->get<float>() });
		}
		return true;
	}

	json WriteFloatCurve(const VfxFloatCurve& curve)
	{
		json result;
		result["interpolation"] = CurveInterpolationToString(curve.interpolation);
		result["keys"] = json::array();
		for (const VfxFloatCurveKey& key : curve.keys)
		{
			result["keys"].push_back({ { "time", key.time }, { "value", key.value } });
		}
		return result;
	}

	bool ReadColorGradient(const json& value, VfxColorGradient& outGradient)
	{
		if (!value.is_object()) return false;
		const std::string interpolationText = value.value("interpolation", std::string("Linear"));
		if (!TryParseCurveInterpolation(interpolationText, outGradient.interpolation)) return false;

		const auto keysIt = value.find("keys");
		if (keysIt == value.end() || !keysIt->is_array()) return false;
		outGradient.keys.clear();
		for (const json& source : *keysIt)
		{
			if (!source.is_object()) return false;
			const auto timeIt = source.find("time");
			const auto colorIt = source.find("color");
			if (timeIt == source.end() || !timeIt->is_number() || colorIt == source.end()) return false;
			VfxColorGradientKey key{};
			key.time = timeIt->get<float>();
			if (!ReadVector4(*colorIt, key.color)) return false;
			outGradient.keys.push_back(key);
		}
		return true;
	}

	json WriteColorGradient(const VfxColorGradient& gradient)
	{
		json result;
		result["interpolation"] = CurveInterpolationToString(gradient.interpolation);
		result["keys"] = json::array();
		for (const VfxColorGradientKey& key : gradient.keys)
		{
			result["keys"].push_back({ { "time", key.time }, { "color", WriteVector4(key.color) } });
		}
		return result;
	}

	const char* BlendModeToString(GpuParticleBlendMode blendMode)
	{
		switch (blendMode)
		{
		case GpuParticleBlendMode::Alpha: return "Alpha";
		case GpuParticleBlendMode::Additive: return "Additive";
		case GpuParticleBlendMode::Multiply: return "Multiply";
		default: return "Additive";
		}
	}

	bool TryParseBlendMode(const std::string& text, GpuParticleBlendMode& outBlendMode)
	{
		if (text == "Alpha") outBlendMode = GpuParticleBlendMode::Alpha;
		else if (text == "Additive") outBlendMode = GpuParticleBlendMode::Additive;
		else if (text == "Multiply") outBlendMode = GpuParticleBlendMode::Multiply;
		else return false;
		return true;
	}

	const char* ParameterTargetToString(GpuParticleParameterTarget target)
	{
		switch (target)
		{
		case GpuParticleParameterTarget::SpawnRate: return "SpawnRate";
		case GpuParticleParameterTarget::BurstCount: return "BurstCount";
		case GpuParticleParameterTarget::LifeTime: return "LifeTime";
		case GpuParticleParameterTarget::Speed: return "Speed";
		case GpuParticleParameterTarget::Size: return "Size";
		case GpuParticleParameterTarget::Alpha: return "Alpha";
		case GpuParticleParameterTarget::Force: return "Force";
		default: return "Speed";
		}
	}

	bool TryParseParameterTarget(const std::string& text, GpuParticleParameterTarget& outTarget)
	{
		if (text == "SpawnRate") outTarget = GpuParticleParameterTarget::SpawnRate;
		else if (text == "BurstCount") outTarget = GpuParticleParameterTarget::BurstCount;
		else if (text == "LifeTime") outTarget = GpuParticleParameterTarget::LifeTime;
		else if (text == "Speed") outTarget = GpuParticleParameterTarget::Speed;
		else if (text == "Size") outTarget = GpuParticleParameterTarget::Size;
		else if (text == "Alpha") outTarget = GpuParticleParameterTarget::Alpha;
		else if (text == "Force") outTarget = GpuParticleParameterTarget::Force;
		else return false;
		return true;
	}

	bool ReadNodePayload(VfxGraphNodeDesc& node, const json& params)
	{
		if (!params.is_object()) return false;
		switch (node.type)
		{
		case VfxGraphNodeType::SpawnRate:
		{
			VfxGraphSpawnRateNode payload{};
			payload.rate = params.value("rate", payload.rate);
			node.payload = payload;
			return true;
		}
		case VfxGraphNodeType::Burst:
		{
			VfxGraphBurstNode payload{};
			payload.count = params.value("count", payload.count);
			node.payload = payload;
			return true;
		}
		case VfxGraphNodeType::SpawnPoint:
			node.payload = VfxGraphSpawnPointNode{};
			return true;
		case VfxGraphNodeType::SpawnSphere:
		{
			VfxGraphSpawnSphereNode payload{};
			payload.radius = params.value("radius", payload.radius);
			node.payload = payload;
			return true;
		}
		case VfxGraphNodeType::SpawnBox:
		{
			VfxGraphSpawnBoxNode payload{};
			const auto it = params.find("size");
			if (it != params.end() && !ReadVector3(*it, payload.size)) return false;
			node.payload = payload;
			return true;
		}
		case VfxGraphNodeType::Lifetime:
		{
			VfxGraphLifetimeNode payload{};
			payload.lifetime = params.value("lifetime", payload.lifetime);
			payload.random = params.value("random", payload.random);
			node.payload = payload;
			return true;
		}
		case VfxGraphNodeType::InitialVelocity:
		{
			VfxGraphInitialVelocityNode payload{};
			const auto velocityIt = params.find("velocity");
			const auto randomIt = params.find("random");
			if (velocityIt != params.end() && !ReadVector3(*velocityIt, payload.velocity)) return false;
			if (randomIt != params.end() && !ReadVector3(*randomIt, payload.random)) return false;
			payload.speed = params.value("speed", payload.speed);
			payload.speedRandom = params.value("speedRandom", payload.speedRandom);
			node.payload = payload;
			return true;
		}
		case VfxGraphNodeType::InitialColor:
		{
			VfxGraphInitialColorNode payload{};
			const auto startIt = params.find("start");
			const auto endIt = params.find("end");
			if (startIt != params.end() && !ReadVector4(*startIt, payload.start)) return false;
			if (endIt != params.end() && !ReadVector4(*endIt, payload.end)) return false;
			payload.alphaFade = params.value("alphaFade", payload.alphaFade);
			node.payload = payload;
			return true;
		}
		case VfxGraphNodeType::InitialSize:
		{
			VfxGraphInitialSizeNode payload{};
			const auto startIt = params.find("start");
			const auto endIt = params.find("end");
			if (startIt != params.end() && !ReadVector2(*startIt, payload.start)) return false;
			if (endIt != params.end() && !ReadVector2(*endIt, payload.end)) return false;
			payload.random = params.value("random", payload.random);
			node.payload = payload;
			return true;
		}
		case VfxGraphNodeType::Gravity:
		{
			VfxGraphGravityNode payload{};
			const auto it = params.find("acceleration");
			if (it != params.end() && !ReadVector3(*it, payload.acceleration)) return false;
			node.payload = payload;
			return true;
		}
		case VfxGraphNodeType::Drag:
		{
			VfxGraphDragNode payload{};
			payload.damping = params.value("damping", payload.damping);
			node.payload = payload;
			return true;
		}
		case VfxGraphNodeType::InitialRotation:
		{
			VfxGraphInitialRotationNode payload{};
			payload.rotation = params.value("rotation", payload.rotation);
			payload.random = params.value("random", payload.random);
			node.payload = payload;
			return true;
		}
		case VfxGraphNodeType::RotationRate:
		{
			VfxGraphRotationRateNode payload{};
			payload.radiansPerSecond = params.value("radiansPerSecond", payload.radiansPerSecond);
			node.payload = payload;
			return true;
		}
		case VfxGraphNodeType::SizeOverLife:
		{
			VfxGraphSizeOverLifeNode payload{};
			const auto curveIt = params.find("curve");
			if (curveIt == params.end() || !ReadFloatCurve(*curveIt, payload.multiplier)) return false;
			node.payload = std::move(payload);
			return true;
		}
		case VfxGraphNodeType::ColorOverLife:
		{
			VfxGraphColorOverLifeNode payload{};
			const auto gradientIt = params.find("gradient");
			if (gradientIt == params.end() || !ReadColorGradient(*gradientIt, payload.gradient)) return false;
			node.payload = std::move(payload);
			return true;
		}
		case VfxGraphNodeType::SpriteRenderer:
		{
			VfxGraphSpriteRendererNode payload{};
			payload.texturePath = params.value("texturePath", payload.texturePath);
			payload.billboard = params.value("billboard", payload.billboard);
			const std::string blendText = params.value("blendMode", std::string("Additive"));
			if (!TryParseBlendMode(blendText, payload.blendMode)) return false;
			node.payload = payload;
			return true;
		}
		default:
			return false;
		}
	}

	json WriteNodePayload(const VfxGraphNodeDesc& node)
	{
		json params = json::object();
		std::visit([&params](const auto& payload)
		{
			using T = std::decay_t<decltype(payload)>;
			if constexpr (std::is_same_v<T, VfxGraphSpawnRateNode>) params["rate"] = payload.rate;
			else if constexpr (std::is_same_v<T, VfxGraphBurstNode>) params["count"] = payload.count;
			else if constexpr (std::is_same_v<T, VfxGraphSpawnSphereNode>) params["radius"] = payload.radius;
			else if constexpr (std::is_same_v<T, VfxGraphSpawnBoxNode>) params["size"] = WriteVector3(payload.size);
			else if constexpr (std::is_same_v<T, VfxGraphLifetimeNode>)
			{
				params["lifetime"] = payload.lifetime;
				params["random"] = payload.random;
			}
			else if constexpr (std::is_same_v<T, VfxGraphInitialVelocityNode>)
			{
				params["velocity"] = WriteVector3(payload.velocity);
				params["random"] = WriteVector3(payload.random);
				params["speed"] = payload.speed;
				params["speedRandom"] = payload.speedRandom;
			}
			else if constexpr (std::is_same_v<T, VfxGraphInitialColorNode>)
			{
				params["start"] = WriteVector4(payload.start);
				params["end"] = WriteVector4(payload.end);
				params["alphaFade"] = payload.alphaFade;
			}
			else if constexpr (std::is_same_v<T, VfxGraphInitialSizeNode>)
			{
				params["start"] = WriteVector2(payload.start);
				params["end"] = WriteVector2(payload.end);
				params["random"] = payload.random;
			}
			else if constexpr (std::is_same_v<T, VfxGraphGravityNode>) params["acceleration"] = WriteVector3(payload.acceleration);
			else if constexpr (std::is_same_v<T, VfxGraphDragNode>) params["damping"] = payload.damping;
			else if constexpr (std::is_same_v<T, VfxGraphInitialRotationNode>)
			{
				params["rotation"] = payload.rotation;
				params["random"] = payload.random;
			}
			else if constexpr (std::is_same_v<T, VfxGraphRotationRateNode>) params["radiansPerSecond"] = payload.radiansPerSecond;
			else if constexpr (std::is_same_v<T, VfxGraphSizeOverLifeNode>) params["curve"] = WriteFloatCurve(payload.multiplier);
			else if constexpr (std::is_same_v<T, VfxGraphColorOverLifeNode>) params["gradient"] = WriteColorGradient(payload.gradient);
			else if constexpr (std::is_same_v<T, VfxGraphSpriteRendererNode>)
			{
				params["texturePath"] = payload.texturePath;
				params["blendMode"] = BlendModeToString(payload.blendMode);
				params["billboard"] = payload.billboard;
			}
		}, node.payload);
		return params;
	}
}

bool VfxGraphSerializer::Load(VfxGraphDesc& outGraph, const std::string& filePath)
{
	json root;
	if (!JsonFileIO::LoadJsonFile(filePath, root) || !root.is_object()) return false;

	VfxGraphDesc graph{};
	graph.schemaVersion = root.value("schemaVersion", 0u);
	if (graph.schemaVersion != VfxGraphDesc::kSchemaVersion) return false;
	graph.graphName = root.value("graphName", std::string{});
	if (graph.graphName.empty()) return false;

	const auto parameterIt = root.find("userParameters");
	if (parameterIt != root.end())
	{
		if (!parameterIt->is_array()) return false;
		for (const json& source : *parameterIt)
		{
			if (!source.is_object()) return false;
			GpuParticleUserParameterDesc parameter{};
			parameter.name = source.value("name", std::string{});
			parameter.defaultValue = source.value("defaultValue", parameter.defaultValue);
			parameter.minValue = source.value("minValue", parameter.minValue);
			parameter.maxValue = source.value("maxValue", parameter.maxValue);
			graph.userParameters.push_back(std::move(parameter));
		}
	}

	const auto emitterIt = root.find("emitters");
	if (emitterIt == root.end() || !emitterIt->is_array()) return false;
	for (const json& emitterSource : *emitterIt)
	{
		if (!emitterSource.is_object()) return false;
		VfxGraphEmitterDesc emitter{};
		emitter.name = emitterSource.value("name", std::string{});
		emitter.maxParticles = emitterSource.value("maxParticles", emitter.maxParticles);
		emitter.loop = emitterSource.value("loop", emitter.loop);
		emitter.duration = emitterSource.value("duration", emitter.duration);

		const auto bindingIt = emitterSource.find("parameterBindings");
		if (bindingIt != emitterSource.end())
		{
			if (!bindingIt->is_array()) return false;
			for (const json& bindingSource : *bindingIt)
			{
				if (!bindingSource.is_object()) return false;
				GpuParticleParameterBindingDesc binding{};
				binding.parameterName = bindingSource.value("parameterName", std::string{});
				const std::string targetText = bindingSource.value("target", std::string{});
				if (!TryParseParameterTarget(targetText, binding.target)) return false;
				binding.scale = bindingSource.value("scale", binding.scale);
				binding.bias = bindingSource.value("bias", binding.bias);
				emitter.parameterBindings.push_back(std::move(binding));
			}
		}

		const auto nodesIt = emitterSource.find("nodes");
		if (nodesIt == emitterSource.end() || !nodesIt->is_array()) return false;
		for (const json& nodeSource : *nodesIt)
		{
			if (!nodeSource.is_object()) return false;
			VfxGraphNodeDesc node{};
			node.id = nodeSource.value("id", 0u);
			node.name = nodeSource.value("name", std::string("Node"));
			node.enabled = nodeSource.value("enabled", true);
			const std::string stageText = nodeSource.value("stage", std::string{});
			const std::string typeText = nodeSource.value("type", std::string{});
			if (!TryParseVfxGraphNodeStage(stageText, node.stage) || !TryParseVfxGraphNodeType(typeText, node.type)) return false;
			const auto editorPositionIt = nodeSource.find("editorPosition");
			if (editorPositionIt != nodeSource.end() && !ReadVector2(*editorPositionIt, node.editorPosition)) return false;
			const auto paramsIt = nodeSource.find("params");
			if (paramsIt == nodeSource.end() || !ReadNodePayload(node, *paramsIt)) return false;
			emitter.nodes.push_back(std::move(node));
		}

		const auto edgesIt = emitterSource.find("edges");
		if (edgesIt != emitterSource.end())
		{
			if (!edgesIt->is_array()) return false;
			for (const json& edgeSource : *edgesIt)
			{
				if (!edgeSource.is_object()) return false;
				VfxGraphEdgeDesc edge{};
				edge.fromNodeId = edgeSource.value("from", 0u);
				edge.toNodeId = edgeSource.value("to", 0u);
				emitter.edges.push_back(edge);
			}
		}
		graph.emitters.push_back(std::move(emitter));
	}

	outGraph = std::move(graph);
	return true;
}

bool VfxGraphSerializer::Save(const VfxGraphDesc& graph, const std::string& filePath)
{
	json root;
	root["schemaVersion"] = VfxGraphDesc::kSchemaVersion;
	root["graphName"] = graph.graphName;
	root["userParameters"] = json::array();
	for (const GpuParticleUserParameterDesc& parameter : graph.userParameters)
	{
		root["userParameters"].push_back({
			{ "name", parameter.name },
			{ "defaultValue", parameter.defaultValue },
			{ "minValue", parameter.minValue },
			{ "maxValue", parameter.maxValue }
		});
	}

	root["emitters"] = json::array();
	for (const VfxGraphEmitterDesc& emitter : graph.emitters)
	{
		json emitterJson;
		emitterJson["name"] = emitter.name;
		emitterJson["maxParticles"] = emitter.maxParticles;
		emitterJson["loop"] = emitter.loop;
		emitterJson["duration"] = emitter.duration;
		emitterJson["parameterBindings"] = json::array();
		for (const GpuParticleParameterBindingDesc& binding : emitter.parameterBindings)
		{
			emitterJson["parameterBindings"].push_back({
				{ "parameterName", binding.parameterName },
				{ "target", ParameterTargetToString(binding.target) },
				{ "scale", binding.scale },
				{ "bias", binding.bias }
			});
		}

		emitterJson["nodes"] = json::array();
		for (const VfxGraphNodeDesc& node : emitter.nodes)
		{
			emitterJson["nodes"].push_back({
				{ "id", node.id },
				{ "name", node.name },
				{ "stage", ToString(node.stage) },
				{ "type", ToString(node.type) },
				{ "enabled", node.enabled },
				{ "editorPosition", WriteVector2(node.editorPosition) },
				{ "params", WriteNodePayload(node) }
			});
		}

		emitterJson["edges"] = json::array();
		for (const VfxGraphEdgeDesc& edge : emitter.edges)
		{
			emitterJson["edges"].push_back({ { "from", edge.fromNodeId }, { "to", edge.toNodeId } });
		}
		root["emitters"].push_back(std::move(emitterJson));
	}
	return JsonFileIO::SaveJsonFile(filePath, root, 4);
}

} // namespace Ken4lowEngine
