from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MATERIAL_H = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "Material.h"
MATERIAL_CPP = MATERIAL_H.with_suffix(".cpp")
MATERIAL_SOURCE_H = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "MaterialDescLoader.h"
MATERIAL_SOURCE_CPP = MATERIAL_SOURCE_H.with_suffix(".cpp")
MATERIAL_JSON_H = PROJECT_ROOT / "Engine" / "Graphics" / "Material" / "MaterialDescJsonConverter.h"
MATERIAL_JSON_CPP = MATERIAL_JSON_H.with_suffix(".cpp")
FORWARD_QUEUE = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Forward" / "ForwardRenderQueue.h"
OBJECT_COMMON_H = PROJECT_ROOT / "Engine" / "Graphics" / "Renderer" / "Object3D" / "Object3DCommon.h"


class ForwardRenderQueueFoundationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.material_h = MATERIAL_H.read_text(encoding="utf-8")
        cls.material_cpp = MATERIAL_CPP.read_text(encoding="utf-8")
        cls.material_source_h = MATERIAL_SOURCE_H.read_text(encoding="utf-8")
        cls.material_source_cpp = MATERIAL_SOURCE_CPP.read_text(encoding="utf-8")
        cls.material_json_h = MATERIAL_JSON_H.read_text(encoding="utf-8")
        cls.material_json_cpp = MATERIAL_JSON_CPP.read_text(encoding="utf-8")
        cls.forward_queue = FORWARD_QUEUE.read_text(encoding="utf-8")
        cls.object_common_h = OBJECT_COMMON_H.read_text(encoding="utf-8")

    def test_material_owns_high_level_forward_blend_classification(self) -> None:
        self.assertIn("enum class MaterialBlendMode", self.material_h)
        for mode in ("Opaque", "Masked", "Transparent", "Additive"):
            self.assertIn(mode, self.material_h)
        self.assertIn("MaterialBlendMode blendMode = MaterialBlendMode::Opaque", self.material_h)
        self.assertIn("GetBlendMode()", self.material_h)
        self.assertIn("blendMode_ = desc.blendMode", self.material_cpp)
        self.assertIn("Blend Mode##Material", self.material_cpp)

    def test_material_source_and_json_preserve_blend_mode(self) -> None:
        self.assertIn("MaterialBlendMode blendMode = MaterialBlendMode::Opaque", self.material_source_h)
        self.assertIn("desc.blendMode = normalizedSource.blendMode", self.material_source_cpp)
        self.assertIn('static constexpr const char* BlendMode = "blendMode"', self.material_json_h)
        self.assertIn("BlendModeFromString", self.material_json_h)
        self.assertIn('Keys::BlendMode, "opaque"', self.material_json_cpp)
        self.assertIn("json[Keys::BlendMode] = ToString(normalized.blendMode)", self.material_json_cpp)
        self.assertIn("return MaterialBlendMode::Opaque", self.material_json_cpp)

    def test_forward_policy_maps_material_modes_to_stable_queue_contracts(self) -> None:
        self.assertIn("enum class ForwardRenderBucket", self.forward_queue)
        self.assertIn("enum class ForwardSortMode", self.forward_queue)
        self.assertIn("ResolveForwardRenderPolicy", self.forward_queue)
        self.assertIn("ForwardRenderBucket::Opaque, BlendMode::kBlendModeNone, ForwardSortMode::FrontToBack, true, false", self.forward_queue)
        self.assertIn("ForwardRenderBucket::Masked, BlendMode::kBlendModeNone, ForwardSortMode::FrontToBack, true, true", self.forward_queue)
        self.assertIn("ForwardRenderBucket::Transparent, BlendMode::kBlendModeNormal, ForwardSortMode::BackToFront, false, false", self.forward_queue)
        self.assertIn("ForwardRenderBucket::Additive, BlendMode::kBlendModeAdd, ForwardSortMode::BackToFront, false, false", self.forward_queue)

    def test_forward_policy_is_compiled_by_object_renderer(self) -> None:
        self.assertIn("Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h", self.object_common_h)


if __name__ == "__main__":
    unittest.main()
