#include "VfxGraphSerializer.h"

#include "JsonFileIO.h"

#include <json.hpp>
#include <type_traits>
#include <utility>

namespace Ken4lowEngine
{
namespace
{
using json = nlohmann::json;

bool ReadVector2(const json& v, Vector2& out)
{
	if (!v.is_array() || v.size() != 2u || !v[0].is_number() || !v[1].is_number()) return false;
	out = { v[0].get<float>(), v[1].get<float>() };
	return true;
}
bool ReadVector3(const json& v, Vector3& out)
{
	if (!v.is_array() || v.size() != 3u) return false;
	for (size_t i = 0; i < 3u; ++i) if (!v[i].is_number()) return false;
	out = { v[0].get<float>(), v[1].get<float>(), v[2].get<float>() };
	return true;
}
bool ReadVector4(const json& v, Vector4& out)
{
	if (!v.is_array() || v.size() != 4u) return false;
	for (size_t i = 0; i < 4u; ++i) if (!v[i].is_number()) return false;
	out = { v[0].get<float>(), v[1].get<float>(), v[2].get<float>(), v[3].get<float>() };
	return true;
}
json WriteVector2(const Vector2& v) { return json::array({ v.x, v.y }); }
json WriteVector3(const Vector3& v) { return json::array({ v.x, v.y, v.z }); }
json WriteVector4(const Vector4& v) { return json::array({ v.x, v.y, v.z, v.w }); }

const char* CurveInterpolationToString(VfxCurveInterpolation value)
{
	switch (value)
	{
	case VfxCurveInterpolation::Step: return "Step";
	case VfxCurveInterpolation::SmoothStep: return "SmoothStep";
	case VfxCurveInterpolation::Linear:
	default: return "Linear";
	}
}
bool TryParseCurveInterpolation(const std::string& text, VfxCurveInterpolation& out)
{
	if (text == "Linear") out = VfxCurveInterpolation::Linear;
	else if (text == "Step") out = VfxCurveInterpolation::Step;
	else if (text == "SmoothStep") out = VfxCurveInterpolation::SmoothStep;
	else return false;
	return true;
}

bool ReadFloatCurve(const json& value, VfxFloatCurve& outCurve)
{
	if (!value.is_object()) return false;
	if (!TryParseCurveInterpolation(value.value("interpolation", std::string("Linear")), outCurve.interpolation)) return false;
	const auto keysIt = value.find("keys");
	if (keysIt == value.end() || !keysIt->is_array()) return false;
	outCurve.keys.clear();
	for (const json& source : *keysIt)
	{
		if (!source.is_object() || !source.contains("time") || !source.contains("value") || !source["time"].is_number() || !source["value"].is_number()) return false;
		outCurve.keys.push_back({ source["time"].get<float>(), source["value"].get<float>() });
	}
	return true;
}
json WriteFloatCurve(const VfxFloatCurve& curve)
{
	json result;
	result["interpolation"] = CurveInterpolationToString(curve.interpolation);
	result["keys"] = json::array();
	for (const auto& key : curve.keys) result["keys"].push_back({ { "time", key.time }, { "value", key.value } });
	return result;
}

bool ReadColorGradient(const json& value, VfxColorGradient& outGradient)
{
	if (!value.is_object()) return false;
	if (!TryParseCurveInterpolation(value.value("interpolation", std::string("Linear")), outGradient.interpolation)) return false;
	const auto keysIt = value.find("keys");
	if (keysIt == value.end() || !keysIt->is_array()) return false;
	outGradient.keys.clear();
	for (const json& source : *keysIt)
	{
		if (!source.is_object() || !source.contains("time") || !source["time"].is_number() || !source.contains("color")) return false;
		VfxColorGradientKey key{};
		key.time = source["time"].get<float>();
		if (!ReadVector4(source["color"], key.color)) return false;
		outGradient.keys.push_back(key);
	}
	return true;
}
json WriteColorGradient(const VfxColorGradient& gradient)
{
	json result;
	result["interpolation"] = CurveInterpolationToString(gradient.interpolation);
	result["keys"] = json::array();
	for (const auto& key : gradient.keys) result["keys"].push_back({ { "time", key.time }, { "color", WriteVector4(key.color) } });
	return result;
}

const char* BlendModeToString(GpuParticleBlendMode mode)
{
	switch (mode)
	{
	case GpuParticleBlendMode::Alpha: return "Alpha";
	case GpuParticleBlendMode::Multiply: return "Multiply";
	case GpuParticleBlendMode::Additive:
	default: return "Additive";
	}
}
bool TryParseBlendMode(const std::string& text, GpuParticleBlendMode& out)
{
	if (text == "Alpha") out = GpuParticleBlendMode::Alpha;
	else if (text == "Additive") out = GpuParticleBlendMode::Additive;
	else if (text == "Multiply") out = GpuParticleBlendMode::Multiply;
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
bool TryParseParameterTarget(const std::string& text, GpuParticleParameterTarget& out)
{
	if (text == "SpawnRate") out = GpuParticleParameterTarget::SpawnRate;
	else if (text == "BurstCount") out = GpuParticleParameterTarget::BurstCount;
	else if (text == "LifeTime") out = GpuParticleParameterTarget::LifeTime;
	else if (text == "Speed") out = GpuParticleParameterTarget::Speed;
	else if (text == "Size") out = GpuParticleParameterTarget::Size;
	else if (text == "Alpha") out = GpuParticleParameterTarget::Alpha;
	else if (text == "Force") out = GpuParticleParameterTarget::Force;
	else return false;
	return true;
}

bool ReadNodePayload(VfxGraphNodeDesc& node, const json& params)
{
	if (!params.is_object()) return false;
	switch (node.type)
	{
	case VfxGraphNodeType::SpawnRate: { VfxGraphSpawnRateNode p{}; p.rate = params.value("rate", p.rate); node.payload = p; return true; }
	case VfxGraphNodeType::Burst: { VfxGraphBurstNode p{}; p.count = params.value("count", p.count); node.payload = p; return true; }
	case VfxGraphNodeType::SpawnPoint: node.payload = VfxGraphSpawnPointNode{}; return true;
	case VfxGraphNodeType::SpawnSphere: { VfxGraphSpawnSphereNode p{}; p.radius = params.value("radius", p.radius); node.payload = p; return true; }
	case VfxGraphNodeType::SpawnBox:
	{
		VfxGraphSpawnBoxNode p{}; if (params.contains("size") && !ReadVector3(params["size"], p.size)) return false; node.payload = p; return true;
	}
	case VfxGraphNodeType::Lifetime: { VfxGraphLifetimeNode p{}; p.lifetime = params.value("lifetime", p.lifetime); p.random = params.value("random", p.random); node.payload = p; return true; }
	case VfxGraphNodeType::InitialVelocity:
	{
		VfxGraphInitialVelocityNode p{}; if (params.contains("velocity") && !ReadVector3(params["velocity"], p.velocity)) return false;
		if (params.contains("random") && !ReadVector3(params["random"], p.random)) return false; p.speed = params.value("speed", p.speed); p.speedRandom = params.value("speedRandom", p.speedRandom); node.payload = p; return true;
	}
	case VfxGraphNodeType::InitialColor:
	{
		VfxGraphInitialColorNode p{}; if (params.contains("start") && !ReadVector4(params["start"], p.start)) return false; if (params.contains("end") && !ReadVector4(params["end"], p.end)) return false; p.alphaFade = params.value("alphaFade", p.alphaFade); node.payload = p; return true;
	}
	case VfxGraphNodeType::InitialSize:
	{
		VfxGraphInitialSizeNode p{}; if (params.contains("start") && !ReadVector2(params["start"], p.start)) return false; if (params.contains("end") && !ReadVector2(params["end"], p.end)) return false; p.random = params.value("random", p.random); node.payload = p; return true;
	}
	case VfxGraphNodeType::Gravity: { VfxGraphGravityNode p{}; if (params.contains("acceleration") && !ReadVector3(params["acceleration"], p.acceleration)) return false; node.payload = p; return true; }
	case VfxGraphNodeType::Drag: { VfxGraphDragNode p{}; p.damping = params.value("damping", p.damping); node.payload = p; return true; }
	case VfxGraphNodeType::InitialRotation: { VfxGraphInitialRotationNode p{}; p.rotation = params.value("rotation", p.rotation); p.random = params.value("random", p.random); node.payload = p; return true; }
	case VfxGraphNodeType::RotationRate: { VfxGraphRotationRateNode p{}; p.radiansPerSecond = params.value("radiansPerSecond", p.radiansPerSecond); node.payload = p; return true; }
	case VfxGraphNodeType::SizeOverLife:
	{
		VfxGraphSizeOverLifeNode p{}; if (!params.contains("curve") || !ReadFloatCurve(params["curve"], p.multiplier)) return false; node.payload = std::move(p); return true;
	}
	case VfxGraphNodeType::ColorOverLife:
	{
		VfxGraphColorOverLifeNode p{}; if (!params.contains("gradient") || !ReadColorGradient(params["gradient"], p.gradient)) return false; node.payload = std::move(p); return true;
	}
	case VfxGraphNodeType::Collision:
	{
		VfxGraphCollisionNode p{};
		if (!TryParseVfxCollisionShape(params.value("shape", std::string("Plane")), p.shape)) return false;
		if (!TryParseVfxCollisionResponse(params.value("response", std::string("Bounce")), p.response)) return false;
		if (params.contains("planeNormal") && !ReadVector3(params["planeNormal"], p.planeNormal)) return false;
		p.planeDistance = params.value("planeDistance", p.planeDistance);
		if (params.contains("sphereCenter") && !ReadVector3(params["sphereCenter"], p.sphereCenter)) return false;
		p.sphereRadius = params.value("sphereRadius", p.sphereRadius);
		p.particleRadius = params.value("particleRadius", p.particleRadius);
		p.restitution = params.value("restitution", p.restitution);
		p.friction = params.value("friction", p.friction);
		p.generateEvent = params.value("generateEvent", p.generateEvent);
		node.payload = p; return true;
	}
	case VfxGraphNodeType::DeathEvent: node.payload = VfxGraphDeathEventNode{}; return true;
	case VfxGraphNodeType::SubEmitter:
	{
		VfxGraphSubEmitterNode p{};
		if (!TryParseVfxParticleEventType(params.value("sourceEvent", std::string("Collision")), p.sourceEvent)) return false;
		p.count = params.value("count", p.count); p.lifeTime = params.value("lifeTime", p.lifeTime); p.speed = params.value("speed", p.speed);
		p.spread = params.value("spread", p.spread); p.inheritVelocity = params.value("inheritVelocity", p.inheritVelocity);
		if (params.contains("startSize") && !ReadVector2(params["startSize"], p.startSize)) return false;
		if (params.contains("endSize") && !ReadVector2(params["endSize"], p.endSize)) return false;
		if (params.contains("startColor") && !ReadVector4(params["startColor"], p.startColor)) return false;
		if (params.contains("endColor") && !ReadVector4(params["endColor"], p.endColor)) return false;
		p.alphaFade = params.value("alphaFade", p.alphaFade); node.payload = p; return true;
	}
	case VfxGraphNodeType::SpriteRenderer:
	{
		VfxGraphSpriteRendererNode p{}; p.texturePath = params.value("texturePath", p.texturePath); p.billboard = params.value("billboard", p.billboard);
		if (!TryParseBlendMode(params.value("blendMode", std::string("Additive")), p.blendMode)) return false; node.payload = p; return true;
	}
	case VfxGraphNodeType::RibbonRenderer:
	{
		VfxGraphRibbonRendererNode p{}; p.texturePath = params.value("texturePath", p.texturePath); p.width = params.value("width", p.width); p.length = params.value("length", p.length);
		if (!TryParseBlendMode(params.value("blendMode", std::string("Additive")), p.blendMode)) return false; node.payload = p; return true;
	}
	case VfxGraphNodeType::TrailRenderer:
	{
		VfxGraphTrailRendererNode p{}; p.texturePath = params.value("texturePath", p.texturePath); p.width = params.value("width", p.width); p.length = params.value("length", p.length);
		if (!TryParseBlendMode(params.value("blendMode", std::string("Additive")), p.blendMode)) return false; node.payload = p; return true;
	}
	case VfxGraphNodeType::MeshRenderer:
	{
		VfxGraphMeshRendererNode p{}; p.meshPath = params.value("meshPath", p.meshPath); p.subMeshIndex = params.value("subMeshIndex", p.subMeshIndex);
		if (!TryParseBlendMode(params.value("blendMode", std::string("Alpha")), p.blendMode)) return false;
		if (params.contains("startScale") && !ReadVector3(params["startScale"], p.startScale)) return false;
		if (params.contains("endScale") && !ReadVector3(params["endScale"], p.endScale)) return false;
		if (params.contains("startRotation") && !ReadVector3(params["startRotation"], p.startRotation)) return false;
		if (params.contains("angularVelocity") && !ReadVector3(params["angularVelocity"], p.angularVelocity)) return false;
		node.payload = p; return true;
	}
	default: return false;
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
		else if constexpr (std::is_same_v<T, VfxGraphLifetimeNode>) { params["lifetime"] = payload.lifetime; params["random"] = payload.random; }
		else if constexpr (std::is_same_v<T, VfxGraphInitialVelocityNode>) { params["velocity"] = WriteVector3(payload.velocity); params["random"] = WriteVector3(payload.random); params["speed"] = payload.speed; params["speedRandom"] = payload.speedRandom; }
		else if constexpr (std::is_same_v<T, VfxGraphInitialColorNode>) { params["start"] = WriteVector4(payload.start); params["end"] = WriteVector4(payload.end); params["alphaFade"] = payload.alphaFade; }
		else if constexpr (std::is_same_v<T, VfxGraphInitialSizeNode>) { params["start"] = WriteVector2(payload.start); params["end"] = WriteVector2(payload.end); params["random"] = payload.random; }
		else if constexpr (std::is_same_v<T, VfxGraphGravityNode>) params["acceleration"] = WriteVector3(payload.acceleration);
		else if constexpr (std::is_same_v<T, VfxGraphDragNode>) params["damping"] = payload.damping;
		else if constexpr (std::is_same_v<T, VfxGraphInitialRotationNode>) { params["rotation"] = payload.rotation; params["random"] = payload.random; }
		else if constexpr (std::is_same_v<T, VfxGraphRotationRateNode>) params["radiansPerSecond"] = payload.radiansPerSecond;
		else if constexpr (std::is_same_v<T, VfxGraphSizeOverLifeNode>) params["curve"] = WriteFloatCurve(payload.multiplier);
		else if constexpr (std::is_same_v<T, VfxGraphColorOverLifeNode>) params["gradient"] = WriteColorGradient(payload.gradient);
		else if constexpr (std::is_same_v<T, VfxGraphCollisionNode>)
		{
			params["shape"] = ToString(payload.shape); params["response"] = ToString(payload.response); params["planeNormal"] = WriteVector3(payload.planeNormal);
			params["planeDistance"] = payload.planeDistance; params["sphereCenter"] = WriteVector3(payload.sphereCenter); params["sphereRadius"] = payload.sphereRadius;
			params["particleRadius"] = payload.particleRadius; params["restitution"] = payload.restitution; params["friction"] = payload.friction; params["generateEvent"] = payload.generateEvent;
		}
		else if constexpr (std::is_same_v<T, VfxGraphSubEmitterNode>)
		{
			params["sourceEvent"] = ToString(payload.sourceEvent); params["count"] = payload.count; params["lifeTime"] = payload.lifeTime; params["speed"] = payload.speed;
			params["spread"] = payload.spread; params["inheritVelocity"] = payload.inheritVelocity; params["startSize"] = WriteVector2(payload.startSize); params["endSize"] = WriteVector2(payload.endSize);
			params["startColor"] = WriteVector4(payload.startColor); params["endColor"] = WriteVector4(payload.endColor); params["alphaFade"] = payload.alphaFade;
		}
		else if constexpr (std::is_same_v<T, VfxGraphSpriteRendererNode>) { params["texturePath"] = payload.texturePath; params["blendMode"] = BlendModeToString(payload.blendMode); params["billboard"] = payload.billboard; }
		else if constexpr (std::is_same_v<T, VfxGraphRibbonRendererNode>) { params["texturePath"] = payload.texturePath; params["blendMode"] = BlendModeToString(payload.blendMode); params["width"] = payload.width; params["length"] = payload.length; }
		else if constexpr (std::is_same_v<T, VfxGraphTrailRendererNode>) { params["texturePath"] = payload.texturePath; params["blendMode"] = BlendModeToString(payload.blendMode); params["width"] = payload.width; params["length"] = payload.length; }
		else if constexpr (std::is_same_v<T, VfxGraphMeshRendererNode>)
		{
			params["meshPath"] = payload.meshPath; params["subMeshIndex"] = payload.subMeshIndex; params["blendMode"] = BlendModeToString(payload.blendMode);
			params["startScale"] = WriteVector3(payload.startScale); params["endScale"] = WriteVector3(payload.endScale); params["startRotation"] = WriteVector3(payload.startRotation); params["angularVelocity"] = WriteVector3(payload.angularVelocity);
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

	if (const auto it = root.find("userParameters"); it != root.end())
	{
		if (!it->is_array()) return false;
		for (const json& source : *it)
		{
			if (!source.is_object()) return false;
			GpuParticleUserParameterDesc p{}; p.name = source.value("name", std::string{}); p.defaultValue = source.value("defaultValue", p.defaultValue); p.minValue = source.value("minValue", p.minValue); p.maxValue = source.value("maxValue", p.maxValue); graph.userParameters.push_back(std::move(p));
		}
	}

	const auto emitterIt = root.find("emitters");
	if (emitterIt == root.end() || !emitterIt->is_array()) return false;
	for (const json& source : *emitterIt)
	{
		if (!source.is_object()) return false;
		VfxGraphEmitterDesc emitter{}; emitter.name = source.value("name", std::string{}); emitter.maxParticles = source.value("maxParticles", emitter.maxParticles); emitter.loop = source.value("loop", emitter.loop); emitter.duration = source.value("duration", emitter.duration);
		if (const auto bindings = source.find("parameterBindings"); bindings != source.end())
		{
			if (!bindings->is_array()) return false;
			for (const json& b : *bindings)
			{
				GpuParticleParameterBindingDesc binding{}; binding.parameterName = b.value("parameterName", std::string{}); if (!TryParseParameterTarget(b.value("target", std::string{}), binding.target)) return false; binding.scale = b.value("scale", binding.scale); binding.bias = b.value("bias", binding.bias); emitter.parameterBindings.push_back(std::move(binding));
			}
		}
		const auto nodes = source.find("nodes");
		if (nodes == source.end() || !nodes->is_array()) return false;
		for (const json& n : *nodes)
		{
			if (!n.is_object()) return false;
			VfxGraphNodeDesc node{}; node.id = n.value("id", 0u); node.name = n.value("name", std::string("Node")); node.enabled = n.value("enabled", true);
			if (!TryParseVfxGraphNodeStage(n.value("stage", std::string{}), node.stage) || !TryParseVfxGraphNodeType(n.value("type", std::string{}), node.type)) return false;
			if (n.contains("editorPosition") && !ReadVector2(n["editorPosition"], node.editorPosition)) return false;
			if (!n.contains("params") || !ReadNodePayload(node, n["params"])) return false;
			emitter.nodes.push_back(std::move(node));
		}
		if (const auto edges = source.find("edges"); edges != source.end())
		{
			if (!edges->is_array()) return false;
			for (const json& e : *edges) emitter.edges.push_back({ e.value("from", 0u), e.value("to", 0u) });
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
	for (const auto& p : graph.userParameters) root["userParameters"].push_back({ { "name", p.name }, { "defaultValue", p.defaultValue }, { "minValue", p.minValue }, { "maxValue", p.maxValue } });
	root["emitters"] = json::array();
	for (const auto& emitter : graph.emitters)
	{
		json e; e["name"] = emitter.name; e["maxParticles"] = emitter.maxParticles; e["loop"] = emitter.loop; e["duration"] = emitter.duration; e["parameterBindings"] = json::array();
		for (const auto& b : emitter.parameterBindings) e["parameterBindings"].push_back({ { "parameterName", b.parameterName }, { "target", ParameterTargetToString(b.target) }, { "scale", b.scale }, { "bias", b.bias } });
		e["nodes"] = json::array();
		for (const auto& node : emitter.nodes) e["nodes"].push_back({ { "id", node.id }, { "name", node.name }, { "stage", ToString(node.stage) }, { "type", ToString(node.type) }, { "enabled", node.enabled }, { "editorPosition", WriteVector2(node.editorPosition) }, { "params", WriteNodePayload(node) } });
		e["edges"] = json::array();
		for (const auto& edge : emitter.edges) e["edges"].push_back({ { "from", edge.fromNodeId }, { "to", edge.toNodeId } });
		root["emitters"].push_back(std::move(e));
	}
	return JsonFileIO::SaveJsonFile(filePath, root, 4);
}

} // namespace Ken4lowEngine
