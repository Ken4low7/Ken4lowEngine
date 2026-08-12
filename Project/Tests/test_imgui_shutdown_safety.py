from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
IMGUI_MANAGER = PROJECT_ROOT / "Engine" / "DebugTools" / "ImGui" / "ImGuiManager.cpp"


class ImGuiShutdownSafetyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = IMGUI_MANAGER.read_text(encoding="utf-8")

    def test_free_callback_does_not_throw_during_backend_shutdown(self) -> None:
        # Dear ImGuiのDX12解放CallbackからC++例外を外へ伝播させないことを固定する。
        start = self.source.index("void ImGuiManager::FreeImGuiSrvDescriptor")
        end = self.source.index("#endif // USE_IMGUI", start)
        callback = self.source[start:end]
        self.assertNotIn("throw std::runtime_error", callback)
        self.assertIn("FreePersistentSrvNoThrow(srvIndex);", callback)

    def test_descriptor_tracking_is_removed_before_srv_release(self) -> None:
        start = self.source.index("void ImGuiManager::FreeImGuiSrvDescriptor")
        end = self.source.index("#endif // USE_IMGUI", start)
        callback = self.source[start:end]
        self.assertLess(
            callback.index("imguiSrvHandleToIndex_.erase(it)"),
            callback.index("FreePersistentSrvNoThrow(srvIndex)"),
        )

    def test_finalize_uses_non_throwing_fallback_release(self) -> None:
        start = self.source.index("void ImGuiManager::Finalize()")
        finalize = self.source[start:]
        self.assertIn("ImGui_ImplDX12_Shutdown();", finalize)
        self.assertIn("FreePersistentSrvNoThrow(srvEntry.second);", finalize)
        self.assertIn("ImGui::DestroyContext();", finalize)

    def test_non_throwing_release_logs_failures_instead_of_terminating(self) -> None:
        self.assertIn("void FreePersistentSrvNoThrow(uint32_t srvIndex) noexcept", self.source)
        self.assertIn("catch (const std::exception& exception)", self.source)
        self.assertIn("ReportImGuiDescriptorReleaseIssue(exception.what());", self.source)


if __name__ == "__main__":
    unittest.main()
