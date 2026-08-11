#include "Wireframe.h"
#include "DirectXCommon.h"
#include "DebugCamera.h"
#include "GameViewportConstants.h"

#include <cstring>

namespace Ken4lowEngine
{

	// ---------- 基本制御 ----------
	// Wireframe のライフサイクルとデバッグ描画の状態を扱う。

	Wireframe* Wireframe::GetInstance()
	{
		static Wireframe instance;
		return &instance;
	}

	void Wireframe::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;
		isDebugCamera_ = false;
		projectionMatrix_ = Matrix4x4::MakeOrthographicMatrix(0.0f, 0.0f, static_cast<float>(GameViewportConstants::Width), static_cast<float>(GameViewportConstants::Height), 0.0f, 1.0f);
		viewProjectionMatrix_ = Matrix4x4::MakeViewportMatrix(0.0f, 0.0f, static_cast<float>(GameViewportConstants::Width), static_cast<float>(GameViewportConstants::Height), 0.0f, 1.0f); // Debug wireframeも固定GameViewport座標で重ねる。
		CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, triangleRootSignature_, trianglePipelineState_);
		CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE, lineRootSignature_, linePipelineState_);
		CreateInstancedWirePSO();
		CreateTransformationMatrix();
		triangleData_ = std::make_unique<WireframeTriangleData>();
		CreateTriangleVertexData(triangleData_.get());
		boxData_ = std::make_unique<WireframeBoxData>();
		CreateBoxVertexData(boxData_.get());
		lineData_ = std::make_unique<WireframeLineData>();
		CreateLineVertexData(lineData_.get());
		boxWireInstancedData_ = std::make_unique<WireframeBoxInstancedData>();
		CreateBoxWireInstancedData(boxWireInstancedData_.get());
		sphereInstancedData_ = std::make_unique<WireframeSphereInstancedData>();
		CreateSphereInstancedData(sphereInstancedData_.get());
		capsuleInstancedData_ = std::make_unique<WireframeCapsuleInstancedData>();
		CreateCapsuleInstancedData(capsuleInstancedData_.get());
		trianglePipelineState_->SetName(L"Wireframe_Triangle_PSO");
		triangleRootSignature_->SetName(L"Wireframe_Triangle_RootSignature");
		linePipelineState_->SetName(L"Wireframe_Line_PSO");
		lineRootSignature_->SetName(L"Wireframe_Line_RootSignature");
		instancedWirePipelineState_->SetName(L"Wireframe_InstancedShape_PSO");
		instancedWireRootSignature_->SetName(L"Wireframe_InstancedShape_RootSignature");
	}

	void Wireframe::Finalize()
	{
		if (!dxCommon_)
		{
			return;
		}

		if (transformationMatrixBuffer_)
		{
			transformationMatrixBuffer_->Unmap(0, nullptr);
		}
		transformationMatrixData_ = nullptr;
		transformationMatrixBuffer_.Reset();

		auto unmapAndReset = [](auto& resource, auto*& mappedPtr)
			{
				if (resource)
				{
					resource->Unmap(0, nullptr);
				}
				mappedPtr = nullptr;
				resource.Reset();
			};

		if (triangleData_)
		{
			unmapAndReset(triangleData_->vertexBuffer, triangleData_->vertexData);
			triangleData_.reset();
		}
		if (boxData_)
		{
			unmapAndReset(boxData_->vertexBuffer, boxData_->vertexData);
			unmapAndReset(boxData_->indexBuffer, boxData_->indexData);
			boxData_.reset();
		}
		if (lineData_)
		{
			unmapAndReset(lineData_->vertexBuffer, lineData_->vertexData);
			lineData_.reset();
		}
		if (boxWireInstancedData_)
		{
			unmapAndReset(boxWireInstancedData_->baseVertexBuffer, boxWireInstancedData_->baseVertexData);
			unmapAndReset(boxWireInstancedData_->indexBuffer, boxWireInstancedData_->indexData);
			unmapAndReset(boxWireInstancedData_->instanceBuffer, boxWireInstancedData_->instanceData);
			boxWireInstancedData_.reset();
		}
		if (sphereInstancedData_)
		{
			unmapAndReset(sphereInstancedData_->baseVertexBuffer, sphereInstancedData_->baseVertexData);
			unmapAndReset(sphereInstancedData_->indexBuffer, sphereInstancedData_->indexData);
			unmapAndReset(sphereInstancedData_->instanceBuffer, sphereInstancedData_->instanceData);
			sphereInstancedData_.reset();
		}
		if (capsuleInstancedData_)
		{
			unmapAndReset(capsuleInstancedData_->baseVertexBuffer, capsuleInstancedData_->baseVertexData);
			unmapAndReset(capsuleInstancedData_->indexBuffer, capsuleInstancedData_->indexData);
			unmapAndReset(capsuleInstancedData_->instanceBuffer, capsuleInstancedData_->instanceData);
			capsuleInstancedData_.reset();
		}

		trianglePipelineState_.Reset();
		linePipelineState_.Reset();
		instancedWirePipelineState_.Reset();
		triangleRootSignature_.Reset();
		lineRootSignature_.Reset();
		instancedWireRootSignature_.Reset();
		camera_ = nullptr;
		dxCommon_ = nullptr;
		Reset();
	}

	void Wireframe::Update()
	{
		if (!transformationMatrixData_ || !camera_)
		{
			return;
		}

		if (!isDebugCamera_)
		{
			transformationMatrixData_->WVP = camera_->GetViewMatrix() * camera_->GetProjectionMatrix();
		}
		else
		{
			transformationMatrixData_->WVP = DebugCamera::GetInstance()->GetViewProjectionMatrix();
		}
	}

	void Wireframe::Draw()
	{
		if (!debugDrawEnabled_ || !dxCommon_ || !transformationMatrixData_)
		{
			Reset();
			return;
		}

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		FrameUploadArena& uploadArena = dxCommon_->GetFrameUploadArena();
		const FrameUploadArena::Allocation transformAllocation = uploadArena.AllocateConstant(*transformationMatrixData_);
		if (!transformAllocation.IsValid())
		{
			Reset();
			return;
		}

		auto uploadDynamicData = [&uploadArena](const void* source, std::size_t sizeBytes, std::size_t alignment)
			{
				FrameUploadArena::Allocation allocation = uploadArena.Allocate(sizeBytes, alignment);
				if (allocation.IsValid() && source && sizeBytes > 0)
				{
					std::memcpy(allocation.cpuAddress, source, sizeBytes);
				}
				return allocation;
			};

		// CPU stagingは従来のMap領域へ蓄積し、GPUにはFence安全な現在Frame専用コピーだけを渡す。
		if (lineIndex_ >= kWireframeLineVertexCount && lineData_ && lineData_->vertexData)
		{
			const std::size_t bytes = static_cast<std::size_t>(lineIndex_) * sizeof(WireframeVertexData);
			const FrameUploadArena::Allocation allocation = uploadDynamicData(lineData_->vertexData, bytes, alignof(WireframeVertexData));
			if (allocation.IsValid())
			{
				D3D12_VERTEX_BUFFER_VIEW view{};
				view.BufferLocation = allocation.gpuAddress;
				view.SizeInBytes = static_cast<UINT>(bytes);
				view.StrideInBytes = sizeof(WireframeVertexData);
				commandList->SetGraphicsRootSignature(lineRootSignature_.Get());
				commandList->SetPipelineState(linePipelineState_.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
				commandList->IASetVertexBuffers(0, 1, &view);
				commandList->SetGraphicsRootConstantBufferView(0, transformAllocation.gpuAddress);
				commandList->DrawInstanced(lineIndex_, 1, 0, 0);
			}
		}

		if (triangleIndex_ >= kWireframeTriangleVertexCount && triangleData_ && triangleData_->vertexData)
		{
			const std::size_t bytes = static_cast<std::size_t>(triangleIndex_) * sizeof(WireframeVertexData);
			const FrameUploadArena::Allocation allocation = uploadDynamicData(triangleData_->vertexData, bytes, alignof(WireframeVertexData));
			if (allocation.IsValid())
			{
				D3D12_VERTEX_BUFFER_VIEW view{};
				view.BufferLocation = allocation.gpuAddress;
				view.SizeInBytes = static_cast<UINT>(bytes);
				view.StrideInBytes = sizeof(WireframeVertexData);
				commandList->SetGraphicsRootSignature(triangleRootSignature_.Get());
				commandList->SetPipelineState(trianglePipelineState_.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				commandList->IASetVertexBuffers(0, 1, &view);
				commandList->SetGraphicsRootConstantBufferView(0, transformAllocation.gpuAddress);
				commandList->DrawInstanced(triangleIndex_, 1, 0, 0);
			}
		}

		if (boxIndex_ >= kWireframeBoxIndexCount && boxVertexIndex_ >= kWireframeBoxVertexCount && boxData_ && boxData_->vertexData && boxData_->indexData)
		{
			const std::size_t vertexBytes = static_cast<std::size_t>(boxVertexIndex_) * sizeof(WireframeVertexData);
			const std::size_t indexBytes = static_cast<std::size_t>(boxIndex_) * sizeof(uint32_t);
			const FrameUploadArena::Allocation vertexAllocation = uploadDynamicData(boxData_->vertexData, vertexBytes, alignof(WireframeVertexData));
			const FrameUploadArena::Allocation indexAllocation = uploadDynamicData(boxData_->indexData, indexBytes, alignof(uint32_t));
			if (vertexAllocation.IsValid() && indexAllocation.IsValid())
			{
				D3D12_VERTEX_BUFFER_VIEW vertexView{};
				vertexView.BufferLocation = vertexAllocation.gpuAddress;
				vertexView.SizeInBytes = static_cast<UINT>(vertexBytes);
				vertexView.StrideInBytes = sizeof(WireframeVertexData);
				D3D12_INDEX_BUFFER_VIEW indexView{};
				indexView.BufferLocation = indexAllocation.gpuAddress;
				indexView.SizeInBytes = static_cast<UINT>(indexBytes);
				indexView.Format = DXGI_FORMAT_R32_UINT;
				commandList->SetGraphicsRootSignature(triangleRootSignature_.Get());
				commandList->SetPipelineState(trianglePipelineState_.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				commandList->IASetVertexBuffers(0, 1, &vertexView);
				commandList->IASetIndexBuffer(&indexView);
				commandList->SetGraphicsRootConstantBufferView(0, transformAllocation.gpuAddress);
				commandList->DrawIndexedInstanced(boxIndex_, 1, 0, 0, 0);
			}
		}

		if (boxWireInstanceCount_ > 0 && boxWireInstancedData_ && boxWireInstancedData_->instanceData)
		{
			const std::size_t bytes = static_cast<std::size_t>(boxWireInstanceCount_) * sizeof(WireframeBoxInstanceData);
			const FrameUploadArena::Allocation allocation = uploadDynamicData(boxWireInstancedData_->instanceData, bytes, alignof(WireframeBoxInstanceData));
			if (allocation.IsValid())
			{
				D3D12_VERTEX_BUFFER_VIEW instanceView{};
				instanceView.BufferLocation = allocation.gpuAddress;
				instanceView.SizeInBytes = static_cast<UINT>(bytes);
				instanceView.StrideInBytes = sizeof(WireframeBoxInstanceData);
				const D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = { boxWireInstancedData_->baseVertexBufferView, instanceView };
				commandList->SetGraphicsRootSignature(instancedWireRootSignature_.Get());
				commandList->SetPipelineState(instancedWirePipelineState_.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
				commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
				commandList->IASetIndexBuffer(&boxWireInstancedData_->indexBufferView);
				commandList->SetGraphicsRootConstantBufferView(0, transformAllocation.gpuAddress);
				commandList->DrawIndexedInstanced(kWireframeBoxWireIndexCount, boxWireInstanceCount_, 0, 0, 0);
			}
		}

		if (sphereInstanceCount_ > 0 && sphereInstancedData_ && sphereInstancedData_->instanceData)
		{
			const std::size_t bytes = static_cast<std::size_t>(sphereInstanceCount_) * sizeof(WireframeSphereInstanceData);
			const FrameUploadArena::Allocation allocation = uploadDynamicData(sphereInstancedData_->instanceData, bytes, alignof(WireframeSphereInstanceData));
			if (allocation.IsValid())
			{
				D3D12_VERTEX_BUFFER_VIEW instanceView{};
				instanceView.BufferLocation = allocation.gpuAddress;
				instanceView.SizeInBytes = static_cast<UINT>(bytes);
				instanceView.StrideInBytes = sizeof(WireframeSphereInstanceData);
				const D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = { sphereInstancedData_->baseVertexBufferView, instanceView };
				commandList->SetGraphicsRootSignature(instancedWireRootSignature_.Get());
				commandList->SetPipelineState(instancedWirePipelineState_.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
				commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
				commandList->IASetIndexBuffer(&sphereInstancedData_->indexBufferView);
				commandList->SetGraphicsRootConstantBufferView(0, transformAllocation.gpuAddress);
				commandList->DrawIndexedInstanced(kWireframeSphereIndexCount, sphereInstanceCount_, 0, 0, 0);
			}
		}

		if (capsuleInstanceCount_ > 0 && capsuleInstancedData_ && capsuleInstancedData_->instanceData)
		{
			const std::size_t bytes = static_cast<std::size_t>(capsuleInstanceCount_) * sizeof(WireframeCapsuleInstanceData);
			const FrameUploadArena::Allocation allocation = uploadDynamicData(capsuleInstancedData_->instanceData, bytes, alignof(WireframeCapsuleInstanceData));
			if (allocation.IsValid())
			{
				D3D12_VERTEX_BUFFER_VIEW instanceView{};
				instanceView.BufferLocation = allocation.gpuAddress;
				instanceView.SizeInBytes = static_cast<UINT>(bytes);
				instanceView.StrideInBytes = sizeof(WireframeCapsuleInstanceData);
				const D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = { capsuleInstancedData_->baseVertexBufferView, instanceView };
				commandList->SetGraphicsRootSignature(instancedWireRootSignature_.Get());
				commandList->SetPipelineState(instancedWirePipelineState_.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
				commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
				commandList->IASetIndexBuffer(&capsuleInstancedData_->indexBufferView);
				commandList->SetGraphicsRootConstantBufferView(0, transformAllocation.gpuAddress);
				commandList->DrawIndexedInstanced(kWireframeCapsuleIndexCount, capsuleInstanceCount_, 0, 0, 0);
			}
		}

		Reset();
	}

	void Wireframe::Reset()
	{
		triangleIndex_ = 0;
		boxVertexIndex_ = 0;
		boxIndex_ = 0;
		lineIndex_ = 0;
		boxWireInstanceCount_ = 0;
		sphereInstanceCount_ = 0;
		capsuleInstanceCount_ = 0;
	}

	void Wireframe::AddBoxWireInstance(const Matrix4x4& world, const Vector4& color)
	{
		if (!boxWireInstancedData_ || !boxWireInstancedData_->instanceData || boxWireInstanceCount_ >= kWireframeBoxWireMaxInstanceCount)
		{
			return;
		}

		WireframeBoxInstanceData& instance = boxWireInstancedData_->instanceData[boxWireInstanceCount_++];
		instance.world = world;
		instance.color = color;
	}

	void Wireframe::AddCapsuleWireInstance(const Matrix4x4& world, const Vector4& color)
	{
		if (!capsuleInstancedData_ || !capsuleInstancedData_->instanceData || capsuleInstanceCount_ >= kWireframeCapsuleMaxInstanceCount)
		{
			return;
		}

		WireframeCapsuleInstanceData& instance = capsuleInstancedData_->instanceData[capsuleInstanceCount_++];
		instance.world = world;
		instance.color = color;
	}

	void Wireframe::AddSphereWireInstance(const Matrix4x4& world, const Vector4& color)
	{
		if (!sphereInstancedData_ || !sphereInstancedData_->instanceData || sphereInstanceCount_ >= kWireframeSphereMaxInstanceCount)
		{
			return;
		}

		WireframeSphereInstanceData& instance = sphereInstancedData_->instanceData[sphereInstanceCount_++];
		instance.world = world;
		instance.color = color;
	}

	void Wireframe::SetDebugDrawEnabled(bool enabled)
	{
#ifdef _DEBUG
		debugDrawEnabled_ = enabled;
#else
		(void)enabled;
		debugDrawEnabled_ = false;
#endif
	}

} // namespace Ken4lowEngine
