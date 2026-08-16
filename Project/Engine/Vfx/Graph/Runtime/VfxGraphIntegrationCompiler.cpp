#include "VfxGraphIntegrationCompiler.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace Ken4lowEngine
{
namespace
{
	bool IsFinite(const Vector3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	bool IsIntegrationNode(VfxGraphNodeType type)
	{
		return type == VfxGraphNodeType::FluidOutput ||
			type == VfxGraphNodeType::LightOutput ||
			type == VfxGraphNodeType::PostEffectOutput;
	}

	std::string NodePrefix(const VfxGraphEmitterDesc& emitter, const VfxGraphNodeDesc& node)
	{
		return "Emitter '" + emitter.name + "' node '" + node.name + "': ";
	}

	void AddBinding(
		VfxCueTrackDesc& track,
		const std::string& parameterName,
		VfxCueBindingTarget target)
	{
		if (parameterName.empty()) return;
		VfxCueTrackBindingDesc binding{};
		binding.parameterName = parameterName;
		binding.target = target;
		binding.scale = 1.0f;
		binding.bias = 0.0f;
		track.bindings.push_back(std::move(binding));
	}

	void InitializeCue(const VfxGraphDesc& graph, VfxCueDesc& cue, const char* suffix, bool loop)
	{
		cue = {};
		cue.schemaVersion = VfxCueDesc::kCurrentSchemaVersion;
		cue.cueName = "__VFX_GRAPH__" + graph.graphName + suffix;
		cue.loop = loop;
		cue.userParameters.reserve(graph.userParameters.size());
		for (const GpuParticleUserParameterDesc& parameter : graph.userParameters)
		{
			VfxCueUserParameterDesc cueParameter{};
			cueParameter.name = parameter.name;
			cueParameter.defaultValue = parameter.defaultValue;
			cueParameter.minValue = parameter.minValue;
			cueParameter.maxValue = parameter.maxValue;
			cue.userParameters.push_back(std::move(cueParameter));
		}
	}
}

bool VfxGraphIntegrationCompiler::Compile(
	const VfxGraphDesc& graph,
	VfxCueDesc& outOneShotCue,
	VfxCueDesc& outLoopCue,
	std::vector<std::string>& errors)
{
	InitializeCue(graph, outOneShotCue, "__OneShot", false);
	InitializeCue(graph, outLoopCue, "__Loop", true);

	std::unordered_set<std::string> parameterNames;
	for (const GpuParticleUserParameterDesc& parameter : graph.userParameters)
	{
		parameterNames.insert(parameter.name);
	}

	bool hasIntegrationNode = false;
	for (const VfxGraphEmitterDesc& emitter : graph.emitters)
	{
		std::vector<const VfxGraphNodeDesc*> nodes;
		for (const VfxGraphNodeDesc& node : emitter.nodes)
		{
			if (node.enabled && IsIntegrationNode(node.type)) nodes.push_back(&node);
		}
		std::sort(nodes.begin(), nodes.end(), [](const VfxGraphNodeDesc* a, const VfxGraphNodeDesc* b)
			{
				if (a->stage != b->stage) return static_cast<uint32_t>(a->stage) < static_cast<uint32_t>(b->stage);
				return a->id < b->id;
			});

		for (const VfxGraphNodeDesc* node : nodes)
		{
			hasIntegrationNode = true;
			const std::string prefix = NodePrefix(emitter, *node);
			VfxCueTrackDesc track{};
			track.name = emitter.name + "/" + node->name;
			track.enabled = true;
			track.startTime = 0.0f;

			auto validateParameter = [&](const std::string& name, const char* label)
				{
					if (!name.empty() && !parameterNames.contains(name)) errors.push_back(prefix + label + " references unknown user parameter: " + name);
				};

			switch (node->type)
			{
			case VfxGraphNodeType::FluidOutput:
			{
				const auto* payload = std::get_if<VfxGraphFluidOutputNode>(&node->payload);
				if (payload == nullptr)
				{
					errors.push_back(prefix + "FluidOutput payload mismatch");
					continue;
				}
				if (static_cast<uint32_t>(payload->domain) > static_cast<uint32_t>(VfxGraphFluidDomain::Volumetric3D)) errors.push_back(prefix + "fluid domain is invalid");
				if (!IsFinite(payload->localOffset) || !IsFinite(payload->localVelocity) || !std::isfinite(payload->duration) || payload->duration <= 0.0f ||
					!std::isfinite(payload->radius) || payload->radius <= 0.0f || !std::isfinite(payload->velocityStrength) || payload->velocityStrength < 0.0f ||
					!std::isfinite(payload->densityRate) || !std::isfinite(payload->temperatureRate) || !std::isfinite(payload->falloffExponent) || payload->falloffExponent <= 0.0f)
					errors.push_back(prefix + "fluid integration values are invalid");
				validateParameter(payload->intensityParameter, "intensityParameter");
				validateParameter(payload->radiusParameter, "radiusParameter");
				track.type = payload->domain == VfxGraphFluidDomain::Volumetric3D ? VfxCueTrackType::VolumetricFluid : VfxCueTrackType::Fluid2D;
				track.duration = payload->duration;
				track.localOffset = payload->localOffset;
				VfxFluidTrackPayload fluid{};
				fluid.localVelocity = payload->localVelocity;
				fluid.radius = payload->radius;
				fluid.velocityStrength = payload->velocityStrength;
				fluid.densityRate = payload->densityRate;
				fluid.temperatureRate = payload->temperatureRate;
				fluid.falloffExponent = payload->falloffExponent;
				track.payload = fluid;
				AddBinding(track, payload->intensityParameter, VfxCueBindingTarget::IntensityScale);
				AddBinding(track, payload->radiusParameter, VfxCueBindingTarget::RadiusScale);
				break;
			}
			case VfxGraphNodeType::LightOutput:
			{
				const auto* payload = std::get_if<VfxGraphLightOutputNode>(&node->payload);
				if (payload == nullptr)
				{
					errors.push_back(prefix + "LightOutput payload mismatch");
					continue;
				}
				if (!IsFinite(payload->localOffset) || !IsFinite(payload->color) || !std::isfinite(payload->duration) || payload->duration <= 0.0f ||
					!std::isfinite(payload->intensity) || payload->intensity < 0.0f || !std::isfinite(payload->range) || payload->range <= 0.0f)
					errors.push_back(prefix + "light integration values are invalid");
				validateParameter(payload->intensityParameter, "intensityParameter");
				validateParameter(payload->radiusParameter, "radiusParameter");
				track.type = VfxCueTrackType::Light;
				track.duration = payload->duration;
				track.localOffset = payload->localOffset;
				VfxLightTrackPayload light{};
				light.color = payload->color;
				light.intensity = payload->intensity;
				light.range = payload->range;
				track.payload = light;
				AddBinding(track, payload->intensityParameter, VfxCueBindingTarget::IntensityScale);
				AddBinding(track, payload->radiusParameter, VfxCueBindingTarget::RadiusScale);
				break;
			}
			case VfxGraphNodeType::PostEffectOutput:
			{
				const auto* payload = std::get_if<VfxGraphPostEffectOutputNode>(&node->payload);
				if (payload == nullptr)
				{
					errors.push_back(prefix + "PostEffectOutput payload mismatch");
					continue;
				}
				if (payload->effectName.empty() || !std::isfinite(payload->duration) || payload->duration <= 0.0f ||
					!std::isfinite(payload->weight) || payload->weight < 0.0f || payload->weight > 1.0f)
					errors.push_back(prefix + "post effect integration values are invalid");
				validateParameter(payload->intensityParameter, "intensityParameter");
				track.type = VfxCueTrackType::PostEffect;
				track.duration = payload->duration;
				VfxPostEffectTrackPayload post{};
				post.effectName = payload->effectName;
				post.weight = payload->weight;
				track.payload = std::move(post);
				AddBinding(track, payload->intensityParameter, VfxCueBindingTarget::IntensityScale);
				break;
			}
			default:
				continue;
			}

			outOneShotCue.duration = (std::max)(outOneShotCue.duration, track.startTime + track.duration);
			outOneShotCue.tracks.push_back(std::move(track));
		}
	}

	if (!hasIntegrationNode)
	{
		outLoopCue.tracks.clear();
		outLoopCue.duration = 0.0f;
		return true;
	}
	if (graph.userParameters.size() > VfxCueDesc::kMaxUserParameters) errors.push_back("VFX Graph integration exceeds Phase18 kMaxUserParameters");
	if (outOneShotCue.tracks.size() > VfxGraphDesc::kMaxIntegrationTracks || outOneShotCue.tracks.size() > VfxCueDesc::kMaxTracks) errors.push_back("VFX Graph exceeds integration track budget");
	if (!errors.empty())
	{
		outOneShotCue.tracks.clear();
		outLoopCue.tracks.clear();
		return false;
	}

	outLoopCue.duration = outOneShotCue.duration;
	outLoopCue.tracks = outOneShotCue.tracks;
	return true;
}

} // namespace Ken4lowEngine
