#pragma once
#include "DX12Include.h"
#include "Matrix4x4.h"
#include "Vector4.h"
#include <Engine/Graphics/Device/Buffer/PerFrameUploadBuffer.h>
#include <cstdint>
#include <string>

namespace Ken4lowEngine
{

struct LegacyMaterialDesc
{
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	float shininess = 32.0f;
	float reflection = 0.0f;
	float roughness = 0.5f;
	Matrix4x4 uvTransform = Matrix4x4::MakeIdentity();
	bool usePointSampling = false;
	std::string baseColorTexturePath;
};

struct PbrMaterialDesc
{
	Vector4 baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float metallicFactor = 0.0f;
	float roughnessFactor = 0.5f;
	float normalScale = 1.0f;
	float occlusionStrength = 1.0f;
	Vector4 emissiveFactor = { 0.0f, 0.0f, 0.0f, 1.0f };
	std::string baseColorTexturePath;
	std::string metallicRoughnessTexturePath;
	std::string normalTexturePath;
	std::string occlusionTexturePath;
	std::string emissiveTexturePath;
};

struct MaterialDesc
{
	LegacyMaterialDesc legacy;
	PbrMaterialDesc pbr;
	bool preferPbrWorkflow = false;
};

class MaterialTextureSlots
{
public:
	void ApplyDesc(const MaterialDesc& desc);
	void Reset();
	void Clear();

	D3D12_GPU_DESCRIPTOR_HANDLE ResolveBaseColor(D3D12_GPU_DESCRIPTOR_HANDLE modelBaseColor) const;
	void BindAdditionalSlots(
		ID3D12GraphicsCommandList* commandList,
		UINT metallicRoughnessRootIndex,
		UINT normalRootIndex,
		UINT occlusionRootIndex,
		UINT emissiveRootIndex) const;

	bool HasBaseColorOverride() const { return hasBaseColorOverride_; }

private:
	D3D12_GPU_DESCRIPTOR_HANDLE baseColor_{};
	D3D12_GPU_DESCRIPTOR_HANDLE metallicRoughness_{};
	D3D12_GPU_DESCRIPTOR_HANDLE normal_{};
	D3D12_GPU_DESCRIPTOR_HANDLE occlusion_{};
	D3D12_GPU_DESCRIPTOR_HANDLE emissive_{};
	bool hasBaseColorOverride_ = false;
};

class Material
{
public:
	struct MaterialCBData
	{
		Vector4 color;
		float shininess;
		float pbrEnabled;
		float metallic;
		float normalScale;
		Matrix4x4 uvTransform;
		float reflection;
		float roughness;
		float usePointSampling;
		float occlusionStrength;
		Vector4 emissiveFactor;
		uint32_t textureFlags;
		float padding[3];
	};

public:
	std::string textureFilePath;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};

	Material() = default;
	void Initialize();
	void Update();
	void ApplyDesc(const MaterialDesc& desc);
	void ResetToDefault();
	void SetPipeline(UINT rootParameterIndex = 0) const;
	void DrawImGui();

	ComPtr<ID3D12Resource> GetMaterialResource();
	MaterialCBData* GetMaterialData() { return materialData_; }

	void SetColor(const Vector4& color) { materialData_->color = color; }
	void SetShininess(float shininess) { materialData_->reflection = shininess; }
	void SetIntensity(float shininess) { materialData_->shininess = shininess; }
	void SetReflection(float reflection) { materialData_->reflection = reflection; }
	void SetUVTransform(const Matrix4x4& uvTransform) { materialData_->uvTransform = uvTransform; }
	void SetUsePointSampling(bool enabled) { materialData_->usePointSampling = enabled ? 1.0f : 0.0f; }
	void SetPbrEnabled(bool enabled) { materialData_->pbrEnabled = enabled ? 1.0f : 0.0f; }
	void SetMetallic(float metallic) { materialData_->metallic = metallic; }
	void SetRoughness(float roughness) { materialData_->roughness = roughness; }
	void SetNormalScale(float normalScale) { materialData_->normalScale = normalScale; }
	void SetOcclusionStrength(float occlusionStrength) { materialData_->occlusionStrength = occlusionStrength; }
	void SetEmissiveFactor(const Vector4& emissiveFactor) { materialData_->emissiveFactor = emissiveFactor; } // 発光量を毎フレーム変更できるようCPU側の値へ直接反映する。

private:
	MaterialCBData materialCpuData_{};
	MaterialCBData* materialData_ = &materialCpuData_;
	mutable PerFrameUploadBuffer<MaterialCBData> materialBuffers_;
};
} // namespace Ken4lowEngine
