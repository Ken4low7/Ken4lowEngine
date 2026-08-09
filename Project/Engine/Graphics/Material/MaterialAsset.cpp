#include "MaterialAsset.h"

#include <utility>

namespace Ken4lowEngine
{
	MaterialAsset::MaterialAsset(std::string id, std::string name, const MaterialDesc& desc)
		: id_(std::move(id))
		, name_(std::move(name))
		, desc_(desc)
	{
	}

	MaterialAsset MaterialAsset::CreateDefault(const std::string& id, const std::string& name)
	{
		MaterialDesc desc{};
		desc.preferPbrWorkflow = false;
		desc.legacy.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		desc.legacy.shininess = 32.0f;
		desc.legacy.reflection = 0.0f;
		desc.legacy.roughness = 0.5f;
		desc.legacy.uvTransform = Matrix4x4::MakeIdentity();
		desc.legacy.usePointSampling = false;

		// 既存Material::Initializeの初期値に合わせたCPU側Defaultだけを作り、描画結果へはまだ反映しない。
		return MaterialAsset(id, name, desc);
	}

	void MaterialAsset::SetDesc(const MaterialDesc& desc)
	{
		// MaterialAssetはCPU側Descの所有だけに留め、MaterialCBData/HLSLレイアウト変更を伴う反映は後段の責務に分ける。
		desc_ = desc;
	}

	MaterialWorkflow MaterialAsset::GetWorkflow() const
	{
		return desc_.preferPbrWorkflow ? MaterialWorkflow::PBR : MaterialWorkflow::Legacy;
	}

	void MaterialAsset::SetPreferPbrWorkflow(bool enabled)
	{
		// PBR優先フラグは将来のPipeline選択用メタデータであり、現時点ではShader切り替えを発生させない。
		desc_.preferPbrWorkflow = enabled;
	}

	void MaterialAsset::AddTextureSlot(const std::string& semantic, const std::string& texturePath)
	{
		textureSlots_.push_back({ semantic, texturePath });
	}

	void MaterialAsset::ClearTextureSlots()
	{
		textureSlots_.clear();
	}
}
