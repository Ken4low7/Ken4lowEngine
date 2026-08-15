from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
ENVIRONMENT_MANAGER = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Environment" / "EnvironmentMapManager.h"
OBJECT3D = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3D.cpp"
INSTANCED = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "InstancedObject3DRenderer.cpp"
ANIMATION = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Animation" / "Core" / "AnimationModel.cpp"
SKYBOX = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "SkyBox" / "SkyBox.cpp"


class EnvironmentMapBindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.environment_manager = ENVIRONMENT_MANAGER.read_text(encoding="utf-8")
        cls.object3d = OBJECT3D.read_text(encoding="utf-8")
        cls.instanced = INSTANCED.read_text(encoding="utf-8")
        cls.animation = ANIMATION.read_text(encoding="utf-8")
        cls.skybox = SKYBOX.read_text(encoding="utf-8")

    def test_environment_map_has_one_scene_wide_owner(self) -> None:
        self.assertIn("class EnvironmentMapManager", self.environment_manager)
        self.assertIn("SetSkyBoxEnvironment", self.environment_manager)
        self.assertIn("SetEnvironmentMapOverride", self.environment_manager)
        self.assertIn("UseSkyBoxEnvironment", self.environment_manager)
        self.assertIn("GetEnvironmentMapHandle", self.environment_manager)
        self.assertIn("GetRevision", self.environment_manager)

    def test_environment_map_rejects_non_cubemap_textures(self) -> None:
        self.assertIn("metadata.IsCubemap()", self.environment_manager)
        self.assertIn("D3D12_SRV_DIMENSION_TEXTURECUBE", self.environment_manager)

    def test_skybox_texture_changes_feed_the_scene_environment(self) -> None:
        self.assertIn("EnvironmentMapManager.h", self.skybox)
        self.assertIn("EnvironmentMapManager::GetInstance()->SetSkyBoxEnvironment(filePath)", self.skybox)

    def test_static_and_instanced_draws_resolve_environment_at_draw_time(self) -> None:
        for renderer in (self.object3d, self.instanced):
            self.assertIn("EnvironmentMapManager.h", renderer)
            self.assertIn("EnvironmentMapManager::GetInstance()->GetEnvironmentMapHandle()", renderer)
            self.assertNotIn('LoadTexture("SkyBox/skybox.dds")', renderer)
            self.assertNotIn('GetSrvHandleGPU("SkyBox/skybox.dds")', renderer)

    def test_animation_legacy_handle_is_mirrored_by_the_shared_manager(self) -> None:
        # AnimationModelの巨大な既存描画経路は壊さず、旧descriptor slotだけをScene共通Environmentへ向け直す。
        self.assertIn("MirrorIntoLegacyBinding", self.environment_manager)
        self.assertIn("CreateShaderResourceView", self.environment_manager)
        self.assertIn("GetFallbackEnvironmentMapPath", self.environment_manager)
        self.assertIn("environmentMapHandle_", self.animation)

    def test_explicit_override_can_diverge_from_visual_skybox(self) -> None:
        self.assertIn("explicitOverrideEnabled_", self.environment_manager)
        self.assertIn("if (explicitOverrideEnabled_)", self.environment_manager)
        self.assertIn("return UseSkyBoxEnvironment()", self.environment_manager)


if __name__ == "__main__":
    unittest.main()
