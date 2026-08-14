from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
CULLING_DIAGNOSTICS = PROJECT_ROOT / "Engine" / "Graphics" / "Culling" / "CullingDiagnostics.h"
NORMAL_CONE = PROJECT_ROOT / "Engine" / "Graphics" / "Culling" / "NormalCone.h"
MESH_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "Mesh" / "Mesh.h"
MESH_SOURCE = MESH_HEADER.with_suffix(".cpp")
OBJECT_COMMON = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3DCommon.cpp"
ANIMATION_PIPELINE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Animation" / "Pipeline" / "AnimationPipelineBuilder.cpp"


class CullingDiagnosticsFoundationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.diagnostics = CULLING_DIAGNOSTICS.read_text(encoding="utf-8")
        cls.normal_cone = NORMAL_CONE.read_text(encoding="utf-8")
        cls.mesh_h = MESH_HEADER.read_text(encoding="utf-8")
        cls.mesh_cpp = MESH_SOURCE.read_text(encoding="utf-8")
        cls.object_common = OBJECT_COMMON.read_text(encoding="utf-8")
        cls.animation_pipeline = ANIMATION_PIPELINE.read_text(encoding="utf-8")

    def test_main_pass_culling_diagnostics_track_surface_modes_and_triangles(self) -> None:
        self.assertIn("BeginMainPass", self.diagnostics)
        self.assertIn("SetActiveSurface", self.diagnostics)
        self.assertIn("RecordIndexedDraw", self.diagnostics)
        self.assertIn("trianglesByCullMode", self.diagnostics)
        self.assertIn("pipelineBindsByPath", self.diagnostics)
        self.assertIn("GetEstimatedRasterizerRejectedTriangles", self.diagnostics)

    def test_mesh_builds_normal_cone_metadata_without_enabling_runtime_rejection(self) -> None:
        self.assertIn("struct NormalCone", self.normal_cone)
        self.assertIn("BuildNormalCone", self.normal_cone)
        self.assertIn("IsBackfaceCullCandidate", self.normal_cone)
        self.assertIn("minDot > 0.0f", self.normal_cone)
        self.assertIn("NormalCone normalCone_", self.mesh_h)
        self.assertIn("normalCone_ = BuildNormalCone", self.mesh_cpp)
        self.assertIn("RecordIndexedDraw", self.mesh_cpp)

    def test_main_render_paths_publish_active_surface_state(self) -> None:
        self.assertIn("SurfacePath::Static", self.object_common)
        self.assertIn("SurfacePath::Alpha", self.object_common)
        self.assertIn("SurfacePath::Instanced", self.object_common)
        self.assertIn("SurfacePath::Animated", self.animation_pipeline)
        self.assertIn("Culling Statistics", self.object_common)
        self.assertIn("Runtime Normal Cone Culling: OFF", self.object_common)


if __name__ == "__main__":
    unittest.main()
