from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
LEVEL_DIR = PROJECT_ROOT / "Engine" / "Scene" / "Level"
NLOHMANN_DIR = PROJECT_ROOT / "Externals" / "nlohmann"
COMPATIBILITY_HEADER = LEVEL_DIR / "SaveCompatibility.h"
MIGRATION_HEADER = LEVEL_DIR / "LevelVersionMigration.h"
MIGRATION_SOURCE = LEVEL_DIR / "LevelVersionMigration.cpp"
SERIALIZER_SOURCE = LEVEL_DIR / "LevelSerializer.cpp"
RUNTIME_TEST = Path(__file__).with_name("SaveCompatibilityRuntimeTests.cpp")


class SaveCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.compatibility = COMPATIBILITY_HEADER.read_text(encoding="utf-8")
        cls.migration_header = MIGRATION_HEADER.read_text(encoding="utf-8")
        cls.migration_source = MIGRATION_SOURCE.read_text(encoding="utf-8")
        cls.serializer_source = SERIALIZER_SOURCE.read_text(encoding="utf-8")

    def test_compatibility_reuses_runtime_migration_path(self) -> None:
        self.assertIn("LevelVersionMigration::MigrateToCurrent", self.compatibility)
        self.assertIn("LevelVersionMigration::MigrateToCurrent", self.serializer_source)
        self.assertNotIn("MigrateVersion1To2", self.compatibility)

    def test_all_historical_level_versions_have_migration_edges(self) -> None:
        self.assertIn("MigrateVersion1To2", self.migration_header)
        self.assertIn("MigrateVersion2To3", self.migration_header)
        self.assertIn("case 1:", self.migration_source)
        self.assertIn("case 2:", self.migration_source)
        self.assertIn("sourceVersion > static_cast<int>(LevelDocument::kCurrentVersion)", self.migration_source)

    def test_runtime_behavior_when_portable_compiler_is_available(self) -> None:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("portable C++20 compiler is not available")

        # Migration tests compile the production migration source instead of maintaining a Python copy of schema rules.
        with tempfile.TemporaryDirectory(prefix="ken4low_phase12_save_") as temp_dir:
            executable = Path(temp_dir) / "save_compatibility_tests"
            command = [
                compiler,
                "-std=c++20",
                str(RUNTIME_TEST),
                str(MIGRATION_SOURCE),
                "-I",
                str(LEVEL_DIR),
                "-I",
                str(NLOHMANN_DIR),
                "-o",
                str(executable),
            ]
            compile_result = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(
                compile_result.returncode,
                0,
                msg=f"C++ save compatibility compile failed:\n{compile_result.stdout}\n{compile_result.stderr}",
            )
            run_result = subprocess.run([str(executable)], capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"C++ save compatibility runtime test failed:\n{run_result.stdout}\n{run_result.stderr}",
            )
            self.assertIn("Save Compatibility runtime tests passed", run_result.stdout)


if __name__ == "__main__":
    unittest.main()
