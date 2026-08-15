from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PLANAR_MANAGER_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "PlanarReflectionManager.h"
PLANAR_MANAGER_INL = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "PlanarReflectionManager.inl"
PLANAR_BRIDGE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "PlanarReflectionSceneBridge.h"
PROBE_BRIDGE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "ReflectionProbeSceneBridge.h"
PLANAR_COMPONENT_H = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "PlanarReflectionComponent.h"
PLANAR_COMPONENT_INL = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "PlanarReflectionComponent.inl"
MODEL_COMPONENT_H = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.h"
MODEL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.cpp"
OBJECT3D_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3D.h"
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
        cls.model_h = MODEL_COMPONENT_H.read_text(encoding="utf-8")
        cls.model = MODEL_COMPONENT.read_text(encoding="utf-8")
        cls.object3d_h = OBJECT3D_H.read_text(encoding="utf-8")
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

    def test_component_auto_fits_to_real_receiver_surface_and_serializes_controls(self) -> None:
        self.assertIn("autoFitToReceiverSurface_ = true", self.component_h)
        self.assertIn("GetPlanePosition() const", self.component_h)
        self.assertIn("TryGetReflectionReceiverSurfacePoint", self.component)
        self.assertIn("GetComponents<ModelComponent>()", self.component)
        self.assertIn('outJson["AutoFitToReceiverSurface"]', self.component)
        self.assertIn('outJson["PlaneOffset"]', self.component)
        self.assertIn('outJson["SurfaceTolerance"]', self.component)
        self.assertIn("desc.position = GetPlanePosition()", self.component)
        self.assertIn("DrawPlane", self.component)

    def test_model_support_point_uses_real_vertices_instead_of_bounding_sphere(self) -> None:
        self.assertIn("TryGetSupportPointAlongWorldDirection", self.object3d_h)
        self.assertIn("model_->GetModelData().subMeshes", self.object3d_h)
        self.assertIn("for (const VertexData& vertex", self.object3d_h)
        self.assertIn("Vector3::Transform(localPosition, world)", self.object3d_h)
        self.assertIn("TryGetReflectionReceiverSurfacePoint", self.model_h)

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

    def test_manager_reuses_exact_view_projection_used_for_capture(self) -> None:
        self.assertIn("Matrix4x4 capturedViewProjection", self.manager_h)
        self.assertIn("surface.capturedViewProjection = reflectedView.viewProjection", self.manager)
        self.assertIn("binding.reflectedViewProjection = surface->capturedViewProjection", self.manager)
        self.assertIn("binding.planePosition = surface->desc.position", self.manager)
        self.assertIn("binding.surfaceTolerance = surface->desc.surfaceTolerance", self.manager)
        self.assertNotIn("BuildPlanarReflectionViewProjection", self.model)
        self.assertNotIn("ReflectPoint", self.model)

    def test_material_layout_carries_planar_plane_and_tolerance_without_root_growth(self) -> None:
        self.assertIn("float planarReflectionEnabled;", self.material_h)
        self.assertIn("float planarReflectionStrength;", self.material_h)
        self.assertIn("Matrix4x4 planarReflectionViewProjection;", self.material_h)
        self.assertIn("Vector4 planarReflectionPlane;", self.material_h)
        self.assertIn("Vector4 planarReflectionSurfaceParams;", self.material_h)
        self.assertIn("sizeof(Material::MaterialCBData) == 240", self.material_cpp)
        self.assertIn("planarBinding.planePosition", self.material_cpp)
        self.assertIn("planarBinding.surfaceTolerance", self.material_cpp)
        self.assertIn("emissiveOrPlanar", self.material_cpp)

    def test_planar_texture_reuses_object_t9_slot_without_root_growth(self) -> None:
        self.assertIn("kEmissiveSRV = 15", self.pipeline)
        self.assertIn("BaseShaderRegister = static_cast<UINT>(i + 1)", self.pipeline)
        self.assertIn("Texture2D<float4> gEmissiveTexture : register(t9)", self.shader)
        self.assertIn("planarReflectionEnabled", self.shader)
        self.assertIn("planarReflectionStrength", self.shader)

    def test_shader_uses_reflected_view_projection_instead_of_screen_copy(self) -> None:
        self.assertIn("gMaterial.planarReflectionViewProjection", self.shader)
        self.assertIn("mul(float4(worldPosition, 1.0f), gMaterial.planarReflectionViewProjection)", self.shader)
        self.assertIn("reflectedClip.xy / reflectedClip.w", self.shader)
        self.assertIn("SampleLevel(gLinearSampler, planarUv, 0.0f)", self.shader)
        self.assertNotIn("input.position.xy / planarSize", self.shader)
        self.assertNotIn("planarUv.x = 1.0f - planarUv.x", self.shader)

    def test_shader_masks_faces_by_normal_and_actual_plane_distance(self) -> None:
        self.assertIn("planarReflectionPlane", self.shader)
        self.assertIn("abs(dot(normal, planarNormal))", self.shader)
        self.assertIn("kPlanarNormalAlignmentThreshold", self.shader)
        self.assertIn("planeDistance = abs(dot(float4(worldPosition, 1.0f), gMaterial.planarReflectionPlane))", self.shader)
        self.assertIn("planeTolerance", self.shader)
        self.assertIn("planeDistance <= planeTolerance", self.shader)


if __name__ == "__main__":
    unittest.main()
