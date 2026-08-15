#pragma once

#include "DX12Include.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Ken4lowEngine
{

/// 現在のForward描画先に対応するDepth attachment情報。
struct RenderDepthBindingDesc
{
	ID3D12Resource* resource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE colorRtv{};
	D3D12_CPU_DESCRIPTOR_HANDLE writableDsv{};
	D3D12_VIEWPORT viewport{};
	float clearDepth = 1.0f;

	[[nodiscard]] bool IsValid() const
	{
		return resource != nullptr && colorRtv.ptr != 0 && writableDsv.ptr != 0 &&
			viewport.Width > 0.0f && viewport.Height > 0.0f;
	}
};

/// Phase17.10でEditorへ公開できるDepth readable-stateの軽量診断値。
struct RenderDepthContextStats
{
	uint64_t prepareCount = 0;
	uint64_t restoreCount = 0;
	uint64_t failedPrepareCount = 0;
	uint64_t overridePushCount = 0;
	uint32_t attachmentCount = 0;
	bool shaderReadPrepared = false;
};

/// Main/ReflectionのどのRenderTargetでもTransparent段階からScene Depthを安全に読むためのContext。
class RenderDepthContext
{
public:
	static RenderDepthContext* GetInstance();

	/// D24 DSVとR24 SRVの両方を作れるR24G8_TYPELESS resourceを生成する。
	static ComPtr<ID3D12Resource> CreateShaderReadableDepth24(
		ID3D12Device* device,
		uint32_t width,
		uint32_t height,
		float clearDepth = 1.0f);

	/// Main viewportのDepth targetを登録する。BackBuffer index変更時はRTVだけ更新してよい。
	void SetDefaultTarget(const RenderDepthBindingDesc& desc);
	void ClearDefaultTarget(ID3D12Resource* expectedResource = nullptr);

	/// Reflection等の一時RenderTargetをStackで上書きする。
	bool PushOverride(const RenderDepthBindingDesc& desc);
	void PopOverride();

	/// Depth resource破棄前にContext所有のread-only DSV/SRVを解放する。
	void ReleaseAttachment(ID3D12Resource* resource);

	/// Transparent開始時にDEPTH_WRITEからDEPTH_READ|PIXEL_SHADER_RESOURCEへ切り替える。
	bool PrepareForShaderRead();
	/// Transparent/Additive終了時にDEPTH_WRITEへ戻す。
	void RestoreDepthWrite();

	[[nodiscard]] bool IsPreparedForShaderRead() const { return prepared_; }
	[[nodiscard]] uint32_t GetActiveDepthSrvIndex() const;
	[[nodiscard]] D3D12_VIEWPORT GetActiveViewport() const;
	[[nodiscard]] float GetActiveClearDepth() const;
	[[nodiscard]] const RenderDepthContextStats& GetStats() const { return stats_; }

private:
	struct Attachment
	{
		uint32_t readOnlyDsvIndex = UINT32_MAX;
		uint32_t srvIndex = UINT32_MAX;
	};

	RenderDepthContext() = default;
	~RenderDepthContext() = default;
	RenderDepthContext(const RenderDepthContext&) = delete;
	RenderDepthContext& operator=(const RenderDepthContext&) = delete;

	bool EnsureAttachment(ID3D12Resource* resource);
	const RenderDepthBindingDesc* GetActiveBinding() const;
	Attachment* FindAttachment(ID3D12Resource* resource);
	const Attachment* FindAttachment(ID3D12Resource* resource) const;
	void RefreshStats();

private:
	RenderDepthBindingDesc defaultTarget_{};
	std::vector<RenderDepthBindingDesc> overrides_{};
	std::unordered_map<ID3D12Resource*, Attachment> attachments_{};
	RenderDepthBindingDesc preparedBinding_{};
	RenderDepthContextStats stats_{};
	bool prepared_ = false;
};

} // namespace Ken4lowEngine
