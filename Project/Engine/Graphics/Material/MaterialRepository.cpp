#include "MaterialRepository.h"

namespace Ken4lowEngine
{
	MaterialRepository* MaterialRepository::GetInstance()
	{
		static MaterialRepository instance;
		static const bool initialized = []()
			{
				instance.InitializeDefaults(); // 初回アクセス時だけ既存描画互換のDefault Materialを登録する。
				return true;
			}();
		(void)initialized;
		return &instance;
	}

	void MaterialRepository::InitializeDefaults()
	{
		Clear();
		Register(MaterialAsset::CreateDefault(kDefaultMaterialId, "Default Material"));
	}

	void MaterialRepository::Clear()
	{
		const bool hadMaterials = !materialsById_.empty() || !idByName_.empty();
		materialsById_.clear();
		idByName_.clear();
		if (hadMaterials)
		{
			++revision_; // 実際に登録状態が変化した場合だけ共有Materialの更新を通知する。
		}
	}

	bool MaterialRepository::Register(const MaterialAsset& asset)
	{
		return Register(std::make_shared<MaterialAsset>(asset));
	}

	bool MaterialRepository::Register(const std::shared_ptr<MaterialAsset>& asset)
	{
		if (!asset || asset->GetId().empty())
		{
			return false;
		}

		const auto existingIt = materialsById_.find(asset->GetId());
		if (existingIt != materialsById_.end() && existingIt->second && !existingIt->second->GetName().empty())
		{
			const auto oldNameIt = idByName_.find(existingIt->second->GetName());
			if (oldNameIt != idByName_.end() && oldNameIt->second == asset->GetId())
			{
				idByName_.erase(oldNameIt); // 別IDが同名を使用中なら、その有効な名前索引は消さない。
			}
		}

		// RepositoryはCPU側Assetの登録だけを行い、TextureロードやMaterialCBData更新は将来の接続層へ任せる。
		materialsById_[asset->GetId()] = asset;
		if (!asset->GetName().empty())
		{
			idByName_[asset->GetName()] = asset->GetId();
		}
		++revision_; // 同じIDの差し替えも描画Componentへ再反映させる。
		return true;
	}

	bool MaterialRepository::Unregister(const std::string& id)
	{
		const auto it = materialsById_.find(id);
		if (it == materialsById_.end())
		{
			return false;
		}
		if (id == kDefaultMaterialId)
		{
			return Register(MaterialAsset::CreateDefault(kDefaultMaterialId, "Default Material")); // 既定Materialは削除せず互換値へ復元する。
		}

		if (it->second && !it->second->GetName().empty())
		{
			const auto nameIt = idByName_.find(it->second->GetName());
			if (nameIt != idByName_.end() && nameIt->second == id)
			{
				idByName_.erase(nameIt); // 削除対象ID自身を指す名前索引だけを取り除く。
			}
		}
		materialsById_.erase(it);
		++revision_;
		return true;
	}

	std::shared_ptr<MaterialAsset> MaterialRepository::CreateOrReplace(const std::string& id, const MaterialDesc& desc, const std::string& name)
	{
		if (id.empty())
		{
			return nullptr;
		}

		const std::string displayName = name.empty() ? id : name;
		auto asset = std::make_shared<MaterialAsset>(id, displayName, desc);
		Register(asset);
		return asset;
	}

	std::shared_ptr<MaterialAsset> MaterialRepository::FindById(const std::string& id) const
	{
		const auto it = materialsById_.find(id);
		return it != materialsById_.end() ? it->second : nullptr;
	}

	std::shared_ptr<MaterialAsset> MaterialRepository::FindByName(const std::string& name) const
	{
		const auto nameIt = idByName_.find(name);
		if (nameIt == idByName_.end())
		{
			return nullptr;
		}
		return FindById(nameIt->second);
	}

	std::shared_ptr<MaterialAsset> MaterialRepository::GetDefaultMaterial() const
	{
		return FindById(kDefaultMaterialId);
	}

	bool MaterialRepository::Contains(const std::string& id) const
	{
		return materialsById_.find(id) != materialsById_.end();
	}

	std::vector<std::string> MaterialRepository::GetRegisteredIds() const
	{
		std::vector<std::string> ids;
		ids.reserve(materialsById_.size());
		for (const auto& [id, material] : materialsById_)
		{
			(void)material;
			ids.push_back(id);
		}
		return ids;
	}
}
