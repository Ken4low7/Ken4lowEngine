#pragma once

/// 旧FPSステージ向けLevelData比較は廃止し、DebugScene起動時に削除済みJSONを自動読込しない。
class LevelDataValidation
{
public:
	void DrawImGui() const noexcept
	{
		// 互換呼び出しだけ残し、旧LevelDataのファイルI/Oは行わない。
	}
};
