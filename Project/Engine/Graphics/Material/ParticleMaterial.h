#pragma once
#include <DX12Include.h>
#include "Vector4.h"
#include "Matrix4x4.h"

#include <array>

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///				　パーティクル用マテリアルクラス
/// -------------------------------------------------------------
class ParticleMaterial
{
public: /// ---------- 構造体 ---------- ///

	// マテリアルデータ 定数バッファで送るデータ
	struct MaterialCBData
	{
		Vector4 color;			// 色
		Matrix4x4 uvTransform;  // UV変換行列
		uint32_t drawType;		// 描画タイプ
		float padding[3];		// パディング
	};

public: /// ---------- メンバ関数 ---------- ///

	static constexpr uint32_t kSlotCount = 256;

	/// <summary>
	/// コンストラクタ
	/// </summary>
	ParticleMaterial() = default;

	/// <summary>
	/// 初期化処理
	/// </summary>
	void Initialize();

	void Finalize();

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update();

	/// <summary>
	/// 指定したルートパラメーターのインデックスに基づいてパイプラインを設定する、オブジェクトを変更しない const メンバー関数。
	/// </summary>
	/// <param name="rootParameterIndex">設定対象のルートパラメーターのインデックス。省略した場合の既定値は 0</param>
	void SetPipeline(UINT rootParameterIndex = 0, uint32_t slot = 0) const;

	/// <summary>
	/// ImGuiの描画
	/// </summary>
	void DrawImGui();

	MaterialCBData* GetSlotData(uint32_t slot)
	{
		return &materialSlots_[slot % kSlotCount];
	}

	const MaterialCBData* GetSlotData(uint32_t slot) const
	{
		return &materialSlots_[slot % kSlotCount];
	}

	// 描画タイプのセッター
	void SetDrawType(uint32_t type, uint32_t slot)
	{
		GetSlotData(slot)->drawType = type;
	}

public: /// ---------- メンバ変数 ---------- ///

	std::array<MaterialCBData, kSlotCount> materialSlots_{}; // CPU側に保持し、描画時だけ現在FrameのUpload Arenaへ転送する。

	// テクスチャ系データ
	std::string textureFilePath;			 // テクスチャファイルパス
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{}; // GPUハンドル

};


} // namespace Ken4lowEngine
