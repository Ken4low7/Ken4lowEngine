from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PLANAR_MANAGER_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "PlanarReflectionManager.h"
PLANAR_MANAGER_INL = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "PlanarReflectionManager.inl"
PLANAR_BRIDGE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "PlanarReflectionSceneBridge.h"
PROBE_BRIDGE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "ReflectionProbeSceneBridge.h"
PLANAR_COMPONENT_H = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "PlanarReflectionComponent.h"
PLANAR_COMPONENT_INL = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "PlanarReflectionComponent.inl"
MODEL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.cpp"
COMPONENT_FACTORY = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Serialization" / "ComponentFactory.cpp"
MATERIAL_H = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "Material.h"
MATERIAL_CPP = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "Material.cpp"
OBJECT_PIPELINE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3DPipelineSet.cpp"
OBJECT_PS = PROJECT_ROOT / "Resources" / "Shaders" / "Object3D" / "Object3d.PS.hlsl"


class PlanarReflectionIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manager_h = PLANAR_MANAGER_H.read_text(encoding="utf-8")
        cls.manager = PLANAR_MANAGER_INL.read_text(encoding="utf-8")
        cls.bridge = PLANAR_BRIDGE.read_text(encoding="utf-8")
        cls.probe_bridge = PROBE_BRIDGE.read_text(encoding="utf-8")
        cls.component_h = PLANAR_COMPONENT_H.read_text(encoding="utf-8")
        cls.component = PLANAR_COMPONENT_INL.read_text(encoding="utf-8")
        cls.model = MODEL_COMPONENT.read_text(encoding="utf-8")
        cls.factory = COMPONENT_FACTORY.read_text(encoding="utf-8")
        cls.material_h = MATERIAL_H.read_text(encoding="utf-8")
        cls.material_cpp = MATERIAL_CPP.read_text(encoding="utf-8")
        cls.pipeline = OBJECT_PIPELINE.read_text(encoding="utf-8")
        cls.shader = OBJECT_PS.read_text(encoding="utf-8")

    def test_manager_builds_reflected_camera_from_plane(self) -> None:
        self.assertIn("ReflectPoint", self.manager)
        self.assertIn("ReflectVector", self.manager)
        self.assertIn("reflectedPosition", self.manager)
        self.assertIn("reflectedForward", self.manager)
        self.assertIn("Matrix4x4::LookAt", self.manager)
        self.assertIn("GetActiveProjectionMatrix", self.manager)
        self.assertIn("PushRenderViewOverride", self.manager)

    def test_planar_target_matches_internal_game_viewport(self) -> None:
        self.assertIn("GameViewportConstants::Width", self.manager)
        self.assertIn("GameViewportConstants::Height", self.manager)
        self.assertIn("D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET", self.manager)
        self.assertIn("CreateSRVForTexture2D", self.manager)
        self.assertIn("D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE", self.manager)

    def test_capture_is_one_surface_per_frame_and_round_robin(self) -> None:
        self.assertIn("captureCursor_", self.manager_h)
        self.assertIn("SurfaceRuntime* surface = FindCaptureCandidate()", self.manager)
        self.assertIn("return CaptureSurface(*surface, drawScene)", self.manager)
        self.assertNotIn("while (SurfaceRuntime* surface = FindCaptureCandidate()", self.manager)
        self.assertIn("PlanarReflectionUpdateMode::EveryFrame", self.manager)

    def test_capture_excludes_mirror_actors_and_avoids_recursion(self) -> None:
        self.assertIn("sceneActor == excludedReceiver", self.bridge)
        self.assertIn("HasActivePlanarSurface(sceneActor)", self.bridge)
        self.assertIn("model->DrawReflectionCapture()", self.bridge)
        self.assertNotIn("GpuParticle", self.bridge)
        self.assertIn("isCapturing_ = true", self.manager)
        self.assertIn("if (isCapturing_) return binding", self.manager)

    def test_component_uses_local_up_as_plane_normal_and_is_serializable(self) -> None:
        self.assertIn("PlanarReflectionUpdateMode::EveryFrame", self.component_h)
        self.assertIn("Vector3::Transform({ 0.0f, 1.0f, 0.0f }, rotation)", self.component)
        self.assertIn('outJson["Strength"]', self.component)
        self.assertIn('outJson["UpdateMode"]', self.component)
        self.assertIn('outJson["FlipNormal"]', self.component)
        self.assertIn("RequestCapture", self.component)
        self.assertIn("DrawPlane", self.component)

    def test_component_factory_exposes_planar_reflection(self) -> None:
        self.assertIn("PlanarReflectionComponent.h", self.factory)
        self.assertIn("MakeComponentTypeInfo<PlanarReflectionComponent>", self.factory)
        self.assertIn('"プラナーリフレクション"', self.factory)

    def test_existing_reflection_hook_schedules_planar_before_main_scene(self) -> None:
        probe_index = self.probe_bridge.index("ReflectionProbeManager::GetInstance()->CapturePending")
        planar_index = self.probe_bridge.index("PlanarReflectionSceneBridge::CapturePending")
        self.assertLess(probe_index, planar_index)
        self.assertIn("HasActivePlanarSurface", self.probe_bridge)

    def test_model_component_scopes_planar_binding_only_for_same_actor(self) -> None:
        self.assertIn("GetComponent<PlanarReflectionComponent>()", self.model)
        self.assertIn("planarManager->ResolveBinding(planar)", self.model)
        self.assertIn("PlanarReflectionManager::ScopedDrawBinding", self.model)
        self.assertIn("object3D_->Draw()", self.model)

    def test_material_layout_reuses_existing_two_float_padding(self) -> None:
        self.assertIn("float planarReflectionEnabled;", self.material_h)
        self.assertIn("float planarReflectionStrength;", self.material_h)
        self.assertNotIn("float padding[2];", self.material_h)
        self.assertIn("sizeof(Material::MaterialCBData) == 144", self.material_cpp)
        self.assertIn("GetCurrentDrawBinding", self.material_cpp)
        self.assertIn("emissiveOrPlanar", self.material_cpp)

    def test_planar_texture_reuses_object_t9_slot_without_root_growth(self) -> None:
        self.assertIn("kEmissiveSRV = 15", self.pipeline)
        self.assertIn("BaseShaderRegister = static_cast<UINT>(i + 1)", self.pipeline)
        self.assertIn("Texture2D<float4> gEmissiveTexture : register(t9)", self.shader)
        self.assertIn("planarReflectionEnabled", self.shader)
        self.assertIn("planarReflectionStrength", self.shader)

    def test_shader_uses_screen_projected_planar_sampling(self) -> None:
        self.assertIn("gEmissiveTexture.GetDimensions", self.shader)
        self.assertIn("input.position.xy / planarSize", self.shader)
        self.assertIn("SampleLevel(gLinearSampler, saturate(planarUv), 0.0f)", self.shader)
        self.assertIn("lerp(shadedColor, planarColor", self.shader)


if __name__ == "__main__":
    unittest.main()
