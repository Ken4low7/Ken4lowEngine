#pragma once

#include <Engine/Graphics/Renderer/GpuFluid/Sph/Interaction/GpuSphRigidbodyInteraction.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{

class GpuSphRigidbodyInteractionDiagnosticsPanel final
{
public:
    static GpuSphRigidbodyInteractionDiagnosticsPanel* GetInstance()
    {
        static GpuSphRigidbodyInteractionDiagnosticsPanel instance;
        return &instance;
    }

    void Draw()
    {
#ifdef USE_IMGUI
        if (ImGui::IsKeyPressed(ImGuiKey_F7, false))
        {
            visible_ = !visible_; // SPH本体のF7診断と同時にW9双方向Interactionも確認する。
        }
        if (!visible_)
        {
            return;
        }

        GpuSphRigidbodyInteraction* interaction = GpuSphRigidbodyInteraction::GetInstance();
        GpuSphRigidbodyInteractionSettings& settings = interaction->GetEditableSettings();
        const GpuSphRigidbodyInteractionStats& stats = interaction->GetStats();

        if (!ImGui::Begin("W9 SPH Rigidbody Interaction", &visible_))
        {
            ImGui::End();
            return;
        }

        ImGui::TextDisabled("F7: toggle with GPU Fluid Diagnostics");
        ImGui::Checkbox("W9 Interaction Enabled", &settings.enabled);

        if (ImGui::CollapsingHeader("Coupling Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Particle Radius Scale", &settings.particleRadiusScale, 0.1f, 1.5f);
            ImGui::DragFloat("Minimum Particle Radius", &settings.minimumParticleRadius, 0.0025f, 0.001f, 0.5f);
            ImGui::SliderFloat("Collision Restitution", &settings.restitution, 0.0f, 1.0f);
            ImGui::SliderFloat("Collision Friction", &settings.friction, 0.0f, 1.0f);
            ImGui::SliderFloat("Coupling Strength", &settings.couplingStrength, 0.0f, 2.0f);
            ImGui::DragFloat("Max Linear Impulse", &settings.maximumLinearImpulse, 1.0f, 1.0f, 500.0f);
            ImGui::DragFloat("Max Angular Impulse", &settings.maximumAngularImpulse, 1.0f, 1.0f, 500.0f);

            if (ImGui::Button("Water Coupling Preset"))
            {
                settings.particleRadiusScale = 0.5f;
                settings.minimumParticleRadius = 0.02f;
                settings.restitution = 0.05f;
                settings.friction = 0.15f;
                settings.couplingStrength = 1.0f;
                settings.maximumLinearImpulse = 50.0f;
                settings.maximumAngularImpulse = 50.0f;
            }
        }

        if (ImGui::CollapsingHeader("W9 Runtime Status", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Runtime: %s", stats.initialized ? "Ready" : "Waiting");
            ImGui::Text("Last Dispatch: %s", stats.lastDispatchSucceeded ? "OK" : "FAILED");
            ImGui::Text("Collider Proxies: %u / %u", stats.proxyCount, GpuSphRigidbodyInteraction::kMaxProxies);
            ImGui::Text("Dynamic Bodies: %u / %u", stats.dynamicBodyCount, GpuSphRigidbodyInteraction::kMaxDynamicBodies);
            ImGui::Text("Physical Particle Radius: %.4f", stats.particleRadius);
            ImGui::Text("Frame Resource Ring: %u", stats.frameResourceCount);
            ImGui::Separator();
            ImGui::Text("Collision Dispatches: %llu", static_cast<unsigned long long>(stats.collisionDispatchCount));
            ImGui::Text("Reaction Clear Dispatches: %llu", static_cast<unsigned long long>(stats.reactionClearDispatchCount));
            ImGui::Text("Async Readbacks: %llu", static_cast<unsigned long long>(stats.readbackCount));
            ImGui::Text("Applied Rigidbody Reactions: %llu", static_cast<unsigned long long>(stats.appliedBodyCount));
            ImGui::Text("Last Linear Impulse: %.4f", stats.lastLinearImpulse);
            ImGui::Text("Last Angular Impulse: %.4f", stats.lastAngularImpulse);
        }

        if (ImGui::CollapsingHeader("Stress Test Guidance"))
        {
            ImGui::BulletText("Start: 1 Dynamic OBB + 1000 SPH particles");
            ImGui::BulletText("Scale: 4 / 16 / 32 Dynamic bodies");
            ImGui::BulletText("Collider proxy hard limit: 64");
            ImGui::BulletText("Dynamic reaction hard limit: 32 bodies");
            ImGui::BulletText("Then test SPH 4096 / 16384 particles");
            ImGui::TextDisabled("Frame Resource readback never calls ExecuteAndWait; reactions arrive a few frames later.");
        }

        ImGui::End();
#endif
    }

private:
    GpuSphRigidbodyInteractionDiagnosticsPanel() = default;
    bool visible_ = false;
};

} // namespace Ken4lowEngine
