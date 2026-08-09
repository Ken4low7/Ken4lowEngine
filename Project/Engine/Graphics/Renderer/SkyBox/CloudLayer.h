#pragma once
#include "DX12Include.h"
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

#include <string>

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// SkyBox とは独立して、カメラ上空へ水平な半透明の雲平面を描画する。
	class CloudLayer final
	{
	private:
		static inline const UINT kNumVertex = 4;
		static inline const UINT kNumIndex = 6;
		static inline constexpr float kBasePlaneExtent = 10000.0f;

		struct Material final
		{
			Vector4 color;
			Matrix4x4 uvTransform;
			Vector4 topColor;
			Vector4 bottomColor;
			Vector4 horizonColor;
			uint32_t textureIndex;
			uint32_t skyType;
			Vector2 uvOffset;
			float cloudHeight;
			float cloudScale;
			uint32_t textureAvailable;
			float padding;
		};

		struct VertexData final
		{
			Vector4 position;
			Vector3 texcoord;
		};

		struct TransformationMatrix final
		{
			Matrix4x4 WVP;
			Matrix4x4 World;
		};

	public:
		void Initialize();
		void Update();
		void Draw();
		void SetSettings(bool enabled, const std::string& texturePath, float height, float scale,
			const Vector2& scrollSpeed, const Vector2& uvOffset, float alpha, const Vector4& tintColor, bool reloadTexture = false);
		void Advance(float deltaTime);

		Vector2 GetUvOffset() const { return uvOffset_; }
		bool IsTextureAvailable() const { return textureAvailable_; }
		bool IsEnabled() const { return enabled_; }
		const std::string& GetTexturePath() const { return texturePath_; }
		uint32_t GetTextureIndex() const { return textureIndex_; }

	private:
		void InitializeMaterial();
		void InitializeVertexBuffer();
		void InitializeIndexBuffer();
		bool LoadTexture(const std::string& texturePath, bool reloadTexture);

		DirectXCommon* dxCommon_ = nullptr;
		ComPtr<ID3D12Resource> materialResource_;
		Material* materialData_ = nullptr;
		ComPtr<ID3D12Resource> vertexResource_;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
		VertexData* vertexData_ = nullptr;
		ComPtr<ID3D12Resource> indexResource_;
		D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
		uint32_t* indexData_ = nullptr;
		ComPtr<ID3D12Resource> wvpResource_;
		TransformationMatrix* wvpData_ = nullptr;

		bool enabled_ = false;
		bool textureAvailable_ = false;
		std::string texturePath_;
		uint32_t textureIndex_ = 0;
		float height_ = 160.0f;
		float scale_ = 1.5f;
		Vector2 scrollSpeed_ = { 0.002f, 0.0005f };
		Vector2 uvOffset_{};
		float alpha_ = 0.55f;
		Vector4 tintColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	};
}
