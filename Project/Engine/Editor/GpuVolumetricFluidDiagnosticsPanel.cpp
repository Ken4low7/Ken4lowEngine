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
		case GpuVolumetricFluidStressPreset::Baseline64: return "標準 64^3";
		case GpuVolumetricFluidStressPreset::Heavy128: return "高負荷 128^3";
		case GpuVolumetricFluidStressPreset::Off:
		default: return "無効 / 手動設定";
		}
	}

	const char* GetRenderModeName(GpuVolumetricFluidRenderMode mode)
	{
		switch (mode)
		{
		case GpuVolumetricFluidRenderMode::DensityDebug: return "密度確認";
		case GpuVolumetricFluidRenderMode::TemperatureDebug: return "温度確認";
		case GpuVolumetricFluidRenderMode::ObstacleDebug: return "障害物確認";
		case GpuVolumetricFluidRenderMode::Smoke:
		default: return "煙";
		}
	}

	void DrawSectionLabel(const char* label)
	{
		ImGui::Separator();
		ImGui::TextUnformatted(label); // 基本APIだけでSection見出しを描画し、ImGuiバージョン依存を増やさない。
	}
#endif // USE_IMGUI
}

void GpuVolumetricFluidDiagnosticsPanel::Draw()
{
#ifdef USE_IMGUI
	if (ImGui::IsKeyPressed(ImGuiKey_F8, false))
	{
		visible_ = !visible_; // 2D流体診断とは別に3Dボリューム流体をF8で開閉する。
	}
	if (!visible_)
	{
		return;
	}

	GpuVolumetricFluidManager* manager = GpuVolumetricFluidManager::GetInstance();
	if (!ImGui::Begin("3Dボリューム流体診断", &visible_))
	{
		ImGui::End();
		return;
	}

	ImGui::TextDisabled("F8: 3D流体パネルの表示切替 | F12: 2D流体パネルの表示切替");
	if (!gridEditorValuesInitialized_)
	{
		RefreshGridEditorValues();
	}

	GpuVolumetricFluidSimulationDesc& simulation = manager->GetEditableSimulationDesc();
	GpuVolumetricFluidDomainMapping& domain = manager->GetEditableDomainMapping();
	GpuVolumetricFluidRenderDesc& render = manager->GetEditableRenderDesc();
	const GpuVolumetricFluidRuntimeStats& stats = manager->GetRuntimeStats();

	if (ImGui::CollapsingHeader("実行制御", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool enabled = manager->IsRuntimeEnabled();
		if (ImGui::Checkbox("3D流体を有効化", &enabled))
		{
			manager->SetRuntimeEnabled(enabled);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("既存Sceneへ影響しないよう初期状態は無効です。");

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
		if (ImGui::Checkbox("Raymarch描画を有効化", &renderEnabled))
		{
			manager->SetRenderEnabled(renderEnabled);
		}

		ImGui::Text("GPUリソース: %s", manager->IsInitialized() ? "確保済み" : "未確保");
		ImGui::Text("シミュレーション: %s", stats.simulationActive ? "実行中" : "待機中");
		ImGui::Text("直前のステップ: %s", stats.lastStepSucceeded ? "成功" : "失敗");
		ImGui::Text("このフレームのSubstep: %u", stats.lastFrameSubsteps);
		ImGui::Text("時間蓄積: %.4f 秒", stats.accumulatorSeconds);
		ImGui::Text("シミュレーション時間: %.2f 秒", stats.elapsedSimulationSeconds);
	}

	if (ImGui::CollapsingHeader("描画 / ライティング", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int mode = static_cast<int>(render.mode);
		const char* modes[] = { "煙", "密度確認", "温度確認", "障害物確認" };
		if (ImGui::Combo("表示モード", &mode, modes, IM_ARRAYSIZE(modes)))
		{
			render.mode = static_cast<GpuVolumetricFluidRenderMode>(std::clamp(mode, 0, 3));
		}
		ImGui::TextDisabled("現在: %s", GetRenderModeName(render.mode));
		ImGui::SliderFloat("不透明度", &render.opacity, 0.0f, 1.0f);
		ImGui::DragFloat("密度倍率", &render.densityScale, 0.05f, 0.0f, 100.0f);
		ImGui::DragFloat("温度倍率", &render.temperatureScale, 0.05f, 0.001f, 100.0f);
		ImGui::DragFloat("光の吸収", &render.absorption, 0.05f, 0.0f, 50.0f);
		ImGui::DragFloat("熱発光", &render.emissionStrength, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("Rayステップ倍率", &render.stepScale, 0.02f, 0.1f, 8.0f);
		ImGui::SliderFloat("早期終了透過率", &render.earlyExitTransmittance, 0.0f, 0.25f, "%.4f");
		int maxSteps = static_cast<int>(render.maxSteps);
		if (ImGui::SliderInt("最大Rayステップ数", &maxSteps, 16, 1024))
		{
			render.maxSteps = static_cast<uint32_t>(std::clamp(maxSteps, 16, 1024));
		}

		DrawSectionLabel("指向性散乱");
		ImGui::DragFloat("散乱の強さ", &render.scatteringStrength, 0.02f, 0.0f, 10.0f);
		ImGui::DragFloat("環境散乱", &render.ambientScattering, 0.01f, 0.0f, 5.0f);
		ImGui::SliderFloat("異方性", &render.anisotropy, -0.94f, 0.94f);
		ImGui::SliderFloat("自己影の強さ", &render.selfShadowStrength, 0.0f, 1.0f);
		ImGui::DragFloat("影サンプル距離（Cell）", &render.shadowSampleDistanceCells, 0.1f, 0.1f, 32.0f);
		ImGui::ColorEdit4("煙の色", &render.smokeColor.x);
		ImGui::ColorEdit4("低温色", &render.coldColor.x);
		ImGui::ColorEdit4("高温色", &render.hotColor.x);
		ImGui::ColorEdit4("障害物色", &render.obstacleColor.x);
	}

	if (ImGui::CollapsingHeader("領域 / Solver", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool domainChanged = false;
		domainChanged |= ImGui::DragFloat3("領域原点", &domain.origin.x, 0.05f);
		domainChanged |= ImGui::DragFloat3("領域軸 U", &domain.axisU.x, 0.01f, -1.0f, 1.0f);
		domainChanged |= ImGui::DragFloat3("領域軸 V", &domain.axisV.x, 0.01f, -1.0f, 1.0f);
		domainChanged |= ImGui::DragFloat3("領域軸 W", &domain.axisW.x, 0.01f, -1.0f, 1.0f);
		if (!domain.IsValid())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "U/V/W軸はゼロベクトルにせず、互いに直交させてください。");
		}
		else if (domainChanged)
		{
			manager->RequestReset(); // 座標系変更後に旧Gridの値を新しい領域へ誤解釈しないよう再初期化する。
		}

		DrawSectionLabel("固定ステップ");
		ImGui::DragFloat("固定デルタタイム", &simulation.fixedDeltaTime, 0.0005f, 1.0f / 240.0f, 1.0f / 15.0f, "%.5f");
		int maxSubsteps = static_cast<int>(simulation.maxSubsteps);
		if (ImGui::SliderInt("最大Substep数", &maxSubsteps, 1, 8))
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

		DrawSectionLabel("Grid再構築");
		int width = static_cast<int>(pendingGridWidth_);
		int height = static_cast<int>(pendingGridHeight_);
		int depth = static_cast<int>(pendingGridDepth_);
		int pressureIterations = static_cast<int>(pendingPressureIterations_);
		if (ImGui::InputInt("Grid幅", &width)) pendingGridWidth_ = static_cast<uint32_t>(std::clamp(width, 8, 256));
		if (ImGui::InputInt("Grid高さ", &height)) pendingGridHeight_ = static_cast<uint32_t>(std::clamp(height, 8, 256));
		if (ImGui::InputInt("Grid奥行き", &depth)) pendingGridDepth_ = static_cast<uint32_t>(std::clamp(depth, 8, 256));
		ImGui::DragFloat("Cellサイズ", &pendingCellSize_, 0.005f, 0.001f, 10.0f);
		if (ImGui::SliderInt("圧力反復回数", &pressureIterations, 1, 192))
		{
			pendingPressureIterations_ = static_cast<uint32_t>((std::max)(1, pressureIterations));
		}
		if (ImGui::Button("Grid / 圧力設定を適用"))
		{
			manager->RequestGridReconfigure(
				pendingGridWidth_,
				pendingGridHeight_,
				pendingGridDepth_,
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
		const float depthWorld = static_cast<float>(simulation.grid.depth) * simulation.grid.cellSize;
		ImGui::Text("現在のGrid: %ux%ux%u | Cell %.4f | World %.2f x %.2f x %.2f",
			simulation.grid.width,
			simulation.grid.height,
			simulation.grid.depth,
			simulation.grid.cellSize,
			widthWorld,
			heightWorld,
			depthWorld);
	}

	if (ImGui::CollapsingHeader("診断", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const uint64_t configuredBytes = simulation.grid.GetVoxelCount() * 39ull;
		const double activeMemoryMiB = static_cast<double>(stats.approximateGpuMemoryBytes) / (1024.0 * 1024.0);
		const double configuredMemoryMiB = static_cast<double>(configuredBytes) / (1024.0 * 1024.0);
		ImGui::Text("Texture3D論理使用量: %.2f MiB 使用中 | %.2f MiB 設定値", activeMemoryMiB, configuredMemoryMiB);
		ImGui::Text("Scene Emitter: %u | Scene Obstacle: %u", stats.sceneEmitterCount, stats.sceneObstacleCount);
		ImGui::Text("負荷確認Emitter: %u | 負荷確認Obstacle: %u", stats.syntheticEmitterCount, stats.syntheticObstacleCount);
		ImGui::Text("Emitter Upload: %u 採用 | %u 除外", stats.lastInjectedEmitterCount, stats.lastCulledEmitterCount);
		ImGui::Text("Obstacle Raster: %u 採用 | %u 除外", stats.lastRasterObstacleCount, stats.lastCulledObstacleCount);
		ImGui::Text("1回のProjectionに使う圧力反復: %u", stats.lastPressureIterationCount);
		ImGui::Text("累計シミュレーションステップ: %llu", static_cast<unsigned long long>(stats.totalSimulationSteps));
		ImGui::Text("リセット / 再構築 / 失敗: %llu / %llu / %llu",
			static_cast<unsigned long long>(stats.resetCount),
			static_cast<unsigned long long>(stats.reconfigureCount),
			static_cast<unsigned long long>(stats.failedReconfigureCount));
		ImGui::Text("重複フレーム更新スキップ: %llu", static_cast<unsigned long long>(stats.duplicateFrameUpdateSkipCount));

		DrawSectionLabel("累計GPU処理回数");
		ImGui::Text("障害物Raster Dispatch : %llu", static_cast<unsigned long long>(stats.obstacleDispatchCount));
		ImGui::Text("Emitter Injection      : %llu", static_cast<unsigned long long>(stats.emitterDispatchCount));
		ImGui::Text("速度Dispatch           : %llu", static_cast<unsigned long long>(stats.velocityDispatchCount));
		ImGui::Text("圧力Dispatch           : %llu", static_cast<unsigned long long>(stats.pressureDispatchCount));
		ImGui::Text("Scalar Dispatch         : %llu", static_cast<unsigned long long>(stats.scalarDispatchCount));
		ImGui::Text("Force Dispatch          : %llu", static_cast<unsigned long long>(stats.forceDispatchCount));
		ImGui::Text("Raymarch Draw / Packet  : %llu / %llu",
			static_cast<unsigned long long>(stats.forwardDrawCount),
			static_cast<unsigned long long>(stats.forwardPacketCount));

		const RenderDepthContextStats& depthStats = RenderDepthContext::GetInstance()->GetStats();
		DrawSectionLabel("深度を使った合成");
		ImGui::Text("Attachment: %u | Shader読取準備: %s", depthStats.attachmentCount, depthStats.shaderReadPrepared ? "はい" : "いいえ");
		ImGui::Text("準備 / 復元 / 失敗: %llu / %llu / %llu",
			static_cast<unsigned long long>(depthStats.prepareCount),
			static_cast<unsigned long long>(depthStats.restoreCount),
			static_cast<unsigned long long>(depthStats.failedPrepareCount));
		ImGui::Text("Depth Override Push: %llu", static_cast<unsigned long long>(depthStats.overridePushCount));
		ImGui::TextDisabled("登録済みDepthがないRenderViewOverrideではVolume描画を安全にスキップします。");

		const SRVManager::DescriptorStats descriptorStats = SRVManager::GetInstance()->GetDescriptorStats();
		DrawSectionLabel("共有SRV Descriptor Heap");
		ImGui::Text("Persistent: %u / %u | 最大 %u",
			descriptorStats.persistentInUse,
			descriptorStats.persistentCapacity,
			descriptorStats.persistentHighWater);
		ImGui::Text("Transient: %u / %u | 最大 %u",
			descriptorStats.transientInUse,
			descriptorStats.transientCapacity,
			descriptorStats.transientHighWater);
		ImGui::Text("Descriptor枯渇回数: %llu", static_cast<unsigned long long>(descriptorStats.exhaustionCount));

		const FrameUploadArena::Stats uploadStats = DirectXCommon::GetInstance()->GetFrameUploadArena().GetStats();
		DrawSectionLabel("共有Frame Upload Arena");
		ImGui::Text("使用量 / 容量: %.1f / %.1f KiB",
			static_cast<double>(uploadStats.usedBytes) / 1024.0,
			static_cast<double>(uploadStats.capacityBytes) / 1024.0);
		ImGui::Text("最大使用量: %.1f KiB | Overflow %.1f KiB",
			static_cast<double>(uploadStats.highWaterBytes) / 1024.0,
			static_cast<double>(uploadStats.overflowBytes) / 1024.0);
	}

	if (ImGui::CollapsingHeader("負荷確認", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("プリセット: %s", GetStressPresetName(manager->GetStressPreset()));
		if (ImGui::Button("無効 / 手動設定"))
		{
			manager->ApplyStressPreset(GpuVolumetricFluidStressPreset::Off);
		}
		ImGui::SameLine();
		if (ImGui::Button("標準 64^3"))
		{
			manager->ApplyStressPreset(GpuVolumetricFluidStressPreset::Baseline64);
			gridEditorValuesInitialized_ = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("高負荷 128^3"))
		{
			manager->ApplyStressPreset(GpuVolumetricFluidStressPreset::Heavy128);
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

		ImGui::TextDisabled("64^3は論理フィールド約9.75 MiB、128^3はDriver側の追加領域を除いて約78 MiBです。");
		ImGui::TextDisabled("Grid変更はUpdateへ遅延し、Texture3D再生成前にGPU使用完了を保証します。");
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
