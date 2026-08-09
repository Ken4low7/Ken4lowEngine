#include "OcclusionDebugController.h"

#include "OcclusionCullingSystem.h"
#include "Stage.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
#ifdef USE_IMGUI
	const char* ToReasonText(K4E::OcclusionCullingSystem::DebugFailReason reason)
	{
		switch (reason)
		{
		case K4E::OcclusionCullingSystem::DebugFailReason::None:
			return "判定成功 / Occluded";
		case K4E::OcclusionCullingSystem::DebugFailReason::CoverageInsufficient:
			return "coverage不足";
		case K4E::OcclusionCullingSystem::DebugFailReason::DepthInsufficient:
			return "depth条件不足";
		case K4E::OcclusionCullingSystem::DebugFailReason::FrustumOutside:
			return "Frustum外";
		case K4E::OcclusionCullingSystem::DebugFailReason::InFrontOfOccluder:
			return "Occluderより手前";
		case K4E::OcclusionCullingSystem::DebugFailReason::NoOccluder:
			return "有効なOccluderなし";
		default:
			return "不明";
		}
	}

	ImVec2 ToScreenPoint(const K4E::OcclusionCullingSystem::ScreenRect& rect, float x, float y)
	{
		(void)rect; // rectは将来の拡張で使うかもなので一応引数に残す
		// DirectX12のD3D12_VIEWPORTと区別するためImGui側のViewport名を明示する
		const ImGuiViewport* imguiMainViewport = ImGui::GetMainViewport();
		return ImVec2(
			imguiMainViewport->WorkPos.x + x * imguiMainViewport->WorkSize.x,
			imguiMainViewport->WorkPos.y + y * imguiMainViewport->WorkSize.y);
	}

	void DrawScreenRect(const K4E::OcclusionCullingSystem::ScreenRect& rect, ImU32 color, float thickness)
	{
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		drawList->AddRect(
			ToScreenPoint(rect, rect.minX, rect.minY),
			ToScreenPoint(rect, rect.maxX, rect.maxY),
			color,
			0.0f,
			0,
			thickness);
	}

	void DrawDebugScreenRects(const K4E::OcclusionCullingSystem& occlusionSystem)
	{
		if (occlusionSystem.IsShowOccluderScreenRects())
		{
			for (const auto& occluderDebug : occlusionSystem.GetOccluderDebugInfos())
			{
				if (!occluderDebug.hasRect) { continue; }
				DrawScreenRect(occluderDebug.rect, IM_COL32(255, 180, 16, 230), 2.0f);
			}
		}

		if (occlusionSystem.IsShowChunkScreenRects())
		{
			for (const auto& chunkDebug : occlusionSystem.GetChunkDebugInfos())
			{
				if (!chunkDebug.hasRect) { continue; }

				ImU32 color = IM_COL32(64, 220, 96, 180);
				float thickness = 1.0f;
				if (chunkDebug.occluded)
				{
					color = IM_COL32(190, 32, 255, 230);
					thickness = 2.5f;
				}
				else if (chunkDebug.failReason == K4E::OcclusionCullingSystem::DebugFailReason::DepthInsufficient)
				{
					color = IM_COL32(32, 128, 255, 220);
					thickness = 2.0f;
				}
				else if (chunkDebug.failReason == K4E::OcclusionCullingSystem::DebugFailReason::InFrontOfOccluder)
				{
					color = IM_COL32(255, 128, 16, 220);
					thickness = 2.0f;
				}

				DrawScreenRect(chunkDebug.rect, color, thickness);
			}
		}
	}
#endif
}

/// -------------------------------------------------------------
///							ImGui描画処理
/// -------------------------------------------------------------
void OcclusionDebugController::DrawImGui(K4E::Stage* stage)
{
#ifdef USE_IMGUI
	// 単体表示時もDocking可能な通常ウィンドウとして開く。
	if (ImGui::Begin("オクルージョンカリングデバッグ"))
	{
		DrawImGuiContent(stage);
	}
	ImGui::End();
#else
	(void)stage;
#endif
}

void OcclusionDebugController::DrawImGuiContent(K4E::Stage* stage)
{
#ifdef USE_IMGUI
	if (!stage) { return; }

	auto& occlusionSystem = stage->GetOcclusionCullingSystem();
	bool enabled = occlusionSystem.IsEnabled();
	bool showOccluderBounds = occlusionSystem.IsShowOccluderBounds();
	bool showOccludedBounds = occlusionSystem.IsShowOccludedBounds();
	bool showOccluderScreenRects = occlusionSystem.IsShowOccluderScreenRects();
	bool showChunkScreenRects = occlusionSystem.IsShowChunkScreenRects();
	float coverageThreshold = occlusionSystem.GetCoverageThreshold();
	float depthBias = occlusionSystem.GetDepthBias();
	float occlusionMargin = occlusionSystem.GetOcclusionMargin();
	bool conservativeMode = occlusionSystem.IsConservativeMode();
	int selectedChunkId = occlusionSystem.GetDebugSelectedChunkId();

	DrawDebugScreenRects(occlusionSystem);

	if (ImGui::Checkbox("Occlusion Culling 有効", &enabled))
	{
		occlusionSystem.SetEnabled(enabled);
	}
	if (ImGui::Checkbox("Occluder Bounds表示", &showOccluderBounds))
	{
		occlusionSystem.SetShowOccluderBounds(showOccluderBounds);
	}
	if (ImGui::Checkbox("Occluded Bounds表示", &showOccludedBounds))
	{
		occlusionSystem.SetShowOccludedBounds(showOccludedBounds);
	}
	if (ImGui::Checkbox("Occluderスクリーン矩形表示", &showOccluderScreenRects))
	{
		occlusionSystem.SetShowOccluderScreenRects(showOccluderScreenRects);
	}
	if (ImGui::Checkbox("Chunkスクリーン矩形表示", &showChunkScreenRects))
	{
		occlusionSystem.SetShowChunkScreenRects(showChunkScreenRects);
	}

	ImGui::SeparatorText("判定パラメータ（推奨値調整）");
	if (ImGui::SliderFloat("coverageThreshold", &coverageThreshold, 0.50f, 1.0f, "%.3f"))
	{
		occlusionSystem.SetCoverageThreshold(coverageThreshold);
	}
	ImGui::SameLine();
	if (ImGui::Button("coverage 推奨 0.98"))
	{
		occlusionSystem.SetCoverageThreshold(0.98f);
	}
	if (ImGui::DragFloat("depthBias", &depthBias, 0.001f, 0.0f, 0.20f, "%.3f"))
	{
		occlusionSystem.SetDepthBias(depthBias);
	}
	ImGui::SameLine();
	if (ImGui::Button("depth 推奨 0.020"))
	{
		occlusionSystem.SetDepthBias(0.020f);
	}
	if (ImGui::DragFloat("occlusionMargin", &occlusionMargin, 0.001f, 0.0f, 0.10f, "%.3f"))
	{
		occlusionSystem.SetOcclusionMargin(occlusionMargin);
	}
	ImGui::SameLine();
	if (ImGui::Button("margin 推奨 0.020"))
	{
		occlusionSystem.SetOcclusionMargin(0.020f);
	}
	if (ImGui::Checkbox("conservativeMode（Occluder最奥Depthで安全判定）", &conservativeMode))
	{
		occlusionSystem.SetConservativeMode(conservativeMode);
	}
	ImGui::TextWrapped("推奨値は安全確認用の出発点です。見えている物を消す場合は coverageThreshold / depthBias / occlusionMargin を大きくするか conservativeMode を有効にしてください。");

	const auto& stats = occlusionSystem.GetStatistics();
	ImGui::SeparatorText("統計 / 失敗理由");
	ImGui::Text("Occluder数: %d", stats.occluderCount);
	ImGui::Text("Occlusion判定対象Chunk数: %d", stats.testedChunkCount);
	ImGui::Text("OcclusionでカリングされたChunk数: %d", stats.occludedChunkCount);
	ImGui::Text("coverage不足: %d", stats.coverageFailedCount);
	ImGui::Text("depth条件不足: %d", stats.depthFailedCount);
	ImGui::Text("Frustum外: %d", stats.frustumOutsideCount);
	ImGui::Text("Occluderより手前: %d", stats.inFrontOfOccluderCount);

	ImGui::SeparatorText("選択中Chunk");
	if (ImGui::DragInt("Chunk ID", &selectedChunkId, 1.0f, 0, 100000))
	{
		occlusionSystem.SetDebugSelectedChunkId(selectedChunkId);
	}
	if (const auto* selectedDebug = occlusionSystem.GetSelectedChunkDebugInfo())
	{
		ImGui::Text("選択中Chunkの最手前Depth: %.6f", selectedDebug->hasRect ? selectedDebug->rect.minDepth : 0.0f);
		ImGui::Text("選択中Chunkの最奥Depth: %.6f", selectedDebug->hasRect ? selectedDebug->rect.maxDepth : 0.0f);
		ImGui::Text("選択中Chunkの平均Depth(表示用): %.6f", selectedDebug->hasRect ? selectedDebug->rect.averageDepth : 0.0f);
		ImGui::Text("対応Occluderの最手前Depth: %.6f", selectedDebug->hasMatchedOccluder ? selectedDebug->matchedOccluderRect.minDepth : 0.0f);
		ImGui::Text("対応Occluderの最奥Depth: %.6f", selectedDebug->hasMatchedOccluder ? selectedDebug->matchedOccluderRect.maxDepth : 0.0f);
		ImGui::Text("対応Occluderの平均Depth(表示用): %.6f", selectedDebug->hasMatchedOccluder ? selectedDebug->matchedOccluderRect.averageDepth : 0.0f);
		ImGui::Text("Depth差: %.6f / bias %.6f", selectedDebug->bestDepthDelta, occlusionSystem.GetDepthBias());
		ImGui::Text("Depth判定結果: %s", selectedDebug->depthPassed ? "OK（Occluderより奥）" : "NG（手前または差不足）");
		ImGui::Text("Coverage判定結果: %s (%.3f / threshold %.3f)", selectedDebug->coveragePassed ? "OK" : "NG", selectedDebug->bestCoverage, occlusionSystem.GetCoverageThreshold());
		ImGui::Text("最終Occlusion判定結果: %s", selectedDebug->occluded ? "Occluded(Draw Skip)" : "Visible(Draw)");
		ImGui::Text("理由: %s", ToReasonText(selectedDebug->failReason));
		if (selectedDebug->hasRect)
		{
			ImGui::Text("screenRect: (%.3f, %.3f) - (%.3f, %.3f)",
				selectedDebug->rect.minX,
				selectedDebug->rect.minY,
				selectedDebug->rect.maxX,
				selectedDebug->rect.maxY);
		}
		else
		{
			ImGui::Text("screenRect: 未生成");
		}
	}
	else
	{
		ImGui::Text("指定IDのChunk Debug情報がありません。");
	}

	ImGui::Separator();
	ImGui::TextWrapped("StageChunk Culling 後の Chunk Bounds をスクリーン矩形に投影し、Occluder に十分覆われ、かつ奥にある場合だけ Draw をスキップします。");
	ImGui::TextWrapped("Frustum Culling のカリング Chunk は赤、Occlusion Culling のカリング Chunk は紫、depth条件不足は青、Occluderより手前は橙、Occluder は黄で表示します。Update / Collision は止めず Draw だけをスキップします。");
#else
	(void)stage;
#endif
}
