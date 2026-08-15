#ifndef KEN4LOW_GPU_FLUID_COMMON_HLSLI
#define KEN4LOW_GPU_FLUID_COMMON_HLSLI

struct GpuFluidSimulationConstants
{
	uint gridWidth;
	uint gridHeight;
	float invGridWidth;
	float invGridHeight;

	float cellSize;
	float invCellSize;
	float deltaTime;
	float elapsedTime;

	float velocityDissipation;
	float densityDissipation;
	float temperatureDissipation;
	float vorticityStrength;

	float ambientTemperature;
	float buoyancy;
	float smokeWeight;
	float padding;
};

// セル中心を正規化UVへ変換し、全Fluid Compute Passのサンプリング規約を統一する。
float2 GpuFluidCellToUv(uint2 cell, GpuFluidSimulationConstants fluid)
{
	return (float2(cell) + 0.5f) * float2(fluid.invGridWidth, fluid.invGridHeight);
}

// Clamp samplerだけに境界処理を任せず、常にセル中心範囲からサンプリングする。
float2 GpuFluidClampUvToCellCenters(float2 uv, GpuFluidSimulationConstants fluid)
{
	const float2 halfTexel = 0.5f * float2(fluid.invGridWidth, fluid.invGridHeight);
	return clamp(uv, halfTexel, 1.0f - halfTexel);
}

bool GpuFluidIsInsideGrid(int2 cell, GpuFluidSimulationConstants fluid)
{
	return cell.x >= 0 && cell.y >= 0 &&
		cell.x < int(fluid.gridWidth) && cell.y < int(fluid.gridHeight);
}

#endif // KEN4LOW_GPU_FLUID_COMMON_HLSLI
