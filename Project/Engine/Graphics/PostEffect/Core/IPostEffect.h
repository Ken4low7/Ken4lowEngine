#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class PostEffectPipelineBuilder;


/// -------------------------------------------------------------
///				ポストエフェクトのインターフェース
/// -------------------------------------------------------------
class IPostEffect
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// IPostEffect の仮想デストラクタ。デフォルト実装を使用し、派生クラスを安全に破棄できるようにします。
	/// </summary>
	virtual ~IPostEffect() = default;

	/// <summary>
	/// DirectX の共通リソースとポストエフェクトパイプラインビルダーを用いて初期化を行う純粋仮想関数。
	/// </summary>
	/// <param name="dxCommon">DirectX に関する共通リソースやコンテキストを提供する DirectXCommon オブジェクトへのポインタ。</param>
	/// <param name="builder">ポストエフェクトのパイプラインを構築・設定するための PostEffectPipelineBuilder オブジェクトへのポインタ。</param>
	virtual void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) = 0;

	/// <summary>
	/// ポストエフェクトの終了処理を行う純粋仮想関数。派生クラスで実装する必要があります。
	/// </summary>
	virtual void Finalize() = 0;

	/// <summary>
	/// オブジェクトの状態を更新します。仮想関数であり、派生クラスでオーバーライドできます。
	/// </summary>
	virtual void Update() {} // 更新処理が必要ない場合もあるので空でOK

	/// <summary>
	/// コマンドリストに対して SRV/UAV/DSV のバインディングや状態を適用するための純粋仮想関数。派生クラスで実装する必要があります。
	/// </summary>
	/// <param name="commandList">操作対象の ID3D12GraphicsCommandList へのポインタ。リソースバインディングや描画コマンドを記録するコマンドリスト。</param>
	/// <param name="srvIndex">Shader Resource View (SRV) の基準インデックス。デスクリプタヒープやルートテーブル内の開始位置を示します。</param>
	/// <param name="uavIndex">Unordered Access View (UAV) の基準インデックス。</param>
	/// <param name="dsvIndex">Depth Stencil View (DSV) のインデックス。</param>
	virtual void Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex) = 0;

	/// <summary>
	/// ImGui の描画処理を行う仮想メンバ関数。
	/// </summary>
	virtual void DrawImGui() {} // ImGuiが必要ない場合もあるので空でOK
};


} // namespace Ken4lowEngine
