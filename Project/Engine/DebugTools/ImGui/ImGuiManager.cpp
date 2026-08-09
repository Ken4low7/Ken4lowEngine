#include "ImGuiManager.h"

#include "WinApp.h"
#include "DirectXCommon.h"
#include "SRVManager.h"
#include "../../Editor/EditorPanelIds.h"
#include "../../Editor/EditorShell.h"

#include <cassert>
#include <fstream>
#include <stdexcept>

#ifdef USE_IMGUI
// DockBuilder APIを使って初期Docking配置を組むため内部ヘッダーを参照する
#include <imgui_internal.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	constexpr const char* kEditorLayoutIniFilename = "ken4low_editor_layout_v2.ini";

	bool FileExists(const char* path)
	{
		if (path == nullptr)
		{
			return false;
		}

		std::ifstream file(path, std::ios::binary);
		return file.good();
	}

	void ApplyUnrealInspiredStyle()
	{
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowPadding = ImVec2(8.0f, 8.0f);
		style.FramePadding = ImVec2(8.0f, 4.0f);
		style.CellPadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(6.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(5.0f, 4.0f);
		style.IndentSpacing = 18.0f;
		style.ScrollbarSize = 13.0f;
		style.GrabMinSize = 10.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.WindowRounding = 0.0f;
		style.ChildRounding = 2.0f;
		style.FrameRounding = 2.0f;
		style.PopupRounding = 2.0f;
		style.ScrollbarRounding = 3.0f;
		style.GrabRounding = 2.0f;
		style.TabRounding = 2.0f;
		style.WindowMenuButtonPosition = ImGuiDir_Right;

		ImVec4* colors = style.Colors;
		colors[ImGuiCol_Text] = ImVec4(0.88f, 0.89f, 0.91f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.46f, 0.48f, 0.52f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.060f, 0.070f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.055f, 0.060f, 0.070f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.070f, 0.075f, 0.085f, 0.98f);
		colors[ImGuiCol_Border] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.090f, 0.100f, 0.120f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.140f, 0.160f, 0.190f, 1.00f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.180f, 0.210f, 0.250f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.040f, 0.045f, 0.055f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.060f, 0.068f, 0.082f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.040f, 0.045f, 0.055f, 1.00f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.045f, 0.050f, 0.060f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.030f, 0.034f, 0.042f, 1.00f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.180f, 0.190f, 0.220f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.260f, 0.280f, 0.320f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.330f, 0.350f, 0.400f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.95f, 0.53f, 0.08f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.78f, 0.39f, 0.06f, 1.00f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.58f, 0.10f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.110f, 0.120f, 0.145f, 1.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.190f, 0.210f, 0.250f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.65f, 0.32f, 0.04f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.120f, 0.140f, 0.170f, 1.00f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.200f, 0.230f, 0.280f, 1.00f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.290f, 0.240f, 0.160f, 1.00f);
		colors[ImGuiCol_Separator] = ImVec4(0.180f, 0.190f, 0.220f, 1.00f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.85f, 0.46f, 0.08f, 1.00f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(1.00f, 0.58f, 0.10f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.31f, 0.34f, 0.35f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.85f, 0.46f, 0.08f, 0.75f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 0.58f, 0.10f, 0.95f);
		colors[ImGuiCol_TableHeaderBg] = ImVec4(0.090f, 0.100f, 0.120f, 1.00f);
		colors[ImGuiCol_TableBorderStrong] = ImVec4(0.180f, 0.190f, 0.220f, 1.00f);
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.120f, 0.130f, 0.150f, 1.00f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.025f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.78f, 0.39f, 0.06f, 0.45f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 0.58f, 0.10f, 0.95f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.95f, 0.53f, 0.08f, 0.90f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.55f);
		// 色と余白をEditor全体で一度だけ設定し、各Panelが独自Styleを持たないようにする。
	}
}
#endif // USE_IMGUI


namespace Ken4lowEngine
{

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

	using namespace Microsoft::WRL;

	/// -------------------------------------------------------------
	///					シングルトンインスタンス
	/// -------------------------------------------------------------
	ImGuiManager* ImGuiManager::GetInstance()
	{
		static ImGuiManager instance;
		return &instance;
	}


	/// -------------------------------------------------------------
	///							初期化処理
	/// -------------------------------------------------------------
	void ImGuiManager::Initialize(WinApp* winApp, DirectXCommon* dxCommon)
	{
#ifdef USE_IMGUI
		// 初期化順の不整合を起動時に明示的なエラーとして検出する
		if (initialized_)
		{
			throw std::runtime_error("ImGuiManager is already initialized");
		}
		if (winApp == nullptr || dxCommon == nullptr || dxCommon->GetDevice() == nullptr || dxCommon->GetCommandManager() == nullptr || dxCommon->GetCommandManager()->GetCommandQueue() == nullptr)
		{
			throw std::runtime_error("Invalid arguments for ImGuiManager::Initialize");
		}

		auto* srvManager = SRVManager::GetInstance();
		if (srvManager->GetDescriptorHeap() == nullptr)
		{
			throw std::runtime_error("SRV descriptor heap is not initialized for ImGuiManager");
		}

#pragma region ImGuiの初期化を行いDirectX12とWindowsAPIを使ってImGuiをセットアップする
		IMGUI_CHECKVERSION();						  // ImGuiのバージョンチェック
		ImGui::CreateContext();						  // ImGuiコンテキストの作成

		ImGuiIO& io = ImGui::GetIO();				  // ImGuiIOへの参照を取得

		// Phase 1の新しいUE風配置を旧iniと分離し、初回だけ確実にDefault Layoutを構築する。
		io.IniFilename = kEditorLayoutIniFilename;
		shouldApplyDefaultDockLayout_ = !FileExists(io.IniFilename);
		resetDockLayoutRequested_ = false;
		dockLayoutInitialized_ = false;

		// フォントの設定
		io.Fonts->AddFontFromFileTTF("Resources/Fonts/NotoSansJP-VariableFont_wght.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese()); // 日本語フォントの追加

		// ImGuiウィンドウをエンジンのメインウィンドウ内でドッキングできるようにする
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		// 今回は別OSウィンドウ化を避けるため Multi-Viewport は明示的に無効のままにする
		io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
		io.ConfigWindowsMoveFromTitleBarOnly = true;

		ApplyUnrealInspiredStyle();
		const bool win32Initialized = ImGui_ImplWin32_Init(winApp->GetHwnd()); // Win32バックエンド初期化の失敗を起動時に検出する
		assert(win32Initialized && "ImGui_ImplWin32_Init failed");
		if (!win32Initialized)
		{
			ImGui::DestroyContext();
			throw std::runtime_error("ImGui_ImplWin32_Init failed");
		}

		ImGui_ImplDX12_InitInfo dx12InitInfo{};
		dx12InitInfo.Device = dxCommon->GetDevice();
		dx12InitInfo.CommandQueue = dxCommon->GetCommandManager()->GetCommandQueue();
		dx12InitInfo.NumFramesInFlight = static_cast<int>(dxCommon->GetSwapChainDesc().BufferCount);
		dx12InitInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		dx12InitInfo.SrvDescriptorHeap = srvManager->GetDescriptorHeap();
		dx12InitInfo.SrvDescriptorAllocFn = AllocateImGuiSrvDescriptor;
		dx12InitInfo.SrvDescriptorFreeFn = FreeImGuiSrvDescriptor;
		dx12InitInfo.UserData = this;

		const bool dx12Initialized = ImGui_ImplDX12_Init(&dx12InitInfo); // 1.92以降の動的フォントテクスチャ更新に対応したDX12初期化を行う
		assert(dx12Initialized && "ImGui_ImplDX12_Init failed");
		if (!dx12Initialized)
		{
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
			throw std::runtime_error("ImGui_ImplDX12_Init failed");
		}

		initialized_ = true;
#pragma endregion
#else
		(void)winApp;
		(void)dxCommon;
#endif // USE_IMGUI
	}

#ifdef USE_IMGUI
	/// -------------------------------------------------------------
	///					ImGui用SRVディスクリプタ確保
	/// -------------------------------------------------------------
	void ImGuiManager::AllocateImGuiSrvDescriptor(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
	{
		if (info == nullptr || info->UserData == nullptr || outCpuHandle == nullptr || outGpuHandle == nullptr)
		{
			throw std::runtime_error("Invalid ImGui DX12 SRV allocation request");
		}

		auto* self = static_cast<ImGuiManager*>(info->UserData);
		auto* srvManager = SRVManager::GetInstance();
		const uint32_t srvIndex = srvManager->Allocate();
		*outCpuHandle = srvManager->GetCPUDescriptorHandle(srvIndex);
		*outGpuHandle = srvManager->GetGPUDescriptorHandle(srvIndex);

		// フォントを含むImGuiテクスチャ用SRVが有効なヒープ上に確保されたことを記録する
		assert(outCpuHandle->ptr != 0 && outGpuHandle->ptr != 0);
		self->imguiSrvHandleToIndex_[outCpuHandle->ptr] = srvIndex;
	}

	/// -------------------------------------------------------------
	///					ImGui用SRVディスクリプタ解放
	/// -------------------------------------------------------------
	void ImGuiManager::FreeImGuiSrvDescriptor(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
	{
		if (info == nullptr || info->UserData == nullptr || cpuHandle.ptr == 0 || gpuHandle.ptr == 0)
		{
			throw std::runtime_error("Invalid ImGui DX12 SRV free request");
		}

		auto* self = static_cast<ImGuiManager*>(info->UserData);
		auto it = self->imguiSrvHandleToIndex_.find(cpuHandle.ptr);
		if (it == self->imguiSrvHandleToIndex_.end())
		{
			throw std::runtime_error("Unknown ImGui DX12 SRV descriptor handle");
		}

		// ImGuiバックエンドから返却されたフォント/テクスチャ用SRVをSRVManagerへ戻す
		SRVManager::GetInstance()->Free(it->second);
		self->imguiSrvHandleToIndex_.erase(it);
	}
#endif // USE_IMGUI




	/// -------------------------------------------------------------
	///						フレーム開始処理
	/// -------------------------------------------------------------
	void ImGuiManager::BeginFrame()
	{
#ifdef USE_IMGUI
		// DX12/Win32バックエンド初期化前のNewFrame呼び出しを防ぐ
		if (!initialized_)
		{
			throw std::runtime_error("ImGuiManager::BeginFrame called before Initialize");
		}

		// ImGuiバックエンドとコンテキストのフレーム開始をManagerに集約する
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		DrawDockSpace();
		EditorShell::GetInstance()->Draw(); // 常設ShellパネルはScene固有UIより先に登録してDock先を安定させる。
#endif // USE_IMGUI
	}



	/// -------------------------------------------------------------
	///						DockSpace描画処理
	/// -------------------------------------------------------------
	void ImGuiManager::DrawDockSpace()
	{
#ifdef USE_IMGUI
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		// 中央ノードへMain Viewportを置きつつ、Editor全体を一つのDockSpaceとして管理する。
		const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

		if (shouldApplyDefaultDockLayout_ || resetDockLayoutRequested_)
		{
			// 保存済みiniが無い初回起動、または明示リセット時だけDockBuilderで初期配置を作る
			SetupDefaultDockLayout(dockspaceId, viewport);
			dockLayoutInitialized_ = true;
			shouldApplyDefaultDockLayout_ = false;
			resetDockLayoutRequested_ = false;
		}
#endif // USE_IMGUI
	}

#ifdef USE_IMGUI
	void ImGuiManager::SetupDefaultDockLayout(ImGuiID dockspaceId, const ImGuiViewport* viewport)
	{
		if (dockspaceId == 0 || viewport == nullptr)
		{
			return;
		}

		ImGuiID centerNode = dockspaceId;
		ImGuiID toolbarNode = 0;
		ImGuiID leftNode = 0;
		ImGuiID rightNode = 0;
		ImGuiID bottomNode = 0;
		ImGuiID outlinerNode = 0;
		ImGuiID detailsNode = 0;

		ImGui::DockBuilderRemoveNodeChildNodes(dockspaceId);
		ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);
		ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Up, 0.075f, &toolbarNode, &centerNode);
		ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Left, 0.18f, &leftNode, &centerNode);
		ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Right, 0.26f, &rightNode, &centerNode);
		ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Down, 0.31f, &bottomNode, &centerNode);
		ImGui::DockBuilderSplitNode(rightNode, ImGuiDir_Down, 0.58f, &detailsNode, &outlinerNode);

		// UEの基本配置に合わせ、中央Viewport・左配置・右階層/詳細・下Asset/Logへ分ける。
		ImGui::DockBuilderDockWindow(EditorPanelIds::Toolbar, toolbarNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::PlaceActors, leftNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::MainViewport, centerNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::Scene, centerNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::WorldOutliner, outlinerNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::Details, detailsNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::ContentBrowser, bottomNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::OutputLog, bottomNode);

		ImGui::DockBuilderDockWindow(EditorPanelIds::PostEffectSettings, detailsNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::LightEditor, detailsNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::Parameters, detailsNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::Display, detailsNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::JsonAssetManager, detailsNode);

		ImGui::DockBuilderDockWindow("Player Debug", bottomNode);
		ImGui::DockBuilderDockWindow("Weapon Debug", bottomNode);
		ImGui::DockBuilderDockWindow("Enemy Debug", bottomNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::GameDebug, bottomNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::CollisionDebug, bottomNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::CullingDebug, bottomNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::PhysicsWorldDebug, bottomNode);
		ImGui::DockBuilderDockWindow(EditorPanelIds::GpuParticleEditor, bottomNode);
		ImGui::DockBuilderFinish(dockspaceId);
	}
#endif // USE_IMGUI


	/// -------------------------------------------------------------
	///					Dockingレイアウトリセット要求
	/// -------------------------------------------------------------
	void ImGuiManager::RequestResetDockLayout()
	{
#ifdef USE_IMGUI
		// Windowメニューからの操作で次フレームに初期DockBuilder配置を再適用する
		resetDockLayoutRequested_ = true;
		shouldApplyDefaultDockLayout_ = false;
		dockLayoutInitialized_ = false;
#endif // USE_IMGUI
	}


	/// -------------------------------------------------------------
	///						フレーム終了処理
	/// -------------------------------------------------------------
	void ImGuiManager::EndFrame()
	{
#ifdef USE_IMGUI
		// ImGuiの内部描画コマンド生成をManagerに集約する
		ImGui::Render();
#endif // USE_IMGUI
	}



	/// -------------------------------------------------------------
	///						描画開始処理
	/// -------------------------------------------------------------
	void ImGuiManager::Draw()
	{
#ifdef USE_IMGUI
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();

		ComPtr<ID3D12GraphicsCommandList> commandList = dxCommon->GetCommandManager()->GetCommandList();

		// ImGui::Imageが読むGameRenderTargetはSRVのまま維持し、
		// ImGui本体の描画先だけをBackBuffer RTVへ明示的に戻す。
		dxCommon->PrepareBackBufferForImGui();

		// ImGui描画に必要なSRVヒープ設定をManagerに集約する
		SRVManager::GetInstance()->PreDraw();

		// 実際のcommandListにImGuiの描画コマンドをBackBufferへ積む処理をManagerに集約する
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
#endif // USE_IMGUI
	}



	/// -------------------------------------------------------------
	///					Win32メッセージ処理
	/// -------------------------------------------------------------
	bool ImGuiManager::ProcessWin32Message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
#ifdef USE_IMGUI
		// Win32バックエンド固有のメッセージ処理をManagerに集約する
		return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam) != 0;
#else
		(void)hwnd;
		(void)msg;
		(void)wparam;
		(void)lparam;
		return false;
#endif // USE_IMGUI
	}


	/// -------------------------------------------------------------
	///						終了処理
	/// -------------------------------------------------------------
	void ImGuiManager::Finalize()
	{
#ifdef USE_IMGUI
		if (!initialized_)
		{
			return;
		}

		// DestroyContext前にiniへ書き出して、終了直前のDockingレイアウトを確実に残す
		if (ImGui::GetIO().IniFilename != nullptr)
		{
			ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
		}

		// ImGuiバックエンドとコンテキストの終了処理をManagerに集約する
		ImGui_ImplDX12_Shutdown();
		// DX12バックエンドから返却されなかったSRVがあればFinalize時に回収する
		for (const auto& srvEntry : imguiSrvHandleToIndex_)
		{
			SRVManager::GetInstance()->Free(srvEntry.second);
		}
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		imguiSrvHandleToIndex_.clear();
		initialized_ = false;
		dockLayoutInitialized_ = false;
		shouldApplyDefaultDockLayout_ = false;
		resetDockLayoutRequested_ = false;
#endif // USE_IMGUI
	}


} // namespace Ken4lowEngine
