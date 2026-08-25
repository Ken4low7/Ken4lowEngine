#pragma once

#include "Engine/Graphics/Material/Material.h"
#include "Vector3.h"

namespace Ken4lowEngine
{
	/// <summary>Planar Reflection Captureへ自身の3D形状を描画できるComponent向け契約です。</summary>
	class ReflectionCaptureDrawable
	{
	public:
		virtual ~ReflectionCaptureDrawable() = default;
		virtual MaterialBlendMode GetReflectionCaptureBlendMode() const = 0;
		virtual Vector3 GetReflectionCaptureSortPosition() const = 0;
		virtual void DrawReflectionCapture() = 0; // 新しい3D Componentも描画分類とSort位置を公開するだけで鏡Captureへ参加できる。
	};
} // namespace Ken4lowEngine
