from __future__ import annotations

import importlib.util
import inspect
import traceback
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent


def load_module(path: Path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    failures: list[str] = []
    executed = 0

    for path in sorted(THIS_DIR.glob("test_*.py")):
        module = load_module(path)
        for name, function in inspect.getmembers(module, inspect.isfunction):
            if not name.startswith("test_") or function.__module__ != module.__name__:
                continue
            executed += 1
            try:
                function()
                print(f"PASS {path.name}::{name}")
            except Exception:
                failures.append(f"{path.name}::{name}")
                print(f"FAIL {path.name}::{name}")
                traceback.print_exc()

    if executed == 0:
        print("Phase19 static test runner found no tests.")
        return 1
    if failures:
        print(f"Phase19 static tests failed: {len(failures)} / {executed}")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print(f"Phase19 static tests passed: {executed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
