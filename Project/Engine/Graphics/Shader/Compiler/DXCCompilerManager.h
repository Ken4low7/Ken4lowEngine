#pragma once
#include <DX12Include.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///			DirectX12のHLSLコンパイラーを管理するクラス
/// -------------------------------------------------------------
class DXCCompilerManager
{
public: /// ---------- 型定義 ---------- ///

	struct ShaderCacheStats
	{
		uint64_t requestCount = 0;
		uint64_t hitCount = 0;
		uint64_t missCount = 0;
		uint64_t compileCount = 0;
		uint64_t invalidationCount = 0;
		uint64_t clearCount = 0;
		uint32_t entryCount = 0;
	};

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

	/// <summary>
	/// Source / local include tree / entry point / profile / compile optionからMachine非依存のCache Keyを構築します。
	/// </summary>
	std::string BuildShaderCacheKey(
		const wchar_t* filePath,
		const wchar_t* entryPoint,
		const wchar_t* profile) const;

	/// <summary>同一KeyのDXILがあれば共有参照を返します。</summary>
	Microsoft::WRL::ComPtr<IDxcBlob> FindCachedShader(std::string_view cacheKey);

	/// <summary>成功したDXILをSource Pathと一緒にMemory Cacheへ保存します。</summary>
	void StoreCachedShader(
		std::string cacheKey,
		std::wstring sourcePath,
		Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob);

	/// <summary>指定Sourceから作成された古いEntryだけを明示的に破棄します。</summary>
	uint32_t InvalidateShader(const wchar_t* filePath);

	/// <summary>全Shader Cacheを破棄します。</summary>
	void ClearShaderCache();

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

	ShaderCacheStats GetShaderCacheStats() const;

private: /// ---------- 内部型 ---------- ///

	struct ShaderCacheEntry
	{
		std::wstring sourcePath;
		Microsoft::WRL::ComPtr<IDxcBlob> blob;
	};

private: /// ---------- メンバ変数 ---------- ///

	ComPtr<IDxcUtils> dxcUtils_;
	ComPtr<IDxcCompiler3> dxcCompiler_;
	ComPtr<IDxcIncludeHandler> includeHandler_;

	// Phase 9.6ではDXILそのものをKey生成情報と分離して保持し、明示Invalidationも可能にする。
	mutable std::mutex shaderCacheMutex_;
	std::unordered_map<std::string, ShaderCacheEntry> shaderCache_;
	ShaderCacheStats shaderCacheStats_{};
};
} // namespace Ken4lowEngine
