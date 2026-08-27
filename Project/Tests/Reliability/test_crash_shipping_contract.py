from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
CRASH_REPORTER = PROJECT_ROOT / "Engine" / "Core" / "Diagnostics" / "CrashReporter.h"
DIAGNOSTIC_REPORT = PROJECT_ROOT / "Engine" / "Core" / "Diagnostics" / "EngineDiagnosticReport.h"
BUILD_PROFILE = PROJECT_ROOT / "Engine" / "Core" / "Diagnostics" / "BuildProfile.h"
WIN_MAIN = PROJECT_ROOT / "WinMain.cpp"
PACKAGE_SCRIPT = PROJECT_ROOT / "Tools" / "Scripts" / "PackageRelease.ps1"
SOAK_SCRIPT = PROJECT_ROOT / "Tools" / "Scripts" / "RunSoakTest.ps1"


class CrashShippingContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.crash = CRASH_REPORTER.read_text(encoding="utf-8")
        cls.diagnostic = DIAGNOSTIC_REPORT.read_text(encoding="utf-8")
        cls.profile = BUILD_PROFILE.read_text(encoding="utf-8")
        cls.win_main = WIN_MAIN.read_text(encoding="utf-8")
        cls.package = PACKAGE_SCRIPT.read_text(encoding="utf-8")
        cls.soak = SOAK_SCRIPT.read_text(encoding="utf-8")

    def test_unhandled_crash_produces_dump_stack_and_diagnostic_context(self) -> None:
        self.assertIn("MiniDumpWriteDump", self.crash)
        self.assertIn("CaptureStackBackTrace", self.crash)
        self.assertIn("SymFromAddr", self.crash)
        self.assertIn("EngineDiagnosticReport::Write", self.crash)
        self.assertIn("MiniDumpWithThreadInfo", self.crash)

    def test_crash_handler_is_installed_before_engine_construction(self) -> None:
        install_index = self.win_main.index("CrashReporter::Install")
        engine_index = self.win_main.index("std::make_unique<GameApplication>")
        self.assertLess(install_index, engine_index)
        self.assertIn("CrashReporter::WriteManualReport(exception.what())", self.win_main)

    def test_diagnostic_report_records_build_and_process_context(self) -> None:
        for token in (
            "BuildProfile=",
            "ProcessId=",
            "ThreadId=",
            "CommandLine=",
            "WorkingDirectory=",
            "PhysicalMemoryLoadPercent=",
        ):
            self.assertIn(token, self.diagnostic)

    def test_build_profile_separates_debug_development_and_shipping(self) -> None:
        self.assertIn("Debug", self.profile)
        self.assertIn("Development", self.profile)
        self.assertIn("Shipping", self.profile)
        self.assertIn("defined(_DEBUG)", self.profile)
        self.assertIn("defined(NDEBUG)", self.profile)

    def test_release_package_hashes_payload_and_separates_symbols(self) -> None:
        self.assertIn("Get-FileHash", self.package)
        self.assertIn("PackageManifest.json", self.package)
        self.assertIn("Compress-Archive", self.package)
        self.assertIn("*.pdb", self.package)
        self.assertIn("Symbols", self.package)
        self.assertIn("[switch]$DryRun", self.package)

    def test_soak_runner_drives_the_same_runtime_telemetry_gate(self) -> None:
        self.assertIn("KEN4LOW_RELIABILITY_CSV", self.soak)
        self.assertIn("KEN4LOW_SOAK_SECONDS", self.soak)
        self.assertIn("KEN4LOW_STREAMING_STRESS", self.soak)
        self.assertIn("AnalyzeReliabilityTelemetry.py", self.soak)
        self.assertIn("--require-soak", self.soak)


if __name__ == "__main__":
    unittest.main()
