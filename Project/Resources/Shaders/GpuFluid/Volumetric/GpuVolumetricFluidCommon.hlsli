struct GpuVolumetricFluidSimulationConstants
{
    uint gridWidth;
    uint gridHeight;
    uint gridDepth;
    float cellSize;

    float invGridWidth;
    float invGridHeight;
    float invGridDepth;
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
    float padding0;
    float padding1;
    float padding2;
};

float3 GpuVolumetricFluidCellToUvw(
    uint3 cell,
    GpuVolumetricFluidSimulationConstants fluid)
{
    return (float3(cell) + 0.5f) *
        float3(fluid.invGridWidth, fluid.invGridHeight, fluid.invGridDepth);
}

float3 GpuVolumetricFluidClampUvwToCellCenters(
    float3 uvw,
    GpuVolumetricFluidSimulationConstants fluid)
{
    // Trilinear sampleがVolume外へ跨がないよう、UVWを最外周voxel中心までに制限する。
    const float3 halfTexel = 0.5f *
        float3(fluid.invGridWidth, fluid.invGridHeight, fluid.invGridDepth);
    return clamp(uvw, halfTexel, 1.0f - halfTexel);
}

int3 GpuVolumetricFluidClampCell(
    int3 cell,
    GpuVolumetricFluidSimulationConstants fluid)
{
    const int3 maxCell = int3(
        int(fluid.gridWidth) - 1,
        int(fluid.gridHeight) - 1,
        int(fluid.gridDepth) - 1);
    return clamp(cell, int3(0, 0, 0), maxCell);
}

bool GpuVolumetricFluidIsInsideGrid(
    int3 cell,
    GpuVolumetricFluidSimulationConstants fluid)
{
    return all(cell >= int3(0, 0, 0)) &&
        cell.x < int(fluid.gridWidth) &&
        cell.y < int(fluid.gridHeight) &&
        cell.z < int(fluid.gridDepth);
}
