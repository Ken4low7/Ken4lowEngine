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
            visible_ = !visible_; // SPH本体の診断と同時にRigidbodyとの相互作用も確認する。
        }
        if (!visible_)
        {
            return;
        }

        GpuSphRigidbodyInteraction* interaction = GpuSphRigidbodyInteraction::GetInstance();
        GpuSphRigidbodyInteractionSettings& settings = interaction->GetEditableSettings();
        const GpuSphRigidbodyInteractionStats& stats = interaction->GetStats();

        if (!ImGui::Begin("SPH / Rigidbody 相互作用", &visible_))
        {
            ImGui::End();
            return;
        }

        ImGui::TextDisabled("F7: 表示切替");
        ImGui::Checkbox("相互作用を有効化", &settings.enabled);

        if (ImGui::CollapsingHeader("相互作用設定", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("粒子半径倍率", &settings.particleRadiusScale, 0.1f, 1.5f);
            ImGui::DragFloat("最小粒子半径", &settings.minimumParticleRadius, 0.0025f, 0.001f, 0.5f);
            ImGui::SliderFloat("反発係数", &settings.restitution, 0.0f, 1.0f);
            ImGui::SliderFloat("摩擦係数", &settings.friction, 0.0f, 1.0f);
            ImGui::SliderFloat("連成強度", &settings.couplingStrength, 0.0f, 2.0f);
            ImGui::DragFloat("最大直線インパルス", &settings.maximumLinearImpulse, 1.0f, 1.0f, 500.0f);
            ImGui::DragFloat("最大角インパルス", &settings.maximumAngularImpulse, 1.0f, 1.0f, 500.0f);

            if (ImGui::Button("水向け設定を適用"))
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

        if (ImGui::CollapsingHeader("実行状況", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("初期化: %s", stats.initialized ? "完了" : "待機中");
            ImGui::Text("直前のDispatch: %s", stats.lastDispatchSucceeded ? "成功" : "失敗");
            ImGui::Text("Collider Proxy: %u / %u", stats.proxyCount, GpuSphRigidbodyInteraction::kMaxProxies);
            ImGui::Text("動的Rigidbody: %u / %u", stats.dynamicBodyCount, GpuSphRigidbodyInteraction::kMaxDynamicBodies);
            ImGui::Text("物理粒子半径: %.4f", stats.particleRadius);
            ImGui::Text("フレームリソース数: %u", stats.frameResourceCount);
            ImGui::Separator();
            ImGui::Text("衝突Dispatch: %llu", static_cast<unsigned long long>(stats.collisionDispatchCount));
            ImGui::Text("反作用初期化Dispatch: %llu", static_cast<unsigned long long>(stats.reactionClearDispatchCount));
            ImGui::Text("非同期読取回数: %llu", static_cast<unsigned long long>(stats.readbackCount));
            ImGui::Text("反作用を適用したRigidbody数: %llu", static_cast<unsigned long long>(stats.appliedBodyCount));
            ImGui::Text("直前の直線インパルス: %.4f", stats.lastLinearImpulse);
            ImGui::Text("直前の角インパルス: %.4f", stats.lastAngularImpulse);
        }

        if (ImGui::CollapsingHeader("負荷確認の目安"))
        {
            ImGui::BulletText("開始: 動的OBB 1個 + SPH粒子1000個");
            ImGui::BulletText("Rigidbody数: 4 / 16 / 32個へ段階的に増加");
            ImGui::BulletText("Collider Proxy上限: 64個");
            ImGui::BulletText("動的Rigidbody反作用上限: 32個");
            ImGui::BulletText("SPH粒子数: 4096 / 16384個でも確認");
            ImGui::TextDisabled("反作用の読取はGPU待機を行わないため、数フレーム遅れて反映されます。");
        }

        ImGui::End();
#endif
    }

private:
    GpuSphRigidbodyInteractionDiagnosticsPanel() = default;
    bool visible_ = false;
};

} // namespace Ken4lowEngine
