#pragma once
#include <Sprite.h>
#include <NumberSpriteDrawer.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///				　		武器スロットクラス
/// -------------------------------------------------------------
class WeaponSlot
{
public: /// ---------- メンバ関数 ---------- ///

	// スロット数定数
	static constexpr int kSlotCount = 6;

	// レイアウト設定構造体
	struct Layout
	{
		float slotSize = 128.0f; // 1枠のサイズ（正方形想定）
		float spacing = 8.0f;  // 枠間
		float marginBottom = 32.0f; // 画面下からの余白
	};

	// 弾薬情報構造体
	struct WeaponAmmoInfo
	{
		int currentAmmo = 0;
		int reserveAmmo = 0;
	};

	// HUD側に渡す1スロット分の表示データ
	struct HudSlotState
	{
		bool useAmmo = false; // 弾薬を使う武器かどうか（近接は false になる）
		WeaponAmmoInfo ammoInfo;   // 弾薬情報（currentAmmo, reserveAmmo）
	};

	// HUD全体に渡す表示データ
	struct HudSnapshot
	{
		int selectedIndex = -1; // 選択中スロット番号（0..5、-1は無し）
		std::array<HudSlotState, kSlotCount> slotStates; // スロットごとの表示データ
	};

public: /// ---------- メンバ関数 ---------- ///

	// frameTex/selectedTex は K4E::TextureManager 等で読み込んだハンドルを渡す想定
	void Initialize(const std::string& frameTex, const std::string& selectedTex, const Layout& layout = {});

	/// <summary>
	/// スロット番号表示用の数字スプライト初期化。
	/// </summary>
	void InitializeSlotNumbers(const std::string& numberTex,
		float srcDigitWidth = 50.0f, float srcDigitHeight = 50.0f,
		const K4E::Vector2& offset = { 8.0f, 8.0f },
		float spacing = 24.0f,
		float drawDigitWidth = -1.0f, float drawDigitHeight = -1.0f);

	/// <summary>
	/// 弾薬表示（残弾/予備弾）用の数字スプライト初期化。
	/// ※まずは Primary(スロット0)だけ表示する前提。
	/// </summary>
	void InitializeAmmoNumbers(const std::string& numberTex,
		float srcDigitW = 50.0f, float srcDigitH = 50.0f,
		const K4E::Vector2& padding = { 10.0f, 10.0f },
		float spacing = 2.0f,
		float drawDigitW = -1.0f, float drawDigitH = -1.0f);

	// 弾薬区切り文字スプライト初期化
	void InitializeAmmoDelimiter(const std::string& slashTex,
		const K4E::Vector2& size = { -1.0f, -1.0f },
		const K4E::Vector2& offset = { 0.0f, 0.0f });

	// 更新処理
	void Update(const HudSnapshot& hud);

	// 描画処理
	void Draw();

	void SetVisibleSlotCount(int count);
	int GetVisibleSlotCount() const { return visibleSlotCount_; }

	// スロットアイコン（スロット0..5に対応して6枚渡す）
	void InitializeIcons(const std::array<std::string, kSlotCount>& iconTex);

	// 全スロット同じアイコンを使う場合（従来互換）
	void InitializeIcons(const std::string& iconTex);

private: /// ---------- メンバ関数 ---------- ///

	// レイアウト再構築処理
	void RebuildLayout();

private: /// ---------- メンバ変数 ---------- ///

	Layout layout_{};

	int selectedIndex_ = -1;
	int visibleSlotCount_ = kSlotCount;

	std::string frameTex_;
	std::string selectedTex_;

	// 通常枠 / 選択枠（まずは枠だけ描画するので2枚持ちが安全）
	std::array<std::unique_ptr<K4E::Sprite>, kSlotCount> frame_;
	std::array<std::unique_ptr<K4E::Sprite>, kSlotCount> frameSelected_;

	// スロット番号表示用
	bool drawSlotNumbers_ = false;
	K4E::NumberSpriteDrawer numberDrawer_;
	K4E::Vector2 numberOffset_{ 8.0f, 8.0f };
	float numberSpacing_ = 24.0f;

	// --- 弾薬(残弾/予備弾)表示 ---
	bool drawAmmo_ = false;
	K4E::NumberSpriteDrawer ammoDrawer_;
	K4E::Vector2 ammoPadding_{ 10.0f, 10.0f };
	float ammoSpacing_ = 2.0f;
	float ammoDigitW_ = 24.0f;
	float ammoDigitH_ = 24.0f;

	// アイコン用スプライト配列
	std::array<std::unique_ptr<K4E::Sprite>, kSlotCount> icon_;

	// 弾薬情報配列（スロット0..5に対応して6個）
	std::vector<WeaponAmmoInfo> ammoInfos_;
	std::array<bool, kSlotCount> ammoUses_{};

	float numberDigitW_ = 50.0f;
	float numberDigitH_ = 50.0f;

	std::array<std::unique_ptr<K4E::Sprite>, kSlotCount> ammoSlash_;
	K4E::Vector2 ammoSlashSize_{ 0.0f, 0.0f };
	K4E::Vector2 ammoSlashOffset_{ 0.0f, 0.0f };
};

