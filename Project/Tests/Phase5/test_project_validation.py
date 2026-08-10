from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parents[2] / "Tools/Scripts"
sys.path.insert(0, str(SCRIPTS_DIR))

import project_validation as validation  # noqa: E402


class ProjectValidationTests(unittest.TestCase):
    def test_resolve_project_resource_path(self) -> None:
        resolved = validation.resolve_project_path("Resources/JSON/ProjectSettings.json")
        self.assertEqual(resolved, validation.PROJECT_ROOT / "Resources/JSON/ProjectSettings.json")

    def test_project_settings_rejects_unknown_version(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "ProjectSettings.json"
            path.write_text(json.dumps({
                "Format": "Ken4lowProjectSettings",
                "Version": 999,
                "ProjectName": "Test",
                "ResourceRoot": "Resources",
                "SceneRegistry": "Resources/JSON/Scenes/SceneRegistry.json",
                "FallbackAssets": {
                    "TextureKey": "missing",
                    "ModelKey": "missing",
                    "AudioMode": "Silent"
                }
            }), encoding="utf-8")
            issues = validation.validate_project_settings(path)
            self.assertTrue(any("Version 1以外" in issue.message for issue in issues))

    def test_level_prefab_walker_finds_nested_reference(self) -> None:
        document = {
            "Actors": [
                {"Prefab": {"Path": "Resources/ActorPrefabs/TestActor.json"}}
            ]
        }
        self.assertEqual(
            list(validation._visit_level_prefabs(document)),
            ["Resources/ActorPrefabs/TestActor.json"])

    def test_repository_project_settings_is_valid(self) -> None:
        self.assertEqual(validation.validate_project_settings(), [])


if __name__ == "__main__":
    unittest.main()
