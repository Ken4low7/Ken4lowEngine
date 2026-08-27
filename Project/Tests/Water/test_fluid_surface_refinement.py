from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SHADER_ROOT = ROOT / "Resources/Shaders/GpuFluid/Sph"
SCREEN_ROOT = SHADER_ROOT / "ScreenSpace"
PARTICLE_VS = SCREEN_ROOT / "GpuSphScreenSpaceParticle.VS.hlsl"
DEPTH_PS = SCREEN_ROOT / "GpuSphScreenSpaceDepth.PS.hlsl"
THICKNESS_PS = SCREEN_ROOT / "GpuSphScreenSpaceThickness.PS.hlsl"
BLUR_PS = SCREEN_ROOT / "GpuSphScreenSpaceBlur.PS.hlsl"
COMPOSITE_PS = SCREEN_ROOT / "GpuSphScreenSpaceComposite.PS.hlsl"
RESET_CS = SHADER_ROOT / "GpuSphReset.CS.hlsl"


class FluidSurfaceRefinementTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.particle_vs = PARTICLE_VS.read_text(encoding="utf-8")
        cls.depth_ps = DEPTH_PS.read_text(encoding="utf-8")
        cls.thickness_ps = THICKNESS_PS.read_text(encoding="utf-8")
        cls.blur_ps = BLUR_PS.read_text(encoding="utf-8")
        cls.composite_ps = COMPOSITE_PS.read_text(encoding="utf-8")
        cls.reset_cs = RESET_CS.read_text(encoding="utf-8")

    def test_velocity_anisotropic_splat_stretches_only_the_render_surface(self) -> None:
        self.assertIn("particle.velocity", self.particle_vs)
        self.assertIn("viewVelocity", self.particle_vs)
        self.assertIn("targetStretch", self.particle_vs)
        self.assertIn("transverseScale", self.particle_vs)
        self.assertIn("viewOffset", self.particle_vs)
        self.assertIn("input.viewOffset", self.depth_ps)
        self.assertIn("input.viewOffset", self.thickness_ps)

    def test_surface_filter_uses_wider_bilateral_and_curvature_relaxation(self) -> None:
        self.assertIn("offset = -6", self.blur_ps)
        self.assertIn("offset <= 6", self.blur_ps)
        self.assertIn("rangeWeight", self.blur_ps)
        self.assertIn("curvatureTarget", self.blur_ps)
        self.assertIn("curvatureDelta", self.blur_ps)
        self.assertIn("maxRelaxation", self.blur_ps)

    def test_normal_reconstruction_prefers_the_lower_depth_discontinuity(self) -> None:
        self.assertIn("leftDepth", self.composite_ps)
        self.assertIn("rightDepth", self.composite_ps)
        self.assertIn("upDepth", self.composite_ps)
        self.assertIn("downDepth", self.composite_ps)
        self.assertIn("SafeNormalFromDepth", self.composite_ps)
        self.assertIn("abs(rightDepth - centerDepth)", self.composite_ps)

    def test_water_optics_use_rgb_transmittance_reflection_and_foam_foundation(self) -> None:
        self.assertIn("transmittance", self.composite_ps)
        self.assertIn("exp(-extinction", self.composite_ps)
        self.assertIn("environmentReflection", self.composite_ps)
        self.assertIn("specular", self.composite_ps)
        self.assertIn("ComputeFoam", self.composite_ps)
        self.assertIn("depthCurvature", self.composite_ps)
        self.assertIn("thicknessGradient", self.composite_ps)

    def test_reset_keeps_grid_contract_and_adds_deterministic_jitter(self) -> None:
        self.assertIn("particle.position = gSph.spawnOrigin + float3(x, y, z) * gSph.spawnSpacing", self.reset_cs)
        self.assertIn("ParticleHash", self.reset_cs)
        self.assertIn("ParticleHash01", self.reset_cs)
        self.assertIn("gSph.spawnSpacing * 0.08f", self.reset_cs)
        self.assertIn("particle.predictedPosition = particle.position", self.reset_cs)  # 初期配置の再現性と補助位置の同期を維持する。


if __name__ == "__main__":
    unittest.main()
