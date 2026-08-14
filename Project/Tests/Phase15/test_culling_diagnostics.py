from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
CULLING_DIAGNOSTICS = PROJECT_ROOT / "Engine" / "Graphics" / "Culling" / "CullingDiagnostics.h"
NORMAL_CONE = PROJECT_ROOT / "Engine" / "Graphics" / "Culling" / "NormalCone.h"
MESHLET_VISIBILITY = PROJECT_ROOT / "Engine" / "Graphics" / "Culling" / "MeshletVisibility.h"
MESH_HEADER = PROJECT_ROOT / "Engine" / "Graphics" / "Mesh" / "Mesh.h"
MESH_SOURCE = MESH_HEADER.with_suffix(".cpp")
OBJECT_COMMON_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3DCommon.h"
OBJECT_COMMON = OBJECT_COMMON_H.with_suffix(".cpp")
ANIMATION_PIPELINE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Animation" / "Pipeline" / "AnimationPipelineBuilder.cpp"
ANIMATION_LOD_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Animation" / "LOD" / "AnimationModelLODBuilder.h"
ANIMATION_LOD_CPP = ANIMATION_LOD_H.with_suffix(".cpp")
GAME_APPLICATION = PROJECT_ROOT / "Engine" / "Core" / "Application" / "GameApplication.cpp"


class CullingDiagnosticsFoundationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.diagnostics = CULLING_DIAGNOSTICS.read_text(encoding="utf-8")
        cls.normal_cone = NORMAL_CONE.read_text(encoding="utf-8")
        cls.meshlet_visibility = MESHLET_VISIBILITY.read_text(encoding="utf-8")
        cls.mesh_h = MESH_HEADER.read_text(encoding="utf-8")
        cls.mesh_cpp = MESH_SOURCE.read_text(encoding="utf-8")
        cls.object_common_h = OBJECT_COMMON_H.read_text(encoding="utf-8")
        cls.object_common = OBJECT_COMMON.read_text(encoding="utf-8")
        cls.animation_pipeline = ANIMATION_PIPELINE.read_text(encoding="utf-8")
        cls.animation_lod_h = ANIMATION_LOD_H.read_text(encoding="utf-8")
        cls.animation_lod_cpp = ANIMATION_LOD_CPP.read_text(encoding="utf-8")
        cls.game_application = GAME_APPLICATION.read_text(encoding="utf-8")

    def test_main_pass_culling_diagnostics_track_surface_modes_and_triangles(self) -> None:
        self.assertIn("BeginMainPass", self.diagnostics)
        self.assertIn("EndMainPass", self.diagnostics)
        self.assertIn("SetActiveSurface", self.diagnostics)
        self.assertIn("RecordIndexedDraw", self.diagnostics)
        self.assertIn("trianglesByCullMode", self.diagnostics)
        self.assertIn("pipelineBindsByPath", self.diagnostics)
        self.assertIn("visibilityMeshletInstances", self.diagnostics)
        self.assertIn("normalConeCandidateMeshletInstances", self.diagnostics)
        self.assertIn("GetEstimatedRasterizerRejectedTriangles", self.diagnostics)

    def test_mesh_builds_normal_cone_and_visibility_meshlets_without_runtime_rejection(self) -> None:
        self.assertIn("struct NormalCone", self.normal_cone)
        self.assertIn("BuildNormalCone", self.normal_cone)
        self.assertIn("IsBackfaceCullCandidate", self.normal_cone)
        self.assertIn("minDot > 0.0f", self.normal_cone)
        self.assertIn("struct VisibilityMeshlet", self.meshlet_visibility)
        self.assertIn("maxVertices = 64", self.meshlet_visibility)
        self.assertIn("maxTriangles = 126", self.meshlet_visibility)
        self.assertIn("BuildMeshletBounds", self.meshlet_visibility)
        self.assertIn("BuildVisibilityMeshlets", self.meshlet_visibility)
        self.assertIn("normalCone", self.meshlet_visibility)
        self.assertIn("std::vector<VisibilityMeshlet> visibilityMeshlets_", self.mesh_h)
        self.assertIn("visibilityMeshlets_ = BuildVisibilityMeshlets", self.mesh_cpp)
        self.assertIn("normalConeCandidateMeshlets", self.mesh_cpp)
        self.assertIn("RecordIndexedDraw", self.mesh_cpp)
        self.assertIn("Runtime Meshlet CullはまだOFF", self.mesh_cpp)

    def test_skinned_lods_preserve_bind_pose_meshlet_reference_metadata(self) -> None:
        self.assertIn("std::vector<VisibilityMeshlet> visibilityMeshlets", self.animation_lod_h)
        self.assertIn("normalConeCandidateMeshletCount", self.animation_lod_h)
        self.assertIn("normalConeCandidateTriangleCount", self.animation_lod_h)
        self.assertIn("R.visibilityMeshlets = BuildVisibilityMeshlets", self.animation_lod_cpp)
        self.assertIn("meshlet.startIndex += R.startIndex", self.animation_lod_cpp)
        self.assertIn("Bind Pose基準のreference metadata", self.animation_lod_h)

    def test_main_render_paths_publish_active_surface_state(self) -> None:
        self.assertIn("SurfacePath::Static", self.object_common)
        self.assertIn("SurfacePath::Alpha", self.object_common)
        self.assertIn("SurfacePath::Instanced", self.object_common)
        self.assertIn("SurfacePath::Animated", self.animation_pipeline)
        self.assertIn("Culling Statistics", self.object_common)
        self.assertIn("Normal Cone / Visibility Meshlet", self.object_common)
        self.assertIn("Runtime Normal Cone Culling: OFF", self.object_common)

    def test_main_pass_diagnostics_are_closed_before_debug_and_particle_draws(self) -> None:
        self.assertIn("void EndObject3DPass()", self.object_common_h)
        self.assertIn("EndMainPass", self.object_common)
        scene_draw = self.game_application.index("sceneManager_->Draw3DObjects();")
        end_stats = self.game_application.index("EndObject3DPass();", scene_draw)
        wireframe_draw = self.game_application.index("Wireframe::GetInstance()->Draw();", scene_draw)
        self.assertLess(scene_draw, end_stats)
        self.assertLess(end_stats, wireframe_draw)


if __name__ == "__main__":
    unittest.main()
