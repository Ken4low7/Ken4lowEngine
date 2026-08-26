#ifndef KEN4LOW_GPU_SPH_KERNEL_HLSLI
#define KEN4LOW_GPU_SPH_KERNEL_HLSLI

static const float kGpuSphPi = 3.14159265358979323846f;
static const float kGpuSphTaitExponent = 7.0f;
static const float kGpuSphNegativePressureRatio = 0.02f;
static const float kGpuSphTensileCorrectionStrength = 0.25f;
static const float kGpuSphSurfaceTension = 0.0728f;
static const float kGpuSphXsphStrength = 0.025f;
static const float kGpuSphCflNumber = 0.4f;

// Poly6 kernelは密度推定に使用する。
float GpuSphPoly6Kernel(float distanceValue, float smoothingRadius)
{
    if (distanceValue < 0.0f || distanceValue >= smoothingRadius || smoothingRadius <= 0.0f)
    {
        return 0.0f;
    }

    const float h2 = smoothingRadius * smoothingRadius;
    const float q = h2 - distanceValue * distanceValue;
    const float h4 = h2 * h2;
    const float h8 = h4 * h4;
    const float h9 = h8 * smoothingRadius;
    const float coefficient = 315.0f / (64.0f * kGpuSphPi * h9);
    return coefficient * q * q * q;
}

// Spiky gradientは圧力勾配の方向と大きさを返す。
float3 GpuSphSpikyGradient(float3 delta, float smoothingRadius)
{
    const float distanceValue = length(delta);
    if (distanceValue <= 1.0e-6f || distanceValue >= smoothingRadius || smoothingRadius <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float h2 = smoothingRadius * smoothingRadius;
    const float h4 = h2 * h2;
    const float h6 = h4 * h2;
    const float q = smoothingRadius - distanceValue;
    const float coefficient = -45.0f / (kGpuSphPi * h6);
    return coefficient * q * q * (delta / distanceValue);
}

// Viscosity Laplacianは近傍速度差を滑らかにする。
float GpuSphViscosityLaplacian(float distanceValue, float smoothingRadius)
{
    if (distanceValue < 0.0f || distanceValue >= smoothingRadius || smoothingRadius <= 0.0f)
    {
        return 0.0f;
    }

    const float h2 = smoothingRadius * smoothingRadius;
    const float h4 = h2 * h2;
    const float h6 = h4 * h2;
    return (45.0f / (kGpuSphPi * h6)) * (smoothingRadius - distanceValue);
}

// W7では線形圧力をTait EOSへ置き換え、弱圧縮性液体として密度を保つ。
float GpuSphTaitPressure(float densityValue, float targetDensity, float stiffness)
{
    const float safeTargetDensity = max(targetDensity, 1.0e-5f);
    const float densityRatio = clamp(densityValue / safeTargetDensity, 0.01f, 2.0f);
    const float bulkModulus = max(stiffness, 0.0f) * safeTargetDensity / kGpuSphTaitExponent;
    const float pressureValue = bulkModulus * (pow(densityRatio, kGpuSphTaitExponent) - 1.0f);
    const float minimumPressure = -bulkModulus * kGpuSphNegativePressureRatio;
    return max(pressureValue, minimumPressure);
}

// 負圧時の過剰な凝集を弱め、粒子の不安定なペア形成を抑える。
float GpuSphApplyTensileCorrection(float pressureTerm, float distanceValue, float smoothingRadius)
{
    if (pressureTerm >= 0.0f)
    {
        return pressureTerm;
    }

    const float referenceKernel = max(GpuSphPoly6Kernel(smoothingRadius * 0.3f, smoothingRadius), 1.0e-6f);
    const float normalizedKernel = saturate(GpuSphPoly6Kernel(distanceValue, smoothingRadius) / referenceKernel);
    const float correction = kGpuSphTensileCorrectionStrength * pow(normalizedKernel, 4.0f);
    return pressureTerm * (1.0f - correction);
}

// 表面張力はPoly6を0～1へ正規化したCohesion重みとして使用する。
float GpuSphCohesionWeight(float distanceValue, float smoothingRadius)
{
    if (distanceValue <= 1.0e-6f || distanceValue >= smoothingRadius)
    {
        return 0.0f;
    }

    const float centerKernel = max(GpuSphPoly6Kernel(0.0f, smoothingRadius), 1.0e-6f);
    return saturate(GpuSphPoly6Kernel(distanceValue, smoothingRadius) / centerKernel);
}

// GPU Readback無しでCFL条件を満たすよう、1 Stepで進み過ぎる速度を制限する。
float3 GpuSphClampVelocityByCfl(float3 velocityValue, float smoothingRadius, float deltaTime)
{
    const float safeDeltaTime = max(deltaTime, 1.0e-6f);
    const float maxVelocity = max(kGpuSphCflNumber * smoothingRadius / safeDeltaTime, 0.1f);
    const float speed = length(velocityValue);
    if (speed <= maxVelocity || speed <= 1.0e-6f)
    {
        return velocityValue;
    }
    return velocityValue * (maxVelocity / speed);
}

#endif // KEN4LOW_GPU_SPH_KERNEL_HLSLI
