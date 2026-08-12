from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPLAY_DIR = PROJECT_ROOT / "Engine" / "Core" / "Replay"
REPLAY_HEADER = REPLAY_DIR / "DeterministicReplay.h"
RUNTIME_TEST = Path(__file__).with_name("DeterministicReplayRuntimeTests.cpp")


class DeterministicReplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = REPLAY_HEADER.read_text(encoding="utf-8")

    def test_format_tracks_inputs_rng_delta_and_state_hash(self) -> None:
        self.assertIn("fixedDeltaMicroseconds", self.header)
        self.assertIn("rngState", self.header)
        self.assertIn("stateHash", self.header)
        self.assertIn("inputPayload", self.header)
        self.assertIn("K4REPLAY", self.header.replace("'", "").replace(",", "").replace(" ", ""))

    def test_playback_rejects_frame_mismatch(self) -> None:
        self.assertIn("frame.frameIndex != expectedFrameIndex", self.header)
        self.assertIn("Replay frame index does not match", self.header)

    def test_runtime_behavior_when_portable_compiler_is_available(self) -> None:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("portable C++20 compiler is not available")

        # The replay format is header-only, so CI can exercise serialization without a Windows graphics runtime.
        with tempfile.TemporaryDirectory(prefix="ken4low_phase12_replay_") as temp_dir:
            executable = Path(temp_dir) / "deterministic_replay_tests"
            command = [
                compiler,
                "-std=c++20",
                str(RUNTIME_TEST),
                "-I",
                str(REPLAY_DIR),
                "-o",
                str(executable),
            ]
            compile_result = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(
                compile_result.returncode,
                0,
                msg=f"C++ deterministic replay compile failed:\n{compile_result.stdout}\n{compile_result.stderr}",
            )
            run_result = subprocess.run([str(executable)], capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"C++ deterministic replay runtime test failed:\n{run_result.stdout}\n{run_result.stderr}",
            )
            self.assertIn("Deterministic Replay runtime tests passed", run_result.stdout)


if __name__ == "__main__":
    unittest.main()
