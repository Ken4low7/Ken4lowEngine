#ifndef KEN4LOW_GPU_SPH_KERNEL_HLSLI
#define KEN4LOW_GPU_SPH_KERNEL_HLSLI

static const float kGpuSphPi = 3.14159265358979323846f;

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

#endif // KEN4LOW_GPU_SPH_KERNEL_HLSLI
