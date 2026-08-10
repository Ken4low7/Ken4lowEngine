from project_validation import (
    validate_level_references,
    validate_project_settings,
    validate_scene_references,
)


def main() -> int:
    issues = []
    issues.extend(validate_project_settings())
    issues.extend(validate_scene_references())
    issues.extend(validate_level_references())
    if issues:
        print(f"Broken reference check failed: {len(issues)} issue(s)")
        for issue in issues:
            print(f"  - {issue.format()}")
        return 1
    print("Broken reference check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
