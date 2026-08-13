from pathlib import Path
import math
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
GIZMO_SOURCE = PROJECT_ROOT / "Engine" / "Editor" / "EditorTransformGizmo.cpp"


def multiply(lhs, rhs):
    return [
        [sum(lhs[row][k] * rhs[k][column] for k in range(4)) for column in range(4)]
        for row in range(4)
    ]


def make_scale(scale):
    return [
        [scale[0], 0.0, 0.0, 0.0],
        [0.0, scale[1], 0.0, 0.0],
        [0.0, 0.0, scale[2], 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def make_rotate_x(angle):
    c = math.cos(angle)
    s = math.sin(angle)
    return [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, c, s, 0.0],
        [0.0, -s, c, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def make_rotate_y(angle):
    c = math.cos(angle)
    s = math.sin(angle)
    return [
        [c, 0.0, s, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [-s, 0.0, c, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def make_rotate_z(angle):
    c = math.cos(angle)
    s = math.sin(angle)
    return [
        [c, s, 0.0, 0.0],
        [-s, c, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def make_translation(position):
    return [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [position[0], position[1], position[2], 1.0],
    ]


def make_affine(scale, rotation, position):
    # Match Matrix4x4::MakeAffineMatrix: Scale * Rx * Ry * Rz * Translate.
    matrix = multiply(make_scale(scale), make_rotate_x(rotation[0]))
    matrix = multiply(matrix, make_rotate_y(rotation[1]))
    matrix = multiply(matrix, make_rotate_z(rotation[2]))
    return multiply(matrix, make_translation(position))


def decompose_gizmo_reference(matrix):
    position = (matrix[3][0], matrix[3][1], matrix[3][2])
    scale = tuple(math.sqrt(sum(matrix[row][column] ** 2 for column in range(3))) for row in range(3))
    if min(scale) <= 1.0e-6:
        raise ValueError("non-invertible scale")

    rotation_matrix = [[matrix[row][column] for column in range(4)] for row in range(4)]
    for column in range(3):
        rotation_matrix[0][column] /= scale[0]
        rotation_matrix[1][column] /= scale[1]
        rotation_matrix[2][column] /= scale[2]

    sin_y = max(-1.0, min(1.0, rotation_matrix[0][2]))
    y = math.asin(sin_y)
    cos_y = math.cos(y)
    if abs(cos_y) > 1.0e-5:
        x = math.atan2(rotation_matrix[1][2], rotation_matrix[2][2])
        z = math.atan2(rotation_matrix[0][1], rotation_matrix[0][0])
    else:
        y_sign = 1.0 if sin_y >= 0.0 else -1.0
        x = math.atan2(-y_sign * rotation_matrix[1][0], rotation_matrix[1][1])
        z = 0.0

    return scale, (x, y, z), position


class EditorTransformGizmoRotationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = GIZMO_SOURCE.read_text(encoding="utf-8")

    def assert_matrix_close(self, lhs, rhs, epsilon=1.0e-5):
        for row in range(4):
            for column in range(4):
                self.assertAlmostEqual(lhs[row][column], rhs[row][column], delta=epsilon)

    def test_gizmo_uses_engine_xyz_row_vector_decomposition(self) -> None:
        self.assertIn("DecomposeGizmoMatrix", self.source)
        self.assertIn("rotationMatrix.m[0][2]", self.source)
        self.assertIn("std::atan2(rotationMatrix.m[1][2], rotationMatrix.m[2][2])", self.source)
        self.assertIn("std::atan2(rotationMatrix.m[0][1], rotationMatrix.m[0][0])", self.source)
        self.assertNotIn("Matrix4x4::Decompose(transformMatrix", self.source)

    def test_each_rotation_axis_round_trips_without_sign_flip(self) -> None:
        for rotation in ((0.75, 0.0, 0.0), (0.0, -0.9, 0.0), (0.0, 0.0, 1.1)):
            original = make_affine((1.0, 1.0, 1.0), rotation, (0.0, 0.0, 0.0))
            scale, decomposed_rotation, position = decompose_gizmo_reference(original)
            rebuilt = make_affine(scale, decomposed_rotation, position)
            self.assert_matrix_close(original, rebuilt)

    def test_combined_rotation_and_non_uniform_scale_round_trip(self) -> None:
        cases = (
            ((1.5, 0.75, 2.0), (0.35, 0.8, -0.6), (3.0, -2.0, 7.0)),
            ((0.6, 2.25, 1.1), (-1.0, -0.7, 1.25), (-4.0, 1.0, 0.5)),
            ((2.0, 1.0, 0.5), (1.2, 1.35, -1.4), (0.0, 5.0, -3.0)),
        )
        for scale, rotation, position in cases:
            original = make_affine(scale, rotation, position)
            decomposed_scale, decomposed_rotation, decomposed_position = decompose_gizmo_reference(original)
            rebuilt = make_affine(decomposed_scale, decomposed_rotation, decomposed_position)
            self.assert_matrix_close(original, rebuilt)

    def test_gimbal_lock_branch_preserves_equivalent_matrix(self) -> None:
        for y in (math.pi / 2.0, -math.pi / 2.0):
            original = make_affine((1.0, 1.0, 1.0), (0.4, y, 0.7), (0.0, 0.0, 0.0))
            scale, rotation, position = decompose_gizmo_reference(original)
            rebuilt = make_affine(scale, rotation, position)
            self.assert_matrix_close(original, rebuilt)

    def test_rotation_writeback_preserves_unedited_transform_channels(self) -> None:
        self.assertIn("EditorTransform editedWorldTransform = worldTransform;", self.source)
        self.assertIn("case EditorViewportTool::Rotate:", self.source)
        self.assertIn("editedWorldTransform.rotation = rotation;", self.source)
        self.assertIn("UnwrapAngleNear(rotation.x, worldTransform.rotation.x)", self.source)
        self.assertIn("選択中ツールの成分だけを書き戻し", self.source)


if __name__ == "__main__":
    unittest.main()
