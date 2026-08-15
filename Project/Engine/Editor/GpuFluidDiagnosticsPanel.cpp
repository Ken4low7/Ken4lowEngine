#include "GpuFluidDiagnosticsPanel.h"

#include "Engine/Graphics/Renderer/GpuFluid/Manager/GpuFluidManager.h"
#include <DirectXCommon.h>

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

namespace
{
#ifdef USE_IMGUI
	const char* GetStressPresetName(GpuFluidStressPreset preset)
	{
		switch (preset)
		{
		case GpuFluidStressPreset::Medium: return "Medium";
		case GpuFluidStressPreset::Heavy: return "Heavy";
		case GpuFluidStressPreset::Extreme: return "Extreme";
		case GpuFluidStressPreset::Off:
		default: return "Off / Custom";
		}
	}

	const char* GetRenderModeName(GpuFluidRenderMode mode)
	{
		switch (mode)
		{
		case GpuFluidRenderMode::Temperature: return "Temperature";
		case GpuFluidRenderMode::Obstacle: return "Obstacle";
		case GpuFluidRenderMode::Density:
		default: return "Density";
		}
	}
#endif // USE_IMGUI
}

void GpuFluidDiagnosticsPanel::Draw()
{
#ifdef USE_IMGUI
	if (ImGui::IsKeyPressed(ImGuiKey_F12, false))
	{
		visible_ = !visible_; // F9/F10/F11の既存診断Shortcutと競合させず、Fluid専用PanelをF12へ割り当てる。
	}
	if (!visible_)
	{
		return;
	}

	GpuFluidManager* manager = GpuFluidManager::GetInstance();
	if (!ImGui::Begin("GPU Fluid Diagnostics", &visible_))
	{
		ImGui::End();
		return;
	}

	ImGui::TextDisabled("F12: toggle panel");
	if (!manager->IsInitialized())
	{
		ImGui::TextDisabled("GPU Fluid runtime is not initialized.");
		ImGui::End();
		return;
	}

	if (!gridEditorValuesInitialized_)
	{
		RefreshGridEditorValues();
	}

	GpuFluidSimulationDesc& simulation = manager->GetEditableSimulationDesc();
	GpuFluidDomainMapping& domain = manager->GetEditableDomainMapping();
	GpuFluidRenderDesc& render = manager->GetEditableRenderDesc();
	const GpuFluidRuntimeStats& stats = manager->GetRuntimeStats();

	if (ImGui::CollapsingHeader("Runtime", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool paused = manager->IsPaused();
		if (ImGui::Checkbox("Paused", &paused))
		{
			manager->SetPaused(paused);
		}
		ImGui::SameLine();
		if (ImGui::Button("Step"))
		{
			manager->RequestSingleStep();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			manager->RequestReset();
		}

		bool renderEnabled = manager->IsRenderEnabled();
		if (ImGui::Checkbox("Forward Rendering", &renderEnabled))
		{
			manager->SetRenderEnabled(renderEnabled);
		}

		ImGui::Text("Simulation: %s", stats.simulationActive ? "Active" : "Idle");
		ImGui::Text("Last Step: %s", stats.lastStepSucceeded ? "OK" : "FAILED");
		ImGui::Text("Substeps this frame: %u", stats.lastFrameSubsteps);
		ImGui::Text("Accumulator: %.4f sec", stats.accumulatorSeconds);
		ImGui::Text("Simulation time: %.2f sec", stats.elapsedSimulationSeconds);
	}

	if (ImGui::CollapsingHeader("Visualization", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int mode = static_cast<int>(render.mode);
		const char* modes[] = { "Density", "Temperature", "Obstacle" };
		if (ImGui::Combo("Render Mode", &mode, modes, IM_ARRAYSIZE(modes)))
		{
			render.mode = static_cast<GpuFluidRenderMode>(std::clamp(mode, 0, 2));
		}
		ImGui::TextDisabled("Current: %s", GetRenderModeName(render.mode));
		ImGui::SliderFloat("Opacity", &render.opacity, 0.0f, 1.0f);
		ImGui::DragFloat("Density Scale", &render.densityScale, 0.05f, 0.0f, 100.0f);
		ImGui::DragFloat("Temperature Scale", &render.temperatureScale, 0.05f, 0.001f, 100.0f);
		ImGui::ColorEdit4("Smoke Color", &render.smokeColor.x);
		ImGui::ColorEdit4("Cold Color", &render.coldColor.x);
		ImGui::ColorEdit4("Hot Color", &render.hotColor.x);
		ImGui::ColorEdit4("Obstacle Color", &render.obstacleColor.x);
	}

	if (ImGui::CollapsingHeader("Domain / Solver", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Domain Origin", &domain.origin.x, 0.05f);
		ImGui::DragFloat3("Domain Axis U", &domain.axisU.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat3("Domain Axis V", &domain.axisV.x, 0.01f, -1.0f, 1.0f);
		if (!domain.IsValid())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "Domain axes must be non-zero and orthogonal.");
		}

		ImGui::SeparatorText("Fixed Step");
		ImGui::DragFloat("Fixed Delta Time", &simulation.fixedDeltaTime, 0.0005f, 1.0f / 240.0f, 1.0f / 15.0f, "%.5f");
		int maxSubsteps = static_cast<int>(simulation.maxSubsteps);
		if (ImGui::SliderInt("Max Substeps", &maxSubsteps, 1, 16))
		{
			simulation.maxSubsteps = static_cast<uint32_t>((std::max)(1, maxSubsteps));
		}
		ImGui::SliderFloat("Velocity Dissipation", &simulation.velocityDissipation, 0.0f, 1.0f);
		ImGui::SliderFloat("Density Dissipation", &simulation.densityDissipation, 0.0f, 1.0f);
		ImGui::SliderFloat("Temperature Dissipation", &simulation.temperatureDissipation, 0.0f, 1.0f);
		ImGui::DragFloat("Vorticity Strength", &simulation.vorticityStrength, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("Buoyancy", &simulation.buoyancy, 0.01f, -20.0f, 20.0f);
		ImGui::DragFloat("Smoke Weight", &simulation.smokeWeight, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("Ambient Temperature", &simulation.ambientTemperature, 0.01f, -20.0f, 20.0f);

		ImGui::SeparatorText("Grid Reconfigure");
		int width = static_cast<int>(pendingGridWidth_);
		int height = static_cast<int>(pendingGridHeight_);
		int pressureIterations = static_cast<int>(pendingPressureIterations_);
		if (ImGui::InputInt("Grid Width", &width)) pendingGridWidth_ = static_cast<uint32_t>((std::max)(8, width));
		if (ImGui::InputInt("Grid Height", &height)) pendingGridHeight_ = static_cast<uint32_t>((std::max)(8, height));
		ImGui::DragFloat("Cell Size", &pendingCellSize_, 0.005f, 0.001f, 10.0f);
		if (ImGui::SliderInt("Pressure Iterations", &pressureIterations, 1, 200))
		{
			pendingPressureIterations_ = static_cast<uint32_t>((std::max)(1, pressureIterations));
		}
		if (ImGui::Button("Apply Grid / Pressure"))
		{
			manager->RequestGridReconfigure(
				pendingGridWidth_,
				pendingGridHeight_,
				pendingCellSize_,
				pendingPressureIterations_);
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload Current"))
		{
			RefreshGridEditorValues();
		}

		const float widthWorld = static_cast<float>(simulation.grid.width) * simulation.grid.cellSize;
		const float heightWorld = static_cast<float>(simulation.grid.height) * simulation.grid.cellSize;
		ImGui::Text("Active Grid: %ux%u | Cell %.4f | World %.2f x %.2f",
			simulation.grid.width,
			simulation.grid.height,
			simulation.grid.cellSize,
			widthWorld,
			heightWorld);
	}

	if (ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const double memoryMiB = static_cast<double>(stats.approximateGpuMemoryBytes) / (1024.0 * 1024.0);
		ImGui::Text("GPU Fluid logical storage: %.2f MiB", memoryMiB);
		ImGui::Text("Scene Emitters: %u | Scene Obstacles: %u", stats.sceneEmitterCount, stats.sceneObstacleCount);
		ImGui::Text("Synthetic Emitters: %u | Synthetic Obstacles: %u", stats.syntheticEmitterCount, stats.syntheticObstacleCount);
		ImGui::Text("Total Simulation Steps: %llu", static_cast<unsigned long long>(stats.totalSimulationSteps));
		ImGui::Text("Reset Count: %llu", static_cast<unsigned long long>(stats.resetCount));

		const FrameUploadArena::Stats uploadStats = DirectXCommon::GetInstance()->GetFrameUploadArena().GetStats();
		const double uploadUsedKiB = static_cast<double>(uploadStats.usedBytes) / 1024.0;
		const double uploadCapacityKiB = static_cast<double>(uploadStats.capacityBytes) / 1024.0;
		const double uploadHighWaterKiB = static_cast<double>(uploadStats.highWaterBytes) / 1024.0;
		const double uploadOverflowKiB = static_cast<double>(uploadStats.overflowBytes) / 1024.0;
		ImGui::SeparatorText("Shared Frame Upload Arena");
		ImGui::Text("Used / Capacity: %.1f / %.1f KiB", uploadUsedKiB, uploadCapacityKiB);
		ImGui::Text("High Water: %.1f KiB", uploadHighWaterKiB);
		ImGui::Text("Overflow: %.1f KiB | allocations %zu", uploadOverflowKiB, uploadStats.overflowAllocationCount);

		ImGui::SeparatorText("Lifetime GPU work counters");
		ImGui::Text("Obstacle Raster Dispatches : %llu", static_cast<unsigned long long>(stats.obstacleDispatchCount));
		ImGui::Text("Emitter Injection Dispatches: %llu", static_cast<unsigned long long>(stats.emitterDispatchCount));
		ImGui::Text("Velocity Dispatches        : %llu", static_cast<unsigned long long>(stats.velocityDispatchCount));
		ImGui::Text("Pressure Dispatches        : %llu", static_cast<unsigned long long>(stats.pressureDispatchCount));
		ImGui::Text("Scalar Dispatches          : %llu", static_cast<unsigned long long>(stats.scalarDispatchCount));
		ImGui::Text("Force Dispatches           : %llu", static_cast<unsigned long long>(stats.forceDispatchCount));
		ImGui::Text("Forward Draws              : %llu", static_cast<unsigned long long>(stats.forwardDrawCount));
	}

	if (ImGui::CollapsingHeader("Stress Test", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Preset: %s", GetStressPresetName(manager->GetStressPreset()));
		if (ImGui::Button("Off / Custom"))
		{
			manager->ApplyStressPreset(GpuFluidStressPreset::Off);
		}
		ImGui::SameLine();
		if (ImGui::Button("Medium"))
		{
			manager->ApplyStressPreset(GpuFluidStressPreset::Medium);
			gridEditorValuesInitialized_ = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Heavy"))
		{
			manager->ApplyStressPreset(GpuFluidStressPreset::Heavy);
			gridEditorValuesInitialized_ = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Extreme"))
		{
			manager->ApplyStressPreset(GpuFluidStressPreset::Extreme);
			gridEditorValuesInitialized_ = false;
		}

		int syntheticEmitters = static_cast<int>(manager->GetSyntheticEmitterCount());
		int syntheticObstacles = static_cast<int>(manager->GetSyntheticObstacleCount());
		bool changed = false;
		changed |= ImGui::SliderInt("Synthetic Emitters", &syntheticEmitters, 0, 256);
		changed |= ImGui::SliderInt("Synthetic Obstacles", &syntheticObstacles, 0, 256);
		if (changed)
		{
			manager->SetSyntheticStressCounts(
				static_cast<uint32_t>((std::max)(0, syntheticEmitters)),
				static_cast<uint32_t>((std::max)(0, syntheticObstacles)));
		}

		ImGui::TextDisabled("Extreme = 1024x1024 / 80 pressure iterations / 64 emitters / 64 obstacles.");
		ImGui::TextDisabled("Grid reconfigure waits only when the preset or grid size actually changes.");
	}

	ImGui::End();
#endif // USE_IMGUI
}

void GpuFluidDiagnosticsPanel::RefreshGridEditorValues()
{
	const GpuFluidSimulationDesc& simulation = GpuFluidManager::GetInstance()->GetSimulationDesc();
	pendingGridWidth_ = simulation.grid.width;
	pendingGridHeight_ = simulation.grid.height;
	pendingCellSize_ = simulation.grid.cellSize;
	pendingPressureIterations_ = simulation.pressureIterations;
	gridEditorValuesInitialized_ = true;
}

} // namespace Ken4lowEngine
