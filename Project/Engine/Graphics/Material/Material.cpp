#include "Material.h"
#include "DirectXCommon.h"
#include "TextureManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	static_assert(sizeof(Material::MaterialCBData) == 144, "MaterialCBData must match the HLSL b0 layout.");

	namespace
	{
		constexpr const char* kNeutralTextureKey = "Generated/Material/neutral_white.dds";

		D3D12_GPU_DESCRIPTOR_HANDLE LoadTextureHandle(const std::string& texturePath)
		{
			TextureManager* textureManager = TextureManager::GetInstance();
			textureManager->LoadTexture(texturePath);
			return textureManager->GetSrvHandleGPU(texturePath);
		}

		D3D12_GPU_DESCRIPTOR_HANDLE GetNeutralTextureHandle()
		{
			TextureManager* textureManager = TextureManager::GetInstance();
			textureManager->CreateSolidColorTexture(kNeutralTextureKey, 255, 255, 255, 255, 1, 1);
			return textureManager->GetSrvHandleGPU(kNeutralTextureKey);
		}
	}

	void MaterialTextureSlots::ApplyDesc(const MaterialDesc& desc)
	{
		Reset();
		const std::string& baseColorPath = desc.preferPbrWorkflow
			? desc.pbr.baseColorTexturePath
			: desc.legacy.baseColorTexturePath;
		if (!baseColorPath.empty())
		{
			baseColor_ = LoadTextureHandle(baseColorPath);
			hasBaseColorOverride_ = true;
		}
		if (!desc.preferPbrWorkflow) return;
		if (!desc.pbr.metallicRoughnessTexturePath.empty()) metallicRoughness_ = LoadTextureHandle(desc.pbr.metallicRoughnessTexturePath);
		if (!desc.pbr.normalTexturePath.empty()) normal_ = LoadTextureHandle(desc.pbr.normalTexturePath);
		if (!desc.pbr.occlusionTexturePath.empty()) occlusion_ = LoadTextureHandle(desc.pbr.occlusionTexturePath);
		if (!desc.pbr.emissiveTexturePath.empty()) emissive_ = LoadTextureHandle(desc.pbr.emissiveTexturePath);
	}

	void MaterialTextureSlots::Reset()
	{
		const D3D12_GPU_DESCRIPTOR_HANDLE neutral = GetNeutralTextureHandle();
		baseColor_ = {};
		metallicRoughness_ = neutral;
		normal_ = neutral;
		occlusion_ = neutral;
		emissive_ = neutral;
		hasBaseColorOverride_ = false;
	}

	void MaterialTextureSlots::Clear()
	{
		baseColor_ = {};
		metallicRoughness_ = {};
		normal_ = {};
		occlusion_ = {};
		emissive_ = {};
		hasBaseColorOverride_ = false;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE MaterialTextureSlots::ResolveBaseColor(D3D12_GPU_DESCRIPTOR_HANDLE modelBaseColor) const
	{
		return hasBaseColorOverride_ ? baseColor_ : modelBaseColor;
	}

	void MaterialTextureSlots::BindAdditionalSlots(
		ID3D12GraphicsCommandList* commandList,
		UINT metallicRoughnessRootIndex,
		UINT normalRootIndex,
		UINT occlusionRootIndex,
		UINT emissiveRootIndex) const
	{
		if (!commandList) return;
		commandList->SetGraphicsRootDescriptorTable(metallicRoughnessRootIndex, metallicRoughness_);
		commandList->SetGraphicsRootDescriptorTable(normalRootIndex, normal_);
		commandList->SetGraphicsRootDescriptorTable(occlusionRootIndex, occlusion_);
		commandList->SetGraphicsRootDescriptorTable(emissiveRootIndex, emissive_);
	}

	void Material::Initialize()
	{
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();
		const uint32_t frameCount = dxCommon->GetCommandManager()->GetFrameResourceCount();
		materialBuffers_.Initialize(dxCommon->GetDevice(), frameCount);
		materialData_ = &materialCpuData_;
		ResetToDefault();
		materialBuffers_.WriteAll(materialCpuData_); // Frames in Flight切替直後でも全Frameが同じMaterial値から開始する。
	}

	void Material::ResetToDefault()
	{
		if (!materialData_) return;
		materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		materialData_->shininess = 32.0f;
		materialData_->pbrEnabled = 0.0f;
		materialData_->metallic = 0.0f;
		materialData_->normalScale = 1.0f;
		materialData_->reflection = 0.0f;
		materialData_->uvTransform = Matrix4x4::MakeIdentity();
		materialData_->roughness = 0.5f;
		materialData_->usePointSampling = 0.0f;
		materialData_->occlusionStrength = 1.0f;
		materialData_->emissiveFactor = { 0.0f, 0.0f, 0.0f, 1.0f };
		materialData_->textureFlags = 0;
		materialData_->padding[0] = materialData_->padding[1] = materialData_->padding[2] = 0.0f;
	}

	void Material::ApplyDesc(const MaterialDesc& desc)
	{
		if (!materialData_) return;
		const bool usePbr = desc.preferPbrWorkflow;
		materialData_->color = usePbr ? desc.pbr.baseColorFactor : desc.legacy.color;
		materialData_->shininess = desc.legacy.shininess;
		materialData_->pbrEnabled = usePbr ? 1.0f : 0.0f;
		materialData_->metallic = desc.pbr.metallicFactor;
		materialData_->normalScale = desc.pbr.normalScale;
		materialData_->uvTransform = desc.legacy.uvTransform;
		materialData_->reflection = desc.legacy.reflection;
		materialData_->roughness = usePbr ? desc.pbr.roughnessFactor : desc.legacy.roughness;
		materialData_->usePointSampling = desc.legacy.usePointSampling ? 1.0f : 0.0f;
		materialData_->occlusionStrength = desc.pbr.occlusionStrength;
		materialData_->emissiveFactor = desc.pbr.emissiveFactor;
		materialData_->textureFlags = 0;
		if (usePbr && !desc.pbr.metallicRoughnessTexturePath.empty()) materialData_->textureFlags |= 1u << 0;
		if (usePbr && !desc.pbr.normalTexturePath.empty()) materialData_->textureFlags |= 1u << 1;
		if (usePbr && !desc.pbr.occlusionTexturePath.empty()) materialData_->textureFlags |= 1u << 2;
		if (usePbr && !desc.pbr.emissiveTexturePath.empty()) materialData_->textureFlags |= 1u << 3;
	}

	void Material::Update()
	{
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();
		const uint32_t frameIndex = dxCommon->GetCommandManager()->GetCurrentFrameIndex();
		materialBuffers_.WriteFrame(frameIndex, materialCpuData_);
	}

	void Material::SetPipeline(UINT rootParameterIndex) const
	{
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();
		auto* commandManager = dxCommon->GetCommandManager();
		const uint32_t frameIndex = commandManager->GetCurrentFrameIndex();
		materialBuffers_.WriteFrame(frameIndex, materialCpuData_);

		const D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = materialBuffers_.GetGpuAddress(frameIndex);
		if (gpuAddress != 0)
		{
			commandManager->GetCommandList()->SetGraphicsRootConstantBufferView(rootParameterIndex, gpuAddress);
		}
	}

	ComPtr<ID3D12Resource> Material::GetMaterialResource()
	{
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();
		return materialBuffers_.GetResource(dxCommon->GetCommandManager()->GetCurrentFrameIndex());
	}

	void Material::DrawImGui()
	{
#ifdef USE_IMGUI
		if (ImGui::CollapsingHeader("Material Settings"))
		{
			ImGui::ColorEdit4("Color", &materialData_->color.x);
			ImGui::DragFloat("Shininess", &materialData_->shininess, 1.0f, 1.0f, 256.0f);
			ImGui::DragFloat("Reflectivity", &materialData_->reflection, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Roughness", &materialData_->roughness, 0.01f, 0.0f, 1.0f);
			bool pbrEnabled = materialData_->pbrEnabled > 0.5f;
			if (ImGui::Checkbox("Use PBR##Material", &pbrEnabled)) materialData_->pbrEnabled = pbrEnabled ? 1.0f : 0.0f;
			ImGui::DragFloat("Metallic##Material", &materialData_->metallic, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Normal Scale##Material", &materialData_->normalScale, 0.01f, 0.0f, 2.0f);
			ImGui::DragFloat("AO Strength##Material", &materialData_->occlusionStrength, 0.01f, 0.0f, 1.0f);
			ImGui::ColorEdit4("Emissive##Material", &materialData_->emissiveFactor.x);
		}
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
