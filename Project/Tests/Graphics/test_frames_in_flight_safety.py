from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
ENGINE_ROOT = PROJECT_ROOT / "Engine"


class FramesInFlightSafetyTests(unittest.TestCase):
    def test_post_effects_do_not_bind_staging_constant_buffers_directly(self) -> None:
        post_effect_root = ENGINE_ROOT / "Graphics" / "PostEffect" / "Effects"
        unsafe_files: list[str] = []
        for source in post_effect_root.rglob("*.cpp"):
            text = source.read_text(encoding="utf-8")
            if "constantBuffer_->GetGPUVirtualAddress()" in text:
                unsafe_files.append(str(source.relative_to(PROJECT_ROOT)))

        self.assertEqual([], unsafe_files, f"PostEffect staging CB is still bound directly: {unsafe_files}")

    def test_wireframe_gpu_reads_frame_upload_copies(self) -> None:
        wireframe = ENGINE_ROOT / "Graphics" / "Renderer" / "Wireframe" / "Core" / "Wireframe.cpp"
        text = wireframe.read_text(encoding="utf-8")
        self.assertIn("GetFrameUploadArena()", text)
        self.assertNotIn("transformationMatrixBuffer_->GetGPUVirtualAddress()", text)
        self.assertNotIn("boxWireInstancedData_->instanceBufferView", text)
        self.assertNotIn("sphereInstancedData_->instanceBufferView", text)
        self.assertNotIn("capsuleInstancedData_->instanceBufferView", text)

    def test_skin_palette_copy_uses_frame_owned_upload_memory(self) -> None:
        skin_cluster = ENGINE_ROOT / "Graphics" / "Renderer" / "Animation" / "Skinning" / "SkinCluster.cpp"
        text = skin_cluster.read_text(encoding="utf-8")
        self.assertIn("paletteUpload.resource", text)
        self.assertIn("paletteUpload.resourceOffsetBytes", text)

    def test_gpu_timestamp_profiler_is_present(self) -> None:
        pipeline = ENGINE_ROOT / "Graphics" / "Pipeline" / "RenderPipelineController.cpp"
        text = pipeline.read_text(encoding="utf-8")
        self.assertIn("D3D12_QUERY_HEAP_TYPE_TIMESTAMP", text)
        self.assertIn("ResolveQueryData", text)
        self.assertIn("GetTimestampFrequency", text)

    def test_runtime_toggle_distinguishes_requested_and_active_state(self) -> None:
        dx_common = ENGINE_ROOT / "Graphics" / "Device" / "Facade" / "DirectXCommon.h"
        text = dx_common.read_text(encoding="utf-8")
        # ON/OFF要求と適用済み状態を分け、記録中フレームの同期方式を途中変更させない。
        self.assertIn("requestedFramesInFlightEnabled_", text)
        self.assertIn("IsFramesInFlightActive", text)


if __name__ == "__main__":
    unittest.main()
