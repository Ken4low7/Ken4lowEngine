from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parents[2]
VISUALIZER = PROJECT_DIR / "Engine/Graphics/RenderGraph/RenderGraphVisualizer.h"
GRAPH = PROJECT_DIR / "Engine/Graphics/RenderGraph/RenderGraph.h"
CONTROLLER = PROJECT_DIR / "Engine/Graphics/Pipeline/RenderPipelineController.h"
PERFORMANCE_VALIDATION = PROJECT_DIR / "ApplicationLayer/Scene/DebugScene/Validation/PerformancePhaseValidation.h"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def test_visualizer_reads_compiled_graph_truth_instead_of_rebuilding_schedule():
    text = read(VISUALIZER)

    # Phase 9.7 must consume the graph-owned diagnostics instead of maintaining a second scheduling model.
    required = [
        "GetCompiledPassHandle",
        "GetPassAccesses",
        "GetDependencies",
        "GetBarrierPlan",
        "GetResourceLifetime",
        "IsPassCulled",
        "IsPassSideEffect",
        "IsResourceOutput",
    ]
    for token in required:
        assert token in text

    assert "ApplyPassCulling" not in text
    assert "BuildBarrierPlan" not in text


def test_visualizer_exposes_phase9_resource_pressure_and_cache_diagnostics():
    text = read(VISUALIZER)

    for token in [
        "GetRenderGraphTransientPool",
        "GetDescriptorStats",
        "GetShaderCacheStats",
        "GetCacheStats",
        "ClearShaderCache",
        "ClearCache",
        "RAW",
        "WAR",
        "WAW",
        "Aliasing Ownership Changes",
    ]:
        assert token in text


def test_render_graph_exposes_read_only_visualization_metadata():
    text = read(GRAPH)

    for token in [
        "GetResourceCount() const",
        "GetPassCount() const",
        "IsResourceOutput",
        "IsPassSideEffect",
        "GetResourceInitialState",
        "GetResourceFinalState",
    ]:
        assert token in text


def test_active_controller_and_debug_validation_register_visualizer():
    controller = read(CONTROLLER)
    validation = read(PERFORMANCE_VALIDATION)

    assert "GetRenderGraph() const" in controller
    assert "GetRenderGraphTransientPool() const" in controller
    assert "RenderGraphVisualizer.h" in validation
    assert "RenderGraphVisualizer::GetInstance()->Draw(*renderPipeline)" in validation
    assert "ImGuiKey_F10" in read(VISUALIZER)
