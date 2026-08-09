#pragma once
#include <DX12Include.h>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///			DirectX12のHLSLコンパイラーを管理するクラス
/// -------------------------------------------------------------
class DXCCompilerManager
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// DXC 用インターフェースの初期化処理を行います。<br/>
	/// ・DxcCreateInstance で IDxcUtils を生成<br/>
	/// ・DxcCreateInstance で IDxcCompiler3 を生成<br/>
	/// ・IDxcUtils::CreateDefaultIncludeHandler で IncludeHandler を生成<br/>
	/// という流れで、シェーダーコンパイルに必要なオブジェクトをまとめて用意します。<br/>
	/// 失敗時は assert で停止します。
	/// </summary>
	void Initialize();

	void Finalize();

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// DXC のユーティリティインターフェース(IDxcUtils)を取得します。<br/>
	/// シェーダーファイルのロード、インクルードパスの解決などに使用します。
	/// </summary>
	IDxcUtils* GetIDxcUtils() const { return dxcUtils_.Get(); }

	/// <summary>
	/// シェーダーコンパイラ本体(IDxcCompiler3)を取得します。<br/>
	/// 実際に HLSL をコンパイルする際に使用します。
	/// </summary>
	IDxcCompiler3* GetIDxcCompiler() const { return dxcCompiler_.Get(); }

	/// <summary>
	/// インクルードファイル解決用ハンドラ(IDxcIncludeHandler)を取得します。<br/>
	/// `#include` 付きの HLSL をコンパイルする際に、ShaderCompiler 側から渡して使います。
	/// </summary>
	IDxcIncludeHandler* GetIncludeHandler() const { return includeHandler_.Get(); }

private: /// ---------- メンバ変数 ---------- ///

	ComPtr<IDxcUtils> dxcUtils_;
	ComPtr<IDxcCompiler3> dxcCompiler_;
	ComPtr<IDxcIncludeHandler> includeHandler_;
};
} // namespace Ken4lowEngine
