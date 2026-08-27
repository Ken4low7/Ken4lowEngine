import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_curve_and_gradient_authoring_types_are_bounded():
    types = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    assert "enum class VfxCurveInterpolation" in types
    assert "VfxFloatCurveKey" in types
    assert "VfxFloatCurve" in types
    assert "VfxColorGradientKey" in types
    assert "VfxColorGradient" in types
    assert "kMaxCurveKeys = 32u" in types
    assert "kMaxGradientKeys = 32u" in types


def test_curve_and_gradient_evaluators_support_three_interpolation_modes():
    types = read("Engine/Vfx/Graph/Data/VfxGraphTypes.cpp")
    for mode in ("Linear", "Step", "SmoothStep"):
        assert f"VfxCurveInterpolation::{mode}" in types
    assert "VfxFloatCurve::Evaluate" in types
    assert "VfxColorGradient::Evaluate" in types
    assert "ApplyCurveInterpolation" in types
    assert "LerpColor" in types


def test_particle_modules_have_explicit_graph_payloads_and_stages():
    header = read("Engine/Vfx/Graph/Data/VfxGraphTypes.h")
    cpp = read("Engine/Vfx/Graph/Data/VfxGraphTypes.cpp")
    for module in (
        "VfxGraphInitialRotationNode",
        "VfxGraphRotationRateNode",
        "VfxGraphSizeOverLifeNode",
        "VfxGraphColorOverLifeNode",
    ):
        assert module in header
    assert "VfxGraphNodeType::InitialRotation" in cpp
    assert "VfxGraphNodeStage::Initialize" in cpp
    for update_module in ("RotationRate", "SizeOverLife", "ColorOverLife"):
        assert f"VfxGraphNodeType::{update_module}" in cpp
    assert "VfxGraphNodeStage::Update" in cpp


def test_serializer_round_trips_curve_gradient_and_rotation_modules():
    serializer = read("Engine/Vfx/Graph/Asset/VfxGraphSerializer.cpp")
    for symbol in (
        "ReadFloatCurve",
        "WriteFloatCurve",
        "ReadColorGradient",
        "WriteColorGradient",
        "TryParseCurveInterpolation",
        "CurveInterpolationToString",
    ):
        assert symbol in serializer
    for key in ('"curve"', '"gradient"', '"interpolation"', '"keys"', '"time"', '"color"'):
        assert key in serializer
    assert '"radiansPerSecond"' in serializer


def test_compiler_validates_authoring_keys_before_gpu_bake():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    assert "ValidateFloatCurve" in compiler
    assert "ValidateColorGradient" in compiler
    assert "curve key time must be in [0, 1]" in compiler
    assert "curve key times must be strictly increasing" in compiler
    assert "gradient key time must be in [0, 1]" in compiler
    assert "gradient key times must be strictly increasing" in compiler
    assert "kMaxCurveKeys" in compiler
    assert "kMaxGradientKeys" in compiler


def test_compiler_bakes_authoring_curves_to_gpu_luts():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    backend = read("Engine/Graphics/Renderer/GpuParticle/Data/GpuParticleEffectDesc.h")
    shader = read("Resources/Shaders/GpuParticle/GpuParticleUpdate.CS.hlsl")
    assert "outEmitter.useSizeCurve = true" in compiler
    assert "outEmitter.sizeCurveLut" in compiler
    assert "curve.Evaluate(1.0f / 3.0f)" in compiler
    assert "outEmitter.useColorGradient = true" in compiler
    assert "outEmitter.colorGradientLut" in compiler
    assert "gradient.Evaluate(2.0f / 3.0f)" in compiler
    assert "Vector4 sizeCurveLut" in backend
    assert "std::array<Vector4, 4> colorGradientLut" in backend
    assert "SampleScalarLut" in shader
    assert "SampleColorGradient" in shader


def test_rotation_modules_lower_to_sprite_rotation_backend():
    compiler = read("Engine/Vfx/Graph/Runtime/VfxGraphCompiler.cpp")
    backend = read("Engine/Graphics/Renderer/GpuParticle/Data/GpuParticleEffectDesc.h")
    shader = read("Resources/Shaders/GpuParticle/GpuParticleUpdate.CS.hlsl")
    assert "outEmitter.startRotation" in compiler
    assert "outEmitter.rotationRandom" in compiler
    assert "outEmitter.rotationSpeed" in compiler
    assert "float startRotation" in backend
    assert "float rotationSpeed" in backend
    assert "p.rotation += p.rotationSpeed * dt" in shader


def test_curve_gradient_sample_exercises_rotation_and_lifetime_modules():
    sample_path = ROOT / "Resources/VfxGraph/Samples/CurveGradientBurst.vfxgraph.json"
    sample = json.loads(sample_path.read_text(encoding="utf-8"))
    assert sample["schemaVersion"] == 1
    emitter = sample["emitters"][0]
    nodes = {node["type"]: node for node in emitter["nodes"]}
    for node_type in ("InitialRotation", "RotationRate", "SizeOverLife", "ColorOverLife"):
        assert node_type in nodes
    assert nodes["SizeOverLife"]["params"]["curve"]["interpolation"] == "SmoothStep"
    assert len(nodes["SizeOverLife"]["params"]["curve"]["keys"]) == 4
    assert len(nodes["ColorOverLife"]["params"]["gradient"]["keys"]) == 4
    assert len(emitter["edges"]) == len(emitter["nodes"]) - 1
