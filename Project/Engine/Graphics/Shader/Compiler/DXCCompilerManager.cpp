#include "DXCCompilerManager.h"
#include <cassert>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///					        初期化処理
/// -------------------------------------------------------------
void DXCCompilerManager::Initialize()
{
	// IDxcUtilsの生成
	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr));

	// IDxcCompiler3の生成
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr));

	// IDxcIncludeHandlerの生成
	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr));
}

void DXCCompilerManager::Finalize()
{
	dxcUtils_.Reset();
	dxcCompiler_.Reset();
	includeHandler_.Reset();
}
} // namespace Ken4lowEngine
