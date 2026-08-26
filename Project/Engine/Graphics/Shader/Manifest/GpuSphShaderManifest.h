#pragma once

#include "ShaderManifestTypes.h"

#include <cstdint>
#include <stdexcept>

namespace Ken4lowEngine
{

enum class GpuSphComputeShaderId : uint32_t
{
    Reset = 0,
    Gravity,
    Predict,
    BoundaryPredicted,
    Density,
    PressureProperty,
    PressureForce,
    ViscosityDelta,
    ViscosityApply,
    Integrate,
    BoundaryPosition,
    SpatialBuildKeys,
    SpatialBitonicSort,
    SpatialClearCellRanges,
    SpatialBuildCellRanges,
    DfSphFactor,
    DfSphDensityPrepare,
    DfSphDensityApply,
    DfSphDivergencePrepare,
    DfSphDivergenceApply,
    CflMetricClear,
    CflMetricMeasure,
    Count,
};

class GpuSphShaderManifest
{
public:
    static const ShaderDescriptor& GetCompute(GpuSphComputeShaderId id)
    {
        switch (id)
        {
        case GpuSphComputeShaderId::Reset:
        {
            static const ShaderDescriptor desc{ L"GpuSphResetCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphReset.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::Gravity:
        {
            static const ShaderDescriptor desc{ L"GpuSphGravityCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphGravity.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::Predict:
        {
            static const ShaderDescriptor desc{ L"GpuSphPredictCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphPredictedPosition.CS.hlsl", L"Predict", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::BoundaryPredicted:
        {
            static const ShaderDescriptor desc{ L"GpuSphBoundaryPredictedCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphBoundary.CS.hlsl", L"ConstrainPredicted", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::Density:
        {
            static const ShaderDescriptor desc{ L"GpuSphDensityCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphDensity.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::PressureProperty:
        {
            static const ShaderDescriptor desc{ L"GpuSphPressurePropertyCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphPressure.CS.hlsl", L"ComputePressure", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::PressureForce:
        {
            static const ShaderDescriptor desc{ L"GpuSphPressureForceCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphPressure.CS.hlsl", L"ApplyPressure", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::ViscosityDelta:
        {
            static const ShaderDescriptor desc{ L"GpuSphViscosityDeltaCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphViscosity.CS.hlsl", L"ComputeViscosityDelta", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::ViscosityApply:
        {
            static const ShaderDescriptor desc{ L"GpuSphViscosityApplyCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphViscosity.CS.hlsl", L"ApplyViscosity", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::Integrate:
        {
            static const ShaderDescriptor desc{ L"GpuSphIntegrateCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphPredictedPosition.CS.hlsl", L"Integrate", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::BoundaryPosition:
        {
            static const ShaderDescriptor desc{ L"GpuSphBoundaryPositionCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphBoundary.CS.hlsl", L"ConstrainPosition", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::SpatialBuildKeys:
        {
            static const ShaderDescriptor desc{ L"GpuSphSpatialBuildKeysCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphSpatialBuildKeys.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::SpatialBitonicSort:
        {
            static const ShaderDescriptor desc{ L"GpuSphSpatialBitonicSortCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphSpatialBitonicSort.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::SpatialClearCellRanges:
        {
            static const ShaderDescriptor desc{ L"GpuSphSpatialClearCellRangesCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphSpatialClearCellRanges.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::SpatialBuildCellRanges:
        {
            static const ShaderDescriptor desc{ L"GpuSphSpatialBuildCellRangesCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphSpatialBuildCellRanges.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::DfSphFactor:
        {
            static const ShaderDescriptor desc{ L"GpuSphDfSphFactorCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphDfSph.CS.hlsl", L"ComputeFactor", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::DfSphDensityPrepare:
        {
            static const ShaderDescriptor desc{ L"GpuSphDfSphDensityPrepareCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphDfSph.CS.hlsl", L"PrepareDensity", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::DfSphDensityApply:
        {
            static const ShaderDescriptor desc{ L"GpuSphDfSphDensityApplyCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphDfSph.CS.hlsl", L"ApplyDensity", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::DfSphDivergencePrepare:
        {
            static const ShaderDescriptor desc{ L"GpuSphDfSphDivergencePrepareCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphDfSph.CS.hlsl", L"PrepareDivergence", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::DfSphDivergenceApply:
        {
            static const ShaderDescriptor desc{ L"GpuSphDfSphDivergenceApplyCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphDfSph.CS.hlsl", L"ApplyDivergence", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::CflMetricClear:
        {
            static const ShaderDescriptor desc{ L"GpuSphCflMetricClearCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphCflMetric.CS.hlsl", L"ClearMetric", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        case GpuSphComputeShaderId::CflMetricMeasure:
        {
            static const ShaderDescriptor desc{ L"GpuSphCflMetricMeasureCS", L"Resources/Shaders/GpuFluid/Sph/GpuSphCflMetric.CS.hlsl", L"MeasureMaxSpeed", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
            return desc;
        }
        default:
            throw std::runtime_error("GpuSphShaderManifest::GetCompute - Unknown id");
        }
    }
};

} // namespace Ken4lowEngine
