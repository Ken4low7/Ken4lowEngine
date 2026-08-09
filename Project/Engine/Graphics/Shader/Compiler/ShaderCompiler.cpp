#include "ShaderCompiler.h"
#include "DXCCompilerManager.h"
#include <cassert>
#include <format>

namespace Ken4lowEngine
{

#pragma comment(lib, "dxcompiler.lib")

	void ShaderCompiler::ValidateArguments(const ShaderDescriptor& desc, DXCCompilerManager* dxcManager)
	{
		assert(dxcManager != nullptr);
		assert(desc.filePath != nullptr);
		assert(desc.entryPoint != nullptr);
		assert(desc.profile != nullptr);
		assert(desc.filePath[0] != L'\0');
		assert(desc.entryPoint[0] != L'\0');
		assert(desc.profile[0] != L'\0');
	}

	Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::CompileShader(const ShaderDescriptor& desc, DXCCompilerManager* dxcManager)
	{
		ValidateArguments(desc, dxcManager);

		Log(ConvertString(std::format(
			L"Begin CompileShader, name:{}, path:{}, entry:{}, profile:{}, rootSig:{}\n",
			desc.debugName,
			desc.filePath,
			desc.entryPoint,
			desc.profile,
			static_cast<int>(desc.rootSignature))));

		IDxcUtils* dxcUtils = dxcManager->GetIDxcUtils();
		IDxcCompiler3* dxcCompiler = dxcManager->GetIDxcCompiler();
		IDxcIncludeHandler* includeHandler = dxcManager->GetIncludeHandler();

		assert(dxcUtils != nullptr);
		assert(dxcCompiler != nullptr);
		assert(includeHandler != nullptr);

		/// ---------- 1. hlsl ファイル読み込み ---------- ///
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
		HRESULT hr = dxcUtils->LoadFile(desc.filePath, nullptr, &shaderSource);
		assert(SUCCEEDED(hr));
		assert(shaderSource != nullptr);

		DxcBuffer shaderSourceBuffer{};
		shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
		shaderSourceBuffer.Size = shaderSource->GetBufferSize();
		shaderSourceBuffer.Encoding = DXC_CP_UTF8;

		/// ---------- 2. Compile ---------- ///
		LPCWSTR arguments[] =
		{
			desc.filePath,
			L"-E", desc.entryPoint,
			L"-T", desc.profile,
			L"-Zi", L"-Qembed_debug",
			L"-Od",
			L"-Zpr"
		};

		Microsoft::WRL::ComPtr<IDxcResult> shaderResult = nullptr;
		hr = dxcCompiler->Compile(&shaderSourceBuffer, arguments, _countof(arguments), includeHandler, IID_PPV_ARGS(&shaderResult));
		assert(SUCCEEDED(hr));
		assert(shaderResult != nullptr);

		/// ---------- 3. エラー確認 ---------- ///
		Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError = nullptr;
		hr = shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
		assert(SUCCEEDED(hr));

		if (shaderError != nullptr && shaderError->GetStringLength() != 0)
		{
			Log(shaderError->GetStringPointer());
			assert(false);
		}

		/// ---------- 4. バイナリ取得 ---------- ///
		Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
		hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
		assert(SUCCEEDED(hr));
		assert(shaderBlob != nullptr);

		Log(ConvertString(std::format(L"Compile Succeeded, name:{}, path:{}, profile:{}\n", desc.debugName, desc.filePath, desc.profile)));

		return shaderBlob;
	}

} // namespace Ken4lowEngine