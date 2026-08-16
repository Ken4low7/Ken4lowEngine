#pragma once

#include "BlendModeType.h"
#include "DX12Include.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// <summary>CPUで生成したBlade Trail帯をそのままGPUへ送る頂点。</summary>
	struct BladeTrailVertex
	{
		Vector3 position{};
		Vector2 texcoord{};
		Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	static_assert(sizeof(BladeTrailVertex) == 36, "BladeTrailVertex input layout drifted.");

	/// <summary>
	/// Root/Tip履歴から生成済みの三角形列を描画する共有Renderer。
	/// ComponentごとにPSOを複製せず、FrameUploadArenaだけを毎Frame使用する。
	/// </summary>
	class BladeTrailRenderer final
	{
	public:
		static BladeTrailRenderer* GetInstance();

		void Acquire();
		void Release();
		void Draw(std::span<const BladeTrailVertex> vertices, const std::string& texturePath, BlendMode blendMode);

		[[nodiscard]] bool IsInitialized() const { return initialized_; }
		[[nodiscard]] uint32_t GetReferenceCount() const { return referenceCount_; }

	private:
		BladeTrailRenderer() = default;
		~BladeTrailRenderer() = default;
		BladeTrailRenderer(const BladeTrailRenderer&) = delete;
		BladeTrailRenderer& operator=(const BladeTrailRenderer&) = delete;

		void Initialize();
		void Finalize();
		void CreateRootSignature();
		void CreatePipelineState(BlendMode blendMode);
		ID3D12PipelineState* GetPipelineState(BlendMode blendMode) const;

	private:
		DirectXCommon* dxCommon_ = nullptr;
		ComPtr<ID3D12RootSignature> rootSignature_;
		std::array<ComPtr<ID3D12PipelineState>, static_cast<size_t>(BlendMode::kcountOfBlendMode)> pipelineStates_{};
		uint32_t referenceCount_ = 0;
		bool initialized_ = false;
	};
}
