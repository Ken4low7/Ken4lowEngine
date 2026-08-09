#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include <cstdint>
#include <unordered_map>

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#endif // USE_IMGUI

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class WinApp;
	class DirectXCommon;

	/// -------------------------------------------------------------
	///						ImGui管理クラス
	/// -------------------------------------------------------------
	class ImGuiManager
	{
	public:	/// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// ImGuiManager のシングルトンインスタンスを取得します。
		/// </summary>
		/// <returns>ImGuiManager の唯一のインスタンス。</returns>
		static ImGuiManager* GetInstance();

		/// <summary>
		/// ImGui の初期化処理を行います。<br/>
		/// ・ImGui コンテキストの作成<br/>
		/// ・SRVManager を使う DX12 用 SRV 確保コールバックの登録<br/>
		/// ・Win32 / DX12 バックエンドの初期化<br/>
		/// を行い、ImGui を使える状態にします。
		/// </summary>
		/// <param name="winApp">ウィンドウハンドル取得に使用する WinApp。</param>
		/// <param name="dxCommon">デバイスやスワップチェーンなどを取得する DirectXCommon。</param>
		void Initialize(WinApp* winApp, DirectXCommon* dxCommon);

		/// <summary>
		/// 1 フレーム分の ImGui の開始処理を行います。<br/>
		/// ・ImGui_ImplDX12_NewFrame()<br/>
		/// ・ImGui_ImplWin32_NewFrame()<br/>
		/// ・ImGui::NewFrame()<br/>
		/// ・DrawDockSpace()<br/>
		/// を呼び出し、このあと ImGui ウィジェットの記述が行える状態にします。
		/// </summary>
		void BeginFrame();

		/// <summary>
		/// メイン ImGui Viewport 全体に DockSpace を描画します。<br/>
		/// DirectX12 の D3D12_VIEWPORT と混同しないよう、ImGui 側の Viewport に限定した処理です。
		/// </summary>
		void DrawDockSpace();

		/// <summary>
		/// 1 フレーム分の ImGui の終了処理を行います。<br/>
		/// ・ImGui::Render() を呼び出し、内部の描画コマンドリストを生成します。<br/>
		/// 実際の描画は Draw() で行います。
		/// </summary>
		void EndFrame();

		/// <summary>
		/// ImGui の描画処理を行います。<br/>
		/// ・SRVManager::PreDraw() で SRV ヒープをセット<br/>
		/// ・ImGui_ImplDX12_RenderDrawData() で ImGui の描画コマンドをコマンドリストに積む<br/>
		/// という処理を行います。<br/>
		/// DirectX の通常の描画コマンドの後や、ポストエフェクトの後に呼び出す想定です。
		/// </summary>
		void Draw();

		/// <summary>
		/// 次フレームでDockBuilderの初期レイアウトを再適用します。
		/// </summary>
		void RequestResetDockLayout();

		/// <summary>
		/// Win32 メッセージを ImGui に渡します。<br/>
		/// WinApp が ImGui バックエンドへ直接依存しないようにするための窓口です。
		/// </summary>
		bool ProcessWin32Message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

		/// <summary>
		/// ImGui の終了処理を行います。<br/>
		/// ・DX12 バックエンド経由で確保した SRV の解放<br/>
		/// ・DX12 / Win32 バックエンドのシャットダウン<br/>
		/// ・ImGui コンテキストの破棄<br/>
		/// を行います。
		/// </summary>
		void Finalize();

	private: /// ---------- メンバ関数 ---------- ///

#ifdef USE_IMGUI
		// ImGuiバックエンドが要求するSRVディスクリプタをSRVManager経由で確保する
		static void AllocateImGuiSrvDescriptor(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle);
		static void FreeImGuiSrvDescriptor(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);
		// DockBuilderで初期配置するウィンドウ名を一箇所に集約する
		void SetupDefaultDockLayout(ImGuiID dockspaceId, const ImGuiViewport* viewport);
#endif // USE_IMGUI

		bool initialized_ = false;

#ifdef USE_IMGUI
		std::unordered_map<SIZE_T, uint32_t> imguiSrvHandleToIndex_;
		bool dockLayoutInitialized_ = false; // 初期DockBuilder配置を毎フレーム上書きしないためのフラグ
		bool shouldApplyDefaultDockLayout_ = false; // 保存済みiniが無い場合だけ初期DockBuilder配置を適用するフラグ
		bool resetDockLayoutRequested_ = false; // WindowメニューからのReset Layout要求を次フレームへ渡すフラグ
#endif // USE_IMGUI

	private: /// ---------- コンストラクタ・デストラクタ ---------- ///

		/// <summary>
		/// 外部からの生成を禁止するためのプライベートコンストラクタ。<br/>
		/// シングルトンとして利用します。
		/// </summary>
		ImGuiManager() = default;

		/// <summary>
		/// デフォルトデストラクタ。
		/// </summary>
		~ImGuiManager() = default;

		/// <summary>
		/// コピーコンストラクタは使用禁止です。
		/// </summary>
		ImGuiManager(const ImGuiManager&) = delete;

		/// <summary>
		/// 代入演算子は使用禁止です。
		/// </summary>
		const ImGuiManager& operator=(const ImGuiManager&) = delete;
	};


} // namespace Ken4lowEngine
