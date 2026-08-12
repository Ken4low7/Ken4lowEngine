from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[2]
EDITOR_DIR = PROJECT_ROOT / "Engine" / "Editor"
HISTORY_HEADER = EDITOR_DIR / "EditorCommandHistory.h"
RUNTIME_TEST = Path(__file__).with_name("EditorCommandHistoryRuntimeTests.cpp")


class EditorCommandHistoryContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HISTORY_HEADER.read_text(encoding="utf-8")

    def test_composite_command_replays_forward_and_undoes_reverse(self) -> None:
        self.assertIn("class EditorCompositeCommand final", self.header)
        self.assertIn("commands_.rbegin()", self.header)
        self.assertIn("(*it)->Undo()", self.header)

    def test_transactions_group_already_executed_commands(self) -> None:
        self.assertIn("bool BeginTransaction(std::string name)", self.header)
        self.assertIn("bool CommitTransaction()", self.header)
        self.assertIn("bool CancelTransaction()", self.header)
        self.assertIn("transactionCommands_.push_back", self.header)
        self.assertIn("std::make_unique<EditorCompositeCommand>", self.header)
        self.assertIn("!transactionActive_ && cursor_ > 0", self.header)

    def test_replay_scope_restores_flag_and_undo_restores_cursor_on_failure(self) -> None:
        self.assertIn("class ReplayScope final", self.header)
        self.assertIn("history_.isReplaying_ = false", self.header)
        self.assertIn("const std::size_t previousCursor = cursor_", self.header)
        self.assertIn("cursor_ = previousCursor", self.header)

    def test_history_exposes_diagnostics_for_editor_ui(self) -> None:
        self.assertIn("GetHistorySize() const", self.header)
        self.assertIn("GetTransactionCommandCount() const", self.header)
        self.assertIn("GetCapacity() const", self.header)
        self.assertIn("GetTransactionName() const", self.header)

    def test_runtime_behavior_when_portable_compiler_is_available(self) -> None:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("portable C++20 compiler is not available")

        # Header-only history is compiled directly so transaction/replay behavior is exercised, not only source text.
        with tempfile.TemporaryDirectory(prefix="ken4low_phase11_history_") as temp_dir:
            executable = Path(temp_dir) / "editor_command_history_tests"
            command = [
                compiler,
                "-std=c++20",
                str(RUNTIME_TEST),
                "-I",
                str(EDITOR_DIR),
                "-o",
                str(executable),
            ]
            compile_result = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(
                compile_result.returncode,
                0,
                msg=f"C++ editor command history runtime compile failed:\n{compile_result.stdout}\n{compile_result.stderr}",
            )

            run_result = subprocess.run([str(executable)], capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(
                run_result.returncode,
                0,
                msg=f"C++ editor command history runtime test failed:\n{run_result.stdout}\n{run_result.stderr}",
            )
            self.assertIn("Editor Command History runtime tests passed", run_result.stdout)


if __name__ == "__main__":
    unittest.main()
