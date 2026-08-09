#pragma once
#include "DX12Include.h"
#include "BlendModeType.h"

#include <array>
#include <unordered_map>
#include <string>

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///			ブレンドステートを生成するファクトリークラス
/// -------------------------------------------------------------
class BlendStateFactory
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// BlendStateFactory のシングルトンインスタンスを取得します。<br/>
	/// 初回呼び出し時に静的ローカル変数としてインスタンスが生成されます。
	/// </summary>
	/// <returns>BlendStateFactory の唯一のインスタンス。</returns>
	static BlendStateFactory* GetInstance();

	/// <summary>
	/// ブレンドステートの初期化処理を行います。<br/>
	/// BlendMode 列挙の全モード(kBlendModeNone / Normal / Add / Subtract / Multiply / Screen …)に対して、<br/>
	/// 対応する D3D12_RENDER_TARGET_BLEND_DESC を設定し、内部配列に保存します。<br/>
	/// アルファブレンドや加算合成など、よく使うブレンド設定を一括で用意するための関数です。
	/// </summary>
	void Initialize();

	/// <summary>
	/// BlendMode に応じた RenderTarget[0] 用のブレンド設定を取得します。<br/>
	/// Initialize() で事前に生成された配列から、指定モードの D3D12_RENDER_TARGET_BLEND_DESC を返します。<br/>
	/// index が範囲外の場合は assert でチェックされます。
	/// </summary>
	void Finalize();

	/// <summary>
	/// BlendMode に応じた RenderTarget[0] 用のブレンド設定を取得します。<br/>
	/// Initialize() で事前に生成された配列から、指定モードの D3D12_RENDER_TARGET_BLEND_DESC を返します。<br/>
	/// index が範囲外の場合は assert でチェックされます。
	/// </summary>
	/// <param name="blendMode">取得したいブレンドモード。</param>
	/// <returns>指定したブレンドモード用の D3D12_RENDER_TARGET_BLEND_DESC への参照。</returns>
	const D3D12_RENDER_TARGET_BLEND_DESC& GetBlendDesc(BlendMode blendMode) const;

	/// <summary>
	/// 名前付きカスタムブレンド設定を登録します。<br/>
	/// 例：<c>"AdditiveSoft"</c> や <c>"CustomOutlineBlend"</c> のような名前で、<br/>
	/// 任意の D3D12_RENDER_TARGET_BLEND_DESC をマップに保存しておき、あとから名前で取得できます。
	/// </summary>
	/// <param name="name">カスタムブレンドを識別する任意の文字列。</param>
	/// <param name="desc">登録するブレンド設定。</param>
	void RegisterCustomBlend(const std::string& name, const D3D12_RENDER_TARGET_BLEND_DESC& desc);

	/// <summary>
	/// 名前付きカスタムブレンド設定を取得します。<br/>
	/// RegisterCustomBlend() で登録済みの名前なら、そのブレンド設定へのポインタを返し、<br/>
	/// 見つからない場合は nullptr を返します。
	/// </summary>
	/// <param name="name">取得したいカスタムブレンドの名前。</param>
	/// <returns>
	/// 見つかった場合は D3D12_RENDER_TARGET_BLEND_DESC へのポインタ、<br/>
	/// 見つからない場合は nullptr。
	/// </returns>
	const D3D12_RENDER_TARGET_BLEND_DESC* GetCustomBlend(const std::string& name) const;

private: /// ---------- メンバ変数 ---------- ///

	std::array<D3D12_RENDER_TARGET_BLEND_DESC, blendModeNum> blendDescs_ = {};
	std::unordered_map<std::string, D3D12_RENDER_TARGET_BLEND_DESC> customBlends_;

private: /// ---------- コピー禁止 ---------- ///

	/// <summary>外部からの生成を禁止するためのプライベートコンストラクタ。</summary>
	BlendStateFactory() = default;

	/// <summary>デフォルトデストラクタ。</summary>
	~BlendStateFactory() = default;

	/// <summary>コピーコンストラクタは禁止。</summary>
	BlendStateFactory(const BlendStateFactory&) = delete;

	/// <summary>代入演算子は禁止。</summary>
	BlendStateFactory& operator=(const BlendStateFactory&) = delete;
};


} // namespace Ken4lowEngine
