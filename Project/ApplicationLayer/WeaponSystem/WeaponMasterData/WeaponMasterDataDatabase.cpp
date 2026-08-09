#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterDataLoader.h"
#include "WeaponMasterDataValidator.h"

#include <string_view>
#include <array>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <filesystem>

#include "WeaponMasterData.h"

namespace
{
	void EnsureFireModeVectorValid(FWeaponMasterData& data)
	{
		// 空なら default を入れる
		if (data.supportedFireModels.empty())
		{
			data.supportedFireModels.push_back(data.defaultFireMode);
		}

		// 重複除去（順序維持）
		std::vector<EFireMode> uniqueModes;
		uniqueModes.reserve(data.supportedFireModels.size());

		for (auto m : data.supportedFireModels)
		{
			if (std::find(uniqueModes.begin(), uniqueModes.end(), m) == uniqueModes.end())
			{
				uniqueModes.push_back(m);
			}
		}
		data.supportedFireModels = std::move(uniqueModes);

		// default が含まれていなければ追加
		if (std::find(data.supportedFireModels.begin(), data.supportedFireModels.end(), data.defaultFireMode)
			== data.supportedFireModels.end())
		{
			data.supportedFireModels.push_back(data.defaultFireMode);
		}

		// フルオート互換フラグを同期（validatorの整合性対策）
		const bool hasFullAuto =
			(std::find(data.supportedFireModels.begin(), data.supportedFireModels.end(), EFireMode::FullAuto)
				!= data.supportedFireModels.end());

		data.bIsAutomatic = hasFullAuto;

		// 複数モードなら切替可能に寄せる（運用しやすくする）
		if (data.supportedFireModels.size() <= 1)
		{
			data.bCanToggleFireMode = false;
		}
	}

	void EnsureBurstChargeConsistency(FWeaponMasterData& data)
	{
		const bool hasBurst =
			(std::find(data.supportedFireModels.begin(), data.supportedFireModels.end(), EFireMode::Burst)
				!= data.supportedFireModels.end());

		const bool hasCharge =
			(std::find(data.supportedFireModels.begin(), data.supportedFireModels.end(), EFireMode::Charge)
				!= data.supportedFireModels.end());

		if (hasBurst)
		{
			if (!data.burstSettings.has_value())
			{
				data.burstSettings.emplace();
				data.burstSettings->count = 3;
				data.burstSettings->interval = 0.08f;
			}
			else
			{
				if (data.burstSettings->count <= 0) data.burstSettings->count = 3;
				if (data.burstSettings->interval < 0.0f) data.burstSettings->interval = 0.08f;
			}
		}
		else
		{
			data.burstSettings.reset();
		}

		if (hasCharge)
		{
			if (!data.chargeSettings.has_value())
			{
				data.chargeSettings.emplace();
				data.chargeSettings->maxChargeTime = 1.0f;
			}
			else
			{
				if (data.chargeSettings->maxChargeTime <= 0.0f)
					data.chargeSettings->maxChargeTime = 1.0f;
			}
		}
		else
		{
			data.chargeSettings.reset();
		}
	}
}

/// -------------------------------------------------------------
///		指定したディレクトリからデータを読み込む関数
/// -------------------------------------------------------------
bool WeaponMasterDataDatabase::LoadFromDirectory(const std::filesystem::path& dirPath, std::string* outError)
{
	namespace fs = std::filesystem;

	if (!fs::exists(dirPath))
	{
		return Fail(outError, "WeaponMasterDataDatabase: 指定されたディレクトリが存在しません: " + dirPath.string());
	}
	if (!fs::is_directory(dirPath))
	{
		return Fail(outError, "WeaponMasterDataDatabase: 指定されたパスはディレクトリではありません: " + dirPath.string());
	}

	// weapons 直下の旧式 json は読まない。カテゴリフォルダ配下のみ読む。
	struct CatFolder { std::string_view name; EWeaponCategory cat; };
	static constexpr std::array<CatFolder, 6> kCategoryFolders = {
		CatFolder{ "primary", EWeaponCategory::Primary },
		CatFolder{ "backup",  EWeaponCategory::Backup  },
		CatFolder{ "melee",   EWeaponCategory::Melee   },
		CatFolder{ "special", EWeaponCategory::Special },
		CatFolder{ "sniper",  EWeaponCategory::Sniper  },
		CatFolder{ "heavy",   EWeaponCategory::Heavy   },
	};

	std::unordered_map<int32_t, FWeaponMasterData> tempMap;
	std::unordered_map<int32_t, fs::path> idToFileMap; // 重複チェック用

	auto loadFolder = [&](const fs::path& folderPath, EWeaponCategory cat) -> bool
		{
			if (!fs::exists(folderPath) || !fs::is_directory(folderPath))
				return true; // 無いカテゴリはスキップ

			for (const auto& entry : fs::directory_iterator(folderPath))
			{
				if (!entry.is_regular_file()) continue;
				if (entry.path().extension() != ".json") continue;

				FWeaponMasterData weaponData;
				std::string errorMsg;
				if (!WeaponMasterDataLoader::LoadFromFile(entry.path(), weaponData, &errorMsg))
				{
					return Fail(outError, errorMsg + " (file: " + entry.path().string() + ")");
				}

				// フォルダ名をカテゴリとして採用（JSON側より優先）
				weaponData.coreData.category = cat;

				// カテゴリに応じた整形（必須）
				NormalizeByCategory(weaponData);

				// Validate（ローダー後の最終チェック）
				std::string validateErr;
				if (!WeaponMasterDataValidator::Validate(weaponData, &validateErr))
				{
					return Fail(outError,
						"WeaponMasterDataDatabase: validation failed\n"
						"file: " + entry.path().string() + "\n" + validateErr);
				}

				const int32_t weaponID = weaponData.coreData.weaponID;
				if (weaponID <= 0)
				{
					return Fail(outError, "WeaponMasterDataDatabase: weaponID must be > 0 (file: " + entry.path().string() + ")");
				}

				if (idToFileMap.contains(weaponID))
				{
					return Fail(outError,
						"WeaponMasterDataDatabase: weaponID duplicate: " + std::to_string(weaponID) +
						"\n  first : " + idToFileMap[weaponID].string() +
						"\n  second: " + entry.path().string());
				}

				idToFileMap.emplace(weaponID, entry.path());
				tempMap.emplace(weaponID, std::move(weaponData));
			}

			return true;
		};

	// weapons 直下は見ない。primary/backup/... の中だけ読む。
	for (const auto& cf : kCategoryFolders)
	{
		const fs::path folder = dirPath / std::string(cf.name);
		if (!loadFolder(folder, cf.cat))
		{
			return false;
		}
	}

	// 読めたものだけ反映
	weaponDataMap_ = std::move(tempMap);
	sourceDirectory_ = dirPath; // Reload は root(".../weapons") を基準にする
	isLoaded_ = true;
	return true;
}

/// -------------------------------------------------------------
///					リロード処理を実行します
/// -------------------------------------------------------------
bool WeaponMasterDataDatabase::Reload(std::string* outError)
{
	if (!isLoaded_ || sourceDirectory_.empty())
	{
		return Fail(outError, "WeaponMasterDataDatabase: データがロードされていないか、ソースディレクトリが不明です。");
	}
	return LoadFromDirectory(sourceDirectory_, outError);
}

/// -------------------------------------------------------------
///			オブジェクトやコンテナの内容をクリアする
/// -------------------------------------------------------------
void WeaponMasterDataDatabase::Clear()
{
	weaponDataMap_.clear();
	isLoaded_ = false;
	sourceDirectory_.clear();
}

/// -------------------------------------------------------------
///		新しい32ビットIDを生成して返すメンバー関数
/// -------------------------------------------------------------
int32_t WeaponMasterDataDatabase::CreateNewID()
{
	const int32_t id = GenerateNewID();

	FWeaponMasterData data{};
	data.coreData.weaponID = id;
	data.coreData.weaponName = "Weapon" + std::to_string(id);

	// 新規作成時の最低限の整合性
	data.defaultFireMode = EFireMode::SemiAuto;
	data.supportedFireModels = { EFireMode::SemiAuto };
	data.coreData.category = EWeaponCategory::Primary;

	NormalizeByCategory(data);

	weaponDataMap_[id] = std::move(data);
	return id;
}

/// -------------------------------------------------------------
///		指定されたIDのオブジェクトを複製し、新しいオブジェクトのIDを返します
/// -------------------------------------------------------------
int32_t WeaponMasterDataDatabase::Duplicate(int32_t srcID)
{
	auto it = weaponDataMap_.find(srcID);
	if (it == weaponDataMap_.end()) return 0;

	const int32_t id = GenerateNewID();

	FWeaponMasterData copy = it->second;
	copy.coreData.weaponID = id;

	// 名前をわかりやすく維持
	if (copy.coreData.weaponName.empty())
	{
		copy.coreData.weaponName = "Weapon" + std::to_string(id);
	}
	else
	{
		copy.coreData.weaponName += "_Copy";
	}

	NormalizeByCategory(copy);

	weaponDataMap_[id] = std::move(copy);
	return id;
}

/// -------------------------------------------------------------
///		指定された武器IDに対応する要素を削除
/// -------------------------------------------------------------
bool WeaponMasterDataDatabase::RemoveByID(int32_t weaponID)
{
	return weaponDataMap_.erase(weaponID) > 0;
}

/// -------------------------------------------------------------
///		指定された名前を持つ新しい武器マスターデータを作成し、そのIDを返す
/// -------------------------------------------------------------
int32_t WeaponMasterDataDatabase::CreateNewWithName(const std::string& name)
{
	const int32_t id = GenerateNewID();

	FWeaponMasterData data{};
	data.coreData.weaponID = id;
	data.coreData.weaponName = name.empty() ? ("Weapon" + std::to_string(id)) : name;

	// 新規作成時の最低限の整合性
	data.defaultFireMode = EFireMode::SemiAuto;
	data.supportedFireModels = { EFireMode::SemiAuto };
	data.coreData.category = EWeaponCategory::Primary;

	NormalizeByCategory(data);

	weaponDataMap_[id] = std::move(data);
	return id;
}

/// -------------------------------------------------------------
///		カテゴリに応じてデータを無効化/有効化する整形関数
/// -------------------------------------------------------------
void WeaponMasterDataDatabase::NormalizeByCategory(FWeaponMasterData& data)
{
	// まず fire mode 系を整える
	EnsureFireModeVectorValid(data);

	const bool isMelee = (data.coreData.category == EWeaponCategory::Melee);

	if (isMelee)
	{
		// 近接武器は meleeData 必須
		if (!data.meleeData.has_value())
		{
			data.meleeData.emplace();
		}

		// 近接武器は弾薬/弾道を無効化
		data.projectileData.reset();
		data.burstSettings.reset();
		data.chargeSettings.reset();

		data.stats.ammoType = EAmmoType::None;
		data.stats.capacity = 0;
		data.stats.maxReserveAmmo = 0;
		data.stats.bHasChamber = false;
		data.stats.chamberSize = 0;

		// 近接でも validator 上 ammoPerShot > 0 が必要なので 1 に補正
		if (data.stats.ammoPerShot <= 0) data.stats.ammoPerShot = 1;

		// 近接は射撃モードを固定（運用上の事故防止）
		data.defaultFireMode = EFireMode::SemiAuto;
		data.supportedFireModels.clear();
		data.supportedFireModels.push_back(EFireMode::SemiAuto);
		data.bCanToggleFireMode = false;
		data.bIsAutomatic = false;
	}
	else
	{
		// 銃器は meleeData を無効化
		data.meleeData.reset();

		// 弾薬の最低限補完
		if (data.stats.ammoType == EAmmoType::None)
		{
			data.stats.ammoType = EAmmoType::Default;
		}
		if (data.stats.capacity <= 0)
		{
			data.stats.capacity = 30;
		}
		if (data.stats.ammoPerShot <= 0)
		{
			data.stats.ammoPerShot = 1;
		}
		if (data.stats.maxReserveAmmo < 0)
		{
			data.stats.maxReserveAmmo = 0;
		}
		if (data.stats.chamberSize < 0)
		{
			data.stats.chamberSize = 0;
		}

		// projectileData は銃器なら持たせておくと runtime 側で扱いやすい（任意）
		if (!data.projectileData.has_value())
		{
			data.projectileData.emplace();
		}

		// 再度 fire mode を整形（default変更等の影響吸収）
		EnsureFireModeVectorValid(data);

		// Burst / Charge の optional を対応モードに合わせて整形
		EnsureBurstChargeConsistency(data);
	}
}

/// -------------------------------------------------------------
///		指定された武器IDに対応する武器マスターデータを検索
/// -------------------------------------------------------------
const FWeaponMasterData* WeaponMasterDataDatabase::FindByID(int32_t weaponID) const
{
	auto it = weaponDataMap_.find(weaponID);
	if (it == weaponDataMap_.end()) return nullptr;
	return &it->second;
}

/// -------------------------------------------------------------
///		指定した武器IDに対応する変更可能な FWeaponMasterData オブジェクトを検索
/// -------------------------------------------------------------
FWeaponMasterData* WeaponMasterDataDatabase::FindMutableByID(int32_t weaponID)
{
	auto it = weaponDataMap_.find(weaponID);
	if (it == weaponDataMap_.end()) return nullptr;
	return &it->second;
}

/// -------------------------------------------------------------
///		指定した武器IDがコレクションに含まれているかを判定
/// -------------------------------------------------------------
bool WeaponMasterDataDatabase::ContainsID(int32_t weaponID) const
{
	return weaponDataMap_.contains(weaponID);
}

/// -------------------------------------------------------------
///	指定した武器IDに対応する FWeaponMasterData への const 参照を取得
/// -------------------------------------------------------------
const FWeaponMasterData& WeaponMasterDataDatabase::GetByID(int32_t weaponID) const
{
	const FWeaponMasterData* data = FindByID(weaponID);

	if (!data)
	{
		throw std::runtime_error("WeaponMasterDataDatabase: 指定された weaponID に対応するデータが存在しません: " + std::to_string(weaponID));
	}
	return *data;
}

/// -------------------------------------------------------------
///			昇順にソートされた ID のリストを返す
/// -------------------------------------------------------------
std::vector<int32_t> WeaponMasterDataDatabase::GetSortedIDList() const
{
	std::vector<int32_t> ids;
	ids.reserve(weaponDataMap_.size());
	for (const auto& [id, _] : weaponDataMap_) ids.push_back(id);
	std::sort(ids.begin(), ids.end());
	return ids;
}

/// -------------------------------------------------------------
///		指定カテゴリーの武器IDを昇順ソートして返す
/// -------------------------------------------------------------
std::vector<int32_t> WeaponMasterDataDatabase::GetSortedIDListByCategory(EWeaponCategory category) const
{
	std::vector<int32_t> ids;
	ids.reserve(weaponDataMap_.size());

	for (const auto& [id, data] : weaponDataMap_)
	{
		if (data.coreData.category == category)
		{
			ids.push_back(id);
		}
	}
	std::sort(ids.begin(), ids.end());
	return ids;
}

/// -------------------------------------------------------------
///		エラーメッセージを設定し、失敗を示す false を返す
/// -------------------------------------------------------------
bool WeaponMasterDataDatabase::Fail(std::string* outError, std::string_view msg)
{
	if (outError) *outError = std::string(msg);
	return false;
}

/// -------------------------------------------------------------
///		新しい32ビットIDを生成して返すメンバー関数
/// -------------------------------------------------------------
int32_t WeaponMasterDataDatabase::GenerateNewID() const
{
	int32_t maxId = 0;
	for (const auto& [id, _] : weaponDataMap_)
	{
		maxId = std::max(maxId, id);
	}
	return maxId + 1;
}