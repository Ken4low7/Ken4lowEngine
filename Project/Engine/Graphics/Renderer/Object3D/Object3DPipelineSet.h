#pragma once
#include "PipelineCommon.h"
#include "Material.h"

namespace Ken4lowEngine
{
	class PipelineFactory;
	class DXCCompilerManager;

	class Object3DPipelineSet
	{
	public:
		void Initialize(
			PipelineFactory& factory,
			DXCCompilerManager* dxcManager,
			DXGI_FORMAT rtvFormat,
			DXGI_FORMAT dsvFormat);

		void Finalize();

		const PipelineBundle& GetDefault(MaterialCullMode cullMode = MaterialCullMode::Back) const;
		const PipelineBundle& GetAlpha(MaterialCullMode cullMode = MaterialCullMode::Back) const;
		const PipelineBundle& GetAdditive(MaterialCullMode cullMode = MaterialCullMode::Back) const;
		const PipelineBundle& GetShadow() const { return shadowPipeline_; }

	private:
		PipelineBundle defaultPipeline_{};
		PipelineBundle defaultFrontPipeline_{};
		PipelineBundle defaultTwoSidedPipeline_{};
		PipelineBundle alphaPipeline_{};
		PipelineBundle alphaFrontPipeline_{};
		PipelineBundle alphaTwoSidedPipeline_{};
		PipelineBundle additivePipeline_{};
		PipelineBundle additiveFrontPipeline_{};
		PipelineBundle additiveTwoSidedPipeline_{};
		PipelineBundle shadowPipeline_{};
	};
} // namespace Ken4lowEngine
