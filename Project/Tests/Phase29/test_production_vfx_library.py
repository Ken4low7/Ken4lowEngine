import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CATALOG = ROOT / "Engine/Vfx/Library/VfxProductionLibrary.h"
PRODUCTION = ROOT / "Resources/VfxGraph/Production"

ENTRY_PATTERN = re.compile(
    r'VfxProductionLibraryEntry\{\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*'
    r'VfxProductionCategory::(\w+),\s*VfxProductionCostClass::(\w+),\s*(true|false),\s*"([^"]*)"\s*\}'
)


def read_catalog_entries():
    text = CATALOG.read_text(encoding="utf-8-sig")
    entries = []
    for match in ENTRY_PATTERN.finditer(text):
        library_id, graph_name, asset_path, category, cost_class, loop, tags = match.groups()
        entries.append(
            {
                "id": library_id,
                "graphName": graph_name,
                "assetPath": asset_path,
                "category": category,
                "costClass": cost_class,
                "loop": loop == "true",
                "tags": tags,
            }
        )
    return text, entries


def load_asset(entry):
    relative = entry["assetPath"].removeprefix("Resources/")
    return json.loads((ROOT / "Resources" / relative).read_text(encoding="utf-8-sig"))


def all_node_types(asset):
    return {node["type"] for emitter in asset["emitters"] for node in emitter["nodes"] if node.get("enabled", True)}


def test_catalog_contains_ten_unique_production_entries():
    _, entries = read_catalog_entries()
    assert len(entries) == 10
    assert len({entry["id"] for entry in entries}) == 10
    assert len({entry["graphName"] for entry in entries}) == 10
    assert len({entry["assetPath"] for entry in entries}) == 10
    assert {entry["category"] for entry in entries} == {"Combat", "Elemental", "Magic", "Environment"}


def test_catalog_uses_existing_vfx_graph_runtime_only():
    text, _ = read_catalog_entries()
    assert "VfxGraphRuntime::GetInstance()->LoadGraph" in text
    assert "LoadCategory" in text
    assert "LoadAll" in text
    assert "GpuParticleManager" not in text
    assert "new VfxGraphRuntime" not in text


def test_every_catalog_entry_maps_to_a_real_graph_with_matching_name():
    _, entries = read_catalog_entries()
    for entry in entries:
        asset_path = ROOT / entry["assetPath"]
        assert asset_path.exists(), entry["assetPath"]
        asset = json.loads(asset_path.read_text(encoding="utf-8-sig"))
        assert asset["schemaVersion"] == 1
        assert asset["graphName"] == entry["graphName"]
        assert asset["emitters"]
        assert not asset["graphName"].startswith("Phase")


def test_production_assets_have_explicit_scalability_and_reasonable_bounds():
    _, entries = read_catalog_entries()
    for entry in entries:
        asset = load_asset(entry)
        scalability = asset["scalability"]
        assert scalability["maxDrawDistance"] > scalability["lodFarDistance"] > scalability["lodNearDistance"] > 0.0
        assert 0.0 < scalability["lodFarScale"] <= scalability["lodMidScale"] <= 1.0
        assert 1 <= scalability["budgetCost"] <= 8
        assert scalability["frustumCulling"] is True


def test_cost_classes_match_authored_budget_costs():
    _, entries = read_catalog_entries()
    for entry in entries:
        budget_cost = load_asset(entry)["scalability"]["budgetCost"]
        if entry["costClass"] == "Low":
            assert budget_cost <= 2
        elif entry["costClass"] == "Medium":
            assert 3 <= budget_cost <= 4
        elif entry["costClass"] == "High":
            assert budget_cost >= 5
        else:
            raise AssertionError(entry["costClass"])


def test_loop_metadata_matches_emitters_and_spawn_policy():
    _, entries = read_catalog_entries()
    for entry in entries:
        asset = load_asset(entry)
        loops = {bool(emitter["loop"]) for emitter in asset["emitters"]}
        assert loops == {entry["loop"]}
        node_types = all_node_types(asset)
        if entry["loop"]:
            assert "SpawnRate" in node_types
        else:
            assert "Burst" in node_types


def test_library_covers_phase22_through_phase26_production_features():
    _, entries = read_catalog_entries()
    coverage = set()
    for entry in entries:
        coverage.update(all_node_types(load_asset(entry)))
    for required in (
        "Collision",
        "SubEmitter",
        "RibbonRenderer",
        "TrailRenderer",
        "MeshRenderer",
        "FluidOutput",
        "LightOutput",
        "PostEffectOutput",
        "SizeOverLife",
        "ColorOverLife",
    ):
        assert required in coverage


def test_assets_stay_within_graph_schema_caps_and_have_valid_edges():
    _, entries = read_catalog_entries()
    for entry in entries:
        asset = load_asset(entry)
        assert len(asset["emitters"]) <= 32
        for emitter in asset["emitters"]:
            assert 1 <= emitter["maxParticles"] <= 4096
            assert len(emitter["nodes"]) <= 128
            assert len(emitter["edges"]) <= 256
            ids = {node["id"] for node in emitter["nodes"]}
            assert len(ids) == len(emitter["nodes"])
            for edge in emitter["edges"]:
                assert edge["from"] in ids
                assert edge["to"] in ids
                assert edge["from"] != edge["to"]


def test_library_uses_known_stable_renderer_dependencies():
    _, entries = read_catalog_entries()
    renderer_assets = set()
    for entry in entries:
        asset = load_asset(entry)
        for emitter in asset["emitters"]:
            for node in emitter["nodes"]:
                params = node.get("params", {})
                if "texturePath" in params:
                    renderer_assets.add(params["texturePath"])
                if "meshPath" in params:
                    renderer_assets.add(params["meshPath"])
    assert renderer_assets <= {"Effects/white.dds", "Sample/cube.gltf"}
    assert "Effects/white.dds" in renderer_assets
    assert "Sample/cube.gltf" in renderer_assets


def test_production_folder_has_only_catalogued_vfx_graph_assets():
    _, entries = read_catalog_entries()
    expected = {ROOT / entry["assetPath"] for entry in entries}
    actual = set(PRODUCTION.rglob("*.vfxgraph.json"))
    assert actual == expected
