#define NOMINMAX
#include "EditorWindowManager.h"

#include "EditorPlayController.h"
#include "EditorSelectionOutlineManager.h"
#include "ImGuiManager.h"
#include "PostEffectManager.h"
#include "GameViewportConstants.h"
#include <Input.h>
#include <LightManager.h>
#include <SceneManager.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <filesystem>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

#ifdef _WIN32
#include <shellapi.h>
#include <GameTimer.h>
#endif // _WIN32

namespace Ken4lowEngine
{

	namespace
	{
		std::string ToUtf8Path(const std::filesystem::path& path)
		{
			return path.generic_string();
		}

		EditorInputPolicy GetCurrentEditorInputPolicy(SceneManager* sceneManager)
		{
			BaseScene* scene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
			// Scene未設定時はUI Mouse扱いにしてEditor操作を妨げない。
			return scene ? scene->GetEditorInputPolicy() : EditorInputPolicy::UiMouse;
		}

		bool IsFpsCapturePolicy(SceneManager* sceneManager)
		{
			// F8キャプチャはFPS操作Sceneだけで有効にする。
			return GetCurrentEditorInputPolicy(sceneManager) == EditorInputPolicy::FpsCapture;
		}

		std::string FormatBytes(uintmax_t bytes)
		{
			std::ostringstream stream;
			if (bytes >= 1024ull * 1024ull)
			{
				stream << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
			}
			else if (bytes >= 1024ull)
			{
				stream << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / 1024.0) << " KB";
			}
			else
			{
				stream << bytes << " bytes";
			}
			return stream.str();
		}

#ifdef USE_IMGUI
		void DrawDetailRow(const char* label, const std::string& value)
		{
			ImGui::TextDisabled("%s", label);
			ImGui::SameLine(115.0f);
			ImGui::TextWrapped("%s", value.empty() ? "(none)" : value.c_str());
		}

		std::string TruncateTextToWidth(const std::string& text, float maxWidth)
		{
			if (maxWidth <= 0.0f || ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
			{
				return text;
			}

			const char* ellipsis = "...";
			const float ellipsisWidth = ImGui::CalcTextSize(ellipsis).x;
			std::string result;
			for (char c : text)
			{
				std::string candidate = result + c;
				if (ImGui::CalcTextSize(candidate.c_str()).x + ellipsisWidth > maxWidth)
				{
					break;
				}
				result = std::move(candidate);
			}
			return result.empty() ? ellipsis : (result + ellipsis);
		}

		void DrawPerformanceCompactLine(const PerformanceStats& stats, EditorWindowManager::OutputLogPerformanceDisplayMode mode)
		{
			if (mode == EditorWindowManager::OutputLogPerformanceDisplayMode::FPS)
			{
				ImGui::Text("Instant FPS: %.1f | Average FPS: %.1f | FrameTime: %.2fms | VSync: ON | CPU: %.1f%% | Proc: %.1f%% | Mem: %.1fMB",
					stats.instantFps, stats.fps, stats.frameTimeMs, stats.cpuUsagePercent, stats.processCpuUsagePercent, stats.memoryUsageMB);
			}
			else
			{
				ImGui::Text("FrameTime: %.2fms | Instant FPS: %.1f | Average FPS: %.1f | VSync: ON | CPU: %.1f%% | Proc: %.1f%% | Mem: %.1fMB",
					stats.frameTimeMs, stats.instantFps, stats.fps, stats.cpuUsagePercent, stats.processCpuUsagePercent, stats.memoryUsageMB);
			}
		}

		ImVec2 FitImageSize(uint32_t width, uint32_t height, const ImVec2& bounds)
		{
			if (width == 0 || height == 0 || bounds.x <= 0.0f || bounds.y <= 0.0f)
			{
				return ImVec2(0.0f, 0.0f);
			}
			const float scale = std::min(bounds.x / static_cast<float>(width), bounds.y / static_cast<float>(height));
			return ImVec2(std::floor(static_cast<float>(width) * scale), std::floor(static_cast<float>(height) * scale));
		}

		void DrawCheckerBackground(ImDrawList* drawList, const ImVec2& min, const ImVec2& max)
		{
			const float square = 8.0f;
			const ImU32 a = ImGui::GetColorU32(ImVec4(0.26f, 0.26f, 0.26f, 1.0f));
			const ImU32 b = ImGui::GetColorU32(ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
			for (float y = min.y; y < max.y; y += square)
			{
				for (float x = min.x; x < max.x; x += square)
				{
					const bool even = (static_cast<int>((x - min.x) / square) + static_cast<int>((y - min.y) / square)) % 2 == 0;
					drawList->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + square, max.x), std::min(y + square, max.y)), even ? a : b);
				}
			}
		}
#endif // USE_IMGUI

		std::string GetParentPathText(const EditorAssetEntry& entry)
		{
			const std::filesystem::path relativePath(entry.relativePath);
			const std::filesystem::path parent = relativePath.parent_path();
			return parent.empty() ? "." : parent.generic_string();
		}
	}

	EditorWindowManager* EditorWindowManager::GetInstance()
	{
		static EditorWindowManager instance;
		return &instance;
	}

	void EditorWindowManager::Draw()
	{
#ifdef USE_IMGUI
		InitializeEditorServices();
		const bool buildWasRunning = assetBuildService_.IsRunning();
		assetBuildService_.Update();
		if (buildWasRunning && !assetBuildService_.IsRunning() && assetBuildService_.WasLastBuildSuccessful())
		{
			// Build完了後はCompiled側の変換結果を確認しやすいようContent Browserを再スキャンする。
			assetBrowser_.Refresh();
		}
#ifdef _WIN32
		const auto* gameTimer = GameTimer::GetInstance();
		outputLogPerformanceMonitor_.Update(gameTimer->GetDeltaTime(), gameTimer->GetFPS());
#endif // _WIN32

		auto* input = Input::GetInstance();
		if (input->TriggerRawKey(DIK_F8) && IsFpsCapturePolicy(sceneManager_))
		{
			const bool forceRelease = input->PushRawKey(DIK_LSHIFT) || input->PushRawKey(DIK_RSHIFT);
			// F8はFPS操作Sceneだけ入力キャプチャ切替、Shift+F8は非常口にする。
			if (forceRelease)
			{
				EditorPlayController::GetInstance()->ForceReleaseToEditor();
			}
			else
			{
				EditorPlayController::GetInstance()->ToggleInputCapture();
			}
		}

		// UE5風エディタUIの描画順をManagerに集約する
		DrawMenuBar();
		if (windowState_.showToolbar)
		{
			DrawToolbar();
		}
		DrawMainViewport();
		DrawWorldOutliner();
		DrawDetails();
		DrawContentBrowser();
		DrawOutputLog();
		DrawScene();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawMenuBar()
	{
#ifdef USE_IMGUI
		// メインメニューは各エディタパネルの表示状態を切り替える入口にする
		if (!ImGui::BeginMainMenuBar())
		{
			return;
		}

		if (ImGui::BeginMenu("File"))
		{
			ImGui::MenuItem("New Level");
			ImGui::MenuItem("Open Level");
			ImGui::MenuItem("Save Level");
			ImGui::Separator();
			ImGui::MenuItem("Exit");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			// Commonは常時使うエディタ基本パネルをまとめて表示切替できるようにする
			if (ImGui::BeginMenu("Common"))
			{
				ImGui::MenuItem("Main Viewport", nullptr, &windowState_.showMainViewport);
				ImGui::MenuItem("World Outliner", nullptr, &windowState_.showWorldOutliner);
				ImGui::MenuItem("Details", nullptr, &windowState_.showDetails);
				ImGui::MenuItem("Content Browser", nullptr, &windowState_.showContentBrowser);
				ImGui::MenuItem("Output Log", nullptr, &windowState_.showOutputLog);
				ImGui::MenuItem("Scene", nullptr, &windowState_.showScene);
				ImGui::MenuItem("Toolbar", nullptr, &windowState_.showToolbar);
				ImGui::EndMenu();
			}

			// Renderingは描画・画面・ライト調整系ウィンドウをまとめて表示切替できるようにする
			if (ImGui::BeginMenu("Rendering"))
			{
				ImGui::MenuItem("Light Editor", nullptr, &windowState_.showLightEditor);
				ImGui::MenuItem("Post Effect Settings", nullptr, &windowState_.showPostEffectSettings);
				ImGui::MenuItem("Display", nullptr, &windowState_.showDisplay);
				ImGui::MenuItem("Parameters", nullptr, &windowState_.showParameters);
				ImGui::MenuItem("Json Asset Manager", nullptr, &windowState_.showJsonAssetManager);
				ImGui::EndMenu();
			}

			// Scene Debugは現在のシーンに応じて描かれるデバッグウィンドウをまとめて表示切替できるようにする
			if (ImGui::BeginMenu("Scene Debug"))
			{
				ImGui::MenuItem("Title Debug", nullptr, &windowState_.showTitleDebug);
				ImGui::MenuItem("Stage Select Debug", nullptr, &windowState_.showStageSelectDebug);
				ImGui::MenuItem("Game Debug", nullptr, &windowState_.showGameDebug);
				ImGui::MenuItem("Player Debug", nullptr, &windowState_.showPlayerDebug);
				ImGui::MenuItem("Weapon Debug", nullptr, &windowState_.showWeaponDebug);
				ImGui::MenuItem("Enemy Debug", nullptr, &windowState_.showEnemyDebug);
				ImGui::MenuItem("Collision Debug", nullptr, &windowState_.showCollisionDebug);
				ImGui::MenuItem("Culling Debug", nullptr, &windowState_.showCullingDebug);
				ImGui::MenuItem("FadeManager", nullptr, &windowState_.showFadeManager);
				ImGui::EndMenu();
			}

			// Layout操作はDock再構築と簡易Focusレイアウト入口を明示的に分ける。
			if (ImGui::BeginMenu("Layout"))
			{
				if (ImGui::MenuItem("Rebuild Default Layout"))
				{
					// Rebuild Default Layoutは誤操作を避けるためPopupでOK確認してから実行する。
					openRebuildDefaultLayoutPopup_ = true;
				}
				if (ImGui::MenuItem("Game Focus Layout"))
				{
					// Game Focus LayoutはDock再配置せず周辺ウィンドウだけ一時的に非表示にする。
					windowState_.showMainViewport = true;
					windowState_.showToolbar = false;
					windowState_.showWorldOutliner = false;
					windowState_.showDetails = false;
					windowState_.showContentBrowser = false;
					windowState_.showOutputLog = false;
					windowState_.showScene = false;
					windowState_.showLightEditor = false;
					windowState_.showPostEffectSettings = false;
					windowState_.showDisplay = false;
					windowState_.showParameters = false;
					windowState_.showJsonAssetManager = false;
					windowState_.showTitleDebug = false;
					windowState_.showStageSelectDebug = false;
					windowState_.showGameDebug = false;
					windowState_.showPlayerDebug = false;
					windowState_.showWeaponDebug = false;
					windowState_.showEnemyDebug = false;
					windowState_.showCollisionDebug = false;
					windowState_.showCullingDebug = false;
					windowState_.showFadeManager = false;
				}
				ImGui::EndMenu();
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Tools"))
		{
			ImGui::MenuItem("Stage Editor");
			ImGui::MenuItem("Light Editor");
			ImGui::MenuItem("Particle Editor");
			ImGui::MenuItem("Shader Viewer");
			ImGui::Separator();
			ImGui::MenuItem("Texture Converter");
			ImGui::MenuItem("Mesh Converter");
			ImGui::MenuItem("Font Converter");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Build"))
		{
			const bool buildRunning = assetBuildService_.IsRunning();
			// BuildメニューはTools/Scriptsの既存batを実行する入口にする。
			if (buildRunning)
			{
				ImGui::BeginDisabled();
			}
			if (ImGui::MenuItem("Build Textures"))
			{
				assetBuildService_.StartBuild(EditorAssetBuildKind::Textures);
			}
			if (ImGui::MenuItem("Build Meshes"))
			{
				assetBuildService_.StartBuild(EditorAssetBuildKind::Meshes);
			}
			if (ImGui::MenuItem("Build Fonts"))
			{
				assetBuildService_.StartBuild(EditorAssetBuildKind::Fonts);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Build All Assets"))
			{
				assetBuildService_.StartBuild(EditorAssetBuildKind::All);
			}
			if (buildRunning)
			{
				ImGui::EndDisabled();
			}
			ImGui::TextUnformatted(assetBuildService_.GetStatusText().c_str());
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Debug"))
		{
			ImGui::MenuItem("Show Collider", nullptr, &windowState_.debugShowCollider);
			ImGui::MenuItem("Show Enemy Info", nullptr, &windowState_.debugShowEnemyInfo);
			ImGui::MenuItem("Show Wave Info", nullptr, &windowState_.debugShowWaveInfo);
			ImGui::MenuItem("Show FPS", nullptr, &windowState_.debugShowFps);
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();

		if (openRebuildDefaultLayoutPopup_)
		{
			// PopupをMainMenuBar終了後に開き、MenuItem押下だけでDockBuilderを走らせない。
			ImGui::OpenPopup("Rebuild Default Layout?");
			openRebuildDefaultLayoutPopup_ = false;
		}

		if (ImGui::BeginPopupModal("Rebuild Default Layout?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Rebuild the editor docking layout to the default placement?");
			ImGui::TextUnformatted("Current dock positions will be overwritten when you press OK.");
			ImGui::Separator();
			if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
			{
				// OK時だけDockBuilderによる初期配置再構築を次フレームへ要求する。
				ImGuiManager::GetInstance()->RequestResetDockLayout();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
			{
				// CancelではDockBuilder要求を発行せず現在の保存済みDocking配置を維持する。
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawToolbar()
	{
#ifdef USE_IMGUI
		auto* playController = EditorPlayController::GetInstance();

		const bool fpsCapturePolicy = IsFpsCapturePolicy(sceneManager_);
		// ツールバーは現在Sceneの入力ポリシーに合わせて表示と操作可否を分ける。
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
		if (ImGui::Begin("Toolbar", &windowState_.showToolbar, flags))
		{
			ImGui::Button("Save");
			ImGui::SameLine();
			if (ImGui::Button("Play"))
			{
				// PlayボタンはEditorPlayStateをPlayにし、入力をGameCapturedへ遷移させる。
				playController->Play();
			}
			ImGui::SameLine();
			if (ImGui::Button("Pause"))
			{
				// Pauseボタンは入力状態を維持したままEditorPlayStateだけPauseへ遷移させる。
				playController->Pause();
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop"))
			{
				// Stopボタンは編集状態へ戻し、ゲーム入力をGameReleasedへ解放する。
				playController->Stop();
			}
			ImGui::SameLine();
			// ToolbarのBuildボタンから全アセット変換を実行する。
			if (assetBuildService_.IsRunning())
			{
				ImGui::BeginDisabled();
			}
			if (ImGui::Button("Build"))
			{
				assetBuildService_.StartBuild(EditorAssetBuildKind::All);
			}
			if (assetBuildService_.IsRunning())
			{
				ImGui::EndDisabled();
			}
			ImGui::SameLine();
			ImGui::TextUnformatted("|");
			ImGui::SameLine();
			if (fpsCapturePolicy)
			{
				ImGui::Text("%s  F8: %s", playController->IsGameCaptured() ? "Input: FPS Captured" : "Input: Editor Released", playController->IsGameCaptured() ? "Release" : "Capture");
				ImGui::SameLine();
				if (ImGui::Button(playController->IsGameCaptured() ? "Release Input" : "Capture Input"))
				{
					// ToolbarボタンもFPS操作SceneでだけF8と同じキャプチャ切り替えを実行する。
					playController->ToggleInputCapture();
				}
			}
			else
			{
				// UI Mouse Sceneではキャプチャ操作を出さず常にカーソルUI操作を示す。
				ImGui::TextUnformatted("Input: UI Mouse Mode");
			}
			// Debug Freezeは入力キャプチャとは別状態としてToolbar上で確認できるようにする。
			ImGui::TextUnformatted(playController->GetDebugFreezeStatusText());
			// F8やViewport hoverの状態をToolbar上で即確認できるようにする。
			ImGui::Text("State: %s", playController->GetPlayStateText());
			const char* toolbarInputText = fpsCapturePolicy
				? (playController->IsGameCaptured() ? "FPS Captured" : "Editor Released")
				: "UI Mouse";
			// ToolbarにはPlay状態と入力ポリシーをUE5風に分けて表示する。
			ImGui::Text("Input: %s", toolbarInputText);
			if (!playController->IsPlaying())
			{
				// Edit/Pause中はSceneのゲーム進行がUpdateEditorへ分岐していることを明示する。
				ImGui::TextUnformatted("Game update stopped");
			}
			ImGui::Text("Main Viewport Hovered: %s", inputDebugInfo_.mainViewportHovered ? "true" : "false");
			ImGui::Text("ImGui MouseClicked[0]: %s", inputDebugInfo_.imguiMouseClicked0 ? "true" : "false");
			ImGui::Text("ImGui MouseDown[0]: %s", inputDebugInfo_.imguiMouseDown0 ? "true" : "false");
			ImGui::Text("Input Left Trigger: %s", inputDebugInfo_.inputLeftTrigger ? "true" : "false");
			ImGui::Text("Game Mouse Enabled: %s", inputDebugInfo_.gameMouseEnabled ? "true" : "false");
			ImGui::Text("Game Mouse Position: %.1f, %.1f", inputDebugInfo_.gameMousePosition.x, inputDebugInfo_.gameMousePosition.y);
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawMainViewport()
	{
#ifdef USE_IMGUI
		if (!windowState_.showMainViewport)
		{
			// Main Viewport非表示中はゲーム側へエディタマウス入力を渡さない
			mainViewportRect_.valid = false;
			mainViewportRect_.isHovered = false;
			inputDebugInfo_ = {}; // Main Viewport非表示時もToolbarの入力診断を無効状態へ戻す。
			inputDebugInfo_.gameMousePosition = { -1.0f, -1.0f };
			Input::GetInstance()->SetGameInputEnabled(false);
			Input::GetInstance()->SetEditorViewportMousePosition({ 0.0f, 0.0f }, false);
			Input::GetInstance()->SetLockCursor(false);
			Input::GetInstance()->SetCursorVisible(true);
			return;
		}

		// Main ViewportはGameRenderTargetをDrawListで表示する固定名ウィンドウにする
		if (ImGui::Begin("Main Viewport", &windowState_.showMainViewport, ImGuiWindowFlags_NoScrollbar))
		{
			const ImVec2 availableSize = ImGui::GetContentRegionAvail();
			auto* postEffectManager = PostEffectManager::GetInstance();
			const float renderTargetWidth = static_cast<float>(GameViewportConstants::Width);
			const float renderTargetHeight = static_cast<float>(GameViewportConstants::Height);

			// Main Viewport内では固定解像度GameRenderTargetを16:9維持で最大表示する。
			ImVec2 imageSize = ImVec2(0.0f, 0.0f);
			if (availableSize.x > 1.0f && availableSize.y > 1.0f && renderTargetWidth > 0.0f && renderTargetHeight > 0.0f)
			{
				const float targetAspect = renderTargetWidth / renderTargetHeight;
				imageSize = ImVec2(std::max(0.0f, availableSize.x), std::max(0.0f, availableSize.x / targetAspect));
				if (imageSize.y > availableSize.y)
				{
					imageSize.y = std::max(0.0f, availableSize.y);
					imageSize.x = std::max(0.0f, availableSize.y * targetAspect);
				}
			}

			const ImVec2 contentScreenMin = ImGui::GetCursorScreenPos();
			const ImVec2 imageOffset = ImVec2(
				std::max(0.0f, (availableSize.x - imageSize.x) * 0.5f),
				std::max(0.0f, (availableSize.y - imageSize.y) * 0.5f));
			const ImVec2 imageScreenMin = ImVec2(contentScreenMin.x + imageOffset.x, contentScreenMin.y + imageOffset.y);
			const ImVec2 imageScreenMax = ImVec2(imageScreenMin.x + imageSize.x, imageScreenMin.y + imageSize.y);
			mainViewportScreenPosition_ = { imageScreenMin.x, imageScreenMin.y };
			mainViewportSize_ = { imageSize.x, imageSize.y };
			// 入力はAddImageで描画した中央表示矩形だけを1920x1080へ逆変換する。
			mainViewportRect_.screenMin = mainViewportScreenPosition_;
			mainViewportRect_.screenMax = { imageScreenMax.x, imageScreenMax.y };
			mainViewportRect_.imageSize = mainViewportSize_;
			mainViewportRect_.valid = imageSize.x > 1.0f && imageSize.y > 1.0f;

			const D3D12_GPU_DESCRIPTOR_HANDLE gameSrv = postEffectManager->GetGameRenderTargetSrvHandleGPU();
			if (gameSrv.ptr != 0 && imageSize.x > 1.0f && imageSize.y > 1.0f)
			{
				// SetCursorPosを使わずDrawListへ直接描画してImGuiの境界更新assertを避ける。
				ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(gameSrv.ptr), imageScreenMin, imageScreenMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));

				EditorSelectionOutlineManager* outlineManager = EditorSelectionOutlineManager::GetInstance();
				const D3D12_GPU_DESCRIPTOR_HANDLE outlineSrv = outlineManager->GetOutlineSrvHandleGPU();
				if (outlineManager->HasVisibleOutline() && outlineSrv.ptr != 0)
				{
					// 透明Textureの輪郭だけを同じ矩形へ重ね、選択Object以外のGame描画は変えない。
					ImGui::GetWindowDrawList()->AddImage(
						static_cast<ImTextureID>(outlineSrv.ptr),
						imageScreenMin,
						imageScreenMax,
						ImVec2(0.0f, 0.0f),
						ImVec2(1.0f, 1.0f));
				}
			}
			else if (gameSrv.ptr == 0 && availableSize.x > 1.0f && availableSize.y > 1.0f)
			{
				ImGui::GetWindowDrawList()->AddText(contentScreenMin, ImGui::GetColorU32(ImGuiCol_Text), "GameRenderTarget SRV is not ready.");
			}

#ifdef _DEBUG
			if (showPerformanceOverlay_ && mainViewportRect_.valid)
			{
				// デバッグ中にゲーム画面上で負荷を確認できるよう左上Overlayを描画する。
				ImGui::SetNextWindowBgAlpha(0.45f);
				ImGui::SetNextWindowPos(ImVec2(imageScreenMin.x + 8.0f, imageScreenMin.y + 8.0f), ImGuiCond_Always);
				const ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
					ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
				if (ImGui::Begin("Performance Overlay", nullptr, overlayFlags))
				{
					const PerformanceStats& stats = outputLogPerformanceMonitor_.GetStats();
					if (performanceOverlayCompactMode_)
					{
						DrawPerformanceCompactLine(stats, outputLogPerformanceDisplayMode_);
					}
					else
					{
						ImGui::Text("Instant FPS: %.1f", stats.instantFps);
						ImGui::Text("Average FPS: %.1f", stats.fps);
						ImGui::Text("FrameTime: %.2f ms", stats.frameTimeMs);
						ImGui::Text("VSync: ON");
						ImGui::Text("CPU: %.1f%%", stats.cpuUsagePercent);
						ImGui::Text("Mem: %.1f MB", stats.memoryUsageMB);
					}
				}
				ImGui::End();
			}
#endif // _DEBUG

			if (availableSize.x > 1.0f && availableSize.y > 1.0f)
			{
				// InvisibleButtonはMain ViewportのImGuiアイテム登録だけに使い、ゲーム入力判定は画像矩形で行う。
				ImGui::InvisibleButton("MainViewportImageArea", availableSize);
				const ImVec2 mouseScreen = ImGui::GetMousePos();
				const bool mouseInsideImage = mainViewportRect_.valid &&
					mouseScreen.x >= imageScreenMin.x && mouseScreen.y >= imageScreenMin.y &&
					mouseScreen.x <= imageScreenMax.x && mouseScreen.y <= imageScreenMax.y;
				const bool otherItemActive = ImGui::IsAnyItemActive() && !ImGui::IsItemActive();
				// WantCaptureMouseではなくSceneポリシーとMain Viewport画像上かどうかでゲームクリックを許可する。
				mainViewportRect_.isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && mouseInsideImage && !otherItemActive;
			}
			else
			{
				mainViewportRect_.isHovered = false;
			}

			Vector2 gameMouse = {};
			const bool gameMouseValid = GetMousePositionInGameViewport(gameMouse);
			const EditorInputPolicy inputPolicy = GetCurrentEditorInputPolicy(sceneManager_);
			const bool isPlaying = EditorPlayController::GetInstance()->IsPlaying();
			// Edit/Pause中はMain ViewportクリックをゲームSceneへ渡さず、Play中だけ入力ポリシーを適用する。
			const bool gameInputEnabled = isPlaying && (inputPolicy == EditorInputPolicy::UiMouse || EditorPlayController::GetInstance()->IsGameCaptured()) && mainViewportRect_.isHovered;
			Input::GetInstance()->SetGameInputEnabled(gameInputEnabled);
			Input::GetInstance()->SetEditorViewportMousePosition(gameMouse, gameMouseValid);
			// UI Mouseは常にカーソル表示、FPS CaptureはGameCaptured中だけ中央固定にする。
			const bool lockForFpsCapture = inputPolicy == EditorInputPolicy::FpsCapture && gameInputEnabled;
			Input::GetInstance()->SetLockCursor(lockForFpsCapture);
			Input::GetInstance()->SetCursorVisible(!lockForFpsCapture);
			inputDebugInfo_.mainViewportHovered = mainViewportRect_.isHovered; // 入力許可条件のViewport hoverをToolbarに表示する。
			inputDebugInfo_.imguiMouseClicked0 = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
			inputDebugInfo_.imguiMouseDown0 = ImGui::IsMouseDown(ImGuiMouseButton_Left);
			inputDebugInfo_.inputLeftTrigger = Input::GetInstance()->TriggerMouse(0);
			inputDebugInfo_.gameMouseEnabled = gameInputEnabled && gameMouseValid;
			inputDebugInfo_.gameMousePosition = inputDebugInfo_.gameMouseEnabled ? gameMouse : Vector2{ -1.0f, -1.0f };
		}
		else
		{
			// Main Viewportウィンドウが折りたたまれた場合もゲーム側のマウス入力を無効化する
			mainViewportRect_.valid = false;
			mainViewportRect_.isHovered = false;
			inputDebugInfo_ = {}; // Main Viewportが描けない時はゲームクリック診断も無効化する。
			inputDebugInfo_.gameMousePosition = { -1.0f, -1.0f };
			Input::GetInstance()->SetGameInputEnabled(false);
			Input::GetInstance()->SetEditorViewportMousePosition({ 0.0f, 0.0f }, false);
			Input::GetInstance()->SetLockCursor(false);
			Input::GetInstance()->SetCursorVisible(true);
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	Vector2 EditorWindowManager::ConvertScreenToMainViewportPosition(const Vector2& screenPosition) const
	{
		// 入力系はこの入口でスクリーン座標からMain Viewportローカル座標へ変換する
		return { screenPosition.x - mainViewportScreenPosition_.x, screenPosition.y - mainViewportScreenPosition_.y };
	}

	bool EditorWindowManager::GetMousePositionInGameViewport(Vector2& outMouse) const
	{
#ifdef USE_IMGUI
		outMouse = { 0.0f, 0.0f };
		const EditorInputPolicy inputPolicy = GetCurrentEditorInputPolicy(sceneManager_);
		if (inputPolicy == EditorInputPolicy::FpsCapture && !(EditorPlayController::GetInstance()->IsPlaying() && EditorPlayController::GetInstance()->IsGameCaptured()))
		{
			// FPS Capture SceneはPlayかつGameCaptured中だけゲーム側へマウス入力を渡す。
			return false;
		}

		if (!mainViewportRect_.valid || !mainViewportRect_.isHovered)
		{
			// Main Viewport外ではImGuiのCapture状態に関係なくゲーム側へマウス入力を渡さない。
			return false;
		}

		const ImVec2 mouseScreen = ImGui::GetMousePos();
		const Vector2 mouse = { mouseScreen.x, mouseScreen.y };
		if (mouse.x < mainViewportRect_.screenMin.x || mouse.y < mainViewportRect_.screenMin.y ||
			mouse.x > mainViewportRect_.screenMax.x || mouse.y > mainViewportRect_.screenMax.y)
		{
			return false;
		}

		const float renderTargetWidth = static_cast<float>(GameViewportConstants::Width);
		const float renderTargetHeight = static_cast<float>(GameViewportConstants::Height);
		if (renderTargetWidth <= 0.0f || renderTargetHeight <= 0.0f ||
			mainViewportRect_.imageSize.x <= 0.0f || mainViewportRect_.imageSize.y <= 0.0f)
		{
			return false;
		}

		// 表示矩形内のローカル座標を固定GameViewportRenderTarget(1920x1080)へスケール変換する。
		const Vector2 localMouse = { mouse.x - mainViewportRect_.screenMin.x, mouse.y - mainViewportRect_.screenMin.y };
		outMouse = {
			(localMouse.x / mainViewportRect_.imageSize.x) * renderTargetWidth,
			(localMouse.y / mainViewportRect_.imageSize.y) * renderTargetHeight
		};
		return true;
#else
		outMouse = { 0.0f, 0.0f };
		return false;
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawWorldOutliner()
	{
#ifdef USE_IMGUI
		std::vector<EditorObjectInfo> objects;
		BaseScene* scene = sceneManager_ ? sceneManager_->GetCurrentScene() : nullptr;
		if (scene)
		{
			// Scene側から軽量情報だけを収集し、Outlinerが実オブジェクト寿命へ依存しないようにする。
			scene->CollectEditorObjects(objects);
		}

		if (selection_.HasSelection())
		{
			const EditorObjectInfo& selected = selection_.GetSelected();
			auto selectedObject = std::find_if(objects.begin(), objects.end(), [&selected](const EditorObjectInfo& object)
				{
					return object.id == selected.id && object.sceneName == selected.sceneName;
				});
			if (selectedObject != objects.end())
			{
				// Detailsが古いSceneオブジェクト入口を掴み続けないよう、現在フレームの情報へ更新する。
				selection_.RefreshSelected(*selectedObject);
			}
			else
			{
				// Scene切り替えやロード状態変化で消えた選択はDetails表示前に破棄する。
				selection_.Clear();
			}
		}

		if (!windowState_.showWorldOutliner)
		{
			// Detailsだけ表示している場合も、現在Sceneの収集結果で古い選択を先に破棄する。
			return;
		}

		if (ImGui::Begin("World Outliner", &windowState_.showWorldOutliner))
		{
			const char* sceneLabel = objects.empty() ? "Current Scene" : objects.front().sceneName.c_str();
			if (ImGui::TreeNodeEx(sceneLabel, ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (objects.empty())
				{
					ImGui::TextUnformatted("No editor objects.");
				}
				for (const EditorObjectInfo& object : objects)
				{
					const bool selected = selection_.HasSelection() &&
						selection_.GetSelected().id == object.id &&
						selection_.GetSelected().sceneName == object.sceneName;
					const std::string label = object.displayName + "##" + object.sceneName + std::to_string(object.id);
					if (ImGui::Selectable(label.c_str(), selected))
					{
						// クリックしたOutliner項目の軽量情報を選択状態として保存する。
						selection_.Select(object);
					}
				}
				ImGui::TreePop();
			}
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawDetails()
	{
#ifdef USE_IMGUI
		if (!windowState_.showDetails)
		{
			return;
		}

		if (ImGui::Begin("Details", &windowState_.showDetails))
		{
			if (!selection_.HasSelection())
			{
				ImGui::TextUnformatted("No object selected.");
			}
			else
			{
				const EditorObjectInfo& selected = selection_.GetSelected();
				// DetailsはTransform専用ではなく、Outliner選択に対応するInspector種別で必ず表示内容を切り替える。
				ImGui::Text("Selected Name: %s", selected.displayName.c_str());
				ImGui::Text("Type: %s", selected.typeName.c_str());
				ImGui::Text("Scene: %s", selected.sceneName.c_str());
				ImGui::Text("ID: %llu", static_cast<unsigned long long>(selected.id));
				// 共通表示にInspector Type名を出し、Details側がどの描画分岐を使ったか確認しやすくする。
				ImGui::Text("Inspector Type: %s", ToString(selected.inspectorType));
				if (!selected.inspectorHint.empty())
				{
					ImGui::Text("Inspector: %s", selected.inspectorHint.c_str());
				}
				ImGui::Separator();

				switch (selected.inspectorType)
				{
				case EditorInspectorType::Transform:
				{
					EditorTransform transform{};
					if (selected.TryReadTransform(transform))
					{
						bool changed = false;
						changed |= ImGui::DragFloat3("Position", &transform.position.x, 0.1f);
						changed |= ImGui::DragFloat3("Rotation", &transform.rotation.x, 0.01f);
						changed |= ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f);
						if (changed)
						{
							// ImGuiで編集した値はScene側が用意した書き戻し入口から即時反映する。
							selected.WriteTransform(transform);
						}
					}
					else
					{
						ImGui::Text("%s", selected.transformUnavailableReason.c_str());
					}
					break;
				}
				case EditorInspectorType::PunctualLights:
					ImGui::TextUnformatted("Type: Light Manager / Global Lighting Debug");
					LightManager::GetInstance()->DrawPunctualLightsInspector();
					break;
				case EditorInspectorType::FadeManager:
					if (selected.drawInspector)
					{
						selected.drawInspector();
					}
					else
					{
						ImGui::TextUnformatted("FadeManager Inspector is unavailable.");
					}
					break;
				case EditorInspectorType::StageSelectTextLayout:
					if (selected.drawInspector)
					{
						selected.drawInspector();
					}
					else
					{
						ImGui::TextUnformatted("StageSelect Text Layout Inspector is unavailable.");
					}
					break;
				case EditorInspectorType::PlayerInfo:
				case EditorInspectorType::EnemyManagerInfo:
				case EditorInspectorType::BulletManagerInfo:
				case EditorInspectorType::WaveManagerInfo:
				case EditorInspectorType::StageInfo:
				case EditorInspectorType::HudInfo:
				case EditorInspectorType::CollisionManagerInfo:
				case EditorInspectorType::ManagerInfo:
					if (selected.drawInspector)
					{
						// Scene固有の安全な読み取りだけをDetails側から呼び、未取得値はScene側でN/A表示にする。
						selected.drawInspector();
					}
					else
					{
						ImGui::TextUnformatted("This object has no editable Transform.");
						ImGui::TextUnformatted("Use the dedicated Debug window for detailed editing when available.");
					}
					break;
				case EditorInspectorType::None:
				default:
					ImGui::TextUnformatted("この項目はDetails Inspector未実装です。");
					if (!selected.transformUnavailableReason.empty())
					{
						ImGui::Text("%s", selected.transformUnavailableReason.c_str());
					}
					break;
				}
			}
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawContentBrowser()
	{
#ifdef USE_IMGUI
		if (!windowState_.showContentBrowser)
		{
			return;
		}

		// Content Browserは上部操作を固定し、リスト/グリッドとDetailsをカテゴリ別描画へ分離する。
		if (ImGui::Begin("Content Browser", &windowState_.showContentBrowser))
		{
			const std::array<EditorAssetCategory, 4> categories = {
				EditorAssetCategory::Textures,
				EditorAssetCategory::Models,
				EditorAssetCategory::Shaders,
				EditorAssetCategory::Fonts
			};
			for (EditorAssetCategory category : categories)
			{
				if (category != categories.front())
				{
					ImGui::SameLine();
				}
				const bool selected = assetBrowser_.GetCategory() == category;
				if (selected)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
				}
				if (ImGui::Button(EditorAssetBrowser::GetCategoryName(category)))
				{
					if (assetBrowser_.GetCategory() != category)
					{
						// カテゴリ変更時は古いPreview SRVを解放して本編Texture管理と分離する。
						texturePreviewCache_.Clear();
					}
					assetBrowser_.SetCategory(category);
				}
				if (selected)
				{
					ImGui::PopStyleColor();
				}
			}

			ImGui::Separator();
			const bool sourceSelected = assetBrowser_.GetViewMode() == EditorAssetViewMode::Sources;
			if (sourceSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			}
			if (ImGui::Button("Sources"))
			{
				if (!sourceSelected)
				{
					// Sources/Compiled切替時は選択画像のPreviewキャッシュを破棄する。
					texturePreviewCache_.Clear();
				}
				assetBrowser_.SetViewMode(EditorAssetViewMode::Sources);
			}
			if (sourceSelected)
			{
				ImGui::PopStyleColor();
			}
			ImGui::SameLine();
			const bool compiledSelected = assetBrowser_.GetViewMode() == EditorAssetViewMode::Compiled;
			if (compiledSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			}
			if (ImGui::Button("Compiled"))
			{
				if (!compiledSelected)
				{
					// Sources/Compiled切替時は選択画像のPreviewキャッシュを破棄する。
					texturePreviewCache_.Clear();
				}
				assetBrowser_.SetViewMode(EditorAssetViewMode::Compiled);
			}
			if (compiledSelected)
			{
				ImGui::PopStyleColor();
			}
			ImGui::SameLine();
			if (ImGui::Button("Refresh"))
			{
				// Refreshではファイル変更後の画像を再ロードできるようPreviewキャッシュも消す。
				texturePreviewCache_.Clear();
				assetBrowser_.Refresh();
			}

			char filterBuffer[256] = {};
			const std::string& currentFilter = assetBrowser_.GetSearchFilter();

			// snprintfで検索フィルタ文字列を安全に固定長バッファへコピーする。
			std::snprintf(filterBuffer, sizeof(filterBuffer), "%s", currentFilter.c_str());

			ImGui::SetNextItemWidth(std::max(160.0f, ImGui::GetContentRegionAvail().x * 0.45f));
			if (ImGui::InputTextWithHint("##ContentBrowserSearch", "Search assets...", filterBuffer, sizeof(filterBuffer)))
			{
				assetBrowser_.SetSearchFilter(filterBuffer);
			}
			ImGui::SameLine();
			ImGui::TextDisabled("%zu assets", assetBrowser_.GetFilteredEntries().size());

			if (ImGui::TreeNodeEx("Root", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::TextDisabled("%s", ToUtf8Path(assetBrowser_.GetCurrentRoot()).c_str());
				ImGui::TreePop();
			}
			ImGui::Separator();

			const float availableWidth = ImGui::GetContentRegionAvail().x;
			const bool splitHorizontally = availableWidth >= 620.0f;

			if (assetBrowser_.GetCategory() == EditorAssetCategory::Textures)
			{
				DrawTextureGrid();
			}
			else
			{
				DrawAssetList();
			}

			if (splitHorizontally)
			{
				ImGui::SameLine();
			}
			DrawAssetDetails();
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawAssetList()
	{
#ifdef USE_IMGUI
		const float availableWidth = ImGui::GetContentRegionAvail().x;
		const float detailsWidth = availableWidth >= 620.0f ? std::clamp(availableWidth * 0.36f, 300.0f, 460.0f) : 0.0f;
		const bool splitHorizontally = availableWidth >= 620.0f;
		const ImVec2 childSize = splitHorizontally ? ImVec2(-detailsWidth - ImGui::GetStyle().ItemSpacing.x, 0.0f) : ImVec2(0.0f, std::max(180.0f, ImGui::GetContentRegionAvail().y * 0.52f));
		if (ImGui::BeginChild("AssetList", childSize, true, ImGuiWindowFlags_HorizontalScrollbar))
		{
			ImGui::TextDisabled("Assets");
			ImGui::Separator();
			const auto& entries = assetBrowser_.GetFilteredEntries();
			if (entries.empty())
			{
				ImGui::TextUnformatted("No assets found.");
			}
			const EditorAssetEntry* selectedEntry = assetBrowser_.GetSelectedEntry();
			for (std::size_t index = 0; index < entries.size(); ++index)
			{
				const EditorAssetEntry& entry = entries[index];
				const bool isSelected = selectedEntry && selectedEntry->relativePath == entry.relativePath;
				const std::string rowId = "##asset" + std::to_string(index);
				ImGui::PushID(static_cast<int>(index));
				if (ImGui::Selectable(rowId.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, 42.0f)))
				{
					assetBrowser_.Select(index);
				}
				if (ImGui::IsItemVisible())
				{
					const ImVec2 rowMin = ImGui::GetItemRectMin();
					ImDrawList* drawList = ImGui::GetWindowDrawList();

					const ImU32 iconColor = ImGui::GetColorU32(ImVec4(0.75f, 0.9f, 1.0f, 1.0f));
					const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
					const ImU32 disabledColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);

					// AssetListの行内テキストはDrawListで描画し、ImGuiのレイアウトカーソルを動かさない。
					drawList->AddText(ImVec2(rowMin.x + 8.0f, rowMin.y + 5.0f), iconColor, entry.icon.c_str());
					drawList->AddText(ImVec2(rowMin.x + 66.0f, rowMin.y + 5.0f), textColor, entry.label.c_str());

					const std::string parentPath = GetParentPathText(entry);
					drawList->AddText(ImVec2(rowMin.x + 66.0f, rowMin.y + 23.0f), disabledColor, parentPath.c_str());
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawTextureGrid()
	{
#ifdef USE_IMGUI
		const float availableWidth = ImGui::GetContentRegionAvail().x;
		const float detailsWidth = availableWidth >= 620.0f ? std::clamp(availableWidth * 0.36f, 300.0f, 460.0f) : 0.0f;
		const bool splitHorizontally = availableWidth >= 620.0f;
		const ImVec2 childSize = splitHorizontally ? ImVec2(-detailsWidth - ImGui::GetStyle().ItemSpacing.x, 0.0f) : ImVec2(0.0f, std::max(180.0f, ImGui::GetContentRegionAvail().y * 0.52f));
		if (ImGui::BeginChild("TextureGrid", childSize, true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
		{
			ImGui::TextDisabled("Textures");
			ImGui::SameLine();
			ImGui::TextDisabled("%s", EditorAssetBrowser::GetViewModeName(assetBrowser_.GetViewMode()));
			ImGui::Separator();

			const auto& entries = assetBrowser_.GetFilteredEntries();
			if (entries.empty())
			{
				ImGui::TextUnformatted("No textures found.");
				ImGui::EndChild();
				return;
			}

			const float contentWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
			const float minCellWidth = 132.0f;
			const int columnCount = std::max(1, static_cast<int>(contentWidth / minCellWidth));
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const float cellWidth = std::floor((contentWidth - spacing * static_cast<float>(columnCount - 1)) / static_cast<float>(columnCount));
			const float thumbnailSize = std::max(72.0f, cellWidth - 18.0f);
			const float cellHeight = thumbnailSize + 54.0f;
			const EditorAssetEntry* selectedEntry = assetBrowser_.GetSelectedEntry();

			for (std::size_t index = 0; index < entries.size(); ++index)
			{
				const EditorAssetEntry& entry = entries[index];
				const bool isSelected = selectedEntry && selectedEntry->relativePath == entry.relativePath;
				ImGui::PushID(static_cast<int>(index));
				// サムネイルセルはInvisibleButtonで領域を確保し、装飾だけDrawListへ任せてスクロール時assertを避ける。
				ImGui::InvisibleButton("##TextureCell", ImVec2(cellWidth, cellHeight));
				if (ImGui::IsItemClicked())
				{
					assetBrowser_.Select(index);
				}
				if (ImGui::IsItemVisible())
				{
					ImDrawList* drawList = ImGui::GetWindowDrawList();
					const ImVec2 min = ImGui::GetItemRectMin();
					const ImVec2 max = ImGui::GetItemRectMax();
					const ImU32 backgroundColor = ImGui::GetColorU32(isSelected ? ImVec4(0.18f, 0.34f, 0.58f, 0.88f) : ImVec4(0.12f, 0.12f, 0.14f, 0.86f));
					const ImU32 borderColor = ImGui::GetColorU32(isSelected ? ImVec4(0.45f, 0.72f, 1.0f, 1.0f) : ImVec4(0.28f, 0.28f, 0.32f, 1.0f));
					drawList->AddRectFilled(min, max, backgroundColor, 7.0f);
					drawList->AddRect(min, max, borderColor, 7.0f, 0, isSelected ? 2.5f : 1.0f);

					const ImVec2 thumbMin(min.x + 9.0f, min.y + 9.0f);
					const ImVec2 thumbMax(min.x + 9.0f + thumbnailSize, min.y + 9.0f + thumbnailSize);
					DrawCheckerBackground(drawList, thumbMin, thumbMax);
					drawList->AddRect(thumbMin, thumbMax, ImGui::GetColorU32(ImVec4(0.38f, 0.38f, 0.42f, 1.0f)), 4.0f);

					if (EditorAssetBrowser::IsImageExtension(entry.extension))
					{
						const EditorTexturePreview& preview = texturePreviewCache_.GetOrLoad(std::filesystem::path(entry.absolutePath));
						if (preview.loaded && preview.srvHandleGPU.ptr != 0 && preview.width > 0 && preview.height > 0)
						{
							const ImVec2 imageSize = FitImageSize(preview.width, preview.height, ImVec2(thumbnailSize - 8.0f, thumbnailSize - 8.0f));
							const ImVec2 imageMin(thumbMin.x + (thumbnailSize - imageSize.x) * 0.5f, thumbMin.y + (thumbnailSize - imageSize.y) * 0.5f);
							const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
							drawList->AddImage(static_cast<ImTextureID>(preview.srvHandleGPU.ptr), imageMin, imageMax);
						}
						else
						{
							const char* text = preview.failed ? "No Preview" : "Loading";
							const ImVec2 textSize = ImGui::CalcTextSize(text);
							drawList->AddText(ImVec2(thumbMin.x + (thumbnailSize - textSize.x) * 0.5f, thumbMin.y + (thumbnailSize - textSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
						}
					}
					else
					{
						const char* text = "No Preview";
						const ImVec2 textSize = ImGui::CalcTextSize(text);
						drawList->AddText(ImVec2(thumbMin.x + (thumbnailSize - textSize.x) * 0.5f, thumbMin.y + (thumbnailSize - textSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
					}

					const std::string displayName = TruncateTextToWidth(entry.label, cellWidth - 18.0f);
					drawList->AddText(ImVec2(min.x + 9.0f, thumbMax.y + 8.0f), ImGui::GetColorU32(ImGuiCol_Text), displayName.c_str());

					const std::string badgeText = entry.extension.empty() ? "FILE" : entry.extension.substr(1);
					const ImVec2 badgeSize = ImGui::CalcTextSize(badgeText.c_str());
					const ImVec2 badgeMin(min.x + 9.0f, max.y - 22.0f);
					const ImVec2 badgeMax(badgeMin.x + badgeSize.x + 12.0f, badgeMin.y + 17.0f);
					drawList->AddRectFilled(badgeMin, badgeMax, ImGui::GetColorU32(ImVec4(0.10f, 0.44f, 0.68f, 1.0f)), 8.0f);
					drawList->AddText(ImVec2(badgeMin.x + 6.0f, badgeMin.y + 1.0f), ImGui::GetColorU32(ImGuiCol_Text), badgeText.c_str());
				}

				if ((static_cast<int>(index) + 1) % columnCount != 0)
				{
					ImGui::SameLine();
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawAssetDetails()
	{
#ifdef USE_IMGUI
		if (ImGui::BeginChild("AssetDetails", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
		{
			ImGui::TextUnformatted("Selected Asset");
			ImGui::Separator();
			const EditorAssetEntry* selectedEntry = assetBrowser_.GetSelectedEntry();
			if (!selectedEntry)
			{
				ImGui::TextUnformatted("No asset selected.");
			}
			else
			{
				const bool isImage = EditorAssetBrowser::IsImageExtension(selectedEntry->extension);
				DrawDetailRow("File", selectedEntry->label);
				DrawDetailRow("Category", std::string(EditorAssetBrowser::GetCategoryName(assetBrowser_.GetCategory())) + " / " + EditorAssetBrowser::GetViewModeName(assetBrowser_.GetViewMode()));
				DrawDetailRow("Relative", selectedEntry->relativePath);
				DrawDetailRow("Absolute", selectedEntry->absolutePath);
				DrawDetailRow("Extension", selectedEntry->extension.empty() ? "(none)" : selectedEntry->extension);
				DrawDetailRow("Size", FormatBytes(selectedEntry->sizeBytes) + " (" + std::to_string(static_cast<unsigned long long>(selectedEntry->sizeBytes)) + " bytes)");
				DrawDetailRow("Modified", selectedEntry->modifiedTime);
				DrawDetailRow("Source", EditorAssetBrowser::GetViewModeName(assetBrowser_.GetViewMode()));
				if (ImGui::Button("Copy Path"))
				{
					ImGui::SetClipboardText(selectedEntry->absolutePath.c_str());
					outputLog_.Info("Copied asset path: " + selectedEntry->absolutePath);
				}
				ImGui::SameLine();
#ifdef _WIN32
				if (ImGui::Button("Open Folder"))
				{
					const std::filesystem::path folder = std::filesystem::path(selectedEntry->absolutePath).parent_path();
					const HINSTANCE result = ShellExecuteA(nullptr, "open", folder.string().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
					if (reinterpret_cast<intptr_t>(result) <= 32)
					{
						outputLog_.Warning("Failed to open asset folder: " + folder.string());
					}
				}
#else
				ImGui::BeginDisabled();
				ImGui::Button("Open Folder");
				ImGui::EndDisabled();
#endif // _WIN32
				ImGui::Separator();

				if (isImage)
				{
					// Detailsの大きいPreviewもEditorTexturePreviewCache経由で同じSRVを再利用する。
					const std::filesystem::path previewPath(selectedEntry->absolutePath);
					const EditorTexturePreview& preview = texturePreviewCache_.GetOrLoad(previewPath);

					if (preview.width > 0 && preview.height > 0)
					{
						DrawDetailRow("Resolution", std::to_string(preview.width) + " x " + std::to_string(preview.height));
					}
					ImGui::TextDisabled("Preview");
					const float maxPreviewWidth = std::max(80.0f, ImGui::GetContentRegionAvail().x - 8.0f);
					if (preview.loaded && preview.srvHandleGPU.ptr != 0 && preview.width > 0 && preview.height > 0)
					{
						const float maxPreviewHeight = std::max(120.0f, ImGui::GetContentRegionAvail().y * 0.58f);
						const ImVec2 imageSize = FitImageSize(preview.width, preview.height, ImVec2(maxPreviewWidth, maxPreviewHeight));
						const ImVec2 previewMin = ImGui::GetCursorScreenPos();
						const ImVec2 previewBox(std::max(80.0f, imageSize.x), std::max(80.0f, imageSize.y));
						// Details PreviewはDummyで領域を確保してからDrawListへ描画し、ImGuiカーソルを直接戻さない。
						ImGui::Dummy(previewBox);
						ImDrawList* drawList = ImGui::GetWindowDrawList();
						DrawCheckerBackground(drawList, previewMin, ImVec2(previewMin.x + previewBox.x, previewMin.y + previewBox.y));
						drawList->AddImage(static_cast<ImTextureID>(preview.srvHandleGPU.ptr), previewMin, ImVec2(previewMin.x + imageSize.x, previewMin.y + imageSize.y));
						drawList->AddRect(previewMin, ImVec2(previewMin.x + previewBox.x, previewMin.y + previewBox.y), ImGui::GetColorU32(ImVec4(0.38f, 0.38f, 0.42f, 1.0f)), 4.0f);
					}
					else
					{
						ImGui::TextWrapped("%s", preview.message.empty() ? "Preview unavailable" : preview.message.c_str());
					}
				}
				else
				{
					ImGui::TextDisabled("Preview");
					ImGui::BeginDisabled();
					ImGui::Button(selectedEntry->icon.c_str(), ImVec2(std::min(160.0f, ImGui::GetContentRegionAvail().x), 88.0f));
					ImGui::EndDisabled();
					ImGui::TextWrapped("%s preview is not implemented yet.", EditorAssetBrowser::GetCategoryName(assetBrowser_.GetCategory()));
				}
			}
		}
		ImGui::EndChild();
#endif // USE_IMGUI
	}

	void EditorWindowManager::DrawOutputLog()
	{
#ifdef USE_IMGUI
		if (!windowState_.showOutputLog)
		{
			return;
		}

		// Output LogはEditorOutputLogのバッファを描画し、Build/Content Browserの結果を集約する。
		if (ImGui::Begin("Output Log", &windowState_.showOutputLog))
		{
			const PerformanceStats& performanceStats = outputLogPerformanceMonitor_.GetStats();

			if (ImGui::Button("Clear"))
			{
				outputLog_.Clear();
			}
			ImGui::SameLine();
			ImGui::Checkbox("Auto Scroll", &outputLogAutoScroll_);
			ImGui::SameLine();
			ImGui::Text("Build: %s", assetBuildService_.GetStatusText().c_str());
			if (windowState_.debugShowFps)
			{
				ImGui::SameLine();
				ImGui::Text("FPS: %.1f", performanceStats.fps);
			}
			ImGui::Separator();
			// ログ表示を邪魔しないようにPerformance情報は通常1行で表示する。
			if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Show Performance In OutputLog", &showPerformanceInOutputLog_);
#ifdef _DEBUG
				ImGui::Checkbox("Show Performance Overlay", &showPerformanceOverlay_);
				ImGui::Checkbox("Overlay Compact Mode", &performanceOverlayCompactMode_);
#else
				bool disabledOverlay = false;
				ImGui::BeginDisabled();
				ImGui::Checkbox("Show Performance Overlay", &disabledOverlay);
				ImGui::Checkbox("Overlay Compact Mode", &disabledOverlay);
				ImGui::EndDisabled();
#endif
				const char* displayModeLabels[] = { "FPS", "FrameTime(ms)" };
				int displayMode = static_cast<int>(outputLogPerformanceDisplayMode_);
				if (ImGui::Combo("Display Mode", &displayMode, displayModeLabels, IM_ARRAYSIZE(displayModeLabels)))
				{
					outputLogPerformanceDisplayMode_ = static_cast<OutputLogPerformanceDisplayMode>(displayMode);
				}

				if (showPerformanceInOutputLog_)
				{
					DrawPerformanceCompactLine(performanceStats, outputLogPerformanceDisplayMode_);
					if (ImGui::CollapsingHeader("Performance Details"))
					{
						ImGui::Text("Instant FPS: %.1f", performanceStats.instantFps);
						ImGui::Text("Average FPS: %.1f", performanceStats.fps);
						ImGui::Text("FrameTime: %.2f ms", performanceStats.frameTimeMs);
						ImGui::Text("CPU Usage: %.1f %%", performanceStats.cpuUsagePercent);
						ImGui::Text("Process CPU Usage: %.1f %%", performanceStats.processCpuUsagePercent);
						ImGui::Text("Memory Usage: %.1f MB", performanceStats.memoryUsageMB);
					}
				}
				ImGui::Separator();
			}

			if (ImGui::BeginChild("OutputLogScroll", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar))
			{
				const std::vector<EditorLogEntry> entries = outputLog_.GetEntries();
				for (const EditorLogEntry& entry : entries)
				{
					ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
					if (entry.level == EditorLogLevel::Warning)
					{
						color = ImVec4(1.0f, 0.82f, 0.2f, 1.0f);
					}
					else if (entry.level == EditorLogLevel::Error)
					{
						color = ImVec4(1.0f, 0.25f, 0.25f, 1.0f);
					}
					ImGui::PushStyleColor(ImGuiCol_Text, color);
					ImGui::TextWrapped("[%s] %s", ToString(entry.level), entry.message.c_str());
					ImGui::PopStyleColor();
				}
				if (outputLogAutoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
				{
					ImGui::SetScrollHereY(1.0f);
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	
	void EditorWindowManager::DrawScene()
	{
#ifdef USE_IMGUI
		if (!windowState_.showScene)
		{
			return;
		}

		if (ImGui::Begin("Scene", &windowState_.showScene))
		{
			BaseScene* currentScene = sceneManager_ ? sceneManager_->GetCurrentScene() : nullptr;
			std::vector<EditorObjectInfo> sceneObjects;
			if (currentScene) { currentScene->CollectEditorObjects(sceneObjects); }
			const char* currentSceneName = sceneObjects.empty() ? "(loading / none)" : sceneObjects.front().sceneName.c_str();
			// Sceneウィンドウは現在シーン確認と通常シーン遷移の最小入口に限定する。
			ImGui::Text("Current Scene: %s", currentSceneName);
			ImGui::Separator();
			if (sceneManager_ && ImGui::Button("TitleScene")) { sceneManager_->ChangeScene("TitleScene"); }
			ImGui::SameLine();
			if (sceneManager_ && ImGui::Button("StageSelectScene")) { sceneManager_->ChangeScene("StageSelectScene"); }
			ImGui::SameLine();
			if (sceneManager_ && ImGui::Button("GamePlayScene")) { sceneManager_->ChangeScene("GamePlayScene"); }
			ImGui::SameLine();
			// DebugScene は開発用導線だけ残し、キー入力遷移は復活させない。
			if (sceneManager_ && ImGui::Button("DebugScene")) { sceneManager_->ChangeScene("DebugScene"); }
		}
		ImGui::End();
#endif // USE_IMGUI
	}

void EditorWindowManager::InitializeEditorServices()
	{
#ifdef USE_IMGUI
		if (editorServicesInitialized_)
		{
			return;
		}

		// Editor専用サービスはImGui有効時だけ初期化し、通常ビルドの依存を増やさない。
		outputLog_.Info("[Editor] UE5-style editor shell initialized.");
		assetBrowser_.Initialize(&outputLog_);
		assetBuildService_.Initialize(&outputLog_);
		editorServicesInitialized_ = true;
#endif // USE_IMGUI
	}

	void EditorWindowManager::AddOutputLog(EditorLogLevel level, const std::string& message)
	{
#ifdef USE_IMGUI
		// モード切替などEditorWindowManager外からの通知もOutput Logへ集約する。
		InitializeEditorServices();
		outputLog_.Add(level, message);
#else
		(void)level;
		(void)message;
#endif // USE_IMGUI
	}

	void EditorWindowManager::FinalizeEditorServices()
	{
#ifdef USE_IMGUI
		if (!editorServicesInitialized_)
		{
			return;
		}

		// DirectXCommon/SRVManager破棄前にプレビュー用SRVとD3D12Resourceを明示解放する。
		texturePreviewCache_.Clear();
		outputLog_.Info("[Editor] Preview cache cleared.");
		editorServicesInitialized_ = false;
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
