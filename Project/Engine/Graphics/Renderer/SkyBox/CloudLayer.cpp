#define NOMINMAX
#include "CloudLayer.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"
#include "SkyBoxManager.h"
#include "TextureManager.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace Ken4lowEngine
{
	void CloudLayer::Initialize()
	{
		dxCommon_ = DirectXCommon::GetInstance();
		InitializeMaterial();
		InitializeVertexBuffer();
		InitializeIndexBuffer();
		wvpResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationMatrix));
		wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
		wvpData_->World = Matrix4x4::MakeIdentity();
		wvpData_->WVP = Matrix4x4::MakeIdentity();
	}

	void CloudLayer::Update()
	{
		const Vector3 cameraPosition = CameraManager::GetInstance()->GetActiveCameraPosition();
		const float planeScale = kBasePlaneExtent * std::max(scale_, 0.01f);
		// 雲は SkyBox の内側へ貼らず、カメラ上空へ追従する水平平面として配置する。
		const Matrix4x4 world = Matrix4x4::MakeAffineMatrix(
			{ planeScale, 1.0f, planeScale }, { 0.0f, 0.0f, 0.0f }, { cameraPosition.x, cameraPosition.y + height_, cameraPosition.z });
		wvpData_->World = world;
		wvpData_->WVP = Matrix4x4::Multiply(world, CameraManager::GetInstance()->GetActiveViewProjectionMatrix());
	}

	void CloudLayer::Draw()
	{
		if (!enabled_ || !textureAvailable_)
		{
			return;
		}

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SkyBoxManager::GetInstance()->SetCloudRenderSetting();
		materialData_->color = { tintColor_.x, tintColor_.y, tintColor_.z, tintColor_.w * alpha_ };
		materialData_->textureIndex = textureIndex_;
		materialData_->uvOffset = uvOffset_;
		materialData_->cloudHeight = height_;
		materialData_->cloudScale = scale_;
		materialData_->textureAvailable = 1u;
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
		commandList->IASetIndexBuffer(&indexBufferView_);
		commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
		commandList->DrawIndexedInstanced(kNumIndex, 1, 0, 0, 0);
	}

	void CloudLayer::SetSettings(bool enabled, const std::string& texturePath, float height, float scale,
		const Vector2& scrollSpeed, const Vector2& uvOffset, float alpha, const Vector4& tintColor, bool reloadTexture)
	{
		enabled_ = enabled;
		height_ = height;
		scale_ = scale;
		scrollSpeed_ = scrollSpeed;
		uvOffset_ = uvOffset;
		alpha_ = alpha;
		tintColor_ = tintColor;
		if (texturePath_ != texturePath || reloadTexture)
		{
			textureAvailable_ = LoadTexture(texturePath, reloadTexture);
		}
		texturePath_ = texturePath;
		std::cout << "[CloudLayer] texture path=Resources/Textures/Compiled/" << texturePath_
			<< " enabled=" << (enabled_ ? "true" : "false")
			<< " available=" << (textureAvailable_ ? "true" : "false")
			<< " srvIndex=" << textureIndex_ << std::endl;
	}

	void CloudLayer::Advance(float deltaTime)
	{
		uvOffset_.x = std::fmod(uvOffset_.x + scrollSpeed_.x * deltaTime, 1.0f);
		uvOffset_.y = std::fmod(uvOffset_.y + scrollSpeed_.y * deltaTime, 1.0f);
	}

	void CloudLayer::InitializeMaterial()
	{
		materialResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));
		materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
		materialData_->color = tintColor_;
		materialData_->uvTransform = Matrix4x4::MakeIdentity();
		materialData_->topColor = {};
		materialData_->bottomColor = {};
		materialData_->horizonColor = {};
		materialData_->textureIndex = textureIndex_;
		materialData_->skyType = 3u;
		materialData_->uvOffset = uvOffset_;
		materialData_->cloudHeight = height_;
		materialData_->cloudScale = scale_;
		materialData_->textureAvailable = 0u;
		materialData_->padding = 0.0f;
	}

	void CloudLayer::InitializeVertexBuffer()
	{
		vertexResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * kNumVertex);
		vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
		vertexBufferView_.SizeInBytes = sizeof(VertexData) * kNumVertex;
		vertexBufferView_.StrideInBytes = sizeof(VertexData);
		vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
		vertexData_[0] = { { -1.0f, 0.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } };
		vertexData_[1] = { {  1.0f, 0.0f, -1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } };
		vertexData_[2] = { { -1.0f, 0.0f,  1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } };
		vertexData_[3] = { {  1.0f, 0.0f,  1.0f, 1.0f }, { 1.0f, 1.0f, 0.0f } };
	}

	void CloudLayer::InitializeIndexBuffer()
	{
		indexResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(uint32_t) * kNumIndex);
		indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
		indexBufferView_.SizeInBytes = sizeof(uint32_t) * kNumIndex;
		indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
		indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
		indexData_[0] = 0; indexData_[1] = 1; indexData_[2] = 2;
		indexData_[3] = 2; indexData_[4] = 1; indexData_[5] = 3;
	}

	bool CloudLayer::LoadTexture(const std::string& texturePath, bool reloadTexture)
	{
		const std::filesystem::path compiledPath = std::filesystem::path("Resources/Textures/Compiled") / texturePath;
		if (texturePath.empty() || !std::filesystem::exists(compiledPath))
		{
			return false;
		}

		DirectX::ScratchImage validationImage;
		const std::wstring path = compiledPath.wstring();
		const HRESULT result = compiledPath.extension() == ".dds"
			? DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, validationImage)
			: DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, validationImage);
		if (FAILED(result) || validationImage.GetMetadata().IsCubemap())
		{
			return false;
		}

		TextureManager* textureManager = TextureManager::GetInstance();
		if (reloadTexture) textureManager->ReloadTexture(texturePath); else textureManager->LoadTexture(texturePath);
		textureIndex_ = textureManager->GetSrvIndex(texturePath);
		return true;
	}
}
