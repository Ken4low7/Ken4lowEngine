#pragma once
#include "DX12Include.h"
#include "LogString.h"
#include "ShaderManifestTypes.h"

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class DXCCompilerManager;

	/// <summary>
	/// ShaderManifest に定義された契約情報をもとに、
	/// HLSL を DXC でコンパイルする専用クラス。
	/// 
	/// このクラスは「どのシェーダーを使うか」を判断しない。
	/// path / entry point / profile / root signature 契約は
	/// ShaderManifest 側で一元管理する。
	/// </summary>
	class ShaderCompiler
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// ShaderDescriptor の契約情報を使ってシェーダーをコンパイルする。
		/// </summary>
		static Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
			const ShaderDescriptor& desc,
			DXCCompilerManager* dxcManager);

	private: /// ---------- 内部関数 ---------- ///

		/// <summary>
		/// シェーダーディスクリプタとDXCコンパイラマネージャーの引数を検証します。
		/// </summary>
		/// <param name="desc">検証するシェーダーディスクリプタ。</param>
		/// <param name="dxcManager">DXCコンパイラマネージャーへのポインタ。</param>
		static void ValidateArguments(
			const ShaderDescriptor& desc,
			DXCCompilerManager* dxcManager);
	};

} // namespace Ken4lowEngine