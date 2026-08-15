#include "GpuVolumetricFluidDiagnosticsPanel.h"

#include "Engine/Graphics/RenderTarget/Depth/RenderDepthContext.h"
#include "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/GpuVolumetricFluidManager.h"
#include <DirectXCommon.h>
#include <SRVManager.h>

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

namespace
{
#ifdef USE_IMGUI
	const char* GetStressPresetName(GpuVolumetricFluidStressPreset preset)
	{
		switch (preset)
		{
		case GpuVolumetricFluidStressPreset::Baseline64: return "Baseline 64^3";
		case GpuVolumetricFluidStressPreset::Heavy128: return "Heavy 128^3";
		case GpuVolumetricFluidStressPreset::Off:
		default: return "Off / Custom";
		}
	}

	const char* GetRenderModeName(GpuVolumetricFluidRenderMode mode)
	{
		switch (mode)
		{
		case GpuVolumetricFluidRenderMode::DensityDebug: return "Density Debug";
		case GpuVolumetricFluidRenderMode::TemperatureDebug: return "Temperature Debug";
		case GpuVolumetricFluidRenderMode::ObstacleDebug: return "Obstacle Debug";
		case GpuVolumetricFluidRenderMode::Smoke:
		default: return "Smoke";
		}
	}

	void DrawSectionLabel(const char* label)
	{
		ImGui::Separator();
		ImGui::TextUnformatted(label); // 古いImGuiでも使える基本APIだけでSection見出しを構成する。
	}
#endif // USE_IMGUI
}

void GpuVolumetricFluidDiagnosticsPanel::Draw()
{
#ifdef USE_IMGUI
	if (ImGui::IsKeyPressed(ImGuiKey_F8, false))
	{
		visible_ = !visible_; // F12のPhase16 Panelと分離し、2D/3D診断を独立して開けるようF8へ割り当てる。
	}
	if (!visible_)
	{
		return;
	}

	GpuVolumetricFluidManager* manager = GpuVolumetricFluidManager::GetInstance();
	if (!ImGui::Begin("GPU Volumetric Fluid Diagnostics", &visible_))
	{
		ImGui::End();
		return;
	}

	ImGui::TextDisabled("F8: toggle 3D panel | F12: toggle 2D panel");
	if (!gridEditorValuesInitialized_)
	{
		RefreshGridEditorValues();
	}

	GpuVolumetricFluidSimulationDesc& simulation = manager->GetEditableSimulationDesc();
	GpuVolumetricFluidDomainMapping& domain = manager->GetEditableDomainMapping();
	GpuVolumetricFluidRenderDesc& render = manager->GetEditableRenderDesc();
	const GpuVolumetricFluidRuntimeStats& stats = manager->GetRuntimeStats();

	if (ImGui::CollapsingHeader("Runtime", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool enabled = manager->IsRuntimeEnabled();
		if (ImGui::Checkbox("Enable 3D Runtime", &enabled))
		{
			manager->SetRuntimeEnabled(enabled);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("default OFF to preserve Phase16 scenes");

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
		if (ImGui::Checkbox("Forward Raymarch", &renderEnabled))
		{
			manager->SetRenderEnabled(renderEnabled);
		}

		ImGui::Text("Resources: %s", manager->IsInitialized() ? "Initialized" : "Not allocated");
		ImGui::Text("Simulation: %s", stats.simulationActive ? "Active" : "Idle");
		ImGui::Text("Last Step: %s", stats.lastStepSucceeded ? "OK" : "FAILED");
		ImGui::Text("Substeps this frame: %u", stats.lastFrameSubsteps);
		ImGui::Text("Accumulator: %.4f sec", stats.accumulatorSeconds);
		ImGui::Text("Simulation time: %.2f sec", stats.elapsedSimulationSeconds);
	}

	if (ImGui::CollapsingHeader("Visualization / Lighting", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int mode = static_cast<int>(render.mode);
		const char* modes[] = { "Smoke", "Density Debug", "Temperature Debug", "Obstacle Debug" };
		if (ImGui::Combo("Render Mode", &mode, modes, IM_ARRAYSIZE(modes)))
		{
			render.mode = static_cast<GpuVolumetricFluidRenderMode>(std::clamp(mode, 0, 3));
		}
		ImGui::TextDisabled("Current: %s", GetRenderModeName(render.mode));
		ImGui::SliderFloat("Opacity", &render.opacity, 0.0f, 1.0f);
		ImGui::DragFloat("Density Scale", &render.densityScale, 0.05f, 0.0f, 100.0f);
		ImGui::DragFloat("Temperature Scale", &render.temperatureScale, 0.05f, 0.001f, 100.0f);
		ImGui::DragFloat("Absorption", &render.absorption, 0.05f, 0.0f, 50.0f);
		ImGui::DragFloat("Thermal Emission", &render.emissionStrength, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("Ray Step Scale", &render.stepScale, 0.02f, 0.1f, 8.0f);
		ImGui::SliderFloat("Early Exit T", &render.earlyExitTransmittance, 0.0f, 0.25f, "%.4f");
		int maxSteps = static_cast<int>(render.maxSteps);
		if (ImGui::SliderInt("Max Ray Steps", &maxSteps, 16, 1024))
		{
			render.maxSteps = static_cast<uint32_t>(std::clamp(maxSteps, 16, 1024));
		}

		DrawSectionLabel("Directional Scattering");
		ImGui::DragFloat("Scattering Strength", &render.scatteringStrength, 0.02f, 0.0f, 10.0f);
		ImGui::DragFloat("Ambient Scattering", &render.ambientScattering, 0.01f, 0.0f, 5.0f);
		ImGui::SliderFloat("Anisotropy", &render.anisotropy, -0.94f, 0.94f);
		ImGui::SliderFloat("Self Shadow Strength", &render.selfShadowStrength, 0.0f, 1.0f);
		ImGui::DragFloat("Shadow Sample Distance (cells)", &render.shadowSampleDistanceCells, 0.1f, 0.1f, 32.0f);
		ImGui::ColorEdit4("Smoke Color", &render.smokeColor.x);
		ImGui::ColorEdit4("Cold Color", &render.coldColor.x);
		ImGui::ColorEdit4("Hot Color", &render.hotColor.x);
		ImGui::ColorEdit4("Obstacle Color", &render.obstacleColor.x);
	}

	if (ImGui::CollapsingHeader("Domain / Solver", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool domainChanged = false;
		domainChanged |= ImGui::DragFloat3("Domain Origin", &domain.origin.x, 0.05f);
		domainChanged |= ImGui::DragFloat3("Domain Axis U", &domain.axisU.x, 0.01f, -1.0f, 1.0f);
		domainChanged |= ImGui::DragFloat3("Domain Axis V", &domain.axisV.x, 0.01f, -1.0f, 1.0f);
		domainChanged |= ImGui::DragFloat3("Domain Axis W", &domain.axisW.x, 0.01f, -1.0f, 1.0f);
		if (!domain.IsValid())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "U/V/W axes must be non-zero and pairwise orthogonal.");
		}
		else if (domainChanged)
		{
			manager->RequestReset(); // World/Grid対応が変わったら旧座標系のfieldを新Domainとして解釈せずゼロから再開する。
		}

		DrawSectionLabel("Fixed Step");
		ImGui::DragFloat("Fixed Delta Time", &simulation.fixedDeltaTime, 0.0005f, 1.0f / 240.0f, 1.0f / 15.0f, "%.5f");
		int maxSubsteps = static_cast<int>(simulation.maxSubsteps);
		if (ImGui::SliderInt("Max Substeps", &maxSubsteps, 1, 8))
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

		DrawSectionLabel("Grid Reconfigure");
		int width = static_cast<int>(pendingGridWidth_);
		int height = static_cast<int>(pendingGridHeight_);
		int depth = static_cast<int>(pendingGridDepth_);
		int pressureIterations = static_cast<int>(pendingPressureIterations_);
		if (ImGui::InputInt("Grid Width", &width)) pendingGridWidth_ = static_cast<uint32_t>(std::clamp(width, 8, 256));
		if (ImGui::InputInt("Grid Height", &height)) pendingGridHeight_ = static_cast<uint32_t>(std::clamp(height, 8, 256));
		if (ImGui::InputInt("Grid Depth", &depth)) pendingGridDepth_ = static_cast<uint32_t>(std::clamp(depth, 8, 256));
		ImGui::DragFloat("Cell Size", &pendingCellSize_, 0.005f, 0.001f, 10.0f);
		if (ImGui::SliderInt("Pressure Iterations", &pressureIterations, 1, 192))
		{
			pendingPressureIterations_ = static_cast<uint32_t>((std::max)(1, pressureIterations));
		}
		if (ImGui::Button("Apply Grid / Pressure"))
		{
			manager->RequestGridReconfigure(
				pendingGridWidth_,
				pendingGridHeight_,
				pendingGridDepth_,
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
		const float depthWorld = static_cast<float>(simulation.grid.depth) * simulation.grid.cellSize;
		ImGui::Text("Configured Grid: %ux%ux%u | Cell %.4f | World %.2f x %.2f x %.2f",
			simulation.grid.width,
			simulation.grid.height,
			simulation.grid.depth,
			simulation.grid.cellSize,
			widthWorld,
			heightWorld,
			depthWorld);
	}

	if (ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const uint64_t configuredBytes = simulation.grid.GetVoxelCount() * 39ull;
		const double activeMemoryMiB = static_cast<double>(stats.approximateGpuMemoryBytes) / (1024.0 * 1024.0);
		const double configuredMemoryMiB = static_cast<double>(configuredBytes) / (1024.0 * 1024.0);
		ImGui::Text("Texture3D logical storage: %.2f MiB active | %.2f MiB configured", activeMemoryMiB, configuredMemoryMiB);
		ImGui::Text("Scene Emitters: %u | Scene Obstacles: %u", stats.sceneEmitterCount, stats.sceneObstacleCount);
		ImGui::Text("Synthetic Emitters: %u | Synthetic Obstacles: %u", stats.syntheticEmitterCount, stats.syntheticObstacleCount);
		ImGui::Text("Emitter Upload: %u accepted | %u culled", stats.lastInjectedEmitterCount, stats.lastCulledEmitterCount);
		ImGui::Text("Obstacle Raster: %u accepted | %u culled", stats.lastRasterObstacleCount, stats.lastCulledObstacleCount);
		ImGui::Text("Pressure Iterations / projection: %u", stats.lastPressureIterationCount);
		ImGui::Text("Total Simulation Steps: %llu", static_cast<unsigned long long>(stats.totalSimulationSteps));
		ImGui::Text("Reset / Reconfigure / Failed: %llu / %llu / %llu",
			static_cast<unsigned long long>(stats.resetCount),
			static_cast<unsigned long long>(stats.reconfigureCount),
			static_cast<unsigned long long>(stats.failedReconfigureCount));
		ImGui::Text("Duplicate Frame Update Skips: %llu", static_cast<unsigned long long>(stats.duplicateFrameUpdateSkipCount));

		DrawSectionLabel("Lifetime GPU work counters");
		ImGui::Text("Obstacle Raster Dispatches : %llu", static_cast<unsigned long long>(stats.obstacleDispatchCount));
		ImGui::Text("Emitter Injection Dispatches: %llu", static_cast<unsigned long long>(stats.emitterDispatchCount));
		ImGui::Text("Velocity Dispatches        : %llu", static_cast<unsigned long long>(stats.velocityDispatchCount));
		ImGui::Text("Pressure Dispatches        : %llu", static_cast<unsigned long long>(stats.pressureDispatchCount));
		ImGui::Text("Scalar Dispatches          : %llu", static_cast<unsigned long long>(stats.scalarDispatchCount));
		ImGui::Text("Force Dispatches           : %llu", static_cast<unsigned long long>(stats.forceDispatchCount));
		ImGui::Text("Raymarch Draws / Packets   : %llu / %llu",
			static_cast<unsigned long long>(stats.forwardDrawCount),
			static_cast<unsigned long long>(stats.forwardPacketCount));

		const RenderDepthContextStats& depthStats = RenderDepthContext::GetInstance()->GetStats();
		DrawSectionLabel("Depth-aware Composition");
		ImGui::Text("Attachments: %u | Prepared: %s", depthStats.attachmentCount, depthStats.shaderReadPrepared ? "yes" : "no");
		ImGui::Text("Prepare / Restore / Failed: %llu / %llu / %llu",
			static_cast<unsigned long long>(depthStats.prepareCount),
			static_cast<unsigned long long>(depthStats.restoreCount),
			static_cast<unsigned long long>(depthStats.failedPrepareCount));
		ImGui::Text("Depth Override Pushes: %llu", static_cast<unsigned long long>(depthStats.overridePushCount));
		ImGui::TextDisabled("RenderViewOverride without a registered depth attachment safely skips volume draw.");

		const SRVManager::DescriptorStats descriptorStats = SRVManager::GetInstance()->GetDescriptorStats();
		DrawSectionLabel("Shared SRV Descriptor Heap");
		ImGui::Text("Persistent: %u / %u | high water %u",
			descriptorStats.persistentInUse,
			descriptorStats.persistentCapacity,
			descriptorStats.persistentHighWater);
		ImGui::Text("Transient: %u / %u | high water %u",
			descriptorStats.transientInUse,
			descriptorStats.transientCapacity,
			descriptorStats.transientHighWater);
		ImGui::Text("Descriptor Exhaustions: %llu", static_cast<unsigned long long>(descriptorStats.exhaustionCount));

		const FrameUploadArena::Stats uploadStats = DirectXCommon::GetInstance()->GetFrameUploadArena().GetStats();
		DrawSectionLabel("Shared Frame Upload Arena");
		ImGui::Text("Used / Capacity: %.1f / %.1f KiB",
			static_cast<double>(uploadStats.usedBytes) / 1024.0,
			static_cast<double>(uploadStats.capacityBytes) / 1024.0);
		ImGui::Text("High Water: %.1f KiB | Overflow %.1f KiB",
			static_cast<double>(uploadStats.highWaterBytes) / 1024.0,
			static_cast<double>(uploadStats.overflowBytes) / 1024.0);
	}

	if (ImGui::CollapsingHeader("Stress Test", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Preset: %s", GetStressPresetName(manager->GetStressPreset()));
		if (ImGui::Button("Off / Custom"))
		{
			manager->ApplyStressPreset(GpuVolumetricFluidStressPreset::Off);
		}
		ImGui::SameLine();
		if (ImGui::Button("Baseline 64^3"))
		{
			manager->ApplyStressPreset(GpuVolumetricFluidStressPreset::Baseline64);
			gridEditorValuesInitialized_ = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Heavy 128^3"))
		{
			manager->ApplyStressPreset(GpuVolumetricFluidStressPreset::Heavy128);
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

		ImGui::TextDisabled("64^3 = ~9.75 MiB logical fields. 128^3 = ~78 MiB before driver allocation overhead.");
		ImGui::TextDisabled("Grid changes are deferred to Update and fence-safe before Texture3D recreation.");
	}

	ImGui::End();
#endif // USE_IMGUI
}

void GpuVolumetricFluidDiagnosticsPanel::RefreshGridEditorValues()
{
	const GpuVolumetricFluidSimulationDesc& simulation =
		GpuVolumetricFluidManager::GetInstance()->GetSimulationDesc();
	pendingGridWidth_ = simulation.grid.width;
	pendingGridHeight_ = simulation.grid.height;
	pendingGridDepth_ = simulation.grid.depth;
	pendingCellSize_ = simulation.grid.cellSize;
	pendingPressureIterations_ = simulation.pressureIterations;
	gridEditorValuesInitialized_ = true;
}

} // namespace Ken4lowEngine
