#pragma once
#include "DX12Include.h"
#include "PipelineFactory.h"
#include "SkyBoxPipelineSet.h"

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// -------------------------------------------------------------
	///             SkyBox 描画の共通設定を行う管理クラス
	/// -------------------------------------------------------------
	class SkyBoxManager
	{
	public:
		static SkyBoxManager* GetInstance();

		void Initialize(DirectXCommon* dxCommon);
		void Finalize();

		/// <summary>
		/// SkyBox 描画前の共通設定を行う。
		/// </summary>
		void SetRenderSetting();
		void SetCloudRenderSetting();

	private:
		DirectXCommon* dxCommon_ = nullptr;

		PipelineFactory pipelineFactory_{};
		SkyBoxPipelineSet pipelineSet_{};

	private:
		SkyBoxManager() = default;
		~SkyBoxManager() = default;
		SkyBoxManager(const SkyBoxManager&) = delete;
		SkyBoxManager& operator=(const SkyBoxManager&) = delete;
	};
}