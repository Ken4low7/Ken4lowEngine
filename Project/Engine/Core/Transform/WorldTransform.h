#pragma once
#include <DX12Include.h>
#include <Engine/Graphics/Device/Buffer/PerFrameUploadBuffer.h>
#include "Vector3.h"
#include "Matrix4x4.h"

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class Camera;

	/// -------------------------------------------------------------
	///　				　ワールド変換データクラス
	/// -------------------------------------------------------------
	class WorldTransform
	{
	public: /// ---------- 構造体 ---------- ///

		/// <summary>
		/// シェーダーへ送る座標変換行列一式です。
		/// </summary>
		struct TransformationMatrix final
		{
			Matrix4x4 WVP;
			Matrix4x4 World;
			Matrix4x4 WorldInversedTranspose;
		};

	public: /// ---------- メンバ変数 ---------- ///
		Vector3 scale_ = { 1.0f, 1.0f, 1.0f };
		Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };
		Vector3 translate_ = { 0.0f, 0.0f, 0.0f };
		Vector3 worldTranslate_ = { 0.0f, 0.0f, 0.0f };
		Vector3 worldRotate_ = { 0.0f, 0.0f, 0.0f };
		Matrix4x4 matWorld_;
		const WorldTransform* parent_ = nullptr;

	public: /// ---------- メンバ関数 ---------- ///
		void Initialize();
		void Update();
		void UpdateWithWorldMatrix(const Matrix4x4& worldMatrix);
		void SetPipeline(UINT rootParameterIndex = 1);
		const Matrix4x4& GetWorldMatrix() const { return matWorld_; }

	private:
		TransformationMatrix transformationData_{};
		PerFrameUploadBuffer<TransformationMatrix> transformationBuffers_;
	};

} // namespace Ken4lowEngine
