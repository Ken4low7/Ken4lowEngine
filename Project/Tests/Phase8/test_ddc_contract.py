import unittest
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = PROJECT_DIR / "Tools" / "Scripts"


class DerivedDataCacheContractTests(unittest.TestCase):
    def read_script(self, name: str) -> str:
        return (SCRIPTS_DIR / name).read_text(encoding="utf-8-sig")

    def test_shared_ddc_helpers_are_content_addressed(self):
        common = self.read_script("BuildAssetCommon.ps1")
        self.assertIn("function Get-DerivedDataBuildKey", common)
        self.assertIn("function Get-DdcEntryDirectory", common)
        self.assertIn("DerivedDataCache", common)
        self.assertIn("function Restore-DdcFile", common)
        self.assertIn("function Store-DdcFile", common)

    def test_all_primary_cookers_publish_and_use_build_keys(self):
        for script_name in ("BuildTextures.ps1", "BuildMeshes.ps1", "BuildFonts.ps1"):
            with self.subTest(script=script_name):
                script = self.read_script(script_name)
                self.assertIn("[switch]$DisableDdc", script)
                self.assertIn("Get-DerivedDataBuildKey", script)
                self.assertIn("BuildKey", script)
                self.assertIn("Get-DdcEntryDirectory", script)
                self.assertIn("DDC HIT", script)
                self.assertIn("DDC MISS", script)

    def test_single_file_cookers_restore_and_store_payloads(self):
        for script_name in ("BuildTextures.ps1", "BuildMeshes.ps1"):
            with self.subTest(script=script_name):
                script = self.read_script(script_name)
                self.assertIn("Restore-DdcFile", script)
                self.assertIn("Store-DdcFile", script)

    def test_font_cooker_uses_directory_cache_for_multi_file_output(self):
        script = self.read_script("BuildFonts.ps1")
        self.assertIn("function Restore-FontDdc", script)
        self.assertIn("function Store-FontDdc", script)
        self.assertIn('"Textures"', script)
        self.assertIn('"Metadata"', script)


if __name__ == "__main__":
    unittest.main()
