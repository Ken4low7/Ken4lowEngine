#pragma once
#include "PipelineCommon.h"

namespace Ken4lowEngine
{
	class PipelineFactory;
	class DXCCompilerManager;

	/// -------------------------------------------------------------
	///              SkyBox 描画用パイプラインをまとめるクラス
	/// -------------------------------------------------------------
	class SkyBoxPipelineSet
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// SkyBox 用パイプラインを初期化する。
		/// </summary>
		void Initialize(PipelineFactory& factory, DXCCompilerManager* dxcManager, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat);

		/// <summary>
		/// 保持中の SkyBox 用パイプラインを解放する。
		/// </summary>
		void Finalize();

	public: /// ---------- アクセッサ ---------- ///

		/// <summary>
		/// SkyBox 描画用パイプラインを取得する。
		/// </summary>
		const PipelineBundle& GetDefault() const { return defaultPipeline_; }
		const PipelineBundle& GetCloud() const { return cloudPipeline_; }

	private: /// ---------- メンバ変数 ---------- ///

		// SkyBox 描画用の PSO / RootSignature のまとまり
		PipelineBundle defaultPipeline_{};
		PipelineBundle cloudPipeline_{};
	};
}