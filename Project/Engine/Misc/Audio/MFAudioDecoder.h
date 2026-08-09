#pragma once
#include <xaudio2.h>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///					デコード済み音声データ
	/// -------------------------------------------------------------
	/// XAudio2 でそのまま再生できるように、
	/// 波形フォーマット情報と PCM バッファをまとめて保持する。
	struct DecodedAudioData
	{
		WAVEFORMATEX waveFormat{};
		std::vector<BYTE> pcmData;
	};

	/// -------------------------------------------------------------
	///			Media Foundation を使って音声をデコードするクラス
	/// -------------------------------------------------------------
	/// MP3 / WAV などの音声ファイルを読み込み、
	/// XAudio2 に渡しやすい PCM(16bit) 形式へ変換する。
	class MFAudioDecoder
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// 音声ファイルを PCM(16bit) へデコードする。
		/// 成功時は outData に波形フォーマットと PCM データを書き込む。
		/// </summary>
		/// <param name="filePath">デコードする音声ファイルパス</param>
		/// <param name="outData">デコード結果の出力先</param>
		/// <returns>デコード成功なら true</returns>
		static bool DecodeFile(const std::string& filePath, DecodedAudioData& outData);
	};

} // namespace Ken4lowEngine