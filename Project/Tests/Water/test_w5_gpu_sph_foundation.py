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

    def test_simulation_constant_layout_matches_hlsl_contract(self) -> None:
        self.assertIn("static_assert(sizeof(GpuSphSimulationConstants) == 112)", self.manager_h)
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

    def test_w5_limits_naive_neighbor_search_until_w6(self) -> None:
        self.assertIn("kDefaultParticleCapacity = 65536", self.manager_h)
        self.assertIn("kDefaultActiveParticleCount = 1000", self.manager_h)
        self.assertIn("kMaxNaiveNeighborParticles = 4096", self.manager_h)
        self.assertIn("GetValidatedActiveParticleCount", self.manager_cpp)

    def test_w52_gravity_integrates_velocity(self) -> None:
        self.assertIn("particle.velocity += gSph.gravity * gSph.deltaTime", self.gravity)
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

    def test_w55_density_uses_predicted_positions_and_particle_mass(self) -> None:
        self.assertIn("predictedPosition", self.density)
        self.assertIn("neighborIndex < gSph.activeParticleCount", self.density)
        self.assertIn("gSph.particleMass * GpuSphPoly6Kernel", self.density)
        self.assertIn("particle.density", self.density)

    def test_w56_pressure_is_split_by_uav_barrier_boundary(self) -> None:
        self.assertIn("void ComputePressure", self.pressure)
        self.assertIn("void ApplyPressure", self.pressure)
        self.assertIn("particle.pressure", self.pressure)
        self.assertIn("GpuSphSpikyGradient", self.pressure)
        pressure_property = self.manager_cpp.index("GpuSphComputeShaderId::PressureProperty")
        pressure_force = self.manager_cpp.index("GpuSphComputeShaderId::PressureForce")
        self.assertLess(pressure_property, pressure_force)

    def test_w57_viscosity_uses_scratch_to_avoid_velocity_race(self) -> None:
        self.assertIn("void ComputeViscosityDelta", self.viscosity)
        self.assertIn("gScratch[index]", self.viscosity)
        self.assertIn("void ApplyViscosity", self.viscosity)
        self.assertIn("particle.velocity += gScratch[index].xyz", self.viscosity)
        self.assertIn("VelocityDeltaScratch", self.manager_cpp)
        self.assertIn("D3D12_RESOURCE_BARRIER_TYPE_UAV", self.manager_cpp)

    def test_w58_prediction_has_predict_and_integrate_stages(self) -> None:
        self.assertIn("void Predict", self.prediction)
        self.assertIn("particle.predictedPosition = particle.position + particle.velocity * gSph.deltaTime", self.prediction)
        self.assertIn("void Integrate", self.prediction)
        self.assertIn("particle.position += particle.velocity * gSph.deltaTime", self.prediction)

    def test_reset_seeds_a_3d_particle_block(self) -> None:
        self.assertIn("spawnDimX", self.reset)
        self.assertIn("spawnDimY", self.reset)
        self.assertIn("spawnDimZ", self.reset)
        self.assertIn("particle.position = gSph.spawnOrigin", self.reset)
        self.assertIn("gScratch[index]", self.reset)

    def test_manager_creates_root_signature_scratch_and_all_pipeline_states(self) -> None:
        self.assertIn("CreateRootSignature()", self.manager_cpp)
        self.assertIn("CreateScratchBuffer", self.manager_cpp)
        self.assertIn("CreatePipelineStates", self.manager_cpp)
        self.assertIn("kPipelineStateCount = 11", self.manager_h)
        self.assertIn("BaseShaderRegister = 0", self.manager_cpp)
        self.assertIn("BaseShaderRegister = 1", self.manager_cpp)

    def test_fixed_step_executes_w5_stages_in_dependency_order(self) -> None:
        ordered_markers = [
            "GpuSphComputeShaderId::Gravity",
            "GpuSphComputeShaderId::Predict",
            "GpuSphComputeShaderId::BoundaryPredicted",
            "GpuSphComputeShaderId::Density",
            "GpuSphComputeShaderId::PressureProperty",
            "GpuSphComputeShaderId::PressureForce",
            "GpuSphComputeShaderId::ViscosityDelta",
            "GpuSphComputeShaderId::ViscosityApply",
            "GpuSphComputeShaderId::Integrate",
            "GpuSphComputeShaderId::BoundaryPosition",
        ]
        start = self.manager_cpp.index("bool GpuSphManager::ExecuteSimulationStep")
        end = self.manager_cpp.index("bool GpuSphManager::DispatchStage", start)
        step_body = self.manager_cpp[start:end]
        positions = [step_body.index(marker) for marker in ordered_markers]
        self.assertEqual(positions, sorted(positions))

    def test_framework_ticks_sph_every_frame(self) -> None:
        self.assertIn("GpuSphManager::GetInstance()->Update(deltaTime);", self.framework)
        self.assertIn("GpuSphManager::GetInstance()->Initialize();", self.framework)
        self.assertIn("GpuSphManager::GetInstance()->Finalize();", self.framework)

    def test_diagnostics_exposes_complete_w5_controls_and_counters(self) -> None:
        for marker in (
            'SPH Foundation (W5)',
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
