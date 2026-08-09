#pragma once

#include "MaterialAsset.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// MaterialAssetをID/名前で登録・取得するCPU側Repositoryです。<br/>
	/// 描画処理、GPU転送、Texture実体ロード、Shader管理は行わず、将来Json/glTF/MaterialEditorから
	/// MaterialDescを受け取るための薄い入口として扱います。
	/// </summary>
	class MaterialRepository
	{
	public:
		static constexpr const char* kDefaultMaterialId = "DefaultMaterial";

		/// <summary>Engine全体で共有するMaterialRepositoryを取得します。</summary>
		static MaterialRepository* GetInstance();

		/// <summary>
		/// Default Materialを含む初期状態へ整えます。<br/>
		/// 既存MaterialCBDataの初期値と近いCPU側Descだけを登録し、既存モデル描画には接続しません。
		/// </summary>
		void InitializeDefaults();

		/// <summary>
		/// 登録済みMaterialAssetをすべて削除します。<br/>
		/// GPUリソースを所有していないため、描画リソース解放は発生しません。
		/// </summary>
		void Clear();

		/// <summary>
		/// MaterialAssetを登録します。<br/>
		/// 同じIDが存在する場合は差し替え、名前検索用インデックスも更新します。
		/// </summary>
		bool Register(const MaterialAsset& asset);

		/// <summary>
		/// MaterialAssetを共有ポインタで登録します。<br/>
		/// 外部ローダーやMaterialEditorが作成したAssetをRepositoryへ渡す入口として使います。
		/// </summary>
		bool Register(const std::shared_ptr<MaterialAsset>& asset);

		/// <summary>指定IDのMaterialAssetを登録解除し、名前索引も同時に破棄します。</summary>
		bool Unregister(const std::string& id);

		/// <summary>
		/// MaterialDescからMaterialAssetを作成または差し替えます。<br/>
		/// Json/glTF読み込み後にCPU側Materialだけを登録するための軽いヘルパーです。
		/// </summary>
		std::shared_ptr<MaterialAsset> CreateOrReplace(const std::string& id, const MaterialDesc& desc, const std::string& name = "");

		/// <summary>
		/// IDからMaterialAssetを取得します。<br/>
		/// 見つからない場合はnullptrを返し、既存描画へのフォールバックは呼び出し側で判断します。
		/// </summary>
		std::shared_ptr<MaterialAsset> FindById(const std::string& id) const;

		/// <summary>
		/// 名前からMaterialAssetを取得します。<br/>
		/// 名前は表示用なので、将来重複を許す設計にする場合はID検索を優先します。
		/// </summary>
		std::shared_ptr<MaterialAsset> FindByName(const std::string& name) const;

		/// <summary>
		/// Default Materialを取得します。<br/>
		/// Repository初期化前でも呼び出せるよう、未登録時はnullptrを返します。
		/// </summary>
		std::shared_ptr<MaterialAsset> GetDefaultMaterial() const;

		/// <summary>
		/// 指定IDのMaterialAssetが登録済みかを返します。
		/// </summary>
		bool Contains(const std::string& id) const;

		/// <summary>
		/// 登録済みMaterialAssetのID一覧を返します。<br/>
		/// MaterialEditorやデバッグ表示から一覧化するためのCPU側情報だけを返します。
		/// </summary>
		std::vector<std::string> GetRegisteredIds() const;

		/// <summary>登録・差し替え・削除が行われるたびに増える更新世代を取得します。</summary>
		uint64_t GetRevision() const { return revision_; }

	private:
		std::unordered_map<std::string, std::shared_ptr<MaterialAsset>> materialsById_;
		std::unordered_map<std::string, std::string> idByName_; // 名前検索はIDへの薄い索引に留め、MaterialAssetの所有はID側へ集約する。
		uint64_t revision_ = 0; // 描画Componentが共有Materialの変更を低コストで検知するための更新世代。
	};
}
