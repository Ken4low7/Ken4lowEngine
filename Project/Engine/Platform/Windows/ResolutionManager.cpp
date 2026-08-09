#include "ResolutionManager.h"
#include <algorithm>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///					　シングルトンインスタンス
	/// -------------------------------------------------------------
	ResolutionManager* ResolutionManager::GetInstance()
	{
		static ResolutionManager instance;
		return &instance;
	}

	/// -------------------------------------------------------------
	///					　　　画面サイズ設定
	/// -------------------------------------------------------------
	void ResolutionManager::SetScreenSize(float width, float height)
	{
		// 解像度が0になると座標変換で除算エラーになるため、最低値を保証する。
		screenWidth_ = std::max(width, 1.0f);
		screenHeight_ = std::max(height, 1.0f);
	}

	/// -------------------------------------------------------------
	///					　　　座標変換
	/// -------------------------------------------------------------
	Vector2 ResolutionManager::ToScreen(const Vector2& logicalPos) const
	{
		// UIは基準解像度で管理し、描画時だけ現在解像度へ変換する。
		return {
			logicalPos.x * GetScaleX(),
			logicalPos.y * GetScaleY()
		};
	}

	/// -------------------------------------------------------------
	///					　　　座標変換（逆）
	/// -------------------------------------------------------------
	Vector2 ResolutionManager::ToLogical(const Vector2& screenPos) const
	{
		// マウス判定は現在解像度から基準解像度へ戻してから行う。
		return {
			screenPos.x / GetScaleX(),
			screenPos.y / GetScaleY()
		};
	}
} // namespace Ken4lowEngine
