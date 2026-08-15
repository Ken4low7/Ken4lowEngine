#pragma once
#include "DX12Include.h"
#include "Matrix4x4.h"
#include "Vector4.h"
#include <Engine/Graphics/Device/Buffer/PerFrameUploadBuffer.h>
#include <cstdint>
#include <string>

namespace Ken4lowEngine
{

enum class MaterialCullMode : uint8_t
{
	Back = 0,
	Front,
	None,
};

enum class MaterialBlendMode : uint8_t
{
	Opaque = 0,
	Masked,
	Transparent,
	Additive,
};

inline float CalculateWorldHandednessDeterminant(const Matrix4x4& world)
{
	return
		world.m[0][0] * (world.m[1][1] * world.m[2][2] - world.m[1][2] * world.m[2][1]) -
		world.m[0][1] * (world.m[1][0] * world.m[2][2] - world.m[1][2] * world.m[2][0]) +
		world.m[0][2] * (world.m[1][0] * world.m[2][1] - world.m[1][1] * world.m[2][0]);
}

inline MaterialCullMode ResolveMaterialCullModeForWorld(MaterialCullMode cullMode, const Matrix4x4& world)
{
	if (cullMode == MaterialCullMode::None || CalculateWorldHandednessDeterminant(world) >= 0.0f)
	{
		return cullMode;
	}
	// 負Scaleなどの鏡映Transformでは巻き順が反転するため、Front/Backを入れ替えて表面契約を維持する。
	return cullMode == MaterialCullMode::Back ? MaterialCullMode::Front : MaterialCullMode::Back;
}

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
	MaterialCullMode cullMode = MaterialCullMode::Back; // Phase15.1: 通常Materialは裏面カリングを既定とする。
	MaterialBlendMode blendMode = MaterialBlendMode::Opaque; // Phase15.2: Forward Queue分類はMaterial側の明示契約から決める。
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
		float reflectionSourceAvailable;
		float planarReflectionEnabled;
		float planarReflectionStrength;
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
	void SetShininess(float shininess) { materialData_->shininess = shininess; } // 光沢幅のSetterは環境反射率ではなくDirect Specularへ反映する。
	void SetIntensity(float shininess) { materialData_->shininess = shininess; }
	void SetReflection(float reflection) { materialData_->reflection = reflection; }
	void SetUVTransform(const Matrix4x4& uvTransform) { materialData_->uvTransform = uvTransform; }
	void SetUsePointSampling(bool enabled) { materialData_->usePointSampling = enabled ? 1.0f : 0.0f; }
	void SetPbrEnabled(bool enabled) { materialData_->pbrEnabled = enabled ? 1.0f : 0.0f; }
	bool IsPbrEnabled() const { return materialData_ && materialData_->pbrEnabled > 0.5f; }
	void SetMetallic(float metallic) { materialData_->metallic = metallic; }
	void SetRoughness(float roughness) { materialData_->roughness = roughness; }
	void SetNormalScale(float normalScale) { materialData_->normalScale = normalScale; }
	void SetOcclusionStrength(float occlusionStrength) { materialData_->occlusionStrength = occlusionStrength; }
	void SetEmissiveFactor(const Vector4& emissiveFactor) { materialData_->emissiveFactor = emissiveFactor; } // 発光量を毎フレーム変更できるようCPU側の値へ直接反映する。
	void SetPlanarReflectionState(bool enabled, float strength)
	{
		materialData_->planarReflectionEnabled = enabled ? 1.0f : 0.0f;
		materialData_->planarReflectionStrength = strength;
	}
	void SetCullMode(MaterialCullMode cullMode) { cullMode_ = cullMode; }
	MaterialCullMode GetCullMode() const { return cullMode_; }
	void SetBlendMode(MaterialBlendMode blendMode) { blendMode_ = blendMode; }
	MaterialBlendMode GetBlendMode() const { return blendMode_; }

private:
	MaterialCBData materialCpuData_{};
	MaterialCBData* materialData_ = &materialCpuData_;
	mutable PerFrameUploadBuffer<MaterialCBData> materialBuffers_;
	MaterialCullMode cullMode_ = MaterialCullMode::Back;
	MaterialBlendMode blendMode_ = MaterialBlendMode::Opaque;
};
} // namespace Ken4lowEngine
