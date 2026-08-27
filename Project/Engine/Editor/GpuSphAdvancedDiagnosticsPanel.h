#pragma once

#include "Engine/Graphics/Renderer/GpuFluid/Sph/Manager/GpuSphManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>

namespace Ken4lowEngine
{

/// DFSPH・CFL・表面パラメータを比較、調整するための診断パネル。
class GpuSphAdvancedDiagnosticsPanel final
{
public:
    static GpuSphAdvancedDiagnosticsPanel* GetInstance()
    {
        static GpuSphAdvancedDiagnosticsPanel instance;
        return &instance;
    }

    void Draw()
    {
#ifdef USE_IMGUI
        if (ImGui::IsKeyPressed(ImGuiKey_F7, false))
        {
            visible_ = !visible_;
        }
        if (!visible_)
        {
            return;
        }

        GpuSphManager* manager = GpuSphManager::GetInstance();
        if (!ImGui::Begin("SPH詳細設定 / DFSPH", &visible_))
        {
            ImGui::End();
            return;
        }

        if (!manager || !manager->IsInitialized())
        {
            ImGui::TextDisabled("SPHシミュレーションが初期化されていません。");
            ImGui::End();
            return;
        }

        GpuSphSimulationSettings& settings = manager->GetEditableSimulationSettings();
        const GpuSphRuntimeStats& stats = manager->GetRuntimeStats();
        const GpuSphParticleBufferStats bufferStats = manager->GetParticleBufferStats();

        if (ImGui::Button("水向け安定設定を適用"))
        {
            manager->ApplyWaterProductionPreset();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("F7: 表示切替");

        ImGui::SeparatorText("DFSPH補正");
        ImGui::Checkbox("DFSPHを使用", &settings.dfsphEnabled);

        int densityIterations = static_cast<int>(settings.dfsphDensityIterations);
        if (ImGui::SliderInt("密度補正回数", &densityIterations, 1, 12))
        {
            settings.dfsphDensityIterations = static_cast<uint32_t>((std::max)(1, densityIterations));
        }

        int divergenceIterations = static_cast<int>(settings.dfsphDivergenceIterations);
        if (ImGui::SliderInt("発散補正回数", &divergenceIterations, 0, 12))
        {
            settings.dfsphDivergenceIterations = static_cast<uint32_t>((std::max)(0, divergenceIterations));
        }

        ImGui::SliderFloat("密度緩和率", &settings.dfsphDensityRelaxation, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("発散緩和率", &settings.dfsphDivergenceRelaxation, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("密度誤差許容値", &settings.dfsphDensityErrorTolerance, 0.0005f, 0.0f, 0.1f, "%.4f");
        ImGui::DragFloat("発散誤差許容値", &settings.dfsphDivergenceErrorTolerance, 0.0005f, 0.0f, 0.1f, "%.4f");
        ImGui::DragFloat("最大速度補正", &settings.maxDfsphVelocityCorrection, 0.05f, 0.05f, 20.0f, "%.2f");
        ImGui::Checkbox("前フレームの解を利用", &settings.dfsphWarmStartEnabled);
        ImGui::SliderFloat("前フレーム利用率", &settings.dfsphWarmStartStrength, 0.0f, 1.0f, "%.3f");

        ImGui::SeparatorText("時間刻み・表面・境界");
        ImGui::Checkbox("CFLで時間刻みを自動調整", &settings.adaptiveCflEnabled);
        ImGui::SliderFloat("CFL係数", &settings.cflNumber, 0.05f, 0.95f, "%.3f");
        ImGui::DragFloat("最小デルタタイム", &settings.minimumDeltaTime, 0.0001f, 1.0f / 2000.0f, settings.fixedDeltaTime, "%.6f");
        ImGui::DragFloat("表面張力", &settings.surfaceTensionStrength, 0.001f, 0.0f, 1.0f, "%.4f");
        ImGui::DragFloat("XSPH補正", &settings.xsphStrength, 0.001f, 0.0f, 0.5f, "%.3f");
        ImGui::SliderFloat("境界摩擦", &settings.boundaryFriction, 0.0f, 1.0f, "%.3f");

        ImGui::SeparatorText("粒子数プリセット");
        if (ImGui::Button("1K")) manager->SetActiveParticleCount(1000u);
        ImGui::SameLine();
        if (ImGui::Button("4K")) manager->SetActiveParticleCount(4096u);
        ImGui::SameLine();
        if (ImGui::Button("16K")) manager->SetActiveParticleCount(16384u);
        ImGui::SameLine();
        if (ImGui::Button("65K")) manager->SetActiveParticleCount(65536u);
        ImGui::Text("使用中 / 最大: %u / %u", bufferStats.activeCount, bufferStats.capacity);
        ImGui::TextDisabled("粒子数を変更するとSPHシミュレーションを再初期化します。");

        ImGui::SeparatorText("実行状況");
        ImGui::Text("ソルバー: %s", stats.dfsphActive ? "DFSPH" : "WCSPH / Tait EOS");
        ImGui::Text("実効dt: %.6f | 設定dt: %.6f", stats.effectiveDeltaTime, settings.fixedDeltaTime);
        ImGui::Text("最大粒子速度: %.3f m/s", stats.lastMeasuredMaxSpeed);
        ImGui::Text("最大密度誤差: %.5f | 許容値: %.5f",
            stats.lastMaxDensityError,
            settings.dfsphDensityErrorTolerance);
        ImGui::Text("最大発散誤差: %.5f | 許容値: %.5f",
            stats.lastMaxDivergenceError,
            settings.dfsphDivergenceErrorTolerance);
        ImGui::Text("計測用フレームリソース: %u | 非同期読取回数: %llu",
            stats.frameResourceCount,
            static_cast<unsigned long long>(stats.cflReadbackCount));
        ImGui::Text("密度補正回数: %u | 発散補正回数: %u",
            stats.lastDensityIterations,
            stats.lastDivergenceIterations);
        ImGui::Text("係数計算Dispatch: %llu", static_cast<unsigned long long>(stats.dfsphFactorDispatchCount));
        ImGui::Text("密度補正Dispatch: %llu", static_cast<unsigned long long>(stats.dfsphDensityDispatchCount));
        ImGui::Text("発散補正Dispatch: %llu", static_cast<unsigned long long>(stats.dfsphDivergenceDispatchCount));
        ImGui::Text("GPU計測Dispatch: %llu", static_cast<unsigned long long>(stats.cflMetricDispatchCount));
        ImGui::Text("CFL安定化回数: %llu", static_cast<unsigned long long>(stats.cflStabilizationCount));
        ImGui::Text("WCSPH圧力Dispatch: %llu", static_cast<unsigned long long>(stats.pressureDispatchCount));
        ImGui::TextDisabled("DFSPHではSpatial Hashを利用して近傍粒子だけを探索します。");
        ImGui::TextDisabled("前フレームのDFSPH係数は最初の補正反復でのみ再利用します。");
        ImGui::TextDisabled("速度・密度・発散の計測はGPU待機を発生させない非同期読取です。");
        ImGui::TextDisabled("DFSPHを無効にするとWCSPHとの挙動比較ができます。"); // 工程番号ではなく現在の機能差だけを説明する。

        ImGui::End();
#endif
    }

private:
    bool visible_ = false;
};

} // namespace Ken4lowEngine
