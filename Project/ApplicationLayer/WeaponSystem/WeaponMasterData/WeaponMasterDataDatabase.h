#pragma once
#include <filesystem>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <vector>

#include "WeaponMasterData.h"

/// -------------------------------------------------------------
///				　		武器マスターデータベース
/// -------------------------------------------------------------
class WeaponMasterDataDatabase
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 指定したディレクトリからデータを読み込む関数。
	/// </summary>
	/// <param name="dirPath">読み込む対象のディレクトリを示すパス。</param>
	/// <param name="outError">エラーが発生した場合にエラーメッセージを格納するための出力用文字列へのポインタ。省略可能（nullptr可）。</param>
	/// <returns>読み込みに成功した場合はtrue、失敗した場合はfalseを返す。</returns>
	bool LoadFromDirectory(const std::filesystem::path& dirPath, std::string* outError = nullptr);

	/// <summary>
	/// リロード処理を実行します。
	/// </summary>
	/// <param name="outError">エラー発生時の説明を格納するための出力引数。省略可能（デフォルトはnullptr）。nullptr以外を渡すと、失敗時にエラーメッセージが設定されます。</param>
	/// <returns>処理が成功した場合はtrue、失敗した場合はfalseを返します。失敗時かつoutErrorが非nullであれば、outErrorにエラーメッセージが設定されます。</returns>
	bool Reload(std::string* outError = nullptr);

	/// <summary>
	/// オブジェクトやコンテナの内容をクリアする。
	/// </summary>
	void Clear();

public: /// ---------- エディタ操作 ---------- ///

	/// <summary>
	/// 新しい32ビットIDを生成して返すメンバー関数。オブジェクトの状態は変更しない（const）。
	/// </summary>
	/// <returns>生成された新しいIDをint32_t型で返します。</returns>
	int32_t CreateNewID();

	/// <summary>
	/// 指定されたIDのオブジェクトを複製し、新しいオブジェクトのIDを返します。
	/// </summary>
	/// <param name="srcID">複製元のオブジェクトまたはリソースの識別子。</param>
	/// <returns>複製されたオブジェクトの識別子。失敗時の戻り値は実装依存です。</returns>
	int32_t Duplicate(int32_t srcID);

	/// <summary>
	/// 指定された武器IDに対応する要素を削除します。
	/// </summary>
	/// <param name="weaponID">削除対象の武器を識別するID。</param>
	/// <returns>削除に成功した場合は true を返します。該当する要素が見つからないなどで削除できなかった場合は false を返します。</returns>
	bool RemoveByID(int32_t weaponID);

	/// <summary>
	/// 指定された名前を持つ新しい武器マスターデータを作成し、そのIDを返します。
	/// </summary>
	/// <param name="name">新しい武器マスターデータに設定する名前。</param>
	int32_t CreateNewWithName(const std::string& name);

	/// <summary>
	/// 「カテゴリに応じてデータを無効化/有効化」する整形関数（Editor側からも呼べるように公開）
	/// 近接：弾薬系を無効化、meleeData必須、弾道/バースト/チャージをnullopt
	/// 銃器：meleeDataをnullopt、弾薬系は最低限の初期値を補完
	/// </summary>
	static void NormalizeByCategory(FWeaponMasterData& data);

public: /// ---------- アクセサ ---------- ///

	/// <summary>
	/// 指定された武器IDに対応する武器マスターデータを検索します。const メンバ関数であり、呼び出し元オブジェクトを変更しません。
	/// </summary>
	/// <param name="weaponID">検索する武器の識別子 (int32_t)。</param>
	/// <returns>指定したIDに対応する const FWeaponMasterData へのポインタ。該当するデータが存在しない場合は nullptr を返します。</returns>
	const FWeaponMasterData* FindByID(int32_t weaponID) const;

	/// <summary>
	/// 指定した武器IDに対応する変更可能な FWeaponMasterData オブジェクトを検索して返します。
	/// </summary>
	/// <param name="weaponID">検索する武器の識別子（ID）。</param>
	/// <returns>一致する FWeaponMasterData へのポインタ。該当するデータが存在しない場合は nullptr を返します。</returns>
	FWeaponMasterData* FindMutableByID(int32_t weaponID);

	/// <summary>
	/// 指定した武器IDがコレクションに含まれているかを判定します（オブジェクトの状態は変更されません）。
	/// </summary>
	/// <param name="weaponID">判定する武器のID。</param>
	/// <returns>weaponIDが含まれていればtrue、含まれていなければfalse。</returns>
	bool ContainsID(int32_t weaponID) const;

	/// <summary>
	/// 指定した武器IDに対応する FWeaponMasterData への const 参照を取得します。これは const メンバ関数であり、呼び出してもオブジェクトの状態は変更されません。
	/// </summary>
	/// <param name="weaponID">取得対象の武器を識別する整数型のID。</param>
	/// <returns>該当する武器の FWeaponMasterData への const 参照。該当データが存在しない場合の動作は実装に依存します。</returns>
	const FWeaponMasterData& GetByID(int32_t weaponID) const;

	/// <summary>
	/// 武器マスターデータ（キー: int32_t、値: FWeaponMasterData）を格納する std::unordered_map への const 参照を返します。const メンバ関数であり、オブジェクトの状態は変更されません。
	/// </summary>
	/// <returns>std::unordered_map<int32_t, FWeaponMasterData> への const 参照。返された参照は元のオブジェクトが存続している間有効であり、呼び出し側は参照先を変更できません。</returns>
	const std::unordered_map<int32_t, FWeaponMasterData>& GetAll() const { return weaponDataMap_; }

	/// <summary>
	/// 昇順にソートされた ID のリストを返します。メソッドはオブジェクトの状態を変更しません。
	/// </summary>
	/// <returns>ソート済みの int32_t ID を格納した std::vector<int32_t> を値で返します（コピー）。要素は昇順に並んでいます。</returns>
	std::vector<int32_t> GetSortedIDList() const;

	/// <summary>
	/// 指定カテゴリーの武器IDを昇順にソートして返します。
	/// </summary>
	std::vector<int32_t> GetSortedIDListByCategory(EWeaponCategory category) const;

public: /// ---------- 状態 ---------- ///

	/// <summary>
	/// weaponDataMap_ に含まれる要素の数を返す const メンバ関数です。
	/// </summary>
	/// <returns>コンテナの要素数を表す size_t 値。</returns>
	size_t Size() const { return weaponDataMap_.size(); }

	/// <summary>
	/// オブジェクトがロード済みかどうかを判定します。
	/// </summary>
	/// <returns>ロードされている場合は true、そうでなければ false を返します。</returns>
	bool IsLoaded() const { return isLoaded_; }

	/// <summary>
	/// ソースディレクトリを表す std::filesystem::path への const 参照を取得します。
	/// </summary>
	/// <returns>sourceDirectory_（ソースディレクトリ）を指す std::filesystem::path 型の const 参照。参照の有効期間は呼び出し先オブジェクトのライフタイムに依存します。</returns>
	const std::filesystem::path& GetSourceDirectory() const { return sourceDirectory_; }

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 出力用のエラーメッセージを設定し、失敗を示す値を返す関数。
	/// </summary>
	/// <param name="outError">エラーメッセージを書き込むための std::string へのポインタ。メッセージの出力先として使用される。</param>
	/// <param name="msg">設定するエラーメッセージの内容を表す std::string_view。</param>
	/// <returns>失敗を示す bool 値（false）。</returns>
	static bool Fail(std::string* outError, std::string_view msg);

	/// <summary>
	/// 新しい一意のIDを生成して返します。オブジェクトの状態は変更しません（const）。
	/// </summary>
	/// <returns>生成された新しいID（int32_t）。</returns>
	int32_t GenerateNewID() const;

private: /// ---------- メンバ変数 ---------- ///

	std::filesystem::path sourceDirectory_;							// データのソースディレクトリ
	std::unordered_map<int32_t, FWeaponMasterData> weaponDataMap_;	// 武器ID → 武器マスターデータ マップ
	bool isLoaded_ = false;											// データがロード済みかどうか
};

