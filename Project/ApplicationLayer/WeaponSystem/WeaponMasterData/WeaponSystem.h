#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

#include "WeaponInstance.h"

#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterData.h"
#include "WeaponParams.h"

/// <summary>
/// 武器マスターデータを読み込み、装備中武器のランタイム状態へ適用するApplication層の管理クラス
/// JSON由来のFWeaponMasterDataをWeaponParamsへ変換し、WeaponInstanceへ渡すことでデータ駆動の武器挙動を成立させる
/// </summary>
class WeaponSystem
{
public:
	/// <summary>
	/// 武器JSONのルートを解決し、カテゴリ別に配置されたマスターデータを読み込む
	/// </summary>
	bool Load(const std::filesystem::path& rootDir, std::string* outError = nullptr);

	/// <summary>
	/// エディタ保存後の再読み込み時に、可能な限り現在装備中の武器IDを維持して再装備する
	/// </summary>
	bool ReloadAndReequip(std::string* outError = nullptr);

	/// <summary>
	/// 現在装備中の武器をDB上の最新データから再構築し、エディタ編集内容をランタイムへ反映する
	/// </summary>
	bool RebuildEquippedFromDatabase(std::string* outError = nullptr);

	/// <summary>
	/// ロード済みDBの先頭IDを装備し、ゲーム開始時の初期武器を確定する
	/// </summary>
	bool EquipFirst(std::string* outError = nullptr);

	bool EquipById(int32_t weaponId, std::string* outError = nullptr);

	void Tick(float dt) { weapon_.Tick(dt); }

	WeaponInstance& Weapon() { return weapon_; }
	const WeaponInstance& Weapon() const { return weapon_; }

	// エディタ側からマスターデータの追加・削除・編集を行うためにDB参照を公開する
	WeaponMasterDataDatabase& Database() { return db_; }
	const WeaponMasterDataDatabase& Database() const { return db_; }

	bool IsLoaded() const { return db_.IsLoaded(); }

	std::vector<int32_t> GetWeaponIdListSorted() const { return db_.GetSortedIDList(); }

	// UI上でカテゴリ別に武器を並べるため、指定カテゴリのIDを昇順で返す
	std::vector<int32_t> GetWeaponIdListSortedByCategory(EWeaponCategory category) const
	{
		return db_.GetSortedIDListByCategory(category);
	}

	int32_t GetEquippedWeaponId() const { return equippedWeaponId_; }
	bool HasEquippedWeapon() const { return equippedWeaponId_ > 0; }

	const FWeaponReticleData& GetEquippedReticleData() const { return equippedReticleData_; }

	bool EquipNext(std::string* outError = nullptr);
	bool EquipPrev(std::string* outError = nullptr);

private:
	static WeaponParams BuildParams(const FWeaponMasterData& md);
	static std::filesystem::path ResolveWeaponsRoot(const std::filesystem::path& inputPath);
	bool TryGetSortedWeaponIds(std::vector<int32_t>& ids, std::string* outError) const;
	bool EquipRelative(int direction, std::string* outError);

private:
	WeaponMasterDataDatabase db_{};
	WeaponInstance weapon_{};
	int32_t equippedWeaponId_ = 0;

	FWeaponReticleData equippedReticleData_{};
};
