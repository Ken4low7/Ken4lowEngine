from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SHADER_ROOT = ROOT / "Resources/Shaders/GpuFluid/Sph"
MANAGER_H = ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph/Manager/GpuSphManager.h"
MANAGER_CPP = ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph/Manager/GpuSphManager.cpp"
MANIFEST = ROOT / "Engine/Graphics/Shader/Manifest/GpuSphShaderManifest.h"
COMMON = SHADER_ROOT / "GpuSphCommon.hlsli"
BUILD_KEYS = SHADER_ROOT / "GpuSphSpatialBuildKeys.CS.hlsl"
BITONIC = SHADER_ROOT / "GpuSphSpatialBitonicSort.CS.hlsl"
CLEAR_RANGES = SHADER_ROOT / "GpuSphSpatialClearCellRanges.CS.hlsl"
BUILD_RANGES = SHADER_ROOT / "GpuSphSpatialBuildCellRanges.CS.hlsl"
DENSITY = SHADER_ROOT / "GpuSphDensity.CS.hlsl"
PRESSURE = SHADER_ROOT / "GpuSphPressure.CS.hlsl"
VISCOSITY = SHADER_ROOT / "GpuSphViscosity.CS.hlsl"
DIAGNOSTICS = ROOT / "Engine/Editor/GpuFluidDiagnosticsPanel.cpp"


class W6SpatialHashGpuSortTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manager_h = MANAGER_H.read_text(encoding="utf-8")
        cls.manager_cpp = MANAGER_CPP.read_text(encoding="utf-8")
        cls.manifest = MANIFEST.read_text(encoding="utf-8")
        cls.common = COMMON.read_text(encoding="utf-8")
        cls.build_keys = BUILD_KEYS.read_text(encoding="utf-8")
        cls.bitonic = BITONIC.read_text(encoding="utf-8")
        cls.clear_ranges = CLEAR_RANGES.read_text(encoding="utf-8")
        cls.build_ranges = BUILD_RANGES.read_text(encoding="utf-8")
        cls.density = DENSITY.read_text(encoding="utf-8")
        cls.pressure = PRESSURE.read_text(encoding="utf-8")
        cls.viscosity = VISCOSITY.read_text(encoding="utf-8")
        cls.diagnostics = DIAGNOSTICS.read_text(encoding="utf-8")

    def test_common_contract_has_hash_entries_cell_ranges_and_3d_grid(self) -> None:
        for marker in (
            "struct GpuSphHashEntry",
            "struct GpuSphCellRange",
            "spatialGridMin",
            "spatialCellSize",
            "spatialGridDimX",
            "spatialGridDimY",
            "spatialGridDimZ",
            "spatialCellCount",
            "RWStructuredBuffer<GpuSphHashEntry> gHashEntries : register(u2)",
            "RWStructuredBuffer<GpuSphCellRange> gCellRanges : register(u3)",
            "GpuSphPositionToCell",
            "GpuSphCellToKey",
        ):
            self.assertIn(marker, self.common)

    def test_cpp_and_hlsl_constant_layout_preserves_w6_fields_under_w95_extension(self) -> None:
        self.assertIn("static_assert(sizeof(GpuSphSimulationConstants) == 208)", self.manager_h)
        self.assertIn("static_assert(sizeof(GpuSphDispatchConstants) == 16)", self.manager_h)
        self.assertIn("GpuSphDispatchCB : register(b1)", self.common)

    def test_manager_allocates_hash_and_cell_range_uavs(self) -> None:
        for marker in (
            "CreateSpatialHashBuffers",
            "GpuSph.W6.HashEntries",
            "GpuSph.W6.CellRanges",
            "hashEntriesUavIndex_",
            "cellRangesUavIndex_",
            "kMaxSpatialCellCapacity = 1u << 20",
        ):
            self.assertIn(marker, self.manager_h + self.manager_cpp)

    def test_root_signature_preserves_w6_bindings_inside_w95_extension(self) -> None:
        self.assertIn("D3D12_ROOT_PARAMETER rootParameters[7]", self.manager_cpp)
        self.assertIn("hashUavRange.BaseShaderRegister = 2", self.manager_cpp)
        self.assertIn("cellRangeUavRange.BaseShaderRegister = 3", self.manager_cpp)
        self.assertIn("D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS", self.manager_cpp)
        self.assertIn("Constants.ShaderRegister = 1", self.manager_cpp)

    def test_key_builder_uses_predicted_3d_cell_and_padding_key(self) -> None:
        self.assertIn("gParticles[index].predictedPosition", self.build_keys)
        self.assertIn("GpuSphPositionToCell", self.build_keys)
        self.assertIn("GpuSphCellToKey", self.build_keys)
        self.assertIn("kGpuSphInvalidIndex", self.build_keys)
        self.assertIn("index < gSph.activeParticleCount", self.build_keys)

    def test_bitonic_sort_uses_xor_partner_and_key_then_particle_order(self) -> None:
        self.assertIn("partnerIndex = index ^ gSortLevelMask", self.bitonic)
        self.assertIn("index & gSortLevel", self.bitonic)
        self.assertIn("a.key != b.key", self.bitonic)
        self.assertIn("a.particleIndex > b.particleIndex", self.bitonic)

    def test_cell_ranges_are_cleared_then_built_with_atomics(self) -> None:
        self.assertIn("range.start = kGpuSphInvalidIndex", self.clear_ranges)
        self.assertIn("range.count = 0", self.clear_ranges)
        self.assertIn("InterlockedMin", self.build_ranges)
        self.assertIn("InterlockedAdd", self.build_ranges)
        clear_pos = self.manager_cpp.index("GpuSphComputeShaderId::SpatialClearCellRanges")
        build_pos = self.manager_cpp.index("GpuSphComputeShaderId::SpatialBuildCellRanges")
        self.assertLess(clear_pos, build_pos)

    def test_spatial_hash_runs_between_predicted_boundary_and_density(self) -> None:
        start = self.manager_cpp.index("bool GpuSphManager::ExecuteSimulationStep")
        end = self.manager_cpp.index("bool GpuSphManager::ExecuteDfSphProjection", start)
        body = self.manager_cpp[start:end]
        boundary = body.index("GpuSphComputeShaderId::BoundaryPredicted")
        spatial = body.index("ExecuteSpatialHashBuild")
        density = body.index("GpuSphComputeShaderId::Density")
        self.assertLess(boundary, spatial)
        self.assertLess(spatial, density)

    def test_manager_executes_full_gpu_bitonic_stage_sequence(self) -> None:
        self.assertIn("for (uint32_t sortLevel = 2; sortLevel <= sortCount", self.manager_cpp)
        self.assertIn("for (uint32_t sortMask = sortLevel >> 1u", self.manager_cpp)
        self.assertIn("GpuSphComputeShaderId::SpatialBuildKeys", self.manager_cpp)
        self.assertIn("GpuSphComputeShaderId::SpatialBitonicSort", self.manager_cpp)
        self.assertIn("spatialHashSortDispatchCount", self.manager_cpp)

    def test_neighbor_forces_use_27_cells_and_no_longer_scan_every_particle(self) -> None:
        for shader in (self.density, self.pressure, self.viscosity):
            self.assertIn("GpuSphGetCellRange", shader)
            self.assertIn("for (int z = -1; z <= 1; ++z)", shader)
            self.assertIn("for (int y = -1; y <= 1; ++y)", shader)
            self.assertIn("for (int x = -1; x <= 1; ++x)", shader)
            self.assertNotIn("neighborIndex < gSph.activeParticleCount", shader)

    def test_active_particle_count_can_scale_to_particle_buffer_capacity(self) -> None:
        validated_start = self.manager_cpp.index("uint32_t GpuSphManager::GetValidatedActiveParticleCount")
        validated_end = self.manager_cpp.index("uint32_t GpuSphManager::GetSortCount", validated_start)
        validated_body = self.manager_cpp[validated_start:validated_end]
        self.assertIn("particleBuffer_.GetCapacity()", validated_body)
        self.assertNotIn("4096", validated_body)
        self.assertIn("sphBufferStats.capacity", self.diagnostics)

    def test_spawn_layout_expands_for_large_particle_counts(self) -> None:
        self.assertIn("std::cbrt", self.manager_cpp)
        self.assertIn("UpdateSpawnLayoutForActiveCount", self.manager_cpp)
        self.assertIn("settings_.spawnDimZ", self.manager_cpp)

    def test_manifest_preserves_w6_spatial_passes_inside_extended_pipeline(self) -> None:
        for marker in (
            "SpatialBuildKeys",
            "SpatialBitonicSort",
            "SpatialClearCellRanges",
            "SpatialBuildCellRanges",
        ):
            self.assertIn(marker, self.manifest)
        self.assertIn("kPipelineStateCount = 22", self.manager_h)

    def test_diagnostics_exposes_w6_grid_sort_and_dispatch_status(self) -> None:
        for marker in (
            "W6 Spatial Hash / GPU Sort",
            "Spatial Hash: %s",
            "Grid: %u x %u x %u | Cells: %u",
            "Bitonic Sort Count",
            "Hash Builds",
            "Sort Dispatches",
            "Cell Range Dispatches",
        ):
            self.assertIn(marker, self.diagnostics)


if __name__ == "__main__":
    unittest.main()
