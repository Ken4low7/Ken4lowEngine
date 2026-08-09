#pragma once

#include "EditorAssetPlacementService.h"
#include "EditorContentBrowserPanel.h"
#include "EditorContext.h"
#include "EditorHierarchyPanel.h"
#include "EditorModeController.h"
#include "EditorPanelIds.h"
#include "EditorPlayController.h"
#include "EditorTransformGizmo.h"
#include "EditorViewportController.h"
#include "EditorWindowManager.h"

#include <CameraManager.h>
#include <DebugCamera.h>
#include <Input.h>
#include <Wireframe.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>
	/// DockSpaceへ常設するEditor Shell固有パネルを描画します。
	/// </summary>
	class EditorShell
	{
	public:
		static EditorShell* GetInstance()
		{
			static EditorShell instance;
			return &instance;
		}

		void Draw()
		{
#ifdef USE_IMGUI
			if (!EditorModeController::GetInstance()->IsEditorModeEnabled())
			{
				return; // ゲームプレビュー中はEditor専用パネルを表示しない。
			}

			ApplyViewportVisualPolicy();
			DrawPlaceActors();
			EditorContentBrowserPanel::GetInstance()->Draw(); // Resources全体を扱うContent Browser V2をDockSpaceへ登録する。
			EditorContentBrowserPanel::GetInstance()->UpdateAssetDragSource(); // 選択したモデルやPrefabをViewportへドラッグできるようにする。
			EditorHierarchyPanel::GetInstance()->Draw(); // World OutlinerとDetailsを同じ選択状態で先に登録する。
#endif
		}

		/// <summary>
		/// メインビューポートの描画後に、操作モードと表示モードを切り替えるツールバーを重ねます。
		/// </summary>
		void DrawViewportOverlay()
		{
#ifdef USE_IMGUI
			if (!EditorModeController::GetInstance()->IsEditorModeEnabled())
			{
				return;
			}

			ApplyViewportVisualPolicy();
			DrawViewportToolbar();
			EditorTransformGizmo::GetInstance()->Draw(); // Phase 7のSelectionをWorld/Local Transform Gizmoへ接続する。
			EditorContentBrowserPanel::GetInstance()->UpdateViewportPicking(); // 軸や平面Handle上のクリックを除外した後に選択を予約する。
			DrawViewportAssetDropTarget();
			UpdateEditorCameraNavigation();
#endif
		}

	private:
		struct PlaceableEntry
		{
			const char* label;
			const char* searchKeywords;
			const char* description;
			EditorPlaceableType type;
		};

#ifdef USE_IMGUI
		enum class ToolbarGlyph
		{
			Save,
			Play,
			Pause,
			Stop,
			Build,
		};
#endif

		EditorShell() = default;
		~EditorShell() = default;
		EditorShell(const EditorShell&) = delete;
		EditorShell& operator=(const EditorShell&) = delete;

#ifdef USE_IMGUI
		static char ToLowerAscii(unsigned char character)
		{
			if (character >= 'A' && character <= 'Z')
			{
				return static_cast<char>(character - 'A' + 'a');
			}
			return static_cast<char>(character);
		}

		static bool ContainsCaseInsensitive(std::string_view text, std::string_view filter)
		{
			if (filter.empty())
			{
				return true;
			}

			std::string loweredText(text);
			std::string loweredFilter(filter);
			std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(), [](unsigned char c) { return ToLowerAscii(c); });
			std::transform(loweredFilter.begin(), loweredFilter.end(), loweredFilter.begin(), [](unsigned char c) { return ToLowerAscii(c); });
			return loweredText.find(loweredFilter) != std::string::npos;
		}

		static bool MatchesPlaceableFilter(const PlaceableEntry& entry, std::string_view filter)
		{
			// 日本語名と英語キーワードの両方を検索対象にして、従来の入力にも対応する。
			return ContainsCaseInsensitive(entry.label, filter) || ContainsCaseInsensitive(entry.searchKeywords, filter);
		}

		void ApplyViewportVisualPolicy()
		{
			const auto* viewportController = EditorViewportController::GetInstance();
			const bool drawEditorVisuals = EditorModeController::GetInstance()->ShouldDrawDebugVisuals() &&
				viewportController->IsEditorDisplay() && viewportController->IsAuxiliaryDisplayEnabled();
			// ゲーム表示ではワイヤー、コライダー、ライト範囲などのEditor補助描画をまとめて除外する。
			Wireframe::GetInstance()->SetDebugDrawEnabled(drawEditorVisuals);
		}

		void ReleaseEditorCameraCursor()
		{
			if (!cameraLookActive_)
			{
				return; // ゲーム入力中のカーソル状態をEditor Camera終了処理で上書きしない。
			}

			cameraLookActive_ = false;
			Input::GetInstance()->SetLockCursor(false);
			Input::GetInstance()->SetCursorVisible(true);
			ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow); // RMB終了時にImGui側のカーソル表示も通常状態へ戻す。
		}

		void UpdateEditorCameraNavigation()
		{
			EditorPlayController* playController = EditorPlayController::GetInstance();
			EditorViewportController* viewportController = EditorViewportController::GetInstance();
			CameraManager* cameraManager = CameraManager::GetInstance();
			const bool useEditorCamera = !playController->IsPlaying() && viewportController->IsEditorDisplay();
			cameraManager->SetUseDebugCamera(useEditorCamera);
			if (!useEditorCamera)
			{
				ReleaseEditorCameraCursor();
				return;
			}

			const EditorViewportRect& viewportRect = EditorWindowManager::GetInstance()->GetMainViewportRect();
			if (!viewportRect.valid || EditorTransformGizmo::GetInstance()->IsUsing() || ImGui::GetDragDropPayload())
			{
				if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
				{
					ReleaseEditorCameraCursor();
				}
				return;
			}

			if (viewportRect.isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				cameraLookActive_ = true;
			}
			if (cameraLookActive_ && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
			{
				ReleaseEditorCameraCursor();
			}

			const bool middlePan = viewportRect.isHovered && ImGui::IsMouseDown(ImGuiMouseButton_Middle);
			const bool wheelDolly = viewportRect.isHovered && std::abs(ImGui::GetIO().MouseWheel) > 0.001f;
			if (!cameraLookActive_ && !middlePan && !wheelDolly)
			{
				return;
			}

			Input* input = Input::GetInstance();
			const ImGuiIO& io = ImGui::GetIO();
			Vector3 localMove{};
			float pitchDelta = 0.0f;
			float yawDelta = 0.0f;

			if (cameraLookActive_)
			{
				input->SetGameInputEnabled(false);
				input->SetLockCursor(true);
				input->SetCursorVisible(false);
				ImGui::SetMouseCursor(ImGuiMouseCursor_None); // RMB中はImGuiがOSカーソルを再表示して点滅させないようにする。

				const float deltaTime = std::clamp(io.DeltaTime, 1.0f / 240.0f, 1.0f / 15.0f);
				const float moveSpeed = input->PushRawKey(DIK_LSHIFT) ? 24.0f : 8.0f;
				const float moveStep = moveSpeed * deltaTime;
				if (input->PushRawKey(DIK_W)) localMove.z += moveStep;
				if (input->PushRawKey(DIK_S)) localMove.z -= moveStep;
				if (input->PushRawKey(DIK_A)) localMove.x -= moveStep;
				if (input->PushRawKey(DIK_D)) localMove.x += moveStep;

				pitchDelta = io.MouseDelta.y * 0.003f;
				yawDelta = -io.MouseDelta.x * 0.003f;
			}

			if (middlePan)
			{
				localMove.x -= io.MouseDelta.x * 0.015f;
				localMove.y += io.MouseDelta.y * 0.015f;
			}
			if (wheelDolly)
			{
				localMove.z += io.MouseWheel * 2.0f;
			}

			DebugCamera* debugCamera = cameraManager->GetDebugCamera();
			if (debugCamera)
			{
				debugCamera->ApplyEditorNavigation(localMove, pitchDelta, yawDelta); // RMB視点操作とWASD移動をDebug Cameraへ集約する。
			}
		}

		bool DrawToolbarGlyphButton(const char* id, ToolbarGlyph glyph, bool enabled, const char* tooltip)
		{
			if (!enabled)
			{
				ImGui::BeginDisabled();
			}

			const ImVec2 buttonSize{ 28.0f, 24.0f };
			const bool clicked = ImGui::InvisibleButton(id, buttonSize);
			const ImVec2 minimum = ImGui::GetItemRectMin();
			const ImVec2 maximum = ImGui::GetItemRectMax();
			const ImVec2 center{ (minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f };
			const ImU32 color = ImGui::GetColorU32(enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled);
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			switch (glyph)
			{
			case ToolbarGlyph::Save:
				drawList->AddRect(ImVec2(center.x - 7.0f, center.y - 8.0f), ImVec2(center.x + 7.0f, center.y + 8.0f), color, 1.0f, 0, 1.8f);
				drawList->AddRectFilled(ImVec2(center.x - 4.5f, center.y - 7.0f), ImVec2(center.x + 3.0f, center.y - 2.0f), color, 0.5f);
				drawList->AddRect(ImVec2(center.x - 4.5f, center.y + 1.0f), ImVec2(center.x + 4.5f, center.y + 6.5f), color, 0.5f, 0, 1.4f);
				break;
			case ToolbarGlyph::Play:
				drawList->AddTriangleFilled(ImVec2(center.x - 5.0f, center.y - 7.0f), ImVec2(center.x - 5.0f, center.y + 7.0f), ImVec2(center.x + 7.0f, center.y), color);
				break;
			case ToolbarGlyph::Pause:
				// Pauseは文字記号を使わず、縦長の矩形を2本描画する。
				drawList->AddRectFilled(ImVec2(center.x - 6.0f, center.y - 7.0f), ImVec2(center.x - 2.0f, center.y + 7.0f), color, 0.8f);
				drawList->AddRectFilled(ImVec2(center.x + 2.0f, center.y - 7.0f), ImVec2(center.x + 6.0f, center.y + 7.0f), color, 0.8f);
				break;
			case ToolbarGlyph::Stop:
				drawList->AddRectFilled(ImVec2(center.x - 6.5f, center.y - 6.5f), ImVec2(center.x + 6.5f, center.y + 6.5f), color, 1.0f);
				break;
			case ToolbarGlyph::Build:
				drawList->AddLine(ImVec2(center.x - 6.0f, center.y + 7.0f), ImVec2(center.x + 4.0f, center.y - 3.0f), color, 3.0f);
				drawList->AddRectFilled(ImVec2(center.x + 1.0f, center.y - 7.0f), ImVec2(center.x + 8.0f, center.y - 1.0f), color, 1.0f);
				break;
			}

			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("%s", tooltip);
			}
			if (!enabled)
			{
				ImGui::EndDisabled();
			}
			return clicked && enabled;
		}

		void DrawPlaybackControls()
		{
			EditorPlayController* playController = EditorPlayController::GetInstance();
			EditorWindowManager* windowManager = EditorWindowManager::GetInstance();

			if (DrawToolbarGlyphButton("##SaveLevelIcon", ToolbarGlyph::Save, true, "レベル保存（Phase 10で接続）"))
			{
				windowManager->AddOutputLog(EditorLogLevel::Info, "レベル保存はPhase 10で接続します。");
			}
			ImGui::SameLine();

			const bool canStartOrResume = playController->IsEditing() || playController->IsPaused();
			if (DrawToolbarGlyphButton("##PlayIcon", ToolbarGlyph::Play, canStartOrResume, playController->IsPaused() ? "再開" : "再生"))
			{
				ReleaseEditorCameraCursor();
				playController->Play();
			}
			ImGui::SameLine();

			if (DrawToolbarGlyphButton("##PauseIcon", ToolbarGlyph::Pause, playController->IsPlaying(), "一時停止"))
			{
				playController->Pause();
				ReleaseEditorCameraCursor();
			}
			ImGui::SameLine();

			if (DrawToolbarGlyphButton("##StopIcon", ToolbarGlyph::Stop, !playController->IsEditing(), "停止"))
			{
				playController->Stop();
				ReleaseEditorCameraCursor();
			}
			ImGui::SameLine();

			const bool canBuild = !windowManager->IsAssetBuildRunning();
			if (DrawToolbarGlyphButton("##BuildAllIcon", ToolbarGlyph::Build, canBuild, canBuild ? "全アセットをビルド" : "アセットをビルド中"))
			{
				windowManager->StartAllAssetBuild(); // 旧Toolbarを隠しても従来のBuild操作を失わないようアイコンから実行する。
			}
		}

		void DrawViewportToolButton(const char* label, EditorViewportTool tool)
		{
			auto* viewportController = EditorViewportController::GetInstance();
			const bool selected = viewportController->GetTool() == tool;
			if (selected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			}

			if (ImGui::Button(label, ImVec2(54.0f, 0.0f)))
			{
				viewportController->SetTool(tool);
			}

			if (selected)
			{
				ImGui::PopStyleColor();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("現在の操作モード: %s\nQ: 選択 / W: 移動 / E: 回転 / R: 拡縮", label);
			}
		}

		void DrawViewportToolbar()
		{
			auto* windowManager = EditorWindowManager::GetInstance();
			const EditorViewportRect& viewportRect = windowManager->GetMainViewportRect();
			if (!viewportRect.valid)
			{
				return;
			}

			const float viewportWidth = viewportRect.screenMax.x - viewportRect.screenMin.x;
			if (viewportWidth < 430.0f)
			{
				return;
			}

			ImGui::SetNextWindowPos(ImVec2(viewportRect.screenMin.x + 8.0f, viewportRect.screenMin.y + 8.0f), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(viewportWidth - 16.0f, 42.0f), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.92f);

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoDocking |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav;

			if (ImGui::Begin(EditorPanelIds::ViewportToolbarOverlay, nullptr, flags))
			{
				DrawPlaybackControls();
				ImGui::SameLine();
				ImGui::TextDisabled("|");
				ImGui::SameLine();
				DrawViewportToolButton("選択", EditorViewportTool::Select);
				ImGui::SameLine();
				DrawViewportToolButton("移動", EditorViewportTool::Translate);
				ImGui::SameLine();
				DrawViewportToolButton("回転", EditorViewportTool::Rotate);
				ImGui::SameLine();
				DrawViewportToolButton("拡縮", EditorViewportTool::Scale);

				auto* viewportController = EditorViewportController::GetInstance();
				if (viewportWidth >= 650.0f)
				{
					ImGui::SameLine();
					ImGui::TextDisabled("|");
					ImGui::SameLine();
					if (ImGui::Button(viewportController->GetGizmoSpaceText(), ImVec2(58.0f, 0.0f)))
					{
						viewportController->ToggleGizmoSpace();
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Gizmoの軸をWorld / Localで切り替えます。\nScaleは常にLocal軸です。");
					}
					ImGui::SameLine();
					bool snapEnabled = viewportController->IsSnapEnabled();
					if (ImGui::Checkbox("Snap##ViewportSnap", &snapEnabled))
					{
						viewportController->SetSnapEnabled(snapEnabled);
					}
					if (snapEnabled && viewportWidth >= 760.0f)
					{
						ImGui::SameLine();
						ImGui::SetNextItemWidth(62.0f);
						if (viewportController->GetTool() == EditorViewportTool::Rotate)
						{
							ImGui::DragFloat("##RotationSnap", &viewportController->GetRotationSnapDegrees(), 1.0f, 0.1f, 180.0f, "%.1f deg");
						}
						else if (viewportController->GetTool() == EditorViewportTool::Scale)
						{
							ImGui::DragFloat("##ScaleSnap", &viewportController->GetScaleSnap(), 0.01f, 0.001f, 10.0f, "%.2f");
						}
						else
						{
							Vector3& translationSnap = viewportController->GetTranslationSnap();
							if (ImGui::DragFloat("##TranslationSnap", &translationSnap.x, 0.05f, 0.001f, 100.0f, "%.2f"))
							{
								translationSnap.y = translationSnap.x;
								translationSnap.z = translationSnap.x;
							}
						}
					}
				}

				if (viewportWidth >= 850.0f)
				{
					ImGui::SameLine();
					ImGui::TextDisabled("|");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(130.0f);
					if (ImGui::BeginCombo("##ビューポート表示", viewportController->GetDisplayModeText()))
					{
						const bool editorSelected = viewportController->IsEditorDisplay();
						if (ImGui::Selectable("エディター表示", editorSelected))
						{
							viewportController->SetDisplayMode(EditorViewportDisplayMode::Editor);
						}
						const bool gameSelected = viewportController->IsGameDisplay();
						if (ImGui::Selectable("ゲーム表示", gameSelected))
						{
							viewportController->SetDisplayMode(EditorViewportDisplayMode::Game);
						}
						ImGui::EndCombo();
					}
				}

				if (viewportWidth >= 1030.0f)
				{
					ImGui::SameLine();
					bool auxiliaryDisplayEnabled = viewportController->IsAuxiliaryDisplayEnabled();
					if (viewportController->IsGameDisplay()) ImGui::BeginDisabled();
					if (ImGui::Checkbox("補助表示##ViewportAuxiliary", &auxiliaryDisplayEnabled))
					{
						viewportController->SetAuxiliaryDisplayEnabled(auxiliaryDisplayEnabled);
					}
					if (viewportController->IsGameDisplay()) ImGui::EndDisabled();
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					{
						ImGui::SetTooltip("グリッド、コライダー、ライト範囲などのEditor補助描画を切り替えます。");
					}

					ImGui::SameLine();
					bool performanceVisible = windowManager->IsPerformanceOverlayVisible();
					if (ImGui::Checkbox("統計##ViewportStats", &performanceVisible))
					{
						windowManager->SetPerformanceOverlayVisible(performanceVisible);
					}
				}
			}
			ImGui::End();
		}

		/// <summary>
		/// Content Browserからドラッグ中のアセットをMain Viewportで受け取り、配置位置へ生成します。
		/// </summary>
		void DrawViewportAssetDropTarget()
		{
			const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
			if (!activePayload || !activePayload->IsDataType(kEditorAssetDragDropPayloadType))
			{
				return;
			}

			auto* windowManager = EditorWindowManager::GetInstance();
			const EditorViewportRect& viewportRect = windowManager->GetMainViewportRect();
			const float viewportWidth = viewportRect.screenMax.x - viewportRect.screenMin.x;
			const float viewportHeight = viewportRect.screenMax.y - viewportRect.screenMin.y;
			if (!viewportRect.valid || viewportWidth <= 1.0f || viewportHeight <= 1.0f)
			{
				return;
			}

			ImGui::SetNextWindowPos(ImVec2(viewportRect.screenMin.x, viewportRect.screenMin.y), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(viewportWidth, viewportHeight), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.0f);
			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoDocking |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav |
				ImGuiWindowFlags_NoBackground;

			if (ImGui::Begin("##MainViewportAssetDropTarget", nullptr, flags))
			{
				ImGui::InvisibleButton("##ViewportAssetDropArea", ImVec2(viewportWidth, viewportHeight));
				if (ImGui::BeginDragDropTarget())
				{
					const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
						kEditorAssetDragDropPayloadType,
						ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
					if (payload && payload->DataSize == sizeof(EditorAssetDragDropPayload))
					{
						const bool canPlace = !EditorPlayController::GetInstance()->IsPlaying();
						const ImU32 borderColor = ImGui::GetColorU32(
							canPlace ? ImVec4(0.95f, 0.62f, 0.12f, 1.0f) : ImVec4(0.95f, 0.24f, 0.20f, 1.0f));
						ImDrawList* drawList = ImGui::GetWindowDrawList();
						drawList->AddRect(
							ImGui::GetItemRectMin(),
							ImGui::GetItemRectMax(),
							borderColor,
							2.0f,
							0,
							4.0f);

						const char* guideText = canPlace
							? "ここにドロップしてアセットを配置"
							: "再生中はアセットを配置できません";
						const ImVec2 textSize = ImGui::CalcTextSize(guideText);
						const ImVec2 itemMin = ImGui::GetItemRectMin();
						const ImVec2 itemMax = ImGui::GetItemRectMax();
						drawList->AddText(
							ImVec2(
								(itemMin.x + itemMax.x - textSize.x) * 0.5f,
								(itemMin.y + itemMax.y - textSize.y) * 0.5f),
							borderColor,
							guideText);

						if (payload->IsDelivery())
						{
							if (!canPlace)
							{
								windowManager->AddOutputLog(EditorLogLevel::Warning, "再生中はビューポートへアセットを配置できません。");
							}
							else
							{
								const ImVec2 mouse = ImGui::GetMousePos();
								Vector3 worldPosition{};
								const bool positionResolved = EditorAssetPlacementService::CalculateDropPosition(
									{ mouse.x, mouse.y },
									viewportRect.screenMin,
									viewportRect.imageSize,
									worldPosition);
								if (!positionResolved)
								{
									windowManager->AddOutputLog(EditorLogLevel::Error, "ドロップ位置をワールド座標へ変換できませんでした。");
								}
								else
								{
									const auto* assetPayload = static_cast<const EditorAssetDragDropPayload*>(payload->Data);
									const EditorAssetPlacementResult result = EditorAssetPlacementService::PlaceAsset(
										windowManager->GetSceneManager(),
										*assetPayload,
										worldPosition);
									windowManager->AddOutputLog(
										result.succeeded ? EditorLogLevel::Info : EditorLogLevel::Error,
										result.message);
								}
							}
						}
					}
					ImGui::EndDragDropTarget();
				}
			}
			ImGui::End();
		}

		void DrawPlaceableCategory(const char* categoryName, const PlaceableEntry* entries, std::size_t count)
		{
			// ImGuiの検索入力バッファをnull終端文字列としてstring_viewへ変換する。
			const std::string_view searchFilter{ placeActorsSearch_.data() };

			bool hasVisibleEntry = false;
			for (std::size_t index = 0; index < count; ++index)
			{
				hasVisibleEntry |= MatchesPlaceableFilter(entries[index], searchFilter);
			}

			if (!hasVisibleEntry)
			{
				return;
			}

			if (!ImGui::CollapsingHeader(categoryName, ImGuiTreeNodeFlags_DefaultOpen))
			{
				return;
			}

			for (std::size_t index = 0; index < count; ++index)
			{
				const PlaceableEntry& entry = entries[index];
				if (!MatchesPlaceableFilter(entry, searchFilter))
				{
					continue;
				}

				ImGui::PushID(entry.searchKeywords);
				if (ImGui::Button(entry.label, ImVec2(-1.0f, 34.0f)))
				{
					EditorContext::GetInstance()->QueuePlacement(entry.type, entry.label);
					// Phase 1では配置要求だけを保存し、実際の生成は後続Phaseで接続する。
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", entry.description);
				}
				ImGui::PopID();
			}
		}

		void DrawPlaceActors()
		{
			constexpr std::array<PlaceableEntry, 4> basicEntries = {
				PlaceableEntry{ "空のアクタ", "Empty Actor", "ルートSceneComponentだけを持つアクタを作成します。", EditorPlaceableType::EmptyActor },
				PlaceableEntry{ "キューブ", "Cube", "キューブモデルを持つアクタを作成します。", EditorPlaceableType::Cube },
				PlaceableEntry{ "スフィア", "Sphere", "球体モデルを持つアクタを作成します。", EditorPlaceableType::Sphere },
				PlaceableEntry{ "平面", "Plane", "平面モデルを持つアクタを作成します。", EditorPlaceableType::Plane },
			};
			constexpr std::array<PlaceableEntry, 3> lightEntries = {
				PlaceableEntry{ "ディレクショナルライト", "Directional Light", "平行光源のLightComponentを持つアクタを作成します。", EditorPlaceableType::DirectionalLight },
				PlaceableEntry{ "ポイントライト", "Point Light", "ポイント光源のLightComponentを持つアクタを作成します。", EditorPlaceableType::PointLight },
				PlaceableEntry{ "スポットライト", "Spot Light", "スポット光源のLightComponentを持つアクタを作成します。", EditorPlaceableType::SpotLight },
			};
			constexpr std::array<PlaceableEntry, 2> volumeEntries = {
				PlaceableEntry{ "トリガーボックス", "Trigger Box", "Overlap専用のBox Triggerアクタを作成します。", EditorPlaceableType::TriggerBox },
				PlaceableEntry{ "トリガースフィア", "Trigger Sphere", "Overlap専用のSphere Triggerアクタを作成します。", EditorPlaceableType::TriggerSphere },
			};

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
			if (ImGui::Begin(EditorPanelIds::PlaceActors, nullptr, flags))
			{
				ImGui::TextUnformatted("アクタを配置");
				ImGui::TextDisabled("配置する種類を選択してください。実際の配置は後続Phaseで接続します。");
				ImGui::Separator();
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputTextWithHint("##PlaceActorsSearch", "クラスを検索...", placeActorsSearch_.data(), placeActorsSearch_.size());
				ImGui::Spacing();

				DrawPlaceableCategory("基本", basicEntries.data(), basicEntries.size());
				DrawPlaceableCategory("ライト", lightEntries.data(), lightEntries.size());
				DrawPlaceableCategory("ボリューム", volumeEntries.data(), volumeEntries.size());

				const EditorPlacementRequest& request = EditorContext::GetInstance()->GetPlacementRequest();
				if (request.pending)
				{
					ImGui::Separator();
					ImGui::Text("選択中: %s", request.displayName.c_str());
					if (ImGui::Button("配置をキャンセル", ImVec2(-1.0f, 0.0f)))
					{
						EditorContext::GetInstance()->ClearPlacementRequest();
					}
				}
			}
			ImGui::End();
		}
#endif

		std::array<char, 96> placeActorsSearch_{};
		bool cameraLookActive_ = false;
	};
} // namespace Ken4lowEngine
