from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_legacy_emitters_default_to_2d_only():
    header = read("Engine/Scene/Actor/Components/FluidEmitterComponent.h")

    assert "enum class FluidEmitterTargetDomain" in header
    assert "Fluid2D = 0" in header
    assert "Volumetric3D" in header
    assert "Both" in header
    assert "targetDomain_ = FluidEmitterTargetDomain::Fluid2D" in header


def test_inspector_serializes_explicit_target_domain_choices():
    source = read("Engine/Scene/Actor/Components/FluidEmitterComponent.cpp")

    assert '"TargetDomain"' in source
    assert '"Target Domain"' in source
    assert '{ "Fluid2D", "2D Fluid" }' in source
    assert '{ "Volumetric3D", "3D Volumetric" }' in source
    assert '{ "Both", "Both (2D + 3D)" }' in source
    assert "ComponentPropertyType::String" in source


def test_2d_and_3d_sources_are_gated_independently():
    source = read("Engine/Scene/Actor/Components/FluidEmitterComponent.cpp")

    build_2d = source[source.index("GpuFluidEmitterSource FluidEmitterComponent::BuildEmitterSource"):
                      source.index("GpuVolumetricFluidEmitterSource FluidEmitterComponent::BuildVolumetricEmitterSource")]
    build_3d = source[source.index("GpuVolumetricFluidEmitterSource FluidEmitterComponent::BuildVolumetricEmitterSource"):
                      source.index("void FluidEmitterComponent::SetRadius")]

    assert "Targets2D()" in build_2d
    assert "TargetsVolumetric3D()" not in build_2d
    assert "TargetsVolumetric3D()" in build_3d
    assert "Targets2D()" not in build_3d


def test_both_is_explicit_opt_in():
    header = read("Engine/Scene/Actor/Components/FluidEmitterComponent.h")

    targets_2d = header[header.index("bool Targets2D() const"):
                        header.index("bool TargetsVolumetric3D() const")]
    targets_3d = header[header.index("bool TargetsVolumetric3D() const"):
                        header.index("bool IsEmissionEnabled() const")]

    assert "FluidEmitterTargetDomain::Both" in targets_2d
    assert "FluidEmitterTargetDomain::Both" in targets_3d


def test_component_description_no_longer_implies_automatic_2d_3d_sharing():
    source = read("Engine/Scene/Actor/Components/FluidEmitterComponent.cpp")

    assert "Target Domain decides whether this source feeds 2D Fluid, 3D Volumetric Fluid, or both." in source
    assert "Target Domainを明示して2Dまたは3D GPU Fluid" in source
