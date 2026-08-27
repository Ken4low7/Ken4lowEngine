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
        self.assertIn("TextureCubeを要求するShaderへ2D Textureを誤Bindしない", self.environment_manager)

    def test_skybox_texture_changes_feed_the_scene_environment(self) -> None:
        self.assertIn("EnvironmentMapManager.h", self.skybox)
        self.assertIn("EnvironmentMapManager::GetInstance()->SetSkyBoxEnvironment(filePath)", self.skybox)

    def test_all_surface_renderers_resolve_environment_at_draw_time(self) -> None:
        for renderer in (self.object3d, self.instanced, self.animation):
            self.assertIn("EnvironmentMapManager.h", renderer)
            self.assertIn("EnvironmentMapManager::GetInstance()->GetEnvironmentMapHandle()", renderer)
            self.assertNotIn('LoadTexture("SkyBox/skybox.dds")', renderer)
            self.assertNotIn('GetSrvHandleGPU("SkyBox/skybox.dds")', renderer)

    def test_environment_switch_does_not_mutate_live_descriptors(self) -> None:
        self.assertNotIn("MirrorIntoLegacyBinding", self.environment_manager)
        self.assertNotIn("CreateShaderResourceView", self.environment_manager)
        self.assertNotIn("CopyDescriptorsSimple", self.environment_manager)
        self.assertIn("Frames in Flight中のdescriptor書き換えは行わない", self.environment_manager)

    def test_fallback_path_is_centralized_in_environment_manager(self) -> None:
        self.assertIn('static const std::string path = "SkyBox/skybox.dds"', self.environment_manager)
        for renderer in (self.object3d, self.instanced, self.animation):
            self.assertNotIn('"SkyBox/skybox.dds"', renderer)

    def test_explicit_override_can_diverge_from_visual_skybox(self) -> None:
        self.assertIn("explicitOverrideEnabled_", self.environment_manager)
        self.assertIn("if (explicitOverrideEnabled_)", self.environment_manager)
        self.assertIn("return UseSkyBoxEnvironment()", self.environment_manager)


if __name__ == "__main__":
    unittest.main()
