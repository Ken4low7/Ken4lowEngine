from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
FRAMEWORK = ROOT / "Engine/Core/Application/Framework.cpp"
LIQUID_ROOT = ROOT / "Engine/Graphics/Renderer/GpuFluid/Liquid/Manager"
MANAGER = LIQUID_ROOT / "GpuProductionLiquidManager.h"
OCEAN_BRIDGE = LIQUID_ROOT / "GpuProductionLiquidOceanBridge.h"
OCEAN_COUPLER = LIQUID_ROOT / "GpuProductionLiquidOceanCoupler.h"
SECONDARY = LIQUID_ROOT / "GpuProductionLiquidSecondaryClassifier.h"
WATER_SURFACE = ROOT / "Engine/Scene/Actor/Components/WaterSurfaceComponent.h"
SECONDARY_SHADER = ROOT / "Resources/Shaders/GpuFluid/Sph/Production/GpuSphSecondaryClassify.CS.hlsl"
OCEAN_SHADER = ROOT / "Resources/Shaders/GpuFluid/Sph/Production/GpuSphOceanCoupling.CS.hlsl"
SPH_MANAGER = ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph/Manager/GpuSphManager.h"
SPH_COMMON = ROOT / "Resources/Shaders/GpuFluid/Sph/GpuSphCommon.hlsli"


class W10ProductionLiquidSystemTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.framework = FRAMEWORK.read_text(encoding="utf-8")
        cls.manager = MANAGER.read_text(encoding="utf-8")
        cls.ocean_bridge = OCEAN_BRIDGE.read_text(encoding="utf-8")
        cls.ocean_coupler = OCEAN_COUPLER.read_text(encoding="utf-8")
        cls.secondary = SECONDARY.read_text(encoding="utf-8")
        cls.water_surface = WATER_SURFACE.read_text(encoding="utf-8")
        cls.secondary_shader = SECONDARY_SHADER.read_text(encoding="utf-8")
        cls.ocean_shader = OCEAN_SHADER.read_text(encoding="utf-8")
        cls.sph_manager = SPH_MANAGER.read_text(encoding="utf-8")
        cls.sph_common = SPH_COMMON.read_text(encoding="utf-8")

    def test_framework_drives_production_liquid_before_and_after_sph(self) -> None:
        self.assertIn("GpuProductionLiquidManager::GetInstance()->Initialize()", self.framework)
        pre = self.framework.index("GpuProductionLiquidManager::GetInstance()->PreSphUpdate(deltaTime)")
        sph = self.framework.index("GpuSphManager::GetInstance()->Update(deltaTime)", pre)
        post = self.framework.index("GpuProductionLiquidManager::GetInstance()->PostSphUpdate()", sph)
        self.assertLess(pre, sph)
        self.assertLess(sph, post)
        self.assertIn("GpuProductionLiquidManager::GetInstance()->Finalize()", self.framework)

    def test_secondary_classifier_is_owned_by_production_runtime(self) -> None:
        self.assertIn("GpuProductionLiquidSecondaryClassifier", self.manager)
        self.assertIn("classifier->Update(", self.manager)
        self.assertIn("sph->GetParticleBuffer()", self.manager)
        self.assertIn("sprayCandidateCount", self.manager)
        self.assertIn("foamCandidateCount", self.manager)
        self.assertIn("bubbleCandidateCount", self.manager)

    def test_secondary_classification_stays_gpu_driven(self) -> None:
        self.assertIn("SetComputeRootDescriptorTable", self.secondary)
        self.assertIn("Dispatch(groupCount, 1, 1)", self.secondary)
        self.assertIn("InterlockedAdd", self.secondary_shader)
        self.assertNotIn("ExecuteAndWait", self.secondary)

    def test_water_surface_implements_ocean_provider_contract(self) -> None:
        self.assertIn("public IGpuProductionLiquidOceanProvider", self.water_surface)
        self.assertIn("GpuProductionLiquidOceanSample SampleOcean", self.water_surface)
        self.assertIn("SetOceanProvider(this)", self.water_surface)
        self.assertIn("ClearOceanProvider(this)", self.water_surface)
        self.assertIn("GetEditableOceanSettings().enabled = true", self.water_surface)

    def test_ocean_sample_reuses_gerstner_surface_data(self) -> None:
        self.assertIn("sample.height = surface.worldPosition.y", self.water_surface)
        self.assertIn("sample.normal = surface.worldNormal", self.water_surface)
        self.assertIn("sample.velocity = surface.worldVelocity", self.water_surface)
        self.assertIn("evaluation.velocityZ += -amplitude * speed * cosinePhase", self.water_surface)

    def test_ocean_to_sph_coupling_is_gpu_driven(self) -> None:
        self.assertIn("GpuProductionLiquidOceanCoupler", self.manager)
        self.assertIn("GpuProductionLiquidOceanCoupler::GetInstance()->Update", self.manager)
        self.assertIn("particles.InsertUavBarrier", self.ocean_coupler)
        self.assertIn("particle.velocity = lerp", self.ocean_shader)
        self.assertIn("surfaceAttraction", self.ocean_shader)
        self.assertNotIn("ExecuteAndWait", self.ocean_coupler)

    def test_spatial_lod_tracks_active_camera_and_blend_band(self) -> None:
        self.assertIn("CameraManager::GetInstance()", self.manager)
        self.assertIn("GetActiveCameraPosition", self.manager)
        self.assertIn("ApplySpatialLod", self.manager)
        self.assertIn("focusDistanceToSimulation", self.manager)
        self.assertIn("oceanVisualBlend", self.manager)
        self.assertIn("SetActiveParticleCount(targetCount)", self.manager)

    def test_sph_to_ocean_feedback_closes_the_bridge(self) -> None:
        self.assertIn("SubmitLiquidFeedback", self.ocean_bridge)
        self.assertIn("oceanFeedbackVelocity_ += impulse", self.manager)
        self.assertIn("lastOceanSample_.velocity += oceanFeedbackVelocity_", self.manager)
        self.assertIn("feedbackStrength", self.manager)
        self.assertIn("feedbackRadius", self.manager)

    def test_ocean_provider_clear_is_identity_safe(self) -> None:
        self.assertIn("void ClearOceanProvider", self.manager)
        self.assertIn("if (oceanProvider_ == provider)", self.manager)
        self.assertIn("SetOceanProvider(nullptr)", self.manager)

    def test_sph_constant_contract_reserves_w10_ocean_tail(self) -> None:
        self.assertIn("oceanCouplingEnabled", self.sph_manager)
        self.assertIn("static_assert(sizeof(GpuSphSimulationConstants) == 272)", self.sph_manager)
        self.assertIn("oceanSurfaceVelocity", self.sph_common)

    def test_quality_presets_use_real_renderer_setting(self) -> None:
        self.assertIn("blurDepthFalloff", self.manager)
        self.assertNotIn("GetEditableSettings().smoothingIterations", self.manager)
        for marker in ("Development", "Interactive", "High", "Cinematic", "Stress"):
            self.assertIn(marker, self.manager)


if __name__ == "__main__":
    unittest.main()
