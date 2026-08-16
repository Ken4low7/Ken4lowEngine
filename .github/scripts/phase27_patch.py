from pathlib import Path

ROOT = Path('Project')


def replace_once(path: str, old: str, new: str) -> None:
    p = ROOT / path
    text = p.read_text(encoding='utf-8-sig')
    if old not in text:
        raise RuntimeError(f'pattern not found in {path}: {old[:120]!r}')
    p.write_text(text.replace(old, new, 1), encoding='utf-8')


# -----------------------------------------------------------------------------
# Graph authoring data / serialization
# -----------------------------------------------------------------------------
replace_once(
    'Engine/Vfx/Graph/Data/VfxGraphTypes.h',
    '''enum class VfxGraphFluidDomain : uint32_t\n{\n\tFluid2D = 0,\n\tVolumetric3D,\n};\n''',
    '''enum class VfxGraphFluidDomain : uint32_t\n{\n\tFluid2D = 0,\n\tVolumetric3D,\n};\n\nenum class VfxGraphBoundsMode : uint32_t\n{\n\tAutomatic = 0,\n\tFixedSphere,\n};\n\nstruct VfxGraphScalabilityDesc\n{\n\tVfxGraphBoundsMode boundsMode = VfxGraphBoundsMode::Automatic;\n\tVector3 fixedBoundsCenter{};\n\tfloat fixedBoundsRadius = 8.0f;\n\tbool frustumCulling = true;\n\tfloat maxDrawDistance = 150.0f;\n\tfloat lodNearDistance = 25.0f;\n\tfloat lodFarDistance = 75.0f;\n\tfloat lodMidScale = 0.65f;\n\tfloat lodFarScale = 0.35f;\n\tuint32_t budgetCost = 1u;\n};\n''')
replace_once(
    'Engine/Vfx/Graph/Data/VfxGraphTypes.h',
    '''\tuint32_t schemaVersion = kSchemaVersion;\n\tstd::string graphName = "NewVfxGraph";\n\tstd::vector<GpuParticleUserParameterDesc> userParameters;\n''',
    '''\tuint32_t schemaVersion = kSchemaVersion;\n\tstd::string graphName = "NewVfxGraph";\n\tVfxGraphScalabilityDesc scalability{};\n\tstd::vector<GpuParticleUserParameterDesc> userParameters;\n''')
replace_once(
    'Engine/Vfx/Graph/Data/VfxGraphTypes.h',
    '''const char* ToString(VfxGraphFluidDomain domain);\nbool TryParseVfxGraphNodeStage''',
    '''const char* ToString(VfxGraphFluidDomain domain);\nconst char* ToString(VfxGraphBoundsMode mode);\nbool TryParseVfxGraphNodeStage''')
replace_once(
    'Engine/Vfx/Graph/Data/VfxGraphTypes.h',
    '''bool TryParseVfxGraphFluidDomain(const std::string& text, VfxGraphFluidDomain& outDomain);\n''',
    '''bool TryParseVfxGraphFluidDomain(const std::string& text, VfxGraphFluidDomain& outDomain);\nbool TryParseVfxGraphBoundsMode(const std::string& text, VfxGraphBoundsMode& outMode);\n''')

replace_once(
    'Engine/Vfx/Graph/Data/VfxGraphTypes.cpp',
    '''const char* ToString(VfxGraphFluidDomain domain)\n{\n\tswitch (domain)\n\t{\n\tcase VfxGraphFluidDomain::Fluid2D: return "Fluid2D";\n\tcase VfxGraphFluidDomain::Volumetric3D: return "Volumetric3D";\n\tdefault: return "Volumetric3D";\n\t}\n}\n''',
    '''const char* ToString(VfxGraphFluidDomain domain)\n{\n\tswitch (domain)\n\t{\n\tcase VfxGraphFluidDomain::Fluid2D: return "Fluid2D";\n\tcase VfxGraphFluidDomain::Volumetric3D: return "Volumetric3D";\n\tdefault: return "Volumetric3D";\n\t}\n}\n\nconst char* ToString(VfxGraphBoundsMode mode)\n{\n\tswitch (mode)\n\t{\n\tcase VfxGraphBoundsMode::Automatic: return "Automatic";\n\tcase VfxGraphBoundsMode::FixedSphere: return "FixedSphere";\n\tdefault: return "Automatic";\n\t}\n}\n''')
replace_once(
    'Engine/Vfx/Graph/Data/VfxGraphTypes.cpp',
    '''bool TryParseVfxGraphFluidDomain(const std::string& text, VfxGraphFluidDomain& outDomain)\n{\n\tif (text == "Fluid2D") outDomain = VfxGraphFluidDomain::Fluid2D;\n\telse if (text == "Volumetric3D") outDomain = VfxGraphFluidDomain::Volumetric3D;\n\telse return false;\n\treturn true;\n}\n''',
    '''bool TryParseVfxGraphFluidDomain(const std::string& text, VfxGraphFluidDomain& outDomain)\n{\n\tif (text == "Fluid2D") outDomain = VfxGraphFluidDomain::Fluid2D;\n\telse if (text == "Volumetric3D") outDomain = VfxGraphFluidDomain::Volumetric3D;\n\telse return false;\n\treturn true;\n}\n\nbool TryParseVfxGraphBoundsMode(const std::string& text, VfxGraphBoundsMode& outMode)\n{\n\tif (text == "Automatic") outMode = VfxGraphBoundsMode::Automatic;\n\telse if (text == "FixedSphere") outMode = VfxGraphBoundsMode::FixedSphere;\n\telse return false;\n\treturn true;\n}\n''')

replace_once(
    'Engine/Vfx/Graph/Asset/VfxGraphSerializer.cpp',
    '''\tgraph.graphName = root.value("graphName", std::string{});\n\tif (graph.graphName.empty()) return false;\n\n\tif (const auto it = root.find("userParameters"); it != root.end())\n''',
    '''\tgraph.graphName = root.value("graphName", std::string{});\n\tif (graph.graphName.empty()) return false;\n\n\tif (const auto scalability = root.find("scalability"); scalability != root.end())\n\t{\n\t\tif (!scalability->is_object()) return false;\n\t\tif (!TryParseVfxGraphBoundsMode(scalability->value("boundsMode", std::string("Automatic")), graph.scalability.boundsMode)) return false;\n\t\tif (scalability->contains("fixedBoundsCenter") && !ReadVector3((*scalability)["fixedBoundsCenter"], graph.scalability.fixedBoundsCenter)) return false;\n\t\tgraph.scalability.fixedBoundsRadius = scalability->value("fixedBoundsRadius", graph.scalability.fixedBoundsRadius);\n\t\tgraph.scalability.frustumCulling = scalability->value("frustumCulling", graph.scalability.frustumCulling);\n\t\tgraph.scalability.maxDrawDistance = scalability->value("maxDrawDistance", graph.scalability.maxDrawDistance);\n\t\tgraph.scalability.lodNearDistance = scalability->value("lodNearDistance", graph.scalability.lodNearDistance);\n\t\tgraph.scalability.lodFarDistance = scalability->value("lodFarDistance", graph.scalability.lodFarDistance);\n\t\tgraph.scalability.lodMidScale = scalability->value("lodMidScale", graph.scalability.lodMidScale);\n\t\tgraph.scalability.lodFarScale = scalability->value("lodFarScale", graph.scalability.lodFarScale);\n\t\tgraph.scalability.budgetCost = scalability->value("budgetCost", graph.scalability.budgetCost);\n\t}\n\n\tif (const auto it = root.find("userParameters"); it != root.end())\n''')
replace_once(
    'Engine/Vfx/Graph/Asset/VfxGraphSerializer.cpp',
    '''\troot["schemaVersion"] = VfxGraphDesc::kSchemaVersion;\n\troot["graphName"] = graph.graphName;\n\troot["userParameters"] = json::array();\n''',
    '''\troot["schemaVersion"] = VfxGraphDesc::kSchemaVersion;\n\troot["graphName"] = graph.graphName;\n\troot["scalability"] = {\n\t\t{ "boundsMode", ToString(graph.scalability.boundsMode) },\n\t\t{ "fixedBoundsCenter", WriteVector3(graph.scalability.fixedBoundsCenter) },\n\t\t{ "fixedBoundsRadius", graph.scalability.fixedBoundsRadius },\n\t\t{ "frustumCulling", graph.scalability.frustumCulling },\n\t\t{ "maxDrawDistance", graph.scalability.maxDrawDistance },\n\t\t{ "lodNearDistance", graph.scalability.lodNearDistance },\n\t\t{ "lodFarDistance", graph.scalability.lodFarDistance },\n\t\t{ "lodMidScale", graph.scalability.lodMidScale },\n\t\t{ "lodFarScale", graph.scalability.lodFarScale },\n\t\t{ "budgetCost", graph.scalability.budgetCost },\n\t};\n\troot["userParameters"] = json::array();\n''')

# -----------------------------------------------------------------------------
# Compiler metadata + conservative automatic bounds
# -----------------------------------------------------------------------------
replace_once(
    'Engine/Vfx/Graph/Runtime/VfxGraphProgram.h',
    '''#include "Engine/Vfx/Data/VfxCueTypes.h"\n''',
    '''#include "Engine/Vfx/Data/VfxCueTypes.h"\n#include "Engine/Graphics/Culling/BoundingVolume.h"\n''')
replace_once(
    'Engine/Vfx/Graph/Runtime/VfxGraphProgram.h',
    '''\tVfxCueDesc integrationOneShotCue{};\n\tVfxCueDesc integrationLoopCue{};\n''',
    '''\tVfxCueDesc integrationOneShotCue{};\n\tVfxCueDesc integrationLoopCue{};\n\tBoundingSphere localBounds{};\n\tVfxGraphScalabilityDesc scalability{};\n''')

replace_once(
    'Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp',
    '''bool IsValidBlendMode(GpuParticleBlendMode blendMode)\n{\n\treturn static_cast<uint32_t>(blendMode) <= static_cast<uint32_t>(GpuParticleBlendMode::Multiply);\n}\n''',
    '''bool IsValidBlendMode(GpuParticleBlendMode blendMode)\n{\n\treturn static_cast<uint32_t>(blendMode) <= static_cast<uint32_t>(GpuParticleBlendMode::Multiply);\n}\n\nfloat Length(const Vector3& value)\n{\n\treturn std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);\n}\n\nfloat MaxSize(const Vector2& value)\n{\n\treturn (std::max)(std::abs(value.x), std::abs(value.y));\n}\n\nBoundingSphere EstimateAutomaticBounds(const VfxGraphDesc& graph)\n{\n\tfloat radius = 1.0f;\n\tfor (const VfxGraphEmitterDesc& emitter : graph.emitters)\n\t{\n\t\tfloat spawnExtent = 0.0f;\n\t\tfloat lifetime = 1.0f;\n\t\tfloat velocityExtent = 0.0f;\n\t\tfloat gravityExtent = 0.0f;\n\t\tfloat renderExtent = 0.1f;\n\t\tfloat childExtent = 0.0f;\n\t\tfor (const VfxGraphNodeDesc& node : emitter.nodes)\n\t\t{\n\t\t\tif (!node.enabled) continue;\n\t\t\tswitch (node.type)\n\t\t\t{\n\t\t\tcase VfxGraphNodeType::SpawnSphere:\n\t\t\t\tspawnExtent = (std::max)(spawnExtent, std::get<VfxGraphSpawnSphereNode>(node.payload).radius);\n\t\t\t\tbreak;\n\t\t\tcase VfxGraphNodeType::SpawnBox:\n\t\t\t{\n\t\t\t\tconst Vector3 size = std::get<VfxGraphSpawnBoxNode>(node.payload).size;\n\t\t\t\tspawnExtent = (std::max)(spawnExtent, 0.5f * Length(size));\n\t\t\t\tbreak;\n\t\t\t}\n\t\t\tcase VfxGraphNodeType::Lifetime:\n\t\t\t{\n\t\t\t\tconst auto& p = std::get<VfxGraphLifetimeNode>(node.payload);\n\t\t\t\tlifetime = (std::max)(lifetime, p.lifetime + p.random);\n\t\t\t\tbreak;\n\t\t\t}\n\t\t\tcase VfxGraphNodeType::InitialVelocity:\n\t\t\t{\n\t\t\t\tconst auto& p = std::get<VfxGraphInitialVelocityNode>(node.payload);\n\t\t\t\tvelocityExtent = (std::max)(velocityExtent, Length(p.velocity) + Length(p.random) + p.speed + p.speedRandom);\n\t\t\t\tbreak;\n\t\t\t}\n\t\t\tcase VfxGraphNodeType::Gravity:\n\t\t\t\tgravityExtent = (std::max)(gravityExtent, Length(std::get<VfxGraphGravityNode>(node.payload).acceleration));\n\t\t\t\tbreak;\n\t\t\tcase VfxGraphNodeType::InitialSize:\n\t\t\t{\n\t\t\t\tconst auto& p = std::get<VfxGraphInitialSizeNode>(node.payload);\n\t\t\t\trenderExtent = (std::max)(renderExtent, (std::max)(MaxSize(p.start), MaxSize(p.end)) + p.random);\n\t\t\t\tbreak;\n\t\t\t}\n\t\t\tcase VfxGraphNodeType::RibbonRenderer:\n\t\t\t{\n\t\t\t\tconst auto& p = std::get<VfxGraphRibbonRendererNode>(node.payload);\n\t\t\t\trenderExtent = (std::max)(renderExtent, p.width + p.length);\n\t\t\t\tbreak;\n\t\t\t}\n\t\t\tcase VfxGraphNodeType::TrailRenderer:\n\t\t\t{\n\t\t\t\tconst auto& p = std::get<VfxGraphTrailRendererNode>(node.payload);\n\t\t\t\trenderExtent = (std::max)(renderExtent, p.width + p.length);\n\t\t\t\tbreak;\n\t\t\t}\n\t\t\tcase VfxGraphNodeType::SubEmitter:\n\t\t\t{\n\t\t\t\tconst auto& p = std::get<VfxGraphSubEmitterNode>(node.payload);\n\t\t\t\tchildExtent = (std::max)(childExtent, p.speed * p.lifeTime + MaxSize(p.endSize));\n\t\t\t\tbreak;\n\t\t\t}\n\t\t\tcase VfxGraphNodeType::FluidOutput:\n\t\t\t{\n\t\t\t\tconst auto& p = std::get<VfxGraphFluidOutputNode>(node.payload);\n\t\t\t\tradius = (std::max)(radius, Length(p.localOffset) + p.radius + Length(p.localVelocity) * p.duration);\n\t\t\t\tbreak;\n\t\t\t}\n\t\t\tcase VfxGraphNodeType::LightOutput:\n\t\t\t{\n\t\t\t\tconst auto& p = std::get<VfxGraphLightOutputNode>(node.payload);\n\t\t\t\tradius = (std::max)(radius, Length(p.localOffset) + p.range);\n\t\t\t\tbreak;\n\t\t\t}\n\t\t\tdefault:\n\t\t\t\tbreak;\n\t\t\t}\n\t\t}\n\t\tconst float particleExtent = spawnExtent + velocityExtent * lifetime + 0.5f * gravityExtent * lifetime * lifetime + renderExtent + childExtent;\n\t\tradius = (std::max)(radius, particleExtent);\n\t}\n\treturn { {}, (std::max)(radius, 0.1f) };\n}\n''')
replace_once(
    'Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp',
    '''\tif (graph.emitters.size() > VfxGraphDesc::kMaxEmitters) result.errors.push_back("VFX Graph exceeds kMaxEmitters");\n\n\tstd::unordered_set<std::string> parameterNames;\n''',
    '''\tif (graph.emitters.size() > VfxGraphDesc::kMaxEmitters) result.errors.push_back("VFX Graph exceeds kMaxEmitters");\n\tconst VfxGraphScalabilityDesc& scalability = graph.scalability;\n\tif (static_cast<uint32_t>(scalability.boundsMode) > static_cast<uint32_t>(VfxGraphBoundsMode::FixedSphere)) result.errors.push_back("VFX Graph boundsMode is invalid");\n\tif (!IsFinite(scalability.fixedBoundsCenter) || !std::isfinite(scalability.fixedBoundsRadius) || scalability.fixedBoundsRadius <= 0.0f) result.errors.push_back("VFX Graph fixed bounds are invalid");\n\tif (!std::isfinite(scalability.maxDrawDistance) || scalability.maxDrawDistance < 0.0f) result.errors.push_back("VFX Graph maxDrawDistance must be finite and >= 0");\n\tif (!std::isfinite(scalability.lodNearDistance) || !std::isfinite(scalability.lodFarDistance) || scalability.lodNearDistance < 0.0f || scalability.lodFarDistance < scalability.lodNearDistance) result.errors.push_back("VFX Graph LOD distances are invalid");\n\tif (!std::isfinite(scalability.lodMidScale) || !std::isfinite(scalability.lodFarScale) || scalability.lodMidScale <= 0.0f || scalability.lodMidScale > 1.0f || scalability.lodFarScale <= 0.0f || scalability.lodFarScale > scalability.lodMidScale) result.errors.push_back("VFX Graph LOD scales must satisfy 0 < far <= mid <= 1");\n\tif (scalability.budgetCost == 0u) result.errors.push_back("VFX Graph budgetCost must be > 0");\n\tresult.program.scalability = scalability;\n\tresult.program.localBounds = scalability.boundsMode == VfxGraphBoundsMode::FixedSphere\n\t\t? BoundingSphere{ scalability.fixedBoundsCenter, scalability.fixedBoundsRadius }\n\t\t: EstimateAutomaticBounds(graph);\n\n\tstd::unordered_set<std::string> parameterNames;\n''')

# -----------------------------------------------------------------------------
# Reuse the existing GPU particle backend, but allow runtime LOD emission scale.
# -----------------------------------------------------------------------------
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\tbool Play(const std::string& effectName, const Vector3& worldPosition)\n\t{\n''',
    '''\tbool Play(const std::string& effectName, const Vector3& worldPosition, float runtimeScale = 1.0f)\n\t{\n\t\truntimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\t\t\tGpuParticleEmitter* emitter = EnsureEmitter(*effect, emitterDesc, index, worldPosition, false, burstSlot, nullptr);\n''',
    '''\t\t\tGpuParticleEmitter* emitter = EnsureEmitter(*effect, emitterDesc, index, worldPosition, false, burstSlot, nullptr, runtimeScale);\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\t\t\t\tEvaluateTargetFactor(*effect, emitterDesc, GpuParticleParameterTarget::BurstCount, nullptr));\n''',
    '''\t\t\t\tEvaluateTargetFactor(*effect, emitterDesc, GpuParticleParameterTarget::BurstCount, nullptr) * runtimeScale);\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\tPlayHandle PlayLoop(const std::string& effectName, const Vector3& worldPosition)\n\t{\n''',
    '''\tPlayHandle PlayLoop(const std::string& effectName, const Vector3& worldPosition, float runtimeScale = 1.0f)\n\t{\n\t\truntimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\t\tinstance.worldPosition = worldPosition;\n\n\t\tfor (std::size_t index = 0; index < effect->emitters.size(); ++index)\n''',
    '''\t\tinstance.worldPosition = worldPosition;\n\t\tinstance.runtimeScale = runtimeScale;\n\n\t\tfor (std::size_t index = 0; index < effect->emitters.size(); ++index)\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\t\t\tGpuParticleEmitter* emitter = EnsureEmitter(*effect, emitterDesc, index, worldPosition, true, handle.id, &instance.parameterOverrides);\n''',
    '''\t\t\tGpuParticleEmitter* emitter = EnsureEmitter(*effect, emitterDesc, index, worldPosition, true, handle.id, &instance.parameterOverrides, runtimeScale);\n''')
# second BurstCount occurrence belongs to PlayLoop
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\t\t\t\tEvaluateTargetFactor(*effect, emitterDesc, GpuParticleParameterTarget::BurstCount, &instance.parameterOverrides));\n''',
    '''\t\t\t\tEvaluateTargetFactor(*effect, emitterDesc, GpuParticleParameterTarget::BurstCount, &instance.parameterOverrides) * runtimeScale);\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\tbool SetLoopPosition(PlayHandle handle, const Vector3& worldPosition)\n\t{\n\t\tauto instanceIt = activeLoops_.find(handle.id);\n\t\tif (instanceIt == activeLoops_.end()) return false;\n\t\tinstanceIt->second.worldPosition = worldPosition;\n\t\treturn RefreshLoopInstance(instanceIt->second);\n\t}\n''',
    '''\tbool SetLoopPosition(PlayHandle handle, const Vector3& worldPosition)\n\t{\n\t\tauto instanceIt = activeLoops_.find(handle.id);\n\t\tif (instanceIt == activeLoops_.end()) return false;\n\t\tinstanceIt->second.worldPosition = worldPosition;\n\t\treturn RefreshLoopInstance(instanceIt->second);\n\t}\n\n\tbool SetLoopRuntimeScale(PlayHandle handle, float runtimeScale)\n\t{\n\t\tauto instanceIt = activeLoops_.find(handle.id);\n\t\tif (instanceIt == activeLoops_.end() || !std::isfinite(runtimeScale)) return false;\n\t\tinstanceIt->second.runtimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);\n\t\treturn RefreshLoopInstance(instanceIt->second);\n\t}\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\t\tVector3 worldPosition{};\n\t\tParameterMap parameterOverrides;\n''',
    '''\t\tVector3 worldPosition{};\n\t\tfloat runtimeScale = 1.0f;\n\t\tParameterMap parameterOverrides;\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\t\tbool loopMode,\n\t\tuint32_t instanceId,\n\t\tconst ParameterMap* parameterOverrides)\n''',
    '''\t\tbool loopMode,\n\t\tuint32_t instanceId,\n\t\tconst ParameterMap* parameterOverrides,\n\t\tfloat runtimeScale)\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\t\tif (!CompileEmitterInfo(effect, emitterDesc, loopMode, parameterOverrides, info)) return nullptr;\n''',
    '''\t\tif (!CompileEmitterInfo(effect, emitterDesc, loopMode, parameterOverrides, runtimeScale, info)) return nullptr;\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\t\tbool loopMode,\n\t\tconst ParameterMap* parameterOverrides,\n\t\tGpuParticleEmitter::EmitterInfo& info)\n\t{\n''',
    '''\t\tbool loopMode,\n\t\tconst ParameterMap* parameterOverrides,\n\t\tfloat runtimeScale,\n\t\tGpuParticleEmitter::EmitterInfo& info)\n\t{\n\t\truntimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\t\tinfo.subEmitterCount = emitterDesc.update.subEmitterCount;\n''',
    '''\t\tinfo.subEmitterCount = ScaleCount(emitterDesc.update.subEmitterCount, runtimeScale);\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\t\tconst float effectiveSpawnRate = (std::max)(emitterDesc.emission.spawnRate * spawnRateFactor, 0.0f);\n''',
    '''\t\tconst float effectiveSpawnRate = (std::max)(emitterDesc.emission.spawnRate * spawnRateFactor * runtimeScale, 0.0f);\n''')
replace_once(
    'Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h',
    '''\t\t\tif (!CompileEmitterInfo(effect, effect.emitters[index], true, &instance.parameterOverrides, info)) return false;\n''',
    '''\t\t\tif (!CompileEmitterInfo(effect, effect.emitters[index], true, &instance.parameterOverrides, instance.runtimeScale, info)) return false;\n''')

# -----------------------------------------------------------------------------
# Reuse/extend the existing unified VFX budget and runtime scale.
# -----------------------------------------------------------------------------
replace_once(
    'Engine/Vfx/Runtime/VfxRuntimeTypes.h',
    '''\tuint32_t maxCameraShakes = 16;\n};\n''',
    '''\tuint32_t maxCameraShakes = 16;\n\tuint32_t maxVfxGraphStartCostPerFrame = 64;\n\tuint32_t maxActiveVfxGraphLoopCost = 128;\n};\n''')
replace_once(
    'Engine/Vfx/Runtime/VfxCueRuntime.h',
    '''\tbool SetWorldPosition(VfxCueHandle handle, const Vector3& worldPosition);\n\tbool SetFloatParameter(VfxCueHandle handle, const std::string& parameterName, float value);\n''',
    '''\tbool SetWorldPosition(VfxCueHandle handle, const Vector3& worldPosition);\n\tbool SetFloatParameter(VfxCueHandle handle, const std::string& parameterName, float value);\n\tbool SetRuntimeScale(VfxCueHandle handle, float runtimeScale);\n''')
replace_once(
    'Engine/Vfx/Runtime/VfxCueRuntime.h',
    '''\t\tVector3 worldPosition{};\n\t\tfloat elapsed = 0.0f;\n''',
    '''\t\tVector3 worldPosition{};\n\t\tfloat runtimeScale = 1.0f;\n\t\tfloat elapsed = 0.0f;\n''')
replace_once(
    'Engine/Vfx/Runtime/VfxCueRuntime.cpp',
    '''bool VfxCueRuntime::SetFloatParameter(VfxCueHandle handle, const std::string& parameterName, float value)\n{\n\tconst auto it = instances_.find(handle.value);\n\tif (it == instances_.end())\n\t{\n\t\tSetStatus(false, "VFX SetFloatParameter failed: handle is not active.");\n\t\treturn false;\n\t}\n\tconst VfxCueUserParameterDesc* parameter = FindParameter(it->second.program, parameterName);\n\tif (parameter == nullptr || !std::isfinite(value))\n\t{\n\t\tSetStatus(false, "VFX SetFloatParameter failed: unknown/invalid parameter=" + parameterName);\n\t\treturn false;\n\t}\n\tit->second.parameters[parameterName] = std::clamp(value, parameter->minValue, parameter->maxValue);\n\tSetStatus(true, "Updated VFX parameter: " + parameterName);\n\treturn true;\n}\n''',
    '''bool VfxCueRuntime::SetFloatParameter(VfxCueHandle handle, const std::string& parameterName, float value)\n{\n\tconst auto it = instances_.find(handle.value);\n\tif (it == instances_.end())\n\t{\n\t\tSetStatus(false, "VFX SetFloatParameter failed: handle is not active.");\n\t\treturn false;\n\t}\n\tconst VfxCueUserParameterDesc* parameter = FindParameter(it->second.program, parameterName);\n\tif (parameter == nullptr || !std::isfinite(value))\n\t{\n\t\tSetStatus(false, "VFX SetFloatParameter failed: unknown/invalid parameter=" + parameterName);\n\t\treturn false;\n\t}\n\tit->second.parameters[parameterName] = std::clamp(value, parameter->minValue, parameter->maxValue);\n\tSetStatus(true, "Updated VFX parameter: " + parameterName);\n\treturn true;\n}\n\nbool VfxCueRuntime::SetRuntimeScale(VfxCueHandle handle, float runtimeScale)\n{\n\tconst auto it = instances_.find(handle.value);\n\tif (it == instances_.end() || !std::isfinite(runtimeScale)) return false;\n\tit->second.runtimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);\n\treturn true;\n}\n''')
replace_once(
    'Engine/Vfx/Runtime/VfxCueRuntime.cpp',
    '''\t}\n\treturn resolved;\n}\n\nvoid VfxCueRuntime::RefreshStats()\n''',
    '''\t}\n\t// Phase27 applies graph LOD to existing Fluid/Light/PostEffect adapters without duplicating subsystem backends.\n\tresolved.intensityScale *= instance.runtimeScale;\n\treturn resolved;\n}\n\nvoid VfxCueRuntime::RefreshStats()\n''')

# -----------------------------------------------------------------------------
# Graph runtime scalability facade.
# -----------------------------------------------------------------------------
(ROOT / 'Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h').write_text(r'''#pragma once

#include "Engine/Vfx/Graph/Asset/VfxGraphSerializer.h"
#include "Engine/Vfx/Graph/Runtime/VfxGraphCompiler.h"
#include "Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h"
#include "Engine/Vfx/Runtime/VfxRuntimeTypes.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{

struct VfxGraphPlayHandle
{
	GpuParticleEffectRuntime::PlayHandle particleHandle{};
	VfxCueHandle integrationHandle{};
	std::string graphName;

	[[nodiscard]] bool IsValid() const
	{
		return particleHandle.IsValid();
	}
};

struct VfxGraphRuntimeStats
{
	uint64_t registeredGraphs = 0u;
	uint64_t compileFailures = 0u;
	uint64_t playRequests = 0u;
	uint64_t playSuccesses = 0u;
	uint64_t loopStarts = 0u;
	uint64_t loopStops = 0u;
	uint64_t reloads = 0u;
	uint64_t integrationStarts = 0u;
	uint64_t integrationStops = 0u;
	uint64_t integrationFailures = 0u;
	uint64_t culledOneShots = 0u;
	uint64_t budgetRejectedPlays = 0u;
	uint64_t lodNearSelections = 0u;
	uint64_t lodMidSelections = 0u;
	uint64_t lodFarSelections = 0u;
	uint64_t loopScaleChanges = 0u;
	uint64_t loopCullTransitions = 0u;
	uint32_t graphStartCostThisFrame = 0u;
	uint32_t activeLoopCount = 0u;
	uint32_t activeLoopCost = 0u;
};

/// <summary>
/// Niagara-like Graph AssetをCompileし、粒子はPhase13、Subsystem統合OutputはPhase18 Runtimeへ渡すFacade。
/// Phase27は既存Camera/Frustum/Budgetを再利用し、Bounds/LOD/Cullingをここで統合する。
/// </summary>
class VfxGraphRuntime
{
public:
	static VfxGraphRuntime* GetInstance();

	void BeginFrame();
	void UpdateScalability();

	bool RegisterGraph(const VfxGraphDesc& graph);
	bool LoadGraph(const std::string& filePath);
	bool ReloadGraph(const std::string& graphName);

	bool Play(const std::string& graphName, const Vector3& worldPosition);
	VfxGraphPlayHandle PlayLoop(const std::string& graphName, const Vector3& worldPosition);
	bool StopLoop(VfxGraphPlayHandle handle);
	bool SetLoopPosition(VfxGraphPlayHandle handle, const Vector3& worldPosition);
	bool SetFloatParameter(const std::string& graphName, const std::string& parameterName, float value);
	bool SetFloatParameter(VfxGraphPlayHandle handle, const std::string& parameterName, float value);

	[[nodiscard]] bool IsRegistered(const std::string& graphName) const;
	[[nodiscard]] const VfxGraphProgram* GetProgram(const std::string& graphName) const;
	[[nodiscard]] const VfxGraphRuntimeStats& GetStats() const { return stats_; }
	[[nodiscard]] bool WasLastOperationSuccessful() const { return lastOperationSucceeded_; }
	[[nodiscard]] const std::string& GetLastStatus() const { return lastStatus_; }

private:
	struct ActiveLoopScalability
	{
		VfxGraphPlayHandle handle{};
		Vector3 worldPosition{};
		float runtimeScale = 1.0f;
		bool culled = false;
		uint32_t budgetCost = 1u;
	};

	VfxGraphRuntime() = default;
	float EvaluateRuntimeScale(const VfxGraphProgram& program, const Vector3& worldPosition, bool& outCulled, float& outDistance) const;
	bool ReserveStartBudget(const VfxGraphProgram& program, bool loopStart);
	void RecordLodSelection(const VfxGraphProgram& program, float distance, float scale);
	void RefreshLoopStats();
	void StopActiveLoopsForGraph(const std::string& graphName);
	void SetStatus(bool success, std::string message);

	std::unordered_map<std::string, VfxGraphProgram> programs_;
	std::unordered_map<std::string, std::string> sourcePaths_;
	std::unordered_map<uint32_t, ActiveLoopScalability> activeLoops_;
	VfxGraphRuntimeStats stats_{};
	bool lastOperationSucceeded_ = true;
	std::string lastStatus_ = "VFX Graph Runtime ready.";
};

} // namespace Ken4lowEngine
''', encoding='utf-8')

(ROOT / 'Engine/Vfx/Graph/Runtime/VfxGraphRuntime.cpp').write_text(r'''#include "VfxGraphRuntime.h"

#include "Engine/Graphics/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Culling/Frustum.h"
#include "Engine/Vfx/Runtime/VfxCueRuntime.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
namespace
{
	Vector3 Add(const Vector3& a, const Vector3& b)
	{
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	float Distance(const Vector3& a, const Vector3& b)
	{
		const float x = a.x - b.x;
		const float y = a.y - b.y;
		const float z = a.z - b.z;
		return std::sqrt(x * x + y * y + z * z);
	}
}

VfxGraphRuntime* VfxGraphRuntime::GetInstance()
{
	static VfxGraphRuntime instance;
	return &instance;
}

void VfxGraphRuntime::BeginFrame()
{
	stats_.graphStartCostThisFrame = 0u;
}

void VfxGraphRuntime::UpdateScalability()
{
	for (auto& [particleHandleId, state] : activeLoops_)
	{
		(void)particleHandleId;
		const VfxGraphProgram* program = GetProgram(state.handle.graphName);
		if (program == nullptr) continue;

		bool culled = false;
		float distance = 0.0f;
		const float scale = EvaluateRuntimeScale(*program, state.worldPosition, culled, distance);
		if (std::abs(scale - state.runtimeScale) > 0.001f)
		{
			if (GpuParticleEffectRuntime::GetInstance()->SetLoopRuntimeScale(state.handle.particleHandle, scale))
			{
				if (state.handle.integrationHandle.IsValid())
				{
					VfxCueRuntime::GetInstance()->SetRuntimeScale(state.handle.integrationHandle, scale);
				}
				++stats_.loopScaleChanges;
				RecordLodSelection(*program, distance, scale);
			}
		}
		if (culled != state.culled)
		{
			++stats_.loopCullTransitions;
		}
		state.runtimeScale = scale;
		state.culled = culled;
	}
	RefreshLoopStats();
}

bool VfxGraphRuntime::RegisterGraph(const VfxGraphDesc& graph)
{
	VfxGraphCompileResult compiled = VfxGraphCompiler::Compile(graph);
	if (!compiled.success)
	{
		++stats_.compileFailures;
		SetStatus(false, compiled.errors.empty() ? "VFX Graph compile failed." : compiled.errors.front());
		return false;
	}

	StopActiveLoopsForGraph(graph.graphName);
	if (!GpuParticleEffectRuntime::GetInstance()->RegisterEffect(compiled.program.particleEffect))
	{
		SetStatus(false, "Phase13 GPU Particle Runtime rejected graph: " + graph.graphName);
		return false;
	}

	VfxCueRuntime* cueRuntime = VfxCueRuntime::GetInstance();
	const auto previous = programs_.find(graph.graphName);
	if (compiled.program.HasIntegrationTracks())
	{
		if (!cueRuntime->RegisterCue(compiled.program.integrationOneShotCue) || !cueRuntime->RegisterCue(compiled.program.integrationLoopCue))
		{
			++stats_.integrationFailures;
			SetStatus(false, "Phase18 VFX Runtime rejected graph integrations: " + graph.graphName);
			return false;
		}
	}
	else if (previous != programs_.end() && previous->second.HasIntegrationTracks())
	{
		cueRuntime->UnregisterCue(previous->second.integrationOneShotCue.cueName);
		cueRuntime->UnregisterCue(previous->second.integrationLoopCue.cueName);
	}

	const bool replacing = previous != programs_.end();
	programs_[graph.graphName] = std::move(compiled.program);
	if (!replacing) ++stats_.registeredGraphs;
	SetStatus(true, "Registered VFX Graph: " + graph.graphName);
	return true;
}

bool VfxGraphRuntime::LoadGraph(const std::string& filePath)
{
	VfxGraphDesc graph{};
	if (!VfxGraphSerializer::Load(graph, filePath))
	{
		SetStatus(false, "Failed to load VFX Graph: " + filePath);
		return false;
	}
	const std::string graphName = graph.graphName;
	if (!RegisterGraph(graph)) return false;
	sourcePaths_[graphName] = filePath;
	SetStatus(true, "Loaded VFX Graph: " + graphName);
	return true;
}

bool VfxGraphRuntime::ReloadGraph(const std::string& graphName)
{
	const auto it = sourcePaths_.find(graphName);
	if (it == sourcePaths_.end())
	{
		SetStatus(false, "Reload failed because source path is not registered: " + graphName);
		return false;
	}
	const std::string path = it->second;
	if (!LoadGraph(path)) return false;
	++stats_.reloads;
	SetStatus(true, "Reloaded VFX Graph: " + graphName);
	return true;
}

bool VfxGraphRuntime::Play(const std::string& graphName, const Vector3& worldPosition)
{
	++stats_.playRequests;
	const VfxGraphProgram* program = GetProgram(graphName);
	if (program == nullptr)
	{
		SetStatus(false, "Play failed because graph is not registered: " + graphName);
		return false;
	}
	if (!ReserveStartBudget(*program, false)) return false;

	bool culled = false;
	float distance = 0.0f;
	const float runtimeScale = EvaluateRuntimeScale(*program, worldPosition, culled, distance);
	if (culled)
	{
		++stats_.culledOneShots;
		SetStatus(false, "VFX Graph one-shot culled by Phase27 visibility policy: " + graphName);
		return false;
	}
	RecordLodSelection(*program, distance, runtimeScale);

	VfxCueHandle integrationHandle{};
	if (program->HasIntegrationTracks())
	{
		integrationHandle = VfxCueRuntime::GetInstance()->Play(program->integrationOneShotCue.cueName, worldPosition);
		if (!integrationHandle.IsValid() || !VfxCueRuntime::GetInstance()->SetRuntimeScale(integrationHandle, runtimeScale))
		{
			if (integrationHandle.IsValid()) VfxCueRuntime::GetInstance()->Stop(integrationHandle);
			++stats_.integrationFailures;
			SetStatus(false, "Phase18 runtime failed to play graph integrations: " + graphName);
			return false;
		}
		++stats_.integrationStarts;
	}

	const bool success = GpuParticleEffectRuntime::GetInstance()->Play(graphName, worldPosition, runtimeScale);
	if (!success && integrationHandle.IsValid())
	{
		VfxCueRuntime::GetInstance()->Stop(integrationHandle);
		++stats_.integrationStops;
	}
	if (success) ++stats_.playSuccesses;
	SetStatus(success, success ? "Played VFX Graph: " + graphName : "Phase13 runtime failed to play graph: " + graphName);
	return success;
}

VfxGraphPlayHandle VfxGraphRuntime::PlayLoop(const std::string& graphName, const Vector3& worldPosition)
{
	const VfxGraphProgram* program = GetProgram(graphName);
	if (program == nullptr)
	{
		SetStatus(false, "PlayLoop failed because graph is not registered: " + graphName);
		return {};
	}
	if (!ReserveStartBudget(*program, true)) return {};

	bool culled = false;
	float distance = 0.0f;
	const float runtimeScale = EvaluateRuntimeScale(*program, worldPosition, culled, distance);
	RecordLodSelection(*program, distance, runtimeScale);

	const GpuParticleEffectRuntime::PlayHandle particleHandle = GpuParticleEffectRuntime::GetInstance()->PlayLoop(graphName, worldPosition, runtimeScale);
	if (!particleHandle.IsValid())
	{
		SetStatus(false, "Phase13 runtime failed to start loop graph: " + graphName);
		return {};
	}

	VfxCueHandle integrationHandle{};
	if (program->HasIntegrationTracks())
	{
		integrationHandle = VfxCueRuntime::GetInstance()->Play(program->integrationLoopCue.cueName, worldPosition);
		if (!integrationHandle.IsValid() || !VfxCueRuntime::GetInstance()->SetRuntimeScale(integrationHandle, runtimeScale))
		{
			GpuParticleEffectRuntime::GetInstance()->StopLoop(particleHandle);
			if (integrationHandle.IsValid()) VfxCueRuntime::GetInstance()->Stop(integrationHandle);
			++stats_.integrationFailures;
			SetStatus(false, "Phase18 runtime failed to start loop graph integrations: " + graphName);
			return {};
		}
		++stats_.integrationStarts;
	}

	VfxGraphPlayHandle handle{ particleHandle, integrationHandle, graphName };
	activeLoops_[particleHandle.id] = { handle, worldPosition, runtimeScale, culled, program->scalability.budgetCost };
	++stats_.loopStarts;
	RefreshLoopStats();
	SetStatus(true, "Looped VFX Graph: " + graphName);
	return handle;
}

bool VfxGraphRuntime::StopLoop(VfxGraphPlayHandle handle)
{
	if (!handle.IsValid())
	{
		SetStatus(false, "StopLoop received invalid graph handle.");
		return false;
	}
	const bool particleSuccess = GpuParticleEffectRuntime::GetInstance()->StopLoop(handle.particleHandle);
	bool integrationSuccess = true;
	if (handle.integrationHandle.IsValid())
	{
		integrationSuccess = VfxCueRuntime::GetInstance()->Stop(handle.integrationHandle);
		if (integrationSuccess) ++stats_.integrationStops;
		else ++stats_.integrationFailures;
	}
	activeLoops_.erase(handle.particleHandle.id);
	const bool success = particleSuccess && integrationSuccess;
	if (success) ++stats_.loopStops;
	RefreshLoopStats();
	SetStatus(success, success ? "Stopped VFX Graph loop: " + handle.graphName : "Failed to stop VFX Graph loop: " + handle.graphName);
	return success;
}

bool VfxGraphRuntime::SetLoopPosition(VfxGraphPlayHandle handle, const Vector3& worldPosition)
{
	if (!handle.IsValid()) return false;
	const bool particleSuccess = GpuParticleEffectRuntime::GetInstance()->SetLoopPosition(handle.particleHandle, worldPosition);
	const bool integrationSuccess = !handle.integrationHandle.IsValid() || VfxCueRuntime::GetInstance()->SetWorldPosition(handle.integrationHandle, worldPosition);
	const auto it = activeLoops_.find(handle.particleHandle.id);
	if (it != activeLoops_.end()) it->second.worldPosition = worldPosition;
	return particleSuccess && integrationSuccess;
}

bool VfxGraphRuntime::SetFloatParameter(const std::string& graphName, const std::string& parameterName, float value)
{
	if (!IsRegistered(graphName)) return false;
	return GpuParticleEffectRuntime::GetInstance()->SetFloatParameter(graphName, parameterName, value);
}

bool VfxGraphRuntime::SetFloatParameter(VfxGraphPlayHandle handle, const std::string& parameterName, float value)
{
	if (!handle.IsValid()) return false;
	const bool particleSuccess = GpuParticleEffectRuntime::GetInstance()->SetFloatParameter(handle.particleHandle, parameterName, value);
	const bool integrationSuccess = !handle.integrationHandle.IsValid() || VfxCueRuntime::GetInstance()->SetFloatParameter(handle.integrationHandle, parameterName, value);
	return particleSuccess && integrationSuccess;
}

bool VfxGraphRuntime::IsRegistered(const std::string& graphName) const
{
	return programs_.contains(graphName);
}

const VfxGraphProgram* VfxGraphRuntime::GetProgram(const std::string& graphName) const
{
	const auto it = programs_.find(graphName);
	return it == programs_.end() ? nullptr : &it->second;
}

float VfxGraphRuntime::EvaluateRuntimeScale(const VfxGraphProgram& program, const Vector3& worldPosition, bool& outCulled, float& outDistance) const
{
	outCulled = false;
	outDistance = 0.0f;
	CameraManager* cameraManager = CameraManager::GetInstance();
	if (cameraManager->GetMainCamera() == nullptr && !cameraManager->HasRenderViewOverride()) return 1.0f;

	BoundingSphere worldBounds = program.localBounds;
	worldBounds.center = Add(worldPosition, program.localBounds.center);
	const Vector3 cameraPosition = cameraManager->GetActiveCameraPosition();
	outDistance = Distance(cameraPosition, worldBounds.center);

	if (program.scalability.maxDrawDistance > 0.0f && outDistance - worldBounds.radius > program.scalability.maxDrawDistance)
	{
		outCulled = true;
		return 0.0f;
	}
	if (program.scalability.frustumCulling)
	{
		Frustum frustum{};
		frustum.BuildFromViewProjection(cameraManager->GetActiveViewProjectionMatrix());
		if (!frustum.Intersects(worldBounds))
		{
			outCulled = true;
			return 0.0f;
		}
	}

	if (outDistance <= program.scalability.lodNearDistance) return 1.0f;
	if (outDistance <= program.scalability.lodFarDistance) return program.scalability.lodMidScale;
	return program.scalability.lodFarScale;
}

bool VfxGraphRuntime::ReserveStartBudget(const VfxGraphProgram& program, bool loopStart)
{
	const VfxRuntimeBudget& budget = VfxCueRuntime::GetInstance()->GetBudget();
	const uint32_t cost = (std::max)(program.scalability.budgetCost, 1u);
	if (stats_.graphStartCostThisFrame + cost > budget.maxVfxGraphStartCostPerFrame)
	{
		++stats_.budgetRejectedPlays;
		SetStatus(false, "VFX Graph rejected by per-frame start budget: " + program.graphName);
		return false;
	}
	if (loopStart && stats_.activeLoopCost + cost > budget.maxActiveVfxGraphLoopCost)
	{
		++stats_.budgetRejectedPlays;
		SetStatus(false, "VFX Graph loop rejected by active graph budget: " + program.graphName);
		return false;
	}
	stats_.graphStartCostThisFrame += cost;
	return true;
}

void VfxGraphRuntime::RecordLodSelection(const VfxGraphProgram& program, float distance, float scale)
{
	if (scale <= 0.0f) return;
	if (distance <= program.scalability.lodNearDistance) ++stats_.lodNearSelections;
	else if (distance <= program.scalability.lodFarDistance) ++stats_.lodMidSelections;
	else ++stats_.lodFarSelections;
}

void VfxGraphRuntime::RefreshLoopStats()
{
	stats_.activeLoopCount = static_cast<uint32_t>(activeLoops_.size());
	stats_.activeLoopCost = 0u;
	for (const auto& [id, state] : activeLoops_)
	{
		(void)id;
		stats_.activeLoopCost += state.budgetCost;
	}
}

void VfxGraphRuntime::StopActiveLoopsForGraph(const std::string& graphName)
{
	std::vector<VfxGraphPlayHandle> handles;
	for (const auto& [id, state] : activeLoops_)
	{
		(void)id;
		if (state.handle.graphName == graphName) handles.push_back(state.handle);
	}
	for (const VfxGraphPlayHandle& handle : handles) StopLoop(handle);
}

void VfxGraphRuntime::SetStatus(bool success, std::string message)
{
	lastOperationSucceeded_ = success;
	lastStatus_ = std::move(message);
}

} // namespace Ken4lowEngine
''', encoding='utf-8')

# -----------------------------------------------------------------------------
# Frame integration: reset Graph budget before gameplay and update LOD after camera/scene.
# -----------------------------------------------------------------------------
replace_once(
    'Engine/Core/Application/GameApplication.cpp',
    '''#include "Engine/Vfx/Runtime/VfxCueRuntime.h"\n''',
    '''#include "Engine/Vfx/Runtime/VfxCueRuntime.h"\n#include "Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h"\n''')
replace_once(
    'Engine/Core/Application/GameApplication.cpp',
    '''\t\tVfxCueRuntime::GetInstance()->BeginFrame();\n\n\t\tif (defaultCamera_)\n''',
    '''\t\tVfxCueRuntime::GetInstance()->BeginFrame();\n\t\tVfxGraphRuntime::GetInstance()->BeginFrame();\n\n\t\tif (defaultCamera_)\n''')
replace_once(
    'Engine/Core/Application/GameApplication.cpp',
    '''\t\tVfxCueRuntime::GetInstance()->Update(GameTimer::GetInstance()->GetDeltaTime(), actorWorld);\n''',
    '''\t\tVfxGraphRuntime::GetInstance()->UpdateScalability();\n\t\tVfxCueRuntime::GetInstance()->Update(GameTimer::GetInstance()->GetDeltaTime(), actorWorld);\n''')

# -----------------------------------------------------------------------------
# Editor authoring controls for Phase27 scalability.
# -----------------------------------------------------------------------------
replace_once(
    'Engine/Vfx/Graph/Editor/VfxGraphEditor.cpp',
    '''\tImGui::SameLine();\n\tImGui::Text("Emitters: %d", static_cast<int>(editableGraph_.emitters.size()));\n\tImGui::Separator();\n''',
    '''\tImGui::SameLine();\n\tImGui::Text("Emitters: %d", static_cast<int>(editableGraph_.emitters.size()));\n\tif (ImGui::TreeNode("Phase27 Scalability"))\n\t{\n\t\tauto& scalability = editableGraph_.scalability;\n\t\tconst char* boundsModes[] = { "Automatic", "FixedSphere" };\n\t\tint boundsMode = static_cast<int>(scalability.boundsMode);\n\t\tif (ImGui::Combo("Bounds Mode", &boundsMode, boundsModes, IM_ARRAYSIZE(boundsModes))) { scalability.boundsMode = static_cast<VfxGraphBoundsMode>(boundsMode); MarkGraphDirty(); }\n\t\tif (scalability.boundsMode == VfxGraphBoundsMode::FixedSphere)\n\t\t{\n\t\t\tif (ImGui::DragFloat3("Bounds Center", &scalability.fixedBoundsCenter.x, 0.05f)) MarkGraphDirty();\n\t\t\tif (ImGui::DragFloat("Bounds Radius", &scalability.fixedBoundsRadius, 0.05f, 0.1f, 10000.0f)) MarkGraphDirty();\n\t\t}\n\t\tif (ImGui::Checkbox("Frustum Culling", &scalability.frustumCulling)) MarkGraphDirty();\n\t\tif (ImGui::DragFloat("Max Draw Distance", &scalability.maxDrawDistance, 1.0f, 0.0f, 100000.0f)) MarkGraphDirty();\n\t\tif (ImGui::DragFloat("LOD Near", &scalability.lodNearDistance, 0.5f, 0.0f, 100000.0f)) MarkGraphDirty();\n\t\tif (ImGui::DragFloat("LOD Far", &scalability.lodFarDistance, 0.5f, 0.0f, 100000.0f)) MarkGraphDirty();\n\t\tif (ImGui::SliderFloat("LOD Mid Scale", &scalability.lodMidScale, 0.05f, 1.0f)) MarkGraphDirty();\n\t\tif (ImGui::SliderFloat("LOD Far Scale", &scalability.lodFarScale, 0.01f, 1.0f)) MarkGraphDirty();\n\t\tint budgetCost = static_cast<int>(scalability.budgetCost);\n\t\tif (ImGui::DragInt("Budget Cost", &budgetCost, 1.0f, 1, 64)) { scalability.budgetCost = static_cast<uint32_t>((std::max)(budgetCost, 1)); MarkGraphDirty(); }\n\t\tImGui::TreePop();\n\t}\n\tImGui::Separator();\n''')

print('Phase27 patch applied successfully.')
