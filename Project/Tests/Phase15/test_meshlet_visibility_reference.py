from math import asin, acos, pi, sin
from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MESHLET_VISIBILITY = PROJECT_ROOT / "Engine" / "Graphics" / "Culling" / "MeshletVisibility.h"
MESH_SOURCE = PROJECT_ROOT / "Engine" / "Graphics" / "Mesh" / "Mesh.cpp"


def evaluate_reference(axis_view_dot: float, min_dot: float, radius: float, distance: float, mode: str, mirrored: bool = False):
    effective_mode = mode
    if mirrored and mode != "none":
        effective_mode = "front" if mode == "back" else "back"

    if effective_mode == "none" or min_dot <= 0.0 or distance <= max(radius, 1.0e-6):
        return False, False

    half_angle = acos(max(-1.0, min(1.0, min_dot)))
    angular_radius = asin(max(0.0, min(1.0, radius / distance)))
    conservative_half_angle = half_angle + angular_radius
    if conservative_half_angle >= pi * 0.5:
        return False, False

    threshold = sin(conservative_half_angle)
    rejected = axis_view_dot <= -threshold if effective_mode == "back" else axis_view_dot >= threshold
    return True, rejected


class MeshletVisibilityReferenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.meshlet_visibility = MESHLET_VISIBILITY.read_text(encoding="utf-8")
        cls.mesh_source = MESH_SOURCE.read_text(encoding="utf-8")

    def test_reference_evaluator_contract_is_present(self) -> None:
        self.assertIn("enum class MeshletReferenceCullMode", self.meshlet_visibility)
        self.assertIn("struct MeshletVisibilityReferenceResult", self.meshlet_visibility)
        self.assertIn("EvaluateMeshletVisibilityReference", self.meshlet_visibility)
        self.assertIn("ResolveMeshletReferenceCullModeForMirroring", self.meshlet_visibility)
        self.assertIn("conservativeHalfAngle", self.meshlet_visibility)
        self.assertIn("angularRadius", self.meshlet_visibility)
        self.assertIn("axisViewDot <= -result.rejectionThreshold", self.meshlet_visibility)
        self.assertIn("axisViewDot >= result.rejectionThreshold", self.meshlet_visibility)

    def test_back_cull_keeps_front_facing_reference_surface(self) -> None:
        evaluated, rejected = evaluate_reference(1.0, 1.0, 0.0, 10.0, "back")
        self.assertTrue(evaluated)
        self.assertFalse(rejected)

    def test_back_cull_rejects_back_facing_reference_surface(self) -> None:
        evaluated, rejected = evaluate_reference(-1.0, 1.0, 0.0, 10.0, "back")
        self.assertTrue(evaluated)
        self.assertTrue(rejected)

    def test_front_cull_rejects_front_facing_reference_surface(self) -> None:
        evaluated, rejected = evaluate_reference(1.0, 1.0, 0.0, 10.0, "front")
        self.assertTrue(evaluated)
        self.assertTrue(rejected)

    def test_mirrored_back_mode_flips_to_front_mode(self) -> None:
        evaluated, rejected = evaluate_reference(1.0, 1.0, 0.0, 10.0, "back", mirrored=True)
        self.assertTrue(evaluated)
        self.assertTrue(rejected)

    def test_wide_cone_is_never_reference_rejected(self) -> None:
        evaluated, rejected = evaluate_reference(-1.0, 0.0, 0.0, 10.0, "back")
        self.assertFalse(evaluated)
        self.assertFalse(rejected)

    def test_two_sided_surface_is_never_reference_rejected(self) -> None:
        evaluated, rejected = evaluate_reference(-1.0, 1.0, 0.0, 10.0, "none")
        self.assertFalse(evaluated)
        self.assertFalse(rejected)

    def test_bounds_angular_radius_keeps_borderline_meshlet_visible(self) -> None:
        evaluated, rejected = evaluate_reference(-0.4, 1.0, 5.0, 10.0, "back")
        self.assertTrue(evaluated)
        self.assertFalse(rejected)

    def test_runtime_meshlet_rejection_stays_disabled(self) -> None:
        self.assertIn("Runtime Meshlet CullはまだOFF", self.mesh_source)


if __name__ == "__main__":
    unittest.main()
