#pragma once
#include <map>
#include <string>
#include <variant>
#include <json.hpp>
#include <limits>
#include <functional>
#include <type_traits>
#include <optional>
#include <vector>
#include <utility>

#include "Vector3.h"
#include "Vector4.h"

namespace Ken4lowEngine
{

	// コードの冗長を防ぐため省略
	using json = nlohmann::json;

	/// -------------------------------------------------------------
	///			　パラメータや調整項目を管理するクラス
	/// -------------------------------------------------------------
	/// ParameterManager は ImGui で調整する値を groupName/key 単位で保持し、
	/// Resources/ParameterManager/<groupName>.json へ保存・読み込みするための共有入口です。
	///
	/// 命名規則:
	/// - groupName は "機能名" または "カテゴリ/機能名" を使う。例: "LightManager", "GPUParticle/Hit"。
	/// - groupName の "/" は保存先サブディレクトリとして扱うため、チーム内で所有カテゴリを明確にする。
	/// - key は JSON 内の安定キーとして扱うため、表示名ではなく英数字中心の lowerCamelCase を推奨する。
	/// - 日本語表示や説明文は SetDisplayName へ分離し、既存 JSON 互換のため key 名変更を避ける。
	/// - groupName/key に空文字、制御文字、Windows ファイル名で危険な文字を使わない。
	///
	/// 反映タイミング:
	/// - 起動時の LoadFiles は JSON 値を ParameterManager に取り込むだけに留める。
	/// - 各機能は LightParameterController のような小さな Controller/Bridge で項目登録と Apply を担当する。
	/// - RegisterParameterApplier 登録後、必要な初期反映を Controller 側で一度だけ呼ぶ。
	/// - ImGui の Save/Load/Apply ボタンは ApplyParameters / ApplyAllParameters を明示的に呼ぶ。
	/// - ゲーム本体クラスが毎フレーム ParameterManager を直接読む実装は、段階的に Controller 経由へ寄せる。
	class ParameterManager
	{
	private: /// ---------- 構造体 ---------- ///

		using ItemValue = std::variant<int32_t, uint32_t, float, Vector3, Vector4, bool, std::string>;
		using RangeValue = std::variant<int32_t, uint32_t, float, Vector3>;

		// 項目ごとのImGui調整範囲
		struct Range
		{
			RangeValue min;
			RangeValue max;
		};

		// 項目構造体
		struct Item
		{
			// 項目の値
			ItemValue value;
			std::optional<Range> range;
			std::string displayName; // JSONキーとは別にImGui専用の表示名を保持する。
			std::vector<std::string> stringOptions; // string型をCombo表示するときの選択候補を保持する。
		};

		// グループ構造体
		struct Group
		{
			std::map<std::string, Item> items;
			std::function<void()> customDraw; // このグループ専用UI
			std::map<const void*, std::function<void()>> appliers; // 保存後に明示的に再取得して反映する最小構成の適用先。
		};

	public:
		/// Save/Load/Apply の結果を ImGui とログの両方で扱うための軽量ステータス。
		struct OperationStatus
		{
			bool succeeded = true;
			std::string message;
		};

	public: /// ---------- メンバ関数 ---------- ///

		// シングルトンインスタンス
		static ParameterManager* GetInstance();

		/// <summary>
		/// グループ作成
		/// </summary>
		/// <param name="groupName">グループ名</param>
		void CreateGroup(const std::string& groupName);

		/// <summary>
		/// 更新処理
		/// </summary>
		void Update(bool* pOpen = nullptr);

		/// <summary>
		/// ファイルに書き出し
		/// </summary>
		/// <param name="groupName">グループ名</param>
		/// <returns>保存できた場合はtrue、失敗時はfalse</returns>
		bool SaveFile(const std::string& groupName);

		/// <summary>
		/// ディレクトリの全ファイル読み込み
		/// </summary>
		bool LoadFiles();

		/// <summary>
		/// ファイルから読み込む
		/// </summary>
		/// <param name="groupName">グループ名</param>
		bool LoadFile(const std::string& groupName);

		// カスタム描画関数登録
		void RegisterCustomDraw(const std::string& groupName, std::function<void()> fn);

		/// <summary>保存済みパラメータをゲーム側へ反映する関数を登録します。</summary>
		void RegisterParameterApplier(const std::string& groupName, const void* owner, std::function<void()> fn);

		/// <summary>登録済みの反映関数を解除します。</summary>
		void UnregisterParameterApplier(const std::string& groupName, const void* owner);

		/// <summary>指定ownerで登録された全グループの反映関数を解除します。</summary>
		void UnregisterAllParameterAppliers(const void* owner);

		/// <summary>指定ownerの反映関数が登録済みか確認します。</summary>
		bool HasParameterApplier(const std::string& groupName, const void* owner) const;

		/// <summary>指定グループの保存済み値をゲーム側へ明示的に反映します。</summary>
		bool ApplyParameters(const std::string& groupName);

		/// <summary>全グループの保存済み値をゲーム側へ明示的に反映します。</summary>
		bool ApplyAllParameters();

	public: /// ---------- 項目の設定 ---------- ///

		/// <summary>
		/// 項目の値を設定するテンプレート関数
		/// </summary>
		/// <typeparam name="T">設定する値の型（int32_t, float, Vector3, bool のいずれか）</typeparam>
		/// <param name="groupName">設定対象のグループ名</param>
		/// <param name="key">設定対象の項目名</param>
		/// <param name="value">設定する値</param>
		template<typename T>
		void SetValue(const std::string& groupName, const std::string& key, const T& value)
		{
			static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float> ||
				std::is_same_v<T, Vector3> || std::is_same_v<T, Vector4> || std::is_same_v<T, bool> || std::is_same_v<T, std::string>,
				"Unsupported type for SetValue"); // サポートされていない型の場合はコンパイルエラー

			WarnIfNameLooksUnsafe(groupName, key); // 保存パスやJSONキーとして危険な名前を早めにログへ出す。

			// 既存項目の調整範囲を残したまま値だけ更新する。
			Group& group = datas_[groupName];
			Item& item = group.items[key];
			item.value = value;
			if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float> || std::is_same_v<T, Vector3>)
			{
				if (item.range && (!std::holds_alternative<T>(item.range->min) || !std::holds_alternative<T>(item.range->max)))
				{
					item.range.reset();
				}
			}
			else
			{
				item.range.reset();
			}
		}

	public: /// ---------- 項目の追加 ---------- ///

		/// <summary>
		/// 項目を追加するテンプレート関数
		/// </summary>
		/// <typeparam name="T">追加する値の型（int32_t, float, Vector3, bool のいずれか）</typeparam>
		/// <param name="groupName">追加対象のグループ名</param>
		/// <param name="key">追加する項目の名前</param>
		/// <param name="value">追加する項目の初期値</param>
		template<typename T>
		void AddItem(const std::string& groupName, const std::string& key, const T& value)
		{
			// キーが存在しない場合のみ項目を追加
			if (datas_[groupName].items.find(key) == datas_[groupName].items.end())
			{
				SetValue(groupName, key, value);
			}
		}

		/// <summary>
		/// ImGuiの調整範囲付きで項目を追加するテンプレート関数
		/// </summary>
		template<typename T>
		void AddItem(const std::string& groupName, const std::string& key, const T& value, const T& minValue, const T& maxValue)
		{
			static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float> || std::is_same_v<T, Vector3>,
				"Unsupported type for ranged AddItem"); // サポートされていない型の場合はコンパイルエラー

			AddItem(groupName, key, value);
			SetRange(groupName, key, minValue, maxValue);
		}

		/// <summary>
		/// 既存項目のImGui調整範囲を設定するテンプレート関数
		/// </summary>
		template<typename T>
		void SetRange(const std::string& groupName, const std::string& key, const T& minValue, const T& maxValue)
		{
			static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float> || std::is_same_v<T, Vector3>,
				"Unsupported type for SetRange"); // サポートされていない型の場合はコンパイルエラー

			Item& item = datas_[groupName].items[key];
			item.range = Range{ minValue, maxValue };
		}

		/// <summary>ImGui表示専用の日本語ラベルを設定します。</summary>
		void SetDisplayName(const std::string& groupName, const std::string& key, const std::string& displayName);

		/// <summary>std::string項目を選択式Comboとして追加します。</summary>
		void AddStringItem(const std::string& groupName, const std::string& key, const std::string& value, const std::vector<std::string>& options);

		/// <summary>既存のstd::string項目に選択候補を設定します。</summary>
		void SetStringOptions(const std::string& groupName, const std::string& key, const std::vector<std::string>& options);

	public: /// ---------- 項目の取得 ---------- ///

		/// <summary>
		/// 項目の値を取得するテンプレート関数
		/// </summary>
		/// <typeparam name="T">取得する値の型（int32_t, float, Vector3, bool のいずれか）</typeparam>
		/// <param name="groupName">取得対象のグループ名</param>
		/// <param name="key">取得対象の項目名</param>
		/// <returns>指定された型の値を返す</returns>
		/// <exception cref="std::runtime_error">
		/// グループやキーが見つからない場合、または型が一致しない場合にスローされる
		template<typename T>
		T GetValue(const std::string& groupName, const std::string& key) const
		{
			static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, float> ||
				std::is_same_v<T, Vector3> || std::is_same_v<T, Vector4> || std::is_same_v<T, bool> || std::is_same_v<T, std::string>,
				"Unsupported type for GetValue"); // サポートされていない型の場合はコンパイルエラー

			// グループが存在しない場合のエラーハンドリング
			auto groupIt = datas_.find(groupName);
			if (groupIt == datas_.end())
			{
				throw std::runtime_error("Group not found: " + groupName);
			}

			// キーが存在しない場合のエラーハンドリング
			const auto& group = groupIt->second;
			auto itemIt = group.items.find(key);
			if (itemIt == group.items.end())
			{
				throw std::runtime_error("Key not found: " + key);
			}

			// 型の一致を確認して値を取得
			const auto& item = itemIt->second;
			if (auto value = std::get_if<T>(&item.value))
			{
				return *value; // 値を返す
			}

			throw std::runtime_error("Type mismatch for key: " + key); // 型が一致しない場合
		}

	private: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// 指定されたアイテム名とアイテムデータを使用してアイテムを描画する関数。
		/// </summary>
		/// <param name="itemName">描画対象のアイテム名。入力専用の参照（const std::string&）</param>
		/// <param name="item">描画に使用するアイテムデータ。非const参照で渡され、関数内で読み取り／更新される可能性がある（ParameterManager::Item&）</param>
		void DrawItem(const std::string& itemName, ParameterManager::Item& item);

		/// <summary>ImGui表示用ラベルを内部キーから分離したID付きラベルとして返します。</summary>
		std::string BuildImGuiLabel(const std::string& itemName, const ParameterManager::Item& item) const;

		/// <summary>ImGui上に数秒だけステータスメッセージを表示します。</summary>
		void SetStatusMessage(const std::string& message, bool succeeded = true, float seconds = 3.0f);

		/// <summary>Save/Load/Applyの結果をログとImGuiステータスへまとめて反映します。</summary>
		void SetOperationStatus(bool succeeded, const std::string& message);

		/// <summary>groupName/keyの命名が保存・共有に不向きな場合だけ警告ログを出します。</summary>
		static void WarnIfNameLooksUnsafe(const std::string& groupName, const std::string& key = {});

	private: /// ---------- メンバ変数 ---------- ///

		// 全データ
		std::map<std::string, Group> datas_;

		// グローバル変数の保存先ファイルパス
		const std::string kDirectoryPath = "Resources/ParameterManager/";

		// Save/Load/Applyの結果をMessageBoxではなくImGui内に一時表示する。
		std::string statusMessage_;
		float statusMessageExpireTime_ = 0.0f;
		bool statusSucceeded_ = true;
		OperationStatus lastOperationStatus_{ true, "ParameterManager ready." };

	private: /// ---------- コピー禁止 ---------- ///

		ParameterManager() = default;
		~ParameterManager() = default;
		ParameterManager(const ParameterManager&) = delete;
		ParameterManager& operator=(const ParameterManager&) = delete;
	};

} // namespace Ken4lowEngine
