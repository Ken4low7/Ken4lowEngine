from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]


class GraphicsShutdownOrderTests(unittest.TestCase):
    def test_render_targets_release_descriptors_before_srv_heap_shutdown(self):
        # Shutdown contract: render targets must return descriptor indices before DirectXCommon destroys descriptor heaps.
        framework = (PROJECT_ROOT / "Engine/Core/Application/Framework.cpp").read_text(encoding="utf-8")
        directx = (PROJECT_ROOT / "Engine/Graphics/Device/Facade/DirectXCommon.cpp").read_text(encoding="utf-8")
        shadow = (PROJECT_ROOT / "Engine/Graphics/RenderTarget/Shadow/ShadowMapRenderTarget.cpp").read_text(encoding="utf-8")

        self.assertNotIn("SRVManager::GetInstance()->Finalize();", framework)
        self.assertIn("dxCommon_->Finalize();", framework)
        self.assertIn("srvManager->Free(shadowMapSrvIndex_);", shadow)

        shadow_finalize = directx.index("shadowMapRenderTarget_->Finalize();")
        srv_finalize = directx.index("SRVManager::GetInstance()->Finalize();")
        self.assertLess(shadow_finalize, srv_finalize)

    def test_directx_common_owns_descriptor_manager_shutdown(self):
        # Keep RTV/DSV/SRV teardown adjacent to the render-target/device lifecycle instead of Framework.
        directx = (PROJECT_ROOT / "Engine/Graphics/Device/Facade/DirectXCommon.cpp").read_text(encoding="utf-8")
        self.assertIn("RTVManager::GetInstance()->Finalize();", directx)
        self.assertIn("DSVManager::GetInstance()->Finalize();", directx)
        self.assertIn("SRVManager::GetInstance()->Finalize();", directx)


if __name__ == "__main__":
    unittest.main()
