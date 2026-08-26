from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
FRAMEWORK = ROOT / "Engine/Core/Application/Framework.cpp"
LIQUID_ROOT = ROOT / "Engine/Graphics/Renderer/GpuFluid/Liquid/Manager"
MANAGER = LIQUID_ROOT / "GpuProductionLiquidManager.h"
OCEAN_BRIDGE = LIQUID_ROOT / "GpuProductionLiquidOceanBridge.h"
SECONDARY = LIQUID_ROOT / "GpuProductionLiquidSecondaryClassifier.h"
WATER_SURFACE = ROOT / "Engine/Scene/Actor/Components/WaterSurfaceComponent.h"
SECONDARY_SHADER = ROOT / "Resources/Shaders/GpuFluid/Sph/Production/GpuSphSecondaryClassify.CS.hlsl"


class W10ProductionLiquidSystemTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.framework = FRAMEWORK.read_text(encoding="utf-8")
        cls.manager = MANAGER.read_text(encoding="utf-8")
        cls.ocean_bridge = OCEAN_BRIDGE.read_text(encoding="utf-8")
        cls.secondary = SECONDARY.read_text(encoding="utf-8")
        cls.water_surface = WATER_SURFACE.read_text(encoding="utf-8")
        cls.secondary_shader = SECONDARY_SHADER.read_text(encoding="utf-8")

    def test_framework_drives_production_liquid_before_and_after_sph(self) -> None:
        self.assertIn("GpuProductionLiquidManager::GetInstance()->Initialize()", self.framework)
        pre = self.framework.index("GpuProductionLiquidManager::GetInstance()->PreSphUpdate(deltaTime)")
        sph = self.framework.index("GpuSphManager::GetInstance()->Update(deltaTime)", pre)
        post = self.framework.index("GpuProductionLiquidManager::GetInstance()->PostSphUpdate()", sph)
        self.assertLess(pre, sph)
        self.assertLess(sph, post)
        self.assertIn("GpuProductionLiquidManager::GetInstance()->Finalize()", self.framework)

    def test_secondary_classifier_is_owned_by_production_runtime(self) -> None:
        self.assertIn('#include "GpuProductionLiquidSecondaryClassifier.h"', self.manager)
        self.assertIn("classifier->Update(", self.manager)
        self.assertIn("sph->GetParticleBuffer()", self.manager)
        self.assertIn("GpuProductionLiquidSecondaryClassifier::GetInstance()->Finalize()", self.manager)
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
        self.assertIn("Vector3 velocity{}", self.ocean_bridge)

    def test_ocean_provider_clear_is_identity_safe(self) -> None:
        self.assertIn("void ClearOceanProvider", self.manager)
        self.assertIn("if (oceanProvider_ == provider)", self.manager)
        self.assertIn("SetOceanProvider(nullptr)", self.manager)

    def test_quality_and_lod_presets_remain_available(self) -> None:
        for marker in (
            "Development",
            "Interactive",
            "High",
            "Cinematic",
            "Stress",
            "highQualityRadius",
            "simulationRadius",
        ):
            self.assertIn(marker, self.manager)


if __name__ == "__main__":
    unittest.main()
