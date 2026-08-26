#ifndef KEN4LOW_GPU_SPH_COMMON_HLSLI
#define KEN4LOW_GPU_SPH_COMMON_HLSLI

static const uint kGpuSphInvalidIndex = 0xffffffffu;

struct GpuSphParticle
{
    float3 position;
    float density;
    float3 velocity;
    float pressure;
    float3 predictedPosition;
    float padding;
};

struct GpuSphHashEntry
{
    uint key;
    uint particleIndex;
};

struct GpuSphCellRange
{
    uint start;
    uint count;
};

struct GpuSphSimulationConstants
{
    uint activeParticleCount;
    float deltaTime;
    float particleMass;
    float smoothingRadius;

    float targetDensity;
    float pressureStiffness;
    float viscosityStrength;
    float boundaryDamping;

    float3 gravity;
    float padding0;

    float3 boundaryMin;
    float padding1;

    float3 boundaryMax;
    float padding2;

    float3 spawnOrigin;
    float spawnSpacing;

    uint spawnDimX;
    uint spawnDimY;
    uint spawnDimZ;
    uint padding3;

    float3 spatialGridMin;
    float spatialCellSize;

    uint spatialGridDimX;
    uint spatialGridDimY;
    uint spatialGridDimZ;
    uint spatialCellCount;

    uint dfsphEnabled;
    uint dfsphDensityIterations;
    uint dfsphDivergenceIterations;
    uint adaptiveCflEnabled;

    float dfsphDensityRelaxation;
    float dfsphDivergenceRelaxation;
    float dfsphDensityErrorTolerance;
    float dfsphDivergenceErrorTolerance;

    float cflNumber;
    float minimumDeltaTime;
    float surfaceTensionStrength;
    float xsphStrength;

    float boundaryFriction;
    float maxDfsphVelocityCorrection;
    uint dfsphWarmStartEnabled;
    float dfsphWarmStartStrength;

    uint oceanCouplingEnabled;
    float oceanVelocityCoupling;
    float oceanSurfaceAttraction;
    float oceanBlendBand;

    float3 oceanSurfacePoint;
    float oceanMaxCorrection;

    float3 oceanSurfaceNormal;
    float oceanPadding0;

    float3 oceanSurfaceVelocity;
    float oceanPadding1;
};

cbuffer GpuSphSimulationCB : register(b0)
{
    GpuSphSimulationConstants gSph;
};

cbuffer GpuSphDispatchCB : register(b1)
{
    uint gSortLevel;
    uint gSortLevelMask;
    uint gSortCount;
    uint gCellCount;
};

RWStructuredBuffer<GpuSphParticle> gParticles : register(u0);
RWStructuredBuffer<float4> gScratch : register(u1);
RWStructuredBuffer<GpuSphHashEntry> gHashEntries : register(u2);
RWStructuredBuffer<GpuSphCellRange> gCellRanges : register(u3);
RWStructuredBuffer<float4> gDfSphState : register(u4);

bool GpuSphIsActiveParticle(uint index)
{
    return index < gSph.activeParticleCount;
}

int3 GpuSphPositionToCell(float3 positionValue)
{
    const float cellSize = max(gSph.spatialCellSize, 1.0e-6f);
    return int3(floor((positionValue - gSph.spatialGridMin) / cellSize));
}

bool GpuSphIsCellValid(int3 cell)
{
    return
        cell.x >= 0 && cell.y >= 0 && cell.z >= 0 &&
        cell.x < int(gSph.spatialGridDimX) &&
        cell.y < int(gSph.spatialGridDimY) &&
        cell.z < int(gSph.spatialGridDimZ);
}

uint GpuSphCellToKey(int3 cell)
{
    return
        uint(cell.x) +
        uint(cell.y) * gSph.spatialGridDimX +
        uint(cell.z) * gSph.spatialGridDimX * gSph.spatialGridDimY;
}

GpuSphCellRange GpuSphGetCellRange(int3 cell)
{
    GpuSphCellRange emptyRange;
    emptyRange.start = kGpuSphInvalidIndex;
    emptyRange.count = 0;
    if (!GpuSphIsCellValid(cell))
    {
        return emptyRange;
    }

    const uint key = GpuSphCellToKey(cell);
    if (key >= gSph.spatialCellCount)
    {
        return emptyRange;
    }
    return gCellRanges[key];
}

#endif // KEN4LOW_GPU_SPH_COMMON_HLSLI
