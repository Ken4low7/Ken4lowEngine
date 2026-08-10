from project_validation import validate_assets


def main() -> int:
    issues = validate_assets()
    if issues:
        print(f"Asset validation failed: {len(issues)} issue(s)")
        for issue in issues:
            print(f"  - {issue.format()}")
        return 1
    print("Asset validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
