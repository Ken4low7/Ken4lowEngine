#pragma once
#include "PipelineCommon.h"
#include "Material.h"
#include <array>

namespace Ken4lowEngine
{
	class PipelineFactory;
	class DXCCompilerManager;

	/// <summary>Object3Dの通常描画を変更せずに使う、GPUインスタンシング専用パイプラインです。</summary>
	class InstancedObject3DPipelineSet
	{
	public:
		void Initialize(PipelineFactory& factory, DXCCompilerManager* dxcManager, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat);
		void Finalize();
		const PipelineBundle& GetDefault(MaterialCullMode cullMode = MaterialCullMode::Back) const;
		const PipelineBundle& GetShadow() const { return shadowPipeline_; }

	private:
		static size_t ToCullModeIndex(MaterialCullMode cullMode);

		std::array<PipelineBundle, 3> defaultPipelines_{};
		PipelineBundle shadowPipeline_{};
	};
}
