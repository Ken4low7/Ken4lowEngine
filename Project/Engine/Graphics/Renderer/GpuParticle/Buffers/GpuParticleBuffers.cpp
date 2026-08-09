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

// スロット数（好きな数でOK。emitを同フレームで何回Dispatchするかの上限）
static constexpr uint32_t kEmitterCBSlotCount = 256;

/// -------------------------------------------------------------
///			　　　	初期化処理
/// -------------------------------------------------------------
void GpuParticleBuffers::Initialize(Camera* camera)
{
	// 引数でカメラのポインタを受け取ってメンバ変数に記録する
	camera_ = camera;

	// パーティクルバッファの生成
	CreateParticleBuffer();

	// ビュー行列と射影行列バッファの生成
	CreatePerViewBuffer();

	// エミッターバッファの生成
	CreateEmitterBuffer();

	// 時間計測用バッファの生成
	CreatePerFrameBuffer();

	// フリーリストインデックスバッファの生成
	CreateFreeListIndexBuffer();

	// フリーリストバッファの生成
	CreateFreeListBuffer();

	particleBuffer_->SetName(L"GpuParticleBuffers::particleBuffer_");
	perViewBuffer_->SetName(L"GpuParticleBuffers::perViewBuffer_");
	emitterBuffer_->SetName(L"GpuParticleBuffers::emitterBuffer_");
	perFrameBuffer_->SetName(L"GpuParticleBuffers::perFrameBuffer_");
	freeListIndexBuffer_->SetName(L"GpuParticleBuffers::freeListIndexBuffer_");
	freeListBuffer_->SetName(L"GpuParticleBuffers::freeListBuffer_");
}

/// -------------------------------------------------------------
///			　　　			更新処理
/// -------------------------------------------------------------
void GpuParticleBuffers::Update(float deltaTime)
{
	perFrameData_->deltaTime = deltaTime; // 

	const Matrix4x4 viewMatrix = CameraManager::GetInstance()->GetActiveViewMatrix();
	const Matrix4x4 projectionMatrix = CameraManager::GetInstance()->GetActiveProjectionMatrix();
	const Matrix4x4 viewProjectionMatrix = Matrix4x4::Multiply(viewMatrix, projectionMatrix);

	// 現在の描画カメラの向きに合わせてBillboard用の回転行列を更新する
	Matrix4x4 billboardMatrix = Matrix4x4::Inverse(viewMatrix);
	billboardMatrix.m[3][0] = billboardMatrix.m[3][1] = billboardMatrix.m[3][2] = 0.0f;

	perViewData_->viewProjectionMatrix = viewProjectionMatrix;
	perViewData_->billboardMatrix = Matrix4x4::Transpose(Matrix4x4::Inverse(billboardMatrix));
	perViewData_->billboardMode = PackBillboardMode(
		GpuParticleKind::Sprite,
		static_cast<uint32_t>(BillboardMode::Camera)
	);

	// 時間計測用データの更新
	perFrameData_->time += perFrameData_->deltaTime;
}

GpuEmitterCBData* GpuParticleBuffers::GetEmitterCBData(uint32_t slot)
{
	const UINT stride = Align256(sizeof(GpuEmitterCBData));
	const uint32_t s = slot % kEmitterCBSlotCount;

	auto* base = reinterpret_cast<uint8_t*>(emitterCBData_);
	return reinterpret_cast<GpuEmitterCBData*>(base + stride * s);
}

D3D12_GPU_VIRTUAL_ADDRESS GpuParticleBuffers::GetEmitterCBAddress(uint32_t slot)
{
	const UINT stride = Align256(sizeof(GpuEmitterCBData));
	const uint32_t s = slot % kEmitterCBSlotCount;

	return emitterBuffer_->GetGPUVirtualAddress() + stride * s;
}

/// -------------------------------------------------------------
///			　	パーティクルバッファの生成
/// -------------------------------------------------------------
void GpuParticleBuffers::CreateParticleBuffer()
{
	// リソースを生成
	particleBuffer_ = ResourceManager::CreateBufferResource(
		DirectXCommon::GetInstance()->GetDevice(), sizeof(ParticleCS) * kMaxParticles,
		D3D12_HEAP_TYPE_DEFAULT,					// デフォルトヒープ
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,	// UAVとして使用するためのフラグ
		D3D12_RESOURCE_STATE_COMMON					// UAV用は COMMON ステートで作成
	);

	// SRVの生成
	particleSrvIndex_ = SRVManager::GetInstance()->Allocate();
	SRVManager::GetInstance()->CreateSRVForStructureBuffer(particleSrvIndex_, particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));

	// UAVの生成
	particleUavIndex_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForStructuredBuffer(particleUavIndex_, particleBuffer_.Get(), kMaxParticles, sizeof(ParticleCS));
}

/// -------------------------------------------------------------
///		　		ビュー行列と射影行列バッファの生成
/// -------------------------------------------------------------
void GpuParticleBuffers::CreatePerViewBuffer()
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();
	UINT size = (sizeof(PerView) + 255) & ~255u; // 256byteアライン

	perViewBuffer_ = ResourceManager::CreateBufferResource(device, size);

	perViewBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));
	perViewData_->viewProjectionMatrix = Matrix4x4::MakeIdentity();
	perViewData_->billboardMatrix = Matrix4x4::MakeIdentity();
	perViewData_->billboardMode = PackBillboardMode(
		GpuParticleKind::Sprite,
		static_cast<uint32_t>(BillboardMode::Camera)
	);
}

/// -------------------------------------------------------------
///			　		エミッターバッファの生成
/// -------------------------------------------------------------
void GpuParticleBuffers::CreateEmitterBuffer()
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();

	const UINT stride = Align256(sizeof(GpuEmitterCBData));
	const UINT bufferSize = stride * kEmitterCBSlotCount;

	emitterBuffer_ = ResourceManager::CreateBufferResource(device, bufferSize);
	emitterBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&emitterCBData_));

	// 全スロット初期化（0クリア）
	std::memset(emitterCBData_, 0, bufferSize);

	// 必要なら 0番スロットにデフォ値だけ入れておく（デバッグ用）
	auto* cb0 = GetEmitterCBData(0);
	cb0->count = 10;
	cb0->frequency = 0.5f;
	cb0->frequencyTime = 0.0f;
	cb0->translate = { 0.0f, 2.0f, 0.0f };
	cb0->radius = 1.0f;
	cb0->emit = 1;
	cb0->type = static_cast<uint32_t>(GpuParticleType::Default);
	cb0->billboardMode = PackBillboardMode(
		GpuParticleKind::Sprite,
		static_cast<uint32_t>(BillboardMode::Camera)
	);
}

/// -------------------------------------------------------------
///			　		時間計測用バッファの生成
/// -------------------------------------------------------------
void GpuParticleBuffers::CreatePerFrameBuffer()
{
	perFrameBuffer_ = ResourceManager::CreateBufferResource(DirectXCommon::GetInstance()->GetDevice(), sizeof(PerFrame));
	perFrameBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&perFrameData_));

	// 初期化
	perFrameData_->time = 0.0f;
	perFrameData_->deltaTime = 1.0f / 60.0f; // 仮に60FPS固定で更新
}

/// -------------------------------------------------------------
///			　		フリーカウンターバッファの生成
/// -------------------------------------------------------------
void GpuParticleBuffers::CreateFreeListIndexBuffer()
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();

	// リソースを生成
	freeListIndexBuffer_ = ResourceManager::CreateBufferResource(
		device,
		sizeof(int32_t),
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON
	);

	// UAVの生成
	freeListIndexUavIndex_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForStructuredBuffer(freeListIndexUavIndex_, freeListIndexBuffer_.Get(), 1, sizeof(int32_t));
}

/// -------------------------------------------------------------
///			　		フリーリストバッファの生成
/// -------------------------------------------------------------
void GpuParticleBuffers::CreateFreeListBuffer()
{
	auto* device = DirectXCommon::GetInstance()->GetDevice();

	// リソースを生成
	freeListBuffer_ = ResourceManager::CreateBufferResource(
		device,
		sizeof(int32_t) * kMaxParticles,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON
	);

	// UAVの生成
	freeListUavIndex_ = UAVManager::GetInstance()->Allocate();
	UAVManager::GetInstance()->CreateUAVForStructuredBuffer(freeListUavIndex_, freeListBuffer_.Get(), kMaxParticles, sizeof(int32_t));
}

} // namespace Ken4lowEngine
