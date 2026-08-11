from project_validation import (
    SETTINGS_PATH,
    validate_level_references,
    validate_project_settings,
    validate_scene_references,
)


def main() -> int:
    issues = []
    # Resource cleanup can remove the legacy scene registry without disabling Level/Prefab reference validation.
    if SETTINGS_PATH.is_file():
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
