#pragma once

#include <Sprite.h>
#include <Vector2.h>

#include <memory>

namespace Ken4lowEngine
{
	/// <summary>
	/// UIで使う白テクスチャ矩形Spriteを生成・配置する小さなFactoryです。
	/// Pause / Result / Overlayに散っていた同じSprite生成処理を集約し、既存UIの座標・サイズ・色・表示順は呼び出し側に残します。
	/// 入力処理、選択状態、HitTest、Scene遷移、Font描画はUIごとの責務が違うため、このFactoryには持たせません。
	/// </summary>
	class UiSpriteFactory
	{
	public:
		/// <summary>
		/// 白テクスチャを使った矩形Spriteを生成します。
		/// 既存見た目を維持するため、位置・サイズ・anchorは呼び出し側が指定した値をそのまま設定します。
		/// </summary>
		static std::unique_ptr<Sprite> CreateWhiteRectSprite(
			const Vector2& position = { 0.0f, 0.0f },
			const Vector2& size = { 1.0f, 1.0f },
			const Vector2& anchor = { 0.0f, 0.0f })
		{
			auto sprite = std::make_unique<Sprite>();
			sprite->Initialize("Effects/white.dds");
			ApplyRect(sprite.get(), position, size, anchor);
			return sprite;
		}

		/// <summary>ボタン本体用の白矩形Spriteを生成します。色や選択状態は既存UI側で設定します。</summary>
		static std::unique_ptr<Sprite> CreateButtonBackgroundSprite(const Vector2& anchor = { 0.0f, 0.0f })
		{
			return CreateWhiteRectSprite({ 0.0f, 0.0f }, { 1.0f, 1.0f }, anchor);
		}

		/// <summary>ボタン枠用の白矩形Spriteを生成します。枠の太さは呼び出し側のsize指定で維持します。</summary>
		static std::unique_ptr<Sprite> CreateButtonBorderSprite(const Vector2& anchor = { 0.0f, 0.0f })
		{
			return CreateWhiteRectSprite({ 0.0f, 0.0f }, { 1.0f, 1.0f }, anchor);
		}

		/// <summary>ボタンaccent用の白矩形Spriteを生成します。表示色とalphaは既存UI側で設定します。</summary>
		static std::unique_ptr<Sprite> CreateAccentSprite(const Vector2& anchor = { 0.0f, 0.5f })
		{
			return CreateWhiteRectSprite({ 0.0f, 0.0f }, { 1.0f, 1.0f }, anchor);
		}

		/// <summary>
		/// 既存UIの矩形Spriteへ position / size / anchor を反映します。
		/// 見た目を変えないため、色・入力・表示順は変更せず、矩形設定だけを共通化します。
		/// </summary>
		static void ApplyRect(Sprite* sprite, const Vector2& position, const Vector2& size, const Vector2& anchor)
		{
			if (!sprite)
			{
				return;
			}

			sprite->SetAnchorPoint(anchor);
			sprite->SetPosition(position);
			sprite->SetSize(size);
			sprite->Update();
		}
	};
} // namespace Ken4lowEngine
