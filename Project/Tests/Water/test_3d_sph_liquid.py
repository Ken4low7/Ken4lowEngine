from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SHADER_ROOT = ROOT / "Resources/Shaders/GpuFluid/Sph"
KERNEL = SHADER_ROOT / "GpuSphKernel.hlsli"
PRESSURE = SHADER_ROOT / "GpuSphPressure.CS.hlsl"
VISCOSITY = SHADER_ROOT / "GpuSphViscosity.CS.hlsl"
DENSITY = SHADER_ROOT / "GpuSphDensity.CS.hlsl"
COMMON = SHADER_ROOT / "GpuSphCommon.hlsli"


class ThreeDimensionalSphLiquidTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.kernel = KERNEL.read_text(encoding="utf-8")
        cls.pressure = PRESSURE.read_text(encoding="utf-8")
        cls.viscosity = VISCOSITY.read_text(encoding="utf-8")
        cls.density = DENSITY.read_text(encoding="utf-8")
        cls.common = COMMON.read_text(encoding="utf-8")

    def test_tait_eos_uses_standard_water_exponent_and_density_ratio(self) -> None:
        self.assertIn("kGpuSphTaitExponent = 7.0f", self.kernel)
        self.assertIn("GpuSphTaitPressure", self.kernel)
        self.assertIn("pow(densityRatio, kGpuSphTaitExponent)", self.kernel)
        self.assertIn("pressureStiffness", self.pressure)
        self.assertIn("GpuSphTaitPressure", self.pressure)

    def test_tensile_stabilization_only_modifies_negative_pressure(self) -> None:
        self.assertIn("kGpuSphTensileCorrectionStrength", self.kernel)
        self.assertIn("if (pressureTerm >= 0.0f)", self.kernel)
        self.assertIn("GpuSphApplyTensileCorrection", self.pressure)
        self.assertIn("pow(normalizedKernel, 4.0f)", self.kernel)

    def test_surface_tension_reuses_spatial_hash_neighbor_search(self) -> None:
        self.assertIn("surfaceTensionStrength", self.common)
        self.assertIn("GpuSphCohesionWeight", self.kernel)
        self.assertIn("cohesionAcceleration", self.viscosity)
        self.assertIn("gSph.surfaceTensionStrength", self.viscosity)
        self.assertIn("GpuSphGetCellRange", self.viscosity)
        for axis in ("for (int z = -1; z <= 1; ++z)", "for (int y = -1; y <= 1; ++y)", "for (int x = -1; x <= 1; ++x)"):
            self.assertIn(axis, self.viscosity)

    def test_xsph_smoothing_is_accumulated_before_velocity_write(self) -> None:
        self.assertIn("xsphStrength", self.common)
        self.assertIn("float3 xsphDelta", self.viscosity)
        self.assertIn("GpuSphPoly6Kernel", self.viscosity)
        self.assertIn("gSph.xsphStrength", self.viscosity)
        self.assertIn("gScratch[index]", self.viscosity)

    def test_cfl_velocity_limit_is_gpu_local_and_step_relative(self) -> None:
        self.assertIn("cflNumber", self.common)
        self.assertIn("GpuSphClampVelocityByCfl", self.kernel)
        self.assertIn("smoothingRadius / safeDeltaTime", self.kernel)
        self.assertIn("gSph.cflNumber", self.viscosity)

    def test_neighbor_passes_keep_spatial_hash_lookup(self) -> None:
        for shader in (self.density, self.pressure, self.viscosity):
            self.assertIn("GpuSphGetCellRange", shader)
            self.assertNotIn("neighborIndex < gSph.activeParticleCount", shader)

    def test_kernel_helpers_do_not_own_buffer_bindings(self) -> None:
        self.assertNotIn("RWStructuredBuffer", self.kernel)
        self.assertNotIn("register(", self.kernel)  # Kernel helperはBuffer bindingを持たず、粒子レイアウトから独立させる。


if __name__ == "__main__":
    unittest.main()
