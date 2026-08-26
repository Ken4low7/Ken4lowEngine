from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPH_ROOT = ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph"
SHADER_ROOT = ROOT / "Resources/Shaders/GpuFluid/Sph"
MANAGER_H = SPH_ROOT / "Manager/GpuSphManager.h"
MANAGER_CPP = SPH_ROOT / "Manager/GpuSphManager.cpp"
MANIFEST = ROOT / "Engine/Graphics/Shader/Manifest/GpuSphShaderManifest.h"
COMMON = SHADER_ROOT / "GpuSphCommon.hlsli"
KERNEL = SHADER_ROOT / "GpuSphKernel.hlsli"
GRAVITY = SHADER_ROOT / "GpuSphGravity.CS.hlsl"
BOUNDARY = SHADER_ROOT / "GpuSphBoundary.CS.hlsl"
DENSITY = SHADER_ROOT / "GpuSphDensity.CS.hlsl"
PRESSURE = SHADER_ROOT / "GpuSphPressure.CS.hlsl"
VISCOSITY = SHADER_ROOT / "GpuSphViscosity.CS.hlsl"
PREDICTION = SHADER_ROOT / "GpuSphPredictedPosition.CS.hlsl"
RESET = SHADER_ROOT / "GpuSphReset.CS.hlsl"
FRAMEWORK = ROOT / "Engine/Core/Application/Framework.cpp"
DIAGNOSTICS = ROOT / "Engine/Editor/GpuFluidDiagnosticsPanel.cpp"


class W5GpuSphFoundationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manager_h = MANAGER_H.read_text(encoding="utf-8")
        cls.manager_cpp = MANAGER_CPP.read_text(encoding="utf-8")
        cls.manifest = MANIFEST.read_text(encoding="utf-8")
        cls.common = COMMON.read_text(encoding="utf-8")
        cls.kernel = KERNEL.read_text(encoding="utf-8")
        cls.gravity = GRAVITY.read_text(encoding="utf-8")
        cls.boundary = BOUNDARY.read_text(encoding="utf-8")
        cls.density = DENSITY.read_text(encoding="utf-8")
        cls.pressure = PRESSURE.read_text(encoding="utf-8")
        cls.viscosity = VISCOSITY.read_text(encoding="utf-8")
        cls.prediction = PREDICTION.read_text(encoding="utf-8")
        cls.reset = RESET.read_text(encoding="utf-8")
        cls.framework = FRAMEWORK.read_text(encoding="utf-8")
        cls.diagnostics = DIAGNOSTICS.read_text(encoding="utf-8")

    def test_simulation_constant_layout_keeps_w5_fields_under_w95_extension(self) -> None:
        self.assertIn("static_assert(sizeof(GpuSphSimulationConstants) == 208)", self.manager_h)
        for member in (
            "activeParticleCount",
            "deltaTime",
            "particleMass",
            "smoothingRadius",
            "targetDensity",
            "pressureStiffness",
            "viscosityStrength",
            "boundaryDamping",
            "gravity",
            "boundaryMin",
            "boundaryMax",
            "spawnOrigin",
            "spawnSpacing",
        ):
            self.assertIn(member, self.common)

    def test_w5_particle_capacity_remains_65536(self) -> None:
        self.assertIn("kDefaultParticleCapacity = 65536", self.manager_h)
        self.assertIn("kDefaultActiveParticleCount = 1000", self.manager_h)
        self.assertIn("GetValidatedActiveParticleCount", self.manager_cpp)

    def test_w52_gravity_integrates_velocity(self) -> None:
        self.assertIn("gParticles[index].velocity += gSph.gravity * gSph.deltaTime", self.gravity)
        self.assertIn("GpuSphComputeShaderId::Gravity", self.manifest)

    def test_w53_boundary_handles_predicted_and_final_position(self) -> None:
        self.assertIn("void ResolveBoundary", self.boundary)
        self.assertIn("void ConstrainPredicted", self.boundary)
        self.assertIn("void ConstrainPosition", self.boundary)
        self.assertIn("boundaryDamping", self.boundary)

    def test_w54_exposes_density_pressure_and_viscosity_kernels(self) -> None:
        self.assertIn("GpuSphPoly6Kernel", self.kernel)
        self.assertIn("GpuSphSpikyGradient", self.kernel)
        self.assertIn("GpuSphViscosityLaplacian", self.kernel)

    def test_w55_density_keeps_predicted_position_and_poly6_contract(self) -> None:
        self.assertIn("predictedPosition", self.density)
        self.assertIn("gSph.particleMass * GpuSphPoly6Kernel", self.density)
        self.assertIn("gParticles[index].density", self.density)
        self.assertNotIn("gParticles[index] = particle;", self.density)

    def test_w56_wc_sph_pressure_fallback_remains_available(self) -> None:
        self.assertIn("void ComputePressure", self.pressure)
        self.assertIn("void ApplyPressure", self.pressure)
        self.assertIn("gParticles[index].pressure", self.pressure)
        self.assertIn("GpuSphSpikyGradient", self.pressure)
        self.assertIn("GpuSphComputeShaderId::PressureProperty", self.manager_cpp)
        self.assertIn("GpuSphComputeShaderId::PressureForce", self.manager_cpp)

    def test_w57_viscosity_uses_scratch_to_avoid_velocity_race(self) -> None:
        self.assertIn("void ComputeViscosityDelta", self.viscosity)
        self.assertIn("gScratch[index]", self.viscosity)
        self.assertIn("void ApplyViscosity", self.viscosity)
        self.assertIn("gParticles[index].velocity", self.viscosity)
        self.assertIn("GpuSphClampVelocityByCfl", self.viscosity)
        self.assertNotIn("gParticles[index] = particle;", self.viscosity)
        self.assertIn("SharedSolverScratch", self.manager_cpp)
        self.assertIn("D3D12_RESOURCE_BARRIER_TYPE_UAV", self.manager_cpp)

    def test_w58_prediction_has_predict_and_integrate_stages(self) -> None:
        self.assertIn("void Predict", self.prediction)
        self.assertIn("gParticles[index].predictedPosition", self.prediction)
        self.assertIn("velocityValue * gSph.deltaTime", self.prediction)
        self.assertIn("void Integrate", self.prediction)
        self.assertIn("gParticles[index].position = positionValue", self.prediction)

    def test_reset_seeds_a_3d_particle_block(self) -> None:
        self.assertIn("spawnDimX", self.reset)
        self.assertIn("spawnDimY", self.reset)
        self.assertIn("spawnDimZ", self.reset)
        self.assertIn("particle.position = gSph.spawnOrigin", self.reset)
        self.assertIn("gScratch[index]", self.reset)

    def test_manager_creates_extended_root_signature_and_all_pipeline_states(self) -> None:
        self.assertIn("CreateRootSignature()", self.manager_cpp)
        self.assertIn("CreateScratchBuffer", self.manager_cpp)
        self.assertIn("CreatePipelineStates", self.manager_cpp)
        self.assertIn("kPipelineStateCount = 22", self.manager_h)
        self.assertIn("BaseShaderRegister = 0", self.manager_cpp)
        self.assertIn("BaseShaderRegister = 1", self.manager_cpp)

    def test_framework_ticks_sph_every_frame(self) -> None:
        self.assertIn("GpuSphManager::GetInstance()->Update(deltaTime);", self.framework)
        self.assertIn("GpuSphManager::GetInstance()->Initialize();", self.framework)
        self.assertIn("GpuSphManager::GetInstance()->Finalize();", self.framework)

    def test_diagnostics_exposes_w5_controls_inside_w6_panel(self) -> None:
        for marker in (
            'SPH Simulation (W5/W6)',
            'SPH Paused',
            'SPH Step',
            'SPH Reset',
            'SPH Active Particles',
            'Particle Mass',
            'Smoothing Radius',
            'Target Density',
            'Pressure Stiffness',
            'Viscosity',
            'Boundary Damping',
            'Total Simulation Steps',
            'Total Dispatches',
        ):
            self.assertIn(marker, self.diagnostics)


if __name__ == "__main__":
    unittest.main()
