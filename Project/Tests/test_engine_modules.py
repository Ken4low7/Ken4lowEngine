from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "Tools/Scripts"))

import ValidateEngineModules as modules


class EngineModuleTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        manifest = {
            "Format": "Ken4lowEngineModules",
            "Version": 1,
            "Modules": [
                {"Name": "Core", "Roots": ["Engine/Core"], "MayDependOn": []},
                {"Name": "Runtime", "Roots": ["Engine/Runtime"], "MayDependOn": ["Core"]},
                {"Name": "Editor", "Roots": ["Engine/Editor"], "MayDependOn": ["Core", "Runtime"]},
                {"Name": "Application", "Roots": ["ApplicationLayer"], "MayDependOn": ["Core", "Runtime", "Editor"]},
            ],
        }
        self.write("Build/Modules/EngineModules.json", json.dumps(manifest))
        self.write("ApplicationLayer/Scenes/SceneFactory.h", "#pragma once\n")

    def write(self, relative: str, text: str) -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def test_rejects_application_includes_from_each_engine_module(self) -> None:
        # 実際に見逃されていた短いSceneFactory includeと、表記違いを同じ境界で検証する。
        includes = [
            '"SceneFactory.h"',
            '<SceneFactory.h>',
            '"Scenes/SceneFactory.h"',
            '"ApplicationLayer/Scenes/SceneFactory.h"',
            '"../../ApplicationLayer/Scenes/SceneFactory.h"',
            '"..\\..\\ApplicationLayer\\Scenes\\SceneFactory.h"',
            '<applicationlayer/scenes/scenefactory.h>',
            '"./../../ApplicationLayer/Scenes/../Scenes/SceneFactory.h"',
        ]
        for owner in ("Core", "Runtime", "Editor"):
            for include in includes:
                with self.subTest(owner=owner, include=include):
                    source = f"Engine/{owner}/Caller.cpp"
                    self.write(source, f"#include {include}\n")
                    errors = modules.validate(self.root)
                    self.assertEqual(len(errors), 1, errors)
                    self.assertIn(source, errors[0])
                    self.assertIn("Application", errors[0])
                    (self.root / source).unlink()

    def test_qualified_missing_application_header_is_still_forbidden(self) -> None:
        self.write("Engine/Core/Caller.cpp", '#include "ApplicationLayer/Missing.h"\n')
        self.assertEqual(len(modules.validate(self.root)), 1)

    def test_quoted_local_header_wins_over_same_named_application_header(self) -> None:
        self.write("Engine/Core/SceneFactory.h", "#pragma once\n")
        self.write("Engine/Core/Caller.cpp", '#include "SceneFactory.h"\n')
        self.assertEqual(modules.validate(self.root), [])

    def test_ambiguous_short_include_does_not_hide_application_candidate(self) -> None:
        self.write("Engine/Runtime/SceneFactory.h", "#pragma once\n")
        self.write("Engine/Core/Caller.cpp", '#include <SceneFactory.h>\n')
        errors = modules.validate(self.root)
        self.assertEqual(len(errors), 1)
        self.assertIn("ApplicationLayer/Scenes/SceneFactory.h", errors[0])

    def test_application_composes_engine_and_local_factory(self) -> None:
        self.write("Engine/Core/Framework.h", "#pragma once\n")
        self.write("ApplicationLayer/GameApplication.cpp", '#include "Framework.h"\n#include "SceneFactory.h"\n')
        self.write("Engine/Runtime/Renderer.cpp", '#include <Framework.h>\n#include <vector>\n#include "vendor.hpp"\n')
        self.assertEqual(modules.validate(self.root), [])

    def test_unowned_source_is_rejected(self) -> None:
        self.write("Engine/Unassigned/Unknown.cpp", "")
        errors = modules.validate(self.root)
        self.assertEqual(len(errors), 1)
        self.assertIn("Unknown.cpp", errors[0])

    def test_repository_boundaries(self) -> None:
        self.assertEqual(modules.validate(), [])


if __name__ == "__main__":
    unittest.main()
