#pragma once
#include <Windows.h>
#include <DisplaySettings.h>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				WindowsAPI - ウィンドウズ作成クラス
	/// -------------------------------------------------------------
	class WinApp
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// WinApp のシングルトンインスタンスを取得します。
		/// </summary>
		/// <returns>WinApp の唯一のインスタンス。</returns>
		static WinApp* GetInstance();

		/// <summary>
		/// 
		/// </summary>
		/// <param name="settings"></param>
		void CreateMainWindow(const DisplaySettings& settings);

		/// <summary>
		/// 終了処理を行います。<br/>
		/// ・ウィンドウの破棄（CloseWindow）<br/>
		/// ・COM ライブラリの終了処理（CoUninitialize）<br/>
		/// を実行します。
		/// </summary>
		void Finalize();

		/// <summary>
		/// Windows メッセージを処理します。<br/>
		/// PeekMessage / TranslateMessage / DispatchMessage を使ってメッセージループを回し、<br/>
		/// WM_QUIT が検出されたら true を返します。<br/>
		/// メインループ側では、この戻り値が true になったらゲームループを抜けるようにします。
		/// </summary>
		/// <returns>アプリ終了リクエスト（WM_QUIT）が来たら true、それ以外は false。</returns>
		bool ProcessMessage();

		/// <summary>
		/// ウィンドウのリサイズ要求を取得します。<br/>
		/// リサイズ要求があれば true を返し、引数 outWidth / outHeight に新しいクライアントサイズを設定します。<br/>
		/// リサイズ要求がなければ false を返します。
		/// </summary>
		/// <param name="outWidth">新しいクライアント領域の幅を受け取る参照。</param>
		/// <param name="outHeight">新しいクライアント領域の高さを受け取る参照。</param>
		/// <returns>リサイズ要求があれば true、それ以外は false。</returns>
		bool ConsumeResize(uint32_t& outWidth, uint32_t& outHeight);

		// 画面設定の変更を予約（UI側から呼ぶ）
		void RequestDisplaySettings(const DisplaySettings& settings);

		// 予約されていたら取り出す（メインループ先頭で呼ぶ）
		bool ConsumeDisplaySettings(DisplaySettings& out);

		// 実際にウィンドウへ適用（Win32）
		bool ApplyDisplaySettings(const DisplaySettings& settings);

		// ImGuiで表示する設定UI
		void DrawDisplaySettingsImGui(bool* pOpen = nullptr);

		// Alt+Enter トグル要求
		void RequestToggleFullscreen();
		bool ConsumeToggleFullscreen();

		// 現在の表示設定
		const DisplaySettings& GetCurrentDisplaySettings() const { return currentDisplaySettings_; }

		// Windowed を保存しておく（Borderlessから戻す用）
		void RememberWindowedSettings(const DisplaySettings& s);
		DisplaySettings GetLastWindowedSettingsOrDefault() const;

		void ToggleWindowResizable();
		void SetWindowResizable(bool enable);
		bool IsWindowResizable() const { return windowedResizable_; }

	public: /// ---------- アクセッサ ---------- ///

		/// <summary>
		/// 作成されたウィンドウの HWND を取得します。<br/>
		/// DirectX のスワップチェーン生成などに使用します。
		/// </summary>
		HWND GetHwnd() const { return hwnd; }

		/// <summary>
		/// ウィンドウクラス登録時に使用したインスタンスハンドル (HINSTANCE) を取得します。
		/// </summary>
		HINSTANCE GetHInstance() const { return wc.hInstance; }

		/// <summary>
		/// クライアント領域の幅を取得します。
		/// </summary>
		/// <returns>クライアント領域の幅。</returns>
		uint32_t GetClientWidth() const { return clientWidth_; }

		/// <summary>
		/// クライアント領域の高さを取得します。
		/// </summary>
		/// <returns>クライアント領域の高さ。</returns>
		uint32_t GetClientHeight() const { return clientHeight_; }

		/// <summary>
		/// クライアント領域サイズの既定値です。
		/// DisplaySettings の既定解像度を参照することで、WinApp 内での解像度直書きを防ぎます。
		/// </summary>
		static inline constexpr UINT32 kClientWidth = DisplaySettings::kDefaultResolution.width;

		/// <summary>
		/// クライアント領域の高さの既定値です。
		/// DisplaySettings の既定解像度を参照することで、WinApp 内での解像度直書きを防ぎます。
		/// </summary>
		static inline constexpr UINT32 kClientHeight = DisplaySettings::kDefaultResolution.height;

	private: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// ウィンドウプロシージャ。<br/>
		/// ・WM_CLOSE：DestroyWindow を呼び出してウィンドウ破棄<br/>
		/// ・WM_DESTROY：PostQuitMessage(0) を呼び出してアプリ終了を通知<br/>
		/// ・それ以外：DefWindowProc に処理を委譲<br/>
		/// ImGui 使用時は、先に ImGuiManager 経由でメッセージを渡します。
		/// </summary>
		/// <param name="hwnd">対象ウィンドウのハンドル。</param>
		/// <param name="msg">メッセージ ID。</param>
		/// <param name="wparam">メッセージ固有の追加情報。</param>
		/// <param name="lparam">メッセージ固有の追加情報。</param>
		/// <returns>メッセージ処理の結果。</returns>
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	private: /// ---------- メンバ変数 ---------- ///

		// ウィンドウハンドル
		HWND hwnd = nullptr;

		// ウィンドウクラスの設定
		WNDCLASS wc{};

		uint32_t clientWidth_ = kClientWidth;
		uint32_t clientHeight_ = kClientHeight;

		bool resizePending_ = false;
		bool inSizeMove_ = false;

		DisplaySettings currentDisplaySettings_{};
		DisplaySettings pendingDisplaySettings_{};
		bool displaySettingsPending_ = false;

		// Windowed ⇄ Borderless の復元用
		RECT savedWindowRect_{};
		DWORD savedStyle_ = 0;
		DWORD savedExStyle_ = 0;
		bool hasSavedWindowed_ = false;

		bool toggleFullscreenPending_ = false;

		DisplaySettings lastWindowedSettings_{};
		bool hasLastWindowedSettings_ = false;

		// Windowed時に枠ドラッグでリサイズできるか（Borderless中でも「戻った時の状態」として保持）
		bool windowedResizable_ = true;

	private: /// ---------- コピー禁止 ---------- ///

		/// <summary>
		/// 外部からの生成を禁止するためのプライベートコンストラクタ。<br/>
		/// シングルトンパターンとして利用します。
		/// </summary>
		WinApp() = default;

		/// <summary>
		/// デフォルトデストラクタ。
		/// </summary>
		~WinApp() = default;

		/// <summary>
		/// コピーコンストラクタは使用禁止です。
		/// </summary>
		WinApp(const WinApp&) = delete;

		/// <summary>
		/// 代入演算子は使用禁止です。
		/// </summary>
		const WinApp& operator=(const WinApp&) = delete;
	};

} // namespace Ken4lowEngine