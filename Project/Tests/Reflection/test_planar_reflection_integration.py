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

    def test_manager_uses_directx_row_vector_oblique_near_plane(self) -> None:
        self.assertIn("BuildObliqueProjection", self.manager)
        self.assertIn("Matrix4x4::TryInverse(projection, inverseProjection)", self.manager)
        self.assertIn("farClipCorner", self.manager)
        self.assertIn("TransformHomogeneousRow", self.manager)
        self.assertIn("oblique.m[0][2] = planeView.x * scale", self.manager)
        self.assertIn("oblique.m[1][2] = planeView.y * scale", self.manager)
        self.assertIn("oblique.m[2][2] = planeView.z * scale", self.manager)
        self.assertIn("oblique.m[3][2] = planeView.w * scale", self.manager)
        self.assertIn("keepSideNormal", self.manager)
        self.assertIn("cameraPosition - surface.desc.position", self.manager)
        self.assertIn("surface.obliqueClipApplied = obliqueClipApplied", self.manager)

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
        self.assertIn("dynamic_cast<ReflectionCaptureDrawable*>", self.bridge)
        self.assertIn("drawable->DrawReflectionCapture()", self.bridge)  # Capture対応Componentを個別列挙せず描画する契約を固定する。
        self.assertNotIn("GpuParticle", self.bridge)
        self.assertIn("isCapturing_ = true", self.manager)
        self.assertIn("if (isCapturing_) return binding", self.manager)

    def test_bridge_leaves_back_side_rejection_to_oblique_projection(self) -> None:
        self.assertNotIn("IsFullyBehindMirrorPlane", self.bridge)
        self.assertNotIn("BoundingSphere", self.bridge)
        self.assertIn("Oblique Near Plane", self.bridge)

    def test_component_auto_fits_to_real_receiver_surface_and_serializes_controls(self) -> None:
        self.assertIn("autoFitToReceiverSurface_ = true", self.component_h)
        self.assertIn("clipPlaneBias_ = 0.01f", self.component_h)
        self.assertIn("GetPlanePosition() const", self.component_h)
        self.assertIn("TryGetReflectionReceiverSurfacePoint", self.component)
        self.assertIn("GetComponents<ModelComponent>()", self.component)
        self.assertIn('outJson["AutoFitToReceiverSurface"]', self.component)
        self.assertIn('outJson["PlaneOffset"]', self.component)
        self.assertIn('outJson["SurfaceTolerance"]', self.component)
        self.assertIn('outJson["ClipPlaneBias"]', self.component)
        self.assertIn("desc.position = GetPlanePosition()", self.component)
        self.assertIn("desc.clipPlaneBias = clipPlaneBias_", self.component)
        self.assertIn("Oblique Clip", self.component)
        self.assertIn("DrawPlane", self.component)

    def test_component_has_six_axis_face_presets(self) -> None:
        self.assertIn("enum class PlanarReflectionFacePreset", self.component_h)
        for face in ("PositiveX", "NegativeX", "PositiveY", "NegativeY", "PositiveZ", "NegativeZ"):
            self.assertIn(face, self.component_h)
            self.assertIn(f"PlanarReflectionFacePreset::{face}", self.component)
        for label in ('"+X##PlanarReflectionFace"', '"-X##PlanarReflectionFace"',
                      '"+Y##PlanarReflectionFace"', '"-Y##PlanarReflectionFace"',
                      '"+Z##PlanarReflectionFace"', '"-Z##PlanarReflectionFace"'):
            self.assertIn(label, self.component)
        self.assertIn("SyncToManager(true)", self.component)

    def test_model_support_point_uses_real_vertices_instead_of_bounding_sphere(self) -> None:
        self.assertIn("TryGetSupportPointAlongWorldDirection", self.object3d_h)
        self.assertIn("model_->GetModelData().subMeshes", self.object3d_h)
        self.assertIn("for (const VertexData& vertex", self.object3d_h)
        self.assertIn("Vector3::Transform(localPosition, world)", self.object3d_h)
        self.assertIn("TryGetReflectionReceiverSurfacePoint", self.model_h)

    def test_component_factory_allows_multiple_planar_reflections(self) -> None:
        self.assertIn("PlanarReflectionComponent.h", self.factory)
        self.assertIn("MakeComponentTypeInfo<PlanarReflectionComponent>", self.factory)
        self.assertIn('MakeComponentTypeInfo<PlanarReflectionComponent>("PlanarReflectionComponent", true', self.factory)
        self.assertIn('"プラナーリフレクション"', self.factory)

    def test_existing_reflection_hook_schedules_planar_before_main_scene(self) -> None:
        probe_index = self.probe_bridge.index("ReflectionProbeManager::GetInstance()->CapturePending")
        planar_index = self.probe_bridge.index("PlanarReflectionSceneBridge::CapturePending")
        self.assertLess(probe_index, planar_index)
        self.assertIn("HasActivePlanarSurface", self.probe_bridge)

    def test_model_component_packs_up_to_six_surfaces_for_same_actor(self) -> None:
        self.assertIn("PlanarReflectionDrawSet planarDrawSet", self.model)
        self.assertIn("GetComponents<PlanarReflectionComponent>()", self.model)
        self.assertIn("planarDrawSet.Add(planarManager->ResolveBinding(planar))", self.model)
        self.assertIn("kMaxPlanarReflectionSurfacesPerDraw", self.model)
        self.assertIn("PlanarReflectionManager::ScopedDrawBinding", self.model)
        self.assertIn("object3D_->Draw()", self.model)
        self.assertNotIn("GetComponent<PlanarReflectionComponent>()", self.model)

    def test_manager_reuses_exact_oblique_view_projection_used_for_capture(self) -> None:
        self.assertIn("Matrix4x4 capturedViewProjection", self.manager_h)
        self.assertIn("reflectedView.projection = PlanarReflectionDetail::BuildObliqueProjection", self.manager)
        self.assertIn("surface.capturedViewProjection = reflectedView.viewProjection", self.manager)
        self.assertIn("binding.reflectedViewProjection = surface->capturedViewProjection", self.manager)
        self.assertIn("binding.planePosition = surface->desc.position", self.manager)
        self.assertIn("binding.surfaceTolerance = surface->desc.surfaceTolerance", self.manager)
        self.assertNotIn("BuildPlanarReflectionViewProjection", self.model)
        self.assertNotIn("ReflectPoint", self.model)

    def test_manager_builds_six_surface_draw_packet_and_transient_descriptor_table(self) -> None:
        self.assertIn("kMaxPlanarReflectionSurfacesPerDraw = 6u", self.manager_h)
        self.assertIn("struct PlanarReflectionDrawSet", self.manager_h)
        self.assertIn("struct PlanarReflectionDrawCBData", self.manager_h)
        self.assertIn("std::array<Matrix4x4, kMaxPlanarReflectionSurfacesPerDraw>", self.manager_h)
        self.assertIn("AllocateTransient(kMaxPlanarReflectionSurfacesPerDraw)", self.manager)
        self.assertIn("CreateShaderResourceView", self.manager)
        self.assertIn("BindCurrentDrawState", self.manager)
        self.assertIn("SetGraphicsRootConstantBufferView(constantBufferRootParameterIndex", self.manager)
        self.assertIn("SetGraphicsRootDescriptorTable(rootParameterIndex, allocation.gpuHandle)", self.manager)

    def test_transient_planar_table_never_reads_shader_visible_descriptor_heap(self) -> None:
        self.assertIn("ID3D12Resource* resource = nullptr", self.manager_h)
        self.assertIn("binding.resource = surface->target->color.Get()", self.manager)
        self.assertIn("sourceBinding.resource", self.manager)
        self.assertIn("CreateShaderResourceView", self.manager)
        self.assertNotIn("CopyDescriptorsSimple(", self.manager)
        self.assertNotIn("GetCPUDescriptorHandle(sourceSrvIndex)", self.manager)

    def test_object_root_signature_reserves_b7_and_t12_to_t17_for_planar(self) -> None:
        self.assertIn("kPlanarReflectionCBV = 19", self.pipeline)
        self.assertIn("kPlanarReflectionSRVTable = 20", self.pipeline)
        self.assertIn("BaseShaderRegister = 12", self.pipeline)
        self.assertIn("NumDescriptors = 6", self.pipeline)
        self.assertIn("ShaderRegister = 7", self.pipeline)
        self.assertIn("std::array<D3D12_DESCRIPTOR_RANGE, 12>", self.pipeline)

    def test_material_keeps_emissive_t9_and_binds_planar_dedicated_slots(self) -> None:
        self.assertIn("sizeof(Material::MaterialCBData) == 240", self.material_cpp)
        self.assertIn("SetGraphicsRootDescriptorTable(emissiveRootIndex, emissive_)", self.material_cpp)
        self.assertNotIn("emissiveOrPlanar", self.material_cpp)
        self.assertIn("BindCurrentDrawState(commandList, 19, 20)", self.material_cpp)
        self.assertIn("Texture2D<float4> gEmissiveTexture : register(t9)", self.shader)

    def test_shader_declares_multi_surface_planar_data_and_texture_array(self) -> None:
        self.assertIn("kMaxPlanarReflectionSurfaces = 6u", self.shader)
        self.assertIn("struct PlanarReflectionDrawData", self.shader)
        self.assertIn("ConstantBuffer<PlanarReflectionDrawData> gPlanarReflection : register(b7)", self.shader)
        self.assertIn("gPlanarReflectionTextures[kMaxPlanarReflectionSurfaces] : register(t12)", self.shader)
        self.assertIn("SamplePlanarReflectionTexture", self.shader)
        self.assertIn("switch (surfaceIndex)", self.shader)

    def test_shader_selects_matching_surface_by_normal_distance_and_projective_uv(self) -> None:
        self.assertIn("gPlanarReflection.surfaceCount", self.shader)
        self.assertIn("for (uint surfaceIndex = 0u; surfaceIndex < kMaxPlanarReflectionSurfaces", self.shader)
        self.assertIn("gPlanarReflection.plane[surfaceIndex]", self.shader)
        self.assertIn("abs(dot(normal, planarNormal))", self.shader)
        self.assertIn("planeDistance >= selectedPlaneDistance", self.shader)
        self.assertIn("gPlanarReflection.reflectedViewProjection[surfaceIndex]", self.shader)
        self.assertIn("selectedSurface = (int)surfaceIndex", self.shader)
        self.assertIn("SamplePlanarReflectionTexture(selectedIndex, selectedUv)", self.shader)
        self.assertNotIn("input.position.xy / planarSize", self.shader)
        self.assertNotIn("planarUv.x = 1.0f - planarUv.x", self.shader)


if __name__ == "__main__":
    unittest.main()