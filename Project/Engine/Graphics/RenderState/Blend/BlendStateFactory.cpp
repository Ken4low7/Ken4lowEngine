#include "BlendStateFactory.h"

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///				シングルトンインスタンス取得
/// -------------------------------------------------------------
BlendStateFactory* BlendStateFactory::GetInstance()
{
	static BlendStateFactory instance;
	return &instance;
}


/// -------------------------------------------------------------
///				ブレンドステートの初期化処理
/// -------------------------------------------------------------
void BlendStateFactory::Initialize()
{
	for (uint32_t i = 0; i < blendModeNum; ++i)
	{
		BlendMode mode = static_cast<BlendMode>(i);
		D3D12_RENDER_TARGET_BLEND_DESC desc = {};
		desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		switch (mode)
		{
		case BlendMode::kBlendModeNone:
			desc.BlendEnable = FALSE;
			break;

		case BlendMode::kBlendModeNormal:
			desc.BlendEnable = TRUE;
			desc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
			desc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			desc.BlendOp = D3D12_BLEND_OP_ADD;
			desc.SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.DestBlendAlpha = D3D12_BLEND_ZERO;
			desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			break;

		case BlendMode::kBlendModeAdd:
			desc.BlendEnable = TRUE;
			desc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
			desc.DestBlend = D3D12_BLEND_ONE;
			desc.BlendOp = D3D12_BLEND_OP_ADD;
			desc.SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.DestBlendAlpha = D3D12_BLEND_ZERO;
			desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			break;

		case BlendMode::kBlendModeSubtract:
			desc.BlendEnable = TRUE;
			desc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
			desc.DestBlend = D3D12_BLEND_ONE;
			desc.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
			desc.SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.DestBlendAlpha = D3D12_BLEND_ZERO;
			desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			break;

		case BlendMode::kBlendModeMultiply:
			desc.BlendEnable = TRUE;
			desc.SrcBlend = D3D12_BLEND_ZERO;
			desc.DestBlend = D3D12_BLEND_SRC_COLOR;
			desc.BlendOp = D3D12_BLEND_OP_ADD;
			desc.SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.DestBlendAlpha = D3D12_BLEND_ZERO;
			desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			break;

		case BlendMode::kBlendModeScreen:
			desc.BlendEnable = TRUE;
			desc.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
			desc.DestBlend = D3D12_BLEND_ONE;
			desc.BlendOp = D3D12_BLEND_OP_ADD;
			desc.SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.DestBlendAlpha = D3D12_BLEND_ZERO;
			desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			break;

		default:
			assert(false && "Invalid BlendMode.");
			break;
		}

		blendDescs_[i] = desc;
	}
}

/// -------------------------------------------------------------
///					ブレンドステートの終了処理
/// -------------------------------------------------------------
void BlendStateFactory::Finalize()
{
	customBlends_.clear();
}

/// -------------------------------------------------------------
///					BlendMode に応じたアクセッサ
/// -------------------------------------------------------------
const D3D12_RENDER_TARGET_BLEND_DESC& BlendStateFactory::GetBlendDesc(BlendMode blendMode) const
{
	uint32_t index = static_cast<uint32_t>(blendMode);
	assert(index < blendDescs_.size());
	return blendDescs_[index];
}


/// -------------------------------------------------------------
///					カスタムブレンドの登録と取得
/// -------------------------------------------------------------
void BlendStateFactory::RegisterCustomBlend(const std::string& name, const D3D12_RENDER_TARGET_BLEND_DESC& desc)
{
	customBlends_[name] = desc;
}


/// -------------------------------------------------------------
///					カスタムブレンドの取得
/// -------------------------------------------------------------
const D3D12_RENDER_TARGET_BLEND_DESC* BlendStateFactory::GetCustomBlend(const std::string& name) const
{
	auto it = customBlends_.find(name);
	if (it != customBlends_.end()) {
		return &it->second;
	}
	return nullptr;
}

} // namespace Ken4lowEngine
