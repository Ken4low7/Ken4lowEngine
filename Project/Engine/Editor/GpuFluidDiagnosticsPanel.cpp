#include "GpuFluidDiagnosticsPanel.h"

#include "Engine/Graphics/Renderer/GpuFluid/Manager/GpuFluidManager.h"
#include "Engine/Graphics/Renderer/GpuFluid/Sph/Manager/GpuSphManager.h"
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
		case GpuFluidStressPreset::Medium: return "中";
		case GpuFluidStressPreset::Heavy: return "高";
		case GpuFluidStressPreset::Extreme: return "最大";
		case GpuFluidStressPreset::Off:
		default: return "無効 / 手動設定";
		}
	}

	const char* GetRenderModeName(GpuFluidRenderMode mode)
	{
		switch (mode)
		{
		case GpuFluidRenderMode::Temperature: return "温度";
		case GpuFluidRenderMode::Obstacle: return "障害物";
		case GpuFluidRenderMode::Density:
		default: return "密度";
		}
	}
#endif // USE_IMGUI
}

void GpuFluidDiagnosticsPanel::Draw()
{
#ifdef USE_IMGUI
	if (ImGui::IsKeyPressed(ImGuiKey_F7, false))
	{
		visible_ = !visible_; // 流体関連の診断をF7からまとめて開閉する。
	}
	if (!visible_)
	{
		return;
	}

	GpuFluidManager* manager = GpuFluidManager::GetInstance();
	if (!ImGui::Begin("GPU流体診断", &visible_))
	{
		ImGui::End();
		return;
	}

	ImGui::TextDisabled("F7: 表示切替");
	if (!manager->IsInitialized())
	{
		ImGui::TextDisabled("GPU流体シミュレーションが初期化されていません。");
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

	if (ImGui::CollapsingHeader("実行制御", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool paused = manager->IsPaused();
		if (ImGui::Checkbox("一時停止", &paused))
		{
			manager->SetPaused(paused);
		}
		ImGui::SameLine();
		if (ImGui::Button("1ステップ実行"))
		{
			manager->RequestSingleStep();
		}
		ImGui::SameLine();
		if (ImGui::Button("リセット"))
		{
			manager->RequestReset();
		}

		bool renderEnabled = manager->IsRenderEnabled();
		if (ImGui::Checkbox("前方描画を有効化", &renderEnabled))
		{
			manager->SetRenderEnabled(renderEnabled);
		}

		ImGui::Text("シミュレーション: %s", stats.simulationActive ? "実行中" : "待機中");
		ImGui::Text("直前のステップ: %s", stats.lastStepSucceeded ? "成功" : "失敗");
		ImGui::Text("このフレームのSubstep: %u", stats.lastFrameSubsteps);
		ImGui::Text("時間蓄積: %.4f 秒", stats.accumulatorSeconds);
		ImGui::Text("シミュレーション時間: %.2f 秒", stats.elapsedSimulationSeconds);
	}

	if (ImGui::CollapsingHeader("SPHシミュレーション", ImGuiTreeNodeFlags_DefaultOpen))
	{
		GpuSphManager* sphManager = GpuSphManager::GetInstance();
		GpuSphSimulationSettings& sphSettings = sphManager->GetEditableSimulationSettings();
		const GpuSphParticleBufferStats sphBufferStats = sphManager->GetParticleBufferStats();
		const GpuSphRuntimeStats& sphStats = sphManager->GetRuntimeStats();
		const double sphMemoryMiB = static_cast<double>(sphStats.approximateGpuMemoryBytes) / (1024.0 * 1024.0);

		bool sphPaused = sphManager->IsPaused();
		if (ImGui::Checkbox("SPHを一時停止", &sphPaused))
		{
			sphManager->SetPaused(sphPaused);
		}
		ImGui::SameLine();
		if (ImGui::Button("SPHを1ステップ実行"))
		{
			sphManager->RequestSingleStep();
		}
		ImGui::SameLine();
		if (ImGui::Button("SPHをリセット"))
		{
			sphManager->RequestReset();
		}

		int activeParticles = static_cast<int>(sphBufferStats.activeCount);
		const int maxActiveParticles = static_cast<int>((std::max)(1u, sphBufferStats.capacity));
		if (ImGui::SliderInt("使用粒子数", &activeParticles, 1, maxActiveParticles))
		{
			sphManager->SetActiveParticleCount(static_cast<uint32_t>((std::max)(1, activeParticles)));
		}

		ImGui::DragFloat("固定デルタタイム", &sphSettings.fixedDeltaTime, 0.0001f, 1.0f / 240.0f, 1.0f / 30.0f, "%.5f");
		int sphMaxSubsteps = static_cast<int>(sphSettings.maxSubsteps);
		if (ImGui::SliderInt("最大Substep数", &sphMaxSubsteps, 1, 8))
		{
			sphSettings.maxSubsteps = static_cast<uint32_t>((std::max)(1, sphMaxSubsteps));
		}
		ImGui::DragFloat("粒子質量", &sphSettings.particleMass, 0.001f, 0.001f, 10.0f);
		ImGui::DragFloat("平滑化半径", &sphSettings.smoothingRadius, 0.001f, 0.01f, 2.0f);
		ImGui::DragFloat("目標密度", &sphSettings.targetDensity, 1.0f, 1.0f, 5000.0f);
		ImGui::DragFloat("圧力剛性", &sphSettings.pressureStiffness, 1.0f, 0.0f, 5000.0f);
		ImGui::DragFloat("粘性", &sphSettings.viscosityStrength, 0.005f, 0.0f, 10.0f);
		ImGui::SliderFloat("境界減衰", &sphSettings.boundaryDamping, 0.0f, 1.0f);
		ImGui::DragFloat3("重力", &sphSettings.gravity.x, 0.05f);
		ImGui::DragFloat3("境界最小", &sphSettings.boundaryMin.x, 0.05f);
		ImGui::DragFloat3("境界最大", &sphSettings.boundaryMax.x, 0.05f);

		ImGui::SeparatorText("SPH実行状況");
		ImGui::Text("粒子Buffer: %s", sphBufferStats.initialized ? "準備完了" : "失敗");
		ImGui::Text("使用中 / 最大: %u / %u", sphBufferStats.activeCount, sphBufferStats.capacity);
		ImGui::Text("粒子Stride: %u bytes", sphBufferStats.strideBytes);
		ImGui::Text("SPH GPU使用量: %.2f MiB", sphMemoryMiB);
		ImGui::Text("直前のステップ: %s | Substep: %u", sphStats.lastStepSucceeded ? "成功" : "失敗", sphStats.lastFrameSubsteps);
		ImGui::Text("累計シミュレーションステップ: %llu", static_cast<unsigned long long>(sphStats.totalSimulationSteps));
		ImGui::Text("リセット回数: %llu | 総Dispatch数: %llu",
			static_cast<unsigned long long>(sphStats.resetCount),
			static_cast<unsigned long long>(sphStats.totalDispatchCount));
		ImGui::Text("重力: %llu | 境界: %llu | 密度: %llu",
			static_cast<unsigned long long>(sphStats.gravityDispatchCount),
			static_cast<unsigned long long>(sphStats.boundaryDispatchCount),
			static_cast<unsigned long long>(sphStats.densityDispatchCount));
		ImGui::Text("圧力: %llu | 粘性: %llu | 予測/積分: %llu",
			static_cast<unsigned long long>(sphStats.pressureDispatchCount),
			static_cast<unsigned long long>(sphStats.viscosityDispatchCount),
			static_cast<unsigned long long>(sphStats.predictionDispatchCount));

		ImGui::SeparatorText("空間ハッシュ / GPUソート");
		ImGui::Text("空間ハッシュ: %s", sphStats.spatialHashReady ? "準備完了" : "待機中 / 無効");
		ImGui::Text("Grid: %u x %u x %u | Cell数: %u",
			sphStats.spatialGridDimX,
			sphStats.spatialGridDimY,
			sphStats.spatialGridDimZ,
			sphStats.spatialCellCount);
		ImGui::Text("Cellサイズ: %.4f | Bitonic Sort数: %u", sphStats.spatialCellSize, sphStats.sortedParticleCount);
		ImGui::Text("Hash構築: %llu | Sort Dispatch: %llu | Cell範囲Dispatch: %llu",
			static_cast<unsigned long long>(sphStats.spatialHashBuildCount),
			static_cast<unsigned long long>(sphStats.spatialHashSortDispatchCount),
			static_cast<unsigned long long>(sphStats.cellRangeDispatchCount));
		if (sphStats.spatialCellCount == 0)
		{
			ImGui::TextDisabled("空間Gridが無効です。平滑化半径を広げるか、シミュレーション境界を小さくしてください。");
		}
		else
		{
			ImGui::TextDisabled("各粒子は現在Cellと周囲26Cellだけを探索し、全粒子走査を避けます。");
		}

		if (sphBufferStats.initialized)
		{
			const GpuSphParticleBuffer& particleBuffer = sphManager->GetParticleBuffer();
			ImGui::Text("SRV: %u | Compute SRV: %u | UAV: %u",
				particleBuffer.GetSrvIndex(),
				particleBuffer.GetComputeSrvIndex(),
				particleBuffer.GetUavIndex());
			ImGui::Text("Resource State: %u", static_cast<uint32_t>(particleBuffer.GetCurrentState()));
		}
	}

	if (ImGui::CollapsingHeader("描画", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int mode = static_cast<int>(render.mode);
		const char* modes[] = { "密度", "温度", "障害物" };
		if (ImGui::Combo("表示モード", &mode, modes, IM_ARRAYSIZE(modes)))
		{
			render.mode = static_cast<GpuFluidRenderMode>(std::clamp(mode, 0, 2));
		}
		ImGui::TextDisabled("現在: %s", GetRenderModeName(render.mode));
		ImGui::SliderFloat("不透明度", &render.opacity, 0.0f, 1.0f);
		ImGui::DragFloat("密度倍率", &render.densityScale, 0.05f, 0.0f, 100.0f);
		ImGui::DragFloat("温度倍率", &render.temperatureScale, 0.05f, 0.001f, 100.0f);
		ImGui::ColorEdit4("煙の色", &render.smokeColor.x);
		ImGui::ColorEdit4("低温色", &render.coldColor.x);
		ImGui::ColorEdit4("高温色", &render.hotColor.x);
		ImGui::ColorEdit4("障害物色", &render.obstacleColor.x);
	}

	if (ImGui::CollapsingHeader("領域 / Solver", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("領域原点", &domain.origin.x, 0.05f);
		ImGui::DragFloat3("領域軸 U", &domain.axisU.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat3("領域軸 V", &domain.axisV.x, 0.01f, -1.0f, 1.0f);
		if (!domain.IsValid())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "領域軸はゼロベクトルにせず、互いに直交させてください。");
		}

		ImGui::SeparatorText("固定ステップ");
		ImGui::DragFloat("固定デルタタイム##Fluid", &simulation.fixedDeltaTime, 0.0005f, 1.0f / 240.0f, 1.0f / 15.0f, "%.5f");
		int maxSubsteps = static_cast<int>(simulation.maxSubsteps);
		if (ImGui::SliderInt("最大Substep数##Fluid", &maxSubsteps, 1, 16))
		{
			simulation.maxSubsteps = static_cast<uint32_t>((std::max)(1, maxSubsteps));
		}
		ImGui::SliderFloat("速度減衰", &simulation.velocityDissipation, 0.0f, 1.0f);
		ImGui::SliderFloat("密度減衰", &simulation.densityDissipation, 0.0f, 1.0f);
		ImGui::SliderFloat("温度減衰", &simulation.temperatureDissipation, 0.0f, 1.0f);
		ImGui::DragFloat("渦度の強さ", &simulation.vorticityStrength, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("浮力", &simulation.buoyancy, 0.01f, -20.0f, 20.0f);
		ImGui::DragFloat("煙の重さ", &simulation.smokeWeight, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("周囲温度", &simulation.ambientTemperature, 0.01f, -20.0f, 20.0f);

		ImGui::SeparatorText("Grid再構築");
		int width = static_cast<int>(pendingGridWidth_);
		int height = static_cast<int>(pendingGridHeight_);
		int pressureIterations = static_cast<int>(pendingPressureIterations_);
		if (ImGui::InputInt("Grid幅", &width)) pendingGridWidth_ = static_cast<uint32_t>((std::max)(8, width));
		if (ImGui::InputInt("Grid高さ", &height)) pendingGridHeight_ = static_cast<uint32_t>((std::max)(8, height));
		ImGui::DragFloat("Cellサイズ", &pendingCellSize_, 0.005f, 0.001f, 10.0f);
		if (ImGui::SliderInt("圧力反復回数", &pressureIterations, 1, 200))
		{
			pendingPressureIterations_ = static_cast<uint32_t>((std::max)(1, pressureIterations));
		}
		if (ImGui::Button("Grid / 圧力設定を適用"))
		{
			manager->RequestGridReconfigure(
				pendingGridWidth_,
				pendingGridHeight_,
				pendingCellSize_,
				pendingPressureIterations_);
		}
		ImGui::SameLine();
		if (ImGui::Button("現在値を再読込"))
		{
			RefreshGridEditorValues();
		}

		const float widthWorld = static_cast<float>(simulation.grid.width) * simulation.grid.cellSize;
		const float heightWorld = static_cast<float>(simulation.grid.height) * simulation.grid.cellSize;
		ImGui::Text("現在のGrid: %ux%u | Cell %.4f | World %.2f x %.2f",
			simulation.grid.width,
			simulation.grid.height,
			simulation.grid.cellSize,
			widthWorld,
			heightWorld);
	}

	if (ImGui::CollapsingHeader("診断", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const double memoryMiB = static_cast<double>(stats.approximateGpuMemoryBytes) / (1024.0 * 1024.0);
		ImGui::Text("GPU流体論理使用量: %.2f MiB", memoryMiB);
		ImGui::Text("Scene Emitter: %u | Scene Obstacle: %u", stats.sceneEmitterCount, stats.sceneObstacleCount);
		ImGui::Text("負荷確認Emitter: %u | 負荷確認Obstacle: %u", stats.syntheticEmitterCount, stats.syntheticObstacleCount);
		ImGui::Text("累計シミュレーションステップ: %llu", static_cast<unsigned long long>(stats.totalSimulationSteps));
		ImGui::Text("リセット回数: %llu", static_cast<unsigned long long>(stats.resetCount));

		const FrameUploadArena::Stats uploadStats = DirectXCommon::GetInstance()->GetFrameUploadArena().GetStats();
		const double uploadUsedKiB = static_cast<double>(uploadStats.usedBytes) / 1024.0;
		const double uploadCapacityKiB = static_cast<double>(uploadStats.capacityBytes) / 1024.0;
		const double uploadHighWaterKiB = static_cast<double>(uploadStats.highWaterBytes) / 1024.0;
		const double uploadOverflowKiB = static_cast<double>(uploadStats.overflowBytes) / 1024.0;
		ImGui::SeparatorText("共有Frame Upload Arena");
		ImGui::Text("使用量 / 容量: %.1f / %.1f KiB", uploadUsedKiB, uploadCapacityKiB);
		ImGui::Text("最大使用量: %.1f KiB", uploadHighWaterKiB);
		ImGui::Text("Overflow: %.1f KiB | Allocation %zu", uploadOverflowKiB, uploadStats.overflowAllocationCount);

		ImGui::SeparatorText("累計GPU処理回数");
		ImGui::Text("障害物Raster Dispatch : %llu", static_cast<unsigned long long>(stats.obstacleDispatchCount));
		ImGui::Text("Emitter Injection      : %llu", static_cast<unsigned long long>(stats.emitterDispatchCount));
		ImGui::Text("速度Dispatch           : %llu", static_cast<unsigned long long>(stats.velocityDispatchCount));
		ImGui::Text("圧力Dispatch           : %llu", static_cast<unsigned long long>(stats.pressureDispatchCount));
		ImGui::Text("Scalar Dispatch         : %llu", static_cast<unsigned long long>(stats.scalarDispatchCount));
		ImGui::Text("Force Dispatch          : %llu", static_cast<unsigned long long>(stats.forceDispatchCount));
		ImGui::Text("Forward Draw            : %llu", static_cast<unsigned long long>(stats.forwardDrawCount));
	}

	if (ImGui::CollapsingHeader("負荷確認", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("プリセット: %s", GetStressPresetName(manager->GetStressPreset()));
		if (ImGui::Button("無効 / 手動設定"))
		{
			manager->ApplyStressPreset(GpuFluidStressPreset::Off);
		}
		ImGui::SameLine();
		if (ImGui::Button("中"))
		{
			manager->ApplyStressPreset(GpuFluidStressPreset::Medium);
			gridEditorValuesInitialized_ = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("高"))
		{
			manager->ApplyStressPreset(GpuFluidStressPreset::Heavy);
			gridEditorValuesInitialized_ = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("最大"))
		{
			manager->ApplyStressPreset(GpuFluidStressPreset::Extreme);
			gridEditorValuesInitialized_ = false;
		}

		int syntheticEmitters = static_cast<int>(manager->GetSyntheticEmitterCount());
		int syntheticObstacles = static_cast<int>(manager->GetSyntheticObstacleCount());
		bool changed = false;
		changed |= ImGui::SliderInt("負荷確認Emitter数", &syntheticEmitters, 0, 256);
		changed |= ImGui::SliderInt("負荷確認Obstacle数", &syntheticObstacles, 0, 256);
		if (changed)
		{
			manager->SetSyntheticStressCounts(
				static_cast<uint32_t>((std::max)(0, syntheticEmitters)),
				static_cast<uint32_t>((std::max)(0, syntheticObstacles)));
		}

		ImGui::TextDisabled("最大設定: 1024x1024 / 圧力反復80回 / Emitter 64個 / Obstacle 64個。");
		ImGui::TextDisabled("Gridサイズまたはプリセットを変更した場合だけ再構築待機が発生します。");
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
