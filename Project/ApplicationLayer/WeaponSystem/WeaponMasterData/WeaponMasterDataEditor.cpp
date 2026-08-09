#include "WeaponMasterDataEditor.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#endif // USE_IMGUI

namespace
{
	static const char* kRarityNames[] = { "コモン", "レア", "エピック", "レジェンダりー", "ミシカル" };
	static const char* kAmmoNames[] = { "デフォルト", "エネルギー", "爆薬", "なし" };
	static const char* kCategoryNames[] = { "プライマリ","バックアップ","近接","特殊","スナイパー","ヘビー" };
	static const char* kFireModeNames[] = { "セミオート", "バースト", "フルオート", "チャージ" };
	static const char* kReticleNames[] = { "なし", "ドット", "クロス", "サークル", "スコープ" };

	// 文字列プリセット
	static const char* kCurrencyPresets[] = { "ゴールド", "ジェム", "コイン" };

	/// ---------- 特殊一覧 ---------- ///
	static const EWeaponAttribute kAttrList[] = {
		EWeaponAttribute::Poison,
		EWeaponAttribute::Burning,
		EWeaponAttribute::AreaDamage,
		EWeaponAttribute::Bouncing,
		EWeaponAttribute::LifeSteal,
		EWeaponAttribute::WallBreak,
		EWeaponAttribute::FixedDelay,
	};
	static const char* kAttrNames[] = { "毒", "発火", "エリアダメージ", "反射", "吸血", "壁貫通", "固定ディレイ" };

	static std::vector<int32_t> BuildSortedIDs(const WeaponMasterDataDatabase& database)
	{
		std::vector<int32_t> ids;
		ids.reserve(database.GetAll().size());
		for (const auto& kv : database.GetAll())
		{
			ids.push_back(kv.first);
		}
		std::sort(ids.begin(), ids.end());
		return ids;
	}

#ifdef USE_IMGUI

	namespace fs = std::filesystem;

	static std::string ToLowerCopy(std::string s)
	{
		for (char& c : s) c = (char)std::tolower((unsigned char)c);
		return s;
	}

	static std::string NormalizePathSlashes(const std::string& s)
	{
		std::string out = s;
		for (char& c : out) if (c == '\\') c = '/';
		return out;
	}

	static bool HasAnyExtension(const fs::path& p, const std::vector<std::string>& extsLower)
	{
		const std::string ext = ToLowerCopy(p.extension().string());
		for (const auto& e : extsLower)
		{
			if (ext == e) return true;
		}
		return false;
	}

	// root配下を再帰走査して。"Resources/..." 形式で返す
	static std::vector<std::string> CollectFilesRecursively(const fs::path& root, const std::vector<std::string>& extsLower)
	{
		std::vector<std::string> out;

		std::error_code ec;
		if (!fs::exists(root, ec)) return out;

		for (fs::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec))
		{
			if (ec) break;

			if (!it->is_regular_file()) continue;
			const fs::path p = it->path();

			if (!HasAnyExtension(p, extsLower)) continue;

			// rootをそのまま前につけて "Resources/Models/..." 形式にする
			std::error_code relEc;
			fs::path rel = fs::relative(p, root, relEc);

			std::string pathStr;
			if (!relEc)
			{
				pathStr = (root / rel).generic_string();
			}
			else
			{
				pathStr = p.generic_string();
			}

			out.push_back(NormalizePathSlashes(pathStr));
		}

		std::sort(out.begin(), out.end());
		out.erase(std::unique(out.begin(), out.end()), out.end());
		return out;
	}

	static std::string StripPrefixIfPresent(std::string s, const std::string& prefix)
	{
		s = NormalizePathSlashes(s);

		std::string p = NormalizePathSlashes(prefix);
		if (s.rfind(p, 0) == 0) // 先頭にprefixがあるか
		{
			return s.substr(p.size());
		}
		return s;
	}

	static void DrawImagePreviewBlock(const char* label, const std::string& path, WeaponEditorHooks& hooks, float maxSize = 128.0f)
	{
		if (path.empty())
		{
			return;
		}

		if (!hooks.GetImagePreview)
		{
			ImGui::TextDisabled("%s: GetImagePreview 未設定", label);
			return;
		}

		WeaponEditorImagePreview pv = hooks.GetImagePreview(path);

		// デバッグ表示（最初は出しておくと切り分けが楽）
		ImGui::TextDisabled("%s Path: %s", label, path.c_str());
		ImGui::TextDisabled("%s TexID: %p", label, pv.imguiTextureId);
		ImGui::TextDisabled("%s Size : %d x %d", label, pv.width, pv.height);

		if (!pv.imguiTextureId || pv.width <= 0 || pv.height <= 0)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "プレビュー取得失敗");
			return;
		}

		float drawW = maxSize;
		float drawH = maxSize * (static_cast<float>(pv.height) / static_cast<float>(pv.width));

		if (drawH > maxSize)
		{
			drawH = maxSize;
			drawW = maxSize * (static_cast<float>(pv.width) / static_cast<float>(pv.height));
		}

		ImGui::TextUnformatted(label);
		ImGui::Image(pv.imguiTextureId, ImVec2(drawW, drawH));
	}

	static void EnsureFireMode(std::vector<EFireMode>& v, EFireMode m)
	{
		if (std::find(v.begin(), v.end(), m) == v.end())
			v.push_back(m);
	}

#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///				　		武器マスターデータエディタ
/// -------------------------------------------------------------
void WeaponMasterDataEditor::DrawImGui(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	ImGui::Begin("武器マスターデータエディタ");

	DrawToolbar(database, hooks);

	ImGui::Columns(2, nullptr, true);
	DrawList(database);
	ImGui::NextColumn();
	DrawInspector(database, hooks);
	ImGui::Columns(1);

	ImGui::End();

	ProcessPending(database, hooks); // Endの後にやるのが安全
#endif // USE_IMGUI
}

void WeaponMasterDataEditor::DrawToolbar(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	ImGui::Checkbox("Auto Apply", &autoApply_);
	ImGui::SameLine();

	if (ImGui::Button("Save All")) { pending_.doSaveAll = true; }
	ImGui::SameLine();

	if (ImGui::Button("Reload"))
	{
		const int32_t keepId = selectedID_;
		std::string err;

		const std::filesystem::path dir = "Resources/JSON/weapons";
		if (!database.LoadFromDirectory(dir, &err))
		{
			lastError_ = err;
		}
		else
		{
			lastError_.clear();

			if (keepId != 0 && database.ContainsID(keepId))
				selectedID_ = keepId;
			else
				selectedID_ = database.GetAll().empty() ? 0 : database.GetAll().begin()->first;

			if (hooks.RebuildLoadout) hooks.RebuildLoadout();
		}
	}

	ImGui::SameLine();
	ImGui::Checkbox("K4E::Group", &groupByCategory_);

	ImGui::SameLine();
	const char* viewLabel = (viewCategory_ < 0) ? "All" : kCategoryNames[viewCategory_];
	if (ImGui::BeginCombo("View##Category", viewLabel))
	{
		bool selAll = (viewCategory_ < 0);
		if (ImGui::Selectable("All", selAll)) viewCategory_ = -1;
		if (selAll) ImGui::SetItemDefaultFocus();

		for (int i = 0; i < (int)IM_ARRAYSIZE(kCategoryNames); ++i)
		{
			bool sel = (viewCategory_ == i);
			if (ImGui::Selectable(kCategoryNames[i], sel)) viewCategory_ = i;
			if (sel) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	ImGui::TextUnformatted(dirty_ ? "*dirty" : " ");

	// 件数表示（全体 / 表示中）
	auto ids = BuildSortedIDs(database);
	const int totalCount = (int)ids.size();
	int viewCount = 0;
	for (int32_t id : ids)
	{
		const FWeaponMasterData* d = database.FindByID(id);
		if (!d) continue;
		if (viewCategory_ >= 0 && (int)d->coreData.category != viewCategory_) continue;
		++viewCount;
	}
	ImGui::Text("Weapons : %d  (view:%d)   Selected : %d", totalCount, viewCount, selectedID_);

	if (!lastError_.empty())
	{
		ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", lastError_.c_str());
	}
#endif // USE_IMGUI

}

void WeaponMasterDataEditor::DrawList(WeaponMasterDataDatabase& database)
{
#ifdef USE_IMGUI
	if (ImGui::Button("新規作成"))
	{
		std::snprintf(newNameBuf_, sizeof(newNameBuf_), "NewWeapon");
		ImGui::OpenPopup("武器を作成");
	}
	ImGui::SameLine();

	if (ImGui::Button("複製") && selectedID_ != 0)
	{
		const int32_t newId = database.Duplicate(selectedID_);
		if (newId != 0) { selectedID_ = newId; dirty_ = true; }
	}
	ImGui::SameLine();

	if (ImGui::Button("削除") && selectedID_ != 0)
	{
		pending_.deleteID = selectedID_;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	// Popup本体（OpenPopupした名前と完全一致させる）
	if (ImGui::BeginPopupModal("武器を作成", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("武器名を入力してください");
		ImGui::InputText("##NewWeaponName", newNameBuf_, sizeof(newNameBuf_)); // Flags無し

		ImGui::Separator();

		if (ImGui::Button("作成"))
		{
			const std::string name = newNameBuf_;
			const int32_t id = database.CreateNewWithName(name); // これが無ければ CreateNewID→名前代入でも可

			selectedID_ = id;
			dirty_ = true;

			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::Separator();

	auto ids = BuildSortedIDs(database);

	auto passFilter = [&](const FWeaponMasterData& d) -> bool
		{
			if (viewCategory_ < 0) return true;
			return (int)d.coreData.category == viewCategory_;
		};

	ImGui::BeginChild("武器リスト", ImVec2(0, 0), true);

	if (groupByCategory_)
	{
		for (int cat = 0; cat < (int)IM_ARRAYSIZE(kCategoryNames); ++cat)
		{
			if (viewCategory_ >= 0 && cat != viewCategory_) continue;

			if (ImGui::CollapsingHeader(kCategoryNames[cat], ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (int32_t id : ids)
				{
					const FWeaponMasterData* data = database.FindByID(id);
					if (!data) continue;
					if ((int)data->coreData.category != cat) continue;
					if (!passFilter(*data)) continue;

					const bool selected = (id == selectedID_);
					std::string label = std::to_string(id) + " : " + data->coreData.weaponName;
					if (ImGui::Selectable(label.c_str(), selected)) selectedID_ = id;
				}
			}
		}
	}
	else
	{
		for (int32_t id : ids)
		{
			const FWeaponMasterData* data = database.FindByID(id);
			if (!data) continue;
			if (!passFilter(*data)) continue;

			const bool selected = (id == selectedID_);
			std::string label =
				std::to_string(id) + " : [" + kCategoryNames[(int)data->coreData.category] + "] " + data->coreData.weaponName;

			if (ImGui::Selectable(label.c_str(), selected)) selectedID_ = id;
		}
	}

	ImGui::EndChild();
#endif // USE_IMGUI

}

void WeaponMasterDataEditor::DrawInspector(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (selectedID_ == 0)
	{
		ImGui::TextDisabled("左のリストから武器を選択してください");
		return;
	}

	FWeaponMasterData* data = database.FindMutableByID(selectedID_);
	if (!data)
	{
		ImGui::TextDisabled("見つかりませんでした");
		return;
	}

	// 適応ボタン
	if (ImGui::Button("ランタイム適用"))
	{
		if (hooks.ApplyToRuntimeIfCurrent) hooks.ApplyToRuntimeIfCurrent(selectedID_, *data);
	}

	ImGui::Separator();

	/// ---------- 共通データ ---------- ///

	/// ---------- 武器の基本データ ---------- ///
	DrawCoreData(data, hooks);

	/// ---------- 経済データ ---------- ///
	DrawEconomyData(data, hooks);

	// 近接かどうか
	const bool isMelee = (data->coreData.category == EWeaponCategory::Melee);

	if (isMelee)
	{
		/// ---------- 近接武器用データ ---------- ///
		DrawMeleeData(data, hooks);
	}
	else
	{
		/// ---------- 射撃性能データ ---------- ///
		DrawStats(data, hooks);

		/// ---------- 操作・反動データ ---------- ///
		DrawHandling(data, hooks);

		/// ---------- レティクルデータ ---------- ///
		DrawReticleData(data, hooks);

		/// ---------- サウンドデータ ---------- ///
		DrawSoundData(data, hooks);

		/// ---------- オプションデータ ---------- ///
		DrawOptionData(data, hooks);

		/// ---------- 特殊効果・オートデータ ---------- ///
		DrawSpecialAndAutoData(data, hooks);
	}

	/// ---------- アセットデータ ---------- ///
	DrawAssetData(data, hooks);

	/// ---------- VFXデータ ---------- ///
	DrawVfxData(data, hooks);

	/// ---------- ソケットデータ ---------- ///
	DrawSocketData(data, hooks);

#endif // USE_IMGUI
}

void WeaponMasterDataEditor::ProcessPending(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (pending_.doSaveAll)
	{
		pending_.doSaveAll = false;
		dirty_ = false;
		if (hooks.SaveAll) hooks.SaveAll();
	}

	if (pending_.deleteID.has_value())
	{
		const int32_t del = *pending_.deleteID;
		pending_.deleteID.reset();

		if (selectedID_ == del) selectedID_ = 0;

		// 将来ファイル削除が絡むなら hooks.RequestDelete を使う
		if (hooks.RequestDelete) hooks.RequestDelete(del);
		else database.RemoveByID(del);

		dirty_ = true;
	}
#endif // USE_IMGUI

}

void WeaponMasterDataEditor::MarkEditedAndMaybeApply(WeaponEditorHooks& hooks, int32_t weaponID, const FWeaponMasterData& data)
{
	dirty_ = true;
#ifdef USE_IMGUI
	if (autoApply_ && hooks.ApplyToRuntimeIfCurrent)
	{
		hooks.ApplyToRuntimeIfCurrent(weaponID, data);
	}
#endif // USE_IMGUI

}

bool WeaponMasterDataEditor::HasAttributes(const std::vector<EWeaponAttribute>& v, EWeaponAttribute attr)
{
	for (auto& x : v) if (x == attr) return true;
	return false;
}

bool WeaponMasterDataEditor::ToggleAttributes(std::vector<EWeaponAttribute>& v, EWeaponAttribute attr)
{
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (v[i] == attr)
		{
			v.erase(v.begin() + i);
			return true;
		}
	}
	v.push_back(attr);
	return true;
}

/// -------------------------------------------------------------
///				　		武器の基本データを描画するUI
/// -------------------------------------------------------------
void WeaponMasterDataEditor::DrawCoreData(FWeaponMasterData* data, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("基本設定", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	bool edited = false;
	bool categoryChanged = false;

	// IDは表示のみ（編集不可）
	ImGui::Text("武器ID : %d", &data->coreData.weaponID);

	// 武器名（表示 + ポップアップ編集）
	ImGui::Text("武器名 : %s", data->coreData.weaponName.c_str());
	ImGui::SameLine();
	if (ImGui::Button("編集##WeaponName"))
	{
		// バッファに現在の名前をコピーしてからポップアップを開く
		std::snprintf(renameBuf_, sizeof(renameBuf_), "%s", data->coreData.weaponName.c_str());
		ImGui::OpenPopup("武器名を変更");
	}

	if (ImGui::BeginPopupModal("武器名を変更", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("新しい武器名を入力してください");
		ImGui::InputText("##RenameWeaponName", renameBuf_, sizeof(renameBuf_));

		ImGui::Separator();

		if (ImGui::Button("変更"))
		{
			data->coreData.weaponName = renameBuf_;
			edited = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// カテゴリー
	int cat = (int)data->coreData.category;
	if (ImGui::Combo("カテゴリー", &cat, kCategoryNames, IM_ARRAYSIZE(kCategoryNames)))
	{
		data->coreData.category = (EWeaponCategory)cat;
		edited = true;
		categoryChanged = true;

		// 近接にした時だけ meleeData を作る
		if (data->coreData.category == EWeaponCategory::Melee)
		{
			if (!data->meleeData.has_value())
			{
				data->meleeData = FMeleeWeaponData{};
			}

			// 近接向けの最低限の整合性（任意だけどおすすめ）
			data->stats.ammoType = EAmmoType::None;
			data->stats.capacity = 0;
			data->stats.maxReserveAmmo = 0;

			// 近接では projectileData は使わない想定
			data->projectileData.reset();
		}
		else
		{
			// 近接以外に切り替えたら meleeData を消す
			if (data->meleeData.has_value())
			{
				data->meleeData.reset();
			}

			// 射撃武器なのに None になっていたら戻す
			if (data->stats.ammoType == EAmmoType::None)
			{
				data->stats.ammoType = EAmmoType::Default;
			}
		}
	}

	// レアリティ
	int rarity = (int)data->coreData.rarity;
	if (ImGui::Combo("レアリティ", &rarity, kRarityNames, IM_ARRAYSIZE(kRarityNames)))
	{
		data->coreData.rarity = (EWeaponRarity)rarity;
		edited = true;
	}

	// ここでまとめて反映
	if (categoryChanged && hooks.RebuildLoadout) hooks.RebuildLoadout();

	// 以降、個別に変更があったらすぐ反映
	if (edited) MarkEditedAndMaybeApply(hooks, selectedID_, *data);

#else
	(void)data;
	(void)hooks;
#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///				　		経済データを描画するUI
/// -------------------------------------------------------------
void WeaponMasterDataEditor::DrawEconomyData(FWeaponMasterData* data, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("経済データ", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	bool edited = false;

	// 通貨タイプ（表示＋ポップアップ編集）
	ImGui::Text("通貨タイプ: %s", data->economyData.currencyType.empty() ? "(未設定)" : data->economyData.currencyType.c_str());
	ImGui::SameLine();
	if (ImGui::Button("編集##CurrencyType"))
	{
		std::snprintf(renameBuf_, sizeof(renameBuf_), "%s", data->economyData.currencyType.c_str());
		ImGui::OpenPopup("通貨タイプを変更");
	}

	if (ImGui::BeginPopupModal("通貨タイプを変更", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("通貨名を入力 (例: Gold / Gems)");
		ImGui::InputText("##CurrencyTypeInput", renameBuf_, sizeof(renameBuf_));

		ImGui::Separator();
		if (ImGui::Button("OK"))
		{
			data->economyData.currencyType = renameBuf_;
			edited = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	edited |= ImGui::DragInt("購入価格", &data->economyData.purchasePrice, 1.0f, 0, 100000000);
	edited |= ImGui::DragInt("解放レベル", &data->economyData.minLevelToUnlock, 1.0f, 0, 1000);

	ImGui::SeparatorText("セール設定");
	edited |= ImGui::Checkbox("期間限定販売", &data->economyData.bIsLimitedTime);
	edited |= ImGui::SliderFloat("割引率", &data->economyData.discountRate, 0.0f, 0.95f, "%.2f");

	// 実価格プレビュー
	int basePrice = data->economyData.purchasePrice;
	float discount = data->economyData.discountRate;
	int finalPrice = (int)(basePrice * (1.0f - discount));
	if (finalPrice < 0) finalPrice = 0;

	ImGui::Text("実価格プレビュー: %d", finalPrice);

	ImGui::SeparatorText("アップグレードコスト");

	// 件数調整
	int count = (int)data->economyData.upgradeCosts.size();
	if (ImGui::DragInt("段階数", &count, 1.0f, 0, 64))
	{
		if (count < 0) count = 0;
		data->economyData.upgradeCosts.resize((size_t)count, 0);
		edited = true;
	}

	// 各段階のコスト編集
	for (size_t i = 0; i < data->economyData.upgradeCosts.size(); ++i)
	{
		ImGui::PushID((int)i);

		int& cost = data->economyData.upgradeCosts[i];
		char label[64];
		std::snprintf(label, sizeof(label), "Lv%zu -> Lv%zu", i, i + 1);

		if (ImGui::DragInt(label, &cost, 1.0f, 0, 100000000))
			edited = true;

		ImGui::SameLine();
		if (ImGui::SmallButton("0にする"))
		{
			cost = 0;
			edited = true;
		}

		ImGui::PopID();
	}

	// 便利ボタン（任意だけど便利）
	if (ImGui::Button("アップグレードコストを初期化(100,200,300)"))
	{
		data->economyData.upgradeCosts = { 100, 200, 300 };
		edited = true;
	}

	// 軽いバリデーション表示
	if (data->economyData.discountRate >= 1.0f)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "※ 割引率が1.0以上だと無料/負価格になります");
	}
	if (data->economyData.purchasePrice < 0)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "※ 購入価格が負になっています");
	}

	// ここでまとめて反映
	if (edited)	MarkEditedAndMaybeApply(hooks, selectedID_, *data);

#else
	(void)data;
	(void)hooks;
#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///				　	射撃性能データを描画するUI
/// -------------------------------------------------------------
void WeaponMasterDataEditor::DrawStats(FWeaponMasterData* data, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("ステータス設定", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	bool edited = false;

	// 基本
	edited |= ImGui::DragFloat("ダメージ", &data->stats.damage, 0.1f, 0.0f, 100000.0f);
	edited |= ImGui::DragFloat("最小ダメージ", &data->stats.minDamage, 0.1f, 0.0f, 100000.0f);
	edited |= ImGui::DragFloat("減衰開始距離", &data->stats.damageFalloffStart, 0.1f, 0.0f, 100000.0f);
	edited |= ImGui::DragFloat("減衰終了距離", &data->stats.damageFalloffEnd, 0.1f, 0.0f, 100000.0f);

	edited |= ImGui::DragFloat("発射速度(RPM)", &data->stats.fireRate, 0.1f, 0.0f, 3000.0f);
	edited |= ImGui::SliderFloat("機動力", &data->stats.mobility, 0.0f, 1.0f);
	edited |= ImGui::DragInt("弾薬容量", &data->stats.capacity, 1.0f, 0, 9999);

	int ammo = (int)data->stats.ammoType;
	if (ImGui::Combo("弾薬タイプ", &ammo, kAmmoNames, IM_ARRAYSIZE(kAmmoNames)))
	{
		data->stats.ammoType = (EAmmoType)ammo;
		edited = true;
	}
	
	ImGui::SeparatorText("リロード・弾数");
	edited |= ImGui::DragFloat("リロード時間(共通)", &data->stats.reloadTime, 0.01f, 0.0f, 60.0f);
	edited |= ImGui::DragFloat("タクティカルリロード", &data->stats.tacticalReloadTime, 0.01f, 0.0f, 60.0f);
	edited |= ImGui::DragFloat("空リロード", &data->stats.emptyReloadTime, 0.01f, 0.0f, 60.0f);
	edited |= ImGui::Checkbox("1発ずつリロード", &data->stats.bReloadByShell);
	edited |= ImGui::Checkbox("リロード中断可能", &data->stats.bCanInterruptReload);

	edited |= ImGui::DragInt("消費弾薬数/発", &data->stats.ammoPerShot, 1.0f, 0, 100);
	edited |= ImGui::DragInt("予備弾薬上限", &data->stats.maxReserveAmmo, 1.0f, 0, 99999);

	edited |= ImGui::Checkbox("薬室あり", &data->stats.bHasChamber);
	edited |= ImGui::DragInt("薬室数", &data->stats.chamberSize, 1.0f, 0, 8);

	ImGui::SeparatorText("部位倍率・クリティカル");
	edited |= ImGui::SliderFloat("クリティカル率", &data->stats.criticalChance, 0.0f, 1.0f);
	edited |= ImGui::DragFloat("ヘッド倍率", &data->stats.headshotMultiplier, 0.01f, 0.0f, 60.0f);
	edited |= ImGui::DragFloat("胴体倍率", &data->stats.bodyMultiplier, 0.01f, 0.0f, 60.0f);
	edited |= ImGui::DragFloat("腕倍率", &data->stats.armMultiplier, 0.01f, 0.0f, 60.0f);
	edited |= ImGui::DragFloat("脚倍率", &data->stats.legMultiplier, 0.01f, 0.0f, 60.0f);

	ImGui::SeparatorText("ペレット");
	edited |= ImGui::DragInt("ペレット数", &data->stats.pelletCount, 1.0f, 1, 128);
	edited |= ImGui::DragFloat("ペレット拡散角", &data->stats.pelletSpreadAngle, 0.1f, 0.0f, 180.0f);

	// 軽いバリデーション
	if (data->stats.damageFalloffEnd > 0.0f && data->stats.damageFalloffStart > data->stats.damageFalloffEnd)
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "※ 減衰開始距離 > 減衰終了距離 になっています");

	// ここでまとめて反映
	if (edited)	MarkEditedAndMaybeApply(hooks, selectedID_, *data);

#else
	(void)data;
	(void)hooks;
#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///				　	操作・反動データを描画するUI
/// -------------------------------------------------------------
void WeaponMasterDataEditor::DrawHandling(FWeaponMasterData* data, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("操作・反動設定", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	bool edited = false;

	// -----------------------------
	// 基本（従来項目）
	// -----------------------------
	edited |= ImGui::SliderFloat("精度", &data->handling.accuracy, 0.0f, 1.0f);
	edited |= ImGui::DragFloat("射撃時拡散増加", &data->handling.spreadIncrease, 0.001f, 0.0f, 100.0f);

	edited |= ImGui::DragFloat("垂直反動(互換)", &data->handling.verticalRecoil, 0.001f, -100.0f, 100.0f);
	edited |= ImGui::DragFloat("水平反動(互換)", &data->handling.horizontalRecoil, 0.001f, -100.0f, 100.0f);
	edited |= ImGui::DragFloat("反動回復速度", &data->handling.recoilRecovery, 0.01f, 0.0f, 1000.0f);

	// -----------------------------
	// 散布界（ブルーム）
	// -----------------------------
	ImGui::SeparatorText("散布界");
	edited |= ImGui::DragFloat("腰撃ち基本拡散", &data->handling.baseHipSpread, 0.01f, 0.0f, 180.0f);
	edited |= ImGui::DragFloat("ADS基本拡散", &data->handling.baseAdsSpread, 0.01f, 0.0f, 180.0f);
	edited |= ImGui::DragFloat("移動拡散倍率", &data->handling.moveSpreadMultiplier, 0.01f, 0.0f, 10.0f);
	edited |= ImGui::DragFloat("ジャンプ拡散倍率", &data->handling.jumpSpreadMultiplier, 0.01f, 0.0f, 10.0f);
	edited |= ImGui::DragFloat("しゃがみ拡散倍率", &data->handling.crouchSpreadMultiplier, 0.01f, 0.0f, 10.0f);
	edited |= ImGui::DragFloat("拡散回復速度", &data->handling.spreadRecoveryRate, 0.01f, 0.0f, 1000.0f);
	edited |= ImGui::DragFloat("最大拡散", &data->handling.maxSpread, 0.01f, 0.0f, 180.0f);

	// -----------------------------
	// 反動（詳細）
	// -----------------------------
	ImGui::SeparatorText("反動(詳細)");
	edited |= ImGui::DragFloat("視点反動Pitch", &data->handling.cameraRecoilPitch, 0.01f, -100.0f, 100.0f);
	edited |= ImGui::DragFloat("視点反動Yaw", &data->handling.cameraRecoilYaw, 0.01f, -100.0f, 100.0f);
	edited |= ImGui::DragFloat("武器キックバック", &data->handling.weaponKickBack, 0.001f, 0.0f, 10.0f);
	edited |= ImGui::DragFloat("反動リセット猶予", &data->handling.recoilResetDelay, 0.001f, 0.0f, 5.0f);

	// -----------------------------
	// ADS
	// -----------------------------
	ImGui::SeparatorText("ADS");
	edited |= ImGui::DragFloat("ADS FOV", &data->handling.adsZoomFov, 0.1f, 1.0f, 179.0f);
	edited |= ImGui::DragFloat("ズーム倍率(UI用)", &data->handling.zoomLevel, 0.01f, 0.1f, 20.0f);

	// 旧互換の速度指定（残してるなら）
	edited |= ImGui::DragFloat("ADS遷移速度(互換)", &data->handling.adsTransitionSpeed, 0.01f, 0.0f, 1000.0f);

	// 実運用向けの時間指定
	edited |= ImGui::DragFloat("ADS入り時間", &data->handling.adsInTime, 0.001f, 0.0f, 5.0f);
	edited |= ImGui::DragFloat("ADS解除時間", &data->handling.adsOutTime, 0.001f, 0.0f, 5.0f);
	edited |= ImGui::DragFloat("ADS移動倍率", &data->handling.adsMoveSpeedMultiplier, 0.01f, 0.0f, 2.0f);

	// -----------------------------
	// 武器切替・スプリント遷移
	// -----------------------------
	ImGui::SeparatorText("切替・スプリント");
	edited |= ImGui::DragFloat("装備時間", &data->handling.equipTime, 0.001f, 0.0f, 5.0f);
	edited |= ImGui::DragFloat("解除時間", &data->handling.unequipTime, 0.001f, 0.0f, 5.0f);
	edited |= ImGui::DragFloat("Sprint->Fire", &data->handling.sprintToFireTime, 0.001f, 0.0f, 5.0f);
	edited |= ImGui::DragFloat("Fire->Sprint", &data->handling.fireToSprintTime, 0.001f, 0.0f, 5.0f);

	// -----------------------------
	// 特殊（FixedDelay属性があるときだけ出すのがおすすめ）
	// -----------------------------
	// HasAttributes(...) が既にある前提。なければ常時表示でもOKです。
	if (HasAttributes(data->attributes, EWeaponAttribute::FixedDelay))
	{
		ImGui::SeparatorText("特殊");
		edited |= ImGui::DragFloat("固定ディレイ", &data->handling.fixedDelayTime, 0.001f, 0.0f, 10.0f);
	}

	// -----------------------------
	// 軽い注意表示
	// -----------------------------
	if (data->handling.baseAdsSpread > data->handling.baseHipSpread)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
			"※ ADS拡散が腰撃ちより大きいです（意図的ならOK）");
	}

	if (data->handling.adsInTime > 0.0f && data->handling.adsOutTime > 0.0f)
	{
		ImGui::TextDisabled("ADS: In %.3fs / Out %.3fs",
			data->handling.adsInTime, data->handling.adsOutTime);
	}

	if (edited)
	{
		MarkEditedAndMaybeApply(hooks, selectedID_, *data);
	}
#else
	(void)data;
	(void)hooks;
#endif // USE_IMGUI
}

void WeaponMasterDataEditor::DrawReticleData(FWeaponMasterData* data, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("レティクル設定", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	bool edited = false;
	namespace fs = std::filesystem;

	// レティクル画像の検索先
	const fs::path reticleRoot = "Resources/Textures/Compiled/UI/Reticles";
	static const std::vector<std::string> kReticleExts = { ".dds" };

	// 毎フレーム再帰走査しないようにキャッシュ
	static std::vector<std::string> cachedReticleFiles;
	static bool scannedOnce = false;

	auto RefreshReticleList = [&]()
		{
			cachedReticleFiles = CollectFilesRecursively(reticleRoot, kReticleExts);
			scannedOnce = true;
		};

	if (!scannedOnce)
	{
		RefreshReticleList();
	}

	// 共通ファイルピッカー（レティクル画像用）
	auto DrawReticleFilePicker = [&](const char* title, const char* idSuffix, std::string& targetPath, float previewSize = 96.0f) -> bool
		{
			bool localEdited = false;

			ImGui::PushID(idSuffix);

			ImGui::TextUnformatted(title);
			ImGui::TextWrapped("現在: %s", targetPath.empty() ? "(未設定)" : targetPath.c_str());

			if (!targetPath.empty())
			{
				std::error_code ecFile;
				const bool exists = fs::exists(fs::path(targetPath), ecFile);
				ImGui::SameLine();
				if (exists)
					ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "OK");
				else
					ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Missing");
			}

			DrawImagePreviewBlock(title, targetPath, hooks, previewSize);

			const char* preview = targetPath.empty() ? "(未設定)" : targetPath.c_str();
			if (ImGui::BeginCombo("画像ファイル", preview))
			{
				bool selNone = targetPath.empty();
				if (ImGui::Selectable("(未設定)", selNone))
				{
					targetPath.clear();
					localEdited = true;
				}
				if (selNone) ImGui::SetItemDefaultFocus();

				for (const auto& path : cachedReticleFiles)
				{
					bool selected = (path == targetPath);
					if (ImGui::Selectable(path.c_str(), selected))
					{
						targetPath = path;
						localEdited = true;
					}
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			if (ImGui::Button("Clear"))
			{
				targetPath.clear();
				localEdited = true;
			}

			ImGui::PopID();
			return localEdited;
		};

	// -----------------------------
	// 基本挙動
	// -----------------------------
	ImGui::SeparatorText("基本");

	int rt = (int)data->reticleData.reticleType;
	if (ImGui::Combo("レティクルタイプ", &rt, kReticleNames, IM_ARRAYSIZE(kReticleNames)))
	{
		data->reticleData.reticleType = (EReticleType)rt;
		edited = true;
	}

	edited |= ImGui::DragFloat("基本サイズ", &data->reticleData.reticleBaseSize, 0.1f, 0.0f, 1000.0f);
	edited |= ImGui::DragFloat("最大サイズ", &data->reticleData.reticleMaxSize, 0.1f, 0.0f, 1000.0f);
	edited |= ImGui::DragFloat("射撃時拡張量", &data->reticleData.reticleExpandPerShot, 0.01f, 0.0f, 100.0f);
	edited |= ImGui::DragFloat("回復速度", &data->reticleData.reticleRecoverSpeed, 0.01f, 0.0f, 1000.0f);

	// -----------------------------
	// 移動時拡散（APEXっぽい広がり用）
	// -----------------------------
	ImGui::SeparatorText("移動拡散演出");

	edited |= ImGui::Checkbox("移動でレティクルを広げる", &data->reticleData.bEnableMoveReticleExpand);
	if (data->reticleData.bEnableMoveReticleExpand)
	{
		edited |= ImGui::DragFloat("歩き倍率", &data->reticleData.moveExpandMultiplier, 0.01f, 0.0f, 10.0f);
		edited |= ImGui::DragFloat("ダッシュ倍率", &data->reticleData.sprintExpandMultiplier, 0.01f, 0.0f, 10.0f);
		edited |= ImGui::DragFloat("空中倍率", &data->reticleData.airExpandMultiplier, 0.01f, 0.0f, 10.0f);
		edited |= ImGui::DragFloat("着地インパルス", &data->reticleData.landExpandImpulse, 0.01f, 0.0f, 100.0f);
		ImGui::TextDisabled("※ 実際の開閉はランタイム側で速度/状態に応じて反映");
	}

	// -----------------------------
	// 画像一覧（デバッグ）
	// -----------------------------
	ImGui::SeparatorText("画像検索");
	ImGui::TextDisabled("Root: %s", reticleRoot.generic_string().c_str());
	ImGui::TextDisabled("CWD : %s", fs::current_path().generic_string().c_str());

	std::error_code ecRoot;
	if (!fs::exists(reticleRoot, ecRoot))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
			"※ フォルダが見つかりません: %s", reticleRoot.generic_string().c_str());
	}

	ImGui::SameLine();
	if (ImGui::Button("再スキャン##Reticle"))
	{
		RefreshReticleList();
	}

	// -----------------------------
	// 腰だめ（HIP）画像
	// -----------------------------
	ImGui::SeparatorText("HIP（腰だめ）");
	edited |= DrawReticleFilePicker("腰だめレティクル", "HipReticle", data->reticleData.reticleTexturePath, 128.0f);

	// -----------------------------
	// ADS切り替え
	// -----------------------------
	ImGui::SeparatorText("ADS");
	edited |= ImGui::Checkbox("ADS時に非表示（スコープ系）", &data->reticleData.bHideReticleInADS);
	edited |= ImGui::Checkbox("ADS時に別レティクル画像を使う", &data->reticleData.bUseAdsReticleOverride);

	if (data->reticleData.bUseAdsReticleOverride)
	{
		edited |= DrawReticleFilePicker("ADSレティクル", "AdsReticle", data->reticleData.adsReticleTexturePath, 128.0f);

		if (ImGui::Button("HIP画像をADSへコピー"))
		{
			data->reticleData.adsReticleTexturePath = data->reticleData.reticleTexturePath;
			edited = true;
		}
	}

	edited |= ImGui::Checkbox("ADS時に中央ドットを表示", &data->reticleData.bUseAdsCenterDot);
	if (data->reticleData.bUseAdsCenterDot)
	{
		edited |= DrawReticleFilePicker("ADS中央ドット", "AdsDot", data->reticleData.adsCenterDotTexturePath, 96.0f);
	}

	edited |= ImGui::DragFloat("ADS切替ブレンド時間", &data->reticleData.adsReticleBlendTime, 0.001f, 0.0f, 1.0f);

	if (!data->reticleData.bHideReticleInADS &&
		!data->reticleData.bUseAdsReticleOverride &&
		!data->reticleData.bUseAdsCenterDot)
	{
		ImGui::TextDisabled("※ ADS中はHIPレティクルをそのまま使用");
	}

	// -----------------------------
	// ヒット / 撃破マーカー
	// -----------------------------
	ImGui::SeparatorText("ヒット / 撃破");
	edited |= ImGui::Checkbox("ヒットマーカー表示", &data->reticleData.bShowHitMarker);

	if (data->reticleData.bShowHitMarker)
	{
		edited |= DrawReticleFilePicker("通常ヒットマーカー", "HitMarker", data->reticleData.hitMarkerTexturePath, 96.0f);
		edited |= ImGui::DragFloat("ヒット表示時間", &data->reticleData.hitMarkerDuration, 0.001f, 0.0f, 1.0f);

		edited |= ImGui::Checkbox("ヘッドショット用マーカーを分ける", &data->reticleData.bUseHeadshotMarker);
		if (data->reticleData.bUseHeadshotMarker)
		{
			edited |= DrawReticleFilePicker("ヘッドショットマーカー", "HeadshotMarker", data->reticleData.headshotHitMarkerTexturePath, 96.0f);
		}

		edited |= ImGui::Checkbox("撃破確認マーカーを使う", &data->reticleData.bUseKillConfirmMarker);
		if (data->reticleData.bUseKillConfirmMarker)
		{
			edited |= DrawReticleFilePicker("撃破確認マーカー", "KillConfirmMarker", data->reticleData.killConfirmMarkerTexturePath, 96.0f);
			edited |= ImGui::DragFloat("撃破表示時間", &data->reticleData.killConfirmDuration, 0.001f, 0.0f, 1.0f);
		}
	}

	// -----------------------------
	// 注意表示（軽いバリデーション）
	// -----------------------------
	if (data->reticleData.reticleMaxSize < data->reticleData.reticleBaseSize)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
			"※ 最大サイズが基本サイズより小さいです（意図的ならOK）");
	}

	if (data->reticleData.bUseAdsReticleOverride && data->reticleData.adsReticleTexturePath.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f),
			"※ ADS別画像ONですが ADSレティクル画像 が未設定です");
	}

	if (data->reticleData.bUseAdsCenterDot && data->reticleData.adsCenterDotTexturePath.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f),
			"※ ADS中央ドットONですが ドット画像 が未設定です");
	}

	if (data->reticleData.bShowHitMarker && data->reticleData.hitMarkerTexturePath.empty())
	{
		ImGui::TextDisabled("※ 通常ヒットマーカー画像が未設定（ランタイム側で図形描画するならOK）");
	}

	if (edited)
	{
		MarkEditedAndMaybeApply(hooks, selectedID_, *data);
	}

#else
	(void)data;
	(void)hooks;
#endif

}

/// -------------------------------------------------------------
///				　	アセットデータを描画するUI
/// -------------------------------------------------------------
void WeaponMasterDataEditor::DrawAssetData(FWeaponMasterData* data, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("アセット", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	bool edited = false;
	namespace fs = std::filesystem;

	// ルートフォルダ
	const fs::path modelRoot = "Resources/Models";
	const fs::path iconRoot = "Resources/Textures/Compiled/UI/Icons";
	const fs::path scopeRoot = "Resources/Textures"; // スコープオーバーレイの置き場所（必要なら細かく分けてOK）

	// 拡張子フィルタ
	static const std::vector<std::string> kModelExts = { ".gltf", ".glb", ".fbx", ".obj" };
	static const std::vector<std::string> kImageExts = { ".png", ".dds", ".jpg", ".jpeg", ".tga", ".bmp" };

	// キャッシュ（毎フレーム走査しない）
	static std::vector<std::string> cachedModelFiles;
	static std::vector<std::string> cachedIconFiles;
	static std::vector<std::string> cachedScopeFiles;
	static bool scannedOnce = false;

	auto RefreshAssetLists = [&]()
		{
			cachedModelFiles = CollectFilesRecursively(modelRoot, kModelExts);
			cachedIconFiles = CollectFilesRecursively(iconRoot, kImageExts);
			cachedScopeFiles = CollectFilesRecursively(scopeRoot, kImageExts);
			scannedOnce = true;
		};

	if (!scannedOnce)
	{
		RefreshAssetLists();
	}

	// 1項目分の共通描画（現在値 / OK-Missing / ファイル名 / コンボ / Clear）
	auto DrawAssetPicker = [&](const char* sectionLabel,
		const fs::path& rootPath,
		const std::vector<std::string>& candidates,
		const char* comboLabel,
		const char* clearButtonId,
		std::string& targetPath,
		bool showImagePreview)   // ★追加
		{
			ImGui::SeparatorText(sectionLabel);

			ImGui::TextDisabled("Root: %s", rootPath.generic_string().c_str());

			std::error_code ecRoot;
			if (!fs::exists(rootPath, ecRoot))
			{
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
					"※ フォルダが見つかりません: %s", rootPath.generic_string().c_str());
			}

			ImGui::TextWrapped("現在: %s", targetPath.empty() ? "(未設定)" : targetPath.c_str());

			if (!targetPath.empty())
			{
				std::error_code ecFile;
				const bool exists = fs::exists(fs::path(targetPath), ecFile);

				ImGui::SameLine();
				if (exists)
					ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "OK");
				else
					ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Missing");

				fs::path p(targetPath);
				ImGui::Text("ファイル名: %s", p.filename().string().c_str());
			}

			// ★追加：画像プレビュー（アイコン用）
			if (showImagePreview)
			{
				DrawImagePreviewBlock("画像プレビュー", targetPath, hooks, 128.0f);
			}

			const char* preview = targetPath.empty() ? "(未設定)" : targetPath.c_str();
			if (ImGui::BeginCombo(comboLabel, preview))
			{
				bool selNone = targetPath.empty();
				if (ImGui::Selectable("(未設定)", selNone))
				{
					targetPath.clear();
					edited = true;
				}
				if (selNone) ImGui::SetItemDefaultFocus();

				for (const auto& path : candidates)
				{
					bool selected = (path == targetPath);
					if (ImGui::Selectable(path.c_str(), selected))
					{
						targetPath = path; // "Resources/..." 形式で保存
						edited = true;
					}
					if (selected) ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			ImGui::SameLine();
			if (ImGui::Button(clearButtonId))
			{
				targetPath.clear();
				edited = true;
			}
		};

	// デバッグ用（読めない原因確認）
	ImGui::TextDisabled("CWD: %s", fs::current_path().generic_string().c_str());
	ImGui::SameLine();
	if (ImGui::Button("再スキャン##Assets"))
	{
		RefreshAssetLists();
	}

	// モデル
	DrawAssetPicker("モデル", modelRoot, cachedModelFiles,
		"モデルファイル", "Clear##ModelPath", data->assetData.modelPath, false);

	// アイコン
	DrawAssetPicker("アイコン", iconRoot, cachedIconFiles,
		"アイコンファイル", "Clear##IconPath", data->assetData.iconPath, true);
	// スコープオーバーレイ（ヘッダにある場合）
	//// もしまだ assetData.adsScopeOverlayPath が無ければ、このブロックはコメントアウトしてください。
	//DrawAssetPicker("ADSスコープオーバーレイ", scopeRoot, cachedScopeFiles,
	//	"スコープ画像", "Clear##ScopeOverlay", data->assetData.adsScopeOverlayPath);

	if (edited)
	{
		MarkEditedAndMaybeApply(hooks, selectedID_, *data);
	}
#else
	(void)data;
	(void)hooks;
#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///				　	サウンドデータを描画するUI
/// -------------------------------------------------------------
void WeaponMasterDataEditor::DrawSoundData(FWeaponMasterData* data, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("サウンド", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	bool edited = false;
	namespace fs = std::filesystem;

	const fs::path soundRoot = "Resources/Sounds";
	const std::string soundPrefix = "Resources/Sounds/";

	// 既存データ（昔の保存形式）を相対パスへ正規化
	auto NormalizeSoundPath = [&](std::string& s)
		{
			if (s.empty()) return;
			const std::string before = s;
			s = StripPrefixIfPresent(s, soundPrefix);
			if (s != before) edited = true;
		};

	NormalizeSoundPath(data->soundData.fireSoundPath);
	NormalizeSoundPath(data->soundData.reloadSoundPath);
	NormalizeSoundPath(data->soundData.emptySoundPath);
	NormalizeSoundPath(data->soundData.equipSoundPath);
	NormalizeSoundPath(data->soundData.impactSoundPath);

	// よく使う音声拡張子
	static const std::vector<std::string> kSoundExts = {
		".wav", ".mp3", ".ogg", ".flac", ".aac", ".m4a"
	};

	// キャッシュ（毎フレーム再帰走査しない）
	static std::vector<std::string> cachedSoundFiles; // "Resources/Sounds/..." 形式
	static bool scannedOnce = false;

	auto RefreshSoundList = [&]()
		{
			cachedSoundFiles = CollectFilesRecursively(soundRoot, kSoundExts);
			scannedOnce = true;
		};

	if (!scannedOnce)
	{
		RefreshSoundList();
	}

	// デバッグ情報（読み込めない原因の確認）
	ImGui::TextDisabled("Root: %s", soundRoot.generic_string().c_str());
	ImGui::TextDisabled("CWD : %s", fs::current_path().generic_string().c_str());

	std::error_code ecRoot;
	if (!fs::exists(soundRoot, ecRoot))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
			"※ フォルダが見つかりません: %s", soundRoot.generic_string().c_str());
	}

	ImGui::SameLine();
	if (ImGui::Button("再スキャン##Sounds"))
	{
		RefreshSoundList();
	}

	// 1項目分の共通描画
	auto DrawSoundPicker = [&](const char* sectionLabel, //	デバッグ用のセクションラベル
		const char* comboLabel, // コンボのラベル
		const char* playBtnId,	// 再生ボタンのID（ラベル）
		const char* stopBtnId,	// 停止ボタンのID（ラベル）
		const char* clearBtnId,	// クリアボタンのID（ラベル）
		std::string& targetPath) // 対象のパス（相対パスで保存）
		{
			ImGui::SeparatorText(sectionLabel);

			// 相対パスを表示（保存値）
			ImGui::TextWrapped("現在: %s", targetPath.empty() ? "(未設定)" : targetPath.c_str());

			// フルパスを組み立てて存在確認
			if (!targetPath.empty())
			{
				fs::path fullPath = soundRoot / fs::path(targetPath);
				std::error_code ecFile;
				const bool exists = fs::exists(fullPath, ecFile);

				ImGui::SameLine();
				if (exists)
					ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "OK");
				else
					ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Missing");

				ImGui::Text("ファイル名: %s", fs::path(targetPath).filename().string().c_str());
				ImGui::TextDisabled("Full: %s", NormalizePathSlashes(fullPath.generic_string()).c_str());
			}

			// 既存ファイルから選択（表示/保存は相対パス）
			const char* preview = targetPath.empty() ? "(未設定)" : targetPath.c_str();
			if (ImGui::BeginCombo(comboLabel, preview))
			{
				bool selNone = targetPath.empty();
				if (ImGui::Selectable("(未設定)", selNone))
				{
					targetPath.clear();
					edited = true;
				}
				if (selNone) ImGui::SetItemDefaultFocus();

				for (const auto& fullPathStr : cachedSoundFiles)
				{
					fs::path fullPath(fullPathStr);

					std::error_code relEc;
					fs::path rel = fs::relative(fullPath, soundRoot, relEc);

					std::string relStr;
					if (!relEc)
						relStr = NormalizePathSlashes(rel.generic_string());
					else
						relStr = StripPrefixIfPresent(fullPathStr, soundPrefix); // 念のため

					bool selected = (relStr == targetPath);
					if (ImGui::Selectable(relStr.c_str(), selected))
					{
						targetPath = relStr; // ← 保存は相対パス
						edited = true;
					}
					if (selected) ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			// 再生 / 停止 / Clear
			if (ImGui::Button(playBtnId))
			{
				if (hooks.PlaySoundPreviewSE && !targetPath.empty())
				{
					// フック側で "Resources/Sounds/" を前置する設計
					hooks.PlaySoundPreviewSE(targetPath);
				}
			}

			if (hooks.StopSoundPreviewSE)
			{
				ImGui::SameLine();
				if (ImGui::Button(stopBtnId))
				{
					hooks.StopSoundPreviewSE();
				}
			}

			ImGui::SameLine();
			if (ImGui::Button(clearBtnId))
			{
				targetPath.clear();
				edited = true;
			}
		};

	// 各サウンド項目
	DrawSoundPicker("発砲音", "発砲音ファイル", "再生##Fire", "停止##Fire", "Clear##Fire", data->soundData.fireSoundPath);
	DrawSoundPicker("リロード", "リロード音ファイル", "再生##Reload", "停止##Reload", "Clear##Reload", data->soundData.reloadSoundPath);
	DrawSoundPicker("空撃ち", "空撃ち音ファイル", "再生##Empty", "停止##Empty", "Clear##Empty", data->soundData.emptySoundPath);
	DrawSoundPicker("装備", "装備音ファイル", "再生##Equip", "停止##Equip", "Clear##Equip", data->soundData.equipSoundPath);
	DrawSoundPicker("ヒット", "ヒット音ファイル", "再生##Impact", "停止##Impact", "Clear##Impact", data->soundData.impactSoundPath);

	if (edited)
	{
		MarkEditedAndMaybeApply(hooks, selectedID_, *data);
	}

#else
	(void)data;
	(void)hooks;
#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///				　	VFXデータを描画するUI
/// -------------------------------------------------------------
void WeaponMasterDataEditor::DrawVfxData(FWeaponMasterData* data, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI

	if (ImGui::CollapsingHeader("VFX", ImGuiTreeNodeFlags_DefaultOpen))
	{
		namespace fs = std::filesystem;

		const fs::path vfxRoot = "Resources/VFX";
		const std::string vfxPrefix = "Resources/VFX/";

		// 既存データ（古い保存形式がフルパスでも相対化して吸収）
		auto NormalizeVfxPath = [&](std::string& s)
			{
				if (s.empty()) return;
				s = StripPrefixIfPresent(s, vfxPrefix);
			};

		NormalizeVfxPath(data->vfxData.muzzleFlashVfxPath);
		NormalizeVfxPath(data->vfxData.tracerVfxPath);
		NormalizeVfxPath(data->vfxData.impactVfxPath);
		NormalizeVfxPath(data->vfxData.shellEjectVfxPath);
		NormalizeVfxPath(data->vfxData.reloadVfxPath);
		NormalizeVfxPath(data->vfxData.chargeVfxPath);
		NormalizeVfxPath(data->vfxData.meleeSwingVfxPath);
		NormalizeVfxPath(data->vfxData.meleeHitVfxPath);

		// 対応拡張子（必要に応じて増やす）
		static const std::vector<std::string> kVfxExts = {
			".json", ".vfx", ".efk", ".efkefc", // VFX定義系
			".png", ".dds", ".jpg", ".jpeg", ".tga", ".bmp", // テクスチャ系
			".gltf", ".glb" // メッシュ系VFXを使う場合
		};

		static std::vector<std::string> cachedVfxFiles; // フルパスで保持
		static bool scannedVfx = false;

		auto RefreshVfxList = [&]()
			{
				cachedVfxFiles = CollectFilesRecursively(vfxRoot, kVfxExts);
				scannedVfx = true;
			};

		if (!scannedVfx) RefreshVfxList();

		ImGui::TextDisabled("Root: %s", vfxRoot.generic_string().c_str());
		ImGui::TextDisabled("CWD : %s", fs::current_path().generic_string().c_str());

		std::error_code ecRoot;
		if (!fs::exists(vfxRoot, ecRoot))
		{
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
				"※ フォルダが見つかりません: %s", vfxRoot.generic_string().c_str());
		}

		ImGui::SameLine();
		if (ImGui::Button("再スキャン##VFX"))
		{
			RefreshVfxList();
		}

		auto DrawVfxPicker = [&](const char* sectionLabel,
			const char* comboLabel,
			const char* clearBtnId,
			std::string& targetPath)
			{
				ImGui::SeparatorText(sectionLabel);

				ImGui::TextWrapped("現在: %s", targetPath.empty() ? "(未設定)" : targetPath.c_str());

				if (!targetPath.empty())
				{
					fs::path fullPath = vfxRoot / fs::path(targetPath);
					std::error_code ecFile;
					const bool exists = fs::exists(fullPath, ecFile);

					ImGui::SameLine();
					if (exists)
						ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "OK");
					else
						ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Missing");

					ImGui::Text("ファイル名: %s", fs::path(targetPath).filename().string().c_str());
				}

				const char* preview = targetPath.empty() ? "(未設定)" : targetPath.c_str();
				if (ImGui::BeginCombo(comboLabel, preview))
				{
					bool selNone = targetPath.empty();
					if (ImGui::Selectable("(未設定)", selNone))
					{
						targetPath.clear();
						MarkEditedAndMaybeApply(hooks, selectedID_, *data);
					}
					if (selNone) ImGui::SetItemDefaultFocus();

					for (const auto& fullPathStr : cachedVfxFiles)
					{
						fs::path fullPath(fullPathStr);

						std::error_code relEc;
						fs::path rel = fs::relative(fullPath, vfxRoot, relEc);

						std::string relStr = relEc
							? StripPrefixIfPresent(fullPathStr, vfxPrefix)
							: NormalizePathSlashes(rel.generic_string());

						bool selected = (relStr == targetPath);
						if (ImGui::Selectable(relStr.c_str(), selected))
						{
							targetPath = relStr; // ← 保存は相対パス
							MarkEditedAndMaybeApply(hooks, selectedID_, *data);
						}
						if (selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::SameLine();
				if (ImGui::Button(clearBtnId))
				{
					targetPath.clear();
					MarkEditedAndMaybeApply(hooks, selectedID_, *data);
				}
			};

		// 射撃系
		DrawVfxPicker("マズルフラッシュ", "マズルVFX", "Clear##MuzzleVfx", data->vfxData.muzzleFlashVfxPath);
		if (ImGui::DragFloat("マズルスケール", &data->vfxData.muzzleFlashScale, 0.01f, 0.0f, 100.0f))
			MarkEditedAndMaybeApply(hooks, selectedID_, *data);

		DrawVfxPicker("トレーサー", "トレーサーVFX", "Clear##TracerVfx", data->vfxData.tracerVfxPath);
		if (ImGui::DragFloat("トレーサースケール", &data->vfxData.tracerScale, 0.01f, 0.0f, 100.0f))
			MarkEditedAndMaybeApply(hooks, selectedID_, *data);

		DrawVfxPicker("ヒットVFX", "ヒットVFXファイル", "Clear##ImpactVfx", data->vfxData.impactVfxPath);
		if (ImGui::DragFloat("ヒットVFXスケール", &data->vfxData.impactScale, 0.01f, 0.0f, 100.0f))
			MarkEditedAndMaybeApply(hooks, selectedID_, *data);

		DrawVfxPicker("薬莢排出VFX", "薬莢VFX", "Clear##ShellVfx", data->vfxData.shellEjectVfxPath);
		DrawVfxPicker("リロードVFX", "リロードVFX", "Clear##ReloadVfx", data->vfxData.reloadVfxPath);
		DrawVfxPicker("チャージVFX", "チャージVFX", "Clear##ChargeVfx", data->vfxData.chargeVfxPath);

		// 近接時だけ近接VFXを見せる
		if (data->coreData.category == EWeaponCategory::Melee)
		{
			DrawVfxPicker("近接スイングVFX", "近接スイングVFX", "Clear##MeleeSwingVfx", data->vfxData.meleeSwingVfxPath);
			if (ImGui::DragFloat("近接スイングスケール", &data->vfxData.meleeSwingScale, 0.01f, 0.0f, 100.0f))
				MarkEditedAndMaybeApply(hooks, selectedID_, *data);

			DrawVfxPicker("近接ヒットVFX", "近接ヒットVFX", "Clear##MeleeHitVfx", data->vfxData.meleeHitVfxPath);
			if (ImGui::DragFloat("近接ヒットスケール", &data->vfxData.meleeHitScale, 0.01f, 0.0f, 100.0f))
				MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		}
	}

#else
	(void)data;
	(void)hooks;
#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///				　	ソケットデータを描画するUI
/// -------------------------------------------------------------
void WeaponMasterDataEditor::DrawSocketData(FWeaponMasterData* data, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("ソケット", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	bool edited = false;

	auto DrawSocketField = [&](const char* label,
		const char* idSuffix,
		std::string& target,
		const char* hint = nullptr)
		{
			ImGui::PushID(idSuffix);

			ImGui::Text("%s: %s", label, target.empty() ? "(未設定)" : target.c_str());
			if (hint && hint[0] != '\0')
			{
				ImGui::TextDisabled("%s", hint);
			}

			ImGui::SameLine();
			if (ImGui::Button("編集"))
			{
				std::snprintf(renameBuf_, sizeof(renameBuf_), "%s", target.c_str());
				ImGui::OpenPopup("ソケット名を編集");
			}

			ImGui::SameLine();
			if (ImGui::Button("Clear"))
			{
				target.clear();
				edited = true;
			}

			if (ImGui::BeginPopupModal("ソケット名を編集", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::TextUnformatted("ソケット名を入力");
				ImGui::InputText("##SocketName", renameBuf_, sizeof(renameBuf_));

				ImGui::Separator();
				if (ImGui::Button("OK"))
				{
					target = renameBuf_;
					edited = true;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("キャンセル"))
				{
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			ImGui::PopID();
		};

	ImGui::SeparatorText("共通");
	DrawSocketField("WeaponAttach", "weaponAttach", data->socketData.weaponAttachSocket, "武器を手に装備する基準ソケット");
	DrawSocketField("RightHand", "rightHand", data->socketData.rightHandSocket, "右手基準（必要なら）");
	DrawSocketField("LeftHandIK", "leftHandIK", data->socketData.leftHandIkSocket, "左手IK追従用");
	DrawSocketField("ADSCamera", "adsCamera", data->socketData.adsCameraSocket, "ADSカメラ基準（任意）");
	DrawSocketField("Magazine", "magazine", data->socketData.magazineSocket, "マガジン着脱用（任意）");

	const bool isMelee = (data->coreData.category == EWeaponCategory::Melee);

	if (!isMelee)
	{
		ImGui::SeparatorText("射撃");
		DrawSocketField("Muzzle", "muzzle", data->socketData.muzzleSocket, "発砲位置 / マズルフラッシュ");
		DrawSocketField("ShellEject", "shell", data->socketData.shellEjectSocket, "薬莢排出位置");
		DrawSocketField("TracerStart", "tracerStart", data->socketData.tracerStartSocket, "トレーサー開始位置（任意）");
		DrawSocketField("Scope", "scope", data->socketData.scopeSocket, "スコープ描画位置（任意）");
	}
	else
	{
		ImGui::SeparatorText("近接");
		DrawSocketField("MeleeTraceStart", "meleeStart", data->socketData.meleeTraceStartSocket, "近接判定の始点");
		DrawSocketField("MeleeTraceEnd", "meleeEnd", data->socketData.meleeTraceEndSocket, "近接判定の終点");
		DrawSocketField("MeleeHit", "meleeHit", data->socketData.meleeHitSocket, "近接ヒットVFX基準（任意）");
	}

	// 参考表示
	ImGui::SeparatorText("推奨例");
	if (!isMelee)
	{
		ImGui::BulletText("muzzle");
		ImGui::BulletText("shell_eject");
		ImGui::BulletText("hand_l_ik");
		ImGui::BulletText("weapon_root");
	}
	else
	{
		ImGui::BulletText("melee_trace_start");
		ImGui::BulletText("melee_trace_end");
		ImGui::BulletText("melee_hit");
	}

	if (edited)
	{
		MarkEditedAndMaybeApply(hooks, selectedID_, *data);
	}

#else
	(void)data;
	(void)hooks;
#endif
}

/// -------------------------------------------------------------
///				　	近接武器データを描画するUI
/// -------------------------------------------------------------
void WeaponMasterDataEditor::DrawMeleeData(FWeaponMasterData* data, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	// 念のためカテゴリ確認（DrawInspector側でも分岐してるけど保険）
	if (data->coreData.category != EWeaponCategory::Melee)
		return;

	if (!ImGui::CollapsingHeader("近接武器データ", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	// optionalが空なら自動生成
	if (!data->meleeData.has_value())
	{
		data->meleeData = FMeleeWeaponData{};
		MarkEditedAndMaybeApply(hooks, selectedID_, *data);
	}

	FMeleeWeaponData& m = *data->meleeData;
	bool edited = false;

	edited |= ImGui::DragFloat("ダメージ", &data->stats.damage, 0.1f, 0.0f, 100000.0f);

	// 近接は減衰を使わないなら最小ダメージを同値にしておくと分かりやすい
	edited |= ImGui::DragFloat("最小ダメージ", &data->stats.minDamage, 0.1f, 0.0f, 100000.0f);

	// 部位倍率（近接でも使うなら表示）
	edited |= ImGui::DragFloat("ヘッド倍率", &data->stats.headshotMultiplier, 0.01f, 0.0f, 10.0f);
	edited |= ImGui::DragFloat("胴体倍率", &data->stats.bodyMultiplier, 0.01f, 0.0f, 10.0f);
	edited |= ImGui::DragFloat("腕倍率", &data->stats.armMultiplier, 0.01f, 0.0f, 10.0f);
	edited |= ImGui::DragFloat("脚倍率", &data->stats.legMultiplier, 0.01f, 0.0f, 10.0f);

	// チャージON時の見た目用プレビュー（任意）
	if (m.bCanCharge)
	{
		const float chargedPreview = data->stats.damage * m.chargeDamageMultiplier;
		ImGui::Text("最大チャージ時ダメージ(目安): %.1f", chargedPreview);
	}

	ImGui::Spacing();

	ImGui::SeparatorText("攻撃範囲・判定");
	edited |= ImGui::DragFloat("攻撃範囲", &m.attackRange, 0.01f, 0.0f, 100.0f);
	edited |= ImGui::DragFloat("攻撃角度", &m.attackArc, 0.1f, 0.0f, 360.0f);
	edited |= ImGui::DragFloat("垂直角度", &m.verticalAngle, 0.1f, -180.0f, 180.0f);

	ImGui::SeparatorText("タイミング");
	edited |= ImGui::DragFloat("発生", &m.startupDelay, 0.001f, 0.0f, 10.0f);
	edited |= ImGui::DragFloat("持続", &m.activeFrames, 0.001f, 0.0f, 10.0f);
	edited |= ImGui::DragFloat("硬直", &m.recoveryDelay, 0.001f, 0.0f, 10.0f);

	ImGui::SeparatorText("コンボ");
	edited |= ImGui::DragInt("最大コンボ数", &m.maxComboCount, 1.0f, 1, 20);
	edited |= ImGui::DragFloat("コンボ猶予", &m.comboWindow, 0.001f, 0.0f, 5.0f);

	ImGui::SeparatorText("特殊アクション");
	edited |= ImGui::DragFloat("前進ダッシュ", &m.dashImpulse, 0.01f, 0.0f, 100.0f);
	edited |= ImGui::Checkbox("チャージ攻撃", &m.bCanCharge);

	ImGui::BeginDisabled(!m.bCanCharge);
	edited |= ImGui::DragFloat("最大チャージ時間", &m.maxChargeTime, 0.001f, 0.0f, 10.0f);
	edited |= ImGui::DragFloat("チャージ倍率", &m.chargeDamageMultiplier, 0.01f, 0.1f, 10.0f);
	ImGui::EndDisabled();

	ImGui::SeparatorText("防御");
	edited |= ImGui::Checkbox("ブロック可能", &m.bCanBlock);

	ImGui::BeginDisabled(!m.bCanBlock);
	edited |= ImGui::SliderFloat("ダメージ軽減率", &m.blockDamageReduction, 0.0f, 1.0f);
	ImGui::EndDisabled();

	ImGui::SeparatorText("ヒット挙動");
	edited |= ImGui::DragFloat("ヒットストップ", &m.hitStopDuration, 0.001f, 0.0f, 1.0f);
	edited |= ImGui::DragFloat("ノックバック", &m.knockbackForce, 0.01f, 0.0f, 10000.0f);

	// 軽い注意
	if (m.activeFrames > (m.recoveryDelay + 0.5f))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
			"※ 持続が長めです（意図的ならOK）");
	}

	if (edited)
	{
		MarkEditedAndMaybeApply(hooks, selectedID_, *data);
	}

#else
	(void)data;
	(void)hooks;
#endif
}

void WeaponMasterDataEditor::DrawOptionData(FWeaponMasterData* data, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("オプション", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	bool edited = false;
	auto Touch = [&]() { edited = true; };

	auto HasFireMode = [&](EFireMode mode) -> bool
		{
			if (data->defaultFireMode == mode) return true;
			return std::find(data->supportedFireModels.begin(), data->supportedFireModels.end(), mode)
				!= data->supportedFireModels.end();
		};

	// =========================================================
	// ✅ 射撃モード
	// =========================================================
	ImGui::SeparatorText("射撃モード");

	// デフォルト射撃モード
	{
		int cur = (int)data->defaultFireMode; // enumが 0.. の前提
		if (ImGui::Combo("デフォルト射撃モード", &cur, kFireModeNames, IM_ARRAYSIZE(kFireModeNames)))
		{
			data->defaultFireMode = (EFireMode)cur;
			Touch();
		}
	}

	// 対応射撃モード（チェックで追加/削除）
	{
		ImGui::TextUnformatted("対応射撃モード:");
		ImGui::Indent();
		for (int i = 0; i < (int)IM_ARRAYSIZE(kFireModeNames); ++i)
		{
			EFireMode m = (EFireMode)i;
			bool has = (std::find(data->supportedFireModels.begin(), data->supportedFireModels.end(), m) != data->supportedFireModels.end());

			// defaultFireMode は常に有効扱いにしたいならロックも可
			bool lock = (m == data->defaultFireMode);
			if (lock) ImGui::BeginDisabled(true);

			if (ImGui::Checkbox(kFireModeNames[i], &has))
			{
				if (has)
					data->supportedFireModels.push_back(m);
				else
					data->supportedFireModels.erase(std::remove(data->supportedFireModels.begin(), data->supportedFireModels.end(), m),
						data->supportedFireModels.end());
				Touch();
			}

			if (lock) ImGui::EndDisabled();
		}
		ImGui::Unindent();
	}

	// フル/セミ切替可能
	if (ImGui::Checkbox("フル/セミ切替可能", &data->bCanToggleFireMode)) Touch();

	// ✅ 整合性：オートONならFullAutoを必ず含める
	if (data->bIsAutomatic)
	{
		if (std::find(data->supportedFireModels.begin(), data->supportedFireModels.end(), EFireMode::FullAuto) == data->supportedFireModels.end()
			&& data->defaultFireMode != EFireMode::FullAuto)
		{
			ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "※ オート有効ですが FullAuto が対応モードにありません。自動で追加してください。");
		}
	}

	const bool supportsBurst = HasFireMode(EFireMode::Burst);
	const bool supportsCharge = HasFireMode(EFireMode::Charge);

	// =========================================================
	// 弾道・判定詳細（Projectile / Hitscan 共通の詳細）
	// =========================================================
	ImGui::SeparatorText("弾道・判定詳細");

	bool hasProjectileDetail = data->projectileData.has_value();
	if (ImGui::Checkbox("弾道・判定詳細を使う", &hasProjectileDetail))
	{
		if (hasProjectileDetail)
		{
			if (!data->projectileData.has_value())
				data->projectileData = FWeaponProjectileData{};
		}
		else
		{
			data->projectileData.reset();
		}
		Touch();
	}

	if (data->projectileData)
	{
		FWeaponProjectileData& p = *data->projectileData;

		ImGui::Indent();

		// ----- 共通（射程・基本） -----
		ImGui::SeparatorText("基本");
		if (ImGui::DragFloat("最大射程", &p.maxRange, 0.1f, 0.0f, 100000.0f)) Touch();
		if (ImGui::DragFloat("スポーン前方オフセット", &p.spawnForwardOffset, 0.01f, -100.0f, 100.0f)) Touch();

		if (ImGui::Checkbox("弾道武器（Projectile）", &p.bIsProjectile)) Touch();

		// ----- 弾道 or ヒットスキャン -----
		if (p.bIsProjectile)
		{
			ImGui::SeparatorText("弾道（Projectile）");
			if (ImGui::DragFloat("弾速", &p.projectileSpeed, 0.1f, 0.0f, 100000.0f)) Touch();
			if (ImGui::DragFloat("重力倍率", &p.gravityScale, 0.01f, 0.0f, 10.0f)) Touch();
			if (ImGui::DragFloat("寿命", &p.projectileLifeTime, 0.01f, 0.0f, 60.0f)) Touch();
			if (ImGui::DragFloat("空気抵抗", &p.projectileDrag, 0.001f, 0.0f, 100.0f)) Touch();
			if (ImGui::DragFloat("弾体半径", &p.projectileRadius, 0.001f, 0.0f, 100.0f)) Touch();
		}
		else
		{
			ImGui::SeparatorText("ヒットスキャン");
			if (ImGui::DragFloat("判定半径（0=線）", &p.traceRadius, 0.001f, 0.0f, 100.0f)) Touch();
		}

		// ----- 特殊判定 -----
		ImGui::SeparatorText("特殊判定");
		if (ImGui::DragInt("貫通回数", &p.pierceCount, 1.0f, 0, 1000)) Touch();
		if (ImGui::DragInt("反射回数", &p.ricochetCount, 1.0f, 0, 1000)) Touch();
		if (ImGui::DragFloat("爆風半径", &p.splashRadius, 0.01f, 0.0f, 10000.0f)) Touch();
		if (ImGui::Checkbox("自己ダメージ有効", &p.bCanDamageSelf)) Touch();

		// 軽いガイド
		if (p.bIsProjectile && p.projectileSpeed <= 0.0f)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
				"※ 弾道武器なのに弾速が0です");
		}
		if (!p.bIsProjectile && p.traceRadius > 0.0f)
		{
			ImGui::TextDisabled("判定半径 > 0 なのでショットガン/太い判定向けです");
		}

		ImGui::Unindent();
	}

	// =========================================================
	// バースト設定
	// =========================================================
	ImGui::SeparatorText("バースト設定");

	if (!supportsBurst)
	{
		ImGui::TextDisabled("※ 射撃モードに Burst が未設定です（設定は可能ですが、ランタイム側で使われない可能性あり）");
	}

	bool hasBurst = data->burstSettings.has_value();
	if (ImGui::Checkbox("バースト設定を使う", &hasBurst))
	{
		if (hasBurst)
		{
			if (!data->burstSettings.has_value())
			{
				data->burstSettings = FBurstSettings{};
				// 使いやすい初期値
				data->burstSettings->count = 3;
				data->burstSettings->interval = 0.08f;
			}
		}
		else
		{
			data->burstSettings.reset();
		}
		Touch();
	}

	if (data->burstSettings)
	{
		FBurstSettings& b = *data->burstSettings;
		ImGui::Indent();

		if (ImGui::DragInt("バースト射撃数", &b.count, 1.0f, 1, 20)) Touch();
		if (ImGui::DragFloat("バースト射撃間隔", &b.interval, 0.001f, 0.0f, 2.0f)) Touch();

		if (b.count <= 1)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
				"※ バースト射撃数は2以上推奨です");
		}

		ImGui::Unindent();
	}

	// =========================================================
	// チャージ設定
	// =========================================================
	ImGui::SeparatorText("チャージ設定");

	if (!supportsCharge)
	{
		ImGui::TextDisabled("※ 射撃モードに Charge が未設定です（設定は可能ですが、ランタイム側で使われない可能性あり）");
	}

	bool hasCharge = data->chargeSettings.has_value();
	if (ImGui::Checkbox("チャージ設定を使う", &hasCharge))
	{
		if (hasCharge)
		{
			if (!data->chargeSettings.has_value())
			{
				data->chargeSettings = FChargeSettings{};
				data->chargeSettings->maxChargeTime = 1.0f;
			}
		}
		else
		{
			data->chargeSettings.reset();
		}
		Touch();
	}

	if (data->chargeSettings)
	{
		FChargeSettings& c = *data->chargeSettings;
		ImGui::Indent();

		if (ImGui::DragFloat("最大チャージ時間", &c.maxChargeTime, 0.01f, 0.0f, 30.0f)) Touch();

		if (c.maxChargeTime <= 0.0f)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
				"※ チャージ時間が0です");
		}

		ImGui::Unindent();
	}

	if (edited)
	{
		MarkEditedAndMaybeApply(hooks, selectedID_, *data);
	}
#else
	(void)data;
	(void)hooks;
#endif // USE_IMGUI
}

void WeaponMasterDataEditor::DrawSpecialAndAutoData(FWeaponMasterData* data, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("特殊効果・オート設定", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Checkbox("オート有効化", &data->bIsAutomatic))
		{
			// オートONなら FullAuto を対応モードに入れる
			if (data->bIsAutomatic)
			{
				EnsureFireMode(data->supportedFireModels, EFireMode::FullAuto);

				// 必要ならデフォルトも寄せる
				// data->defaultFireMode = EFireMode::FullAuto;
			}

			MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		}

		ImGui::Separator();
		ImGui::TextUnformatted("特殊能力:");
		for (int i = 0; i < (int)IM_ARRAYSIZE(kAttrList); ++i)
		{
			const bool has = HasAttributes(data->attributes, kAttrList[i]);
			bool b = has;
			if (ImGui::Checkbox(kAttrNames[i], &b))
			{
				ToggleAttributes(data->attributes, kAttrList[i]);
				MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			}
		}
	}
#else
	(void)data;
	(void)hooks;
#endif // USE_IMGUI
}