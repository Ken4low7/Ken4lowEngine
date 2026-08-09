#pragma once
#include "DX12Include.h"
#include "WireframeConfig.h"
#include "WireframeTypes.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Camera.h"
#include "AABB.h"
#include "OBB.h"
#include "Capsule.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <Segment.h>
#include <Plane.h>
#include <Sphere.h>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;

	/// -------------------------------------------------------------
	///				　	線で形状を描画するクラス
	/// -------------------------------------------------------------
	class Wireframe
	{
	public: /// ---------- テンプレート ---------- ///

		// ComPtrのエイリアス
		template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	public: /// ---------- メンバ関数 ---------- ///

		//　シングルトン取得
		static Wireframe* GetInstance();

		// 初期化処理
		void Initialize(DirectXCommon* dxCommon);

		// 終了処理
		void Finalize();

		// 描画処理
		void Update();

		// 描画実行
		void Draw();

		// 描画リセット: 毎フレームの描画開始前に呼び出し、前フレームの描画内容をクリアする。
		void Reset();

		// デバッグ描画の有効/無効を切り替える。無効にすると描画コストが減る。
		void SetDebugDrawEnabled(bool enabled);

		// デバッグ描画が有効かどうかを返す。
		bool IsDebugDrawEnabled() const { return debugDrawEnabled_; }

		// デバッグ描画がサポートされているかどうかを返す。通常はデバッグビルドでのみ有効。
		static constexpr bool IsDebugDrawSupported()
		{
#ifdef _DEBUG
			return true;
#else
			return false;
#endif
		}

	public: /// ---------- 基本描画: 線・三角形・矩形など、低レベルの描画単位を蓄積する。 ---------- ///

		/// <summary>
		/// 線を描画する。
		/// </summary>
		/// <param name="start">始点</param>
		/// <param name="end">終点</param>
		/// <param name="color">色</param>
		void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color);

		/// <summary>
		/// 線分を描画する。
		/// </summary>
		/// <param name="segment">線分</param>
		/// <param name="color">色</param>
		void DrawSegment(const Segment& segment, const Vector4& color);

		/// <summary>
		/// すすきを描画する。
		/// </summary>
		/// <param name="corners">角の位置</param>
		/// <param name="color">色</param>
		void DrawFrustum(const std::array<Vector3, 8>& corners, const Vector4& color);

		/// <summary>
		/// 円を描画する。
		/// </summary>
		/// <param name="center">中心</param>
		/// <param name="radius">半径</param>
		/// <param name="segmentCount">分割数</param>
		/// <param name="color">色</param>
		void DrawCircle(const Vector3& center, float radius, uint32_t segmentCount, const Vector4& color);

		/// <summary>
		/// 三角形を描画する。
		/// </summary>
		/// <param name="position1">頂点1</param>
		/// <param name="position2">頂点2</param>
		/// <param name="position3">頂点3</param>
		/// <param name="color">色</param>
		void DrawTriangle(const Vector3& position1, const Vector3& position2, const Vector3& position3, const Vector4& color);

		/// <summary>
		/// 矩形を描画する。
		/// </summary>
		/// <param name="position">位置</param>
		/// <param name="size">サイズ</param>
		/// <param name="color">色</param>
		void DrawBox(const Vector3& position, const Vector3& size, const Vector4& color);

		/// <summary>
		/// 多角形を描画する。
		/// </summary>
		/// <param name="center">中心</param>
		/// <param name="radius">半径</param>
		/// <param name="sides">辺の数</param>
		/// <param name="color">色</param>
		void DrawPolygon(const Vector3& center, float radius, uint32_t sides, const Vector4& color);

		/// <summary>
		/// グリッドを描画する。
		/// </summary>
		/// <param name="size">サイズ</param>
		/// <param name="subdivision">分割数</param>
		/// <param name="color">色</param>
		void DrawGrid(const float size, const float subdivision, const Vector4& color);

	public: /// ---------- コリジョン/デバッグ形状: 当たり判定や空間確認用の形状をまとめる。 ---------- ///

		/// <summary>
		/// AABBを描画する。
		/// </summary>
		/// <param name="aabb">AABB</param>
		/// <param name="color">色</param>
		void DrawAABB(const AABB& aabb, const Vector4& color);

		/// <summary>
		/// OBBを描画する。
		/// </summary>
		/// <param name="obb">OBB</param>
		/// <param name="color">色</param>
		void DrawOBB(const OBB& obb, const Vector4& color);

		/// <summary>
		/// 球を描画する。
		/// </summary>
		/// <param name="center">中心</param>
		/// <param name="radius">半径</param>
		/// <param name="color">色</param>
		void DrawSphere(const Vector3& center, const float radius, const Vector4& color);

		/// <summary>
		/// 球を描画する。
		/// </summary>
		/// <param name="sphere">球</param>
		/// <param name="color">色</param>
		void DrawSphere(const Sphere& sphere, const Vector4& color);

		/// <summary>
		/// カプセルを描画する。
		/// </summary>
		/// <param name="center">中心</param>
		/// <param name="radius">半径</param>
		/// <param name="height">高さ</param>
		/// <param name="axis">軸</param>
		/// <param name="segments">分割数</param>
		/// <param name="color">色</param>
		void DrawCapsule(const Vector3& center, float radius, float height, const Vector3& axis, uint32_t segments, const Vector4& color);

		/// <summary>
		/// カプセルを描画する。
		/// </summary>
		/// <param name="capsule">カプセル</param>
		/// <param name="color">色</param>
		void DrawCapsule(const Capsule& capsule, const Vector4& color);

		/// <summary>
		/// 平面を描画する。
		/// </summary>
		/// <param name="plane">平面</param>
		/// <param name="size">サイズ</param>
		/// <param name="color">色</param>
		void DrawPlane(const Plane& plane, float size, const Vector4& color);

		/// <summary>
		/// カイ二ADERを描画する。
		/// </summary>
		/// <param name="baseCenter">底面の中心</param>
		/// <param name="radius">半径</param>
		/// <param name="height">高さ</param>
		/// <param name="axis">軸</param>
		/// <param name="segmentCount">分割数</param>
		/// <param name="color">色</param>
		void DrawCylinder(const Vector3& baseCenter, float radius, float height, const Vector3& axis, uint32_t segmentCount, const Vector4& color);

	public: /// ---------- 多面体/特殊形状: 後で WireframeDebugShapes などへ移しやすい形状群。 ---------- ///

		void DrawTetrahedron(const Vector3& baseCenter, float baseSize, float height, const Vector3& axis, const Vector4& color);
		void DrawPyramid(const Vector3& baseCenter, float baseSize, float height, const Vector3& axis, const Vector4& color);
		void DrawOctahedron(const Vector3& center, float size, const Vector4& color);
		void DrawDodecahedron(const Vector3& center, float size, const Vector4& color);
		void DrawIcosahedron(const Vector3& center, float size, const Vector4& color);
		void DrawTorus(const Vector3& center, float R, float r, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color);
		void DrawRotatingTorus(const Vector3& center, float R, float r, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color, float time);
		void DrawMobiusStrip(const Vector3& center, float R, float w, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color);
		void DrawLemniscate3D(const Vector3& center, float a, float b, float c, uint32_t segments, const Vector4& color);
		void DrawPentagonalPrism(const Vector3& center, float radius, float height, const Vector4& color);
		void DrawPentagonalPyramid(const Vector3& center, float radius, float height, const Vector4& color);

	public: /// ---------- 演出形状: 魔法陣やハートなど、ゲーム演出寄りのワイヤー形状。 ---------- ///

		void DrawPentagram(const Vector3& center, float radius, const Vector4& color);
		void DrawHexagram(const Vector3& center, float radius, const Vector4& color);
		void DrawMagicCircle(const Vector3& center, float radius, const Vector4& color);
		void DrawAdvancedMagicCircle(const Vector3& center, float radius, const Vector4& color);
		void DrawRotatingPentagram(const Vector3& center, float radius, const Vector4& color, float time);
		void DrawExpandingMagicCircle(const Vector3& center, float baseRadius, const Vector4& color, float time);
		void DrawFadingMagicCircle(const Vector3& center, float radius, float time);
		void DrawAnimatedMagicCircle(const Vector3& center, float radius, float time);
		void DrawProgressiveMagicCircle(const Vector3& center, float radius, const Vector4& baseColor, float time);
		void DrawPentagramProgressive(const Vector3& center, float radius, const Vector4& color, float progress);
		void DrawHexagramProgressive(const Vector3& center, float radius, const Vector4& color, float progress);
		void DrawAnimatedHeart(const Vector3& center, float size, float time);
		void DrawGlowingHeart(const Vector3& center, float size, float time);
		void DrawPoppingHeart(const Vector3& center, float size, float time);
		void DrawFloatingHeart(const Vector3& basePosition, float size, float time);
		void DrawMagicPentagram(const Vector3& center, float radius, const Vector4& color);
		void DrawConcentricCircles(const Vector3& center, float radius, uint32_t count, float spacing, const Vector4& color);
		void DrawRotatingMagicCircle(const Vector3& center, float radius, const Vector4& color);

	public: /// ---------- 設定/取得: 呼び出し側が描画環境を差し替えるための最小限のアクセサ。 ---------- ///

		void SetCamera(Camera* camera) { camera_ = camera; }
		void SetProjectionMatrix(const Matrix4x4& projectionMatrix) { projectionMatrix_ = projectionMatrix; }
		void SetDebugCamera(bool isDebugCamera) { isDebugCamera_ = isDebugCamera; }
		const Matrix4x4& GetProjectionMatrix() { return projectionMatrix_; }
		bool GetDebugCamera() const { return isDebugCamera_; }

		// 現在フレームに蓄積されたAABB / OBB共通のBox Wireインスタンス数を返す。
		uint32_t GetBoxWireInstanceCount() const { return boxWireInstanceCount_; }

		// AABB / OBB共通のBox Wireインスタンスバッファへ蓄積できる最大数を返す。
		static constexpr uint32_t GetBoxWireMaxInstanceCount() { return kWireframeBoxWireMaxInstanceCount; }

		// 既存のデバッグ表示との互換用。現在はAABBとOBBを合算した数を返す。
		uint32_t GetAABBInstanceCount() const { return GetBoxWireInstanceCount(); }
		static constexpr uint32_t GetAABBMaxInstanceCount() { return GetBoxWireMaxInstanceCount(); }

		// 現在フレームに蓄積されたSphereインスタンス数を返す。
		uint32_t GetSphereInstanceCount() const { return sphereInstanceCount_; }
		static constexpr uint32_t GetSphereMaxInstanceCount() { return kWireframeSphereMaxInstanceCount; }

		// 現在フレームに蓄積されたCapsuleインスタンス数を返す。
		uint32_t GetCapsuleInstanceCount() const { return capsuleInstanceCount_; }
		static constexpr uint32_t GetCapsuleMaxInstanceCount() { return kWireframeCapsuleMaxInstanceCount; }

	private: /// ---------- メンバ関数 ---------- ///

		// Pipeline生成: DirectX の描画パイプライン構築だけを担当する内部処理。
		void CreateRootSignature(ComPtr<ID3D12RootSignature>& rootSignature);
		void CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType,
			ComPtr<ID3D12RootSignature>& rootSignature,
			ComPtr<ID3D12PipelineState>& pipelineState);
		// local position + world + colorを使うWireframe形状共通のインスタンシングPSOを生成する。
		void CreateInstancedWirePSO();

		// Buffer生成: Wireframe が蓄積する頂点・インデックス・定数バッファを作る内部処理。
		void CreateTriangleVertexData(WireframeTriangleData* triangleData);
		void CreateBoxVertexData(WireframeBoxData* boxData);
		void CreateLineVertexData(WireframeLineData* lineData);
		// 単位キューブ共有メッシュとAABB / OBB共通インスタンスバッファを生成する。
		void CreateBoxWireInstancedData(WireframeBoxInstancedData* boxWireData);
		// Sphereの基本リングメッシュを1回だけ作り、各Sphereはインスタンスデータで描画する。
		void CreateSphereInstancedData(WireframeSphereInstancedData* sphereData);
		// Capsuleの基本ワイヤーメッシュを1回だけ作り、各Capsuleはインスタンスデータで描画する。
		void CreateCapsuleInstancedData(WireframeCapsuleInstancedData* capsuleData);
		void CreateTransformationMatrix();

		// AABB / OBBの単位キューブ線メッシュを共有し、World行列と色だけをインスタンスとして登録する。
		void AddBoxWireInstance(const Matrix4x4& world, const Vector4& color);
		// Sphereごとの差分となるworld行列と色だけをインスタンスとして登録する。
		void AddSphereWireInstance(const Matrix4x4& world, const Vector4& color);
		// Capsuleごとの差分となるworld行列と色だけをインスタンスとして登録する。
		void AddCapsuleWireInstance(const Matrix4x4& world, const Vector4& color);

	private: /// ---------- メンバ変数 ---------- ///

		// DirectXCommon
		DirectXCommon* dxCommon_ = nullptr;
		// カメラ
		Camera* camera_ = nullptr;

		BlendMode blendMode_ = BlendMode::kBlendModeNormal;

		// ルートシグネチャ
		ComPtr<ID3D12RootSignature> triangleRootSignature_;
		ComPtr<ID3D12RootSignature> lineRootSignature_;
		ComPtr<ID3D12RootSignature> instancedWireRootSignature_;

		// パイプラインステート
		ComPtr<ID3D12PipelineState> trianglePipelineState_;
		ComPtr<ID3D12PipelineState> linePipelineState_;
		ComPtr<ID3D12PipelineState> instancedWirePipelineState_;

		// 座標変換行列バッファ
		ComPtr<ID3D12Resource> transformationMatrixBuffer_;

		// 座標変換行列データ
		WireframeTransformationMatrix* transformationMatrixData_ = nullptr;

		// 三角形データ
		std::unique_ptr<WireframeTriangleData> triangleData_;

		// 矩形データ
		std::unique_ptr<WireframeBoxData> boxData_;

		// 線データ
		std::unique_ptr<WireframeLineData> lineData_;

		// AABB / OBBの単位キューブ線メッシュを共有し、world行列と色だけをインスタンスごとに送る。
		std::unique_ptr<WireframeBoxInstancedData> boxWireInstancedData_;

		// Sphereの3リング共有メッシュとインスタンスデータ。
		std::unique_ptr<WireframeSphereInstancedData> sphereInstancedData_;

		// Capsuleの共有ワイヤーメッシュとインスタンスデータ。
		std::unique_ptr<WireframeCapsuleInstancedData> capsuleInstancedData_;

	private: /// ---------- メンバ変数 ---------- ///

		// デバッグカメラの有無
		bool isDebugCamera_ = false;
#ifdef _DEBUG
		bool debugDrawEnabled_ = true;
#else
		bool debugDrawEnabled_ = false;
#endif

		// 三角形
		uint32_t triangleIndex_ = 0; // 三角形のインデックス

		// 矩形
		uint32_t boxIndex_ = 0;
		uint32_t boxVertexIndex_ = 0;

		// 線分
		uint32_t lineIndex_ = 0;

		// 現在フレームに蓄積されたAABB / OBB共通のBox Wireインスタンス数。
		uint32_t boxWireInstanceCount_ = 0;

		// 現在フレームに蓄積されたSphereインスタンス数。
		uint32_t sphereInstanceCount_ = 0;

		// 現在フレームに蓄積されたCapsuleインスタンス数。
		uint32_t capsuleInstanceCount_ = 0;

		// マトリックス
		Matrix4x4 projectionMatrix_{};
		Matrix4x4 viewProjectionMatrix_{};

		// デバッグ用マトリックス
		Matrix4x4 debugProjectionMatrix_{};
		Matrix4x4 debugViewProjectionMatrix_{};

	private: /// ---------- コピー禁止 ---------- ///

		Wireframe() = default;
		~Wireframe() = default;
		Wireframe(const Wireframe&) = delete;
		Wireframe& operator=(const Wireframe&) = delete;
	};

} // namespace Ken4lowEngine
