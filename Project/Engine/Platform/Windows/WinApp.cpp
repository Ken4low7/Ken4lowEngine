#include "WinApp.h"
#include <stdexcept>
#include <vector>
#include <array>
#include <cstddef>

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	namespace
	{
		struct MonitorEnumCtx
		{
			int target = 0;
			int index = 0;
			RECT rect{};
			bool found = false;
		};

		BOOL CALLBACK EnumMonProc(HMONITOR hMon, HDC, LPRECT, LPARAM lp)
		{
			auto* ctx = reinterpret_cast<MonitorEnumCtx*>(lp);
			if (ctx->index == ctx->target)
			{
				MONITORINFO mi{};
				mi.cbSize = sizeof(mi);
				GetMonitorInfo(hMon, &mi);
				ctx->rect = mi.rcMonitor;
				ctx->found = true;
				return FALSE; // stop
			}
			ctx->index++;
			return TRUE;
		}

		RECT GetMonitorRectByIndex(int monitorIndex)
		{
			struct Ctx { int target; int i; RECT rc; bool found; } ctx{ monitorIndex, 0, {}, false };

			auto cb = [](HMONITOR hMon, HDC, LPRECT, LPARAM lp)->BOOL
				{
					auto* c = reinterpret_cast<Ctx*>(lp);
					if (c->i == c->target)
					{
						MONITORINFO mi{}; mi.cbSize = sizeof(mi);
						GetMonitorInfo(hMon, &mi);
						c->rc = mi.rcMonitor;
						c->found = true;
						return FALSE;
					}
					c->i++;
					return TRUE;
				};

			EnumDisplayMonitors(nullptr, nullptr, cb, (LPARAM)&ctx);

			if (ctx.found) return ctx.rc;

			// fallback primary
			HMONITOR h = MonitorFromPoint(POINT{ 0,0 }, MONITOR_DEFAULTTOPRIMARY);
			MONITORINFO mi{}; mi.cbSize = sizeof(mi);
			GetMonitorInfo(h, &mi);
			return mi.rcMonitor;
		}
	}

	/// -------------------------------------------------------------
	///					シングルトンインスタンス
	/// -------------------------------------------------------------
	WinApp* WinApp::GetInstance()
	{
		static WinApp instance;
		return &instance;
	}

	/// -------------------------------------------------------------
	///					メインウィンドウの作成
	/// -------------------------------------------------------------
	void WinApp::CreateMainWindow(const DisplaySettings& settings)
	{
		HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
		if (FAILED(hr)) { throw std::runtime_error("Failed to initialize COM library"); }

		wc.lpfnWndProc = WindowProc;
		wc.lpszClassName = L"CG2WindowClass";
		wc.hInstance = GetModuleHandle(nullptr);
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		RegisterClass(&wc);

		WindowMode mode = settings.mode;
		if (mode == WindowMode::ExclusiveFullscreen) {
			mode = WindowMode::BorderlessFullscreen;
		}

		DWORD style = 0;
		DWORD exStyle = 0;
		RECT wrc{};

		if (mode == WindowMode::BorderlessFullscreen)
		{
			RECT mon = GetMonitorRectByIndex(settings.monitorIndex);
			style = WS_POPUP;
			exStyle = WS_EX_APPWINDOW;
			wrc = mon;
		}
		else
		{
			style = WS_OVERLAPPEDWINDOW;
			exStyle = 0;
			wrc = { 0,0,(LONG)settings.width,(LONG)settings.height };
			AdjustWindowRectEx(&wrc, style, FALSE, exStyle);
		}

		const int winW = wrc.right - wrc.left;
		const int winH = wrc.bottom - wrc.top;

		int x = CW_USEDEFAULT;
		int y = CW_USEDEFAULT;

		if (mode == WindowMode::BorderlessFullscreen)
		{
			x = wrc.left;
			y = wrc.top;
		}

		hwnd = CreateWindowEx(
			exStyle,
			wc.lpszClassName,
			L"Ken4lowEngine",
			style,
			x, y,
			winW, winH,
			nullptr, nullptr,
			wc.hInstance,
			nullptr
		);

		ShowWindow(hwnd, (mode == WindowMode::Windowed && settings.maximize) ? SW_MAXIMIZE : SW_SHOW);

		RECT crc{};
		GetClientRect(hwnd, &crc);
		clientWidth_ = (uint32_t)(crc.right - crc.left);
		clientHeight_ = (uint32_t)(crc.bottom - crc.top);

		currentDisplaySettings_ = settings;
		currentDisplaySettings_.mode = mode;
		if (mode == WindowMode::BorderlessFullscreen)
		{
			currentDisplaySettings_.width = clientWidth_;
			currentDisplaySettings_.height = clientHeight_;
		}
		else
		{
			windowedResizable_ = (style & WS_THICKFRAME) != 0;
			RememberWindowedSettings(currentDisplaySettings_);
		}
	}

	/// -------------------------------------------------------------
	///							終了処理
	/// -------------------------------------------------------------
	void WinApp::Finalize()
	{
		if (hwnd)
		{
			DestroyWindow(hwnd);
			hwnd = nullptr;
		}

		UnregisterClass(wc.lpszClassName, wc.hInstance);
		CoUninitialize();
	}

	/// -------------------------------------------------------------
	///						メッセージ処理
	/// -------------------------------------------------------------
	bool WinApp::ProcessMessage()
	{
		MSG msg{};
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				return true;
			}
		}
		return false;
	}

	bool WinApp::ConsumeResize(uint32_t& outWidth, uint32_t& outHeight)
	{
		if (!resizePending_) return false;
		resizePending_ = false;
		outWidth = clientWidth_;
		outHeight = clientHeight_;
		return true;
	}

	void WinApp::RequestDisplaySettings(const DisplaySettings& settings)
	{
		pendingDisplaySettings_ = settings;
		displaySettingsPending_ = true;
	}

	bool WinApp::ConsumeDisplaySettings(DisplaySettings& out)
	{
		if (!displaySettingsPending_) return false;
		displaySettingsPending_ = false;
		out = pendingDisplaySettings_;
		return true;
	}

	bool WinApp::ApplyDisplaySettings(const DisplaySettings& settings)
	{
		if (!hwnd) return false;

		WindowMode mode = settings.mode;
		if (mode == WindowMode::ExclusiveFullscreen)
		{
			mode = WindowMode::BorderlessFullscreen;
		}

		const RECT mon = GetMonitorRectByIndex(settings.monitorIndex);
		const int monW = mon.right - mon.left;
		const int monH = mon.bottom - mon.top;

		if (mode == WindowMode::BorderlessFullscreen)
		{
			if (!hasSavedWindowed_)
			{
				GetWindowRect(hwnd, &savedWindowRect_);
				savedStyle_ = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE);
				savedExStyle_ = (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE);
				hasSavedWindowed_ = true;
			}

			DWORD style = WS_OVERLAPPEDWINDOW;
			if (!windowedResizable_)
			{
				style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
			}
			DWORD exStyle = WS_EX_APPWINDOW;

			SetWindowLongPtr(hwnd, GWL_STYLE, style);
			SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

			SetWindowPos(hwnd, nullptr,
				mon.left, mon.top, monW, monH,
				SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

			ShowWindow(hwnd, SW_SHOW);
		}
		else
		{
			DWORD style = WS_OVERLAPPEDWINDOW;
			DWORD exStyle = WS_EX_OVERLAPPEDWINDOW;

			SetWindowLongPtr(hwnd, GWL_STYLE, style);
			SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

			RECT rc{ 0,0, (LONG)settings.width, (LONG)settings.height };
			AdjustWindowRectEx(&rc, style, FALSE, exStyle);

			const int winW = rc.right - rc.left;
			const int winH = rc.bottom - rc.top;

			const int x = mon.left + (monW - winW) / 2;
			const int y = mon.top + (monH - winH) / 2;

			SetWindowPos(hwnd, nullptr, x, y, winW, winH,
				SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

			ShowWindow(hwnd, settings.maximize ? SW_MAXIMIZE : SW_SHOW);
		}

		RECT crc{};
		GetClientRect(hwnd, &crc);
		clientWidth_ = (uint32_t)(crc.right - crc.left);
		clientHeight_ = (uint32_t)(crc.bottom - crc.top);
		resizePending_ = true;

		currentDisplaySettings_ = settings;
		currentDisplaySettings_.mode = mode;
		if (mode == WindowMode::BorderlessFullscreen)
		{
			currentDisplaySettings_.width = clientWidth_;
			currentDisplaySettings_.height = clientHeight_;
		}

		return true;
	}

	void WinApp::RequestToggleFullscreen()
	{
		toggleFullscreenPending_ = true;
	}

	bool WinApp::ConsumeToggleFullscreen()
	{
		if (!toggleFullscreenPending_) return false;
		toggleFullscreenPending_ = false;
		return true;
	}

	void WinApp::RememberWindowedSettings(const DisplaySettings& s)
	{
		lastWindowedSettings_ = s;
		hasLastWindowedSettings_ = true;
	}

	DisplaySettings WinApp::GetLastWindowedSettingsOrDefault() const
	{
		if (hasLastWindowedSettings_) return lastWindowedSettings_;
		return DisplaySettings{};
	}

	void WinApp::ToggleWindowResizable()
	{
		SetWindowResizable(!windowedResizable_);
	}

	void WinApp::SetWindowResizable(bool enable)
	{
		windowedResizable_ = enable;

		if (!hwnd) return;
		if (currentDisplaySettings_.mode != WindowMode::Windowed) return;

		DWORD style = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE);
		if (enable)
		{
			style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
		}
		else
		{
			style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
		}

		SetWindowLongPtr(hwnd, GWL_STYLE, style);
		SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}

	void WinApp::DrawDisplaySettingsImGui(bool* pOpen)
	{
#ifdef USE_IMGUI
		// WindowメニューのDisplay表示フラグが閉じている間は画面設定UIを生成しない
		if (pOpen != nullptr && !*pOpen)
		{
			return;
		}

		WinApp* win = WinApp::GetInstance();

		// UI編集中の値を保持する。
		// ただし、Alt+Enter や外部適用などで現在設定が変化した場合は
		// その内容へ追従させて表示の不一致を防ぐ。
		static DisplaySettings edit{};
		static bool initialized = false;

		const DisplaySettings& current = win->GetCurrentDisplaySettings();
		if (!initialized ||
			edit.mode != current.mode ||
			edit.width != current.width ||
			edit.height != current.height ||
			edit.monitorIndex != current.monitorIndex ||
			edit.maximize != current.maximize)
		{
			edit = current;
			initialized = true;
		}

		if (ImGui::Begin("Display", pOpen))
		{
			const char* modeItems[] = { "Windowed", "Borderless" };
			int mode = (edit.mode == WindowMode::Windowed) ? 0 : 1;
			if (ImGui::Combo("Mode", &mode, modeItems, IM_ARRAYSIZE(modeItems)))
			{
				edit.mode = (mode == 0) ? WindowMode::Windowed : WindowMode::BorderlessFullscreen;
			}

			if (edit.mode == WindowMode::Windowed)
			{
				// 解像度候補は DisplaySettings 側のプリセットテーブルから取得する。
				// UI側で固定値を直書きしないことで、候補追加時の修正箇所を一元化する。
				const auto& presets = DisplaySettings::kWindowedResolutionPresets;

				std::array<const char*, DisplaySettings::kWindowedResolutionPresets.size()> resItems{};
				for (size_t i = 0; i < presets.size(); ++i)
				{
					resItems[i] = presets[i].label;
				}

				// 現在の設定値に一致する解像度をプリセット一覧から探す。
				// 一致しない場合は先頭候補を表示する。
				int resIndex = 0;
				for (int i = 0; i < static_cast<int>(presets.size()); ++i)
				{
					if (edit.width == presets[i].width && edit.height == presets[i].height)
					{
						resIndex = i;
						break;
					}
				}

				if (ImGui::Combo("Resolution", &resIndex, resItems.data(), static_cast<int>(resItems.size())))
				{
					edit.width = presets[resIndex].width;
					edit.height = presets[resIndex].height;
				}

				ImGui::Checkbox("Maximize", &edit.maximize);
			}

			if (ImGui::Button("Apply"))
			{
				win->RequestDisplaySettings(edit);
			}

			ImGui::Text("Client: %u x %u", win->GetClientWidth(), win->GetClientHeight());
		}
		ImGui::End();
#else
		(void)pOpen;
#endif // USE_IMGUI
	}

	/// -------------------------------------------------------------
	///					ウィンドウプロシージャ
	/// -------------------------------------------------------------
	LRESULT WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (msg == WM_KEYDOWN)
		{
			const bool altDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
			const bool firstPress = (lparam & (static_cast<long long>(1) << 30)) == 0;

			if (wparam == VK_F4 && !altDown && firstPress)
			{
				auto* winApp = WinApp::GetInstance();

				if (winApp->GetCurrentDisplaySettings().mode != WindowMode::Windowed)
				{
					DisplaySettings s = winApp->GetLastWindowedSettingsOrDefault();
					s.mode = WindowMode::Windowed;
					if (s.width == 0 || s.height == 0)
					{
						// 復帰先のウィンドウ解像度が不正な場合は、
						// DisplaySettings で定義した既定解像度へフォールバックする。
						s.width = DisplaySettings::kDefaultResolution.width;
						s.height = DisplaySettings::kDefaultResolution.height;
					}
					s.maximize = false;
					winApp->RequestDisplaySettings(s);

					winApp->SetWindowResizable(true);
				}
				else
				{
					winApp->ToggleWindowResizable();
				}
				return 0;
			}
		}

#ifdef USE_IMGUI
		// ImGuiのWin32メッセージ処理はImGuiManager経由に集約する
		if (ImGuiManager::GetInstance()->ProcessWin32Message(hwnd, msg, wparam, lparam))
		{
			return true;
		}
#endif // USE_IMGUI

		auto* winApp = WinApp::GetInstance();

		switch (msg)
		{
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_ENTERSIZEMOVE:
			winApp->inSizeMove_ = true;
			return 0;

		case WM_EXITSIZEMOVE:
		{
			winApp->inSizeMove_ = false;
			RECT rc{};
			GetClientRect(hwnd, &rc);
			winApp->clientWidth_ = (uint32_t)(rc.right - rc.left);
			winApp->clientHeight_ = (uint32_t)(rc.bottom - rc.top);
			winApp->resizePending_ = true;
			return 0;
		}
		case WM_SIZE:
		{
			if (wparam == SIZE_MINIMIZED) return 0;

			if (!winApp->inSizeMove_) {
				uint32_t w = LOWORD(lparam);
				uint32_t h = HIWORD(lparam);
				if (w > 0 && h > 0) {
					winApp->clientWidth_ = w;
					winApp->clientHeight_ = h;
					winApp->resizePending_ = true;
				}
			}
			return 0;
		}
		case WM_SYSKEYDOWN:
		{
			const bool altDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
			const bool firstPress = (lparam & (static_cast<long long>(1) << 30)) == 0;

			if (wparam == VK_RETURN && altDown && firstPress)
			{
				WinApp::GetInstance()->RequestToggleFullscreen();
				return 0;
			}
			break;
		}

		case WM_SYSCHAR:
		{
			const bool altDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
			if (wparam == VK_RETURN && altDown) return 0;
			break;
		}

		default:
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		return 0;
	}
} // namespace Ken4lowEngine