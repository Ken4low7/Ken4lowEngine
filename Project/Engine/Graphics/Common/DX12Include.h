#pragma once

#include <cassert>

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <dxgidebug.h>

/* ----- cpp側で書くlib一覧　-----
* #pragma comment(lib, "d3d12.lib")        // Direct3D 12用
* #pragma comment(lib, "dxgi.lib")         // DXGI (DirectX Graphics Infrastructure)用
* #pragma comment(lib, "dxguid.lib")       // DXGIやD3D12で使用するGUID定義用
* #pragma comment(lib, "dxcompiler.lib")   // DXC (DirectX Shader Compiler)用
* #pragma comment(lib, "dxguid.lib")       // DXGIデバッグ用 (dxgidebugを使用する場合)
*/

#include <wrl.h>

using namespace Microsoft::WRL;

#include "BlendModeType.h"
namespace Ken4lowEngine
{

} // namespace Ken4lowEngine
