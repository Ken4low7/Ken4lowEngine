#include "ParameterManager.h"
#include <ImGuiManager.h>
#include <fstream>
#include <LogString.h>
#include <exception>
#include <filesystem>
#include <system_error>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>


namespace
{

	bool HasInvalidGroupPathChars(const std::string& groupName)
	{
		for (unsigned char c : groupName)
		{
			if (c < 0x20)
			{
				return true;
			}

			switch (c)
			{
			case '<':
			case '>':
			case ':':
			case '"':
			case '\\':
			case '|':
			case '?':
			case '*':
				return true;
			default:
				break;
			}
		}

		return groupName.find("//") != std::string::npos;
	}

	bool HasControlChars(const std::string& text)
	{
		for (unsigned char c : text)
		{
			if (c < 0x20)
			{
				return true;
			}
		}
		return false;
	}

	bool IsFiniteJsonNumber(const nlohmann::json& value)
	{
		return value.is_number() && std::isfinite(value.get<double>());
	}

	bool TryReadVector3Json(const nlohmann::json& value, Ken4lowEngine::Vector3& outValue)
	{
		if (!value.is_array() || value.size() != 3) return false;
		if (!IsFiniteJsonNumber(value.at(0)) || !IsFiniteJsonNumber(value.at(1)) || !IsFiniteJsonNumber(value.at(2))) return false;
		outValue = { value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>() };
		return true;
	}

	bool TryReadVector4Json(const nlohmann::json& value, Ken4lowEngine::Vector4& outValue)
	{
		if (!value.is_array() || value.size() != 4) return false;
		if (!IsFiniteJsonNumber(value.at(0)) || !IsFiniteJsonNumber(value.at(1)) || !IsFiniteJsonNumber(value.at(2)) || !IsFiniteJsonNumber(value.at(3))) return false;
		outValue = { value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>(), value.at(3).get<float>() };
		return true;
	}

} // namespace

namespace Ken4lowEngine
{


	/// -------------------------------------------------------------
	///			　		シングルトンインスタンス
	/// -------------------------------------------------------------
	ParameterManager* ParameterManager::GetInstance()
	{
		static ParameterManager instance;
		return &instance;
	}


	/// -------------------------------------------------------------
	///			　			　グループの作成
	/// -------------------------------------------------------------
	void ParameterManager::CreateGroup(const std::string& groupName)
	{
		WarnIfNameLooksUnsafe(groupName);
		// 指定名のオブジェクトがなければ追加する
		datas_[groupName];
	}


	/// -------------------------------------------------------------
	///			　				更新処理
	/// -------------------------------------------------------------
	void ParameterManager::Update(bool* pOpen)
	{
#ifdef USE_IMGUI
		// WindowメニューのParameters表示フラグが閉じている間は編集UIを生成しない
		if (pOpen != nullptr && !*pOpen)
		{
			return;
		}

		if (!ImGui::Begin("Parameters", pOpen, ImGuiWindowFlags_MenuBar))
		{
			ImGui::End();
			return;
		}

		// --- Menu bar（必要なら） ---
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("ファイル"))
			{
				if (ImGui::MenuItem("すべて保存"))
				{
					bool allSaved = true;
					for (auto& [name, _] : datas_) { allSaved = SaveFile(name) && allSaved; }
					if (allSaved)
					{
						const bool applied = ApplyAllParameters(); // 全保存後に登録済みオブジェクトへ明示反映する。
						SetOperationStatus(applied, applied ? "すべて保存しました / ゲームに反映しました" : "保存後の反映で一部失敗しました。ログを確認してください");
					}
					else
					{
						SetOperationStatus(false, "保存に失敗しました。ログを確認してください");
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		// --- 検索 ---
		static char filter[64] = {};
		ImGui::InputTextWithHint("##filter", "グループ検索...", filter, IM_ARRAYSIZE(filter));
		if (!statusMessage_.empty() && ImGui::GetTime() < statusMessageExpireTime_)
		{
			// 操作結果はMessageBoxではなくParametersウィンドウ内に数秒表示する。
			const ImVec4 color = statusSucceeded_
				? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
				: ImVec4(1.0f, 0.45f, 0.25f, 1.0f);
			ImGui::TextColored(color, "%s", statusMessage_.c_str());
		}
		ImGui::TextDisabled("Last: %s", lastOperationStatus_.message.c_str());
		ImGui::Separator();

		// --- 選択グループ ---
		static std::string selectedGroup;
		if (selectedGroup.empty() && !datas_.empty())
		{
			selectedGroup = datas_.begin()->first;
		}

		// 2カラム（Docking不要）
		ImGui::Columns(2, "param_cols", true);
		ImGui::SetColumnWidth(0, 220.0f);

		// 左：グループ一覧（＝サブメニュー）
		ImGui::BeginChild("##groups", ImVec2(0, 0), true);
		for (auto& [groupName, group] : datas_)
		{
			if (filter[0] != '\0')
			{
				if (groupName.find(filter) == std::string::npos) continue;
			}

			bool selected = (selectedGroup == groupName);
			if (ImGui::Selectable(groupName.c_str(), selected))
			{
				selectedGroup = groupName;
			}
		}
		ImGui::EndChild();

		ImGui::NextColumn();

		// 右：選択グループの編集
		ImGui::BeginChild("##items", ImVec2(0, 0), true);

		auto it = datas_.find(selectedGroup);
		if (it != datas_.end())
		{
			auto& group = it->second;

			ImGui::Text("Group: %s", selectedGroup.c_str());
			if (ImGui::Button("保存"))
			{
				const bool saved = SaveFile(selectedGroup);
				if (saved)
				{
					const bool applied = ApplyParameters(selectedGroup); // 保存直後にゲーム側がParameterManagerから再取得して反映できるようにする。
					SetOperationStatus(applied, applied ? "保存しました / ゲームに反映しました" : "保存しました / 反映で失敗しました。ログを確認してください");
				}
				else
				{
					SetOperationStatus(false, "保存に失敗しました。ログを確認してください");
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("読み込み"))
			{
				const bool loaded = LoadFile(selectedGroup);
				if (loaded)
				{
					const bool applied = ApplyParameters(selectedGroup); // 読み込み後も既存成功挙動を保ちつつゲーム側へ明示反映する。
					SetOperationStatus(applied, applied ? "読み込みました / ゲームに反映しました" : "読み込みました / 反映で失敗しました。ログを確認してください");
				}
				else
				{
					SetOperationStatus(false, "読み込みに失敗しました。既定値を維持します");
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("反映"))
			{
				const bool applied = ApplyParameters(selectedGroup); // 保存せず現在のParameterManager値だけをゲーム側へ反映する。
				SetOperationStatus(applied, applied ? "ゲームに反映しました" : "反映に失敗しました。ログを確認してください");
			}
			ImGui::Separator();

			// 1) 自動パラメータ（CollisionManager方式）
			for (auto& [itemName, item] : group.items)
			{
				DrawItem(itemName, item);
			}

			// 2) カスタムUI（WinAppみたいにコンボ/ボタンが必要なもの）
			if (group.customDraw)
			{
				ImGui::Separator();
				group.customDraw();
			}
		}

		ImGui::EndChild();
		ImGui::Columns(1);

		ImGui::End();
#else
		(void)pOpen;
#endif
	}


	void ParameterManager::SetStatusMessage(const std::string& message, bool succeeded, float seconds)
	{
#ifdef USE_IMGUI
		// ImGui内で一時通知するため、現在時刻から表示期限を計算する。
		statusMessage_ = message;
		statusSucceeded_ = succeeded;
		statusMessageExpireTime_ = static_cast<float>(ImGui::GetTime()) + seconds;
#else
		(void)message;
		(void)succeeded;
		(void)seconds;
#endif
	}

	void ParameterManager::SetOperationStatus(bool succeeded, const std::string& message)
	{
		lastOperationStatus_ = { succeeded, message };
		SetStatusMessage(message, succeeded);
		Log(std::string("[ParameterManager] ") + (succeeded ? "Info: " : "Warning: ") + message + "\n");
	}

	void ParameterManager::WarnIfNameLooksUnsafe(const std::string& groupName, const std::string& key)
	{
		// groupNameは保存パスの一部、keyはJSONキーとして共有されるため、危険な文字だけ警告する。
		if (groupName.empty())
		{
			Log("[ParameterManager] Warning: empty groupName is not recommended.\n");
		}
		if (HasControlChars(groupName) || HasControlChars(key))
		{
			Log("[ParameterManager] Warning: groupName/key contains control characters. group=" + groupName + " key=" + key + "\n");
		}
		if (!key.empty() && (key.find('/') != std::string::npos || key.find('\\') != std::string::npos))
		{
			Log("[ParameterManager] Warning: key should not contain path separators. group=" + groupName + " key=" + key + "\n");
		}
	}

	std::string ParameterManager::BuildImGuiLabel(const std::string& itemName, const ParameterManager::Item& item) const
	{
		// 日本語ラベル未設定時は従来通り内部キーをImGui表示にも使う。
		const std::string& visibleName = item.displayName.empty() ? itemName : item.displayName;
		return visibleName + "##" + itemName;
	}

	void ParameterManager::SetDisplayName(const std::string& groupName, const std::string& key, const std::string& displayName)
	{
		// JSON保存キーを変えず、ImGui表示専用ラベルだけを差し替える。
		datas_[groupName].items[key].displayName = displayName;
	}

	void ParameterManager::AddStringItem(const std::string& groupName, const std::string& key, const std::string& value, const std::vector<std::string>& options)
	{
		AddItem(groupName, key, value);
		SetStringOptions(groupName, key, options);
	}

	void ParameterManager::SetStringOptions(const std::string& groupName, const std::string& key, const std::vector<std::string>& options)
	{
		Item& item = datas_[groupName].items[key];
		item.stringOptions = options; // モデルやテクスチャの候補をImGuiのComboで選べるようにする。
	}


	/// -------------------------------------------------------------
	///			　			ファイルを保存する処理
	/// -------------------------------------------------------------
	bool ParameterManager::SaveFile(const std::string& groupName)
	{
		// グループ検索
		auto itGroup = datas_.find(groupName);

		// 未登録グループでは保存対象が無いため、停止せずログを残して呼び出し側へ失敗を返す。
		if (itGroup == datas_.end())
		{
			const std::string filePath = kDirectoryPath + groupName + ".json";
			SetOperationStatus(false, "未登録グループの保存に失敗しました: " + groupName + " (path: " + filePath + ")");
			return false;
		}

		// JSONオブジェクト作成
		json root = json::object();
		root[groupName] = json::object();

		// グループの全項目をループ
		for (const auto& [itemName, item] : itGroup->second.items)
		{
			/// ---------- int32_t型を保持している場合 ---------- ///
			if (std::holds_alternative<int32_t>(item.value))
			{
				root[groupName][itemName] = std::get<int32_t>(item.value);
			}

			/// ---------- uint32_t型を保持している場合 ---------- ///
			else if (std::holds_alternative<uint32_t>(item.value))
			{
				root[groupName][itemName] = std::get<uint32_t>(item.value);
			}

			/// ---------- float型を保持している場合 ---------- ///
			else if (std::holds_alternative<float>(item.value))
			{
				root[groupName][itemName] = std::get<float>(item.value);
			}

			/// ---------- Vector3を保持している場合 ---------- ///
			else if (std::holds_alternative<Vector3>(item.value))
			{
				const Vector3& vec = std::get<Vector3>(item.value);
				root[groupName][itemName] = json::array({ vec.x, vec.y, vec.z });
			}

			/// ---------- Vector4を保持している場合 ---------- ///
			else if (std::holds_alternative<Vector4>(item.value))
			{
				const Vector4& vec = std::get<Vector4>(item.value);
				root[groupName][itemName] = json::array({ vec.x, vec.y, vec.z, vec.w });
			}

			/// ---------- bool型を保持している場合 ---------- ///
			else if (std::holds_alternative<bool>(item.value))
			{
				root[groupName][itemName] = std::get<bool>(item.value);
			}

			/// ---------- string型を保持している場合 ---------- ///
			else if (std::holds_alternative<std::string>(item.value))
			{
				root[groupName][itemName] = std::get<std::string>(item.value);
			}
		}

		// 親ディレクトリが無い環境でも保存できるよう、保存先ディレクトリを再帰的に作成する。
		// "GPUParticle/Hit" のように階層付きグループ名を使う場合も、中間フォルダまで作る。
		const std::string filePath = kDirectoryPath + groupName + ".json";
		std::filesystem::path dir = std::filesystem::path(filePath).parent_path();
		std::error_code errorCode;
		if (!std::filesystem::exists(dir, errorCode))
		{
			std::filesystem::create_directories(dir, errorCode);
		}

		if (errorCode)
		{
			SetOperationStatus(false, "保存先ディレクトリ作成に失敗しました: " + dir.string() + ": " + errorCode.message());
			return false;
		}

		if (!std::filesystem::is_directory(dir, errorCode))
		{
			SetOperationStatus(false, "保存先がディレクトリではありません: " + dir.string());
			return false;
		}

		if (HasInvalidGroupPathChars(groupName))
		{
			Log("[ParameterManager] Warning: groupName contains characters that may be invalid for a file name: " + groupName + " (path: " + filePath + ")\n");
		}

		// ファイルを開けない場合も停止せず、保存できなかったパスをログに残す。
		std::ofstream ofs(filePath);
		if (ofs.fail())
		{
			SetOperationStatus(false, "保存ファイルを開けません: " + filePath);
			return false;
		}

		// JSONデータをファイルに書き込む
		ofs << std::setw(4) << root << std::endl;
		if (ofs.fail())
		{
			SetOperationStatus(false, "保存ファイルへの書き込みに失敗しました: " + filePath);
			return false;
		}

		ofs.close();
		if (ofs.fail())
		{
			SetOperationStatus(false, "保存ファイルのクローズに失敗しました: " + filePath);
			return false;
		}

		SetOperationStatus(true, "保存しました: " + groupName);
		return true;
	}


	/// -------------------------------------------------------------
	///			　		　全グループの読み込み処理
	/// -------------------------------------------------------------
	bool ParameterManager::LoadFiles()
	{
		// ディレクトリがなければスキップ
		std::filesystem::path dir(kDirectoryPath);
		std::error_code errorCode;
		if (!std::filesystem::exists(kDirectoryPath, errorCode))
		{
			SetOperationStatus(false, "ParameterManager保存先が見つかりません。既定値で起動します: " + kDirectoryPath);
			return false;
		}
		if (errorCode)
		{
			SetOperationStatus(false, "ParameterManager保存先の確認に失敗しました。既定値で起動します: " + errorCode.message());
			return false;
		}

		// 各ファイルの処理。階層付きグループ名（例: GPUParticle/Hit）も拾うため再帰的に見る。
		bool allLoaded = true;
		std::filesystem::recursive_directory_iterator dir_it(kDirectoryPath, std::filesystem::directory_options::skip_permission_denied, errorCode);
		if (errorCode)
		{
			SetOperationStatus(false, "ParameterManager保存先を走査できません。既定値で起動します: " + errorCode.message());
			return false;
		}

		for (const std::filesystem::directory_entry& entry : dir_it)
		{
			// ファイルパスを取得
			const std::filesystem::path& filePath = entry.path();

			// ファイル拡張子を取得
			std::string extension = filePath.extension().string();

			// .jsonファイル以外はスキップ
			if (extension.compare(".json") != 0) continue;

			// ファイル読み込み。保存先ディレクトリからの相対パスをグループ名へ戻す。
			std::filesystem::path relativePath = std::filesystem::relative(filePath, kDirectoryPath);
			relativePath.replace_extension();
			allLoaded = LoadFile(relativePath.generic_string()) && allLoaded;
		}

		SetOperationStatus(allLoaded, allLoaded ? "ParameterManager JSONを読み込みました" : "一部ParameterManager JSONの読み込みに失敗しました。既定値を維持します");
		return allLoaded;
	}


	/// -------------------------------------------------------------
	///			　		　各グループの読み込み処理
	/// -------------------------------------------------------------
	bool ParameterManager::LoadFile(const std::string& groupName)
	{
		// 読み込むJSONファイルのフルパスを合成する
		std::string filePath = kDirectoryPath + groupName + ".json";

		// 読み込み用ファイルストリーム
		std::ifstream ifs;

		// ファイルを読み込み用に開く
		ifs.open(filePath);

		// ファイルオープンが失敗した場合
		if (ifs.fail())
		{
			// 読み込み失敗時も既定値で続行できるようにログだけ残す。
			SetOperationStatus(false, "読み込み失敗。既定値を維持します: " + filePath);
			return false;
		}

		// JSON文字列の読み込み
		json root;

		// json文字列からjsonのデータ構造に展開し、失敗時は既定値で続行できるようにする。
		try
		{
			ifs >> root;
		}
		catch (const std::exception& e)
		{
			SetOperationStatus(false, "JSON解析失敗。既定値を維持します: " + filePath + ": " + e.what());
			ifs.close();
			return false;
		}

		// ファイルを閉じる
		ifs.close();

		// グループを検索
		json::iterator itGroup = root.find(groupName);

		// グループが無い場合も既定値で続行できるようにログだけ残す。
		if (itGroup == root.end())
		{
			SetOperationStatus(false, "JSON内にグループが見つかりません。既定値を維持します: " + groupName);
			return false;
		}

		if (!itGroup->is_object())
		{
			SetOperationStatus(false, "JSONグループがobjectではありません。既定値を維持します: " + groupName);
			return false;
		}

		// 各アイテムについて
		bool allItemsLoaded = true;
		for (json::iterator itItem = itGroup->begin(); itItem != itGroup->end(); ++itItem)
		{
			// アイテム名を取得
			const std::string& itemName = itItem.key();

			auto dataGroupIt = datas_.find(groupName);
			auto dataItemIt = (dataGroupIt != datas_.end()) ? dataGroupIt->second.items.find(itemName) : datas_[groupName].items.end();
			if (dataGroupIt != datas_.end() && dataItemIt != dataGroupIt->second.items.end())
			{
				const ItemValue& currentValue = dataItemIt->second.value;
				try
				{
					// 既にコード側で登録済みの項目は、その型を優先して読み込む。
					// Jsonの整数は符号付き/符号なしの判定だけでは既存型を復元できないため、ここで型崩れを防ぐ。
					if (std::holds_alternative<uint32_t>(currentValue) && itItem->is_number_integer())
					{
						const int64_t raw = itItem->get<int64_t>();
						if (raw >= 0 && raw <= static_cast<int64_t>((std::numeric_limits<uint32_t>::max)()))
						{
							SetValue(groupName, itemName, static_cast<uint32_t>(raw));
							continue;
						}
					}
					if (std::holds_alternative<int32_t>(currentValue) && itItem->is_number_integer())
					{
						const int64_t raw = itItem->get<int64_t>();
						if (raw >= static_cast<int64_t>((std::numeric_limits<int32_t>::min)()) && raw <= static_cast<int64_t>((std::numeric_limits<int32_t>::max)()))
						{
							SetValue(groupName, itemName, static_cast<int32_t>(raw));
							continue;
						}
					}
					if (std::holds_alternative<float>(currentValue) && IsFiniteJsonNumber(*itItem))
					{
						SetValue(groupName, itemName, itItem->get<float>());
						continue;
					}
					if (std::holds_alternative<Vector3>(currentValue))
					{
						Vector3 value{};
						if (TryReadVector3Json(*itItem, value))
						{
							SetValue(groupName, itemName, value);
							continue;
						}
					}
					if (std::holds_alternative<Vector4>(currentValue))
					{
						Vector4 value{};
						if (TryReadVector4Json(*itItem, value))
						{
							SetValue(groupName, itemName, value);
							continue;
						}
					}
					if (std::holds_alternative<bool>(currentValue) && itItem->is_boolean())
					{
						SetValue(groupName, itemName, itItem->get<bool>());
						continue;
					}
					if (std::holds_alternative<std::string>(currentValue) && itItem->is_string())
					{
						SetValue(groupName, itemName, itItem->get<std::string>());
						continue;
					}
				} catch (const std::exception& e)
				{
					Log("[ParameterManager] Failed to read typed item: " + itemName + " from " + filePath + ": " + e.what() + "\n");
					allItemsLoaded = false;
					continue;
				}

				Log("[ParameterManager] Type mismatch. Keep default value: group=" + groupName + " key=" + itemName + " file=" + filePath + "\n");
				allItemsLoaded = false;
				continue;
			}

			/// ---------- int32_t型を保持している場合 ---------- ///
			if (itItem->is_number_integer())
			{
				const int64_t raw = itItem->get<int64_t>();
				if (raw >= static_cast<int64_t>((std::numeric_limits<int32_t>::min)()) && raw <= static_cast<int64_t>((std::numeric_limits<int32_t>::max)()))
				{
					SetValue(groupName, itemName, static_cast<int32_t>(raw));
				}
				else
				{
					Log("[ParameterManager] Integer out of int32 range. Skip item: " + itemName + " in " + filePath + "\n");
					allItemsLoaded = false;
				}
			}

			/// ---------- uint32_t型を保持している場合 ---------- ///
			else if (itItem->is_number_unsigned())
			{
				uint32_t value = itItem->get<uint32_t>();
				SetValue(groupName, itemName, value);
			}

			/// ---------- float型を保持している場合 ---------- ///
			else if (itItem->is_number_float())
			{
				if (IsFiniteJsonNumber(*itItem))
				{
					float value = itItem->get<float>();
					SetValue(groupName, itemName, value);
				}
				else
				{
					Log("[ParameterManager] Non-finite float. Skip item: " + itemName + " in " + filePath + "\n");
					allItemsLoaded = false;
				}
			}

			/// ---------- 要素数3の配列である場合 ---------- ///
			else if (itItem->is_array() && itItem->size() == 3)
			{
				Vector3 value{};
				if (TryReadVector3Json(*itItem, value))
				{
					SetValue(groupName, itemName, value);
				}
				else
				{
					Log("[ParameterManager] Invalid Vector3 array. Skip item: " + itemName + " in " + filePath + "\n");
					allItemsLoaded = false;
				}
			}

			/// ---------- 要素数4の配列である場合 ---------- ///
			else if (itItem->is_array() && itItem->size() == 4)
			{
				Vector4 value{};
				if (TryReadVector4Json(*itItem, value))
				{
					SetValue(groupName, itemName, value);
				}
				else
				{
					Log("[ParameterManager] Invalid Vector4 array. Skip item: " + itemName + " in " + filePath + "\n");
					allItemsLoaded = false;
				}
			}

			/// ---------- bool型を保持している場合 ---------- ///
			else if (itItem->is_boolean())
			{
				bool value = itItem->get<bool>();
				SetValue(groupName, itemName, value);
			}

			/// ---------- string型を保持している場合 ---------- ///
			else if (itItem->is_string())
			{
				std::string value = itItem->get<std::string>();
				SetValue(groupName, itemName, value);
			}
			else
			{
				// 不明な型は既定値を残して原因をログに出す。
				Log("[ParameterManager] Unknown data type in file: " + filePath + " for item: " + itemName + "\n");
				allItemsLoaded = false;
			}
		}

		SetOperationStatus(allItemsLoaded, allItemsLoaded ? "読み込みました: " + groupName : "一部項目をスキップして読み込みました: " + groupName);
		return allItemsLoaded;
	}

	void ParameterManager::RegisterCustomDraw(const std::string& groupName, std::function<void()> fn)
	{
		CreateGroup(groupName);                 // グループが無ければ作る（＝左メニューに出る）
		datas_[groupName].customDraw = std::move(fn);
	}

	void ParameterManager::RegisterParameterApplier(const std::string& groupName, const void* owner, std::function<void()> fn)
	{
		if (owner == nullptr || !fn)
		{
			Log("[ParameterManager] Warning: ignored invalid applier registration. group=" + groupName + "\n");
			return;
		}
		CreateGroup(groupName);
		datas_[groupName].appliers[owner] = std::move(fn); // 所有者単位で上書き登録し、同一オブジェクトの重複呼び出しを避ける。
	}

	void ParameterManager::UnregisterParameterApplier(const std::string& groupName, const void* owner)
	{
		auto groupIt = datas_.find(groupName);
		if (groupIt == datas_.end())
		{
			return;
		}
		groupIt->second.appliers.erase(owner); // 破棄済みオブジェクトへ反映しないよう登録を解除する。
	}

	void ParameterManager::UnregisterAllParameterAppliers(const void* owner)
	{
		if (!owner)
		{
			return;
		}

		// 複数グループに同じownerで登録した機能が、Finalize/デストラクタでまとめて解除できる保険。
		for (auto& [_, group] : datas_)
		{
			group.appliers.erase(owner);
		}
	}

	bool ParameterManager::HasParameterApplier(const std::string& groupName, const void* owner) const
	{
		auto groupIt = datas_.find(groupName);
		if (groupIt == datas_.end())
		{
			return false;
		}
		return groupIt->second.appliers.find(owner) != groupIt->second.appliers.end();
	}

	bool ParameterManager::ApplyParameters(const std::string& groupName)
	{
		auto groupIt = datas_.find(groupName);
		if (groupIt == datas_.end())
		{
			SetOperationStatus(false, "未登録グループの反映に失敗しました: " + groupName);
			return false;
		}

		std::vector<std::pair<const void*, std::function<void()>>> applierSnapshot;
		applierSnapshot.reserve(groupIt->second.appliers.size());
		for (const auto& [owner, applier] : groupIt->second.appliers)
		{
			applierSnapshot.emplace_back(owner, applier);
		}

		bool succeeded = true;
		for (auto& [owner, applier] : applierSnapshot)
		{
			// Apply中に対象がUnregisterされた場合は、スナップショットに残っていても呼ばない。
			if (!HasParameterApplier(groupName, owner))
			{
				continue;
			}
			try
			{
				applier(); // 利用側がGetValueで再取得し、ゲーム内の実体へ反映する。
			}
			catch (const std::exception& e)
			{
				Log("[ParameterManager] Failed to apply group: " + groupName + ": " + e.what() + "\n");
				succeeded = false;
			}
			catch (...)
			{
				Log("[ParameterManager] Failed to apply group with unknown exception: " + groupName + "\n");
				succeeded = false;
			}
		}
		if (!succeeded)
		{
			SetOperationStatus(false, "反映で一部失敗しました: " + groupName);
		}
		return succeeded;
	}

	bool ParameterManager::ApplyAllParameters()
	{
		bool succeeded = true;
		for (const auto& [groupName, _] : datas_)
		{
			succeeded = ApplyParameters(groupName) && succeeded; // 全グループを順番に明示反映し、一部失敗も呼び出し側へ伝える。
		}
		return succeeded;
	}


	/// -------------------------------------------------------------
	///                 アイテムを描画する関数
	/// -------------------------------------------------------------
	void ParameterManager::DrawItem(const std::string& itemName, ParameterManager::Item& item)
	{
#ifdef USE_IMGUI
		constexpr int32_t kDefaultIntMin = 0;
		constexpr int32_t kDefaultIntMax = 10000;
		constexpr uint32_t kDefaultUIntMin = 0u;
		constexpr uint32_t kDefaultUIntMax = 10000u;
		constexpr float kDefaultFloatMin = 0.0f;
		constexpr float kDefaultFloatMax = 10000.0f;

		const std::string imguiLabel = BuildImGuiLabel(itemName, item);

		/// ---------- int32_t型を保持している場合 ---------- ///
		if (std::holds_alternative<int32_t>(item.value))
		{
			int32_t& value = std::get<int32_t>(item.value);
			int32_t minValue = kDefaultIntMin;
			int32_t maxValue = kDefaultIntMax;
			if (item.range && std::holds_alternative<int32_t>(item.range->min) && std::holds_alternative<int32_t>(item.range->max))
			{
				minValue = std::get<int32_t>(item.range->min);
				maxValue = std::get<int32_t>(item.range->max);
			}
			int sliderValue = static_cast<int>(value);
			// SliderIntはint範囲内の実用的なmin/maxだけを渡し、符号なし最大値相当の値を避ける。
			if (ImGui::SliderInt(imguiLabel.c_str(), &sliderValue, static_cast<int>(minValue), static_cast<int>(maxValue)))
			{
				value = static_cast<int32_t>(sliderValue);
			}
		}
		/// ---------- uint32_t型を保持している場合 ---------- ///
		else if (std::holds_alternative<uint32_t>(item.value))
		{
			uint32_t& value = std::get<uint32_t>(item.value);
			uint32_t minValue = kDefaultUIntMin;
			uint32_t maxValue = kDefaultUIntMax;
			if (item.range && std::holds_alternative<uint32_t>(item.range->min) && std::holds_alternative<uint32_t>(item.range->max))
			{
				minValue = std::get<uint32_t>(item.range->min);
				maxValue = std::get<uint32_t>(item.range->max);
			}
			float speed = 1.0f;
			// uint32_tはSliderIntに渡さず、U32対応のDragScalarで符号なし範囲を安全に扱う。
			ImGui::DragScalar(imguiLabel.c_str(), ImGuiDataType_U32, &value, speed, &minValue, &maxValue);
		}
		/// ---------- float型を保持している場合 ---------- ///
		else if (std::holds_alternative<float>(item.value))
		{
			float& value = std::get<float>(item.value);
			float minValue = kDefaultFloatMin;
			float maxValue = kDefaultFloatMax;
			if (item.range && std::holds_alternative<float>(item.range->min) && std::holds_alternative<float>(item.range->max))
			{
				minValue = std::get<float>(item.range->min);
				maxValue = std::get<float>(item.range->max);
			}
			// SliderFloatは巨大すぎる最大値ではなく、指定範囲または安全な既定範囲で調整しやすくする。
			ImGui::SliderFloat(imguiLabel.c_str(), &value, minValue, maxValue);
		}
		/// ---------- Vector3を保持している場合 ---------- ///
		else if (std::holds_alternative<Vector3>(item.value))
		{
			Vector3& value = std::get<Vector3>(item.value);
			Vector3 minValue = { kDefaultFloatMin, kDefaultFloatMin, kDefaultFloatMin };
			Vector3 maxValue = { kDefaultFloatMax, kDefaultFloatMax, kDefaultFloatMax };
			if (item.range && std::holds_alternative<Vector3>(item.range->min) && std::holds_alternative<Vector3>(item.range->max))
			{
				minValue = std::get<Vector3>(item.range->min);
				maxValue = std::get<Vector3>(item.range->max);
			}
			// Vector3も項目ごとのmin/maxをDragFloat3へ渡し、座標系などの実用範囲を設定可能にする。
			ImGui::DragFloat3(imguiLabel.c_str(), reinterpret_cast<float*>(&value), 0.1f, minValue.x, maxValue.x);
		}
		/// ------- Vector4を保持している場合 ---------- ///
		else if (std::holds_alternative<Vector4>(item.value))
		{
			Vector4& value = std::get<Vector4>(item.value);
			ImGui::ColorEdit4(imguiLabel.c_str(), reinterpret_cast<float*>(&value));
		}
		/// ---------- bool型を保持している場合 ---------- ///
		else if (std::holds_alternative<bool>(item.value))
		{
			bool& value = std::get<bool>(item.value);
			ImGui::Checkbox(imguiLabel.c_str(), &value);
		}
		/// ---------- string型を保持している場合 ---------- ///
		else if (std::holds_alternative<std::string>(item.value))
		{
			std::string& value = std::get<std::string>(item.value);
			const char* previewValue = value.empty() ? "未選択" : value.c_str();

			if (!item.stringOptions.empty())
			{
				if (ImGui::BeginCombo(imguiLabel.c_str(), previewValue))
				{
					for (const std::string& option : item.stringOptions)
					{
						const bool isSelected = (value == option);
						if (ImGui::Selectable(option.c_str(), isSelected))
						{
							value = option;
						}

						if (isSelected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}

					ImGui::EndCombo();
				}
			}
			else
			{
				char buffer[256] = {};
				std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
				// テクスチャ名などの短い文字列はParameterManager上で直接編集できるようにする。
				if (ImGui::InputText(imguiLabel.c_str(), buffer, IM_ARRAYSIZE(buffer)))
				{
					value = buffer;
				}
			}
		}
		else
		{
			ImGui::Text("Unsupported type for item: %s", itemName.c_str());
		}
#else
		(void)itemName;
		(void)item;
#endif // USE_IMGUI
	}
} // namespace Ken4lowEngine
