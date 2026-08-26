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
        default:
            throw std::runtime_error("GpuSphShaderManifest::GetCompute - Unknown id");
        }
    }
};

} // namespace Ken4lowEngine
