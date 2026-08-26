from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
TYPES = ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph/Data/GpuSphParticleTypes.h"
BUFFER_H = ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph/Resource/GpuSphParticleBuffer.h"
BUFFER_CPP = ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph/Resource/GpuSphParticleBuffer.cpp"
MANAGER = ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph/Manager/GpuSphManager.h"
HLSL = ROOT / "Resources/Shaders/GpuFluid/Sph/GpuSphCommon.hlsli"
FRAMEWORK = ROOT / "Engine/Core/Application/Framework.cpp"
DIAGNOSTICS = ROOT / "Engine/Editor/GpuFluidDiagnosticsPanel.cpp"
BUILD_PROPS = ROOT / "Directory.Build.props"


class W51SphParticleBufferTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.types = TYPES.read_text(encoding="utf-8")
        cls.buffer_h = BUFFER_H.read_text(encoding="utf-8")
        cls.buffer_cpp = BUFFER_CPP.read_text(encoding="utf-8")
        cls.manager = MANAGER.read_text(encoding="utf-8")
        cls.hlsl = HLSL.read_text(encoding="utf-8")
        cls.framework = FRAMEWORK.read_text(encoding="utf-8")
        cls.diagnostics = DIAGNOSTICS.read_text(encoding="utf-8")
        cls.build_props = BUILD_PROPS.read_text(encoding="utf-8")

    def test_cpu_particle_layout_is_future_sph_ready_and_48_bytes(self) -> None:
        self.assertIn("Vector3 position", self.types)
        self.assertIn("float density", self.types)
        self.assertIn("Vector3 velocity", self.types)
        self.assertIn("float pressure", self.types)
        self.assertIn("Vector3 predictedPosition", self.types)
        self.assertIn("static_assert(sizeof(GpuSphParticle) == 48)", self.types)

    def test_hlsl_mirrors_cpu_particle_layout(self) -> None:
        for member in (
            "float3 position",
            "float density",
            "float3 velocity",
            "float pressure",
            "float3 predictedPosition",
            "float padding",
        ):
            self.assertIn(member, self.hlsl)

    def test_particle_buffer_uses_default_heap_structured_srv_and_uav(self) -> None:
        self.assertIn("D3D12_HEAP_TYPE_DEFAULT", self.buffer_cpp)
        self.assertIn("D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS", self.buffer_cpp)
        self.assertIn("CreateSRVForStructureBuffer", self.buffer_cpp)
        self.assertIn("CreateUAVForStructuredBuffer", self.buffer_cpp)
        self.assertIn("sizeof(GpuSphParticle)", self.buffer_cpp)

    def test_compute_srv_and_uav_share_compute_descriptor_heap(self) -> None:
        self.assertIn("computeSrvIndex_ = uavManager->Allocate()", self.buffer_cpp)
        self.assertIn("uavIndex_ = uavManager->Allocate()", self.buffer_cpp)
        self.assertIn("uavManager->CreateSRVForStructureBuffer", self.buffer_cpp)

    def test_resource_exposes_transition_and_uav_barrier(self) -> None:
        self.assertIn("void Transition", self.buffer_h)
        self.assertIn("InsertUavBarrier", self.buffer_h)
        self.assertIn("D3D12_RESOURCE_BARRIER_TYPE_TRANSITION", self.buffer_cpp)
        self.assertIn("D3D12_RESOURCE_BARRIER_TYPE_UAV", self.buffer_cpp)

    def test_manager_keeps_w51_capacity_contract(self) -> None:
        self.assertIn("kDefaultParticleCapacity = 65536", self.manager)
        self.assertIn("GpuSphParticleBuffer particleBuffer_", self.manager)

    def test_framework_owns_sph_runtime_before_descriptor_shutdown(self) -> None:
        self.assertIn("GpuSphManager::GetInstance()->Initialize();", self.framework)
        self.assertIn("GpuSphManager::GetInstance()->Finalize();", self.framework)
        self.assertLess(
            self.framework.index("GpuSphManager::GetInstance()->Finalize();"),
            self.framework.index("UAVManager::GetInstance()->Finalize();"),
        )

    def test_diagnostics_still_exposes_w51_storage_contract(self) -> None:
        self.assertIn('SPH Simulation (W5/W6)', self.diagnostics)
        self.assertIn('Particle Buffer: %s', self.diagnostics)
        self.assertIn('Active / Capacity: %u / %u', self.diagnostics)
        self.assertIn('Particle Stride: %u bytes', self.diagnostics)
        self.assertIn('SRV: %u | Compute SRV: %u | UAV: %u', self.diagnostics)

    def test_build_props_registers_w51_sources(self) -> None:
        for path in (
            "GpuFluid\\Sph\\Data\\GpuSphParticleTypes.h",
            "GpuFluid\\Sph\\Resource\\GpuSphParticleBuffer.h",
            "GpuFluid\\Sph\\Resource\\GpuSphParticleBuffer.cpp",
            "GpuFluid\\Sph\\Manager\\GpuSphManager.h",
            "GpuFluid\\Sph\\Manager\\GpuSphManager.cpp",
            "GpuFluid\\Sph\\GpuSphCommon.hlsli",
        ):
            self.assertIn(path, self.build_props)


if __name__ == "__main__":
    unittest.main()
