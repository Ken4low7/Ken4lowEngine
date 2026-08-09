#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///			DirectX12のフェンスを管理するクラス
/// -------------------------------------------------------------
class DX12FenceManager
{
public: /// ---------- メンバ関数 ---------- ///

	void Initialize(ID3D12Device* device);
	void Finalize();

	/// 従来互換: 新しいFence値を発行してQueueへSignalする。
	void Signal(ID3D12CommandQueue* commandQueue);

	/// 発行したFence値を返し、FrameResourceごとの完了値として保存できるようにする。
	UINT64 SignalAndGetValue(ID3D12CommandQueue* commandQueue);

	/// 従来互換: 最後に発行したFence値まで待機する。
	void Wait();

	/// 指定Fence値まで必要な場合だけ待機する。
	void WaitForValue(UINT64 fenceValue);

	UINT64 GetCompletedValue() const;
	UINT64 GetCurrentValue() const { return fenceValue_; }

private: /// ---------- メンバ変数 ---------- ///

	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	HANDLE fenceEvent_ = nullptr;
	UINT64 fenceValue_ = 0;
};

} // namespace Ken4lowEngine
