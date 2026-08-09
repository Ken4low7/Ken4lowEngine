#pragma once
#include <xaudio2.h>
#include <vector>

namespace Ken4lowEngine
{

/// ---------- 音声データの読み込み ---------- ///

// チャンクヘッダー
struct ChunkHeader
{
	char id[4];	  // チャンクID
	int32_t size; // チャンクサイズ
};

// RIFFヘッダチャンク
struct RiffHeader
{
	ChunkHeader chunk; // RIFF
	char type[4];	   // WAVE
};

// FMTチャンク
struct FormatChunk
{
	ChunkHeader chunk; // fmt
	WAVEFORMATEX fmt;  // 波形フォーマット
};

// 音声データ
struct SoundData
{
	WAVEFORMATEX wfex;		 // 波形フォーマット
	BYTE* pBuffer;			 // バッファの先頭アドレス
	unsigned int bufferSize; // バッファのサイズ
};

} // namespace Ken4lowEngine
