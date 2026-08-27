#pragma once

#include <cstdint>

namespace Ken4lowEngine
{

/// 3Dボリューム流体の実行制御、描画品質、診断、負荷確認をまとめるEditor Panel。
class GpuVolumetricFluidDiagnosticsPanel
{
public:
	static GpuVolumetricFluidDiagnosticsPanel* GetInstance()
	{
		static GpuVolumetricFluidDiagnosticsPanel instance;
		return &instance;
	}

	void Draw();

private:
	GpuVolumetricFluidDiagnosticsPanel() = default;
	~GpuVolumetricFluidDiagnosticsPanel() = default;
	GpuVolumetricFluidDiagnosticsPanel(const GpuVolumetricFluidDiagnosticsPanel&) = delete;
	GpuVolumetricFluidDiagnosticsPanel& operator=(const GpuVolumetricFluidDiagnosticsPanel&) = delete;

	void RefreshGridEditorValues();

private:
	uint32_t pendingGridWidth_ = 64;
	uint32_t pendingGridHeight_ = 64;
	uint32_t pendingGridDepth_ = 64;
	uint32_t pendingPressureIterations_ = 32;
	float pendingCellSize_ = 0.25f;
	bool gridEditorValuesInitialized_ = false;
	bool visible_ = false; // 表示状態だけを保持し、シミュレーション設定はManager側へ集約する。
};

} // namespace Ken4lowEngine
