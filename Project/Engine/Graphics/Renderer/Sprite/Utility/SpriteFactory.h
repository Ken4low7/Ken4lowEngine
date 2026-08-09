#pragma once
#include "DataAssetPresets.h"
#include "Sprite.h"

#include <memory>
#include <string>

namespace Ken4lowEngine
{
	/// <summary>
	/// Sprite の生成と初期設定をまとめて行う補助クラス。
	/// </summary>
	class SpriteFactory
	{
	public:
		/// <summary>
		/// SpritePreset をもとに、初期化済み Sprite を生成します。
		/// </summary>
		/// <param name="preset">Sprite に反映する表示設定。</param>
		/// <returns>初期化と設定反映が完了した Sprite。</returns>
		static std::unique_ptr<Sprite> Create(const SpritePreset& preset);

		/// <summary>
		/// よく使う基本項目だけで Sprite を生成します。
		/// </summary>
		/// <param name="texturePath">使用するテクスチャパス。</param>
		/// <param name="position">表示座標。</param>
		/// <param name="size">表示サイズ。</param>
		/// <param name="color">表示色。</param>
		/// <returns>初期化と設定反映が完了した Sprite。</returns>
		static std::unique_ptr<Sprite> Create(
			const std::string& texturePath,
			const Vector2& position,
			const Vector2& size,
			const Vector4& color = { 1.0f, 1.0f, 1.0f, 1.0f }
		);

		/// <summary>
		/// UV切り出しも含めて Sprite を生成します。
		/// </summary>
		/// <param name="texturePath">使用するテクスチャパス。</param>
		/// <param name="position">表示座標。</param>
		/// <param name="size">表示サイズ。</param>
		/// <param name="textureLeftTop">テクスチャ切り出し左上座標。</param>
		/// <param name="textureSize">テクスチャ切り出しサイズ。</param>
		/// <param name="color">表示色。</param>
		/// <returns>初期化と設定反映が完了した Sprite。</returns>
		static std::unique_ptr<Sprite> CreateUv(
			const std::string& texturePath,
			const Vector2& position,
			const Vector2& size,
			const Vector2& textureLeftTop,
			const Vector2& textureSize,
			const Vector4& color = { 1.0f, 1.0f, 1.0f, 1.0f }
		);
	};
}