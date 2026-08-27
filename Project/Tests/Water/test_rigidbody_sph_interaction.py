from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
INTERACTION = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph/Interaction/GpuSphRigidbodyInteraction.h"
SHADER = PROJECT_ROOT / "Resources/Shaders/GpuFluid/Sph/GpuSphRigidbodyInteraction.CS.hlsl"
REFLECTION_BRIDGE = PROJECT_ROOT / "Engine/Graphics/Renderer/Reflection/ReflectionProbeSceneBridge.h"
PARTICLE_BUFFER = PROJECT_ROOT / "Engine/Graphics/Renderer/GpuFluid/Sph/Resource/GpuSphParticleBuffer.cpp"
DIAGNOSTICS = PROJECT_ROOT / "Engine/Editor/GpuSphRigidbodyInteractionDiagnosticsPanel.h"
EDITOR_OVERLAY = PROJECT_ROOT / "Engine/Editor/EditorLevelOverlay.h"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def test_supports_four_runtime_collider_shapes():
    source = read(INTERACTION)
    shader = read(SHADER)

    assert "ECollisionShapeType::Sphere" in source
    assert "ECollisionShapeType::AABB" in source
    assert "ECollisionShapeType::OBB" in source
    assert "ECollisionShapeType::Capsule" in source
    assert "ResolveSphere" in shader
    assert "ResolveBox" in shader
    assert "ResolveCapsule" in shader


def test_uses_frame_resource_readback_without_gpu_waits():
    source = read(INTERACTION)

    assert "GetFrameResourceCount()" in source
    assert "GetCurrentFrameIndex()" in source
    assert "D3D12_HEAP_TYPE_READBACK" in source
    assert "CopyBufferRegion" in source
    assert "pendingReadback" in source
    assert "ExecuteAndWait" not in source
    assert "WaitAndReset" not in source


def test_delayed_reactions_use_actor_handles_not_persistent_actor_pointers():
    source = read(INTERACTION)

    assert "std::array<ActorHandle, kMaxDynamicBodies> bodyHandles" in source
    assert "actorWorld.MakeActorHandle(actor)" in source
    assert "actorWorld.ResolveActor(slot.bodyHandles[bodyIndex])" in source


def test_gpu_reaction_is_atomic_fixed_point_force_and_torque():
    shader = read(SHADER)

    assert "InterlockedAdd(gReactions[bodyIndex].impulseX" in shader
    assert "InterlockedAdd(gReactions[bodyIndex].impulseY" in shader
    assert "InterlockedAdd(gReactions[bodyIndex].impulseZ" in shader
    assert "InterlockedAdd(gReactions[bodyIndex].torqueX" in shader
    assert "InterlockedAdd(gReactions[bodyIndex].torqueY" in shader
    assert "InterlockedAdd(gReactions[bodyIndex].torqueZ" in shader
    assert "particleImpulse" in shader
    assert "bodyTorqueImpulse" in shader


def test_particle_collision_updates_position_velocity_and_prediction():
    shader = read(SHADER)

    assert "positionValue += normal * (penetration + 1.0e-4f);" in shader
    assert "velocityValue = bodySurfaceVelocity + relativeVelocity;" in shader
    assert "particle.position = positionValue;" in shader
    assert "particle.velocity = velocityValue;" in shader
    assert "particle.predictedPosition" in shader


def test_readback_applies_linear_and_angular_impulses_to_dynamic_bodies():
    source = read(INTERACTION)

    assert "rigidbody->GetBodyType() != BodyType::Dynamic" in source
    assert "rigidbody->SetVelocity(rigidbody->GetVelocity() + linearImpulse * rigidbody->GetInvMass())" in source
    assert "rigidbody->GetInvInertia()" in source
    assert "rigidbody->SetAngularVelocity" in source
    assert "maximumLinearImpulse" in source
    assert "maximumAngularImpulse" in source


def test_is_connected_once_at_main_scene_bridge_and_not_editor_edit_mode():
    source = read(REFLECTION_BRIDGE)

    update_call = "GpuSphRigidbodyInteraction::GetInstance()->Update(actorWorld);"
    assert update_call in source
    assert "isEditorEditing" in source
    assert "if (!isEditorEditing)" in source
    assert source.index(update_call) < source.index("SyncProbes(actorWorld);")


def test_resources_finalize_before_particle_buffer_release():
    source = read(PARTICLE_BUFFER)

    interaction_finalize = "GpuSphRigidbodyInteraction::GetInstance()->Finalize();"
    resource_reset = "resource_.Reset();"
    assert interaction_finalize in source
    assert source.index(interaction_finalize) < source.index(resource_reset)


def test_diagnostics_and_stress_limits_are_exposed():
    source = read(INTERACTION)
    diagnostics = read(DIAGNOSTICS)
    overlay = read(EDITOR_OVERLAY)

    assert "kMaxProxies = 64" in source
    assert "kMaxDynamicBodies = 32" in source
    assert "Collider Proxy" in diagnostics
    assert "動的Rigidbody" in diagnostics
    assert "非同期読取回数" in diagnostics
    assert "反作用を適用したRigidbody数" in diagnostics
    assert "直前の直線インパルス" in diagnostics
    assert "直前の角インパルス" in diagnostics
    assert "GpuSphRigidbodyInteractionDiagnosticsPanel::GetInstance()->Draw();" in overlay  # Editor入口から診断パネルが維持されることを確認する。
