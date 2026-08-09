#pragma once
#include <cstdint>
#include <array>
#include <cstddef>

namespace Ken4lowEngine
{

	/// ---------- ウィンドウモード列挙型 ---------- ///
	enum class WindowMode
	{
		Windowed,			   // ウィンドウモード
		BorderlessFullscreen, // ボーダーレスフルスクリーンモード
		ExclusiveFullscreen   // 排他フルスクリーンモード
	};

	/// <summary>
	/// ウィンドウモード時に選択可能な解像度候補です。
	/// UI表示名もここで一元管理し、WinApp 側での解像度直書きを防ぎます。
	/// </summary>
	struct ResolutionOption
	{
		uint32_t width;
		uint32_t height;
		const char* label;
	};

	/// ---------- 画面設定構造体 ---------- ///
	struct DisplaySettings
	{
		/// <summary>
		/// エンジン全体で使用する既定のウィンドウ解像度です。
		/// 既定値を変更する場合はこの定義を変更し、個別箇所での直書きは行わない想定です。
		/// </summary>
		static constexpr ResolutionOption kDefaultResolution{ 1280, 720, "1280x720" };

		/// <summary>
		/// ウィンドウモード用の解像度プリセット一覧です。
		/// ImGui などの設定UIはこのテーブルを参照し、候補の追加・削除をここへ集約します。
		/// 将来的には設定ファイルやプラットフォーム依存設定から供給する余地があります。
		/// </summary>
		static constexpr std::array<ResolutionOption, 2> kWindowedResolutionPresets{ {
			{ 1280, 720, "1280x720" },
			{ 1920, 1080, "1920x1080" }
		} };

		WindowMode mode = WindowMode::Windowed; // ウィンドウモードのデフォルト値

		uint32_t width = kDefaultResolution.width;   // 画面幅のデフォルト値
		uint32_t height = kDefaultResolution.height; // 画面高さのデフォルト値

		int monitorIndex = 0;  // 使用するモニターのインデックス（0がメインモニター）
		bool maximize = false; // ウィンドウモード時に最大化するかどうか
	};
} // namespace Ken4lowEngine