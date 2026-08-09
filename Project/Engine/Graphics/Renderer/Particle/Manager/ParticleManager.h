#pragma once
#include <DX12Include.h>
#include <ModelData.h>
#include <Emitter.h>
#include <ParticleMaterial.h>
#include <Particle.h>
#include <ParticleMesh.h>
#include "ParticleFactory.h"

#include <unordered_map>
#include <cstddef>
#include <list>
#include <random>
#include <numbers>
#include <functional>

#include <AABB.h>

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ----------///
class DirectXCommon;
class SRVManager;
class Camera;

/// -------------------------------------------------------------
///				パーティクルマネージャークラス
/// -------------------------------------------------------------
class ParticleManager
{
public: /// ---------- 構造体 ---------- ///

	/// ---------- 風のエフェクト ---------- ///
	struct WindZone
	{
		AABB area;		  // 風が吹くエリア
		Vector3 strength; // 風の強さ
	};

	// 加速度エフェクト
	struct AccelerationField
	{
		Vector3 acceleration; // !< 加速度
		AABB area;			  // !< 範囲
	};

	// GPUに送るためのパーティクル変換行列
	struct ParticleForGPU
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Vector4 color;
	};

	// 頂点データ
	struct VertexData
	{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct ParticleGroup
	{
		// マテリアルデータ(テクスチャファイルとテクスチャ用SRVインデックス)
		ParticleMaterial materialData;
		// パーティクルのリスト(std::list<Particle>型)
		uint32_t srvIndex = 0;
		// インスタンシングデータ用SRVインデックス
		ComPtr<ID3D12Resource> instancebuffer;
		// インスタンシングリソース
		ParticleForGPU* mappedData = nullptr;
		// インスタンス数
		uint32_t numParticles = 0;
		// インスタンシングデータを書き込むためのポインタ
		std::list<Particle> particles;
		// パーティクルの種別
		ParticleEffectType type = ParticleEffectType::Default;
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// ParticleManager のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>ParticleManager の唯一のインスタンス。</returns>
	static ParticleManager* GetInstance();

	/// <summary>
	/// パーティクルシステム全体の初期化処理。<br/>
	/// ・DirectXCommon / Camera の保持<br/>
	/// ・乱数エンジンの初期化<br/>
	/// ・ルートシグネチャ / PSO の生成<br/>
	/// ・マテリアルと各種パーティクルメッシュの初期化<br/>
	/// を行います。
	/// </summary>
	/// <param name="dxCommon">DirectX12 共通クラスへのポインタ。</param>
	/// <param name="camera">メインカメラへのポインタ。</param>
	void Initialize(DirectXCommon* dxCommon, Camera* camera);

	/// <summary>
	/// 新しいパーティクルグループを作成します。<br/>
	/// ・指定テクスチャの読み込み<br/>
	/// ・インスタンシング用バッファの作成とマップ<br/>
	/// ・インスタンシング SRV の生成<br/>
	/// を行い、name をキーとして登録します。<br/>
	/// すでに同名のグループが存在する場合は何もしません。
	/// </summary>
	/// <param name="name">パーティクルグループ名（後で Emit 時に使用）。</param>
	/// <param name="textureFilePath">使用するテクスチャファイルのパス。</param>
	/// <param name="effectType">このグループで使用するエフェクト種別。</param>
	void CreateParticleGroup(const std::string& name, const std::string& textureFilePath, ParticleEffectType effectType);

	/// <summary>
	/// 毎フレームの更新処理。<br/>
	/// 各グループのパーティクルに対して：<br/>
	/// ・寿命の更新と削除<br/>
	/// ・位置/スケール/色/回転などの更新<br/>
	/// ・ビルボード行列の計算<br/>
	/// ・インスタンシングバッファへの書き込み<br/>
	/// ・風 / 加速度フィールドの適用<br/>
	/// を行います。
	/// </summary>
	void Update();

	/// <summary>
	/// パーティクル描画処理。<br/>
	/// ・ルートシグネチャ / PSO のセット<br/>
	/// ・マテリアル CBV のセット<br/>
	/// ・テクスチャ SRV / インスタンス SRV のセット<br/>
	/// ・各エフェクトタイプごとのメッシュを使ってインスタンシング描画<br/>
	/// を行います。
	/// </summary>
	void Draw();

	/// <summary>
	/// パーティクル関連リソースの解放処理。<br/>
	/// 各パーティクルグループが保持しているインスタンスバッファなどを解放し、コンテナをクリアします。
	/// </summary>
	void Finalize();

	/// <summary>
	/// 指定グループからパーティクルを発生させます。<br/>
	/// パーティクル数が count を超える場合は、それ以上は生成しません。
	/// </summary>
	/// <param name="name">発生させるパーティクルグループ名。</param>
	/// <param name="position">発生位置。</param>
	/// <param name="count">生成する個数。</param>
	/// <param name="type">使用するエフェクト種別。</param>
	void Emit(const std::string name, const Vector3 position, uint32_t count, ParticleEffectType type);

	/// <summary>
	/// レーザービーム状のパーティクルを 1 本発生させます。<br/>
	/// position と length、色を指定してレーザー用パーティクルを生成します。
	/// </summary>
	void EmitLaser(const std::string& name, const Vector3& position, float length, const Vector3& color);

	/// <summary>
	/// 疑似的に伸びて見えるレーザービームを複数のパーティクルで構成して発生させます。<br/>
	/// startPos から direction 方向に totalLength 分だけ分割して配置し、<br/>
	/// 弾速と同じ velocity で移動させることで「伸びている」ように見せます。
	/// </summary>
	void EmitLaserBeamFakeStretch(const std::string& name,
		const Vector3& startPos,
		const Vector3& direction,
		const Vector3& velocity,
		float totalLength,
		int count,
		const Vector4& color);

	/// <summary>
	/// 登録済みパーティクルグループのコピーを取得します（デバッグ用）。<br/>
	/// ※ 値コピーなので、更新は反映されません。
	/// </summary>
	std::unordered_map<std::string, ParticleManager::ParticleGroup> GetParticleGroups() { return particleGroups; }

	/// Debug表示用：CPUパーティクルの現在数をコピーなしで集計する。
	size_t GetTotalParticleCount() const
	{
		size_t count = 0;
		for (const auto& [name, group] : particleGroups)
		{
			(void)name;
			count += group.particles.size();
		}
		return count;
	}

	/// Debug表示用：現在の実装では list 内の粒子が active 対象。
	size_t GetActiveParticleCount() const { return GetTotalParticleCount(); }

	/// <summary>
	/// パーティクルに関する ImGui デバッグ UI の描画処理。<br/>
	/// （現在はコメントアウトされており、必要に応じて有効化します）
	/// </summary>
	void DrawImGui();

	/// <summary>
	/// デバッグカメラを使用するかどうかを設定します。<br/>
	/// 有効な場合、パーティクルの ViewProjection 行列に DebugCamera を使用します。
	/// </summary>
	void SetDebugCamera(bool isDebugCamera) { isDebugCamera_ = isDebugCamera; }

	/// <summary>
	/// デバッグカメラ使用フラグを取得します。
	/// </summary>
	bool GetDebugCamera() { return isDebugCamera_; }

	/// <summary>
	/// ビルボード描画を有効にするかどうかを設定します。<br/>
	/// 有効な場合、カメラ方向に正面を向ける行列で描画されます。
	/// </summary>
	void SetBillboard(bool isBillboard) { useBillboard = isBillboard; }

	/// <summary>
	/// ビルボード描画が有効かどうかを取得します。
	/// </summary>
	bool GetBillboard() { return useBillboard; }

	/// <summary>
	/// 指定グループのパーティクルエフェクト種別を取得します。<br/>
	/// 見つからない場合は ParticleEffectType::Default を返します。
	/// </summary>
	ParticleEffectType GetGroupType(const std::string& name)
	{
		auto it = particleGroups.find(name);
		if (it != particleGroups.end())
		{
			return it->second.type;
		}
		return ParticleEffectType::Default;
	}

	/// <summary>
	/// 指定グループへの参照を取得します。<br/>
	/// 見つからない場合は std::runtime_error を送出します。
	/// </summary>
	ParticleGroup& GetGroup(const std::string& name)
	{
		auto it = particleGroups.find(name);
		if (it != particleGroups.end())
		{
			return it->second;
		}
		throw std::runtime_error("Particle group not found: " + name);
	}

private: /// ---------- ヘルパー関数 ---------- ///

	/// <summary>
	/// パーティクル用のルートシグネチャを生成します。<br/>
	/// ・マテリアル用 CBV<br/>
	/// ・インスタンス用 SRV（VS）<br/>
	/// ・テクスチャ用 SRV（PS）<br/>
	/// を持つ構成になっています。
	/// </summary>
	void CreateRootSignature();

	/// <summary>
	/// パーティクル描画用のグラフィックスパイプラインステートを生成します。<br/>
	/// ・頂点レイアウト<br/>
	/// ・ブレンドステート（BlendMode に応じたもの）<br/>
	/// ・ラスタライザ・深度ステート<br/>
	/// ・使用するシェーダ（Particle.VS/PS）<br/>
	/// をまとめて設定します。
	/// </summary>
	void CreatePSO();

	/// <summary>
	/// Emitter 設定に従ってパーティクルを生成するヘルパー関数。<br/>
	/// EmitQueue などから内部的に呼び出されます。
	/// </summary>
	std::list<Particle> Emit(const Emitter& emitter, std::mt19937& randomEngine, ParticleEffectType type);

private: /// ---------- メンバ変数 ---------- ///

	ParticleTransform transform;
	ParticleMaterial material_;

	std::unordered_map<ParticleEffectType, ParticleMesh> meshMap_;

	BlendMode blendMode_ = BlendMode::kBlendModeAdd;

	DirectXCommon* dxCommon_ = nullptr;
	SRVManager* srvManager_ = nullptr;
	Camera* camera_ = nullptr;

	// 合成行列
	Matrix4x4 worldViewProjectionMatrix;
	Matrix4x4 debugViewProjectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;

	ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;

	// モデルの読み込み
	ModelData modelData;

	// パーティクルグループコンテナ
	std::unordered_map<std::string, ParticleGroup> particleGroups;

	// ランダムエンジン
	std::random_device seedGeneral;
	std::mt19937 randomEngin;

	std::vector<std::function<void()>> emitQueue;

	// 描画数
	const uint32_t kNumMaxInstance = 1024;

	// ビルボード描画を使用するかどうか
	bool useBillboard = true;

	// 風エフェクト
	bool isWind = false;

	// デバッグカメラを使用するかどうか
	bool isDebugCamera_ = false;

	// Fieldを作る
	AccelerationField accelerationField;

private: /// ---------- コピー禁止 ---------- ///

	/// <summary>
	/// 外部からの生成を禁止するためのプライベートコンストラクタ。<br/>
	/// シングルトンパターンとして利用します。
	/// </summary>
	ParticleManager() = default;

	/// <summary>
	/// デフォルトデストラクタ。
	/// </summary>
	~ParticleManager() = default;

	/// <summary>
	/// コピーコンストラクタは使用禁止です。
	/// </summary>
	ParticleManager(const ParticleManager&) = delete;

	/// <summary>
	/// 代入演算子は使用禁止です。
	/// </summary>
	ParticleManager& operator=(const ParticleManager&) = delete;
};


} // namespace Ken4lowEngine
