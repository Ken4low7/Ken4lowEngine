from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SHADER_ROOT = ROOT / "Resources/Shaders/GpuFluid/Sph"
KERNEL = SHADER_ROOT / "GpuSphKernel.hlsli"
PRESSURE = SHADER_ROOT / "GpuSphPressure.CS.hlsl"
VISCOSITY = SHADER_ROOT / "GpuSphViscosity.CS.hlsl"
DENSITY = SHADER_ROOT / "GpuSphDensity.CS.hlsl"


class W7ThreeDimensionalSphLiquidTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.kernel = KERNEL.read_text(encoding="utf-8")
        cls.pressure = PRESSURE.read_text(encoding="utf-8")
        cls.viscosity = VISCOSITY.read_text(encoding="utf-8")
        cls.density = DENSITY.read_text(encoding="utf-8")

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

    def test_surface_tension_reuses_w6_27_cell_neighbor_search(self) -> None:
        self.assertIn("kGpuSphSurfaceTension = 0.0728f", self.kernel)
        self.assertIn("GpuSphCohesionWeight", self.kernel)
        self.assertIn("cohesionAcceleration", self.viscosity)
        self.assertIn("GpuSphGetCellRange", self.viscosity)
        for axis in ("for (int z = -1; z <= 1; ++z)", "for (int y = -1; y <= 1; ++y)", "for (int x = -1; x <= 1; ++x)"):
            self.assertIn(axis, self.viscosity)

    def test_xsph_smoothing_is_accumulated_before_velocity_write(self) -> None:
        self.assertIn("kGpuSphXsphStrength = 0.025f", self.kernel)
        self.assertIn("float3 xsphDelta", self.viscosity)
        self.assertIn("GpuSphPoly6Kernel", self.viscosity)
        self.assertIn("xsphDelta * kGpuSphXsphStrength", self.viscosity)
        self.assertIn("gScratch[index]", self.viscosity)

    def test_cfl_velocity_limit_is_gpu_local_and_step_relative(self) -> None:
        self.assertIn("kGpuSphCflNumber = 0.4f", self.kernel)
        self.assertIn("GpuSphClampVelocityByCfl", self.kernel)
        self.assertIn("smoothingRadius / safeDeltaTime", self.kernel)
        self.assertIn("GpuSphClampVelocityByCfl", self.viscosity)

    def test_w7_keeps_w6_spatial_hash_in_all_neighbor_passes(self) -> None:
        for shader in (self.density, self.pressure, self.viscosity):
            self.assertIn("GpuSphGetCellRange", shader)
            self.assertNotIn("neighborIndex < gSph.activeParticleCount", shader)

    def test_w7_does_not_add_particle_buffer_or_root_signature_contracts(self) -> None:
        # W7 stability features run inside existing Pressure/Viscosity passes so W8 can consume the same 48-byte particles.
        self.assertNotIn("RWStructuredBuffer", self.kernel)
        self.assertNotIn("register(", self.kernel)


if __name__ == "__main__":
    unittest.main()
