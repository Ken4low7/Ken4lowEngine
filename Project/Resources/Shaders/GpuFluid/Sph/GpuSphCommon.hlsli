#ifndef KEN4LOW_GPU_SPH_COMMON_HLSLI
#define KEN4LOW_GPU_SPH_COMMON_HLSLI

struct GpuSphParticle
{
	float3 position;
	float density;
	float3 velocity;
	float pressure;
	float3 predictedPosition;
	float padding;
};

// W5.1ではParticle Bufferのstride契約だけを定義し、SPH計算はW5.2以降で追加する。

#endif // KEN4LOW_GPU_SPH_COMMON_HLSLI
