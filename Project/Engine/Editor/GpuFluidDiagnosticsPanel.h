#pragma once

#include <cstdint>

namespace Ken4lowEngine
{

/// Phase16 GPU FluidのRuntime制御・診断・Stress TestをまとめるEditor Panel。
class GpuFluidDiagnosticsPanel
{
public:
	static GpuFluidDiagnosticsPanel* GetInstance()
	{
		static GpuFluidDiagnosticsPanel instance;
		return &instance;
	}

	void Draw();

private:
	GpuFluidDiagnosticsPanel() = default;
	~GpuFluidDiagnosticsPanel() = default;
	GpuFluidDiagnosticsPanel(const GpuFluidDiagnosticsPanel&) = delete;
	GpuFluidDiagnosticsPanel& operator=(const GpuFluidDiagnosticsPanel&) = delete;

	void RefreshGridEditorValues();

private:
	uint32_t pendingGridWidth_ = 256;
	uint32_t pendingGridHeight_ = 256;
	uint32_t pendingPressureIterations_ = 40;
	float pendingCellSize_ = 0.1f;
	bool gridEditorValuesInitialized_ = false;
	bool visible_ = false;
};

} // namespace Ken4lowEngine
