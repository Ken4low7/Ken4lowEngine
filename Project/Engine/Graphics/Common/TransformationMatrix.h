#pragma once
#include "Matrix4x4.h"

namespace Ken4lowEngine
{

	///==========================================================
	/// TransformationMatrixを拡張
	///==========================================================
	struct TransformationMatrix final
	{
		Matrix4x4 WVP;					  // World View Projection行列
		Matrix4x4 World;				  // ワールド行列
		Matrix4x4 WorldInversedTranspose; // ワールド行列の逆転置行列
	};

	///==========================================================
	/// TransformationMatrix（行列アニメーション用）を拡張
	/// ==========================================================
	struct TransformationAnimationMatrix final
	{
		Matrix4x4 WVP;					  // World View Projection行列
		Matrix4x4 World;				  // ワールド行列
		Matrix4x4 WorldInversedTranspose; // ワールド行列の逆転置行列
	};
} // namespace Ken4lowEngine
