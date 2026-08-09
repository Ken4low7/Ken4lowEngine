#pragma once
#include <string>
#include <functional>
#include <optional>
#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterData.h"
#include <vector>

/// ---------- 武器エディタの画像プレビュー用構造体 ---------- ///
struct WeaponEditorImagePreview
{
	void* imguiTextureId = nullptr;
	int width = 0;
	int height = 0;
};

/// ---------- 武器エディタ用フック群 ---------- ///
struct WeaponEditorHooks
{
	// 保存
	std::function<void()> SaveAll;

	// 再読込の予約（誰を再選択したいか）
	std::function<void(int32_t weaponID)> RequestReloadFocus;

	// クラス変更時
	std::function<void()> RebuildLoadout;

	// 現在装備ならランタイムへ反映
	std::function<void(int32_t weaponID, const FWeaponMasterData&)> ApplyToRuntimeIfCurrent;

	// 新規武器要求をUI操作中に即時反映せず、エディタ側で安全なタイミングに遅延実行する。
	std::function<void(const std::string& newDisplayName, int32_t baseWeaponIDOrZero)> RequestAdd;

	// 削除予約
	std::function<void(int32_t weaponID)> RequestDelete;

	// サウンドプレビュー（SE試聴）
	std::function<void(const std::string& path)> PlaySoundPreviewSE;

	// サウンドプレビュー停止
	std::function<void()> StopSoundPreviewSE;

	// 画像プレビューの取得
	std::function<WeaponEditorImagePreview(const std::string& path)> GetImagePreview;
};

/// -------------------------------------------------------------
///				　		武器マスターデータエディタ
/// -------------------------------------------------------------
class WeaponMasterDataEditor
{
	// 予約（イテレータ破壊防止）
	struct Pending
	{
		bool doSaveAll = false;
		std::optional<int32_t> deleteID;
	}pending_{};

public: /// ---------- メンバ関数 ---------- ///

	// ImGui描画処理
	void DrawImGui(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks);

private: /// ---------- メンバ関数 ---------- ///

	void DrawToolbar(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks);

	void DrawList(WeaponMasterDataDatabase& database);

	void DrawInspector(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks);

	void ProcessPending(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks);

	void MarkEditedAndMaybeApply(WeaponEditorHooks& hooks, int32_t weaponID, const FWeaponMasterData& data);

	static bool HasAttributes(const std::vector<EWeaponAttribute>& v, EWeaponAttribute attr);

	static bool ToggleAttributes(std::vector<EWeaponAttribute>& v, EWeaponAttribute attr);

private: /// ---------- 武器の設定関数 ---------- ///

	// 武器の基本データを描画・編集する関数
	void DrawCoreData(FWeaponMasterData* data, WeaponEditorHooks& hooks);

	// 武器の経済データを描画・編集する関数
	void DrawEconomyData(FWeaponMasterData* data, WeaponEditorHooks& hooks);

	// 武器の射撃武器用データを描画・編集する関数
	void DrawStats(FWeaponMasterData* data, WeaponEditorHooks& hooks);

	// 武器の操作・反動データを描画・編集する関数
	void DrawHandling(FWeaponMasterData* data, WeaponEditorHooks& hooks);

	// 武器のレティクルデータを描画・編集する関数
	void DrawReticleData(FWeaponMasterData* data, WeaponEditorHooks& hooks);

	// 武器のアセットデータを描画・編集する関数
	void DrawAssetData(FWeaponMasterData* data, WeaponEditorHooks& hooks);

	// 武器のサウンドデータを描画・編集する関数
	void DrawSoundData(FWeaponMasterData* data, WeaponEditorHooks& hooks);

	// 武器のVFXデータを描画・編集する関数
	void DrawVfxData(FWeaponMasterData* data, WeaponEditorHooks& hooks);

	// 武器のソケットデータを描画・編集する関数
	void DrawSocketData(FWeaponMasterData* data, WeaponEditorHooks& hooks);

	// 武器の近接武器用データを描画・編集する関数
	void DrawMeleeData(FWeaponMasterData* data, WeaponEditorHooks& hooks);

	// オプションデータを描画・編集する関数
	void DrawOptionData(FWeaponMasterData* data, WeaponEditorHooks& hooks);

	// 特殊効果・オートデータを描画・編集する関数
	void DrawSpecialAndAutoData(FWeaponMasterData* data, WeaponEditorHooks& hooks);

private: /// ---------- メンバ変数 ---------- ///

	// UIの状態
	int32_t selectedID_ = 0; // 選択中の武器ID
	bool autoApply_ = true;   // 編集即ランタイム反映フラグ
	bool dirty_ = false;     // 変更されたかどうか

	// --- New時の名前入力（InputText用バッファ）---
	char newNameBuf_[64] = "NewWeapon";

	// Renameもしたい場合に使う（今回は付けておくと便利）
	char renameBuf_[64] = "";

	// 最後のエラーメッセージ
	std::string lastError_;

	// 表示フィルター
	int viewCategory_ = -1; // -1=全て、0～=カテゴリ絞り込み
	bool groupByCategory_ = false;
};

