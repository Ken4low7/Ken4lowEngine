#pragma once

#include "FrustumCullingDebugController.h"
#include "GamePlayDebugTools.h"
#include "GamePlayWorld.h"
#include "../Effects/GamePlayEffectController.h"

#include <Editor/EditorPlayController.h>
#include <Editor/EditorWindowManager.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

/// <summary>
/// GamePlayScene の Debug / ImGui ウィンドウを集約する Controller です。<br/>
/// Scene 本体は DebugTools や各 Controller を所有し、このクラスへ表示順だけ委譲します。
/// </summary>
class GamePlayDebugWindow
{
public:
	/// <summary>
	/// Gameplay Debug、Culling Debug、Player Debug の ImGui 表示を既存ウィンドウ構成のまま描画します。
	/// </summary>
	void DrawImGui(
		GamePlayWorld* world,
		GamePlayDebugTools* debugTools,
		FrustumCullingDebugController* frustumCullingDebug,
		GamePlayEffectController* effectController)
	{
#ifdef USE_IMGUI
		if (debugTools)
		{
			debugTools->DrawImGui(world);
		}

		auto& editorWindowState = Ken4lowEngine::EditorWindowManager::GetInstance()->GetWindowState();
		if (world)
		{
			auto& actorWorld = world->GetCharacters().GetActorWorld();
			actorWorld.SetLegacyEditorWindowsEnabled(editorWindowState.showPlayerDebug);
			if (!Ken4lowEngine::EditorPlayController::GetInstance()->IsPlaying()) actorWorld.UpdateEditor(0.0f); // Edit/Pause中だけTransformとCollider表示を同期する。
			if (editorWindowState.showPlayerDebug) actorWorld.DrawImGui(); // GamePlayの実Player Actorと全Componentを編集・Prefab保存できるよう公開する。
		}
		DrawCullingDebugWindow(world, debugTools, frustumCullingDebug, editorWindowState);
		DrawPlayerDebugWindow(effectController, editorWindowState);
#else
		(void)world;
		(void)debugTools;
		(void)frustumCullingDebug;
		(void)effectController;
#endif
	}

private:
#ifdef USE_IMGUI
	void DrawCullingDebugWindow(
		GamePlayWorld* world,
		GamePlayDebugTools* debugTools,
		FrustumCullingDebugController* frustumCullingDebug,
		Ken4lowEngine::EditorWindowState& editorWindowState)
	{
		if (!editorWindowState.showCullingDebug)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(520.0f, 520.0f), ImGuiCond_FirstUseEver);
		// Culling Debug は StageChunk / Occlusion / Frustum を 1 つの Docking 対象ウィンドウへ集約する。
		if (ImGui::Begin("Culling Debug", &editorWindowState.showCullingDebug))
		{
			if (debugTools)
			{
				debugTools->DrawCullingDebugContent(world);
			}
			if (frustumCullingDebug && ImGui::CollapsingHeader("Frustum Culling Debug", ImGuiTreeNodeFlags_DefaultOpen))
			{
				frustumCullingDebug->DrawImGuiContent();
			}
		}
		ImGui::End();
	}

	void DrawPlayerDebugWindow(GamePlayEffectController* effectController, Ken4lowEngine::EditorWindowState& editorWindowState)
	{
		if (!(effectController && effectController->HasPlayerDebugContent() && editorWindowState.showPlayerDebug))
		{
			return;
		}

		// Player Debug へ HP / Damage 系ポストエフェクト調整を追加し、単独浮遊ウィンドウを出さない。
		if (ImGui::Begin("Player Debug", &editorWindowState.showPlayerDebug))
		{
			ImGui::SeparatorText("HP / Damage");
			effectController->DrawPlayerDebugContent();
		}
		ImGui::End();
	}
#endif
};
