from pathlib import Path
import unittest


PROJECT = Path(__file__).resolve().parents[2]
COMPONENT_H = PROJECT / "Engine/Scene/Actor/Components/BladeTrailComponent.h"
COMPONENT_CPP = PROJECT / "Engine/Scene/Actor/Components/BladeTrailComponent.cpp"
RENDERER_H = PROJECT / "Engine/Graphics/Renderer/BladeTrail/BladeTrailRenderer.h"
RENDERER_CPP = PROJECT / "Engine/Graphics/Renderer/BladeTrail/BladeTrailRenderer.cpp"
MANIFEST = PROJECT / "Engine/Graphics/Shader/Manifest/BladeTrailShaderManifest.h"
VS = PROJECT / "Resources/Shaders/BladeTrail/BladeTrail.VS.hlsl"
PS = PROJECT / "Resources/Shaders/BladeTrail/BladeTrail.PS.hlsl"
FACTORY = PROJECT / "Engine/Scene/Actor/Serialization/ComponentFactory.cpp"
BUILD_PROPS = PROJECT / "Directory.Build.props"
SHADER_TYPES = PROJECT / "Engine/Graphics/Shader/Manifest/ShaderManifestTypes.h"


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


class BladeTrailNPointRibbonContracts(unittest.TestCase):
    def test_component_owns_n_point_root_tip_history(self):
        header = text(COMPONENT_H)
        source = text(COMPONENT_CPP)
        self.assertIn("std::deque<BladeTrailSample> samples_", header)
        self.assertIn("Vector3 root", header)
        self.assertIn("Vector3 tip", header)
        self.assertIn("maxSamples_ = 32", header)
        self.assertIn("historyLifetime_ = 0.24f", header)
        self.assertIn("while (samples_.size() >= safeMaxSamples)", source)
        self.assertIn("while (!samples_.empty() && samples_.front().age >= safeLifetime)", source)

    def test_gameplay_samples_after_physics_and_supports_skeletal_world_points(self):
        header = text(COMPONENT_H)
        source = text(COMPONENT_CPP)
        self.assertIn("void PostPhysicsUpdate(float deltaTime) override", header)
        self.assertIn("void BladeTrailComponent::PostPhysicsUpdate", source)
        self.assertIn("SampleCurrentBlade();", source)
        self.assertIn("SetBladeWorldEndpoints", header)
        self.assertIn("useWorldEndpointOverride_", source)
        self.assertIn("Vector3::Transform(localRootOffset_, ownerWorld)", source)
        self.assertIn("Vector3::Transform(localTipOffset_, ownerWorld)", source)

    def test_history_is_smoothed_and_expanded_to_two_triangles_per_segment(self):
        source = text(COMPONENT_CPP)
        self.assertIn("Vector3::CatmullRomSpline", source)
        self.assertIn("smoothingSubdivisions_", source)
        self.assertIn("vertexScratch_.reserve((points.size() - 1u) * 6u)", source)
        self.assertGreaterEqual(source.count("vertexScratch_.push_back"), 6)

    def test_renderer_uses_shared_transient_upload_and_one_draw(self):
        header = text(RENDERER_H)
        source = text(RENDERER_CPP)
        self.assertIn("void Acquire()", header)
        self.assertIn("void Release()", header)
        self.assertIn("FrameUploadArena", source)
        self.assertIn("Allocate(vertexBytes", source)
        self.assertEqual(source.count("DrawInstanced("), 1)
        self.assertIn("D3D12_DEPTH_WRITE_MASK_ZERO", source)
        self.assertIn("D3D12_CULL_MODE_NONE", source)
        self.assertIn("kBlendModeAdd", source)
        self.assertIn("BindSceneRenderTarget", source)

    def test_shader_contract_is_registered_without_reordering_old_values(self):
        manifest = text(MANIFEST)
        shader_types = text(SHADER_TYPES)
        self.assertIn("BladeTrail.VS.hlsl", manifest)
        self.assertIn("BladeTrail.PS.hlsl", manifest)
        self.assertIn("GpuVolumetricFluid,\n\t\tBladeTrail", shader_types)
        self.assertIn("gViewProjection", text(VS))
        self.assertIn("gTexture.Sample", text(PS))

    def test_editor_factory_and_build_inputs_are_registered(self):
        factory = text(FACTORY)
        props = text(BUILD_PROPS)
        self.assertIn('#include "BladeTrailComponent.h"', factory)
        self.assertIn('MakeComponentTypeInfo<BladeTrailComponent>', factory)
        self.assertIn('"ブレードトレイル"', factory)
        self.assertIn('"演出"', factory)
        for required in (
            "BladeTrailRenderer.cpp",
            "BladeTrailComponent.cpp",
            "BladeTrailShaderManifest.h",
            "BladeTrail.VS.hlsl",
            "BladeTrail.PS.hlsl",
        ):
            self.assertIn(required, props)

    def test_inspector_and_serialization_keep_authoring_contract(self):
        source = text(COMPONENT_CPP)
        self.assertIn('ImGui::Button("Preview Arc")', source)
        self.assertIn('ImGui::DragFloat("Preview Duration"', source)
        self.assertIn('previewArcDuration_ = std::clamp(previewArcDuration_, 0.05f, 1.0f)', source)
        self.assertIn("ImGuiColorEditFlags_Uint8", source)
        for field in (
            '"LocalRootOffset"',
            '"LocalTipOffset"',
            '"HistoryLifetime"',
            '"MaxSamples"',
            '"SmoothingSubdivisions"',
            '"PreviewArcDuration"',
            '"HeadColor"',
            '"TailColor"',
            '"TexturePath"',
            '"BlendMode"',
        ):
            self.assertGreaterEqual(source.count(field), 2)

    def test_old_gpu_particle_trail_remains_a_separate_system(self):
        component = text(COMPONENT_H)
        renderer = text(RENDERER_CPP)
        self.assertNotIn("GpuParticleEmitter", component)
        self.assertNotIn("GpuParticleManager", component)
        self.assertNotIn("GpuParticleEmitter", renderer)
        self.assertNotIn("GpuParticleManager", renderer)


if __name__ == "__main__":
    unittest.main()
