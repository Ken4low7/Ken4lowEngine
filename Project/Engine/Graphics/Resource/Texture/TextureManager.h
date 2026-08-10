#pragma once
#include "DX12Include.h"
#include "LogString.h"

#include <DirectXTex.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

namespace Ken4lowEngine
{

	struct TextureMemoryStats
	{
		size_t textureCount = 0;
		size_t descriptorCount = 0;
		uint64_t estimatedGpuBytes = 0;
	};

	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;
	class SRVManager;

	/// -------------------------------------------------------------
	///						テクスチャ管理クラス
	/// -------------------------------------------------------------
	class TextureManager
	{
	private: /// ---------- 構造体 ---------- ///

		// テクスチャデータ構造体
		struct TextureData
		{
			DirectX::TexMetadata metaData = {};			// テクスチャのメタデータ（幅、高さ、ミップレベル数など）
			ComPtr<ID3D12Resource> resource;			// テクスチャリソース
			uint32_t srvIndex = UINT32_MAX;				// SRVヒープ上のインデックス。UINT32_MAXは未割り当てを示す。
			D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU{}; // CPU側のSRVハンドル
			D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU{}; // GPU側のSRVハンドル
		};

	public: /// ---------- メンバ関数 ---------- ///

		// シングルトンインスタンスを取得
		static TextureManager* GetInstance();

		// 初期化処理
		void Initialize(DirectXCommon* dxCommon);

		// 終了処理
		void Finalize();

		// テクスチャリソースの作成
		static ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

		// テクスチャデータのアップロード
		static ComPtr<ID3D12Resource> UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

		// テクスチャデータの読み込み
		static DirectX::ScratchImage LoadTextureData(const std::string& filePath);

		// テクスチャの読み込みとSRV作成
		void LoadTexture(const std::string& filePath);

		// WorkerでDecode済みの画像からMain ThreadでGPU Resource / SRVを生成する。
		void LoadTextureFromData(const std::string& filePath, DirectX::ScratchImage image);

		// AssetRegistryで同一Textureを一意化するため、TextureManagerと同じ規則でPathを解決する。
		std::string ResolveTexturePath(const std::string& filePath);

		// Texture Cacheから取り外し、必要ならGPU Fence完了までResource / SRV解放を遅延する。
		bool UnloadTexture(const std::string& filePath, bool deferredRelease = true);

		// テクスチャの再読み込み
		void ReloadTexture(const std::string& filePath);

		// テクスチャの削除
		void CreateSolidColorTexture(const std::string& key, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint32_t width = 64, uint32_t height = 64);

		// ルートパラメータにSRVテーブルをセット
		void SetGraphicsRootDescriptorTable(ID3D12GraphicsCommandList* commandList, UINT rootParameter, D3D12_GPU_DESCRIPTOR_HANDLE textureSRVHandleGPU);

	public: /// ---------- アクセッサ ---------- ///

		// テクスチャのSRVインデックスを取得
		uint32_t GetTextureIndexByFilePath(const std::string& filePath);

		// テクスチャのSRVハンドルを取得
		D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

		// テクスチャのSRVインデックスを取得
		uint32_t GetSrvIndex(const std::string& filePath);

		// テクスチャのメタデータを取得
		const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

		// テクスチャリソースを取得
		ID3D12Resource* GetResource(const std::string& filePath);

		// 主要なTexture payloadの概算値を取得する。Heap alignmentやDriver Residencyは含めない。
		TextureMemoryStats GetMemoryStats() const
		{
			TextureMemoryStats stats{};
			stats.textureCount = textureDatas.size();

			for (const auto& [path, textureData] : textureDatas)
			{
				(void)path;
				if (textureData.srvIndex != UINT32_MAX)
				{
					++stats.descriptorCount;
				}

				const DirectX::TexMetadata& metadata = textureData.metaData;
				size_t width = metadata.width > 0 ? metadata.width : 1;
				size_t height = metadata.height > 0 ? metadata.height : 1;
				size_t depth = metadata.depth > 0 ? metadata.depth : 1;
				const size_t arraySize = metadata.arraySize > 0 ? metadata.arraySize : 1;
				const size_t mipLevels = metadata.mipLevels > 0 ? metadata.mipLevels : 1;

				for (size_t mip = 0; mip < mipLevels; ++mip)
				{
					size_t rowPitch = 0;
					size_t slicePitch = 0;
					if (SUCCEEDED(DirectX::ComputePitch(metadata.format, width, height, rowPitch, slicePitch)))
					{
						stats.estimatedGpuBytes += static_cast<uint64_t>(slicePitch) *
							static_cast<uint64_t>(depth) * static_cast<uint64_t>(arraySize);
					}

					width = width > 1 ? width >> 1 : 1;
					height = height > 1 ? height >> 1 : 1;
					depth = depth > 1 ? depth >> 1 : 1;
				}
			}

			return stats;
		}

		// テクスチャパスのインデックスを構築
		void BuildTexturePathIndex();

		// クエリに対して、インデックスに登録されたテクスチャパスの中から最も類似するものを返す
		std::string FindCompiledTexturePath(const std::string& query) const;

	private: /// ---------- ヘルパー関数 ---------- ///

		// テクスチャパスを正規化する関数（例: 大文字小文字の統一、スラッシュの統一など）
		std::string NormalizeTexturePath(const std::string& filePath);

		// クエリとテクスチャパスの類似度を計算する関数（例: レーベンシュタイン距離など）
		static std::string NormalizeSlashes(std::string path);

		// 文字列を小文字に変換する関数
		static std::string ToLowerString(std::string s);

	private: /// ---------- メンバ変数 ---------- ///

		// DirectXコモン
		DirectXCommon* dxCommon_ = nullptr;

		// SRVマネージャー
		std::unordered_map<std::string, TextureData> textureDatas;

		// テクスチャパスのインデックス（正規化されたパスをキー、元のパスのリストを値とする）
		std::unordered_map<std::string, std::vector<std::string>> texturePathIndex_;

		// SRVインデックスの割り当て開始位置。これ以降のインデックスはテクスチャ用に割り当てる。
		static uint32_t kSRVIndexTop;

		// コンパイル済みテクスチャのルートディレクトリ
		static constexpr const char* kTextureRootDir = "Resources/Textures/Compiled";

	private: /// ---------- コンストラクタ / デストラクタ / コピー禁止 ---------- ///

		TextureManager() = default;
		~TextureManager() = default;
		TextureManager(const TextureManager&) = delete;
		const TextureManager& operator=(const TextureManager&) = delete;
	};

} // namespace Ken4lowEngine