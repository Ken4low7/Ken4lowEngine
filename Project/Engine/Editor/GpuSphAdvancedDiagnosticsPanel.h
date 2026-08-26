#pragma once

#include "Engine/Graphics/Renderer/GpuFluid/Sph/Manager/GpuSphManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>

namespace Ken4lowEngine
{

/// W9.5: DFSPH / CFL / surface parameterをF7から比較・調整する診断Panel。
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
        if (!ImGui::Begin("W9.5 Advanced SPH / DFSPH", &visible_))
        {
            ImGui::End();
            return;
        }

        if (!manager || !manager->IsInitialized())
        {
            ImGui::TextDisabled("SPH runtime is not initialized.");
            ImGui::End();
            return;
        }

        GpuSphSimulationSettings& settings = manager->GetEditableSimulationSettings();
        const GpuSphRuntimeStats& stats = manager->GetRuntimeStats();
        const GpuSphParticleBufferStats bufferStats = manager->GetParticleBufferStats();

        if (ImGui::Button("Water Production Preset"))
        {
            manager->ApplyWaterProductionPreset();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("F7: toggle");

        ImGui::SeparatorText("DFSPH Projection");
        ImGui::Checkbox("DFSPH Enabled", &settings.dfsphEnabled);

        int densityIterations = static_cast<int>(settings.dfsphDensityIterations);
        if (ImGui::SliderInt("Density Iterations", &densityIterations, 1, 12))
        {
            settings.dfsphDensityIterations = static_cast<uint32_t>((std::max)(1, densityIterations));
        }

        int divergenceIterations = static_cast<int>(settings.dfsphDivergenceIterations);
        if (ImGui::SliderInt("Divergence Iterations", &divergenceIterations, 0, 12))
        {
            settings.dfsphDivergenceIterations = static_cast<uint32_t>((std::max)(0, divergenceIterations));
        }

        ImGui::SliderFloat("Density Relaxation", &settings.dfsphDensityRelaxation, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Divergence Relaxation", &settings.dfsphDivergenceRelaxation, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Density Error Tolerance", &settings.dfsphDensityErrorTolerance, 0.0005f, 0.0f, 0.1f, "%.4f");
        ImGui::DragFloat("Divergence Tolerance", &settings.dfsphDivergenceErrorTolerance, 0.0005f, 0.0f, 0.1f, "%.4f");
        ImGui::DragFloat("Max Velocity Correction", &settings.maxDfsphVelocityCorrection, 0.05f, 0.05f, 20.0f, "%.2f");
        ImGui::Checkbox("Warm Start", &settings.dfsphWarmStartEnabled);
        ImGui::SliderFloat("Warm Start Strength", &settings.dfsphWarmStartStrength, 0.0f, 1.0f, "%.3f");

        ImGui::SeparatorText("CFL / Surface / Boundary");
        ImGui::Checkbox("Adaptive CFL", &settings.adaptiveCflEnabled);
        ImGui::SliderFloat("CFL Number", &settings.cflNumber, 0.05f, 0.95f, "%.3f");
        ImGui::DragFloat("Minimum Delta Time", &settings.minimumDeltaTime, 0.0001f, 1.0f / 2000.0f, settings.fixedDeltaTime, "%.6f");
        ImGui::DragFloat("Surface Tension", &settings.surfaceTensionStrength, 0.001f, 0.0f, 1.0f, "%.4f");
        ImGui::DragFloat("XSPH Strength", &settings.xsphStrength, 0.001f, 0.0f, 0.5f, "%.3f");
        ImGui::SliderFloat("Boundary Friction", &settings.boundaryFriction, 0.0f, 1.0f, "%.3f");

        ImGui::SeparatorText("Particle Stress Presets");
        if (ImGui::Button("1K")) manager->SetActiveParticleCount(1000u);
        ImGui::SameLine();
        if (ImGui::Button("4K")) manager->SetActiveParticleCount(4096u);
        ImGui::SameLine();
        if (ImGui::Button("16K")) manager->SetActiveParticleCount(16384u);
        ImGui::SameLine();
        if (ImGui::Button("65K")) manager->SetActiveParticleCount(65536u);
        ImGui::Text("Active / Capacity: %u / %u", bufferStats.activeCount, bufferStats.capacity);
        ImGui::TextDisabled("Stress order: 1K -> 4K -> 16K -> 65K. Each button requests an SPH reset.");

        ImGui::SeparatorText("W9.5 Runtime");
        ImGui::Text("Solver: %s", stats.dfsphActive ? "DFSPH" : "WCSPH / Tait EOS Fallback");
        ImGui::Text("Effective dt: %.6f | Requested dt: %.6f", stats.effectiveDeltaTime, settings.fixedDeltaTime);
        ImGui::Text("Measured Max Speed: %.3f m/s", stats.lastMeasuredMaxSpeed);
        ImGui::Text("Max Density Error: %.5f | Tolerance: %.5f",
            stats.lastMaxDensityError,
            settings.dfsphDensityErrorTolerance);
        ImGui::Text("Max Divergence Error: %.5f | Tolerance: %.5f",
            stats.lastMaxDivergenceError,
            settings.dfsphDivergenceErrorTolerance);
        ImGui::Text("Metric Frame Resources: %u | Async Readbacks: %llu",
            stats.frameResourceCount,
            static_cast<unsigned long long>(stats.cflReadbackCount));
        ImGui::Text("Density Iterations: %u | Divergence Iterations: %u",
            stats.lastDensityIterations,
            stats.lastDivergenceIterations);
        ImGui::Text("Factor Dispatches: %llu", static_cast<unsigned long long>(stats.dfsphFactorDispatchCount));
        ImGui::Text("Density Projection Dispatches: %llu", static_cast<unsigned long long>(stats.dfsphDensityDispatchCount));
        ImGui::Text("Divergence Projection Dispatches: %llu", static_cast<unsigned long long>(stats.dfsphDivergenceDispatchCount));
        ImGui::Text("GPU Metric Dispatches: %llu", static_cast<unsigned long long>(stats.cflMetricDispatchCount));
        ImGui::Text("CFL Stabilizations: %llu", static_cast<unsigned long long>(stats.cflStabilizationCount));
        ImGui::Text("WCSPH Pressure Dispatches: %llu", static_cast<unsigned long long>(stats.pressureDispatchCount));
        ImGui::TextDisabled("DFSPH ON keeps the W6 27-cell Spatial Hash and bypasses Tait pressure force passes.");
        ImGui::TextDisabled("Warm Start reuses the previous DFSPH kappa only on the first projection iteration.");
        ImGui::TextDisabled("Speed / density / divergence metrics use asynchronous Frame Resource readback without GPU waits.");
        ImGui::TextDisabled("Disable DFSPH to A/B compare against the previous W7 WCSPH behavior.");

        ImGui::End();
#endif
    }

private:
    bool visible_ = false;
};

} // namespace Ken4lowEngine
