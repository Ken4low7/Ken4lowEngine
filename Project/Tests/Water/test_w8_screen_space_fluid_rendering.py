from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
RENDERER = ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph/Renderer/GpuSphScreenSpaceFluidRenderer.h"
SPH_MANAGER_H = ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph/Manager/GpuSphManager.h"
POST_MANAGER_H = ROOT / "Engine/Graphics/PostEffect/Manager/PostEffectManager.h"
POST_MANAGER_CPP = ROOT / "Engine/Graphics/PostEffect/Manager/PostEffectManager.cpp"
SHADER_ROOT = ROOT / "Resources/Shaders/GpuFluid/Sph/ScreenSpace"
PARTICLE_VS = SHADER_ROOT / "GpuSphScreenSpaceParticle.VS.hlsl"
DEPTH_PS = SHADER_ROOT / "GpuSphScreenSpaceDepth.PS.hlsl"
THICKNESS_PS = SHADER_ROOT / "GpuSphScreenSpaceThickness.PS.hlsl"
BLUR_PS = SHADER_ROOT / "GpuSphScreenSpaceBlur.PS.hlsl"
COMPOSITE_PS = SHADER_ROOT / "GpuSphScreenSpaceComposite.PS.hlsl"


class W8ScreenSpaceFluidRenderingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.renderer = RENDERER.read_text(encoding="utf-8")
        cls.sph_manager_h = SPH_MANAGER_H.read_text(encoding="utf-8")
        cls.post_h = POST_MANAGER_H.read_text(encoding="utf-8")
        cls.post_cpp = POST_MANAGER_CPP.read_text(encoding="utf-8")
        cls.particle_vs = PARTICLE_VS.read_text(encoding="utf-8")
        cls.depth_ps = DEPTH_PS.read_text(encoding="utf-8")
        cls.thickness_ps = THICKNESS_PS.read_text(encoding="utf-8")
        cls.blur_ps = BLUR_PS.read_text(encoding="utf-8")
        cls.composite_ps = COMPOSITE_PS.read_text(encoding="utf-8")

    def test_w8_uses_existing_sph_particle_buffer_without_copying_particles(self) -> None:
        self.assertIn("GpuSphParticleBuffer& particleBuffer", self.renderer)
        self.assertIn("GetSrvIndex()", self.renderer)
        self.assertIn("D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE", self.renderer)
        self.assertIn("StructuredBuffer<GpuSphParticle> gParticles", self.particle_vs)

    def test_particle_depth_projects_billboard_spheres_and_writes_hardware_depth(self) -> None:
        self.assertIn("SV_InstanceID", self.particle_vs)
        self.assertIn("viewCenter", self.particle_vs)
        self.assertIn("sqrt(max(1.0f - radiusSquared", self.depth_ps)
        self.assertIn("SV_Depth", self.depth_ps)
        self.assertIn("D3D12_BLEND_OP_MIN", self.renderer)
        self.assertIn("DXGI_FORMAT_R32_FLOAT", self.renderer)

    def test_thickness_is_accumulated_additively(self) -> None:
        self.assertIn("2.0f * sphereZ * radius", self.thickness_ps)
        self.assertIn("D3D12_BLEND_OP_ADD", self.renderer)
        self.assertIn("DXGI_FORMAT_R16_FLOAT", self.renderer)

    def test_depth_is_smoothed_with_two_pass_bilateral_blur(self) -> None:
        self.assertIn("DrawBlur(commandList, depthRaw_, depthPing_", self.renderer)
        self.assertIn("DrawBlur(commandList, depthPing_, depthSmooth_", self.renderer)
        self.assertIn("depthFalloff", self.blur_ps)
        self.assertIn("rangeWeight", self.blur_ps)
        self.assertIn("spatialWeight", self.blur_ps)

    def test_composite_reconstructs_normal_and_uses_scene_refraction(self) -> None:
        self.assertIn("GpuSphReconstructViewPosition", self.composite_ps)
        self.assertIn("SafeNormalFromDepth", self.composite_ps)
        self.assertIn("gSceneColor", self.composite_ps)
        self.assertIn("refractedUv", self.composite_ps)
        self.assertIn("fresnel", self.composite_ps)
        self.assertIn("specular", self.composite_ps)

    def test_scene_color_is_copied_before_composite_to_avoid_rtv_srv_aliasing(self) -> None:
        self.assertIn("GetGameRenderTargetResource", self.post_h)
        self.assertIn("GetGameRenderTargetState", self.post_h)
        self.assertIn("SetGameRenderTargetState", self.post_h)
        self.assertIn("CopyResource(sceneCopy_.Get(), sceneResource)", self.renderer)
        self.assertIn("D3D12_RESOURCE_STATE_COPY_SOURCE", self.renderer)
        self.assertIn("D3D12_RESOURCE_STATE_COPY_DEST", self.renderer)
        self.assertIn("sceneCopySrvIndex_", self.renderer)

    def test_w8_runs_after_scene_draw_and_before_post_effect_transition(self) -> None:
        start = self.post_cpp.index("void PostEffectManager::EndDraw()")
        end = self.post_cpp.index("void PostEffectManager::Resize", start)
        body = self.post_cpp[start:end]
        fluid_draw = body.index("GpuSphScreenSpaceFluidRenderer::GetInstance()->Draw")
        executor_end = body.index("executor_->EndDraw()")
        self.assertLess(fluid_draw, executor_end)

    def test_scene_depth_dsv_is_reused_for_opaque_occlusion(self) -> None:
        self.assertIn("GetSceneDsvHandle", self.post_h)
        self.assertIn("GetSceneDsvHandle", self.renderer)
        self.assertIn("D3D12_COMPARISON_FUNC_LESS_EQUAL", self.renderer)
        self.assertIn("D3D12_DEPTH_WRITE_MASK_ZERO", self.renderer)

    def test_w8_render_targets_define_matching_optimized_clear_values(self) -> None:
        self.assertIn("kDepthClearValue = 1000000.0f", self.renderer)
        self.assertIn("kThicknessClearValue = 0.0f", self.renderer)
        self.assertIn("D3D12_CLEAR_VALUE optimizedClear", self.renderer)
        self.assertIn("&optimizedClear", self.renderer)
        self.assertIn("const float clearValue[4] = { kDepthClearValue", self.renderer)
        self.assertIn("const float clearValue[4] = { kThicknessClearValue", self.renderer)

    def test_water_defaults_reduce_boundary_rebound(self) -> None:
        self.assertIn("float boundaryDamping = 0.05f;", self.sph_manager_h)

    def test_w8_visual_tuning_window_exposes_surface_controls_and_stats(self) -> None:
        self.assertIn('ImGui::Begin("W8 Screen Space Fluid")', self.post_cpp)
        self.assertIn('"Particle Radius"', self.post_cpp)
        self.assertIn('"Blur Depth Falloff"', self.post_cpp)
        self.assertIn('"Refraction Strength"', self.post_cpp)
        self.assertIn('"Water Visual Preset"', self.post_cpp)
        self.assertIn('"W8 Diagnostics"', self.post_cpp)
        self.assertIn("particleDepthDrawCount", self.post_cpp)
        self.assertIn("compositeDrawCount", self.post_cpp)


if __name__ == "__main__":
    unittest.main()
