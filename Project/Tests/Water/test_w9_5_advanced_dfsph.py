from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPH_ROOT = ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph"
SHADER_ROOT = ROOT / "Resources/Shaders/GpuFluid/Sph"
MANAGER_H = SPH_ROOT / "Manager/GpuSphManager.h"
MANAGER_CPP = SPH_ROOT / "Manager/GpuSphManager.cpp"
COMMON = SHADER_ROOT / "GpuSphCommon.hlsli"
DFSPH = SHADER_ROOT / "GpuSphDfSph.CS.hlsl"
METRIC = SHADER_ROOT / "GpuSphCflMetric.CS.hlsl"
RESET = SHADER_ROOT / "GpuSphReset.CS.hlsl"
VISCOSITY = SHADER_ROOT / "GpuSphViscosity.CS.hlsl"
BOUNDARY = SHADER_ROOT / "GpuSphBoundary.CS.hlsl"
MANIFEST = ROOT / "Engine/Graphics/Shader/Manifest/GpuSphShaderManifest.h"
ADVANCED_PANEL = ROOT / "Engine/Editor/GpuSphAdvancedDiagnosticsPanel.h"
OVERLAY = ROOT / "Engine/Editor/EditorLevelOverlay.h"
PARTICLE_TYPES = SPH_ROOT / "Data/GpuSphParticleTypes.h"


class W95AdvancedDfSphTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manager_h = MANAGER_H.read_text(encoding="utf-8")
        cls.manager_cpp = MANAGER_CPP.read_text(encoding="utf-8")
        cls.common = COMMON.read_text(encoding="utf-8")
        cls.dfsph = DFSPH.read_text(encoding="utf-8")
        cls.metric = METRIC.read_text(encoding="utf-8")
        cls.reset = RESET.read_text(encoding="utf-8")
        cls.viscosity = VISCOSITY.read_text(encoding="utf-8")
        cls.boundary = BOUNDARY.read_text(encoding="utf-8")
        cls.manifest = MANIFEST.read_text(encoding="utf-8")
        cls.panel = ADVANCED_PANEL.read_text(encoding="utf-8")
        cls.overlay = OVERLAY.read_text(encoding="utf-8")
        cls.particle_types = PARTICLE_TYPES.read_text(encoding="utf-8")

    def test_particle_contract_stays_48_bytes_while_constants_extend_for_w10(self) -> None:
        self.assertIn("static_assert(sizeof(GpuSphParticle) == 48)", self.particle_types)
        self.assertIn("static_assert(sizeof(GpuSphSimulationConstants) == 272)", self.manager_h)
        self.assertIn("dfsphWarmStartEnabled", self.common)
        self.assertIn("dfsphWarmStartStrength", self.common)

    def test_persistent_dfsph_state_is_u4_and_bound_by_manager(self) -> None:
        self.assertIn("RWStructuredBuffer<float4> gDfSphState : register(u4)", self.common)
        self.assertIn("CreateDfSphStateBuffer", self.manager_h)
        self.assertIn("GpuSph.W9.5.PersistentDfSphState", self.manager_cpp)
        self.assertIn("dfsphStateUavRange.BaseShaderRegister = 4", self.manager_cpp)
        self.assertIn("SetComputeRootDescriptorTable(5", self.manager_cpp)
        self.assertIn("SetComputeRoot32BitConstants(6", self.manager_cpp)
        self.assertIn("D3D12_ROOT_PARAMETER rootParameters[7]", self.manager_cpp)

    def test_reset_clears_warm_start_and_error_state(self) -> None:
        self.assertIn("gDfSphState[index] = float4(0.0f, 0.0f, 0.0f, 0.0f)", self.reset)
        self.assertIn("dfsphStateBarrier", self.manager_cpp)
        self.assertIn("InsertUavBarrier(dfsphStateBuffer_.Get())", self.manager_cpp)

    def test_manifest_registers_density_divergence_and_metric_passes(self) -> None:
        for marker in (
            "DfSphFactor",
            "DfSphDensityPrepare",
            "DfSphDensityApply",
            "DfSphDivergencePrepare",
            "DfSphDivergenceApply",
            "CflMetricClear",
            "CflMetricMeasure",
        ):
            self.assertIn(marker, self.manifest)
        self.assertIn("kPipelineStateCount = 22", self.manager_h)

    def test_dfsph_uses_w6_spatial_hash_and_iterative_projection(self) -> None:
        self.assertIn("GpuSphGetCellRange", self.dfsph)
        for axis in (
            "for (int z = -1; z <= 1; ++z)",
            "for (int y = -1; y <= 1; ++y)",
            "for (int x = -1; x <= 1; ++x)",
        ):
            self.assertIn(axis, self.dfsph)
        self.assertIn("for (uint32_t iteration = 0; iteration < densityIterations", self.manager_cpp)
        self.assertIn("for (uint32_t iteration = 0; iteration < divergenceIterations", self.manager_cpp)
        self.assertIn("ExecuteDfSphProjection", self.manager_cpp)

    def test_wc_sph_remains_a_runtime_fallback(self) -> None:
        self.assertIn("if (settings_.dfsphEnabled)", self.manager_cpp)
        self.assertIn("GpuSphComputeShaderId::PressureProperty", self.manager_cpp)
        self.assertIn("GpuSphComputeShaderId::PressureForce", self.manager_cpp)
        self.assertIn("WCSPH / Tait EOS Fallback", self.panel)

    def test_warm_start_only_applies_on_first_projection_iteration(self) -> None:
        self.assertIn("dfsphWarmStartEnabled = true", self.manager_h)
        self.assertIn("dfsphWarmStartStrength = 0.35f", self.manager_h)
        self.assertIn("gSph.dfsphWarmStartEnabled != 0u && gSortLevel == 0u", self.dfsph)
        self.assertIn("solverState.x", self.dfsph)
        self.assertIn("solverState.y", self.dfsph)
        self.assertIn("iterationConstants.sortLevel = iteration", self.manager_cpp)
        self.assertIn('ImGui::Checkbox("Warm Start"', self.panel)

    def test_gpu_metrics_reduce_speed_density_and_divergence_without_waiting(self) -> None:
        self.assertIn("InterlockedMax(gHashEntries[0].key", self.metric)
        self.assertIn("InterlockedMax(gHashEntries[0].particleIndex", self.metric)
        self.assertIn("InterlockedMax(gHashEntries[1].key", self.metric)
        self.assertIn("gDfSphState[index]", self.metric)
        self.assertIn("struct CflMetricReadback", self.manager_h)
        self.assertIn("static_assert(sizeof(CflMetricReadback) == 16)", self.manager_h)
        self.assertIn("sizeof(CflMetricReadback)", self.manager_cpp)
        self.assertIn("GetCurrentFrameIndex", self.manager_cpp)
        self.assertNotIn("ExecuteAndWait", self.manager_cpp)

    def test_adaptive_cfl_uses_gpu_max_speed_and_minimum_dt(self) -> None:
        self.assertIn("lastMeasuredMaxSpeed_", self.manager_cpp)
        self.assertIn("CalculateEffectiveDeltaTime", self.manager_cpp)
        self.assertIn("settings_.cflNumber", self.manager_cpp)
        self.assertIn("settings_.minimumDeltaTime", self.manager_cpp)
        self.assertIn("effectiveDeltaTime_", self.manager_cpp)

    def test_surface_and_boundary_refinements_are_runtime_tunable(self) -> None:
        self.assertIn("surfaceTensionStrength", self.common)
        self.assertIn("xsphStrength", self.common)
        self.assertIn("boundaryFriction", self.common)
        self.assertIn("gSph.surfaceTensionStrength", self.viscosity)
        self.assertIn("gSph.xsphStrength", self.viscosity)
        self.assertIn("gSph.boundaryFriction", self.boundary)

    def test_f7_panel_exposes_real_convergence_metrics_and_stress_presets(self) -> None:
        self.assertIn('ImGui::Begin("W9.5 Advanced SPH / DFSPH"', self.panel)
        self.assertIn("Max Density Error", self.panel)
        self.assertIn("Max Divergence Error", self.panel)
        self.assertIn("Measured Max Speed", self.panel)
        self.assertIn('ImGui::Button("1K")', self.panel)
        self.assertIn('ImGui::Button("4K")', self.panel)
        self.assertIn('ImGui::Button("16K")', self.panel)
        self.assertIn('ImGui::Button("65K")', self.panel)
        self.assertIn("SetActiveParticleCount(65536u)", self.panel)
        self.assertIn("GpuSphAdvancedDiagnosticsPanel::GetInstance()->Draw()", self.overlay)

    def test_production_preset_enables_full_stability_stack(self) -> None:
        body_start = self.manager_cpp.index("void GpuSphManager::ApplyWaterProductionPreset")
        body_end = self.manager_cpp.index("bool GpuSphManager::CreateRootSignature", body_start)
        body = self.manager_cpp[body_start:body_end]
        for marker in (
            "dfsphEnabled = true",
            "dfsphWarmStartEnabled = true",
            "adaptiveCflEnabled = true",
            "surfaceTensionStrength = 0.0728f",
            "boundaryFriction = 0.08f",
        ):
            self.assertIn(marker, body)

    def test_stress_capacity_remains_65536_particles(self) -> None:
        self.assertIn("kDefaultParticleCapacity = 65536", self.manager_h)
        self.assertIn("particleBuffer_.GetCapacity()", self.manager_cpp)
        self.assertIn("UpdateSpawnLayoutForActiveCount", self.manager_cpp)


if __name__ == "__main__":
    unittest.main()
