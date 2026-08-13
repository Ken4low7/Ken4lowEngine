#include "GpuParticleBuffers.h"
#include <DirectXCommon.h>
#include <ResourceManager.h>
#include <LogString.h>
#include <SRVManager.h>
#include <UAVManager.h>
#include <Camera.h>
#include <CameraManager.h>

namespace Ken4lowEngine
{

void GpuParticleBuffers::Initialize(Camera* camera)
{
	camera_ = camera;

	CreateParticleBuffer();
	InitializePerViewData();
	InitializeEmitterData();
	InitializePerFrameData();
	CreateFreeListIndexBuffer();
	CreateFreeListBuffer();
	CreateGpuDrivenDrawBuffers();

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	const uint32_t frameCount = dxCommon->GetCommandManager()->GetFrameResourceCount();
	perFrameBuffers_.Initialize(dxCommon->GetDevice(), frameCount);
	perFrameBuffers_.WriteAll(perFrameData_);
	emitterBootstrapBuffers_.Initialize(dxCommon->GetDevice(), frameCount);
	emitterBootstrapBuffers_.WriteAll(emitterData_[0]);

	particleBuffer_->SetName(L"GpuParticleBuffers::particleBuffer_");
	freeListIndexBuffer_->SetName(L"GpuParticleBuffers::freeListIndexBuffer_");
	freeListBuffer_->SetName(L"GpuParticleBuffers::freeListBuffer_");
	visibleParticleIndexBuffer_->SetName(L"GpuParticleBuffers::visibleParticleIndexBuffer_");
	indirectDrawArgsBuffer_->SetName(L"GpuParticleBuffers::indirectDrawArgsBuffer_");
}

void GpuParticleBuffers::Update(float deltaTime)
{
	perFrameData_.deltaTime = deltaTime;

	const Matrix4x4 viewMatrix = CameraManager::GetInstance()->GetActiveViewMatrix();
	const Matrix4x4 projectionMatrix = CameraManager::GetInstance()->GetActiveProjectionMatrix();
	const Matrix4x4 viewProjectionMatrix = Matrix4x4::Multiply(viewMatrix, projectionMatrix);

	Matrix4x4 billboardMatrix = Matrix4x4::Inverse(viewMatrix);
	billboardMatrix.m[3][0] = billboardMatrix.m[3][1] = billboardMatrix.m[3][2] = 0.0f;

	perViewData_.viewProjectionMatrix = viewProjectionMatrix;
	perViewData_.billboardMatrix = Matrix4x4::Transpose(Matrix4x4::Inverse(billboardMatrix));
	perViewData_.billboardMode = PackBillboardMode(
		GpuParticleKind::Sprite,
		static_cast<uint32_t>(BillboardMode::Camera)
	);

	perFrameData_.time += perFrameData_.deltaTime;
	const uint32_t frameIndex = DirectXCommon::GetInstance()->GetCommandManager()->GetCurrentFrameIndex();
	perFrameBuffers_.WriteFrame(frameIndex, perFrameData_);
	perViewDirty_ = true; // Update後のPerViewは次のCBV要求時に現在Frame専用Arenaへ1回だけ転送する。
}

ID3D12Resource* GpuParticleBuffers::GetPerFrameBuffer()
{
	const uint32_t frameIndex = DirectXCommon::GetInstance()->GetCommandManager()->GetCurrentFrameIndex();
	return perFrameBuffers_.GetResource(frameIndex).Get();
}

ID3D12Resource* GpuParticleBuffers::GetEmitterBuffer()
{
	const uint32_t frameIndex = DirectXCommon::GetInstance()->GetCommandManager()->GetCurrentFrameIndex();
	return emitterBootstrapBuffers_.GetResource(frameIndex).Get();
}

GpuEmitterCBData* GpuParticleBuffers::GetEmitterCBData(uint32_t slot)
{
	return &emitterData_[slot % kEmitterCBSlotCount];
}

D3D12_GPU_VIRTUAL_ADDRESS GpuParticleBuffers::GetEmitterCBAddress(uint32_t slot)
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	const GpuEmitterCBData& data = emitterData_[slot % kEmitterCBSlotCount];
	const FrameUploadArena::Allocation allocation = dxCommon->GetFrameUploadArena().AllocateConstant(data);
	return allocation.IsValid() ? allocation.gpuAddress : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS GpuParticleBuffers::GetPerViewCBAddress()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	const uint32_t frameIndex = dxCommon->GetCommandManager()->GetCurrentFrameIndex();
	if (perViewDirty_ || perViewUploadedFrameIndex_ != frameIndex || perViewGpuAddress_ == 0)
	{
		const FrameUploadArena::Allocation allocation = dxCommon->GetFrameUploadArena().AllocateConstant(perViewData_);
		if (!allocation.IsValid())
		{
			return 0;
		}
		perViewGpuAddress_ = allocation.gpuAddress;
		perViewUploadedFrameIndex_ = frameIndex;
		perViewDirty_ = false;
	}
	return perViewGpuAddress_;
}

D3D12_GPU_VIRTUAL_ADDRESS GpuParticleBuffers::GetPerFrameCBAddress()
{
	ID3D12Resource* resource = GetPerFrameBuffer();
	return resource ? resource->GetGPUVirtualAddress() : 0;
}

D3D12_GPU_VIRTUAL_ADDRESS GpuParticleBuffers::GetGpuDrivenDrawCBAddress(uint32_t renderGroup, uint32_t primitiveCount, bool indexed)
{
	GpuDrivenDrawCBData data{};
	data.renderGroup = renderGroup;
	data.primitiveCount = primitiveCount;
	data.indexed = indexed ? 1u : 0u;
	data.maxParticles = kMaxParticles;

	// Drawごとの小さなCompaction定数は現在FrameのUpload Arenaから確保してGPU待ちを作らない。
	const FrameUploadArena::Allocation allocation = DirectXCommon::GetInstance()->GetFrameUploadArena().AllocateConstant(data);
	return allocation.IsValid() ? allocation.gpuAddress : 0;
}

void GpuParticleBuffers::CreateParticleBuffer()
{
	particleBuffer_ = ResourceManager::CreateBufferResource(
		DirectXCommon::GetInstance()->GetDevice(), sizeof(ParticleCS) * kMaxParticles,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON
	);

	particleSrvIndex_ = SRVManager::GetInstance()->Allocate();
	SRVManager::GetInstance()->CreateSRVForStructureBuffer(particleSrvIndex_, particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));

	particleUavIndex_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForStructuredBuffer(particleUavIndex_, particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));
}

void GpuParticleBuffers::InitializePerViewData()
{
	perViewData_ = {};
	perViewData_.viewProjectionMatrix = Matrix4x4::MakeIdentity();
	perViewData_.billboardMatrix = Matrix4x4::MakeIdentity();
	perViewData_.billboardMode = PackBillboardMode(
		GpuParticleKind::Sprite,
		static_cast<uint32_t>(BillboardMode::Camera)
	);
	perViewDirty_ = true;
}

void GpuParticleBuffers::InitializeEmitterData()
{
	emitterData_ = {};
	GpuEmitterCBData& cb0 = emitterData_[0];
	cb0.count = 10;
	cb0.frequency = 0.5f;
	cb0.frequencyTime = 0.0f;
	cb0.translate = { 0.0f, 2.0f, 0.0f };
	cb0.radius = 1.0f;
	cb0.emit = 1;
	cb0.type = static_cast<uint32_t>(GpuParticleType::Default);
	cb0.billboardMode = PackBillboardMode(
		GpuParticleKind::Sprite,
		static_cast<uint32_t>(BillboardMode::Camera)
	);
}

void GpuParticleBuffers::InitializePerFrameData()
{
	perFrameData_ = {};
	perFrameData_.time = 0.0f;
	perFrameData_.deltaTime = 1.0f / 60.0f;
}

void GpuParticleBuffers::CreateFreeListIndexBuffer()
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();

	freeListIndexBuffer_ = ResourceManager::CreateBufferResource(
		device,
		sizeof(int32_t),
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON
	);

	freeListIndexUavIndex_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForStructuredBuffer(freeListIndexUavIndex_, freeListIndexBuffer_.Get(), 1, sizeof(int32_t));
}

void GpuParticleBuffers::CreateFreeListBuffer()
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();

	freeListBuffer_ = ResourceManager::CreateBufferResource(
		device,
		sizeof(int32_t) * kMaxParticles,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON
	);

	freeListUavIndex_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForStructuredBuffer(freeListUavIndex_, freeListBuffer_.Get(), kMaxParticles, sizeof(int32_t));
}

void GpuParticleBuffers::CreateGpuDrivenDrawBuffers()
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();

	visibleParticleIndexBuffer_ = ResourceManager::CreateBufferResource(
		device,
		sizeof(uint32_t) * kMaxParticles,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON);

	visibleParticleIndexUavIndex_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForStructuredBuffer(
		visibleParticleIndexUavIndex_, visibleParticleIndexBuffer_.Get(), kMaxParticles, sizeof(uint32_t));

	visibleParticleIndexSrvIndex_ = SRVManager::GetInstance()->Allocate();
	SRVManager::GetInstance()->CreateSRVForStructureBuffer(
		visibleParticleIndexSrvIndex_, visibleParticleIndexBuffer_.Get(), kMaxParticles, sizeof(uint32_t));

	indirectDrawArgsBuffer_ = ResourceManager::CreateBufferResource(
		device,
		sizeof(uint32_t) * kIndirectArgumentWordCount,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON);

	// GPU-driven scratch bufferはCOMMONから最初のUAVアクセスへ暗黙promotionさせ、生成時State警告を出さない。
	indirectDrawArgsUavIndex_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForStructuredBuffer(
		indirectDrawArgsUavIndex_, indirectDrawArgsBuffer_.Get(), kIndirectArgumentWordCount, sizeof(uint32_t));
}

} // namespace Ken4lowEngine
