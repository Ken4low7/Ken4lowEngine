#pragma once
#include "WorldTransform.h"
#include "Matrix4x4.h"

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///						　カメラクラス
/// -------------------------------------------------------------
class Camera
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 仮想デストラクタ。
	/// </summary>
	virtual ~Camera() = default;

	/// <summary>
	/// デフォルトコンストラクタ。<br/>
	/// クライアントサイズからアスペクト比を計算し、<br/>
	/// デフォルトの FOV / ニアクリップ / ファークリップで各種行列を初期化します。
	/// </summary>
	Camera();

	/// <summary>
	/// カメラの状態を更新します。<br/>
	/// ・target_ が現在位置と異なる場合は LookAt でビュー行列を構築<br/>
	/// ・それ以外の場合は WorldTransform からワールド行列を作成して逆行列からビュー行列を計算<br/>
	/// ・FOV / アスペクト比 / ニア・ファークリップから射影行列を再計算<br/>
	/// ・viewProjectionMatrix_ = viewMatrix_ × projectionMatrix_ を更新<br/>
	/// といった処理を行います。
	/// </summary>
	void Update();

	/// <summary>
	/// カメラ用の ImGui デバッグ UI を描画します。<br/>
	/// 位置・回転・FOV・ニア／ファークリップを編集でき、調整結果は次回 Update() で反映されます。<br/>
	/// （USE_IMGUI が定義されている場合のみ有効）
	/// </summary>
	void DrawImGui();

public: /// ---------- セッター ---------- ///

	/// <summary>
	/// ワールド座標系のスケールを設定します。
	/// </summary>
	/// <param name="scale">XYZ スケール。</param>
	void SetScale(const Vector3& scale) { worldTransform_.scale_ = scale; }

	/// <summary>
	/// ワールド座標系の回転を設定します（オイラー角）。<br/>
	/// 単位はラジアンを想定しています。
	/// </summary>
	/// <param name="rotate">XYZ 回転（ラジアン）。</param>
	void SetRotate(const Vector3& rotate) { worldTransform_.rotate_ = rotate; }

	/// <summary>
	/// カメラの位置を設定します。
	/// </summary>
	/// <param name="translate">新しいカメラ位置。</param>
	void SetTranslate(const Vector3& translate) { worldTransform_.translate_ = translate; }

	/// <summary>
	/// 垂直方向視野角(FoV Y)を設定します（ラジアン）。
	/// </summary>
	/// <param name="fovY">垂直方向 FOV。</param>
	void SetFovY(const float fovY) { fovY_ = fovY; }

	/// <summary>
	/// アスペクト比を設定します。<br/>
	/// 通常は「画面幅 / 画面高さ」を指定します。
	/// </summary>
	/// <param name="aspectRatio">アスペクト比。</param>
	void SetAspectRatio(const float aspectRatio);

	/// <summary>
	/// ニアクリップ距離を設定します。
	/// </summary>
	/// <param name="nearClip">ニアクリップ距離。</param>
	void SetNearClip(const float nearClip) { nearClip_ = nearClip; }

	/// <summary>
	/// ファークリップ距離を設定します。
	/// </summary>
	/// <param name="farClip">ファークリップ距離。</param>
	void SetFarClip(const float farClip) { farClip_ = farClip; }

	/// <summary>
	/// ビュー行列を直接設定します。<br/>
	/// 外部で計算したビュー行列をそのまま使いたい場合に利用します。
	/// </summary>
	/// <param name="viewMatrix">新しいビュー行列。</param>
	void SetViewMatrix(const Matrix4x4& viewMatrix) { viewMatrix_ = viewMatrix; }

	/// <summary>
	/// 射影行列を直接設定します。
	/// </summary>
	/// <param name="projectionMatrix">新しい射影行列。</param>
	void SetProjectionMatrix(const Matrix4x4& projectionMatrix) { projectionMatrix_ = projectionMatrix; }

	/// <summary>
	/// ビュー射影行列を直接設定します。
	/// </summary>
	/// <param name="viewProjectionMatrix">新しいビュー射影行列。</param>
	void SetViewProjectionMatrix(const Matrix4x4& viewProjectionMatrix) { viewProjectionMatrix_ = viewProjectionMatrix; }

	/// <summary>
	/// 注視点（ターゲット）を設定します。<br/>
	/// target_ がカメラ位置と異なる場合、Update() 内で LookAt によるビュー行列計算に切り替わります。
	/// </summary>
	/// <param name="target">カメラが向く注視位置。</param>
	void SetTraget(const Vector3& target) { target_ = target; } // ※命名は既存コードを維持

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// 現在のスケールを取得します。
	/// </summary>
	const Vector3& GetScale() const { return worldTransform_.scale_; }

	/// <summary>
	/// 現在の回転（オイラー角）を取得します。
	/// </summary>
	const Vector3& GetRotate() const { return worldTransform_.rotate_; }

	/// <summary>
	/// 現在のカメラ位置を取得します。
	/// </summary>
	const Vector3& GetTranslate() const { return worldTransform_.translate_; }

	/// <summary>
	/// ワールド行列を取得します。
	/// </summary>
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

	/// <summary>
	/// ビュー行列を取得します。
	/// </summary>
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }

	/// <summary>
	/// 射影行列を取得します。
	/// </summary>
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

	/// <summary>
	/// ビュー射影行列を取得します。
	/// </summary>
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

	/// <summary>
	/// 現在のニアクリップ距離を取得します。
	/// </summary>
	float GetNearClip() const { return nearClip_; }

	/// <summary>
	/// 現在のファークリップ距離を取得します。
	/// </summary>
	float GetFarClip() const { return farClip_; }

	/// <summary>
	/// カメラの前方ベクトル（ワールド空間）を取得します。<br/>
	/// ローカル Z+ を前方向とみなし、現在の回転行列を適用して正規化したベクトルを返します。
	/// </summary>
	Vector3 GetForward() const;
	void SetForward(const Vector3& forward);

	/// <summary>
	/// 垂直方向視野角(FoV Y)を取得します（ラジアン）。
	/// </summary>
	float GetFovY() const { return fovY_; }

	/// <summary>
	/// 現在のアスペクト比を取得します。
	/// </summary>
	float GetAspectRatio() const { return aspectRatio_; }

private: /// ---------- メンバ変数 ----- ///

	// Transform情報
	WorldTransform worldTransform_;

	// ワールド行列データ
	Matrix4x4 worldMatrix_;

	// ビュー行列データ
	Matrix4x4 viewMatrix_;

	Vector3 target_{ 0, 0, 1 };  // デフォルトでZ前方を注視

	// プロジェクション行列データ
	Matrix4x4 projectionMatrix_;
	float fovY_;		   // 水平方向視野角
	float aspectRatio_; // アスペクト比
	float nearClip_;	   // ニアクリップ
	float farClip_;	   // ファークリップ

	// 合成行列
	Matrix4x4 viewProjectionMatrix_;

};


} // namespace Ken4lowEngine
