from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
CAMERA_MANAGER_H = PROJECT_ROOT / "Engine" / "Graphics" / "Camera" / "Manager" / "CameraManager.h"
CAMERA_MANAGER_CPP = PROJECT_ROOT / "Engine" / "Graphics" / "Camera" / "Manager" / "CameraManager.cpp"
WORLD_TRANSFORM = PROJECT_ROOT / "Engine" / "Core" / "Transform" / "WorldTransform.cpp"
PROBE_MANAGER_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "ReflectionProbeManager.h"
PROBE_MANAGER_INL = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "ReflectionProbeManager.inl"
PROBE_BRIDGE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Reflection" / "ReflectionProbeSceneBridge.h"
PROBE_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ReflectionProbeComponent.h"
PROBE_COMPONENT_INL = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ReflectionProbeComponent.inl"
MODEL_COMPONENT = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Components" / "ModelComponent.cpp"
COMPONENT_FACTORY = PROJECT_ROOT / "Engine" / "Scene" / "Actor" / "Serialization" / "ComponentFactory.cpp"
GAME_APPLICATION = PROJECT_ROOT / "Engine" / "Core" / "Application" / "GameApplication.cpp"
ENVIRONMENT_MANAGER = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Environment" / "EnvironmentMapManager.h"
EDITOR_CONTEXT = PROJECT_ROOT / "Engine" / "Editor" / "EditorContext.h"


class ReflectionProbeIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.camera_h = CAMERA_MANAGER_H.read_text(encoding="utf-8")
        cls.camera_cpp = CAMERA_MANAGER_CPP.read_text(encoding="utf-8")
        cls.world_transform = WORLD_TRANSFORM.read_text(encoding="utf-8")
        cls.manager_h = PROBE_MANAGER_H.read_text(encoding="utf-8")
        cls.manager = PROBE_MANAGER_INL.read_text(encoding="utf-8")
        cls.bridge = PROBE_BRIDGE.read_text(encoding="utf-8")
        cls.component = PROBE_COMPONENT.read_text(encoding="utf-8")
        cls.component_inl = PROBE_COMPONENT_INL.read_text(encoding="utf-8")
        cls.model = MODEL_COMPONENT.read_text(encoding="utf-8")
        cls.factory = COMPONENT_FACTORY.read_text(encoding="utf-8")
        cls.game = GAME_APPLICATION.read_text(encoding="utf-8")
        cls.environment = ENVIRONMENT_MANAGER.read_text(encoding="utf-8")
        cls.editor_context = EDITOR_CONTEXT.read_text(encoding="utf-8")

    def test_camera_manager_has_scoped_render_view_override_contract(self) -> None:
        self.assertIn("struct RenderViewOverride", self.camera_h)
        self.assertIn("PushRenderViewOverride", self.camera_h)
        self.assertIn("PopRenderViewOverride", self.camera_h)
        self.assertIn("GetActiveRenderViewOverride", self.camera_cpp)
        self.assertIn("renderViewOverrides_.back()", self.camera_cpp)

    def test_world_transform_snapshots_each_multi_view_draw(self) -> None:
        self.assertIn("GetFrameUploadArena().AllocateConstant(transformationData_)", self.world_transform)
        self.assertIn("allocation.gpuAddress", self.world_transform)
        self.assertNotIn("transformationBuffers_.WriteFrame(frameIndex, transformationData_)", self.world_transform)

    def test_probe_target_is_texture_cube_with_six_render_faces(self) -> None:
        self.assertIn("kCubeFaceCount = 6", self.manager)
        self.assertIn("DepthOrArraySize = static_cast<UINT16>(ReflectionProbeDetail::kCubeFaceCount)", self.manager)
        self.assertIn("D3D12_RTV_DIMENSION_TEXTURE2DARRAY", self.manager)
        self.assertIn("D3D12_SRV_DIMENSION_TEXTURECUBE", self.manager)
        self.assertIn("FirstArraySlice = face", self.manager)
        self.assertIn("std::numbers::pi_v<float> * 0.5f", self.manager)

    def test_probe_capture_is_bounded_to_one_probe_per_frame(self) -> None:
        self.assertIn("ProbeRuntime* probe = FindCaptureCandidate()", self.manager)
        self.assertIn("return CaptureProbe(*probe, drawStaticScene)", self.manager)
        self.assertNotIn("while (ProbeRuntime* probe = FindCaptureCandidate()", self.manager)

    def test_probe_capture_uses_global_environment_to_avoid_feedback(self) -> None:
        self.assertIn("isCapturing_ = true", self.manager)
        self.assertIn("GetGlobalEnvironmentMapHandle", self.manager)
        self.assertIn("GetGlobalEnvironmentMapHandle", self.environment)
        self.assertIn("ScopedDrawOverride", self.environment)

    def test_model_draw_resolves_nearest_probe_and_refreshes_current_view(self) -> None:
        self.assertIn("ReflectionProbeManager::GetInstance()->ResolveReflectionHandle(GetWorldPosition())", self.model)
        self.assertIn("EnvironmentMapManager::ScopedDrawOverride", self.model)
        self.assertIn("PrepareForCurrentRenderView", self.model)
        self.assertIn("object3D_->Update(); // Probe", self.model)
        self.assertIn("this,", self.model)
        self.assertIn("static_cast<ModelComponent*>(payload)->DrawWithReflectionBinding()", self.model)

    def test_initial_capture_is_static_opaque_and_masked_only(self) -> None:
        self.assertIn("DrawReflectionCapture", self.model)
        self.assertIn("MaterialBlendMode::Transparent", self.model)
        self.assertIn("MaterialBlendMode::Additive", self.model)
        self.assertIn("model->DrawReflectionCapture()", self.bridge)
        self.assertNotIn("GpuParticle", self.bridge)
        self.assertNotIn("AnimatedModelComponent", self.bridge)

    def test_component_supports_modes_json_and_manual_recapture(self) -> None:
        self.assertIn("ReflectionProbeUpdateMode::Static", self.component_inl)
        self.assertIn("ReflectionProbeUpdateMode::OnDemand", self.component_inl)
        self.assertIn("ReflectionProbeUpdateMode::EveryFrame", self.component_inl)
        self.assertIn('outJson["InfluenceRadius"]', self.component_inl)
        self.assertIn('outJson["Resolution"]', self.component_inl)
        self.assertIn('outJson["UpdateMode"]', self.component_inl)
        self.assertIn("RequestCapture", self.component)
        self.assertIn("再キャプチャ", self.component_inl)

    def test_component_is_serializable_and_editor_placeable_contract_exists(self) -> None:
        self.assertIn("ReflectionProbeComponent.h", self.factory)
        self.assertIn('MakeComponentTypeInfo<ReflectionProbeComponent>', self.factory)
        self.assertIn("ReflectionProbe", self.editor_context)

    def test_probe_capture_runs_before_main_scene_render_target(self) -> None:
        capture_index = self.game.index("ReflectionProbeSceneBridge::CapturePending")
        main_target_index = self.game.index("PostEffectManager::GetInstance()->BeginDraw();", capture_index)
        self.assertLess(capture_index, main_target_index)
        self.assertIn("ReflectionProbeManager::GetInstance()->Initialize(dxCommon_)", self.game)
        self.assertIn("ReflectionProbeManager::GetInstance()->Finalize()", self.game)


if __name__ == "__main__":
    unittest.main()
