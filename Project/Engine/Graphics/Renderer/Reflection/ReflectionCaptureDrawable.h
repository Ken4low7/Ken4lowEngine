#pragma once

namespace Ken4lowEngine
{
	/// <summary>Planar Reflection Captureへ自身の3D形状を描画できるComponent向け契約です。</summary>
	class ReflectionCaptureDrawable
	{
	public:
		virtual ~ReflectionCaptureDrawable() = default;
		virtual void DrawReflectionCapture() = 0; // 新しい3D Componentもこの契約を実装するだけで鏡Captureへ参加できる。
	};
} // namespace Ken4lowEngine
