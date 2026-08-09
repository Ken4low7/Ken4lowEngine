#include "ParticleManager.h"
#include <ResourceManager.h>
#include <DirectXCommon.h>
#include <LogString.h>
#include <SRVManager.h>
#include <TextureManager.h>
#include "BlendStateFactory.h"
#include "ShaderCompiler.h"
#include "Camera.h"
#include <ImGuiManager.h>
#include <DebugCamera.h>
#include "CollisionUtility.h"
#include "LinearInterpolation.h"
#include "ParticleShaderManifest.h"
#include <GameTimer.h>

namespace Ken4lowEngine
{


	/// -------------------------------------------------------------
	///				　　　 風が吹くエリアと風の強さ
	/// -------------------------------------------------------------
	std::vector<ParticleManager::WindZone> windZones = {
		{ { {-5.0f, -5.0f, -5.0f}, {5.0f, 5.0f, 5.0f} }, {0.1f, 0.0f, 0.0f} },
		{ { {10.0f, -5.0f, -5.0f}, {15.0f, 5.0f, 5.0f} }, {0.0f, 0.0f, 0.1f} }
	};

	/// -------------------------------------------------------------
	///				    シングルトンインスタンス
	/// -------------------------------------------------------------
	ParticleManager* ParticleManager::GetInstance()
	{
		static ParticleManager instance;
		return &instance;
	}


	/// -------------------------------------------------------------
	///				           初期化処理
	/// -------------------------------------------------------------
	void ParticleManager::Initialize(DirectXCommon* dxCommon, Camera* camera)
	{
		// 引数でDirectXCommonとSRVマネージャーのポインタを受け取ってメンバ変数に記録する
		dxCommon_ = dxCommon;
		camera_ = camera;

		srvManager_ = SRVManager::GetInstance();

		// ランダムエンジンの初期化
		randomEngin.seed(seedGeneral());

		accelerationField.acceleration = { 15.0f, 0.0f, 0.0f };
		accelerationField.area.min = { -10.0f, -10.0f, -30.0f };
		accelerationField.area.max = { 10.0f, 10.0f, 30.0f };

		// パイプライン生成
		CreatePSO();

		// マテリアルデータの初期化
		material_.Initialize();

		// メッシュデータの初期化
		meshMap_[ParticleEffectType::Default].Initialize();
		meshMap_[ParticleEffectType::Slash].InitializeRing();
		meshMap_[ParticleEffectType::Ring].InitializeRing();
		meshMap_[ParticleEffectType::Cylinder].InitializeCylinder();
		meshMap_[ParticleEffectType::Star].InitializeStar();
		meshMap_[ParticleEffectType::Smoke].InitializeSmoke();

		meshMap_[ParticleEffectType::Flash].Initialize();       // 四角メッシュで十分
		meshMap_[ParticleEffectType::Spark].Initialize();       // 同上

		meshMap_[ParticleEffectType::EnergyGather].Initialize();
		meshMap_[ParticleEffectType::Charge].Initialize();     // 同上
		meshMap_[ParticleEffectType::Explosion].Initialize(); // 同上

		meshMap_[ParticleEffectType::Blood].Initialize();     // 血飛沫用のメッシュ

		meshMap_[ParticleEffectType::LaserBeam].Initialize(); // レーザービーム用のメッシュ

		rootSignature->SetName(L"ParticleManager_RootSignature");
		graphicsPipelineState->SetName(L"ParticleManager_PSO");
	}


	/// -------------------------------------------------------------
	///				    パーティクルグループの生成
	/// -------------------------------------------------------------
	void ParticleManager::CreateParticleGroup(const std::string& name, const std::string& textureFilePath, ParticleEffectType effectType)
	{
		TextureManager::GetInstance()->LoadTexture(textureFilePath);

		// すでに存在していれば何もせずに戻る
		if (particleGroups.find(name) != particleGroups.end()) return;

		// 新たな空のパーティクルグループを作成し、コンテナに登録
		ParticleGroup group{};
		group.materialData.textureFilePath = textureFilePath;
		group.materialData.gpuHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath);

		// インスタンスバッファ作成
		group.instancebuffer = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ParticleForGPU) * kNumMaxInstance);
		group.instancebuffer->Map(0, nullptr, reinterpret_cast<void**>(&group.mappedData));
		group.instancebuffer->SetName(L"ParticleManager_InstanceBuffer");

		// パーティクルのエフェクトの種類を設定
		group.type = effectType;

		// 初期化
		for (uint32_t i = 0; i < kNumMaxInstance; ++i)
		{
			group.mappedData[i].WVP = Matrix4x4::MakeIdentity();
			group.mappedData[i].World = Matrix4x4::MakeIdentity();
		}

		// インスタンシング用SRVの生成
		group.srvIndex = srvManager_->Allocate();
		srvManager_->CreateSRVForStructureBuffer(group.srvIndex, group.instancebuffer.Get(), kNumMaxInstance, sizeof(ParticleForGPU));

		particleGroups.emplace(name, group);
	}


	/// -------------------------------------------------------------
	///				           　更新処理
	/// -------------------------------------------------------------
	void ParticleManager::Update()
	{
		// ビュー行列とプロジェクション行列をカメラから取得
		Matrix4x4 cameraMatrix = Matrix4x4::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, camera_->GetRotate(), camera_->GetTranslate());
		Matrix4x4 viewMatrix = Matrix4x4::Inverse(cameraMatrix);
		Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix();
		Matrix4x4 viewProjectionMatrix = Matrix4x4::Multiply(viewMatrix, projectionMatrix);

		// デバッグカメラが有効ならそちらを使う
		if (isDebugCamera_)
		{
#ifdef _DEBUG
			debugViewProjectionMatrix_ = DebugCamera::GetInstance()->GetViewProjectionMatrix();
			viewProjectionMatrix = debugViewProjectionMatrix_;
#endif
		}

		// ビルボード用行列（回転行列のみ）
		Matrix4x4 backToFrontMatrix = Matrix4x4::MakeIdentity(); // 必要ならY軸回転行列に置換
		Matrix4x4 billboardMatrix = Matrix4x4::Multiply(backToFrontMatrix, cameraMatrix);
		billboardMatrix.m[3][0] = billboardMatrix.m[3][1] = billboardMatrix.m[3][2] = 0.0f;

		float kDeltaTime = GameTimer::GetInstance()->GetDeltaTime();

		// パーティクルグループごとに更新処理
		for (auto& group : particleGroups)
		{
			group.second.numParticles = 0;

			auto& particles = group.second.particles;
			for (auto particleIt = particles.begin(); particleIt != particles.end(); )
			{
				auto& particle = *particleIt;

				// 寿命切れパーティクルを削除
				if (particle.currentTime >= particle.lifeTime)
				{
					particleIt = particles.erase(particleIt);
					continue;
				}

				// 更新対象としてカウント
				if (group.second.numParticles < kNumMaxInstance)
				{
					// 経過割合
					float t = particle.currentTime / particle.lifeTime;

					// スケール補間
					particle.transform.scale_ = Lerp(particle.startScale, particle.endScale, t);

					// 位置更新
					particle.transform.translate_ += particle.velocity * kDeltaTime;
					particle.currentTime += kDeltaTime;

					switch (group.second.type)
					{
						/*case ParticleEffectType::Ring:
							particle.transform.rotate_.z += 1.5f * kDeltaTime;
							break;*/

					case ParticleEffectType::Cylinder:
						particle.transform.rotate_.y += 1.5f * kDeltaTime;
						break;
					case ParticleEffectType::Charge: {
						if (particle.mode == ParticleMode::Orbit) {
							float t2 = (particle.currentTime * particle.orbitSpeed) + particle.orbitPhase;
							float r = particle.orbitRadius;

							Vector3 localPos = {
								std::sin(t2) * r,
								std::sin(t2 * 1.5f) * 0.5f,
								std::sin(2.0f * t2) * r * 0.5f
							};

							Matrix4x4 rotMat = Matrix4x4::MakeRotateAxisAngleMatrix(particle.orbitAxis, particle.orbitPhase);
							Vector3 rotatedPos = Vector3::Transform(localPos, rotMat);
							particle.transform.translate_ = particle.orbitCenter + rotatedPos;
						}
						else if (particle.mode == ParticleMode::Explode) {
							// 通常のvelocityによる移動処理
							particle.transform.translate_ += particle.velocity * kDeltaTime;
						}
						break;
					}
					case ParticleEffectType::Flash: {
						// 拡大して白く光って消える
						particle.transform.scale_ = Lerp(particle.startScale, particle.endScale, particle.currentTime / particle.lifeTime);
						break;
					}
					case ParticleEffectType::Ring: {
						float t3 = particle.currentTime / particle.lifeTime;

						particle.transform.scale_ = Lerp(particle.startScale, particle.endScale, t3);
						particle.color.w = 1.0f - t3;

						break;
					}
					case ParticleEffectType::Spark: {
						// 飛び散る破片
						particle.transform.translate_ += particle.velocity * kDeltaTime;
						break;
					}

					case ParticleEffectType::Explosion: {
						// 単純に速度で飛び散ってスケール縮小
						particle.transform.translate_ += particle.velocity * kDeltaTime;

						float timeRate = particle.currentTime / particle.lifeTime;
						particle.transform.scale_ = Lerp(particle.startScale, particle.endScale, timeRate);
						particle.color.w = 1.0f - timeRate;
						break;
					}

					default:
						break;
					}

					// 行列更新（transformに任せる）
					particle.transform.UpdateMatrix(viewProjectionMatrix, useBillboard, billboardMatrix);

					// 書き込み
					auto& instance = group.second.mappedData[group.second.numParticles];
					instance.WVP = particle.transform.GetWVPMatrix();
					instance.World = particle.transform.GetWorldMatrix();

					// 色とアルファ
					instance.color = particle.color;
					instance.color.w = 1.0f - t;

					// 風の影響（必要なら）
					if (isWind)
					{
						for (const auto& zone : windZones)
						{
							if (CollisionUtility::IsCollision(accelerationField.area, particle.transform.translate_))
							{
								particle.velocity += zone.strength;
							}
						}
					}

					++group.second.numParticles;
				}

				++particleIt;
			}
		}

		// マテリアル更新
		material_.Update();

		// --- emitQueue の実行 ---
		for (const auto& emitFunc : emitQueue)
		{
			emitFunc();
		}
		emitQueue.clear();
	}


	/// -------------------------------------------------------------
	///				           　描画処理
	/// -------------------------------------------------------------
	void ParticleManager::Draw()
	{
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();

		// ルートシグネチャを設定
		commandList->SetGraphicsRootSignature(rootSignature.Get());

		// パイプラインステートオブジェクト (PSO) を設定
		commandList->SetPipelineState(graphicsPipelineState.Get());

		//プリミティブトポロジを設定
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// すべてのパーティクルグループについて処理する
		for (auto& group : particleGroups)
		{
			if (group.second.numParticles == 0) continue;

			// マテリアルCBVを設定
			material_.SetPipeline();

			// テクスチャのSRVのデスクリプタテーブルを設定
			commandList->SetGraphicsRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(group.second.srvIndex));

			// インスタンシングデータのSRVのデスクリプタテーブルを設定
			commandList->SetGraphicsRootDescriptorTable(2, group.second.materialData.gpuHandle);

			auto meshIt = meshMap_.find(group.second.type);
			if (meshIt != meshMap_.end())
			{
				commandList->IASetVertexBuffers(0, 1, &meshIt->second.GetVertexBufferView());
				meshIt->second.Draw(group.second.numParticles);
			}
			else
			{
				Log("Uninitialized ParticleMesh for type: " + std::to_string((int)group.second.type));
			}

			// インスタンス数をリセット
			group.second.numParticles = 0;
		}
	}


	/// -------------------------------------------------------------
	///					　		解放処理
	/// -------------------------------------------------------------
	void ParticleManager::Finalize()
	{
		emitQueue.clear();

		for (auto& [key, group] : particleGroups)
		{
			group.instancebuffer.Reset();
			group.mappedData = nullptr;

			// SRVManager に解放APIがあるなら呼ぶ（なければ一旦OK）
			// srvManager_->Free(group.srvIndex);
		}
		particleGroups.clear();

		// マテリアル解放
		material_.Finalize();

		// ここが今回の“ド本命”
		meshMap_.clear();                 // ParticleMeshが持つVB/IBが解放される
		graphicsPipelineState.Reset();
		rootSignature.Reset();

		dxCommon_ = nullptr;
		srvManager_ = nullptr;
		camera_ = nullptr;
	}


	/// -------------------------------------------------------------
	///					　パーティクル射出処理
	/// -------------------------------------------------------------
	void ParticleManager::Emit(const std::string name, const Vector3 position, uint32_t count, ParticleEffectType type)
	{
		// パーティクルグループが存在するかどうか
		assert(particleGroups.find(name) != particleGroups.end() && "Particle Group is not found");

		// パーティクルグループを取得
		ParticleGroup& particleGroup = particleGroups[name];

		// 最大数に達している場合
		if (particleGroup.particles.size() >= count) return;

		// パーティクルの生成
		for (uint32_t index = 0; index < count; ++index)
		{
			// パーティクルの生成と追加
			particleGroup.particles.push_back(ParticleFactory::Create(randomEngin, position, type));
		}
	}

	void ParticleManager::EmitLaser(const std::string& name, const Vector3& position, float length, const Vector3& color)
	{
		assert(particleGroups.find(name) != particleGroups.end());

		ParticleGroup& group = particleGroups[name];
		if (group.particles.size() >= kNumMaxInstance) return;

		group.particles.push_back(ParticleFactory::CreateLaserBeam(position, length, color));
	}

	void ParticleManager::EmitLaserBeamFakeStretch(const std::string& name, const Vector3& startPos, const Vector3& direction, const Vector3& velocity, float totalLength, int count, const Vector4& color)
	{
		Vector3 dirNorm = direction;
		Vector3::Normalize(dirNorm);
		float step = totalLength / (count * 2.0f); // 実質2倍に密集

		emitQueue.push_back([=, this]() {
			auto& group = GetGroup(name);

			for (int i = 0; i < count; ++i)
			{
				Vector3 pos = startPos + dirNorm * (i * step);

				Particle p;
				p.transform.translate_ = pos;
				p.transform.scale_ = { 0.1f, 0.1f, 0.1f };
				p.startScale = p.transform.scale_;
				p.endScale = { 0.0f, 0.0f, 0.0f };
				p.transform.rotate_ = { 0.0f, 0.0f, 0.0f };
				p.color = color;
				p.lifeTime = 0.2f;
				p.velocity = velocity; // 弾と同じ方向に一定速度で移動させる（数値は速度）
				p.currentTime = 0.0f;

				group.particles.push_back(p);
			}
			});
	}


	/// -------------------------------------------------------------
	///					　パーティクルImGui描画
	/// -------------------------------------------------------------
	void ParticleManager::DrawImGui()
	{
		//// ImGuiでuseBillboardの切り替えボタンを追加
		//ImGui::Begin("Particle Manager"); // ウィンドウの開始
		//if (ImGui::Button(useBillboard ? "Disable Billboard" : "Enable Billboard"))
		//{
		//	// ボタンが押されたらuseBillboardの値を切り替える
		//	useBillboard = !useBillboard;
		//}

		//if (ImGui::Button(isWind ? "Disable Wind" : "Enable Wind"))
		//{
		//	isWind = !isWind;
		//}

		//ImGui::End(); // ウィンドウの終了
	}


	/// -------------------------------------------------------------
	///					　ルートシグネチャの生成処理
	/// -------------------------------------------------------------
	void ParticleManager::CreateRootSignature()
	{
		HRESULT hr{};

		//RootSignature作成
		D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
		descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		// Particle用のRootSignature
		D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
		descriptorRangeForInstancing[0].BaseShaderRegister = 0; // 0から始まる
		descriptorRangeForInstancing[0].NumDescriptors = 1;		// 数は1つ
		descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
		descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER rootParameters[3] = {};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;								// CBVを使う
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;								// PixelShaderを使う
		rootParameters[0].Descriptor.ShaderRegister = 0;												// レジスタ番号０とバインド

		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;					// DescriptorTableを使う
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;							// VertexShaderで使う
		rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;				// Tableの中身の配列を指定
		rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing); // Tableで利用する

		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;					// DescriptorTableを使う
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;								// PixelxShaderで使う
		rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;				// Tableの中身の配列を指定
		rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing); // Tableで利用する

		descriptionRootSignature.pParameters = rootParameters;											//ルートパラメータ配列へのポインタ
		descriptionRootSignature.NumParameters = _countof(rootParameters);								//配列の長さ

		D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
		staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;										//バイリニアフィルタ
		staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;									//0～1の範囲外をリピート
		staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;									//比較しない
		staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;													//ありったけのMipmapを使う
		staticSamplers[0].ShaderRegister = 0;															//レジスタ番号０を使う
		staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;								//PixelShaderで使う
		descriptionRootSignature.pStaticSamplers = staticSamplers;
		descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

		Microsoft::WRL::ComPtr <ID3DBlob> signatureBlob = nullptr;
		Microsoft::WRL::ComPtr <ID3DBlob> errorBlob = nullptr;
		hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
		if (FAILED(hr))
		{
			Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
			assert(false);
		}

		hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
		assert(SUCCEEDED(hr));
	}


	/// -------------------------------------------------------------
	///			　パイプラインシグネイチャーの生成処理
	/// -------------------------------------------------------------
	void ParticleManager::CreatePSO()
	{
		HRESULT hr{};

		// ルートシグネチャの生成
		CreateRootSignature();

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
		inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElementDescs[2] = { "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };


		D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
		inputLayoutDesc.pInputElementDescs = inputElementDescs;
		inputLayoutDesc.NumElements = _countof(inputElementDescs);

		//BlendStateの設定
		const D3D12_RENDER_TARGET_BLEND_DESC blendDesc = BlendStateFactory::GetInstance()->GetBlendDesc(blendMode_); // アルファブレンドを使用

		//RasterizerStateの設定
		D3D12_RASTERIZER_DESC rasterizerDesc{};
		//裏面（時計回り）を表示しない
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
		//三角形の中を塗りつぶす
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

		const ShaderDescriptor& vsDesc =
			ParticleShaderManifest::Get(ParticleShaderId::ParticleVS);
		const ShaderDescriptor& psDesc =
			ParticleShaderManifest::Get(ParticleShaderId::ParticlePS);

		assert(vsDesc.stage == ShaderStage::Vertex);
		assert(psDesc.stage == ShaderStage::Pixel);
		assert(vsDesc.rootSignature == RootSignatureType::Particle);
		assert(psDesc.rootSignature == RootSignatureType::Particle);

		Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(
			vsDesc,
			dxCommon_->GetDXCCompilerManager());
		assert(vertexShaderBlob != nullptr);

		Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = ShaderCompiler::CompileShader(
			psDesc,
			dxCommon_->GetDXCCompilerManager());
		assert(pixelShaderBlob != nullptr);

		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
		//Depthの機能を有効化する
		depthStencilDesc.DepthEnable = false;
		//書き込みします
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		//比較関数はLessEqual。つまり、近ければ描画される
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		//Depthを描くのをやめる
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};												// パイプラインステートディスクリプタの初期化
		graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();												// RootSgnature
		graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;													// InputLayout
		graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),vertexShaderBlob->GetBufferSize() };	// VertexDhader
		graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),pixelShaderBlob->GetBufferSize() };	// PixelShader
		graphicsPipelineStateDesc.BlendState.RenderTarget[0] = blendDesc;															// BlendState
		graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;													// RasterizeerState

		//レンダーターゲットの設定
		graphicsPipelineStateDesc.NumRenderTargets = 1;
		graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		//利用するトポロジー（形態）のタイプ。三角形
		graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;					// プリミティブトポロジーの設定

		// サンプルマスクとサンプル記述子の設定
		graphicsPipelineStateDesc.SampleDesc.Count = 1;
		graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

		// DepthStencilステートの設定
		graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
		graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		// パイプラインステートオブジェクトの生成
		hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
		assert(SUCCEEDED(hr));
	}

	std::list<Particle> ParticleManager::Emit(const Emitter& emitter, std::mt19937& randomEngine, ParticleEffectType type)
	{
		std::list<Particle> particles;
		for (uint32_t count = 0; count < emitter.count; ++count)
		{
			particles.push_back(ParticleFactory::Create(randomEngine, emitter.transform.translate_, type));
		}

		return particles;
	}

} // namespace Ken4lowEngine
