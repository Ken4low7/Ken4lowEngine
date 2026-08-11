from project_validation import (
    SETTINGS_PATH,
    validate_all_json,
    validate_level_references,
    validate_project_settings,
    validate_scene_references,
)


def main() -> int:
    issues = []
    # Legacy ProjectSettings/SceneRegistry validation is only enabled when that configuration still exists.
    if SETTINGS_PATH.is_file():
        issues.extend(validate_project_settings())
        issues.extend(validate_scene_references())
    issues.extend(validate_all_json())
    issues.extend(validate_level_references())
    if issues:
        print(f"Asset validation failed: {len(issues)} issue(s)")
        for issue in issues:
            print(f"  - {issue.format()}")
        return 1
    print("Asset validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
