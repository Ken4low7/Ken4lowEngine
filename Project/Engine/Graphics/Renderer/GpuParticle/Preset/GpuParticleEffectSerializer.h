#pragma once

#include <optional>
#include <string>

#include "GpuParticleEffectDesc.h"

namespace Ken4lowEngine
{
	/// <summary>
	/// 描画方式をJSON用の文字列（"Sprite" / "Mesh"）へ変換します。
	/// JSON上ではenumの数値ではなく、人が読み書きしやすい文字列として扱います。
	/// </summary>
	std::string ToString(GpuParticleRenderType type);

	/// <summary>
	/// JSON用文字列を描画方式へ変換します。
	/// 未知の文字列は、旧データや手編集したJSONでも安全に読み込めるようSpriteとして扱います。
	/// </summary>
	GpuParticleRenderType GpuParticleRenderTypeFromString(const std::string& text);

	/// <summary>BlendModeをJSON上で数値ではなく読みやすい文字列として保存するための変換です。</summary>
	std::string ToString(GpuParticleBlendMode mode);
	/// <summary>不明なJSON文字列を安全なAlphaへフォールバックして読み込みます。</summary>
	GpuParticleBlendMode GpuParticleBlendModeFromString(const std::string& text);

	/// <summary>SpawnShapeをJSON上で数値ではなく読みやすい文字列として保存するための変換です。</summary>
	std::string ToString(GpuParticleSpawnShape shape);
	/// <summary>不明なJSON文字列を安全なPointへフォールバックして読み込みます。</summary>
	GpuParticleSpawnShape GpuParticleSpawnShapeFromString(const std::string& text);

	/// <summary>
	/// 複数Emitterを持つ新しいEffect設定をJSONへ保存・読み込みするクラスです。
	/// 既存Emitter用Serializerとは分離し、ParameterManagerや既存描画処理を肥大化・変更させません。
	/// </summary>
	class GpuParticleEffectSerializer
	{
	public:
		/// <summary>
		/// ImGuiで編集したEffect設定をJSONファイルから復元するための処理です。
		/// 不正JSONでクラッシュしないように検証し、失敗時はdescを変更せずfalseを返します。
		/// </summary>
		static bool Load(GpuParticleEffectDesc& desc, const std::string& filePath);

		/// <summary>
		/// ImGuiで編集したEffect設定をJSONファイル化するための処理です。
		/// 保存先フォルダが無い場合は作成し、保存できなければfalseを返します。
		/// </summary>
		static bool Save(const GpuParticleEffectDesc& desc, const std::string& filePath);

		/// <summary>既存呼び出しとの互換用に、読み込み結果をoptionalで返します。</summary>
		static std::optional<GpuParticleEffectDesc> LoadFromFile(const std::string& filePath);

		/// <summary>既存呼び出しとの互換用Saveラッパーです。</summary>
		static bool SaveToFile(const GpuParticleEffectDesc& effect, const std::string& filePath);

		/// <summary>クラス経由の既存呼び出し向けに、JSON用描画方式名へ変換します。</summary>
		static const char* ToString(GpuParticleRenderType value);

		/// <summary>JSONの描画方式名をSprite/Meshへ変換します。</summary>
		static bool TryParseRenderType(const std::string& text, GpuParticleRenderType& outValue);
	};

} // namespace Ken4lowEngine
