#include "Wireframe.h"
#include "DirectXCommon.h"
#include "DebugCamera.h"
#include "GameViewportConstants.h"

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
		// パイプラインステートの生成
		// 三角形用のPSOを作成
		CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, triangleRootSignature_, trianglePipelineState_);
		// 線用のPSOを作成
		CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE, lineRootSignature_, linePipelineState_);
		// AABB / OBB / Sphereで共有するWireframeインスタンシング専用PSOを作成
		CreateInstancedWirePSO();
		// 座標変換行列データの生成
		CreateTransformationMatrix();
		// 三角形の頂点データを生成
		triangleData_ = std::make_unique<WireframeTriangleData>();
		CreateTriangleVertexData(triangleData_.get());
		// 矩形の頂点座標を生成
		boxData_ = std::make_unique<WireframeBoxData>();
		CreateBoxVertexData(boxData_.get());
		// 線の頂点を生成
		lineData_ = std::make_unique<WireframeLineData>();
		CreateLineVertexData(lineData_.get());
		// AABB / OBB用の共有単位キューブと共通インスタンスバッファを生成
		boxWireInstancedData_ = std::make_unique<WireframeBoxInstancedData>();
		CreateBoxWireInstancedData(boxWireInstancedData_.get());
		// Sphere用の共有3リングメッシュとインスタンスバッファを生成
		sphereInstancedData_ = std::make_unique<WireframeSphereInstancedData>();
		CreateSphereInstancedData(sphereInstancedData_.get());
		// Capsule用の共有ワイヤーメッシュとインスタンスバッファを生成
		capsuleInstancedData_ = std::make_unique<WireframeCapsuleInstancedData>();
		CreateCapsuleInstancedData(capsuleInstancedData_.get());
		// 名前の設定
		trianglePipelineState_->SetName(L"Wireframe_Triangle_PSO");
		triangleRootSignature_->SetName(L"Wireframe_Triangle_RootSignature");
		linePipelineState_->SetName(L"Wireframe_Line_PSO");
		lineRootSignature_->SetName(L"Wireframe_Line_RootSignature");
		instancedWirePipelineState_->SetName(L"Wireframe_InstancedShape_PSO");
		instancedWireRootSignature_->SetName(L"Wireframe_InstancedShape_RootSignature");
	}

	void Wireframe::Finalize()
	{
		// すでに解放済みなら何もしない
		if (!dxCommon_) { return; }
		// ---- Mapしているポインタを先に無効化 & Unmap ----
		if (transformationMatrixBuffer_) {
			transformationMatrixBuffer_->Unmap(0, nullptr);
		}
		transformationMatrixData_ = nullptr;
		transformationMatrixBuffer_.Reset();
		auto UnmapAndResetVB = [](auto& resource, auto*& mappedPtr) {
			if (resource) { resource->Unmap(0, nullptr); }
			mappedPtr = nullptr;
			resource.Reset();
			};
		if (triangleData_) {
			UnmapAndResetVB(triangleData_->vertexBuffer, triangleData_->vertexData);
			triangleData_.reset();
		}
		if (boxData_) {
			if (boxData_->vertexBuffer) boxData_->vertexBuffer->Unmap(0, nullptr);
			boxData_->vertexData = nullptr;
			boxData_->vertexBuffer.Reset();
			if (boxData_->indexBuffer) boxData_->indexBuffer->Unmap(0, nullptr);
			boxData_->indexData = nullptr;
			boxData_->indexBuffer.Reset();
			boxData_.reset();
		}
		if (lineData_) {
			UnmapAndResetVB(lineData_->vertexBuffer, lineData_->vertexData);
			lineData_.reset();
		}
		if (boxWireInstancedData_) {
			// AABB / OBB共有頂点・index・インスタンスの各Mapを解除してからGPUリソースを破棄する。
			UnmapAndResetVB(boxWireInstancedData_->baseVertexBuffer, boxWireInstancedData_->baseVertexData);
			UnmapAndResetVB(boxWireInstancedData_->indexBuffer, boxWireInstancedData_->indexData);
			UnmapAndResetVB(boxWireInstancedData_->instanceBuffer, boxWireInstancedData_->instanceData);
			boxWireInstancedData_.reset();
		}
		if (sphereInstancedData_) {
			// Sphere共有頂点・index・インスタンスの各Mapを解除してからGPUリソースを破棄する。
			UnmapAndResetVB(sphereInstancedData_->baseVertexBuffer, sphereInstancedData_->baseVertexData);
			UnmapAndResetVB(sphereInstancedData_->indexBuffer, sphereInstancedData_->indexData);
			UnmapAndResetVB(sphereInstancedData_->instanceBuffer, sphereInstancedData_->instanceData);
			sphereInstancedData_.reset();
		}
		if (capsuleInstancedData_) {
			// Capsule共有頂点・index・インスタンスの各Mapを解除してからGPUリソースを破棄する。
			UnmapAndResetVB(capsuleInstancedData_->baseVertexBuffer, capsuleInstancedData_->baseVertexData);
			UnmapAndResetVB(capsuleInstancedData_->indexBuffer, capsuleInstancedData_->indexData);
			UnmapAndResetVB(capsuleInstancedData_->instanceBuffer, capsuleInstancedData_->instanceData);
			capsuleInstancedData_.reset();
		}
		// ---- PSO / RootSignature を解放 ----
		trianglePipelineState_.Reset();
		linePipelineState_.Reset();
		instancedWirePipelineState_.Reset();
		triangleRootSignature_.Reset();
		lineRootSignature_.Reset();
		instancedWireRootSignature_.Reset();
		// ---- 参照だけ持ってるポインタは切る ----
		camera_ = nullptr;
		dxCommon_ = nullptr;
		Reset();
	}

	void Wireframe::Update()
	{
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
		if (!debugDrawEnabled_)
		{
			Reset();
			return;
		}
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
#pragma region ---------- 線の描画 ----------
		commandList->SetGraphicsRootSignature(lineRootSignature_.Get());										// ルートシグネチャの設定
		commandList->SetPipelineState(linePipelineState_.Get());												// パイプラインステートの設定
		commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);									// プリミティブトポロジーの設定
		commandList->IASetVertexBuffers(0, 1, &lineData_->vertexBufferView);									// 頂点バッファビューの設定
		commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress()); // 座標変換行列の設定
		if (lineIndex_ >= 2) commandList->DrawInstanced(lineIndex_, 1, 0, 0);							// 描画
#pragma endregion ---------------------------
#pragma region ---------- 三角形の描画 ----------
		commandList->SetGraphicsRootSignature(triangleRootSignature_.Get());									// ルートシグネチャの設定
		commandList->SetPipelineState(trianglePipelineState_.Get());											// パイプラインステートの設定
		commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);								// プリミティブトポロジーの設定
		commandList->IASetVertexBuffers(0, 1, &triangleData_->vertexBufferView);								// 頂点バッファビューの設定
		commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress()); // 座標変換行列の設定
		if (triangleIndex_ >= 3) commandList->DrawInstanced(triangleIndex_, 1, 0, 0);				// 描画
#pragma endregion -------------------------------
#pragma region ---------- 矩形の描画 ----------
		commandList->IASetVertexBuffers(0, 1, &boxData_->vertexBufferView);     // 頂点バッファビューの設定
		commandList->IASetIndexBuffer(&boxData_->indexBufferView);              // インデックスバッファビューの設定
		commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress()); // 座標変換行列の設定
		if (boxIndex_ >= 6) commandList->DrawIndexedInstanced(boxIndex_ * kWireframeBoxIndexCount, 1, 0, 0, 0);               // インデックスを使用して描画
#pragma endregion -----------------------------
#pragma region ---------- AABB / OBB共通インスタンシング描画 ----------
		if (boxWireInstanceCount_ > 0 && boxWireInstancedData_)
		{
			commandList->SetGraphicsRootSignature(instancedWireRootSignature_.Get());
			commandList->SetPipelineState(instancedWirePipelineState_.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			const D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
				boxWireInstancedData_->baseVertexBufferView,
				boxWireInstancedData_->instanceBufferView
			};
			commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
			commandList->IASetIndexBuffer(&boxWireInstancedData_->indexBufferView);
			commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress());
			// 共有する24 indexを、蓄積した全AABB / OBBについて1回のDrawCallで描画する。
			commandList->DrawIndexedInstanced(kWireframeBoxWireIndexCount, boxWireInstanceCount_, 0, 0, 0);
		}
#pragma endregion -------------------------------------------
#pragma region ---------- Sphereインスタンシング描画 ----------
		if (sphereInstanceCount_ > 0 && sphereInstancedData_)
		{
			commandList->SetGraphicsRootSignature(instancedWireRootSignature_.Get());
			commandList->SetPipelineState(instancedWirePipelineState_.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			const D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
				sphereInstancedData_->baseVertexBufferView,
				sphereInstancedData_->instanceBufferView
			};
			commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
			commandList->IASetIndexBuffer(&sphereInstancedData_->indexBufferView);
			commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress());
			// XY / XZ / YZの共有3リングを、蓄積した全Sphereについて1回のDrawCallで描画する。
			commandList->DrawIndexedInstanced(kWireframeSphereIndexCount, sphereInstanceCount_, 0, 0, 0);
		}
#pragma endregion -------------------------------------------
#pragma region ---------- Capsuleインスタンシング描画 ----------
		if (capsuleInstanceCount_ > 0 && capsuleInstancedData_)
		{
			commandList->SetGraphicsRootSignature(instancedWireRootSignature_.Get());
			commandList->SetPipelineState(instancedWirePipelineState_.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			const D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = {
				capsuleInstancedData_->baseVertexBufferView,
				capsuleInstancedData_->instanceBufferView
			};
			commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
			commandList->IASetIndexBuffer(&capsuleInstancedData_->indexBufferView);
			commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixBuffer_->GetGPUVirtualAddress());
			// 共有する単位Capsuleを、蓄積した全Capsuleについて1回のDrawCallで描画する。
			commandList->DrawIndexedInstanced(kWireframeCapsuleIndexCount, capsuleInstanceCount_, 0, 0, 0);
		}
#pragma endregion -------------------------------------------
		// リセット処理
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
		// AABB / OBBの単位キューブ線メッシュを共有し、World行列と色だけをインスタンスとして登録する。
		if (!boxWireInstancedData_ || !boxWireInstancedData_->instanceData ||
			boxWireInstanceCount_ >= kWireframeBoxWireMaxInstanceCount)
		{
			return;
		}

		WireframeBoxInstanceData& instance = boxWireInstancedData_->instanceData[boxWireInstanceCount_++];
		instance.world = world;
		instance.color = color;
	}

	void Wireframe::AddCapsuleWireInstance(const Matrix4x4& world, const Vector4& color)
	{
		// 共有単位Capsuleに対し、Capsuleごとの差分となるworld行列と色だけを登録する。
		if (!capsuleInstancedData_ || !capsuleInstancedData_->instanceData ||
			capsuleInstanceCount_ >= kWireframeCapsuleMaxInstanceCount)
		{
			return;
		}

		WireframeCapsuleInstanceData& instance = capsuleInstancedData_->instanceData[capsuleInstanceCount_++];
		instance.world = world;
		instance.color = color;
	}

	void Wireframe::AddSphereWireInstance(const Matrix4x4& world, const Vector4& color)
	{
		// 共有単位球リングに対し、Sphereごとの差分となるworld行列と色だけを登録する。
		if (!sphereInstancedData_ || !sphereInstancedData_->instanceData ||
			sphereInstanceCount_ >= kWireframeSphereMaxInstanceCount)
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
